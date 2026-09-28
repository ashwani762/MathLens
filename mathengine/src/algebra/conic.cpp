// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Recognises lines and conic sections in x and y, derives their standard form with steps,
// and reports geometric parameters used by the app's explanatory drawings.
#include <algorithm>
#include <cmath>

#include "calculus/calculus.h"
#include "solvers.h"

namespace me {

namespace {

std::optional<Rational> coeffNum(const E& e) {
    E s = simplify(e);
    if (isNum(s)) return s->num;
    return std::nullopt;
}

double d(const Rational& r) { return r.toDouble(); }

E X() { return sym("x"); }
E Y() { return sym("y"); }

// (v - c)^2 displayed nicely
E shifted(const E& v, const Rational& c) { return c.isZero() ? pow(v, num(2)) : pow(add(v, num(-c)), num(2)); }

std::string pointTex(const Rational& a, const Rational& b) { return "\\left(" + toLatex(num(a)) + ", " + toLatex(num(b)) + "\\right)"; }

std::string exactSqrt(const Rational& r) { return toLatex(simplify(sqrtE(num(r)))); }

std::string m_(const std::string& s) { return "$" + s + "$"; }

}  // namespace

std::optional<ConicAnalysis> analyzeConic(const E& lhs, const E& rhs) {
    E f = expand(sub(lhs, rhs));
    std::vector<std::string> vars;
    collectVars(f, vars);
    for (auto& v : vars) if (v != "x" && v != "y") return std::nullopt;
    if (polyDegree(f, "x") > 2 || polyDegree(f, "y") > 2 || polyDegree(f, "x") < 0 || polyDegree(f, "y") < 0) return std::nullopt;
    auto at0 = [](const E& e) { return simplify(substitute(substitute(e, "x", num(0)), "y", num(0))); };
    E fx = derivativeOf(f, "x"), fy = derivativeOf(f, "y");
    auto A = coeffNum(mul(num(Rational(1, 2)), at0(derivativeOf(fx, "x"))));
    auto B = coeffNum(at0(derivativeOf(fx, "y")));
    auto C = coeffNum(mul(num(Rational(1, 2)), at0(derivativeOf(fy, "y"))));
    auto D = coeffNum(at0(fx));
    auto Ee = coeffNum(at0(fy));
    auto F = coeffNum(at0(f));
    if (!A || !B || !C || !D || !Ee || !F) return std::nullopt;
    // f must be exactly the general quadratic (rules out x^2 y etc.)
    E rebuilt = add({mul(num(*A), pow(X(), num(2))), mul({num(*B), X(), Y()}), mul(num(*C), pow(Y(), num(2))),
                     mul(num(*D), X()), mul(num(*Ee), Y()), num(*F)});
    if (!isNumVal(simplify(expand(sub(rebuilt, f))), 0)) return std::nullopt;
    if (!B->isZero()) return std::nullopt;  // rotated conics are not handled yet

    ConicAnalysis out;
    Method m{"Identify the shape", {}};
    const Rational a = *A, c = *C, dd = *D, e = *Ee, ff = *F;
    // show the general form with a positive leading square term
    E shown = ((!A->isZero() && A->isNeg()) || (A->isZero() && C->isNeg())) ? simplify(neg(f)) : simplify(f);
    std::string general = toLatex(shown) + " = 0";

    if (a.isZero() && c.isZero()) {
        if (dd.isZero() && e.isZero()) return std::nullopt;
        if (!e.isZero()) {
            Rational slope = -dd / e, icpt = -ff / e;
            E yx = simplify(add(mul(num(slope), X()), num(icpt)));
            m.steps.push_back({"Solve for " + m_("y"), "Write the line in slope-intercept form " + m_("y = mx + b") + ".", "y = " + toLatex(yx), {}});
            m.steps.push_back({"Read the slope and intercept", "The slope " + m_("m") + " is the rise for every step of 1 to the right.",
                               "m = " + toLatex(num(slope)) + ", \\quad b = " + toLatex(num(icpt)), {}});
            out.shape = {"line", {d(slope), d(icpt)}};
            out.answer = "\\text{Line: } y = " + toLatex(yx);
            out.facts = {{"Shape", "\\text{Line}"}, {"Slope", toLatex(num(slope))}, {"y-intercept", toLatex(num(icpt))}};
        } else {
            Rational xc = -ff / dd;
            m.steps.push_back({"Solve for " + m_("x"), "There is no " + m_("y") + ", so every point shares the same " + m_("x") + ": a vertical line.", "x = " + toLatex(num(xc)), {}});
            out.shape = {"vline", {d(xc)}};
            out.answer = "\\text{Vertical line: } x = " + toLatex(num(xc));
            out.facts = {{"Shape", "\\text{Vertical line}"}};
        }
        out.method = m;
        return out;
    }

    m.steps.push_back({"Recognise a conic", "The equation has squared terms in " + m_("x") + " and/or " + m_("y") + " and no " + m_("xy") + " term, so its graph is a conic section.", general, {}});

    if (!a.isZero() && !c.isZero()) {
        Rational h = -dd / (Rational(2) * a), k = -e / (Rational(2) * c);
        Rational rhs = a * h * h + c * k * k - ff;  // a(x-h)^2 + c(y-k)^2 = rhs
        if (!dd.isZero() || !e.isZero()) {
            m.steps.push_back({"Complete the square", "Group the " + m_("x") + " terms and the " + m_("y") + " terms and complete each square.",
                               toLatex(simplify(add(mul(num(a), shifted(X(), h)), mul(num(c), shifted(Y(), k))))) + " = " + toLatex(num(rhs)), {}});
        }
        if (a == c) {
            Rational rr = rhs / a;
            if (rr.sign() <= 0) return std::nullopt;
            if (!a.isOne()) m.steps.push_back({"Divide by " + m_(toLatex(num(a))), "", toLatex(add(shifted(X(), h), shifted(Y(), k))) + " = " + toLatex(num(rr)), {}});
            std::string r = exactSqrt(rr);
            m.steps.push_back({"Compare with the circle equation", m_("(x-h)^{2} + (y-k)^{2} = r^{2}") + ": every point is at distance " + m_("r") + " from the centre " + m_("(h, k)") + ".",
                               "\\text{centre } " + pointTex(h, k) + ", \\quad r = " + r, {}});
            out.shape = {"circle", {d(h), d(k), std::sqrt(d(rr))}};
            out.answer = "\\text{Circle: centre } " + pointTex(h, k) + ", \\ r = " + r;
            out.facts = {{"Shape", "\\text{Circle}"}, {"Centre", pointTex(h, k)}, {"Radius", r},
                         {"Area", toLatex(simplify(mul(num(rr), PI())))}, {"Circumference", toLatex(simplify(mul({num(2), sqrtE(num(rr)), PI()})))}};
        } else if ((a * c).sign() > 0) {
            Rational A2 = rhs / a, B2 = rhs / c;
            if (A2.sign() <= 0 || B2.sign() <= 0) return std::nullopt;
            m.steps.push_back({"Divide to get 1 on the right", "", toLatex(add(divide(shifted(X(), h), num(A2)), divide(shifted(Y(), k), num(B2)))) + " = 1", {}});
            std::string sa = exactSqrt(A2), sb = exactSqrt(B2);
            Rational c2 = (A2 - B2).abs();
            m.steps.push_back({"Read the semi-axes", m_("\\frac{(x-h)^{2}}{a^{2}} + \\frac{(y-k)^{2}}{b^{2}} = 1") + " is an ellipse centred at " + m_("(h, k)") + ".",
                               "a = " + sa + ", \\quad b = " + sb, {}});
            m.steps.push_back({"Find the foci", "The foci lie on the longer axis at distance " + m_("c = \\sqrt{|a^{2} - b^{2}|}") + " from the centre.", "c = " + exactSqrt(c2), {}});
            out.shape = {"ellipse", {d(h), d(k), std::sqrt(d(A2)), std::sqrt(d(B2))}};
            out.answer = "\\text{Ellipse: centre } " + pointTex(h, k) + ", \\ a = " + sa + ", \\ b = " + sb;
            out.facts = {{"Shape", "\\text{Ellipse}"}, {"Centre", pointTex(h, k)}, {"Semi-axes", sa + ", \\ " + sb}, {"Focal distance", exactSqrt(c2)}};
        } else {
            if (rhs.isZero()) return std::nullopt;  // two crossing lines
            Rational A2 = rhs / a, B2 = rhs / c;
            bool alongX = A2.sign() > 0;
            Rational pa = alongX ? A2 : B2, pb = alongX ? -B2 : -A2;
            E form = alongX ? sub(divide(shifted(X(), h), num(pa)), divide(shifted(Y(), k), num(pb)))
                            : sub(divide(shifted(Y(), k), num(pa)), divide(shifted(X(), h), num(pb)));
            m.steps.push_back({"Divide to get 1 on the right", "", toLatex(form) + " = 1", {}});
            std::string sa = exactSqrt(pa), sb = exactSqrt(pb);
            m.steps.push_back({"Recognise a hyperbola", "A difference of squares equal to 1 gives two branches opening along the " + m_(alongX ? "x" : "y") + "-axis.",
                               "a = " + sa + ", \\quad b = " + sb, {}});
            std::string slope = toLatex(simplify(divide(sqrtE(num(alongX ? pb : pa)), sqrtE(num(alongX ? pa : pb)))));
            m.steps.push_back({"Find the asymptotes", "Far from the centre the branches approach two lines.", "y - " + toLatex(num(k)) + " = \\pm " + slope + "\\left(x - " + toLatex(num(h)) + "\\right)", {}});
            out.shape = {"hyperbola", {d(h), d(k), std::sqrt(d(pa)), std::sqrt(d(pb)), alongX ? 0.0 : 1.0}};
            out.answer = "\\text{Hyperbola: centre } " + pointTex(h, k) + ", \\ a = " + sa + ", \\ b = " + sb;
            out.facts = {{"Shape", "\\text{Hyperbola}"}, {"Centre", pointTex(h, k)}, {"Focal distance", exactSqrt(pa + pb)}};
        }
        out.method = m;
        return out;
    }

    // parabola: exactly one squared variable
    bool opensY = !a.isZero();  // y = ...x^2
    Rational sq = opensY ? a : c, lin = opensY ? e : dd, other = opensY ? dd : e;
    if (lin.isZero()) return std::nullopt;  // pair of parallel lines
    // lin*v = -(sq*u^2 + other*u + ff)  ->  v = q (u - h)^2 + k
    Rational q = -sq / lin;
    Rational h = -other / (Rational(2) * sq);
    Rational k = -(sq * h * h + other * h + ff) / lin;
    Rational p = Rational(1) / (Rational(4) * q);
    E u = opensY ? X() : Y(), v = opensY ? Y() : X();
    m.steps.push_back({"Solve for " + m_(opensY ? "y" : "x"), "Only one variable is squared, so the graph is a parabola.",
                       toLatex(v) + " = " + toLatex(simplify(add(mul(num(q), pow(u, num(2))), add(mul(num(-other / lin), u), num(-ff / lin))))), {}});
    m.steps.push_back({"Complete the square (vertex form)", "", toLatex(v) + " = " + toLatex(simplify(add(mul(num(q), shifted(u, h)), num(k)))), {}});
    Rational vx = opensY ? h : k, vy = opensY ? k : h;
    Rational focX = opensY ? h : k + p, focY = opensY ? k + p : h;
    m.steps.push_back({"Vertex, focus and directrix", "A parabola is every point equally far from the focus and the directrix. Here " + m_("p = \\frac{1}{4a}") + ".",
                       "\\text{vertex } " + pointTex(vx, vy) + ", \\quad \\text{focus } " + pointTex(focX, focY) + ", \\quad " +
                           (opensY ? "y = " + toLatex(num(k - p)) : "x = " + toLatex(num(k - p))), {}});
    out.shape = {"parabola", {d(vx), d(vy), d(p), opensY ? 0.0 : 1.0}};
    out.answer = "\\text{Parabola: vertex } " + pointTex(vx, vy);
    out.facts = {{"Shape", "\\text{Parabola}"}, {"Vertex", pointTex(vx, vy)}, {"Focus", pointTex(focX, focY)},
                 {"Directrix", (opensY ? "y = " : "x = ") + toLatex(num(k - p))}};
    out.method = m;
    return out;
}

}  // namespace me
