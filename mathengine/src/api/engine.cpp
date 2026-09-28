// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Dispatcher: classifies the input and routes it to the right solver. Also exports the C API.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <regex>

#include "api/evaluator.h"
#include "calculus/calculus.h"
#include "core/poly.h"
#include "mathengine/mathengine.h"
#include "matrix/matrix.h"
#include "parse/latex_import.h"
#include "solution/plotutil.h"
#include "solution/story.h"
#include "solvers.h"

namespace me {

namespace {

using T = RNode::T;

Solution fail(const std::string& msg, const std::string& inputLatex = "") {
    Solution s;
    s.ok = false;
    s.error = msg;
    s.inputLatex = inputLatex;
    return s;
}

std::vector<std::string> varsOf(std::initializer_list<E> es) {
    std::vector<std::string> v;
    for (auto& e : es) collectVars(e, v);
    return v;
}

bool isComplexProblem(const E& e, const std::string& var) {
    return containsSym(e, "i") || containsFunc(e, "conj") || containsFunc(e, "re") || containsFunc(e, "im") || containsFunc(e, "arg") ||
           var == "z";
}

bool needsSplit(const E& e, const std::string& var) {
    std::function<bool(const E&)> f = [&](const E& n) {
        if (n->kind == Kind::Func && (n->name == "conj" || n->name == "re" || n->name == "im" || n->name == "abs") && containsSym(n, var))
            return true;
        for (auto& a : n->args) if (f(a)) return true;
        return false;
    };
    return f(e);
}

std::optional<BigInt> asInteger(const E& e) {
    E s = simplify(e);
    if (isInteger(s)) return s->num.num();
    return std::nullopt;
}

// Letters in a graph that are neither coordinates nor constants: parameters shown as sliders.
std::vector<std::string> graphParams(const std::vector<std::string>& vars) {
    std::vector<std::string> out;
    for (auto& v : vars)
        if (v.size() == 1 && v != "x" && v != "y" && v != "r" && v != "z" && v != "e" && v != "i") out.push_back(v);
    std::ranges::sort(out);
    return out;
}

// graph for an equation in x and y
void graphEquation(PlotSpec& ps, const E& L, const E& R, const std::string& label) {
    std::vector<std::string> vars = varsOf({L, R});
    // y = f(x; a, b, ...): an explicit curve whose parameters become sliders (default value 1 in the app)
    if (auto params = graphParams(vars); !params.empty()) {
        const E* side = isSym(L, "y") && !containsSym(R, "y") ? &R : isSym(R, "y") && !containsSym(L, "y") ? &L : nullptr;
        if (side && containsSym(*side, "x")) {
            ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(*side), label});
            for (auto& p : params) if (std::ranges::find(ps.params, p) == ps.params.end()) ps.params.push_back(p);
            return;
        }
    }
    E f = simplify(sub(L, R));
    bool hasX = std::ranges::find(vars, std::string("x")) != vars.end();
    bool hasY = std::ranges::find(vars, std::string("y")) != vars.end();
    bool hasR = std::ranges::find(vars, std::string("r")) != vars.end();
    bool hasT = std::ranges::find(vars, std::string("theta")) != vars.end();
    if (hasR && !hasX && !hasY) {
        // polar r = f(theta)
        if (isSym(L, "r") && !containsSym(R, "r")) { ps.curves.push_back({PlotCurve::Type::Polar, toPlain(R), label}); return; }
        if (isSym(R, "r") && !containsSym(L, "r")) { ps.curves.push_back({PlotCurve::Type::Polar, toPlain(L), label}); return; }
    }
    if (hasT && !hasX && !hasY && !hasR) {
        ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(substitute(f, "theta", sym("x"))), label});
        return;
    }
    if (hasY) {
        E fe = expand(f);
        if (polyDegree(fe, "y") == 1) {
            E c1 = simplify(derivativeOf(fe, "y"));
            if (!containsSym(c1, "y") && !containsSym(c1, "x")) {
                E yx = simplify(divide(neg(substitute(fe, "y", num(0))), c1));
                ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(yx), label});
                return;
            }
        }
        ps.curves.push_back({PlotCurve::Type::Implicit, toPlain(f), label});
        ps.xmin = -10; ps.xmax = 10; ps.ymin = -10; ps.ymax = 10;
        return;
    }
    if (hasX) {
        E fe = expand(f);
        if (polyDegree(fe, "x") == 1 && vars.size() == 1) {
            E c1 = simplify(derivativeOf(fe, "x"));
            ps.curves.push_back({PlotCurve::Type::Vertical, toPlain(simplify(divide(neg(substitute(fe, "x", num(0))), c1))), label});
            return;
        }
        // equation in x only: plot both sides
        if (hasFreeVars(L)) ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(L), "y = " + toLatex(L)});
        if (hasFreeVars(R) || isNum(R)) ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(R), "y = " + toLatex(R)});
        return;
    }
    if (vars.size() == 1) {
        std::string v = vars[0];
        ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(substitute(f, v, sym("x"))), label});
    }
}

Solution dispatchFunc(const R& r, const std::string& inputLatex) {
    const std::string& n = r->op;
    auto kid = [&](size_t i) -> Result<E> {
        if (i >= r->kids.size()) return std::unexpected(Error{"Missing argument"});
        return toExpr(r->kids[i]);
    };
    auto varAt = [&](size_t i, const E& f) {
        if (i < r->kids.size() && r->kids[i]->t == T::Sym) return r->kids[i]->op;
        std::vector<std::string> v;
        collectVars(f, v);
        return varPick(v);
    };
    if (n == "diff") {
        auto f = kid(0);
        if (!f) return fail(f.error().message, inputLatex);
        int order = (r->kids.size() > 2 && r->kids[2]->t == T::Num) ? (int)r->kids[2]->value.num().toInt64() : 1;
        if (order < 1 || order > 10) return fail("Derivative order must be between 1 and 10", inputLatex);
        return solveDerivative(*f, varAt(1, *f), order, inputLatex);
    }
    if (n == "int") {
        auto f = kid(0);
        if (!f) return fail(f.error().message, inputLatex);
        std::optional<std::pair<E, E>> bounds;
        if (r->kids.size() >= 4) {
            auto a = kid(2), b = kid(3);
            if (!a || !b) return fail("Invalid bounds", inputLatex);
            bounds = std::pair{simplify(*a), simplify(*b)};
        }
        return solveIntegral(*f, varAt(1, *f), bounds, inputLatex);
    }
    if (n == "lim") {
        auto f = kid(0);
        if (!f) return fail(f.error().message, inputLatex);
        auto p = kid(2);
        if (!p) return fail("Limit needs a point: lim(f, x, a)", inputLatex);
        return solveLimit(*f, varAt(1, *f), *p, inputLatex);
    }
    if (n == "fact") {
        auto a = kid(0);
        if (!a) return fail(a.error().message, inputLatex);
        return solveFactorial(simplify(*a), inputLatex);
    }
    if (n == "factor" || n == "primefactors" || n == "isprime") {
        auto a = kid(0);
        if (!a) return fail(a.error().message, inputLatex);
        if (auto i = asInteger(*a)) return solvePrimeFactorization(*i, inputLatex);
        if (n != "factor") return fail("Prime factorization needs an integer", inputLatex);
        return factorExpression(*a, inputLatex);
    }
    if (n == "expand") {
        auto a = kid(0);
        if (!a) return fail(a.error().message, inputLatex);
        return expandExpression(*a, inputLatex);
    }
    if (n == "simplify") {
        auto a = kid(0);
        if (!a) return fail(a.error().message, inputLatex);
        return simplifyExpression(*a, inputLatex);
    }
    if ((n == "gcd" || n == "lcm") && r->kids.size() >= 2) {
        std::vector<BigInt> nums;
        for (size_t i = 0; i < r->kids.size(); ++i) {
            auto a = kid(i);
            if (!a) return fail(a.error().message, inputLatex);
            auto v = asInteger(*a);
            if (!v) return fail("gcd and lcm need integers", inputLatex);
            nums.push_back(*v);
        }
        return solveGcdLcm(nums, n == "lcm", inputLatex);
    }
    if ((n == "binom" || n == "perm") && r->kids.size() == 2) {
        auto a = kid(0), b = kid(1);
        if (!a || !b) return fail("Invalid arguments", inputLatex);
        auto na = asInteger(*a), nb = asInteger(*b);
        if (!na || !nb) return fail("Combinations need integers", inputLatex);
        return solveCombinatorics(*na, *nb, n == "binom", inputLatex);
    }
    if (n == "roots") {
        auto a = kid(0);
        if (!a) return fail(a.error().message, inputLatex);
        std::vector<std::string> v;
        collectVars(*a, v);
        return solveEquation(*a, num(0), varPick(v), inputLatex, false);
    }
    if (n == "polar") {
        auto a = kid(0);
        if (!a) return fail(a.error().message, inputLatex);
        Solution s;
        s.kind = "complex";
        s.anim = "complex";
        s.title = "Polar form";
        s.inputLatex = inputLatex;
        E z = simplify(*a);
        s.answerLatex = toLatex(z);
        s.answers.push_back(s.answerLatex);
        addComplexFacts(s, z);
        for (auto& [k, v] : s.facts) if (k == "Polar form") s.answerLatex = v;
        s.methods.push_back({"Polar form", {{"Find the modulus and the argument", m("r = \\sqrt{a^{2}+b^{2}}, \\ \\theta = \\arg(z)"), s.answerLatex, {}}}});
        return s;
    }
    if (isStatsFn(n) || isFinanceFn(n)) {
        std::vector<double> xs;
        for (size_t i = 0; i < r->kids.size(); ++i) {
            auto a = kid(i);
            if (!a) return fail(a.error().message, inputLatex);
            auto v = evalReal(simplify(*a));
            if (!v) return fail("Every argument must be a number.", inputLatex);
            xs.push_back(*v);
        }
        return isStatsFn(n) ? solveStatistics(n, xs, inputLatex) : solveFinance(n, xs, inputLatex);
    }
    if (n == "graph3d" || n == "surface") {
        auto f = kid(0);
        if (!f) return fail(f.error().message, inputLatex);
        return solveSurface(*f, inputLatex);
    }
    if (n == "taylor") {
        auto f = kid(0);
        if (!f) return fail(f.error().message, inputLatex);
        std::string v = varAt(1, *f);
        E a = num(0);
        if (r->kids.size() > 2) { auto av = kid(2); if (!av) return fail("Invalid centre", inputLatex); a = *av; }
        int order = 5;
        if (r->kids.size() > 3 && r->kids[3]->t == T::Num) order = (int)r->kids[3]->value.num().toInt64();
        order = std::clamp(order, 1, 12);
        return solveTaylor(*f, v, a, order, inputLatex);
    }
    if (n == "dsolve" && !r->kids.empty() && r->kids[0]->t == T::Rel) {
        auto L = toExpr(r->kids[0]->kids[0]), Rr = toExpr(r->kids[0]->kids[1]);
        if (!L || !Rr) return fail("Invalid differential equation", inputLatex);
        return solveODE(*L, *Rr, inputLatex);
    }
    if (n == "solve") {
        if (r->kids.empty()) return fail("solve needs an equation", inputLatex);
        R target = r->kids[0];
        std::string forVar = r->kids.size() > 1 && r->kids[1]->t == T::Sym ? r->kids[1]->op : "";
        if (target->t == T::Rel) {
            auto L = toExpr(target->kids[0]), Rr = toExpr(target->kids[1]);
            if (!L || !Rr) return fail("Invalid equation", inputLatex);
            std::string v = forVar.empty() ? varPick(varsOf({*L, *Rr})) : forVar;
            if (target->op == "=") return solveEquation(*L, *Rr, v, inputLatex, isComplexProblem(simplify(sub(*L, *Rr)), v));
            return solveInequality(*L, *Rr, target->op, v, inputLatex);
        }
        auto e = toExpr(target);
        if (!e) return fail(e.error().message, inputLatex);
        std::string v = forVar.empty() ? varPick(varsOf({*e})) : forVar;
        return solveEquation(*e, num(0), v, inputLatex, isComplexProblem(*e, v));
    }
    Solution notCommand;  // not a command: caller handles it as an expression
    notCommand.ok = false;
    return notCommand;
}

Solution solvePolarCurve(const E& f, const E& sl, const E& sr, const std::string& inputLatex);
std::optional<Solution> solveFunctionGraph(const E& f, const E& sl, const E& sr, const std::string& inputLatex);
Solution solveCurveFamily(const E& f, const E& sl, const E& sr, const std::vector<std::string>& params, const std::string& inputLatex);

Solution solveRel(const R& rel, const std::string& inputLatex) {
    auto L = toExpr(rel->kids[0]);
    if (!L) return fail(L.error().message, inputLatex);
    auto Rr = toExpr(rel->kids[1]);
    if (!Rr) return fail(Rr.error().message, inputLatex);
    std::vector<std::string> vars = varsOf({*L, *Rr});
    if (rel->op != "=") {
        if (vars.size() > 1) return fail("Inequalities with more than one variable aren't supported yet.", inputLatex);
        return solveInequality(*L, *Rr, rel->op, varPick(vars), inputLatex);
    }
    E f = simplify(sub(*L, *Rr));
    if (isODE(f)) return solveODE(*L, *Rr, inputLatex);
    // z = f(x, y): a surface
    {
        bool hasZ = std::ranges::find(vars, std::string("z")) != vars.end();
        E sl = simplify(*L), sr = simplify(*Rr);
        if (hasZ && vars.size() >= 2 && std::ranges::all_of(vars, [](const std::string& v) { return v == "x" || v == "y" || v == "z"; })) {
            if (isSym(sl, "z") && !containsSym(sr, "z")) return solveSurface(sr, inputLatex);
            if (isSym(sr, "z") && !containsSym(sl, "z")) return solveSurface(sl, inputLatex);
        }
    }
    // y = f(x) / relation in x and y: solve for y and graph the curve
    bool xy = std::ranges::find(vars, std::string("x")) != vars.end() && std::ranges::find(vars, std::string("y")) != vars.end();
    bool polar = std::ranges::find(vars, std::string("r")) != vars.end() && std::ranges::find(vars, std::string("theta")) != vars.end();
    // y = f(x) with parameters (a, b, k, ...): a family of curves, explored with sliders
    if (xy && !graphParams(vars).empty()) {
        E sl = simplify(*L), sr = simplify(*Rr);
        const E* side = isSym(sl, "y") && !containsSym(sr, "y") ? &sr : isSym(sr, "y") && !containsSym(sl, "y") ? &sl : nullptr;
        if (side && containsSym(*side, "x")) return solveCurveFamily(*side, sl, sr, graphParams(vars), inputLatex);
    }
    if (polar && vars.size() == 2) {
        E sl = simplify(*L), sr = simplify(*Rr);
        E f = isSym(sl, "r") && !containsSym(sr, "r") ? sr : isSym(sr, "r") && !containsSym(sl, "r") ? sl : nullptr;
        if (f) return solvePolarCurve(f, sl, sr, inputLatex);
    }
    if ((xy && vars.size() == 2) || (polar && vars.size() == 2)) {
        std::string v = polar ? "r" : "y";
        Solution s = solveEquation(*L, *Rr, v, inputLatex, false);
        PlotSpec ps;
        graphEquation(ps, simplify(*L), simplify(*Rr), toLatex(simplify(*L)) + " = " + toLatex(simplify(*Rr)));
        s.plot = ps;
        s.kind = s.ok ? "graph" : s.kind;
        s.anim = "graph";
        if (!polar) {
            if (auto conic = analyzeConic(simplify(*L), simplify(*Rr))) {
                // lead with the geometry; solving for y stays available as a second method
                std::vector<Method> methods{conic->method};
                if (s.ok) for (auto& mm : s.methods) methods.push_back(mm);
                s.ok = true;
                s.error.clear();
                s.kind = "graph";
                s.title = "Identify the curve";
                s.methods = methods;
                s.answerLatex = conic->answer;
                s.answers = {conic->answer};
                s.facts = conic->facts;
                s.plot->shape = conic->shape;
                // frame the curve around its own geometry
                const auto& sp = conic->shape.params;
                const std::string& ty = conic->shape.type;
                double cx = 0, cy = 0, rad = 5;
                if (ty == "circle") { cx = sp[0]; cy = sp[1]; rad = sp[2] * 1.5; }
                else if (ty == "ellipse") { cx = sp[0]; cy = sp[1]; rad = std::max(sp[2], sp[3]) * 1.4; }
                else if (ty == "hyperbola") { cx = sp[0]; cy = sp[1]; rad = (sp[2] + std::hypot(sp[2], sp[3])) * 1.8; }
                else if (ty == "parabola") { cx = sp[0]; cy = sp[1] + (sp[3] < 0.5 ? 2 * std::fabs(sp[2]) * 4 : 0); rad = std::max(3.0, std::fabs(sp[2]) * 10); }
                if (ty != "line" && ty != "vline") {
                    s.plot->xmin = cx - rad; s.plot->xmax = cx + rad;
                    s.plot->ymin = cy - rad; s.plot->ymax = cy + rad;
                    s.plot->autoRange = false;
                }
                return s;
            }
            // y = f(x) that isn't a conic: read the graph (roots, intercept, turning points, ends)
            E sl = simplify(*L), sr = simplify(*Rr);
            const E* side = isSym(sl, "y") && !containsSym(sr, "y") ? &sr : isSym(sr, "y") && !containsSym(sl, "y") ? &sl : nullptr;
            if (side && containsSym(*side, "x"))
                if (auto fg = solveFunctionGraph(*side, sl, sr, inputLatex)) return *fg;
        }
        if (!s.ok) {
            // still useful: show the graph
            s.ok = true;
            s.error.clear();
            s.title = "Graph the equation";
            s.answerLatex = toLatex(simplify(*L)) + " = " + toLatex(simplify(*Rr));
            s.answers = {s.answerLatex};
            s.methods = {{"Graph", {{"Plot every point that satisfies the equation", "", s.answerLatex, {}}}}};
        }
        return s;
    }
    std::string var = varPick(vars);
    if (needsSplit(f, var) && (isComplexProblem(f, var) || containsSym(f, "i"))) return solveComplexEquation(*L, *Rr, var, inputLatex);
    return solveEquation(*L, *Rr, var, inputLatex, isComplexProblem(f, var));
}


// r = f(theta): a curve described by distance and angle. Recognises roses, cardioids/limaçons,
// circles through the origin and Archimedean spirals; tabulates r for a few angles.


// y = f(x): read the graph like a textbook does: roots, y-intercept, turning points, end behaviour.
E num0() { return num(0); }

std::optional<Solution> solveFunctionGraph(const E& f, const E& sl, const E& sr, const std::string& inputLatex) {
    auto val = [](const E& e) -> double {
        auto pr = Program::compile(toPlain(e));
        return pr ? pr->eval(0, 0, 0) : NAN;
    };
    auto finiteReal = [&](const E& e) { return !containsSym(e, "n") && !containsSym(e, "i") && std::isfinite(val(e)); };
    Solution s;
    s.kind = "function";
    s.anim = "graph";
    s.inputLatex = inputLatex;
    int deg = polyDegree(expand(f), "x");
    static const char* names[] = {"Constant", "Linear", "Quadratic", "Cubic", "Quartic", "Quintic"};
    s.title = deg >= 0 && deg <= 5 ? std::string(names[deg]) + " function" : "Graph of a function";
    Method mm{"Read the graph", {}};
    PlotSpec ps;
    graphEquation(ps, sl, sr, toLatex(sl) + " = " + toLatex(sr));
    // roots
    std::vector<E> roots;
    solveEquation(f, num0(), "x", "", false, &roots);
    std::vector<E> rr;
    for (auto& r : roots) if (finiteReal(r)) rr.push_back(simplify(r));
    std::ranges::sort(rr, [&](const E& a, const E& b) { return val(a) < val(b); });
    if (rr.size() > 6) rr.resize(6);
    std::string rootsTex;
    for (size_t i = 0; i < rr.size(); ++i) rootsTex += (i ? ",\\ " : "") + toLatex(rr[i]);
    mm.steps.push_back({"Where it crosses the x-axis", "Set " + m("y = 0") + " and solve.",
                        rr.empty() ? "\\text{no real roots}" : toLatex(f) + " = 0 \\quad\\Rightarrow\\quad x = " + rootsTex, {}});
    for (auto& r : rr) ps.points.push_back({val(r), 0.0, "\\left(" + toLatex(r) + ", 0\\right)"});
    // y-intercept
    E y0 = simplify(substitute(f, "x", num0()));
    bool hasY0 = finiteReal(y0);
    if (hasY0) {
        mm.steps.push_back({"Where it crosses the y-axis", "Set " + m("x = 0") + ".", "y = " + toLatex(y0), {}});
        bool dup = std::ranges::any_of(rr, [&](const E& r) { return std::fabs(val(r)) < 1e-12; });
        if (!dup) ps.points.push_back({0.0, val(y0), "\\left(0, " + toLatex(y0) + "\\right)"});
    }
    // turning points
    E d1 = simplify(derivativeOf(f, "x"));
    E d2 = simplify(derivativeOf(d1, "x"));
    std::vector<E> crit;
    solveEquation(d1, num0(), "x", "", false, &crit);
    std::vector<std::string> turns;
    std::string maxTex, minTex;
    for (auto& c : crit) {
        if (!finiteReal(c)) continue;
        E cx = simplify(c);
        E cy = simplify(substitute(f, "x", cx));
        if (!finiteReal(cy)) continue;
        double k = val(substitute(d2, "x", cx));
        std::string pt = "\\left(" + toLatex(cx) + ", " + toLatex(cy) + "\\right)";
        if (k < -1e-12) maxTex += (maxTex.empty() ? "" : ",\\ ") + pt;
        else if (k > 1e-12) minTex += (minTex.empty() ? "" : ",\\ ") + pt;
        else continue;  // flat inflection: not a turning point
        turns.push_back(pt);
        ps.points.push_back({val(cx), val(cy), pt});
        if (turns.size() >= 6) break;
    }
    mm.steps.push_back({"Turning points", "Where the slope is zero: solve " + m("f'(x) = " + toLatex(d1) + " = 0") + ". The sign of " + m("f''") +
                        " tells a maximum from a minimum.",
                        turns.empty() ? "\\text{none}" : (maxTex.empty() ? "" : "\\text{max } " + maxTex) + (maxTex.empty() || minTex.empty() ? "" : ",\\quad ") +
                        (minTex.empty() ? "" : "\\text{min } " + minTex), {}});
    // end behaviour of a polynomial
    if (deg >= 1) {
        E lead = simplify(divide(expand(f), pow(sym("x"), num((long long)deg))));
        double a = val(substitute(lead, "x", num(1000000LL)));
        auto inf = [](bool pos) { return pos ? std::string("\\infty") : std::string("-\\infty"); };
        bool right = a > 0, left = (deg % 2 == 0) ? a > 0 : a < 0;
        mm.steps.push_back({"At the far ends", "The highest power " + m("x^{" + std::to_string(deg) + "}") + " wins.",
                            "x \\to \\infty:\\ y \\to " + inf(right) + ",\\quad x \\to -\\infty:\\ y \\to " + inf(left), {}});
    }
    if (rr.empty() && turns.empty() && !hasY0) return std::nullopt;
    s.methods.push_back(mm);
    if (!rr.empty()) s.facts.push_back({"Roots", "x = " + rootsTex});
    if (hasY0) s.facts.push_back({"y-intercept", toLatex(y0)});
    if (!maxTex.empty()) s.facts.push_back({"Maximum", maxTex});
    if (!minTex.empty()) s.facts.push_back({"Minimum", minTex});
    s.answerLatex = rr.empty() ? (turns.empty() ? "y = " + toLatex(f) : "\\text{turns at } " + turns[0]) : "\\text{crosses zero at } x = " + rootsTex;
    s.answers.push_back(s.answerLatex);
    // frame the interesting part: all key points with generous margins (at least 4 x 4)
    if (!ps.points.empty()) {
        double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
        for (auto& pt : ps.points) { x0 = std::min(x0, pt.x); x1 = std::max(x1, pt.x); y0 = std::min(y0, pt.y); y1 = std::max(y1, pt.y); }
        double w = std::max(4.0, (x1 - x0) * 1.8), h = std::max(4.0, (y1 - y0) * 1.8);
        double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        ps.xmin = cx - w / 2; ps.xmax = cx + w / 2; ps.ymin = cy - h / 2; ps.ymax = cy + h / 2;
        ps.autoRange = false;
    }
    s.plot = ps;
    s.ok = true;
    return s;
}

// y = f(x; a, b, ...): one curve for every choice of the parameters.
Solution solveCurveFamily(const E& f, const E& sl, const E& sr, const std::vector<std::string>& params, const std::string& inputLatex) {
    Solution s;
    s.kind = "graph";
    s.anim = "graph";
    s.title = "Family of curves";
    s.inputLatex = inputLatex;
    std::string names;
    for (size_t i = 0; i < params.size(); ++i) names += (i ? (i + 1 == params.size() ? " and " : ", ") : "") + m(params[i]);
    E g = f;
    for (auto& p : params) g = substitute(g, p, num(1));
    g = simplify(g);
    std::string all1;
    for (size_t i = 0; i < params.size(); ++i) all1 += (i ? " = " : "") + params[i];
    Method mm{"Family of curves", {}};
    mm.steps.push_back({"One curve for every choice of " + names, "Fixing the parameters picks one curve. Changing them reshapes it.", toLatex(sl) + " = " + toLatex(sr), {}});
    mm.steps.push_back({"Start from " + m(all1 + " = 1"), "Substitute 1 for each parameter.", "y = " + toLatex(g), {}});
    mm.steps.push_back({"Explore", "Open the graph and move the slider for " + names + " to see how each one changes the curve.", "", {}});
    s.methods.push_back(mm);
    s.answerLatex = toLatex(sl) + " = " + toLatex(sr);
    s.answers.push_back(s.answerLatex);
    s.facts.push_back({"Parameters", names.empty() ? "" : [&] { std::string t; for (size_t i = 0; i < params.size(); ++i) t += (i ? ",\\ " : "") + params[i]; return t; }()});
    PlotSpec ps;
    graphEquation(ps, sl, sr, toLatex(sl) + " = " + toLatex(sr));
    s.plot = ps;
    s.ok = true;
    return s;
}

Solution solvePolarCurve(const E& f, const E& sl, const E& sr, const std::string& inputLatex) {
    Solution s;
    s.kind = "graph";
    s.anim = "graph";
    s.title = "Polar curve";
    s.inputLatex = inputLatex;
    std::string plain = toPlain(f);
    // "1 + (2*cos(theta))" -> "1 + 2*cos(theta)" so the shape patterns see one canonical form
    plain = std::regex_replace(plain, std::regex("\\+ \\(\\(-1\\)\\*((cos|sin)\\([^()]*\\))\\)"), "- $1");       // + ((-1)*sin(t)) -> - sin(t)
    plain = std::regex_replace(plain, std::regex("\\+ \\(\\(-([0-9.]+)\\)\\*((cos|sin)\\([^()]*\\))\\)"), "- $1*$2");  // + ((-2)*cos(t)) -> - 2*cos(t)
    plain = std::regex_replace(plain, std::regex("\\((-?[0-9.]+\\*(cos|sin)\\([^()]*\\))\\)"), "$1");
    std::string what, why;
    std::smatch mt;
    static const std::regex rose("^(-?[0-9.]+\\*)?(cos|sin)\\(([0-9]+)\\*theta\\)$");
    static const std::regex circ("^(-?[0-9.]+\\*)?(cos|sin)\\(theta\\)$");
    static const std::regex lima("^(-?[0-9.]+) [+-] ([0-9.]+\\*)?(cos|sin)\\(theta\\)$");
    static const std::regex spiral("^([0-9.]+\\*)?theta$");
    if (std::regex_match(plain, mt, rose)) {
        long k = std::stol(mt[3]);
        long petals = k % 2 ? k : 2 * k;
        what = "A rose with " + std::to_string(petals) + " petals";
        why = "In " + m("r = a\\" + std::string(mt[2]) + "(k\\theta)") + " the number of petals is " + m("k") + " when " + m("k") + " is odd and " + m("2k") +
              " when it is even. Here " + m("k = " + std::string(mt[3])) + ".";
        s.facts.push_back({"Petals", std::to_string(petals)});
    } else if (std::regex_match(plain, mt, circ)) {
        std::string a = mt[1].matched ? std::string(mt[1]).substr(0, mt[1].length() - 1) : "1";
        what = "A circle through the origin";
        why = m("r = a\\" + std::string(mt[2]) + "\\theta") + " is a circle of diameter " + m("|a|") + " that passes through the origin. Here " + m("a = " + a) + ".";
        s.facts.push_back({"Diameter", a[0] == '-' ? a.substr(1) : a});
    } else if (std::regex_match(plain, mt, lima)) {
        double a = std::atof(std::string(mt[1]).c_str()), b = mt[2].matched ? std::atof(std::string(mt[2]).c_str()) : 1.0;
        what = std::fabs(std::fabs(a) - b) < 1e-12 ? "A cardioid" : (std::fabs(a) < b ? "A limaçon with an inner loop" : "A limaçon");
        why = "In " + m("r = a \\pm b\\" + std::string(mt[3]) + "\\theta") + ": equal " + m("a") + " and " + m("b") + " give a heart-shaped cardioid; " + m("a < b") +
              " adds an inner loop.";
    } else if (std::regex_match(plain, mt, spiral)) {
        what = "An Archimedean spiral";
        why = "The distance grows in proportion to the angle, so each turn moves out by the same amount.";
    } else {
        what = "A polar curve";
    }
    Method mm{"Polar curve", {}};
    mm.steps.push_back({"Read it in polar form", "Each angle " + m("\\theta") + " gives a distance " + m("r") + " from the origin, and the point sits at " +
                        m("(r\\cos\\theta,\\ r\\sin\\theta)") + ".", toLatex(sl) + " = " + toLatex(sr), {}});
    // a few values of r
    auto prog = Program::compile(plain);
    if (prog) {
        static const std::pair<const char*, double> angles[] = {{"0", 0.0}, {"\\frac{\\pi}{6}", M_PI / 6}, {"\\frac{\\pi}{4}", M_PI / 4}, {"\\frac{\\pi}{3}", M_PI / 3}, {"\\frac{\\pi}{2}", M_PI / 2}};
        std::string tab = "\\begin{array}{c|c} \\theta & r \\\\ \\hline ";
        for (auto& [lab, th] : angles) {
            double r = prog->eval(0, 0, th);
            char b[32];
            snprintf(b, sizeof b, "%.3g", std::fabs(r) < 1e-12 ? 0.0 : r);
            tab += std::string(lab) + " & " + b + " \\\\ ";
        }
        tab += "\\end{array}";
        mm.steps.push_back({"Try a few angles", "Plot each pair and join them as " + m("\\theta") + " turns.", tab, {}});
    }
    if (!why.empty()) mm.steps.push_back({"Recognise the shape", why, "", {}});
    s.methods.push_back(mm);
    s.answerLatex = "\\text{" + what + "}";
    s.answers.push_back(s.answerLatex);
    PlotSpec ps;
    graphEquation(ps, sl, sr, toLatex(sl) + " = " + toLatex(sr));
    s.plot = ps;
    s.ok = true;
    return s;
}


std::string rawLatexPlain(const R& r) {
    auto e = toExpr(r);
    if (!e) return "that number";
    return toPlain(simplify(*e));
}

bool hasSymbol(const R& r) {
    if (r->t == T::Sym && r->op != "pi" && r->op != "e") return true;
    for (auto& k : r->kids) if (hasSymbol(k)) return true;
    return false;
}

// A division whose denominator is a constant equal to zero (1/0, x/0, 5/(2-2)): explain why it's undefined.
std::optional<std::string> literalDivisionByZero(const R& r) {
    if (r->t == T::Bin && (r->op == "/" || r->op == "\xC3\xB7") && r->kids.size() == 2 && !hasSymbol(r->kids[1])) {
        auto d = toExpr(r->kids[1]);
        if (d && isNumVal(simplify(*d), 0)) {
            bool zeroTop = false;
            if (!hasSymbol(r->kids[0])) if (auto n = toExpr(r->kids[0])) zeroTop = isNumVal(simplify(*n), 0);
            if (zeroTop) return std::string("0 \xC3\xB7 0 is undefined: every number times 0 is 0, so there is no single answer. Mathematicians call it indeterminate.");
            return std::string("Division by zero is undefined: no number multiplied by 0 can give ") + rawLatexPlain(r->kids[0]) + ".";
        }
    }
    for (auto& k : r->kids) if (auto m2 = literalDivisionByZero(k)) return m2;
    return std::nullopt;
}

}  // namespace

Solution solve(std::string_view input) {
    auto parsed = parse(input);
    if (!parsed) return fail(parsed.error().message);
    R root = *parsed;
    std::string inputLatex = rawLatex(root);
    if (auto why = literalDivisionByZero(root)) return fail(*why, inputLatex);
    Solution sol;
    if (isMatrixProblem(root)) {
        sol = solveMatrix(root, inputLatex);
    } else if (root->t == T::List) {
        if (root->op == "chain") return fail("Compound inequalities aren't supported yet. Try splitting into two inequalities.", inputLatex);
        std::vector<std::pair<E, E>> eqs;
        for (size_t li = 0; li < root->kids.size(); ++li) {
            const R& k = root->kids[li];
            if (k->t != T::Rel || k->op != "=") {
                std::string n = std::to_string(li + 1);
                std::string why = k->t == T::Rel ? "Line " + n + " is an inequality. Lines are solved together only when every line is an equation."
                                                 : "Line " + n + " has no \u201c=\u201d, so it isn't an equation. Solve it on its own, or give it an \u201c=\u201d to join the system.";
                return fail(why, inputLatex);
            }
            auto L = toExpr(k->kids[0]), Rr = toExpr(k->kids[1]);
            if (!L || !Rr) return fail("Invalid equation in the system", inputLatex);
            eqs.push_back({*L, *Rr});
        }
        sol = solveSystem(eqs, inputLatex);
    } else if (root->t == T::Rel) {
        sol = solveRel(root, inputLatex);
    } else if (root->t == T::Func && (sol = dispatchFunc(root, inputLatex), sol.ok || !sol.error.empty())) {
        // handled
    } else if (root->t == T::Fact) {
        auto a = toExpr(root->kids[0]);
        if (!a) return fail(a.error().message, inputLatex);
        E n = simplify(*a);
        if (!hasFreeVars(n)) sol = solveFactorial(n, inputLatex);
        else sol = simplifyExpression(func("fact", n), inputLatex);
    } else {
        auto e = toExpr(root);
        if (!e) return fail(e.error().message, inputLatex);
        E s = simplify(*e);
        if (!hasFreeVars(s)) {
            if (containsSym(s, "i") || containsSym(*e, "i")) sol = solveComplexArithmetic(root, inputLatex);
            else sol = solveArithmetic(root, inputLatex);
        } else {
            sol = simplifyExpression(*e, inputLatex);
        }
    }
    if (sol.inputLatex.empty()) sol.inputLatex = inputLatex;
    writeStory(sol);
    return sol;
}

PlotSpec graph(std::string_view input, std::string* error) {
    PlotSpec ps;
    auto parsed = parse(input);
    if (!parsed) { if (error) *error = parsed.error().message; return ps; }
    R root = *parsed;
    std::vector<R> items = root->t == T::List ? root->kids : std::vector<R>{root};
    for (auto& it : items) {
        if (it->t == T::Rel) {
            auto L = toExpr(it->kids[0]), Rr = toExpr(it->kids[1]);
            if (!L || !Rr) continue;
            graphEquation(ps, simplify(*L), simplify(*Rr), rawLatex(it));
        } else {
            auto e = toExpr(it);
            if (!e) { if (error) *error = e.error().message; continue; }
            E s = simplify(*e);
            std::vector<std::string> vars;
            collectVars(s, vars);
            if (vars.empty()) {
                ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(s), "y = " + toLatex(s)});
            } else if (vars.size() == 1) {
                ps.curves.push_back({PlotCurve::Type::Explicit, plainInX(s, vars[0]), "y = " + toLatex(s)});
            } else if (containsSym(s, "x") && !containsSym(s, "y") && !graphParams(vars).empty()) {
                ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(s), "y = " + toLatex(s)});
                for (auto& p : graphParams(vars)) if (std::ranges::find(ps.params, p) == ps.params.end()) ps.params.push_back(p);
            } else if (vars.size() == 2 && containsSym(s, "x") && containsSym(s, "y")) {
                ps.curves.push_back({PlotCurve::Type::Implicit, toPlain(s), toLatex(s) + " = 0"});
            }
        }
    }
    if (ps.curves.empty() && error && error->empty()) *error = "Nothing to graph";
    return ps;
}

std::string solveJson(std::string_view input) { return toJson(solve(input)); }

std::string latexJson(std::string_view input) {
    auto parsed = parse(input);
    if (!parsed) return "{\"ok\":false,\"error\":\"" + jsonEscape(parsed.error().message) + "\",\"pos\":" + std::to_string(parsed.error().position) + "}";
    return "{\"ok\":true,\"latex\":\"" + jsonEscape(rawLatex(*parsed)) + "\"}";
}

std::string graphJson(std::string_view input) {
    std::string err;
    PlotSpec ps = graph(input, &err);
    if (ps.curves.empty()) return "{\"ok\":false,\"error\":\"" + jsonEscape(err) + "\"}";
    std::string j = toJson(ps);
    return "{\"ok\":true," + j.substr(1);
}

}  // namespace me

// ============================================================ C API

struct me_program {
    me::Program program;
};

static char* dupString(const std::string& s) {
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (!p) return nullptr;
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

extern "C" {

char* me_solve(const char* input) { return dupString(me::solveJson(input ? input : "")); }
char* me_latex(const char* input) { return dupString(me::latexJson(input ? input : "")); }
char* me_graph(const char* input) { return dupString(me::graphJson(input ? input : "")); }
char* me_from_latex(const char* latex) { return dupString(me::latexToEngine(latex ? latex : "")); }

me_program* me_compile(const char* expr) {
    auto p = me::Program::compile(expr ? expr : "");
    if (!p) return nullptr;
    return new me_program{std::move(*p)};
}

double me_eval(const me_program* p, double x, double y, double theta) { return p ? p->program.eval(x, y, theta) : NAN; }

void me_eval_many(const me_program* p, const double* xs, const double* ys, int n, double* out) {
    if (!p || !xs || !out || n <= 0) return;
    for (int i = 0; i < n; ++i) out[i] = p->program.eval(xs[i], ys ? ys[i] : 0.0, xs[i]);
}

int me_uses_y(const me_program* p) { return p && p->program.usesY() ? 1 : 0; }
void me_program_free(me_program* p) { delete p; }
void me_free(char* s) { std::free(s); }

}  // extern "C"
