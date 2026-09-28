// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Equations, inequalities and algebraic simplification with steps.
#include <algorithm>
#include <cmath>
#include <functional>

#include "calculus/calculus.h"
#include "solution/plotutil.h"
#include "core/poly.h"
#include "solvers.h"

namespace me {

std::string eqTex(const E& l, const E& r, const std::string& rel) { return toLatex(l) + " " + rel + " " + toLatex(r); }

std::string varPick(const std::vector<std::string>& vars) {
    for (const char* pref : {"x", "y", "z", "t", "n", "a", "theta"})
        if (std::ranges::find(vars, std::string(pref)) != vars.end()) return pref;
    return vars.empty() ? "x" : vars[0];
}

namespace {

std::string ordinalSub(size_t i) { return "_{" + std::to_string(i + 1) + "}"; }

double numVal(const E& e) {
    auto v = evalReal(e);
    return v ? *v : NAN;
}

void sortRoots(std::vector<E>& r) {
    std::ranges::stable_sort(r, [](const E& a, const E& b) { return numVal(a) < numVal(b); });
    // dedupe
    std::vector<E> out;
    for (auto& x : r) {
        bool dup = false;
        for (auto& y : out) if (eq(x, y) || std::fabs(numVal(x) - numVal(y)) < 1e-12) { dup = true; break; }
        if (!dup) out.push_back(x);
    }
    r = out;
}

// Rational -> "add"/"subtract" wording
std::string moveWord(const E& term, bool fromLeft = true) {
    (void)fromLeft;
    if (isNegativeTerm(term)) return "Add " + m(toLatex(simplify(neg(term)))) + " to both sides";
    return "Subtract " + m(toLatex(term)) + " from both sides";
}

// ------------------------------------------------------------------ quadratic helpers

struct Quad {
    Rational a, b, c, D;
    std::vector<E> roots;  // real roots ascending, or complex pair
    bool real = true;
    std::string pm;        // compact "x = ..." right-hand side with ±
};

Quad analyzeQuadratic(const Rational& a, const Rational& b, const Rational& c) {
    Quad q{a, b, c, b * b - Rational(4) * a * c, {}, true, ""};
    const E I = IMAG();
    if (q.D.sign() >= 0) {
        E sq = simplify(sqrtE(num(q.D)));
        if (isNum(sq)) {
            Rational s = sq->num;
            q.roots = {num((-b - s) / (Rational(2) * a)), num((-b + s) / (Rational(2) * a))};
            sortRoots(q.roots);
            if (q.roots.size() == 1) q.pm = toLatex(q.roots[0]);
            return q;
        }
        auto [k, rad] = splitCoeff(sq);
        // integer normalisation of (-b ± k rad) / 2a
        Rational B = -b, K = k, A2 = Rational(2) * a;
        BigInt L(1);
        for (auto* r : {&B, &K, &A2}) L = L / BigInt::gcd(L, r->den()) * r->den();
        BigInt Bi = (B * Rational(L)).num(), Ki = (K * Rational(L)).num(), Ai = (A2 * Rational(L)).num();
        BigInt g = BigInt::gcd(BigInt::gcd(Bi, Ki), Ai);
        Bi = Bi / g; Ki = Ki / g; Ai = Ai / g;
        if (Ai.isNeg()) { Ai = -Ai; Bi = -Bi; }
        std::string radTex = (Ki.isOne() ? "" : Ki.toString()) + toLatex(rad);
        std::string numer = Bi.isZero() ? "\\pm " + radTex : Bi.toString() + " \\pm " + radTex;
        q.pm = Ai.isOne() ? numer : "\\frac{" + numer + "}{" + Ai.toString() + "}";
        E r1 = simplify(divide(sub(num(Rational(Bi)), mul(num(Rational(Ki)), rad)), num(Rational(Ai))));
        E r2 = simplify(divide(add(num(Rational(Bi)), mul(num(Rational(Ki)), rad)), num(Rational(Ai))));
        q.roots = {r1, r2};
        return q;
    }
    q.real = false;
    Rational re = -b / (Rational(2) * a);
    E im = simplify(divide(sqrtE(num(-q.D)), num((Rational(2) * a).abs())));
    q.roots = {simplify(sub(num(re), mul(im, I))), simplify(add(num(re), mul(im, I)))};
    std::string imTex = isNumVal(im, 1) ? "i" : toLatex(mul(im, I));
    if (isNum(im)) imTex = toLatex(simplify(mul(im, I)));
    q.pm = (re.isZero() ? "" : toLatex(num(re)) + " ") + "\\pm " + imTex;
    return q;
}

// ------------------------------------------------------------------ polynomial solving

struct PolySolved {
    std::vector<E> real;     // exact (or numeric) real roots
    std::vector<E> complex;  // non-real roots
    std::vector<Method> methods;
    bool numeric = false;
    bool allReals = false;   // identity 0 = 0
    bool none = false;       // contradiction c = 0
};

E X(const std::string& v) { return sym(v); }

std::string polyEq(const Poly& p, const std::string& var) { return toLatex(p.toExpr(var)) + " = 0"; }

// The quadratic a x^2 + b x + c = 0: several methods
void quadraticMethods(const Poly& P, const std::string& var, PolySolved& out, const std::vector<Step>& prelude) {
    Rational a = P.coeff(2), b = P.coeff(1), c = P.coeff(0);
    Quad q = analyzeQuadratic(a, b, c);
    E x = X(var);
    auto rootsText = [&](std::vector<Step>& st) {
        if (q.roots.size() == 1) st.push_back({"There is one repeated solution", "", var + " = " + toLatex(q.roots[0]), {}});
        else st.push_back({"The solutions are", "", var + ordinalSub(0) + " = " + toLatex(q.roots[0]) + ", \\quad " + var + ordinalSub(1) + " = " + toLatex(q.roots[1]), {}});
    };
    // --- factoring (only with rational roots)
    bool rationalRoots = q.real && std::ranges::all_of(q.roots, [](const E& r) { return isNum(r); });
    if (rationalRoots) {
        Method mf{"Factoring", prelude};
        if (c.isZero()) {
            E inner = simplify(add(mul(num(a), x), num(b)));
            mf.steps.push_back({"Factor out " + m(var), "Both terms contain " + m(var) + ".", toLatex(x) + "\\left(" + toLatex(inner) + "\\right) = 0", {}});
        } else if (b.isZero()) {
            // difference of squares a x^2 - s^2
            Factorization fz = factorQ(P);
            mf.steps.push_back({"Use the difference of squares", m("a^{2} - b^{2} = (a-b)(a+b)"), toLatex(factorizationToExpr(fz, var)) + " = 0", {}});
        } else {
            Factorization fz = factorQ(P);
            Rational ac = a * c;
            // find m, n with m n = ac, m + n = b (integers if possible)
            auto [cont, prim] = P.primitive();
            Rational A = prim.coeff(2), B = prim.coeff(1), C = prim.coeff(0);
            Rational AC = A * C;
            std::string splitTex;
            if (AC.isInteger() && B.isInteger()) {
                BigInt acv = AC.num();
                for (auto& d : divisors(acv)) {
                    for (int s : {1, -1}) {
                        BigInt m1 = d * BigInt(s);
                        if (m1.isZero()) continue;
                        BigInt n1 = acv / m1;
                        if (m1 * n1 == acv && Rational(m1 + n1) == B) {
                            E expr = add({mul(num(A), pow(x, num(2))), mul(num(Rational(m1)), x), mul(num(Rational(n1)), x), num(C)});
                            // raw (unsimplified) print of the split
                            std::string t = toLatex(simplify(mul(num(A), pow(x, num(2)))));
                            auto term = [&](const Rational& k, const std::string& body) {
                                std::string kt = k.abs().isOne() && !body.empty() ? "" : k.abs().toString();
                                return (k.isNeg() ? " - " : " + ") + kt + body;
                            };
                            t += term(Rational(m1), var) + term(Rational(n1), var) + term(C, "");
                            splitTex = t;
                            (void)expr;
                            goto found;
                        }
                    }
                }
            }
        found:
            if (!splitTex.empty()) {
                mf.steps.push_back({"Split the middle term",
                                    "Find two numbers that multiply to " + m("a\\cdot c = " + AC.toString()) + " and add up to " + m("b = " + B.toString()) + ".",
                                    splitTex + " = 0", {}});
                mf.steps.push_back({"Factor by grouping", "", toLatex(factorizationToExpr(fz, var)) + " = 0", {}});
            } else {
                mf.steps.push_back({"Factor", "", toLatex(factorizationToExpr(fz, var)) + " = 0", {}});
            }
            (void)ac;
        }
        mf.steps.push_back({"Use the zero product property", "A product is zero when at least one factor is zero.", "", {}});
        rootsText(mf.steps);
        out.methods.push_back(mf);
    }
    // --- square root method when b = 0
    if (b.isZero() && !c.isZero()) {
        Method ms{"Square root property", prelude};
        E rhs = num(-c / a);
        ms.steps.push_back({"Isolate " + m(var + "^{2}"), "", toLatex(pow(x, num(2))) + " = " + toLatex(rhs), {}});
        if ((-c / a).sign() > 0) {
            ms.steps.push_back({"Take the square root of both sides", "Remember both the positive and the negative root.",
                                var + " = \\pm\\sqrt{" + toLatex(rhs) + "}", {}});
            rootsText(ms.steps);
        } else {
            ms.steps.push_back({"Take the square root of both sides", "The square root of a negative number is imaginary: " + m("\\sqrt{-1} = i"),
                                var + " = \\pm\\sqrt{" + toLatex(rhs) + "} = " + q.pm, {}});
        }
        out.methods.push_back(ms);
    }
    // --- quadratic formula (always)
    {
        Method mq{"Quadratic formula", prelude};
        mq.steps.push_back({"Identify the coefficients", "Compare with " + m("a" + var + "^{2} + b" + var + " + c = 0") + ".",
                            "a = " + toLatex(num(a)) + ", \\quad b = " + toLatex(num(b)) + ", \\quad c = " + toLatex(num(c)), {}});
        auto par = [](const Rational& r) { return r.isNeg() || !r.isInteger() ? "\\left(" + toLatex(num(r)) + "\\right)" : toLatex(num(r)); };
        mq.steps.push_back({"Write the quadratic formula", "", var + " = \\frac{-b \\pm \\sqrt{b^{2} - 4ac}}{2a}", {}});
        mq.steps.push_back({"Substitute the values", "",
                            var + " = \\frac{-" + par(b) + " \\pm \\sqrt{" + par(b) + "^{2} - 4 \\cdot " + par(a) + " \\cdot " + par(c) + "}}{2 \\cdot " + par(a) + "}",
                            {}});
        Step disc{"Compute the discriminant", "", "b^{2} - 4ac = " + toLatex(num(q.D)), {}};
        if (q.D.sign() > 0) disc.text = "It is positive, so there are two real solutions.";
        else if (q.D.isZero()) disc.text = "It is zero, so there is exactly one (repeated) solution.";
        else disc.text = "It is negative, so there are no real solutions: the two solutions are complex conjugates.";
        mq.steps.push_back(disc);
        if (q.roots.size() == 2 && !q.pm.empty()) mq.steps.push_back({"Simplify", "", var + " = " + q.pm, {}});
        rootsText(mq.steps);
        out.methods.push_back(mq);
    }
    // --- completing the square
    if (!b.isZero()) {
        Method mc{"Completing the square", prelude};
        Rational p = b / a, r = c / a;
        E xe = x;
        if (!a.isOne()) mc.steps.push_back({"Divide by the leading coefficient", "", toLatex(Poly({r, p, Rational(1)}).toExpr(var)) + " = 0", {}});
        mc.steps.push_back({"Move the constant to the right side", "", toLatex(simplify(add(pow(xe, num(2)), mul(num(p), xe)))) + " = " + toLatex(num(-r)), {}});
        Rational h = p / Rational(2);
        mc.steps.push_back({"Add " + m(toLatex(num(h * h))) + " to both sides", "Half of the " + m(var) + "-coefficient, squared: " + m("\\left(\\frac{" + toLatex(num(p)) + "}{2}\\right)^{2}"),
                            toLatex(simplify(add({pow(xe, num(2)), mul(num(p), xe), num(h * h)}))) + " = " + toLatex(num(-r + h * h)), {}});
        mc.steps.push_back({"Write as a perfect square", "", toLatex(pow(add(xe, num(h)), num(2))) + " = " + toLatex(num(-r + h * h)), {}});
        mc.steps.push_back({"Take the square root", "", toLatex(simplify(add(xe, num(h)))) + " = \\pm" + toLatex(simplify(sqrtE(num(-r + h * h)))), {}});
        rootsText(mc.steps);
        out.methods.push_back(mc);
    }
    if (q.real) out.real = q.roots;
    else out.complex = q.roots;
}

// Solve P(x) = 0 for any degree
PolySolved solvePoly(const Poly& P0, const std::string& var, const std::vector<Step>& prelude, bool complexMode) {
    PolySolved out;
    E x = X(var);
    if (P0.isZero()) { out.allReals = true; return out; }
    if (P0.deg() == 0) { out.none = true; return out; }
    // make integer coefficients
    std::vector<Step> pre = prelude;
    Poly P = P0;
    auto [cont, prim] = P.primitive();
    if (!cont.isOne() && P.deg() >= 1) {
        bool frac = false;
        for (auto& c : P.coeffs()) if (!c.isInteger()) frac = true;
        if (frac) pre.push_back({"Clear the fractions", "Multiply both sides by " + m(toLatex(num(cont.inv().abs()))) + ".", polyEq(prim, var), {}});
        else if (!cont.abs().isOne()) pre.push_back({"Divide both sides by " + m(toLatex(num(cont))), "", polyEq(prim, var), {}});
        else pre.push_back({"Multiply both sides by -1", "", polyEq(prim, var), {}});
        P = prim;
    }
    if (P.deg() == 1) {
        Method ml{"Isolate the variable", pre};
        Rational a = P.coeff(1), b = P.coeff(0);
        if (!b.isZero()) ml.steps.push_back({moveWord(num(b)), "", toLatex(simplify(mul(num(a), x))) + " = " + toLatex(num(-b)), {}});
        E r = num(-b / a);
        if (!a.isOne()) ml.steps.push_back({"Divide both sides by " + m(toLatex(num(a))), "", var + " = " + toLatex(r), {}});
        out.real = {r};
        out.methods.push_back(ml);
        return out;
    }
    if (P.deg() == 2) {
        quadraticMethods(P, var, out, pre);
        return out;
    }
    int n = P.deg();
    // two-term: a x^n + c = 0
    int nz = 0;
    for (auto& c : P.coeffs()) if (!c.isZero()) ++nz;
    if (nz == 2 && !P.coeff(0).isZero()) {
        Method mr{"Take the n-th root", pre};
        E w = num(-P.coeff(0) / P.lead());
        mr.steps.push_back({"Isolate " + m(var + "^{" + std::to_string(n) + "}"), "", toLatex(pow(x, num(n))) + " = " + toLatex(w), {}});
        std::vector<Step> rs;
        auto roots = complexRoots(w, n, &rs);
        for (auto& r : roots) {
            auto z = evalComplex(r);
            if (z && std::fabs(z->imag()) < 1e-12) out.real.push_back(simplify(r));
            else out.complex.push_back(r);
        }
        if (complexMode || out.real.empty()) {
            for (auto& s : rs) mr.steps.push_back(s);
        } else {
            mr.steps.push_back({"Take the real " + std::string(n % 2 ? "" : "positive and negative ") + "root", "",
                                var + " = " + (n % 2 ? std::string("") : "\\pm ") + toLatex(simplify(pow(num(w->num.abs()), num(Rational(1, n))))), {}});
        }
        sortRoots(out.real);
        out.methods.push_back(mr);
        return out;
    }
    // biquadratic a x^4 + b x^2 + c
    if (n == 4 && P.coeff(1).isZero() && P.coeff(3).isZero()) {
        Method mb{"Substitution", pre};
        mb.steps.push_back({"Substitute " + m("u = " + var + "^{2}"), "The equation only contains even powers.",
                            toLatex(Poly({P.coeff(0), P.coeff(2), P.coeff(4)}).toExpr("u")) + " = 0", {}});
        Quad q = analyzeQuadratic(P.coeff(4), P.coeff(2), P.coeff(0));
        std::string us;
        for (size_t i = 0; i < q.roots.size(); ++i) us += (i ? ", \\quad " : "") + std::string("u = ") + toLatex(q.roots[i]);
        mb.steps.push_back({"Solve the quadratic in u", "Use the quadratic formula.", us, {}});
        for (auto& u : q.roots) {
            auto roots = complexRoots(u, 2, nullptr);
            for (auto& r : roots) {
                auto z = evalComplex(r);
                if (z && std::fabs(z->imag()) < 1e-12) out.real.push_back(simplify(r));
                else out.complex.push_back(simplify(r));
            }
        }
        sortRoots(out.real);
        mb.steps.push_back({"Substitute back " + m(var + " = \\pm\\sqrt{u}"), "", "", {}});
        out.methods.push_back(mb);
        return out;
    }
    // rational root theorem + synthetic division
    Method mr{"Rational root theorem", pre};
    Poly rest = P;
    std::vector<Rational> found = P.rationalRoots();
    if (!found.empty()) {
        BigInt a0 = P.coeff(0).num(), an = P.lead().num();
        mr.steps.push_back({"List possible rational roots", "Any rational root " + m("\\frac{p}{q}") + " has " + m("p") + " dividing " +
                                                                 m(a0.toString()) + " and " + m("q") + " dividing " + m(an.toString()) + ".", "", {}});
    }
    for (auto& r : found) {
        while (rest.deg() >= 1 && rest.eval(r).isZero()) {
            Poly lin = Poly::linear(Rational(1), -r);
            Poly q, rem;
            Poly::divmod(rest, lin, q, rem);
            Step s{"Test " + m(var + " = " + toLatex(num(r))), m("P(" + toLatex(num(r)) + ") = 0") + ", so " + m(toLatex(lin.toExpr(var))) + " is a factor.",
                   "\\left(" + toLatex(lin.toExpr(var)) + "\\right)\\left(" + toLatex(q.toExpr(var)) + "\\right) = 0", {}};
            // synthetic division detail
            std::string row;
            for (int k = rest.deg(); k >= 0; --k) row += (k == rest.deg() ? "" : ", \\ ") + toLatex(num(rest.coeff(k)));
            std::string qrow;
            for (int k = q.deg(); k >= 0; --k) qrow += (k == q.deg() ? "" : ", \\ ") + toLatex(num(q.coeff(k)));
            s.sub.push_back({"Synthetic division by " + m(toLatex(num(r))), "Coefficients " + m(row) + " give the quotient coefficients " + m(qrow) + " with remainder 0.", "", {}});
            mr.steps.push_back(s);
            out.real.push_back(num(r));
            rest = q;
        }
    }
    if (rest.deg() == 2) {
        Quad q = analyzeQuadratic(rest.coeff(2), rest.coeff(1), rest.coeff(0));
        mr.steps.push_back({"Solve the remaining quadratic", "Use the quadratic formula on " + m(toLatex(rest.toExpr(var)) + " = 0") + ".",
                            var + " = " + (q.pm.empty() && !q.roots.empty() ? toLatex(q.roots[0]) : q.pm), {}});
        if (q.real) for (auto& r : q.roots) out.real.push_back(r);
        else for (auto& r : q.roots) out.complex.push_back(r);
    } else if (rest.deg() == 1) {
        out.real.push_back(num(-rest.coeff(0) / rest.coeff(1)));
    } else if (rest.deg() >= 3) {
        auto nr = rest.numericRoots();
        mr.steps.push_back({"Approximate the remaining roots", "The factor " + m(toLatex(rest.toExpr(var))) + " has no rational roots; use a numerical method (Durand–Kerner).", "", {}});
        out.numeric = true;
        for (auto& z : nr) {
            if (std::fabs(z.imag()) < 1e-9) out.real.push_back(approxE(z.real()));
            else out.complex.push_back(add(approxE(z.real()), mul(approxE(z.imag()), IMAG())));
        }
    }
    sortRoots(out.real);
    out.methods.push_back(mr);
    return out;
}

// denominators (bases with negative powers) that contain var, from the unsimplified input
void collectDenominators(const E& e, const std::string& var, std::vector<E>& out) {
    if (e->kind == Kind::Pow && isNum(e->args[1]) && e->args[1]->num.isNeg() && containsSym(e->args[0], var)) {
        E d = simplify(e->args[0]);
        for (auto& o : out) if (eq(o, d)) return;
        out.push_back(d);
    }
    for (auto& a : e->args) collectDenominators(a, var, out);
}

// ------------------------------------------------------------------ linear (balance method) steps

// true when the (unsimplified) expression contains a product or power of a sum
bool hasBrackets(const E& e) {
    if (e->kind == Kind::Mul) for (auto& a : e->args) if (a->kind == Kind::Add) return true;
    if (e->kind == Kind::Pow && e->args[0]->kind == Kind::Add) return true;
    for (auto& a : e->args) if (hasBrackets(a)) return true;
    return false;
}

Method linearBalance(const E& L, const E& R, const std::string& var, E& root, bool& special, std::string& specialText) {
    Method m_{"Balance method", {}};
    E x = X(var);
    auto lp = Poly::from(L, var), rp = Poly::from(R, var);
    Poly Lp = *lp, Rp = *rp;
    if (hasBrackets(L) || hasBrackets(R)) {
        m_.steps.push_back({"Expand and simplify each side", "Use the distributive property " + m("a(b+c) = ab + ac") + " and combine like terms.",
                            toLatex(Lp.toExpr(var)) + " = " + toLatex(Rp.toExpr(var)), {}});
    }
    if (Lp.deg() > 1 || Rp.deg() > 1) {
        // x^2 terms cancel
        Poly d = Lp - Rp;
        m_.steps.push_back({"Move everything to the left side", "The squared terms cancel.", toLatex(d.toExpr(var)) + " = 0", {}});
        Lp = d;
        Rp = Poly();
    }
    // clear fractions
    BigInt lcd(1);
    for (auto* p : {&Lp, &Rp}) for (auto& c : p->coeffs()) lcd = lcd / BigInt::gcd(lcd, c.den()) * c.den();
    if (!lcd.isOne()) {
        Lp = Lp.scale(Rational(lcd));
        Rp = Rp.scale(Rational(lcd));
        m_.steps.push_back({"Multiply both sides by " + m(lcd.toString()), "This clears all fractions.",
                            toLatex(Lp.toExpr(var)) + " = " + toLatex(Rp.toExpr(var)), {}});
    }
    Rational a1 = Lp.coeff(1), b1 = Lp.coeff(0), a2 = Rp.coeff(1), b2 = Rp.coeff(0);
    // keep the variable on the side with the larger coefficient
    if (a1.isZero() && !a2.isZero()) {
        std::swap(a1, a2);
        std::swap(b1, b2);
        m_.steps.push_back({"Swap the sides", "", toLatex(Poly({b1, a1}).toExpr(var)) + " = " + toLatex(Poly({b2, a2}).toExpr(var)), {}});
    }
    if (!a2.isZero()) {
        E term = simplify(mul(num(a2), x));
        Step s{moveWord(term), "", toLatex(Poly({b1, a1 - a2}).toExpr(var)) + " = " + toLatex(num(b2)), {}};
        s.sub.push_back({"", "", toLatex(Poly({b1, a1}).toExpr(var)) + (a2.isNeg() ? " + " + toLatex(simplify(neg(term))) : " - " + toLatex(term)) + " = " +
                                     toLatex(Poly({b2, a2}).toExpr(var)) + (a2.isNeg() ? " + " + toLatex(simplify(neg(term))) : " - " + toLatex(term)), {}});
        m_.steps.push_back(s);
        a1 = a1 - a2;
        a2 = Rational(0);
    }
    if (a1.isZero()) {
        special = true;
        specialText = b1 == b2 ? "all" : "none";
        m_.steps.push_back({b1 == b2 ? "The equation is always true" : "The equation is never true",
                            b1 == b2 ? "Both sides are identical, so every value of " + m(var) + " is a solution."
                                     : "The variable cancels and leaves a false statement, so there is no solution.",
                            toLatex(num(b1)) + (b1 == b2 ? " = " : " \\ne ") + toLatex(num(b2)), {}});
        return m_;
    }
    if (!b1.isZero()) {
        Step s{moveWord(num(b1)), "", toLatex(simplify(mul(num(a1), x))) + " = " + toLatex(num(b2 - b1)), {}};
        std::string op = b1.isNeg() ? " + " + toLatex(num(-b1)) : " - " + toLatex(num(b1));
        s.sub.push_back({"", "", toLatex(Poly({b1, a1}).toExpr(var)) + op + " = " + toLatex(num(b2)) + op, {}});
        m_.steps.push_back(s);
    }
    Rational r = (b2 - b1) / a1;
    if (!a1.isOne()) {
        Step s{a1.isMinusOne() ? std::string("Multiply both sides by ") + m("-1") : "Divide both sides by " + m(toLatex(num(a1))), "", var + " = " + toLatex(num(r)), {}};
        if (!a1.isMinusOne())
            s.sub.push_back({"", "", "\\frac{" + toLatex(simplify(mul(num(a1), x))) + "}{" + toLatex(num(a1)) + "} = \\frac{" + toLatex(num(b2 - b1)) + "}{" + toLatex(num(a1)) + "}", {}});
        m_.steps.push_back(s);
    }
    root = num(r);
    return m_;
}

// ------------------------------------------------------------------ isolation (inverse operations)

struct Branch {
    E lhs, rhs;
};

class Isolator {
public:
    Isolator(std::string var, bool complexMode) : v_(std::move(var)), cx_(complexMode) {}
    std::vector<Step> steps;
    bool usedPeriodic = false;
    bool squared = false;

    // returns solutions (expressions possibly containing integer parameter n)
    std::optional<std::vector<E>> run(E lhs, E rhs, int depth = 0) {
        if (depth > 20) return std::nullopt;
        lhs = simplify(lhs);
        rhs = simplify(rhs);
        if (!containsSym(lhs, v_) && containsSym(rhs, v_)) std::swap(lhs, rhs);
        if (containsSym(rhs, v_)) {
            lhs = simplify(sub(lhs, rhs));
            rhs = num(0);
        }
        if (isSym(lhs, v_)) return std::vector<E>{rhs};
        if (!containsSym(lhs, v_)) return std::nullopt;
        auto rec = [&](E l, E r, const std::string& title, const std::string& text = "") -> std::optional<std::vector<E>> {
            l = simplify(l);
            r = simplify(r);
            steps.push_back({title, text, eqTex(l, r), {}});
            return run(l, r, depth + 1);
        };
        switch (lhs->kind) {
            case Kind::Add: {
                std::vector<E> with, without;
                for (auto& t : lhs->args) (containsSym(t, v_) ? with : without).push_back(t);
                if (with.size() != 1) return std::nullopt;
                E others = simplify(add(without));
                return rec(with[0], sub(rhs, others), moveWord(others));
            }
            case Kind::Mul: {
                std::vector<E> with, without;
                for (auto& t : lhs->args) (containsSym(t, v_) ? with : without).push_back(t);
                if (with.size() != 1) {
                    if (isNumVal(rhs, 0)) {
                        // zero product property
                        steps.push_back({"Use the zero product property", "Set each factor equal to zero.", "", {}});
                        std::vector<E> all;
                        for (auto& fct : with) {
                            Isolator sub(v_, cx_);
                            auto r = sub.run(fct, num(0), depth + 1);
                            if (!r) return std::nullopt;
                            for (auto& s : sub.steps) steps.push_back(s);
                            all.insert(all.end(), r->begin(), r->end());
                        }
                        return all;
                    }
                    return std::nullopt;
                }
                E c = simplify(mul(without));
                if (isNumVal(c, -1)) return rec(with[0], neg(rhs), "Multiply both sides by " + m("-1"));
                return rec(with[0], divide(rhs, c), "Divide both sides by " + m(toLatex(c)));
            }
            case Kind::Pow: {
                const E& b = lhs->args[0];
                const E& ex = lhs->args[1];
                if (containsSym(b, v_) && !containsSym(ex, v_)) {
                    if (isNumVal(ex, -1)) return rec(b, pow(rhs, num(-1)), "Take the reciprocal of both sides");
                    if (isNum(ex) && ex->num.isInteger()) {
                        long long n = ex->num.num().toInt64();
                        if (n < 0) return rec(pow(b, num(-n)), pow(rhs, num(-1)), "Take the reciprocal of both sides");
                        if (cx_) {
                            std::vector<Step> rs;
                            auto roots = complexRoots(rhs, (int)n, &rs);
                            for (auto& s : rs) steps.push_back(s);
                            std::vector<E> all;
                            for (auto& r : roots) {
                                Isolator sub(v_, cx_);
                                auto s = sub.run(b, r, depth + 1);
                                if (!s) return std::nullopt;
                                all.insert(all.end(), s->begin(), s->end());
                            }
                            return all;
                        }
                        auto rv = evalReal(rhs);
                        if (n % 2 == 0) {
                            if (rv && *rv < 0) {
                                steps.push_back({"No real solution", "An even power can never be negative.", "", {}});
                                return std::vector<E>{};
                            }
                            E root = simplify(pow(rhs, num(Rational(1, n))));
                            steps.push_back({"Take the " + (n == 2 ? std::string("square") : std::to_string(n) + "th") + " root of both sides",
                                             "Remember the positive and the negative root.", toLatex(b) + " = \\pm " + toLatex(root), {}});
                            std::vector<E> all;
                            for (E r : {root, simplify(neg(root))}) {
                                Isolator sub(v_, cx_);
                                auto s = sub.run(b, r, depth + 1);
                                if (!s) return std::nullopt;
                                for (auto& st : sub.steps) steps.push_back(st);
                                all.insert(all.end(), s->begin(), s->end());
                            }
                            return all;
                        }
                        return rec(b, pow(rhs, num(Rational(1, n))), "Take the " + std::to_string(n) + "th root of both sides");
                    }
                    if (isNum(ex)) {
                        // rational power p/q: raise to q/p
                        Rational e = ex->num;
                        auto rv = evalReal(rhs);
                        if (e.den().isEven() && rv && *rv < 0 && e.num().sign() > 0) {
                            steps.push_back({"No real solution", "An even root is never negative.", "", {}});
                            return std::vector<E>{};
                        }
                        squared = squared || e.den().isEven();
                        std::string t = e == Rational(1, 2) ? "Square both sides" : "Raise both sides to the power " + m(toLatex(num(e.inv())));
                        return rec(b, pow(rhs, num(e.inv())), t);
                    }
                    return std::nullopt;
                }
                if (!containsSym(b, v_) && containsSym(ex, v_)) {
                    auto rv = evalReal(rhs);
                    if (rv && *rv <= 0 && !cx_) {
                        steps.push_back({"No real solution", "An exponential expression is always positive.", "", {}});
                        return std::vector<E>{};
                    }
                    if (isSym(b, "e")) return rec(ex, func("ln", rhs), "Take the natural logarithm of both sides", m("\\ln(e^{u}) = u"));
                    // try to write rhs as a power of b
                    E lg = simplify(func("log", {b, rhs}));
                    return rec(ex, lg, "Take the logarithm base " + m(toLatex(b)) + " of both sides", m("\\log_{b}(b^{u}) = u"));
                }
                return std::nullopt;
            }
            case Kind::Func: {
                const std::string& fn = lhs->name;
                if (fn == "ln") return rec(lhs->args[0], pow(EUL(), rhs), "Exponentiate both sides", m("e^{\\ln(u)} = u"));
                if (fn == "log" && lhs->args.size() == 2)
                    return rec(lhs->args[1], pow(lhs->args[0], rhs), "Rewrite in exponential form", m("\\log_{b}(u) = c \\iff u = b^{c}"));
                if (fn == "abs") {
                    auto rv = evalReal(rhs);
                    if (rv && *rv < 0) {
                        steps.push_back({"No solution", "An absolute value is never negative.", "", {}});
                        return std::vector<E>{};
                    }
                    steps.push_back({"Split the absolute value", m("|u| = c \\iff u = c \\text{ or } u = -c"),
                                     toLatex(lhs->args[0]) + " = " + toLatex(rhs) + " \\quad \\text{or} \\quad " + toLatex(lhs->args[0]) + " = " + toLatex(simplify(neg(rhs))), {}});
                    std::vector<E> all;
                    for (E r : {rhs, simplify(neg(rhs))}) {
                        Isolator sub(v_, cx_);
                        auto s = sub.run(lhs->args[0], r, depth + 1);
                        if (!s) return std::nullopt;
                        for (auto& st : sub.steps) steps.push_back(st);
                        all.insert(all.end(), s->begin(), s->end());
                    }
                    return all;
                }
                if (fn == "sin" || fn == "cos" || fn == "tan") return trig(fn, lhs->args[0], rhs, depth);
                if (fn == "asin") return rec(lhs->args[0], func("sin", rhs), "Take the sine of both sides");
                if (fn == "acos") return rec(lhs->args[0], func("cos", rhs), "Take the cosine of both sides");
                if (fn == "atan") return rec(lhs->args[0], func("tan", rhs), "Take the tangent of both sides");
                return std::nullopt;
            }
            default: return std::nullopt;
        }
    }

private:
    std::string v_;
    bool cx_;

    std::optional<std::vector<E>> trig(const std::string& fn, const E& u, const E& rhs, int depth) {
        auto rv = evalReal(rhs);
        if (!rv) return std::nullopt;
        if ((fn == "sin" || fn == "cos") && std::fabs(*rv) > 1 + 1e-12) {
            steps.push_back({"No real solution", m("\\" + fn + "(u)") + " is always between " + m("-1") + " and " + m("1") + ".", "", {}});
            return std::vector<E>{};
        }
        usedPeriodic = true;
        E n = sym("n");
        E pi = PI();
        std::vector<E> branches;
        E alpha = simplify(func(fn == "sin" ? "asin" : fn == "cos" ? "acos" : "atan", rhs));
        std::string text;
        if (fn == "sin") {
            branches = {simplify(add(alpha, mul({num(2), pi, n}))), simplify(add(sub(pi, alpha), mul({num(2), pi, n})))};
            text = "The sine is " + m(toLatex(rhs)) + " at " + m(toLatex(alpha)) + " and " + m(toLatex(simplify(sub(pi, alpha)))) + ", repeating every " + m("2\\pi") + ".";
        } else if (fn == "cos") {
            branches = {simplify(add(alpha, mul({num(2), pi, n}))), simplify(add(neg(alpha), mul({num(2), pi, n})))};
            if (isNumVal(alpha, 0) || eq(alpha, pi)) branches.pop_back();
            text = "The cosine is " + m(toLatex(rhs)) + " at " + m("\\pm " + toLatex(alpha)) + ", repeating every " + m("2\\pi") + ".";
        } else {
            branches = {simplify(add(alpha, mul(pi, n)))};
            text = "The tangent repeats every " + m("\\pi") + ".";
        }
        std::string bl;
        for (size_t i = 0; i < branches.size(); ++i) bl += (i ? " \\quad \\text{or} \\quad " : "") + toLatex(u) + " = " + toLatex(branches[i]);
        steps.push_back({"Use the inverse " + std::string(fn == "sin" ? "sine" : fn == "cos" ? "cosine" : "tangent"), text + " Here " + m("n") + " is any integer.", bl, {}});
        std::vector<E> all;
        for (auto& b : branches) {
            Isolator sub(v_, cx_);
            auto s = sub.run(u, b, depth + 1);
            if (!s) return std::nullopt;
            for (auto& st : sub.steps) steps.push_back(st);
            all.insert(all.end(), s->begin(), s->end());
        }
        return all;
    }
};

// numeric real roots of f on [lo, hi]
std::vector<double> numericRealRoots(const E& f, const std::string& var, double lo = -60, double hi = 60) {
    std::vector<double> roots;
    const int N = 6000;
    auto F = [&](double x) {
        auto v = evalReal(f, {{var, x}});
        return v ? *v : NAN;
    };
    double px = lo, pf = F(lo);
    for (int k = 1; k <= N; ++k) {
        double x = lo + (hi - lo) * k / N;
        double fx = F(x);
        if (std::isfinite(pf) && std::isfinite(fx)) {
            if (pf == 0) roots.push_back(px);
            else if ((pf < 0) != (fx < 0)) {
                double a = px, b = x, fa = pf;
                for (int it = 0; it < 100; ++it) {
                    double mid = (a + b) / 2, fm = F(mid);
                    if (!std::isfinite(fm)) break;
                    if ((fa < 0) == (fm < 0)) { a = mid; fa = fm; } else b = mid;
                }
                double r = (a + b) / 2;
                // reject poles: |f| must be small near r
                double fr = F(r);
                if (std::isfinite(fr) && std::fabs(fr) < 1e-6) roots.push_back(r);
            }
        }
        px = x;
        pf = fx;
    }
    std::vector<double> out;
    for (double r : roots) if (out.empty() || std::fabs(out.back() - r) > 1e-7) out.push_back(r);
    return out;
}

E approxNum(double v) { return approxE(v); }

bool checkSolution(const E& lhs, const E& rhs, const std::string& var, const E& r) {
    auto rv = evalComplex(r);
    if (!rv) return true;
    auto a = evalComplex(lhs, {{var, *rv}}), b = evalComplex(rhs, {{var, *rv}});
    if (!a || !b) return false;
    if (!std::isfinite(a->real()) || !std::isfinite(b->real())) return false;
    // real-domain check for real solutions
    if (std::fabs(rv->imag()) < 1e-12) {
        auto ar = evalReal(lhs, {{var, rv->real()}}), br = evalReal(rhs, {{var, rv->real()}});
        if (!ar || !br || !std::isfinite(*ar) || !std::isfinite(*br)) return false;
        return std::fabs(*ar - *br) < 1e-7 * std::max(1.0, std::fabs(*br));
    }
    return std::abs(*a - *b) < 1e-7 * std::max(1.0, std::abs(*b));
}

void finishPlotEq(Solution& sol, const E& L, const E& R, const std::string& var, const std::vector<E>& roots) {
    PlotSpec ps;
    std::vector<std::string> vars;
    collectVars(L, vars);
    collectVars(R, vars);
    if (vars.size() > 1) return;
    E f = simplify(sub(L, R));
    if (!isNumVal(R, 0) && !isNumVal(L, 0) && hasFreeVars(L) && hasFreeVars(R)) {
        ps.curves.push_back({PlotCurve::Type::Explicit, plainInX(L, var), "y = " + toLatex(L)});
        ps.curves.push_back({PlotCurve::Type::Explicit, plainInX(R, var), "y = " + toLatex(R)});
    } else if (!hasFreeVars(R) && hasFreeVars(L)) {
        ps.curves.push_back({PlotCurve::Type::Explicit, plainInX(L, var), "y = " + toLatex(L)});
        ps.curves.push_back({PlotCurve::Type::Explicit, plainInX(R, var), "y = " + toLatex(R)});
    } else {
        ps.curves.push_back({PlotCurve::Type::Explicit, plainInX(f, var), "y = " + toLatex(f)});
    }
    std::vector<double> xs;
    for (auto& r : roots) {
        if (containsSym(r, "n")) continue;
        auto v = evalReal(r);
        if (!v) continue;
        auto y = evalReal(isNumVal(R, 0) ? L : R, {{var, *v}});
        ps.points.push_back({*v, y ? *y : 0.0, var + " = " + toLatex(r)});
        xs.push_back(*v);
    }
    if (!xs.empty()) {
        // frame the interesting region: the roots plus some margin (and the vertex for quadratics)
        double lo = *std::ranges::min_element(xs), hi = *std::ranges::max_element(xs);
        double margin = std::max(1.2, (hi - lo) * 0.4);
        ps.xmin = lo - margin;
        ps.xmax = hi + margin;
    }
    fitY(ps, isNumVal(R, 0) ? f : L, var);
    for (auto& pt : ps.points) {
        ps.ymin = std::min(ps.ymin, pt.y - 1);
        ps.ymax = std::max(ps.ymax, pt.y + 1);
    }
    sol.plot = ps;
}

void setAnswers(Solution& sol, const std::string& var, const std::vector<E>& real, const std::vector<E>& complex, bool numeric, bool complexMode) {
    std::vector<E> main = real;
    if (complexMode || real.empty()) main.insert(main.end(), complex.begin(), complex.end());
    std::string rel = numeric ? " \\approx " : " = ";
    if (main.empty()) {
        sol.answerLatex = complex.empty() ? "\\text{No solution}" : "\\text{No real solution}";
        sol.answers.push_back(sol.answerLatex);
    } else if (main.size() == 1) {
        sol.answerLatex = var + rel + toLatex(main[0]);
        sol.answers.push_back(sol.answerLatex);
    } else {
        std::string s;
        for (size_t i = 0; i < main.size(); ++i) {
            std::string a = var + ordinalSub(i) + rel + toLatex(main[i]);
            sol.answers.push_back(a);
            s += (i ? ", \\quad " : "") + a;
        }
        sol.answerLatex = s;
    }
    bool complexShown = complexMode || real.empty();
    if (!complex.empty() && !complexShown) {
        std::string s;
        for (size_t i = 0; i < complex.size(); ++i) s += (i ? ", \\ " : "") + toLatex(complex[i]);
        sol.facts.push_back({"Complex solutions", s});
    }
    if (!complex.empty() && complexShown && real.empty() && !complexMode) sol.facts.push_back({"Note", "\\text{No real solutions; the solutions are complex}"});
    for (auto& r : real) {
        if (!isNum(r) && !containsSym(r, "n")) {
            if (auto v = evalReal(r)) sol.facts.push_back({"Decimal", var + " \\approx " + numLatex(*v, 8)});
        }
    }
}

// The equation being solved, so every answer can be verified by substitution before it is shown.
struct EquationContext {
    E lhs, rhs;
    bool active = false;
};
thread_local EquationContext gEq;

// Substitute a candidate back into the original equation. Periodic families (with n) are checked at n = 0, 1.
bool verifies(const E& L, const E& R, const std::string& var, const E& r, bool numeric) {
    std::vector<E> probes;
    if (containsSym(r, "n")) probes = {simplify(substitute(r, "n", num(0))), simplify(substitute(r, "n", num(1)))};
    else probes = {r};
    for (auto& p : probes) {
        auto zv = evalComplex(p);
        if (!zv) return true;  // symbolic answer we cannot evaluate: keep
        auto a = evalComplex(L, {{var, *zv}}), b = evalComplex(R, {{var, *zv}});
        if (!a || !b || !std::isfinite(a->real()) || !std::isfinite(b->real()) || !std::isfinite(a->imag()) || !std::isfinite(b->imag())) return false;
        double scale = std::max({1.0, std::abs(*a), std::abs(*b)});
        if (std::abs(*a - *b) > (numeric ? 1e-4 : 1e-7) * scale) return false;
        // real roots must also be in the real domain (e.g. sqrt, ln)
        if (std::fabs(zv->imag()) < 1e-12) {
            auto ar = evalReal(L, {{var, zv->real()}}), br = evalReal(R, {{var, zv->real()}});
            if (!ar || !br || !std::isfinite(*ar) || !std::isfinite(*br)) return false;
        }
    }
    return true;
}

void addCheckStep(Solution& sol, const E& L, const E& R, const std::string& var, const std::vector<E>& roots) {
    if (roots.empty() || roots.size() > 4 || containsSym(roots[0], "n")) return;
    Step check{"Check", "Substitute each answer into the original equation: both sides must agree.", "", {}};
    for (auto& r : roots) {
        E l = simplify(substitute(L, var, r)), rr = simplify(substitute(R, var, r));
        std::string lt = toLatex(l), rt = toLatex(rr);
        if (lt.size() > 60 || rt.size() > 60) {
            auto a = evalComplex(L, {{var, *evalComplex(r)}});
            lt = a ? numLatex(a->real(), 6) : lt;
            rt = lt;
        }
        check.sub.push_back({"", "", var + " = " + toLatex(r) + ": \\quad " + lt + " = " + rt + " \\ \\checkmark", {}});
    }
    for (auto& m_ : sol.methods) m_.steps.push_back(check);
}

void setAnswersOut(std::vector<E>* out, Solution& sol, const std::string& var, const std::vector<E>& realIn, const std::vector<E>& complexIn, bool numeric,
                   bool complexMode) {
    std::vector<E> real, complex, dropped;
    if (gEq.active) {
        for (auto& r : realIn) (verifies(gEq.lhs, gEq.rhs, var, r, numeric) ? real : dropped).push_back(r);
        for (auto& r : complexIn) (verifies(gEq.lhs, gEq.rhs, var, r, numeric) ? complex : dropped).push_back(r);
        if (!dropped.empty()) {
            std::string d;
            for (auto& r : dropped) d += (d.empty() ? "" : ", ") + var + " = " + toLatex(r);
            for (auto& m_ : sol.methods)
                m_.steps.push_back({"Discard what fails the check", m(d) + " does not satisfy the original equation, so it is not a solution.", "", {}});
        }
        std::vector<E> shown = real;
        if (complexMode || real.empty()) shown.insert(shown.end(), complex.begin(), complex.end());
        if (!numeric) addCheckStep(sol, gEq.lhs, gEq.rhs, var, shown);
    } else { real = realIn; complex = complexIn; }
    setAnswers(sol, var, real, complex, numeric, complexMode);
    if (!out) return;
    *out = real;
    if (complexMode || real.empty()) out->insert(out->end(), complex.begin(), complex.end());
}

std::string kindForDegree(int d) {
    if (d == 1) return "linear";
    if (d == 2) return "quadratic";
    return "polynomial";
}

}  // namespace

// ================================================================== equation solver

Solution solveEquation(const E& lhs0, const E& rhs0, const std::string& var, const std::string& inputLatex, bool complexMode,
                       std::vector<E>* rootsOut) {
    // remember the original equation (restored on exit, since solving can recurse)
    EquationContext saved = gEq;
    gEq = {simplify(lhs0), simplify(rhs0), true};
    struct Restore { EquationContext s; ~Restore() { gEq = s; } } restore{saved};
    Solution sol;
    sol.inputLatex = inputLatex;
    sol.title = "Solve for " + m(var);
    E L = simplify(lhs0), R = simplify(rhs0);
    E x = X(var);
    if (!containsSym(L, var) && !containsSym(R, var)) {
        E d = simplify(sub(L, R));
        sol.kind = "identity";
        sol.anim = "linear";
        bool t = isNumVal(d, 0);
        sol.answerLatex = t ? "\\text{True}" : "\\text{False}";
        sol.answers.push_back(sol.answerLatex);
        sol.methods.push_back({"Evaluate", {{"Simplify both sides", "", eqTex(L, R, t ? "=" : "\\ne"), {}}}});
        return sol;
    }
    E f = simplify(sub(L, R));
    std::vector<Step> prelude;

    // ---------- polynomial / rational
    std::vector<E> dens;
    collectDenominators(lhs0, var, dens);
    collectDenominators(rhs0, var, dens);
    if (auto rf = toRationalFunction(f, var)) {
        Poly P = rf->first, Q = rf->second;
        // restrictions from the original denominators (they may cancel during simplification)
        for (auto& d : dens) {
            auto dp = Poly::from(d, var);
            if (!dp || dp->deg() < 1) continue;
            Poly g = Poly::gcd(Q, *dp);
            Poly extra, rem;
            Poly::divmod(dp->monic(), g.deg() >= 0 && !g.isZero() ? g : Poly::constant(Rational(1)), extra, rem);
            if (rem.isZero() && extra.deg() >= 1) { Q = Q * extra; P = P * extra; }
        }
        bool hasDen = Q.deg() >= 1;
        if (!hasDen) {
            int d = P.deg();
            sol.kind = kindForDegree(std::max(d, 1));
            sol.anim = sol.kind == "linear" ? "linear" : (d == 2 ? "quadratic" : "polynomial");
            auto lp = Poly::from(L, var), rp = Poly::from(R, var);
            if (d <= 1 && lp && rp) {
                E root;
                bool special = false;
                std::string st;
                Method mb = linearBalance(lhs0, rhs0, var, root, special, st);
                sol.methods.push_back(mb);
                if (special) {
                    sol.answerLatex = st == "all" ? "\\text{Infinitely many solutions}" : "\\text{No solution}";
                    sol.answers.push_back(sol.answerLatex);
                    return sol;
                }
                setAnswersOut(rootsOut, sol, var, {root}, {}, false, complexMode);
                finishPlotEq(sol, L, R, var, {root});
                return sol;
            }
            // move everything to one side
            if (!isNumVal(R, 0) || !eq(expand(L), L))
                prelude.push_back({"Move all terms to one side", "Write the equation in standard form.", polyEq(P, var), {}});
            PolySolved ps = solvePoly(P, var, prelude, complexMode);
            if (ps.allReals || ps.none) {
                sol.answerLatex = ps.allReals ? "\\text{Infinitely many solutions}" : "\\text{No solution}";
                sol.answers.push_back(sol.answerLatex);
                sol.methods.push_back({"Simplify", prelude});
                return sol;
            }
            sol.methods = ps.methods;
            setAnswersOut(rootsOut, sol, var, ps.real, ps.complex, ps.numeric, complexMode);
            finishPlotEq(sol, L, R, var, ps.real);
            if (complexMode || ps.real.empty()) sol.anim = "complex";
            // complex solutions: hand every root to the app for an Argand-plane explanation
            if (!ps.complex.empty() && sol.plot) {
                PlotShape roots{"roots", {}};
                for (auto* list : {&ps.real, &ps.complex})
                    for (auto& r : *list)
                        if (auto z = evalComplex(r)) { roots.params.push_back(z->real()); roots.params.push_back(z->imag()); }
                sol.plot->shape = roots;
            }
            return sol;
        }
        // rational equation
        sol.kind = "rational";
        sol.anim = "linear";
        E Qe = Q.toExpr(var);
        prelude.push_back({"Find the restrictions", "The denominator can't be zero: " + m(toLatex(Qe) + " \\ne 0") + ".", "", {}});
        prelude.push_back({"Multiply both sides by the common denominator", "Multiply by " + m(toLatex(factorizationToExpr(factorQ(Q), var))) + " to clear the fractions.",
                           polyEq(P, var), {}});
        PolySolved ps = solvePoly(P, var, prelude, complexMode);
        std::vector<E> good, bad;
        for (auto& r : ps.real) {
            auto rr = asCRat(r);
            bool excluded = rr ? Q.eval(rr->re).isZero() : false;
            if (!rr) {
                auto v = evalReal(r);
                excluded = v && std::fabs(Q.evalC(*v).real()) < 1e-9;
            }
            (excluded ? bad : good).push_back(r);
        }
        for (auto& mt : ps.methods) {
            if (!bad.empty()) {
                std::string b;
                for (auto& r : bad) b += (b.empty() ? "" : ", ") + var + " = " + toLatex(r);
                mt.steps.push_back({"Discard excluded values", m(b) + " makes a denominator zero, so it is not a solution.", "", {}});
            } else {
                mt.steps.push_back({"Check the restrictions", "None of the solutions makes a denominator zero.", "", {}});
            }
        }
        sol.methods = ps.methods;
        if (ps.allReals) {
            sol.answerLatex = "\\text{All } " + var + " \\text{ except where } " + toLatex(Qe) + " = 0";
            sol.answers.push_back(sol.answerLatex);
            return sol;
        }
        setAnswersOut(rootsOut, sol, var, good, ps.complex, ps.numeric, complexMode);
        finishPlotEq(sol, L, R, var, good);
        return sol;
    }

    // ---------- inverse operations
    {
        Isolator iso(var, complexMode);
        auto r = iso.run(L, R);
        if (r) {
            sol.kind = iso.usedPeriodic ? "trigonometric" : "equation";
            sol.anim = iso.usedPeriodic ? "trig" : "linear";
            std::vector<E> sols;
            for (auto& s : *r) {
                E ss = simplify(s);
                if (containsSym(ss, "n") || checkSolution(L, R, var, ss)) sols.push_back(ss);
            }
            if (!containsSym(sols.empty() ? num(0) : sols[0], "n")) sortRoots(sols);
            Method mi{"Inverse operations", iso.steps};
            if (iso.squared && sols.size() != r->size())
                mi.steps.push_back({"Check for extraneous solutions", "Squaring can introduce solutions that don't satisfy the original equation; they were removed.", "", {}});
            sol.methods.push_back(mi);
            setAnswersOut(rootsOut, sol, var, sols, {}, false, complexMode);
            if (iso.usedPeriodic) {
                sol.answerLatex += ", \\quad n \\in \\mathbb{Z}";
                std::vector<E> first;
                for (auto& s : sols) for (int k = -1; k <= 1; ++k) first.push_back(simplify(substitute(s, "n", num(k))));
                finishPlotEq(sol, L, R, var, first);
                sol.plot->xmin = -2 * M_PI;
                sol.plot->xmax = 2 * M_PI;
            } else finishPlotEq(sol, L, R, var, sols);
            return sol;
        }
    }

    // ---------- single square root: isolate and square
    {
        std::vector<E> rads;
        std::function<void(const E&)> find = [&](const E& e) {
            if (e->kind == Kind::Pow && isNum(e->args[1]) && e->args[1]->num == Rational(1, 2) && containsSym(e->args[0], var)) {
                for (auto& r : rads) if (eq(r, e)) return;
                rads.push_back(e);
                return;
            }
            for (auto& a : e->args) find(a);
        };
        find(f);
        if (rads.size() == 1) {
            E S = sym("s_");
            E g = expand(replaceSub(f, rads[0], S));
            if (polyDegree(g, "s_") == 1) {
                // g = c1 * s + c0
                E c1 = simplify(derivativeOf(g, "s_"));
                E c0 = simplify(substitute(g, "s_", num(0)));
                E iso = simplify(divide(neg(c0), c1));
                E u = rads[0]->args[0];
                Method mr{"Isolate and square", {}};
                mr.steps.push_back({"Isolate the square root", "", eqTex(rads[0], iso), {}});
                E sq = expand(pow(iso, num(2)));
                mr.steps.push_back({"Square both sides", m("\\left(\\sqrt{u}\\right)^{2} = u"), eqTex(u, sq), {}});
                auto rf2 = toRationalFunction(simplify(sub(u, sq)), var);
                if (rf2) {
                    PolySolved ps = solvePoly(rf2->first, var, {}, false);
                    std::vector<E> good;
                    std::string rejected;
                    for (auto& r : ps.real) {
                        if (checkSolution(L, R, var, r)) good.push_back(r);
                        else rejected += (rejected.empty() ? "" : ", ") + var + " = " + toLatex(r);
                    }
                    if (!ps.methods.empty()) {
                        Step s{"Solve the resulting equation", "", polyEq(rf2->first, var), {}};
                        for (auto& st : ps.methods[0].steps) s.sub.push_back(st);
                        mr.steps.push_back(s);
                    }
                    mr.steps.push_back({"Check each solution in the original equation",
                                        rejected.empty() ? "All solutions work." : m(rejected) + " is extraneous (it doesn't satisfy the original equation).", "", {}});
                    sol.kind = "radical";
                    sol.anim = "linear";
                    sol.methods.push_back(mr);
                    setAnswersOut(rootsOut, sol, var, good, {}, false, false);
                    finishPlotEq(sol, L, R, var, good);
                    return sol;
                }
            }
        }
    }

    // ---------- substitution t = b^x or t = f(x)
    {
        std::vector<E> cands;
        std::function<void(const E&)> find = [&](const E& e) {
            if (e->kind == Kind::Pow && !containsSym(e->args[0], var) && containsSym(e->args[1], var)) {
                // base^(k x): candidate base^x
                E c = pow(e->args[0], x);
                bool have = false;
                for (auto& o : cands) if (eq(o, c)) have = true;
                if (!have) cands.push_back(c);
            }
            if (e->kind == Kind::Func && e->args.size() == 1 && isSym(e->args[0], var)) {
                bool have = false;
                for (auto& o : cands) if (eq(o, e)) have = true;
                if (!have) cands.push_back(e);
            }
            for (auto& a : e->args) find(a);
        };
        find(f);
        for (auto& c : cands) {
            E T = sym("t_");
            std::function<E(const E&)> rw = [&](const E& e) -> E {
                if (eq(e, c)) return T;
                if (c->kind == Kind::Pow && e->kind == Kind::Pow && eq(e->args[0], c->args[0])) {
                    E k = simplify(divide(e->args[1], x));
                    if (isInteger(k)) return pow(T, k);
                }
                if (e->args.empty()) return e;
                auto n = std::make_shared<Node>(*e);
                for (auto& a : n->args) a = rw(a);
                return n;
            };
            E g = simplify(rw(f));
            if (containsSym(g, var)) continue;
            auto tp = toRationalFunction(g, "t_");
            if (!tp || tp->first.deg() < 1) continue;
            Method ms{"Substitution", {}};
            ms.steps.push_back({"Substitute " + m("t = " + toLatex(c)), "", polyEq(tp->first, "t"), {}});
            PolySolved ps = solvePoly(tp->first, "t", {}, false);
            std::string ts;
            for (size_t i = 0; i < ps.real.size(); ++i) ts += (i ? ", \\quad " : "") + std::string("t = ") + toLatex(ps.real[i]);
            ms.steps.push_back({"Solve for t", "", ts.empty() ? "\\text{no real } t" : ts, {}});
            std::vector<E> sols;
            bool periodic = false;
            for (auto& t : ps.real) {
                Isolator iso(var, false);
                auto r = iso.run(c, t);
                if (!r) continue;
                Step s{"Solve " + m(eqTex(c, t)), "", "", iso.steps};
                if (iso.steps.empty() && r->empty()) s.text = "No solution for this value.";
                if (r->empty() && !iso.steps.empty()) s.text = iso.steps.back().text;
                ms.steps.push_back(s);
                periodic = periodic || iso.usedPeriodic;
                for (auto& v : *r) sols.push_back(simplify(v));
            }
            sol.kind = "equation";
            sol.anim = periodic ? "trig" : "linear";
            if (!periodic) sortRoots(sols);
            sol.methods.push_back(ms);
            setAnswersOut(rootsOut, sol, var, sols, {}, false, false);
            if (periodic) sol.answerLatex += ", \\quad n \\in \\mathbb{Z}";
            finishPlotEq(sol, L, R, var, periodic ? std::vector<E>{} : sols);
            return sol;
        }
    }

    // ---------- numeric fallback
    auto roots = numericRealRoots(f, var);
    sol.kind = "numeric";
    sol.anim = "linear";
    std::vector<E> rs;
    for (double r : roots) rs.push_back(approxNum(r));
    Method mn{"Numerical solution", {{"No algebraic method applies", "Find where " + m(toLatex(f)) + " crosses zero using bisection on the graph.", "", {}}}};
    if (!roots.empty()) {
        std::string s;
        for (size_t i = 0; i < rs.size(); ++i) s += (i ? ", \\ " : "") + var + " \\approx " + toLatex(rs[i]);
        mn.steps.push_back({"Refine each crossing", "", s, {}});
    }
    if (rs.empty()) {
        // Not a proof that no solution exists: say exactly what was checked.
        mn.steps.push_back({"No crossing found", "A numerical search didn't find any point where the two sides are equal.", "", {}});
        sol.methods.push_back(mn);
        sol.answerLatex = "\\text{No solution found}";
        sol.answers.push_back(sol.answerLatex);
        finishPlotEq(sol, L, R, var, rs);
        return sol;
    }
    sol.methods.push_back(mn);
    setAnswersOut(rootsOut, sol, var, rs, {}, true, false);
    finishPlotEq(sol, L, R, var, rs);
    return sol;
}

// ================================================================== inequalities

namespace {

bool relHolds(double v, const std::string& op) {
    if (op == "<") return v < 0;
    if (op == "<=") return v <= 1e-12;
    if (op == ">") return v > 0;
    if (op == ">=") return v >= -1e-12;
    if (op == "!=") return std::fabs(v) > 1e-12;
    return false;
}

std::string relTex(const std::string& op) {
    if (op == "<=") return "\\le";
    if (op == ">=") return "\\ge";
    if (op == "!=") return "\\ne";
    return op;
}

std::string flip(const std::string& op) {
    if (op == "<") return ">";
    if (op == ">") return "<";
    if (op == "<=") return ">=";
    if (op == ">=") return "<=";
    return op;
}

}  // namespace

Solution solveInequality(const E& lhs0, const E& rhs0, const std::string& op, const std::string& var, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "inequality";
    sol.anim = "linear";
    sol.title = "Solve the inequality";
    sol.inputLatex = inputLatex;
    E L = simplify(lhs0), R = simplify(rhs0);
    E f = simplify(sub(L, R));
    E x = X(var);
    auto rf = toRationalFunction(f, var);
    // linear: balance with sign flip
    if (rf && rf->second.deg() == 0 && rf->first.deg() == 1) {
        Poly P = rf->first.scale(rf->second.lead().inv());
        auto lp = Poly::from(L, var), rp = Poly::from(R, var);
        Method mb{"Balance method", {}};
        Rational a1 = lp->coeff(1), b1 = lp->coeff(0), a2 = rp->coeff(1), b2 = rp->coeff(0);
        std::string o = op;
        if (!a2.isZero()) {
            mb.steps.push_back({moveWord(simplify(mul(num(a2), x))), "", toLatex(Poly({b1, a1 - a2}).toExpr(var)) + " " + relTex(o) + " " + toLatex(num(b2)), {}});
            a1 = a1 - a2;
        }
        if (!b1.isZero()) mb.steps.push_back({moveWord(num(b1)), "", toLatex(simplify(mul(num(a1), x))) + " " + relTex(o) + " " + toLatex(num(b2 - b1)), {}});
        Rational r = (b2 - b1) / a1;
        if (a1.isNeg()) {
            o = flip(o);
            mb.steps.push_back({"Divide both sides by " + m(toLatex(num(a1))), "Dividing by a negative number flips the inequality sign.", var + " " + relTex(o) + " " + toLatex(num(r)), {}});
        } else if (!a1.isOne()) {
            mb.steps.push_back({"Divide both sides by " + m(toLatex(num(a1))), "", var + " " + relTex(o) + " " + toLatex(num(r)), {}});
        }
        std::string interval;
        std::string rt = toLatex(num(r));
        if (o == "<") interval = "\\left(-\\infty, " + rt + "\\right)";
        else if (o == "<=") interval = "\\left(-\\infty, " + rt + "\\right]";
        else if (o == ">") interval = "\\left(" + rt + ", \\infty\\right)";
        else if (o == ">=") interval = "\\left[" + rt + ", \\infty\\right)";
        else interval = "\\mathbb{R} \\setminus \\left\\{" + rt + "\\right\\}";
        sol.answerLatex = var + " " + relTex(o) + " " + rt;
        sol.answers.push_back(sol.answerLatex);
        sol.facts.push_back({"Interval notation", interval});
        sol.methods.push_back(mb);
        PlotSpec ps = plotFunctions({{L, "y"}, {R, "y"}}, var);
        includeX(ps, {r.toDouble()});
        sol.plot = ps;
        return sol;
    }
    // critical points: roots of numerator and denominator
    std::vector<E> crit;
    std::vector<bool> isPole;
    if (rf) {
        auto addRoots = [&](const Poly& p, bool pole) {
            if (p.deg() < 1) return;
            PolySolved ps = solvePoly(p, var, {}, false);
            for (auto& r : ps.real) { crit.push_back(r); isPole.push_back(pole); }
        };
        addRoots(rf->first, false);
        addRoots(rf->second, true);
    } else {
        for (double r : numericRealRoots(f, var)) { crit.push_back(approxNum(r)); isPole.push_back(false); }
    }
    // sort together
    std::vector<size_t> idx(crit.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::ranges::sort(idx, [&](size_t a, size_t b) { return numVal(crit[a]) < numVal(crit[b]); });
    std::vector<E> cs;
    std::vector<bool> poles;
    for (size_t i : idx) {
        if (!cs.empty() && std::fabs(numVal(cs.back()) - numVal(crit[i])) < 1e-12) { if (isPole[i]) poles.back() = true; continue; }
        cs.push_back(crit[i]);
        poles.push_back(isPole[i]);
    }
    Method mc{"Sign chart", {}};
    mc.steps.push_back({"Move everything to one side", "", toLatex(f) + " " + relTex(op) + " 0", {}});
    if (!cs.empty()) {
        std::string cps;
        for (auto& c : cs) cps += (cps.empty() ? "" : ", \\ ") + var + " = " + toLatex(c);
        mc.steps.push_back({"Find the critical points", "These are where the expression is zero or undefined.", cps, {}});
    }
    // test intervals
    std::vector<std::pair<std::string, bool>> pieces;  // interval latex, holds
    std::string answer;
    Step test{"Test a point in each interval", "", "", {}};
    auto F = [&](double v) { auto r = evalReal(f, {{var, v}}); return r ? *r : NAN; };
    size_t n = cs.size();
    std::vector<std::string> parts;
    for (size_t i = 0; i <= n; ++i) {
        double lo = i == 0 ? -INFINITY : numVal(cs[i - 1]);
        double hi = i == n ? INFINITY : numVal(cs[i]);
        double t = std::isinf(lo) && std::isinf(hi) ? 0 : std::isinf(lo) ? hi - 1 : std::isinf(hi) ? lo + 1 : (lo + hi) / 2;
        double v = F(t);
        bool holds = std::isfinite(v) && relHolds(v, op);
        test.sub.push_back({"", "At " + m(var + " = " + fmtDouble(t, 4)) + " the expression is " + m(v > 0 ? "positive" : "negative") + (holds ? " ✓" : " ✗"), "", {}});
        if (holds) {
            bool incLo = i > 0 && (op == "<=" || op == ">=") && !poles[i - 1];
            bool incHi = i < n && (op == "<=" || op == ">=") && !poles[i];
            std::string s = std::string(incLo ? "\\left[" : "\\left(") + (i == 0 ? "-\\infty" : toLatex(cs[i - 1])) + ", " + (i == n ? "\\infty" : toLatex(cs[i])) + (incHi ? "\\right]" : "\\right)");
            parts.push_back(s);
        }
    }
    // isolated points for <= / >= where the interval on both sides fails but f = 0
    if (op == "<=" || op == ">=") {
        for (size_t i = 0; i < n; ++i) {
            if (poles[i]) continue;
            bool covered = false;
            for (auto& p : parts) if (p.find(toLatex(cs[i])) != std::string::npos) covered = true;
            if (!covered) parts.push_back("\\left\\{" + toLatex(cs[i]) + "\\right\\}");
        }
    }
    mc.steps.push_back(test);
    if (parts.empty()) answer = "\\text{No solution}";
    else if (parts.size() == 1 && parts[0] == "\\left(-\\infty, \\infty\\right)") answer = "\\text{All real numbers}";
    else for (size_t i = 0; i < parts.size(); ++i) answer += (i ? " \\cup " : "") + parts[i];
    mc.steps.push_back({"Combine the intervals", "", answer, {}});
    sol.methods.push_back(mc);
    sol.answerLatex = answer.rfind("\\text", 0) == 0 ? answer : var + " \\in " + answer;
    sol.answers.push_back(sol.answerLatex);
    PlotSpec ps = plotFunctions({{f, "y"}}, var);
    std::vector<double> xs;
    for (auto& c : cs) xs.push_back(numVal(c));
    includeX(ps, xs);
    fitY(ps, f, var);
    sol.plot = ps;
    return sol;
}

// ================================================================== expressions

Solution simplifyExpression(const E& e0, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "simplify";
    sol.anim = "polynomial";
    sol.title = "Simplify";
    sol.inputLatex = inputLatex;
    E e = e0;
    E s = simplify(e);
    std::vector<std::string> vars;
    collectVars(s, vars);
    std::string var = varPick(vars);
    Method mm{"Simplify", {}};
    E result = s;
    auto rf = vars.size() == 1 ? toRationalFunction(s, var) : std::nullopt;
    bool isFraction = rf && (rf->second.deg() >= 1 || containsSym(numDen(s).second, var));
    if (isFraction) {
        // rational expression: combine / cancel
        auto [n0, d0] = numDen(s);
        Poly P = rf->first, Q = rf->second;
        auto pn = Poly::from(n0, var), pd = Poly::from(d0, var);
        bool single = s->kind != Kind::Add;
        if (single && pn && pd) {
            Factorization fn = factorQ(*pn), fd = factorQ(*pd);
            mm.steps.push_back({"Factor the numerator and denominator", "", "\\frac{" + toLatex(factorizationToExpr(fn, var)) + "}{" + toLatex(factorizationToExpr(fd, var)) + "}", {}});
            Poly g = Poly::gcd(*pn, *pd);
            if (g.deg() >= 1) mm.steps.push_back({"Cancel the common factor", "Cancel " + m(toLatex(g.toExpr(var))) + " (valid when it isn't zero).", "", {}});
        } else {
            mm.steps.push_back({"Find a common denominator", "Rewrite every fraction over the least common denominator " + m(toLatex(factorizationToExpr(factorQ(Q), var))) + ".", "", {}});
            mm.steps.push_back({"Combine the numerators", "", "\\frac{" + toLatex(P.toExpr(var)) + "}{" + toLatex(Q.toExpr(var)) + "}", {}});
        }
        Factorization fnum = factorQ(P);
        Factorization fden = factorQ(Q);
        E numF = fnum.factors.size() > 1 ? factorizationToExpr(fnum, var) : P.toExpr(var);
        E denF = fden.factors.size() > 1 ? factorizationToExpr(fden, var) : Q.toExpr(var);
        result = Q.deg() == 0 ? P.toExpr(var) : divide(numF, denF);
        std::string rt = Q.deg() == 0 ? toLatex(P.toExpr(var)) : "\\frac{" + toLatex(numF) + "}{" + toLatex(denF) + "}";
        mm.steps.push_back({"Result", "", rt, {}});
        sol.answerLatex = rt;
        sol.answers.push_back(rt);
        if (Q.deg() > 0) sol.facts.push_back({"Expanded", "\\frac{" + toLatex(P.toExpr(var)) + "}{" + toLatex(Q.toExpr(var)) + "}"});
    } else {
        E ex = expand(s);
        bool expandedDiffers = !eq(ex, s);
        if (expandedDiffers) {
            mm.steps.push_back({"Expand", "Use the distributive property " + m("a(b+c) = ab+ac") + " and multiply out powers.", "", {}});
            // show raw distribution result before collecting
            mm.steps.push_back({"Combine like terms", "Add the coefficients of terms with the same variable part.", toLatex(ex), {}});
            result = ex;
        } else if (!eq(s, e)) {
            mm.steps.push_back({"Combine like terms", "", toLatex(s), {}});
        } else {
            mm.steps.push_back({"Already simplified", "The expression can't be simplified further.", toLatex(s), {}});
        }
        sol.answerLatex = toLatex(result);
        sol.answers.push_back(sol.answerLatex);
        if (vars.size() == 1) {
            if (auto p = Poly::from(result, var); p && p->deg() >= 2) {
                Factorization fz = factorQ(*p);
                if (fz.factors.size() > 1 || (fz.factors.size() == 1 && fz.factors[0].mult > 1))
                    sol.facts.push_back({"Factored form", toLatex(factorizationToExpr(fz, var))});
            }
        }
    }
    sol.methods.push_back(mm);
    if (vars.size() == 1) {
        sol.plot = plotFunctions({{s, "y"}}, var);
        if (auto p = Poly::from(s, var); p && p->deg() >= 1) {
            for (auto& r : p->numericRoots())
                if (std::fabs(r.imag()) < 1e-9) sol.plot->points.push_back({r.real(), 0, "root"});
        }
    }
    return sol;
}

Solution factorExpression(const E& e0, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "factor";
    sol.anim = "polynomial";
    sol.title = "Factor";
    sol.inputLatex = inputLatex;
    E e = simplify(e0);
    std::vector<std::string> vars;
    collectVars(e, vars);
    std::string var = varPick(vars);
    auto p = vars.size() <= 1 ? Poly::from(e, var) : std::nullopt;
    if (!p || p->deg() < 1) {
        // multivariate: common factor only
        E ex = expand(e);
        std::vector<E> terms = ex->kind == Kind::Add ? ex->args : std::vector<E>{ex};
        Rational g(0);
        for (auto& t : terms) g = rgcd(g, splitCoeff(t).first);
        Method mm{"Common factor", {}};
        E res = ex;
        if (!g.isZero() && !g.isOne()) {
            res = mul(num(g), simplify(divide(ex, num(g))));
            mm.steps.push_back({"Factor out the greatest common factor " + m(toLatex(num(g))), "", toLatex(res), {}});
        } else mm.steps.push_back({"No common factor", "", toLatex(ex), {}});
        sol.methods.push_back(mm);
        sol.answerLatex = toLatex(res);
        sol.answers.push_back(sol.answerLatex);
        return sol;
    }
    Poly P = *p;
    Method mm{"Factoring", {}};
    E x = X(var);
    auto [cont, prim] = P.primitive();
    if (!cont.isOne()) mm.steps.push_back({"Factor out the greatest common factor", "", toLatex(num(cont)) + "\\left(" + toLatex(prim.toExpr(var)) + "\\right)", {}});
    int low = 0;
    while (low < prim.deg() && prim.coeff(low).isZero()) ++low;
    if (low > 0) {
        std::vector<Rational> c(prim.coeffs().begin() + low, prim.coeffs().end());
        E xl = low == 1 ? x : pow(x, num(low));
        mm.steps.push_back({"Factor out " + m(toLatex(xl)), "Every term contains " + m(toLatex(xl)) + ".", toLatex(xl) + "\\left(" + toLatex(Poly(c).toExpr(var)) + "\\right)", {}});
    }
    Factorization fz = factorQ(P);
    if (prim.deg() == 2 && low == 0) {
        Rational a = prim.coeff(2), b = prim.coeff(1), c = prim.coeff(0);
        if (b.isZero() && a.sign() > 0 && c.sign() < 0) mm.steps.push_back({"Difference of squares", m("a^{2} - b^{2} = (a - b)(a + b)"), "", {}});
        else if (b * b == Rational(4) * a * c) mm.steps.push_back({"Perfect square trinomial", m("a^{2} \\pm 2ab + b^{2} = (a \\pm b)^{2}"), "", {}});
        else if (fz.factors.size() > 1) mm.steps.push_back({"Split the middle term", "Find two numbers whose product is " + m("ac = " + (a * c).toString()) + " and whose sum is " + m("b = " + b.toString()) + ", then group.", "", {}});
    } else if (prim.deg() >= 3 && fz.factors.size() > 1) {
        std::string roots;
        for (auto& r : P.rationalRoots()) roots += (roots.empty() ? "" : ", ") + toLatex(num(r));
        if (!roots.empty()) mm.steps.push_back({"Find rational roots", "By the rational root theorem, test divisors of the constant term: roots " + m(roots) + ".", "", {}});
        mm.steps.push_back({"Divide out each root", "Each root " + m("r") + " gives a factor " + m("(" + var + " - r)") + " (synthetic division).", "", {}});
    }
    E res = factorizationToExpr(fz, var);
    bool irreducible = fz.factors.size() == 1 && fz.factors[0].mult == 1;
    if (irreducible && cont.isOne()) mm.steps.push_back({"The polynomial is irreducible over the rationals", "", toLatex(P.toExpr(var)), {}});
    else mm.steps.push_back({"Factored form", "", toLatex(res), {}});
    sol.methods.push_back(mm);
    sol.answerLatex = toLatex(res);
    sol.answers.push_back(sol.answerLatex);
    sol.plot = plotFunctions({{P.toExpr(var), "y"}}, var);
    for (auto& r : P.rationalRoots()) sol.plot->points.push_back({r.toDouble(), 0, var + " = " + toLatex(num(r))});
    return sol;
}

Solution expandExpression(const E& e0, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "expand";
    sol.anim = "polynomial";
    sol.title = "Expand";
    sol.inputLatex = inputLatex;
    E e = simplify(e0);
    E ex = expand(e);
    Method mm{"Distribute", {}};
    // show one intermediate: product of sums distributed term by term
    if (e->kind == Kind::Pow && e->args[0]->kind == Kind::Add && isInteger(e->args[1])) {
        long long n = e->args[1]->num.num().toInt64();
        if (n == 2 && e->args[0]->args.size() == 2) {
            mm.steps.push_back({"Use the square of a binomial", m("(a+b)^{2} = a^{2} + 2ab + b^{2}"), "", {}});
        } else {
            std::string prod;
            for (long long k = 0; k < n && k < 6; ++k) prod += "\\left(" + toLatex(e->args[0]) + "\\right)";
            mm.steps.push_back({"Write the power as a product", "", prod, {}});
        }
    } else {
        mm.steps.push_back({"Multiply every term by every term", "Use the distributive property " + m("(a+b)(c+d) = ac+ad+bc+bd") + ".", "", {}});
    }
    mm.steps.push_back({"Combine like terms", "", toLatex(ex), {}});
    sol.methods.push_back(mm);
    sol.answerLatex = toLatex(ex);
    sol.answers.push_back(sol.answerLatex);
    std::vector<std::string> vars;
    collectVars(ex, vars);
    if (vars.size() == 1) sol.plot = plotFunctions({{ex, "y"}}, vars[0]);
    return sol;
}

}  // namespace me
