// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Solution / step model shared by all solvers, plus JSON serialization.
#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace me {

// One explained step. `text` may contain inline math between $...$.
struct Step {
    std::string title;          // short instruction, e.g. "Subtract 3 from both sides"
    std::string text;           // explanation (optional)
    std::string latex;          // resulting expression / equation
    std::vector<Step> sub;      // expandable detail steps
};

struct Method {
    std::string name;
    std::vector<Step> steps;
};

struct PlotCurve {
    enum class Type { Explicit, Implicit, Polar, Vertical, Surface } type = Type::Explicit;
    std::string expr;    // plain, re-parseable expression; explicit: f(x), implicit: F(x,y)=0, polar: r(theta)
    std::string label;   // latex
};

struct PlotPoint {
    double x, y;
    std::string label;
};

// Geometric description of a recognised curve, for explanatory drawings.
//   circle   [h, k, r]
//   ellipse  [h, k, a, b]            (a = semi-axis along x, b along y)
//   hyperbola[h, k, a, b, axis]      (axis 0: opens along x, 1: along y)
//   parabola [h, k, p, axis]         (vertex (h,k), focal length p; axis 0: opens along y, 1: along x)
//   line     [m, b]                  y = m x + b ;  vline [c]  x = c
struct PlotShape {
    std::string type;
    std::vector<double> params;
};

struct PlotSpec {
    std::vector<PlotCurve> curves;
    std::optional<PlotShape> shape;
    std::vector<PlotPoint> points;
    std::optional<std::pair<double, double>> shade;  // area under curves[0] between a and b
    std::optional<std::string> tangentAt;              // (unused placeholder for future)
    double xmin = -10, xmax = 10, ymin = -7, ymax = 7;
    bool autoRange = true;
    std::vector<std::string> params;  // free parameters in curve exprs (e.g. a, b in y = a sin(bx)); shown as sliders
};

struct Solution {
    bool ok = true;
    std::string error;
    std::string kind;        // arithmetic, linear, quadratic, polynomial, derivative, integral, limit, system, complex, ...
    std::string title;       // "Solve for x", "Differentiate", ...
    std::string inputLatex;
    std::string answerLatex;
    std::vector<std::string> answers;              // individual answers (latex)
    std::vector<std::pair<std::string, std::string>> facts;  // extra info: label -> latex
    std::vector<Method> methods;
    std::optional<PlotSpec> plot;
    std::string anim;        // visualization id
    // Explain-animation captions for successive phases, generated from this solution
    // (plain text with inline $math$). Scenes never invent their own claims when these exist.
    std::vector<std::string> story;
};

std::string toJson(const Solution& s);
std::string toJson(const PlotSpec& p);
std::string jsonEscape(const std::string& s);

// small helpers for building step text
inline std::string m(const std::string& latex) { return "$" + latex + "$"; }

}  // namespace me
