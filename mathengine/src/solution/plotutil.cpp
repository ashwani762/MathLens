// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "solution/plotutil.h"

#include "core/poly.h"

#include <algorithm>
#include <cmath>

namespace me {

std::string plainInX(const E& f, const std::string& var) {
    if (var == "x") return toPlain(f);
    return toPlain(substitute(f, var, sym("x")));
}

void fitY(PlotSpec& p, const E& f, const std::string& var) {
    std::vector<double> ys;
    for (int k = 0; k <= 240; ++k) {
        double x = p.xmin + (p.xmax - p.xmin) * k / 240.0;
        auto v = evalReal(f, {{var, x}});
        if (v && std::isfinite(*v)) ys.push_back(*v);
    }
    if (ys.size() < 4) return;
    std::ranges::sort(ys);
    double lo = ys[ys.size() / 12], hi = ys[ys.size() * 11 / 12];
    lo = std::min(lo, 0.0);
    hi = std::max(hi, 0.0);
    double span = std::max(hi - lo, 2.0);
    p.ymin = lo - 0.25 * span;
    p.ymax = hi + 0.25 * span;
}

void includeX(PlotSpec& p, const std::vector<double>& xs) {
    if (xs.empty()) return;
    double lo = *std::ranges::min_element(xs), hi = *std::ranges::max_element(xs);
    double span = std::max(hi - lo, 4.0);
    double c = (lo + hi) / 2;
    p.xmin = std::min(p.xmin, c - span);
    p.xmax = std::max(p.xmax, c + span);
    if (p.xmax - p.xmin > 200) { p.xmin = c - span; p.xmax = c + span; }
}

// Narrow the x window until the curve has a readable number of turns (e.g. sin(x^2)).
static void adaptXRange(PlotSpec& p, const E& f, const std::string& var) {
    double span = p.xmax - p.xmin, c = (p.xmin + p.xmax) / 2;
    for (int iter = 0; iter < 5 && span > 1.5; ++iter) {
        int extrema = 0;
        double prev = NAN, prevSlope = 0;
        std::vector<double> mags;
        for (int k = 0; k <= 400; ++k) {
            double x = c - span / 2 + span * k / 400.0;
            auto v = evalReal(f, {{var, x}});
            if (!v || !std::isfinite(*v)) { prev = NAN; continue; }
            mags.push_back(std::fabs(*v));
            if (std::isfinite(prev)) {
                double slope = *v - prev;
                if (slope * prevSlope < 0) ++extrema;
                if (slope != 0) prevSlope = slope;
            }
            prev = *v;
        }
        // explosive growth (e^x, x^5 ...): the extremes dwarf the typical values
        bool explosive = false;
        if (mags.size() > 20) {
            std::ranges::sort(mags);
            double typical = mags[mags.size() * 3 / 4] + 1e-9;
            explosive = mags.back() / typical > 12;
        }
        if (extrema <= 8 && !explosive) break;
        span /= 2;
    }
    p.xmin = c - span / 2;
    p.xmax = c + span / 2;
}

// For polynomials, frame the roots and turning points (critical points) with a margin.
static bool polyRange(PlotSpec& p, const E& f, const std::string& var) {
    auto P = Poly::from(f, var);
    if (!P || P->deg() < 2) return false;
    std::vector<double> xs;
    for (auto& r : P->numericRoots()) if (std::fabs(r.imag()) < 1e-9) xs.push_back(r.real());
    for (auto& r : P->derivative().numericRoots()) if (std::fabs(r.imag()) < 1e-9) xs.push_back(r.real());
    if (xs.empty()) return false;
    double lo = *std::ranges::min_element(xs), hi = *std::ranges::max_element(xs);
    double margin = std::max(1.5, (hi - lo) * 0.45);
    p.xmin = lo - margin;
    p.xmax = hi + margin;
    return true;
}

PlotSpec plotFunctions(const std::vector<std::pair<E, std::string>>& fs, const std::string& var) {
    PlotSpec p;
    if (!fs.empty() && !polyRange(p, fs[0].first, var)) adaptXRange(p, fs[0].first, var);
    for (auto& [f, label] : fs) {
        if (!containsSym(f, var) && hasFreeVars(f)) continue;
        std::vector<std::string> vars;
        collectVars(f, vars);
        if (vars.size() > 1 || (vars.size() == 1 && vars[0] != var)) continue;
        p.curves.push_back({PlotCurve::Type::Explicit, plainInX(f, var), label + " = " + toLatex(f)});
    }
    if (!fs.empty()) fitY(p, fs[0].first, var);
    return p;
}

}  // namespace me
