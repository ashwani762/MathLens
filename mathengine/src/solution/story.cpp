// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Explain-animation captions derived from a finished solution. Every caption is built from the
// solution's own data (answers, curves, crossing points, shape parameters, facts), so the
// animation never states anything the solver did not compute.
#include <algorithm>
#include <cmath>
#include <regex>

#include "solution/story.h"

namespace me {

namespace {

std::string mth(const std::string& latex) { return "$" + latex + "$"; }

// "x_{1} = 2" -> "x = 2"
std::string dropIndex(const std::string& a) { return std::regex_replace(a, std::regex("_\\{\\d+\\}"), ""); }

std::string joinAnswers(const Solution& s, size_t maxN = 4) {
    std::vector<std::string> as;
    for (auto& a : s.answers) as.push_back(mth(dropIndex(a)));
    if (as.empty()) return mth(s.answerLatex);
    if (as.size() > maxN) return mth(s.answerLatex);
    std::string out;
    for (size_t i = 0; i < as.size(); ++i) out += (i == 0 ? "" : (i + 1 == as.size() ? " and " : ", ")) + as[i];
    return out;
}

std::string label(const PlotCurve& c) { return c.label; }  // already "y = ..."

// "\text{No solution found}" -> "No solution found"
std::string plain(const std::string& latex) {
    std::string t = std::regex_replace(latex, std::regex("\\\\text\\{([^}]*)\\}"), "$1");
    return t.find('\\') == std::string::npos ? t : mth(latex);
}

std::string fact(const Solution& s, const std::string& key) {
    for (auto& [k, v] : s.facts) if (k == key) return v;
    return "";
}

std::string num(double v) {
    char b[32];
    snprintf(b, sizeof b, "%.4g", v);
    return b;
}

bool isEquationKind(const std::string& k) {
    static const char* ks[] = {"linear", "quadratic", "polynomial", "rational", "radical", "equation", "trigonometric", "numeric", "identity"};
    for (auto x : ks) if (k == x) return true;
    return false;
}

std::vector<std::string> equationStory(const Solution& s) {
    const PlotSpec& p = *s.plot;
    std::vector<std::string> st;
    bool none = s.answers.empty() || s.answerLatex.find("No") != std::string::npos;
    if (p.shape && p.shape->type == "roots") {
        return {"The solutions are complex numbers, so the real graph can't show them all",
                "Place each on the complex plane: real part across, imaginary part up",
                "So " + joinAnswers(s)};
    }
    std::vector<const PlotCurve*> explicitCurves;
    for (auto& c : p.curves) if (c.type == PlotCurve::Type::Explicit) explicitCurves.push_back(&c);
    // "right side y = 0" reads better as a single curve meeting zero
    if (explicitCurves.size() == 2 && (explicitCurves[1]->label == "y = 0" || explicitCurves[1]->expr == "0")) explicitCurves.pop_back();
    std::vector<double> xs;
    for (auto& pt : p.points) if (std::isfinite(pt.x)) xs.push_back(pt.x);
    std::sort(xs.begin(), xs.end(), [](double a, double b) { return std::fabs(a) < std::fabs(b); });
    if (xs.size() > 3) xs.resize(3);
    std::sort(xs.begin(), xs.end());
    std::string crossings;
    for (size_t i = 0; i < xs.size(); ++i) {
        double r = std::round(xs[i]);
        bool exact = std::fabs(xs[i] - r) < 1e-9;
        crossings += (i ? ", " : "") + mth("x " + std::string(exact ? "= " : "\\approx ") + num(xs[i]));
    }
    if (explicitCurves.size() >= 2) {
        st.push_back("Left side " + mth(label(*explicitCurves[0])) + ", right side " + mth(label(*explicitCurves[1])));
        st.push_back(none ? "The two curves never meet" : "The sides are equal where the curves cross" + (crossings.empty() ? "" : ": " + crossings));
    } else if (explicitCurves.size() == 1) {
        st.push_back("Move everything to one side and draw " + mth(label(*explicitCurves[0])));
        st.push_back(none ? "The curve never reaches " + mth("y = 0") : "It meets " + mth("y = 0") + " at " + (crossings.empty() ? "the solutions" : crossings));
    }
    st.push_back(none ? plain(s.answerLatex) : "So " + joinAnswers(s));
    return st;
}

std::vector<std::string> shapeStory(const Solution& s, const PlotShape& sh) {
    const auto& q = sh.params;
    auto pt = [&](double a, double b) { return mth("(" + num(a) + ", " + num(b) + ")"); };
    if (sh.type == "circle")
        return {"Fix the centre " + pt(q[0], q[1]),
                "Every point on the curve is " + mth(num(q[2])) + " away from it (Pythagoras)",
                "Those points form a circle of radius " + mth(num(q[2]))};
    if (sh.type == "ellipse")
        return {"Two foci on the longer axis",
                "For every point, the distances to the foci add up to " + mth(num(2 * std::max(q[2], q[3]))),
                "An ellipse with semi-axes " + mth(num(q[2])) + " and " + mth(num(q[3]))};
    if (sh.type == "hyperbola")
        return {"Two foci and two branches",
                "For every point, the distances to the foci differ by " + mth(num(2 * q[2])),
                "A hyperbola centred at " + pt(q[0], q[1])};
    if (sh.type == "parabola")
        return {"A focus and a straight line, the directrix",
                "Every point is equally far from the focus and the directrix",
                "A parabola with vertex " + pt(q[0], q[1])};
    if (sh.type == "line")
        return {"A straight line keeps the same steepness", "Move 1 to the right and it rises " + mth(num(q[0])), "Slope " + mth(num(q[0])) + ", crossing the y-axis at " + mth(num(q[1]))};
    if (sh.type == "vline") return {"No y at all, so every point has the same x", "A vertical line", mth(s.answerLatex)};
    return {};
}

}  // namespace

void writeStory(Solution& s) {
    if (!s.ok || !s.story.empty()) return;
    const std::string& k = s.kind;
    const PlotSpec* p = s.plot ? &*s.plot : nullptr;
    std::vector<std::string>& st = s.story;

    if (p && p->shape && p->shape->type == "matrix2") {
        const auto& q = p->shape->params;
        auto col = [&](double x, double y) { return mth("(" + num(x) + ", " + num(y) + ")"); };
        st.push_back("The columns " + col(q[0], q[2]) + " and " + col(q[1], q[3]) + " are where the unit square's sides land");
        double det = q[4];
        if (std::fabs(det) < 1e-12) st.push_back("It flattens the square onto a line: " + mth("\\det = 0") + ", so there is no inverse");
        else st.push_back("Its area scales by " + mth("|\\det| = " + num(std::fabs(det))) + (det < 0 ? ", and the square is flipped over" : ""));
        if (q[5] > 0.5) {
            auto how = [&](double l) {
                std::string v = mth(num(l));
                if (std::fabs(l) < 1e-12) return v + " (squashed to a point)";
                if (l < 0) return v + " (flipped)";
                return v;
            };
            if (std::fabs(q[6] - q[9]) < 1e-12) st.push_back("Along the eigenvector line, vectors keep their direction and scale by " + how(q[6]));
            else st.push_back("On the two eigenvector lines, vectors stay on their line and scale by " + how(q[6]) + " and " + how(q[9]));
        } else st.push_back("No real eigenvectors: every direction gets turned");
        return;
    }
    if (p && p->shape && p->shape->type != "roots") { st = shapeStory(s, *p->shape); if (!st.empty()) return; }
    if (isEquationKind(k) && p) { st = equationStory(s); return; }
    if (k == "function" && p && !p->curves.empty()) {
        std::string roots = fact(s, "Roots"), mx = fact(s, "Maximum"), mn = fact(s, "Minimum");
        std::string turns = mx.empty() ? mn : (mn.empty() ? mx : mx + ",\\ " + mn);
        st = {"Draw " + mth(p->curves[0].label),
              roots.empty() ? "It never crosses the x-axis" : "It crosses the x-axis at " + mth(roots),
              turns.empty() ? "No turning points: it keeps rising or falling" : "It turns at " + mth(turns)};
        return;
    }
    if (k == "graph" && p && !p->params.empty()) {
        std::string names;
        for (size_t i = 0; i < p->params.size(); ++i) names += (i ? (i + 1 == p->params.size() ? " and " : ", ") : "") + mth(p->params[i]);
        std::string start = s.methods.empty() || s.methods[0].steps.size() < 2 ? "" : s.methods[0].steps[1].latex;
        st = {"A family of curves: one for every value of " + names,
              start.empty() ? "Here every parameter is 1" : "With every parameter at 1 it is " + mth(start),
              "Open the graph and slide " + names + " to reshape it"};
        return;
    }
    if (k == "graph" && p) {
        bool polar = std::any_of(p->curves.begin(), p->curves.end(), [](auto& c) { return c.type == PlotCurve::Type::Polar; });
        if (polar) st = {"In polar form the distance from the centre depends on the angle", "Sweep the angle and trace the radius", plain(s.answerLatex)};
        else st = {"Every point " + mth("(x, y)") + " that makes the equation true", "Trace them all and a curve appears", mth(s.inputLatex)};
        return;
    }
    if (k == "inequality") {
        std::string ans = s.answerLatex.find("\\text") == 0 ? plain(s.answerLatex) : mth(s.answerLatex);
        if (!ans.empty() && ans[0] >= 'A' && ans[0] <= 'Z') ans[0] = char(ans[0] - 'A' + 'a');
        st = {"Find where the two sides are equal: the critical points", "Test one value between them to see which side is true", "True for " + ans};
        return;
    }
    if ((k == "simplify" || k == "factor" || k == "expand") && p && !p->curves.empty()) {
        std::string roots;
        for (size_t i = 0; i < p->points.size() && i < 3; ++i) roots += (i ? ", " : "") + mth("x = " + num(p->points[i].x));
        std::string verb = k == "factor" ? "Factored" : k == "expand" ? "Expanded" : "Simplified";
        st = {"The expression as a curve " + mth(p->curves[0].label),
              roots.empty() ? "Rewriting it never changes the curve, only its form" : "Its roots, where the curve crosses zero: " + roots,
              verb + ": " + mth(s.answerLatex)};
        return;
    }
    if (k == "derivative" && p && p->curves.size() >= 2) {
        st = {"The curve " + mth(p->curves[0].label), "The tangent's slope at each point is the derivative", mth(s.answerLatex)};
        return;
    }
    if (k == "integral" && p && !p->curves.empty()) {
        if (p->shade) st = {"The area under " + mth(p->curves[0].label) + " between the bounds", "Slices approximate it; thinner slices get closer",
                            "Exact area " + mth("= " + (s.answers.empty() ? s.answerLatex : s.answers[0]))};
        else st = {"Start from " + mth(p->curves[0].label), "The antiderivative measures the area swept so far", mth(s.answerLatex)};
        return;
    }
    if (k == "limit") {
        bool dne = s.answerLatex.find("does not exist") != std::string::npos;
        bool atInf = s.inputLatex.find("\\infty") != std::string::npos;
        std::string target = s.answers.empty() ? "" : s.answers[0];
        if (dne) st = {"Look at the function near the point", "From the left and from the right it heads in different directions", "So the limit does not exist"};
        else if (atInf) st = {"Follow the curve far to the right", "The values settle towards " + mth(target), mth(s.answerLatex)};
        else st = {"Look at the function near the point", "From both sides the values approach " + mth(target), mth(s.answerLatex)};
        return;
    }
    if (k == "system") {
        std::string at;
        if (p && !p->points.empty() && p->points.size() <= 4)
            for (size_t i = 0; i < p->points.size(); ++i) at += std::string(i ? (i + 1 == p->points.size() ? " and " : ", ") : "") + mth("(" + num(p->points[i].x) + ", " + num(p->points[i].y) + ")");
        else
            for (size_t i = 0; i < s.answers.size() && i < 6; ++i) at += (i ? ", " : "") + mth(s.answers[i]);
        st = {"Each equation is a line or curve", "A solution must lie on all of them at once", s.answers.empty() ? plain(s.answerLatex) : "They meet at " + at};
        return;
    }
    if (k == "complex") {
        std::string re = fact(s, "Real part"), im = fact(s, "Imaginary part"), mod = fact(s, "Modulus");
        if (!re.empty()) st = {"Real part " + mth(re) + " across", "Imaginary part " + mth(im) + " up", "Length " + mth(mod)};
        else if (p && p->shape && p->shape->type == "roots") st = {"Complex solutions", "Plotted on the complex plane", "So " + joinAnswers(s)};
        return;
    }
    if (k == "taylor") {
        std::string fn = p && !p->curves.empty() ? p->curves[0].label : s.inputLatex;
        st = {"The function " + mth(fn), "Each extra term matches one more derivative at the centre", mth(s.answerLatex)};
        return;
    }
    if (k == "ode") { st = {"Each constant " + mth("C") + " gives one solution curve", "Together they fill the plane: one curve through each point", mth(s.answerLatex)}; return; }
    if (k == "statistics") { st = {"Each value is a dot on the number line", "The mean is the point where the data balances", mth(s.answerLatex)}; return; }
    if (k == "surface") { st = {"A height above every point " + mth("(x, y)"), "A ball rolling downhill follows the steepest slope", mth(s.answerLatex)}; return; }
    if (k == "factorial") { st = {"Line the objects up", "Count every possible order", mth(s.answerLatex)}; return; }
    if (k == "arithmetic") {
        if (s.answerLatex.rfind("\\frac{", 0) == 0) st = {mth(s.inputLatex), "Split one whole into equal slices", mth("= " + s.answerLatex)};
        return;
    }
    if (k == "finance") {
        const std::string& t = s.title;
        if (t == "Standard normal CDF") st = {"The standard normal bell curve", "Shade the area to the left of the value", mth(s.answerLatex)};
        else if (t == "Future value" || t == "Present value" || t == "Continuous compounding")
            st = {"Start with the principal", "Each period multiplies it by the growth factor", mth(s.answerLatex)};
        else if (t.find("Black") != std::string::npos || t.find("option") != std::string::npos || t.find("binomial") != std::string::npos || t.find("jump") != std::string::npos)
            st = {"At expiry the option pays only if it finishes in the money", "Before expiry it is worth more: time and uncertainty have value", mth(s.answerLatex)};
        else st = {t, mth(s.answerLatex)};
        return;
    }
}

}  // namespace me
