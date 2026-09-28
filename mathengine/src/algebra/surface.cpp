// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Surfaces z = f(x, y): partial derivatives, gradient, critical points and the
// second-derivative (Hessian) test, plus a surface plot for the app's 3D view.
#include <algorithm>
#include <cmath>

#include "calculus/calculus.h"
#include "solvers.h"

namespace me {

Solution solveSurface(const E& f0, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "surface";
    sol.title = "Analyse the surface";
    sol.inputLatex = inputLatex;
    E f = simplify(f0);
    Method meth{"Multivariable calculus", {}};
    E fx = simplify(derivativeOf(f, "x")), fy = simplify(derivativeOf(f, "y"));
    meth.steps.push_back({"A surface over the plane", "Every point " + m("(x, y)") + " gets a height " + m("z") + ".", "z = " + toLatex(f), {}});
    meth.steps.push_back({"Partial derivatives", "Differentiate in one direction while holding the other variable fixed.",
                          "\\frac{\\partial z}{\\partial x} = " + toLatex(fx) + ", \\quad \\frac{\\partial z}{\\partial y} = " + toLatex(fy), {}});
    meth.steps.push_back({"Gradient", "It points uphill, in the direction of steepest ascent.", "\\nabla z = \\left(" + toLatex(fx) + ", \\ " + toLatex(fy) + "\\right)", {}});
    PlotSpec ps;
    ps.curves.push_back({PlotCurve::Type::Surface, toPlain(f), "z = " + toLatex(f)});
    ps.xmin = -3; ps.xmax = 3; ps.ymin = -3; ps.ymax = 3; ps.autoRange = false;
    // critical points: fx = fy = 0
    SystemResult cr = solveSystemCore({{fx, num(0)}, {fy, num(0)}}, {"x", "y"});
    std::string answer = "z = " + toLatex(f);
    if (cr.status == SystemResult::Status::Unique && !cr.solutions.empty()) {
        E fxx = simplify(derivativeOf(fx, "x")), fyy = simplify(derivativeOf(fy, "y")), fxy = simplify(derivativeOf(fx, "y"));
        Step crit{"Critical points", "Where both partial derivatives are zero the tangent plane is flat.", "", {}};
        std::string summary;
        for (auto& p : cr.solutions) {
            auto sub2 = [&](const E& e) { return simplify(substitute(substitute(e, "x", p[0]), "y", p[1])); };
            E a = sub2(fxx), b = sub2(fyy), c = sub2(fxy), zv = sub2(f);
            E D = simplify(sub(mul(a, b), pow(c, num(2))));
            auto dv = evalReal(D), av = evalReal(a);
            std::string kind = !dv ? "unknown" : (*dv > 0 ? ((av && *av > 0) ? "local minimum" : "local maximum") : (*dv < 0 ? "saddle point" : "inconclusive"));
            std::string pt = "\\left(" + toLatex(p[0]) + ", " + toLatex(p[1]) + "\\right)";
            crit.sub.push_back({"At " + m(pt), "Second-derivative test: " + m("D = f_{xx}f_{yy} - f_{xy}^{2}") + ".",
                                "D = " + toLatex(D) + " \\Rightarrow \\text{" + kind + "}, \\ z = " + toLatex(zv), {}});
            summary += (summary.empty() ? "" : ", \\ ") + std::string("\\text{") + kind + " at } " + pt;
            if (auto xv = evalReal(p[0]), yv = evalReal(p[1]); xv && yv) ps.points.push_back({*xv, *yv, kind});
        }
        crit.latex = summary;
        meth.steps.push_back(crit);
        answer = summary;
    } else {
        meth.steps.push_back({"Critical points", "The partial derivatives are never both zero (or the system isn't solvable symbolically).", "", {}});
    }
    sol.methods.push_back(meth);
    sol.answerLatex = answer;
    sol.answers.push_back(answer);
    sol.facts = {{"f_x", toLatex(fx)}, {"f_y", toLatex(fy)}};
    sol.plot = ps;
    return sol;
}

}  // namespace me
