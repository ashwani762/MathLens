// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Generates the Explain captions ("story") for a finished solution from its own data.
#pragma once
#include "solution/steps.h"

namespace me {
void writeStory(Solution& s);
}
