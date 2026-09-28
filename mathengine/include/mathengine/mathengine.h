// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

/*
 * MathEngine public API.
 *
 * A portable, dependency-free C++23 math solver. The C interface below is stable and
 * easy to bind from Kotlin/JNI, Swift, Dart FFI, WebAssembly, Python ctypes, etc.
 * All returned strings are UTF-8 JSON and must be released with me_free().
 *
 * Solution JSON:
 * {
 *   "ok": true, "error": "...",            // error only when ok == false
 *   "kind": "quadratic", "title": "Solve for $x$",
 *   "input": "<latex>", "answer": "<latex>", "answers": ["<latex>", ...],
 *   "facts":   [{"label": "Decimal", "latex": "..."}],
 *   "methods": [{"name": "Quadratic formula",
 *                "steps": [{"title": "...", "text": "... $inline$ ...", "latex": "...", "sub": [...]}]}],
 *   "plot": {"curves": [{"type": "explicit|implicit|polar|vertical", "expr": "x^2-1", "label": "<latex>"}],
 *            "points": [{"x": 1, "y": 0, "label": "<latex>"}], "shade": [a, b],
 *            "view": [xmin, xmax, ymin, ymax], "auto": true},
 *   "anim": "quadratic"                      // id of the matching 3D visualization
 * }
 */
#ifndef MATHENGINE_H
#define MATHENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct me_program me_program;

/* Solve anything: arithmetic, equations, systems, calculus, complex numbers... */
char* me_solve(const char* input);

/* Live preview while typing: {"ok":true,"latex":"..."} or {"ok":false,"error":"..."} */
char* me_latex(const char* input);

/* LaTeX -> engine syntax (for recognised or pasted LaTeX). */
char* me_from_latex(const char* latex);

/* Graph specification for an input (explicit, implicit, polar curves): the "plot" object above. */
char* me_graph(const char* input);

/* Fast evaluation for plotting. Variables: x, y, theta. Returns NULL on error. */
me_program* me_compile(const char* expr);
double me_eval(const me_program* p, double x, double y, double theta);
/* Evaluates n points; ys may be NULL. Writes NaN where undefined. */
void me_eval_many(const me_program* p, const double* xs, const double* ys, int n, double* out);
int me_uses_y(const me_program* p);
void me_program_free(me_program* p);

void me_free(char* s);

#ifdef __cplusplus
}

#include <string>
#include <string_view>

namespace me {
std::string solveJson(std::string_view input);
std::string latexJson(std::string_view input);
std::string graphJson(std::string_view input);
}  // namespace me
#endif

#endif /* MATHENGINE_H */
