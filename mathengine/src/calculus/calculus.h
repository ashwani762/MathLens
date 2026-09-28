// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Differentiation, integration and limits with step-by-step explanations.
#pragma once
#include <optional>
#include <string>

#include "core/expr.h"
#include "solution/steps.h"

namespace me {

// silent versions (used by the parser for nested commands and by other solvers)
E derivativeOf(const E& f, const std::string& var, int order = 1);
std::optional<E> antiderivativeOf(const E& f, const std::string& var);

// step-by-step solvers
Solution solveDerivative(const E& f, const std::string& var, int order, const std::string& inputLatex);
Solution solveIntegral(const E& f, const std::string& var, const std::optional<std::pair<E, E>>& bounds,
                       const std::string& inputLatex);
Solution solveLimit(const E& f, const std::string& var, const E& point, const std::string& inputLatex);

// helpers shared with other modules
std::string ddx(const std::string& var, const std::string& latex);
double numericIntegral(const E& f, const std::string& var, double a, double b, bool* ok = nullptr);

}  // namespace me
