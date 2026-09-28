// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Helpers to build graph specifications for solutions.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/expr.h"
#include "solution/steps.h"

namespace me {

// Plain expression in terms of x (the app's evaluator always uses x / y / theta)
std::string plainInX(const E& f, const std::string& var);

PlotSpec plotFunctions(const std::vector<std::pair<E, std::string>>& fs, const std::string& var);
void fitY(PlotSpec& p, const E& f, const std::string& var);
// widen the x range so that all given x values are visible
void includeX(PlotSpec& p, const std::vector<double>& xs);

}  // namespace me
