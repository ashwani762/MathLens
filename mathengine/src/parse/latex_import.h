// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Converts LaTeX (typed, or produced by the on-device recognition model) into engine syntax.
#pragma once
#include <string>
#include <string_view>

namespace me {

bool looksLikeLatex(std::string_view s);
std::string latexToEngine(std::string_view latex);

}  // namespace me
