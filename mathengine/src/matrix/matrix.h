// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Matrix solver entry points.
#pragma once
#include <string>

#include "parse/parser.h"
#include "solution/steps.h"

namespace me {

bool isMatrixProblem(const R& raw);
Solution solveMatrix(const R& raw, const std::string& inputLatex);

}  // namespace me
