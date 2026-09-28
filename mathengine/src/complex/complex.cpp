// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Complex numbers ("z solver"): exact arithmetic with steps, polar/exponential forms,
// n-th roots via De Moivre, and equations involving conj(z), |z|, Re(z), Im(z).
#include <algorithm>
#include <cmath>
#include <functional>

#include "core/poly.h"
#include "solution/plotutil.h"
#include "solvers.h"

namespace me {

namespace {

std::string crTex(const CRat& z) { return toLatex(fromCRat(z)); }
std::string parenIfSum(const CRat& z) {
    std::string s = crTex(z);
    return (!z.re.isZero() && !z.im.isZero()) || z.re.isNeg() || z.im.isNeg() ? "\\left(" + s + "\\right)" : s;
}

CRat cmul(const CRat& a, const CRat& b) { return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re}; }

// Split an expression into real and imaginary parts, treating every free symbol as real.
std::optional<std::pair<E, E>> splitReIm(const E& e) {
    switch (e->kind) {
        case Kind::Num: return std::pair{e, num(0)};
        case Kind::Sym:
            if (e->name == "i") return std::pair{num(0), num(1)};
            return std::pair{e, num(0)};
        case Kind::Add: {
            std::vector<E> re, im;
            for (auto& a : e->args) {
                auto p = splitReIm(a);
                if (!p) return std::nullopt;
                re.push_back(p->first);
                im.push_back(p->second);
            }
            return std::pair{simplify(add(re)), simplify(add(im))};
        }
        case Kind::Mul: {
            E re = num(1), im = num(0);
            for (auto& a : e->args) {
                auto p = splitReIm(a);
                if (!p) return std::nullopt;
                E nr = simplify(sub(mul(re, p->first), mul(im, p->second)));
                E ni = simplify(add(mul(re, p->second), mul(im, p->first)));
                re = nr;
                im = ni;
            }
            return std::pair{expand(re), expand(im)};
        }
        case Kind::Pow: {
            auto b = splitReIm(e->args[0]);
            if (!b) return std::nullopt;
            if (isNumVal(b->second, 0) && !containsSym(e->args[1], "i")) return std::pair{e, num(0)};
            if (!isInteger(e->args[1])) return std::nullopt;
            long long n = e->args[1]->num.num().toInt64();
            if (std::llabs(n) > 8) return std::nullopt;
            E re = b->first, im = b->second;
            if (n < 0) {
                E d = add(pow(re, num(2)), pow(im, num(2)));
                re = simplify(divide(re, d));
                im = simplify(divide(neg(im), d));
                n = -n;
            }
            E rr = num(1), ri = num(0);
            for (long long k = 0; k < n; ++k) {
                E nr = simplify(sub(mul(rr, re), mul(ri, im)));
                E ni = simplify(add(mul(rr, im), mul(ri, re)));
                rr = expand(nr);
                ri = expand(ni);
            }
            return std::pair{rr, ri};
        }
        case Kind::Func: {
            const std::string& n = e->name;
            if (n == "conj" || n == "re" || n == "im" || n == "abs") {
                auto p = splitReIm(e->args[0]);
                if (!p) return std::nullopt;
                if (n == "conj") return std::pair{p->first, simplify(neg(p->second))};
                if (n == "re") return std::pair{p->first, num(0)};
                if (n == "im") return std::pair{p->second, num(0)};
                return std::pair{simplify(sqrtE(add(pow(p->first, num(2)), pow(p->second, num(2))))), num(0)};
            }
            for (auto& a : e->args) if (containsSym(a, "i")) return std::nullopt;
            return std::pair{e, num(0)};
        }
    }
    return std::nullopt;
}

}  // namespace

std::vector<E> complexRoots(const E& w0, int n, std::vector<Step>* steps) {
    std::vector<E> roots;
    E w = simplify(w0);
    E r = simplify(func("abs", w));
    E th = simplify(func("arg", w));
    if (isNumVal(w, 0)) return {num(0)};
    auto wz = evalComplex(w);
    if (!wz) return roots;
    bool exactTh = !isFunc(th, "arg");
    if (!exactTh) th = num(Rational::fromDecimal(fmtDouble(std::arg(*wz), 10)));
    E rn = simplify(pow(r, num(Rational(1, n))));
    if (steps) {
        steps->push_back({"Write in polar form", "Modulus " + m("r = " + toLatex(r)) + " and argument " + m("\\theta = " + toLatex(th)) + ".",
                          toLatex(w) + " = " + toLatex(r) + "\\left(\\cos\\left(" + toLatex(th) + "\\right) + i\\sin\\left(" + toLatex(th) + "\\right)\\right)", {}});
        steps->push_back({"Apply De Moivre's theorem", "There are exactly " + m(std::to_string(n)) + " roots, spaced evenly around a circle.",
                          "z_{k} = \\sqrt[" + std::to_string(n) + "]{r}\\left(\\cos\\frac{\\theta + 2\\pi k}{" + std::to_string(n) +
                              "} + i\\sin\\frac{\\theta + 2\\pi k}{" + std::to_string(n) + "}\\right), \\quad k = 0, \\dots, " + std::to_string(n - 1), {}});
    }
    std::string list;
    for (int k = 0; k < n; ++k) {
        E ang = simplify(divide(add(th, mul({num(2), PI(), num(k)})), num(n)));
        E c = simplify(func("cos", ang)), s = simplify(func("sin", ang));
        E z;
        if (containsFunc(c, "cos") || containsFunc(s, "sin")) {
            Cx v = std::polar(std::pow(std::abs(*wz), 1.0 / n), (std::arg(*wz) + 2 * M_PI * k) / n);
            auto rnd = [](double d) { return std::fabs(d) < 1e-12 ? num(0) : approxE(d); };
            z = simplify(add(rnd(v.real()), mul(rnd(v.imag()), IMAG())));
        } else {
            z = simplify(expand(mul(rn, add(c, mul(IMAG(), s)))));
        }
        roots.push_back(z);
        list += (k ? ", \\quad " : "") + std::string("z_{") + std::to_string(k) + "} = " + toLatex(z);
    }
    if (steps) steps->push_back({"List the roots", "", list, {}});
    return roots;
}

void addComplexFacts(Solution& sol, const E& z0) {
    E z = simplify(z0);
    auto c = asCRat(z);
    if (!c) return;
    E r = simplify(func("abs", z));
    sol.facts.push_back({"Real part", toLatex(num(c->re))});
    sol.facts.push_back({"Imaginary part", toLatex(num(c->im))});
    sol.facts.push_back({"Modulus", "\\left|z\\right| = " + toLatex(r)});
    sol.facts.push_back({"Conjugate", "\\overline{z} = " + crTex({c->re, -c->im})});
    if (!(c->re.isZero() && c->im.isZero())) {
        E th = simplify(func("arg", z));
        std::string tht = isFunc(th, "arg") ? numLatex(std::atan2(c->im.toDouble(), c->re.toDouble()), 6) : toLatex(th);
        sol.facts.push_back({"Argument", "\\theta = " + tht});
        sol.facts.push_back({"Polar form", toLatex(r) + "\\left(\\cos\\left(" + tht + "\\right) + i\\sin\\left(" + tht + "\\right)\\right)"});
        sol.facts.push_back({"Exponential form", toLatex(r) + " e^{i " + (tht.find_first_of("+-") != std::string::npos && tht[0] != '-' ? "\\left(" + tht + "\\right)" : tht) + "}"});
    }
    PlotSpec ps;
    ps.curves.clear();
    ps.points.push_back({c->re.toDouble(), c->im.toDouble(), "z = " + crTex(*c)});
    double R = std::max({std::fabs(c->re.toDouble()), std::fabs(c->im.toDouble()), 1.0}) * 1.6;
    ps.xmin = -R; ps.xmax = R; ps.ymin = -R; ps.ymax = R;
    ps.autoRange = false;
    sol.plot = ps;  // complex plane (Argand diagram)
}

// ------------------------------------------------------------------ arithmetic with steps

namespace {

class ComplexStepper {
public:
    std::vector<Step> steps;

    std::optional<CRat> eval(const R& r) {
        using T = RNode::T;
        switch (r->t) {
            case T::Num: return CRat{r->value, Rational(0)};
            case T::Sym:
                if (r->op == "i") return CRat{Rational(0), Rational(1)};
                return std::nullopt;
            case T::Neg: {
                auto a = eval(r->kids[0]);
                if (!a) return std::nullopt;
                return CRat{-a->re, -a->im};
            }
            case T::Bin: {
                auto a = eval(r->kids[0]);
                if (!a) return std::nullopt;
                if (r->op == "^") {
                    if (r->kids[1]->t != T::Num || !r->kids[1]->value.isInteger()) return std::nullopt;
                    return power(*a, r->kids[1]->value.num().toInt64());
                }
                auto b = eval(r->kids[1]);
                if (!b) return std::nullopt;
                const std::string& op = r->op;
                bool cplx = !a->im.isZero() || !b->im.isZero();
                if (op == "+" || op == "-") {
                    CRat res = op == "+" ? CRat{a->re + b->re, a->im + b->im} : CRat{a->re - b->re, a->im - b->im};
                    if (cplx)
                        steps.push_back({op == "+" ? "Add the complex numbers" : "Subtract the complex numbers", "Combine real parts and imaginary parts separately.",
                                         parenIfSum(*a) + " " + op + " " + parenIfSum(*b) + " = " + crTex(res), {}});
                    return res;
                }
                if (op == "*") {
                    CRat res = cmul(*a, *b);
                    if (!a->im.isZero() && !b->im.isZero()) {
                        Step s{"Multiply using FOIL", "Multiply each part, then use " + m("i^{2} = -1") + ".",
                               parenIfSum(*a) + parenIfSum(*b) + " = " + crTex(res), {}};
                        s.sub.push_back({"Expand", "",
                                         toLatex(num(a->re * b->re)) + " + " + toLatex(simplify(mul(num(a->re * b->im), IMAG()))) + " + " +
                                             toLatex(simplify(mul(num(a->im * b->re), IMAG()))) + " + " + toLatex(num(a->im * b->im)) + "i^{2}", {}});
                        s.sub.push_back({"Replace " + m("i^{2}") + " with " + m("-1"), "", crTex(res), {}});
                        steps.push_back(s);
                    } else if (cplx) {
                        steps.push_back({"Distribute", "", parenIfSum(*a) + " \\cdot " + parenIfSum(*b) + " = " + crTex(res), {}});
                    }
                    return res;
                }
                if (op == "/" || op == "div") {
                    Rational d = b->re * b->re + b->im * b->im;
                    if (d.isZero()) return std::nullopt;
                    CRat conj{b->re, -b->im};
                    CRat numr = cmul(*a, conj);
                    CRat res{numr.re / d, numr.im / d};
                    if (!b->im.isZero()) {
                        Step s{"Multiply by the conjugate", "Multiply numerator and denominator by " + m(crTex(conj)) + " to make the denominator real.",
                               "\\frac{" + crTex(*a) + "}{" + crTex(*b) + "} = " + crTex(res), {}};
                        s.sub.push_back({"", "", "\\frac{" + parenIfSum(*a) + parenIfSum(conj) + "}{" + parenIfSum(*b) + parenIfSum(conj) + "}", {}});
                        s.sub.push_back({"Denominator", m("(a+bi)(a-bi) = a^{2}+b^{2}"), "\\frac{" + crTex(numr) + "}{" + toLatex(num(d)) + "}", {}});
                        steps.push_back(s);
                    } else if (cplx) {
                        steps.push_back({"Divide each part", "", "\\frac{" + crTex(*a) + "}{" + crTex(*b) + "} = " + crTex(res), {}});
                    }
                    return res;
                }
                return std::nullopt;
            }
            case T::Abs: {
                auto a = eval(r->kids[0]);
                if (!a) return std::nullopt;
                E v = simplify(func("abs", fromCRat(*a)));
                auto c = asCRat(v);
                steps.push_back({"Find the modulus", m("|a+bi| = \\sqrt{a^{2}+b^{2}}"), "\\left|" + crTex(*a) + "\\right| = " + toLatex(v), {}});
                if (!c) { irrational = v; return std::nullopt; }
                return c;
            }
            case T::Func: {
                const std::string& n = r->op;
                if (n == "sqrt" && r->kids.size() == 1) {
                    auto a = eval(r->kids[0]);
                    if (!a || !a->im.isZero()) return std::nullopt;
                    E v = simplify(sqrtE(num(a->re)));
                    auto c = asCRat(v);
                    if (a->re.isNeg()) steps.push_back({"Square root of a negative number", m("\\sqrt{-a} = i\\sqrt{a}"), "\\sqrt{" + toLatex(num(a->re)) + "} = " + toLatex(v), {}});
                    if (!c) { irrational = v; return std::nullopt; }
                    return c;
                }
                if (n == "conj" || n == "re" || n == "im") {
                    auto a = eval(r->kids[0]);
                    if (!a) return std::nullopt;
                    CRat res = n == "conj" ? CRat{a->re, -a->im} : n == "re" ? CRat{a->re, Rational(0)} : CRat{a->im, Rational(0)};
                    std::string title = n == "conj" ? "Take the conjugate" : n == "re" ? "Take the real part" : "Take the imaginary part";
                    steps.push_back({title, n == "conj" ? "Flip the sign of the imaginary part." : "", crTex(res), {}});
                    return res;
                }
                return std::nullopt;
            }
            default: return std::nullopt;
        }
    }
    std::optional<E> irrational;

private:
    std::optional<CRat> power(const CRat& a, long long n) {
        if (a.re.isZero() && a.im.isOne()) {
            long long k = ((n % 4) + 4) % 4;
            CRat res = k == 0 ? CRat{1, 0} : k == 1 ? CRat{0, 1} : k == 2 ? CRat{-1, 0} : CRat{0, -1};
            steps.push_back({"Powers of i repeat every 4", m("i^{1} = i,\\ i^{2} = -1,\\ i^{3} = -i,\\ i^{4} = 1") + ". Use the remainder of " + m(std::to_string(n) + " \\div 4") + ".",
                             "i^{" + std::to_string(n) + "} = i^{" + std::to_string(k) + "} = " + crTex(res), {}});
            return res;
        }
        if (std::llabs(n) > 64) return std::nullopt;
        CRat base = a;
        long long m_ = n;
        if (n < 0) {
            Rational d = a.re * a.re + a.im * a.im;
            if (d.isZero()) return std::nullopt;
            base = {a.re / d, -a.im / d};
            m_ = -n;
        }
        CRat res{1, 0};
        for (long long k = 0; k < m_; ++k) res = cmul(res, base);
        if (!a.im.isZero()) {
            if (m_ == 2)
                steps.push_back({"Square the binomial", m("(a+bi)^{2} = a^{2} + 2abi + b^{2}i^{2}"), parenIfSum(a) + "^{2} = " + crTex(res), {}});
            else
                steps.push_back({"Raise to the power " + m(std::to_string(n)), "Multiply repeatedly (or use De Moivre's theorem).", parenIfSum(a) + "^{" + std::to_string(n) + "} = " + crTex(res), {}});
        }
        return res;
    }
};

}  // namespace

Solution solveComplexArithmetic(const R& raw, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "complex";
    sol.anim = "complex";
    sol.title = "Simplify the complex expression";
    sol.inputLatex = inputLatex;
    ComplexStepper cs;
    auto v = cs.eval(raw);
    E result;
    if (v) result = fromCRat(*v);
    else {
        auto e = toExpr(raw);
        if (!e) { sol.ok = false; sol.error = e.error().message; return sol; }
        result = simplify(*e);
        cs.steps.push_back({"Simplify", "", toLatex(result), {}});
    }
    if (cs.steps.empty()) cs.steps.push_back({"Simplify", "", toLatex(result), {}});
    cs.steps.push_back({"Write in standard form", m("a + bi"), toLatex(result), {}});
    sol.methods.push_back({"Complex arithmetic", cs.steps});
    sol.answerLatex = toLatex(result);
    sol.answers.push_back(sol.answerLatex);
    addComplexFacts(sol, result);
    return sol;
}

// ------------------------------------------------------------------ equations

Solution solveComplexEquation(const E& lhs, const E& rhs, const std::string& var, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "complex";
    sol.anim = "complex";
    sol.title = "Solve for " + m(var);
    sol.inputLatex = inputLatex;
    std::vector<std::string> used;
    collectVars(lhs, used);
    collectVars(rhs, used);
    std::string a = "a", b = "b";
    if (std::ranges::find(used, a) != used.end() || std::ranges::find(used, b) != used.end()) { a = "p"; b = "q"; }
    E zab = add(sym(a), mul(sym(b), IMAG()));
    E f = simplify(sub(substitute(lhs, var, zab), substitute(rhs, var, zab)));
    Method mm{"Write z = a + bi", {}};
    mm.steps.push_back({"Let " + m(var + " = " + a + " + " + b + "i"), "where " + m(a) + " and " + m(b) + " are real numbers.",
                        eqTex(simplify(substitute(lhs, var, zab)), simplify(substitute(rhs, var, zab))), {}});
    auto parts = splitReIm(f);
    if (!parts) { sol.ok = false; sol.error = "Can't separate real and imaginary parts of this equation."; return sol; }
    E re = simplify(parts->first), im = simplify(parts->second);
    mm.steps.push_back({"Separate real and imaginary parts", "A complex number is zero only when both its real and imaginary parts are zero.",
                        "\\begin{cases} " + toLatex(re) + " = 0 \\\\ " + toLatex(im) + " = 0 \\end{cases}", {}});
    SystemResult sys = solveSystemCore({{re, num(0)}, {im, num(0)}}, {a, b});
    if (sys.status != SystemResult::Status::Unique || sys.methods.empty()) {
        sol.ok = false;
        sol.error = sys.status == SystemResult::Status::None ? "This equation has no solution." : "Couldn't solve the resulting system.";
        return sol;
    }
    std::vector<E> zs;
    std::string abText;
    for (auto& v : sys.solutions) {
        zs.push_back(simplify(add(v[0], mul(v[1], IMAG()))));
        abText += (abText.empty() ? "" : " \\quad \\text{or} \\quad ") + a + " = " + toLatex(v[0]) + ", \\ " + b + " = " + toLatex(v[1]);
    }
    Step s{"Solve the system for " + m(a) + " and " + m(b), "", abText, {}};
    for (auto& st : sys.methods.front().steps) s.sub.push_back(st);
    mm.steps.push_back(s);
    if (zs.empty()) { sol.ok = false; sol.error = "No solution found."; return sol; }
    std::string ans;
    for (size_t i = 0; i < zs.size(); ++i) {
        std::string t = var + (zs.size() > 1 ? "_{" + std::to_string(i + 1) + "}" : "") + " = " + toLatex(zs[i]);
        sol.answers.push_back(t);
        ans += (i ? ", \\quad " : "") + t;
    }
    mm.steps.push_back({"Combine", "", ans, {}});
    sol.answerLatex = ans;
    sol.methods.push_back(mm);
    if (zs.size() == 1) addComplexFacts(sol, zs[0]);
    return sol;
}

}  // namespace me
