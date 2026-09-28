// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Limits: direct substitution, factor & cancel, L'Hôpital's rule, degree comparison at infinity,
// and a numeric fallback with constant recognition.
#include <algorithm>
#include <cmath>

#include "calculus/calculus.h"
#include "solution/plotutil.h"
#include "core/poly.h"

namespace me {

namespace {

std::optional<E> recognize(double v) {
    if (!std::isfinite(v)) return std::nullopt;
    for (int d = 1; d <= 12; ++d) {
        double n = v * d;
        if (std::fabs(n - std::round(n)) < 1e-7 * std::max(1.0, std::fabs(n))) return num(Rational((long long)std::llround(n), d));
    }
    struct C { double val; E e; };
    std::vector<C> consts = {{M_PI, PI()}, {M_E, EUL()}, {std::log(2.0), func("ln", num(2))}, {std::sqrt(2.0), sqrtE(num(2))},
                             {std::sqrt(3.0), sqrtE(num(3))}};
    for (auto& c : consts)
        for (int d = 1; d <= 6; ++d)
            for (int k = -12; k <= 12; ++k) {
                if (k == 0) continue;
                if (std::fabs(v - c.val * k / d) < 1e-7) return simplify(mul(num(Rational(k, d)), c.e));
            }
    for (int k = -4; k <= 4; ++k)
        if (k != 0 && std::fabs(v - std::exp((double)k)) < 1e-7 * std::exp((double)k)) return simplify(pow(EUL(), num(k)));
    return std::nullopt;
}

bool isFiniteValue(const E& v) {
    if (isUndefined(v) || containsSym(v, "inf")) return false;
    auto d = evalReal(v);
    return d && std::isfinite(*d);
}

std::string limTex(const std::string& var, const E& p, const std::string& body) {
    return "\\lim_{" + var + " \\to " + toLatex(p) + "} " + body;
}

double pointValue(const E& p) {
    if (isSym(p, "inf")) return INFINITY;
    if (p->kind == Kind::Mul && containsSym(p, "inf")) return -INFINITY;
    auto v = evalReal(p);
    return v ? *v : NAN;
}

double sampleNear(const E& f, const std::string& var, double p, int side, double h) {
    double x = std::isinf(p) ? (p > 0 ? 1.0 / h : -1.0 / h) : p + side * h;
    auto v = evalReal(f, {{var, x}});
    return v ? *v : NAN;
}

}  // namespace

Solution solveLimit(const E& f0, const std::string& var, const E& point, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "limit";
    sol.anim = "limit";
    sol.title = "Evaluate the limit";
    sol.inputLatex = inputLatex;
    E f = simplify(f0);
    E p = simplify(point);
    double pv = pointValue(p);
    Method meth{"Limit rules", {}};
    auto finish = [&](const E& value, const std::string& how) {
        meth.steps.push_back({how, "", toLatex(value), {}});
        sol.answerLatex = inputLatex + " = " + toLatex(value);
        sol.answers.push_back(toLatex(value));
        if (!isNum(value) && isFiniteValue(value)) sol.facts.push_back({"Decimal", numLatex(*evalReal(value), 10)});
        sol.methods.push_back(meth);
        PlotSpec ps = plotFunctions({{f, "f(" + var + ")"}}, var);
        if (std::isfinite(pv)) {
            ps.xmin = pv - 6; ps.xmax = pv + 6;
            fitY(ps, f, var);
            if (auto v = evalReal(value)) ps.points.push_back({pv, *v, "limit"});
        }
        sol.plot = ps;
        return sol;
    };
    std::string body = toLatex(f);
    if (f->kind == Kind::Add) body = "\\left(" + body + "\\right)";

    if (!containsSym(f, var)) return finish(f, "The expression does not depend on " + m(var));

    // 1. direct substitution (finite point)
    if (std::isfinite(pv)) {
        E v = simplify(substitute(f, var, p));
        if (isFiniteValue(v)) {
            meth.steps.push_back({"Substitute " + m(var + " = " + toLatex(p)), "The function is continuous here, so plug the value in.",
                                  limTex(var, p, body) + " = " + toLatex(v), {}});
            return finish(v, "Simplify");
        }
        meth.steps.push_back({"Try direct substitution", "Substituting gives an indeterminate or undefined form.", "", {}});
    }

    auto [N, D] = numDen(f);
    // 2. rational functions
    if (auto rf = toRationalFunction(f, var)) {
        Poly P = rf->first, Q = rf->second;
        if (std::isfinite(pv)) {
            auto pr = asCRat(p);
            if (pr && pr->im.isZero()) {
                Rational x0 = pr->re;
                // cancel common factors (toRationalFunction already reduced by the gcd)
                E reduced = simplify(divide(P.toExpr(var), Q.toExpr(var)));
                auto fzN = factorQ(Poly::from(N, var).value_or(P));
                auto fzD = factorQ(Poly::from(D, var).value_or(Q));
                meth.steps.push_back({"Factor", "Factor the numerator and the denominator.",
                                      "\\frac{" + toLatex(factorizationToExpr(fzN, var)) + "}{" + toLatex(factorizationToExpr(fzD, var)) + "}", {}});
                meth.steps.push_back({"Cancel common factors", "", limTex(var, p, toLatex(reduced)), {}});
                if (!Q.eval(x0).isZero()) {
                    E v = num(P.eval(x0) / Q.eval(x0));
                    meth.steps.push_back({"Substitute " + m(var + " = " + toLatex(p)), "", "", {}});
                    return finish(v, "Evaluate");
                }
                // still a zero denominator -> infinite limit or DNE
                double l = sampleNear(f, var, pv, -1, 1e-6), r = sampleNear(f, var, pv, 1, 1e-6);
                if (l > 0 && r > 0) return finish(INF(), "Both one-sided limits grow without bound");
                if (l < 0 && r < 0) return finish(simplify(neg(INF())), "Both one-sided limits decrease without bound");
                meth.steps.push_back({"Compare one-sided limits",
                                      "From the left the function goes to " + m(l < 0 ? "-\\infty" : "\\infty") + ", from the right to " +
                                          m(r < 0 ? "-\\infty" : "\\infty") + ".",
                                      "", {}});
                sol.methods.push_back(meth);
                sol.answerLatex = "\\text{The limit does not exist}";
                sol.answers.push_back("\\text{DNE}");
                sol.plot = plotFunctions({{f, "f(" + var + ")"}}, var);
                return sol;
            }
        } else {
            int dn = P.deg(), dd = Q.deg();
            std::string xn = dd == 1 ? var : var + "^{" + std::to_string(dd) + "}";
            meth.steps.push_back({"Divide by the highest power", "Divide numerator and denominator by " + m(xn) +
                                                                      ". Terms like " + m("\\frac{c}{" + var + "^{k}}") + " go to 0.", "", {}});
            if (dn < dd) return finish(num(0), "The denominator grows faster, so the limit is 0");
            if (dn == dd) return finish(num(P.lead() / Q.lead()), "Equal degrees: the limit is the ratio of leading coefficients");
            int sgn = (P.lead() / Q.lead()).sign();
            if (pv < 0 && (dn - dd) % 2 == 1) sgn = -sgn;
            return finish(sgn > 0 ? INF() : simplify(neg(INF())), "The numerator grows faster");
        }
    }

    // 3. L'Hôpital's rule for 0/0 or inf/inf
    E n = N, d = D;
    for (int iter = 0; iter < 5; ++iter) {
        double nv = sampleNear(n, var, pv, 1, std::isfinite(pv) ? 1e-9 : 1e-9);
        double dv = sampleNear(d, var, pv, 1, std::isfinite(pv) ? 1e-9 : 1e-9);
        bool zz = std::fabs(nv) < 1e-6 && std::fabs(dv) < 1e-6;
        bool ii = std::fabs(nv) > 1e6 && std::fabs(dv) > 1e6;
        if (!(zz || ii) || isNumVal(d, 1)) break;
        E dn = simplify(derivativeOf(n, var)), dd = simplify(derivativeOf(d, var));
        meth.steps.push_back({"Apply L'Hôpital's rule",
                              std::string("The limit has the form ") + m(zz ? "\\frac{0}{0}" : "\\frac{\\infty}{\\infty}") +
                                  ", so differentiate the numerator and the denominator.",
                              limTex(var, p, "\\frac{" + toLatex(dn) + "}{" + toLatex(dd) + "}"), {}});
        n = dn;
        d = dd;
        E q = simplify(divide(n, d));
        if (std::isfinite(pv)) {
            E v = simplify(substitute(q, var, p));
            if (isFiniteValue(v)) return finish(v, "Substitute " + m(var + " = " + toLatex(p)));
        } else if (auto rf = toRationalFunction(q, var); rf && rf->first.deg() <= rf->second.deg()) {
            if (rf->first.deg() < rf->second.deg()) return finish(num(0), "The denominator grows faster, so the limit is 0");
            return finish(num(rf->first.lead() / rf->second.lead()), "Ratio of leading coefficients");
        } else if (!containsSym(q, var)) return finish(q, "Simplify");
    }

    // 4. numeric estimate
    double a1 = sampleNear(f, var, pv, 1, 1e-5), a2 = sampleNear(f, var, pv, 1, 1e-7);
    double b2 = sampleNear(f, var, pv, -1, 1e-7);
    bool twoSided = std::isfinite(pv);
    if (std::isfinite(a2) && std::fabs(a1 - a2) < 1e-3 * std::max(1.0, std::fabs(a2)) &&
        (!twoSided || (std::isfinite(b2) && std::fabs(a2 - b2) < 1e-3 * std::max(1.0, std::fabs(a2))))) {
        meth.steps.push_back({"Estimate numerically", "Evaluate the function closer and closer to the point.",
                              "f \\approx " + numLatex(a2, 8), {}});
        if (auto r = recognize(a2)) return finish(*r, "The values approach");
        return finish(num(Rational::fromDecimal(fmtDouble(a2, 8))), "The values approach");
    }
    if (std::fabs(a2) > 1e6 && (!twoSided || std::fabs(b2) > 1e6) && (!twoSided || (a2 > 0) == (b2 > 0)))
        return finish(a2 > 0 ? INF() : simplify(neg(INF())), "The function grows without bound");
    sol.methods.push_back(meth);
    sol.answerLatex = "\\text{The limit does not exist}";
    sol.answers.push_back("\\text{DNE}");
    sol.plot = plotFunctions({{f, "f(" + var + ")"}}, var);
    return sol;
}

}  // namespace me
