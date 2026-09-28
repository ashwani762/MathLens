// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Ordinary differential equations (first order: direct, separable, linear;
// second order: constant-coefficient linear) and Taylor series, with steps.
#include <algorithm>
#include <cmath>

#include "calculus/calculus.h"
#include "core/poly.h"
#include "solution/plotutil.h"
#include "solvers.h"

namespace me {

namespace {

const std::string Y = "y", Y1 = "y'", Y2 = "y''";

E coeffOf(const E& f, const std::string& v) { return simplify(derivativeOf(f, v)); }

bool freeOf(const E& e, std::initializer_list<const char*> vars) {
    for (auto v : vars) if (containsSym(e, v)) return false;
    return true;
}

std::string intTex(const E& f, const std::string& v) { return "\\int " + toLatex(f) + " \\, d" + v; }

// family of explicit solutions for a few values of C, for the graph and Explain view
PlotSpec family(const E& sol, const std::string& c) {
    PlotSpec ps;
    for (int k = -2; k <= 2; ++k) {
        E yk = simplify(substitute(sol, c, num(k)));
        if (containsSym(yk, "C_1") || containsSym(yk, "C_2")) yk = simplify(substitute(substitute(yk, "C_1", num(k)), "C_2", num(1)));
        std::vector<std::string> vars;
        collectVars(yk, vars);
        if (vars.size() > 1) continue;
        ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(yk), c + " = " + std::to_string(k)});
    }
    ps.xmin = -4; ps.xmax = 4; ps.ymin = -6; ps.ymax = 6;
    return ps;
}

}  // namespace

bool isODE(const E& e) { return containsSym(e, Y1) || containsSym(e, Y2); }

Solution solveODE(const E& lhs, const E& rhs, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "ode";
    sol.title = "Solve the differential equation";
    sol.inputLatex = inputLatex;
    E f = expand(sub(lhs, rhs));
    E x = sym("x"), y = sym(Y);
    Method meth{"Differential equation", {}};
    auto finish = [&](const E& solution, const std::string& c) {
        sol.answerLatex = "y = " + toLatex(solution);
        sol.answers.push_back(sol.answerLatex);
        sol.methods.push_back(meth);
        sol.plot = family(solution, c);
        sol.facts.push_back({"General solution", "\\text{one curve for every value of } " + c});
        return sol;
    };

    if (containsSym(f, Y2)) {
        // a y'' + b y' + c y = g(x)
        E a = coeffOf(f, Y2), b = coeffOf(f, Y1), c = coeffOf(f, Y);
        E rest = simplify(expand(sub(f, add({mul(a, sym(Y2)), mul(b, sym(Y1)), mul(c, y)}))));
        if (!isNum(a) || !isNum(b) || !isNum(c) || !freeOf(rest, {"y", "y'", "y''"})) {
            sol.ok = false;
            sol.error = "Only second-order equations with constant coefficients are supported.";
            return sol;
        }
        Rational A = a->num, B = b->num, C = c->num;
        meth.steps.push_back({"Write the characteristic equation", "Try " + m("y = e^{rx}") + ": each derivative brings down a factor " + m("r") + ".",
                           toLatex(simplify(add({mul(num(A), pow(sym("r"), num(2))), mul(num(B), sym("r")), num(C)}))) + " = 0", {}});
        Rational D = B * B - Rational(4) * A * C;
        E C1 = sym("C_1"), C2 = sym("C_2");
        E yh;
        if (D.sign() > 0) {
            auto roots = quadraticRoots(A, B, C);
            meth.steps.push_back({"Two real roots", "The discriminant " + m(toLatex(num(D))) + " is positive.",
                               "r_{1} = " + toLatex(roots[1]) + ", \\quad r_{2} = " + toLatex(roots[0]), {}});
            yh = add(mul(C1, pow(EUL(), mul(roots[1], x))), mul(C2, pow(EUL(), mul(roots[0], x))));
        } else if (D.isZero()) {
            Rational r = -B / (Rational(2) * A);
            meth.steps.push_back({"A repeated root", "The discriminant is zero, so multiply the second solution by " + m("x") + ".", "r = " + toLatex(num(r)), {}});
            yh = mul(add(C1, mul(C2, x)), pow(EUL(), mul(num(r), x)));
        } else {
            Rational al = -B / (Rational(2) * A);
            E be = simplify(divide(sqrtE(num(-D)), num((Rational(2) * A).abs())));
            meth.steps.push_back({"Complex roots", "Roots " + m("\\alpha \\pm \\beta i") + " give oscillating solutions (Euler's formula).",
                               "r = " + (al.isZero() ? std::string("") : toLatex(num(al)) + " ") + "\\pm " + toLatex(simplify(mul(be, IMAG()))), {}});
            yh = mul(pow(EUL(), mul(num(al), x)), add(mul(C1, func("cos", mul(be, x))), mul(C2, func("sin", mul(be, x)))));
        }
        yh = simplify(yh);
        E full = yh;
        if (!isNumVal(rest, 0)) {
            // constant forcing: particular solution y_p = -rest / c
            if (isNum(rest) && !C.isZero()) {
                E yp = num(-rest->num / C);
                meth.steps.push_back({"Particular solution", "A constant right-hand side has a constant particular solution.", "y_{p} = " + toLatex(yp), {}});
                full = simplify(add(yh, yp));
            } else {
                sol.ok = false;
                sol.error = "Only homogeneous or constant-forced second-order equations are supported.";
                return sol;
            }
        }
        meth.steps.push_back({"Combine", "The general solution has two free constants.", "y = " + toLatex(full), {}});
        return finish(full, "C_1");
    }

    // first order: solve for y'
    E a = coeffOf(f, Y1);
    if (containsSym(a, Y1) || isNumVal(a, 0)) { sol.ok = false; sol.error = "Couldn't isolate y'."; return sol; }
    E R = simplify(divide(neg(simplify(sub(f, mul(a, sym(Y1))))), a));
    meth.steps.push_back({"Isolate the derivative", "", "y' = " + toLatex(R), {}});
    E Cc = sym("C");

    if (!containsSym(R, Y)) {
        auto F = antiderivativeOf(R, "x");
        if (!F) { sol.ok = false; sol.error = "Couldn't integrate the right-hand side."; return sol; }
        meth.steps.push_back({"Integrate both sides", m("y") + " is an antiderivative of the right-hand side.", "y = " + intTex(R, "x") + " = " + toLatex(*F) + " + C", {}});
        return finish(simplify(add(*F, Cc)), "C");
    }

    // linear: y' = p(x) y + q(x)
    E p = simplify(derivativeOf(R, Y));
    if (!containsSym(p, Y)) {
        E q = simplify(sub(R, mul(p, y)));
        if (isNumVal(q, 0) || !containsSym(p, "x")) {
            // separable or homogeneous linear: y' = p(x) y -> y = C e^{∫p}
            if (isNumVal(q, 0)) {
                auto P = antiderivativeOf(p, "x");
                if (P) {
                    meth.steps.push_back({"Separate the variables", "Divide by " + m("y") + " and multiply by " + m("dx") + ".", "\\frac{dy}{y} = " + toLatex(p) + " \\, dx", {}});
                    meth.steps.push_back({"Integrate both sides", "", "\\ln\\left|y\\right| = " + toLatex(*P) + " + c", {}});
                    E yy = simplify(mul(Cc, pow(EUL(), *P)));
                    meth.steps.push_back({"Exponentiate", "Absorb " + m("\\pm e^{c}") + " into a single constant " + m("C") + ".", "y = " + toLatex(yy), {}});
                    return finish(yy, "C");
                }
            }
        }
        // integrating factor μ = e^{-∫p}
        auto P = antiderivativeOf(neg(p), "x");
        if (P) {
            E mu = simplify(pow(EUL(), *P));
            meth.steps.push_back({"Recognise a linear equation", "Write it as " + m("y' + P(x)\\,y = Q(x)") + ".",
                               "y' + " + toLatex(simplify(neg(p))) + "\\,y = " + toLatex(q), {}});
            meth.steps.push_back({"Integrating factor", m("\\mu = e^{\\int P\\,dx}") + " turns the left side into " + m("(\\mu y)'") + ".", "\\mu = " + toLatex(mu), {}});
            auto I = antiderivativeOf(simplify(mul(mu, q)), "x");
            if (I) {
                meth.steps.push_back({"Integrate", "", "\\mu y = " + intTex(simplify(mul(mu, q)), "x") + " = " + toLatex(*I) + " + C", {}});
                E yy = expand(divide(add(*I, Cc), mu));
                meth.steps.push_back({"Divide by the integrating factor", "", "y = " + toLatex(yy), {}});
                return finish(yy, "C");
            }
        }
    }

    // separable: y' = g(x) h(y)
    std::vector<E> fs = R->kind == Kind::Mul ? R->args : std::vector<E>{R};
    std::vector<E> gx, hy;
    for (auto& t : fs) {
        if (containsSym(t, "x") && containsSym(t, Y)) { gx.clear(); hy.clear(); break; }
        (containsSym(t, Y) ? hy : gx).push_back(t);
    }
    if (!hy.empty()) {
        E g = simplify(mul(gx)), h = simplify(mul(hy));
        auto H = antiderivativeOf(simplify(pow(h, num(-1))), Y);
        auto G = antiderivativeOf(g, "x");
        if (H && G) {
            meth.steps.push_back({"Separate the variables", "Collect " + m("y") + " with " + m("dy") + " and " + m("x") + " with " + m("dx") + ".",
                               "\\frac{dy}{" + toLatex(h) + "} = " + toLatex(g) + " \\, dx", {}});
            meth.steps.push_back({"Integrate both sides", "", toLatex(*H) + " = " + toLatex(*G) + " + C", {}});
            std::vector<E> roots;
            Solution s2 = solveEquation(*H, add(*G, Cc), Y, "", false, &roots);
            if (s2.ok && !roots.empty()) {
                meth.steps.push_back({"Solve for " + m("y"), "", "y = " + toLatex(roots.back()), {}});
                return finish(roots.back(), "C");
            }
            sol.answerLatex = toLatex(*H) + " = " + toLatex(*G) + " + C";
            sol.answers.push_back(sol.answerLatex);
            sol.methods.push_back(meth);
            return sol;
        }
    }
    sol.ok = false;
    sol.error = "This differential equation isn't separable or linear, so I can't solve it yet.";
    return sol;
}

Solution solveTaylor(const E& f0, const std::string& var, const E& a0, int n, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "taylor";
    sol.title = "Taylor series";
    sol.inputLatex = inputLatex;
    E f = simplify(f0), a = simplify(a0), x = sym(var);
    Method meth{"Taylor expansion", {}};
    meth.steps.push_back({"The Taylor formula", "Match the function's value and first " + std::to_string(n) + " derivatives at " + m(var + " = " + toLatex(a)) + ".",
                       "f(" + var + ") \\approx \\sum_{k=0}^{" + std::to_string(n) + "} \\frac{f^{(k)}(" + toLatex(a) + ")}{k!}" +
                           (isNumVal(a, 0) ? var + "^{k}" : "\\left(" + var + " - " + toLatex(a) + "\\right)^{k}"), {}});
    Step table{"Compute the derivatives at " + m(var + " = " + toLatex(a)), "", "", {}};
    std::vector<E> terms;
    E d = f;
    BigInt fact(1);
    for (int k = 0; k <= n; ++k) {
        if (k > 0) { d = simplify(derivativeOf(d, var)); fact *= BigInt(k); }
        E v = simplify(substitute(d, var, a));
        if (isUndefined(v)) { sol.ok = false; sol.error = "The function isn't differentiable at that point."; return sol; }
        std::string dk = k == 0 ? "f" : (k <= 3 ? "f" + std::string(k, '\'') : "f^{(" + std::to_string(k) + ")}");
        table.sub.push_back({"", "", dk + "(" + toLatex(a) + ") = " + toLatex(v), {}});
        E shift = isNumVal(a, 0) ? x : add(x, neg(a));
        terms.push_back(mul({v, pow(num(Rational(fact)), num(-1)), k == 0 ? num(1) : pow(shift, num(k))}));
    }
    meth.steps.push_back(table);
    E poly = simplify(add(terms));
    // series read in ascending powers: 1 + x + x^2/2 + ...
    std::string asc;
    for (auto& t : terms) {
        E ts = simplify(t);
        if (isNumVal(ts, 0)) continue;
        std::string tl = toLatex(ts);
        if (asc.empty()) asc = tl;
        else if (tl[0] == '-') asc += " - " + tl.substr(1);
        else asc += " + " + tl;
    }
    if (asc.empty()) asc = "0";
    meth.steps.push_back({"Assemble the polynomial", "", "f(" + var + ") \\approx " + asc, {}});
    sol.methods.push_back(meth);
    sol.answerLatex = toLatex(f) + " \\approx " + asc;
    sol.answers.push_back(asc);
    // f first, then the partial sums T_1 .. T_n so the app can animate the approximation improving
    std::vector<std::pair<E, std::string>> curves{{f, "f"}};
    for (int k = 1; k <= n; ++k) {
        std::vector<E> part(terms.begin(), terms.begin() + k + 1);
        E pk = simplify(add(part));
        if (k > 1 && eq(pk, curves.back().first)) continue;  // skip orders that add a zero term
        curves.push_back({pk, "T_{" + std::to_string(k) + "}"});
    }
    PlotSpec ps = plotFunctions(curves, var);
    if (auto av = evalReal(a)) { ps.xmin = *av - 4; ps.xmax = *av + 4; fitY(ps, f, var); }
    sol.plot = ps;
    sol.facts.push_back({"Order", std::to_string(n)});
    return sol;
}

}  // namespace me
