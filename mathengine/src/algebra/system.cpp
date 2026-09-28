// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Systems of equations: linear (substitution, elimination, Cramer's rule, Gauss-Jordan)
// and small nonlinear systems by substitution.
#include <algorithm>
#include <cmath>

#include "calculus/calculus.h"
#include "core/poly.h"
#include "solution/plotutil.h"
#include "solvers.h"

namespace me {

namespace {

using Mat = std::vector<std::vector<Rational>>;

std::string matTex(const Mat& A, int augCol = -1, bool det = false) {
    size_t cols = A.empty() ? 0 : A[0].size();
    std::string spec;
    for (size_t c = 0; c < cols; ++c) spec += (augCol >= 0 && (int)c == augCol ? "|c" : "c");
    std::string s = det ? "\\left|\\begin{array}{" + spec + "}" : "\\left[\\begin{array}{" + spec + "}";
    for (size_t r = 0; r < A.size(); ++r) {
        for (size_t c = 0; c < cols; ++c) s += (c ? " & " : "") + toLatex(num(A[r][c]));
        if (r + 1 < A.size()) s += " \\\\ ";
    }
    s += det ? "\\end{array}\\right|" : "\\end{array}\\right]";
    return s;
}

Rational det(Mat A) {
    size_t n = A.size();
    Rational d(1);
    for (size_t c = 0; c < n; ++c) {
        size_t p = c;
        while (p < n && A[p][c].isZero()) ++p;
        if (p == n) return Rational(0);
        if (p != c) { std::swap(A[p], A[c]); d = -d; }
        d *= A[c][c];
        for (size_t r = c + 1; r < n; ++r) {
            Rational f = A[r][c] / A[c][c];
            for (size_t k = c; k < n; ++k) A[r][k] -= f * A[c][k];
        }
    }
    return d;
}

std::string linTex(const std::vector<Rational>& row, const std::vector<std::string>& vars) {
    std::vector<E> t;
    for (size_t i = 0; i < vars.size(); ++i) if (!row[i].isZero()) t.push_back(mul(num(row[i]), sym(vars[i])));
    E l = simplify(add(t));
    return toLatex(l) + " = " + toLatex(num(row.back()));
}

std::string casesTex(const Mat& aug, const std::vector<std::string>& vars) {
    std::string s = "\\begin{cases} ";
    for (size_t r = 0; r < aug.size(); ++r) s += (r ? " \\\\ " : "") + linTex(aug[r], vars);
    return s + " \\end{cases}";
}

std::string rowName(size_t i) { return "R_{" + std::to_string(i + 1) + "}"; }

// Gauss-Jordan with recorded row operations. Returns status.
SystemResult::Status gaussJordan(Mat aug, const std::vector<std::string>& vars, Method& mth, std::vector<Rational>& sol) {
    size_t n = aug.size(), nv = vars.size();
    mth.steps.push_back({"Write the augmented matrix", "Each row is an equation. The last column holds the constants.", matTex(aug, (int)nv), {}});
    size_t row = 0;
    std::vector<int> pivCol;
    for (size_t col = 0; col < nv && row < n; ++col) {
        size_t p = row;
        while (p < n && aug[p][col].isZero()) ++p;
        if (p == n) continue;
        if (p != row) {
            std::swap(aug[p], aug[row]);
            mth.steps.push_back({"Swap " + m(rowName(row)) + " and " + m(rowName(p)), "", matTex(aug, (int)nv), {}});
        }
        if (!aug[row][col].isOne()) {
            Rational inv = aug[row][col].inv();
            for (auto& v : aug[row]) v *= inv;
            mth.steps.push_back({"Scale " + m(rowName(row)), m(rowName(row) + " \\to " + toLatex(num(inv)) + rowName(row)) + " makes the pivot 1.", matTex(aug, (int)nv), {}});
        }
        std::vector<std::string> ops;
        for (size_t r = 0; r < n; ++r) {
            if (r == row || aug[r][col].isZero()) continue;
            Rational f = aug[r][col];
            for (size_t c = 0; c <= nv; ++c) aug[r][c] -= f * aug[row][c];
            std::string ft = f.isOne() ? "" : toLatex(num(f.abs()));
            ops.push_back(rowName(r) + " \\to " + rowName(r) + (f.isNeg() ? " + " : " - ") + ft + rowName(row));
        }
        if (!ops.empty()) {
            std::string t;
            for (size_t i = 0; i < ops.size(); ++i) t += (i ? ", \\ " : "") + ops[i];
            mth.steps.push_back({"Eliminate column " + std::to_string(col + 1), m(t), matTex(aug, (int)nv), {}});
        }
        pivCol.push_back((int)col);
        ++row;
    }
    for (size_t r = row; r < n; ++r) {
        if (!aug[r][nv].isZero()) {
            mth.steps.push_back({"Contradiction", "A row reads " + m("0 = " + toLatex(num(aug[r][nv]))) + ", so the system has no solution.", "", {}});
            return SystemResult::Status::None;
        }
    }
    if (row < nv) {
        mth.steps.push_back({"Free variables", "There are fewer pivots than unknowns, so there are infinitely many solutions.", casesTex(Mat(aug.begin(), aug.begin() + row), vars), {}});
        return SystemResult::Status::Infinite;
    }
    sol.assign(nv, Rational(0));
    for (size_t r = 0; r < row; ++r) sol[pivCol[r]] = aug[r][nv];
    std::string s;
    for (size_t i = 0; i < nv; ++i) s += (i ? ", \\quad " : "") + vars[i] + " = " + toLatex(num(sol[i]));
    mth.steps.push_back({"Read off the solution", "The matrix is now in reduced row echelon form.", s, {}});
    return SystemResult::Status::Unique;
}

void cramer(const Mat& aug, const std::vector<std::string>& vars, Method& mth) {
    size_t n = vars.size();
    Mat A(n, std::vector<Rational>(n));
    for (size_t r = 0; r < n; ++r) for (size_t c = 0; c < n; ++c) A[r][c] = aug[r][c];
    Rational D = det(A);
    mth.steps.push_back({"Compute the determinant of the coefficient matrix", "", "D = " + matTex(A, -1, true) + " = " + toLatex(num(D)), {}});
    if (D.isZero()) {
        mth.steps.push_back({"The determinant is zero", "Cramer's rule doesn't apply: the system has no unique solution.", "", {}});
        return;
    }
    std::string res;
    for (size_t k = 0; k < n; ++k) {
        Mat Ak = A;
        for (size_t r = 0; r < n; ++r) Ak[r][k] = aug[r][n];
        Rational Dk = det(Ak);
        mth.steps.push_back({"Replace column " + std::to_string(k + 1) + " with the constants", "",
                             "D_{" + vars[k] + "} = " + matTex(Ak, -1, true) + " = " + toLatex(num(Dk)), {}});
        res += (k ? ", \\quad " : "") + vars[k] + " = \\frac{D_{" + vars[k] + "}}{D} = " + toLatex(num(Dk / D));
    }
    mth.steps.push_back({"Divide", "", res, {}});
}

void substitution2(const Mat& aug, const std::vector<std::string>& vars, Method& mth) {
    // choose the equation/variable with coefficient ±1 if possible
    int be = 0, bv = 0;
    int best = 99;
    for (int e = 0; e < 2; ++e)
        for (int v = 0; v < 2; ++v) {
            const Rational& c = aug[e][v];
            if (c.isZero()) continue;
            int score = c.abs().isOne() ? 0 : (c.isInteger() ? 1 : 2);
            if (score < best) { best = score; be = e; bv = v; }
        }
    int ov = 1 - bv, oe = 1 - be;
    const std::string& X = vars[bv];
    const std::string& Y = vars[ov];
    // X = (c - b Y) / a
    Rational a = aug[be][bv], b = aug[be][ov], c = aug[be][2];
    E exprX = simplify(add(num(c / a), mul(num(-b / a), sym(Y))));
    mth.steps.push_back({"Solve one equation for " + m(X), "Use " + m(linTex(aug[be], vars)) + ".", X + " = " + toLatex(exprX), {}});
    // substitute into the other
    Rational a2 = aug[oe][bv], b2 = aug[oe][ov], c2 = aug[oe][2];
    E lhs = add(mul(num(a2), exprX), mul(num(b2), sym(Y)));
    std::string sub = toLatex(num(a2)) + "\\left(" + toLatex(exprX) + "\\right) + " + toLatex(simplify(mul(num(b2), sym(Y)))) + " = " + toLatex(num(c2));
    if (a2.isZero()) sub = linTex(aug[oe], vars);
    mth.steps.push_back({"Substitute into the other equation", "", sub, {}});
    E L = expand(lhs);
    mth.steps.push_back({"Simplify", "", toLatex(L) + " = " + toLatex(num(c2)), {}});
    auto p = Poly::from(L, Y);
    if (!p || p->coeff(1).isZero()) return;
    Rational yv = (c2 - p->coeff(0)) / p->coeff(1);
    mth.steps.push_back({"Solve for " + m(Y), "", Y + " = " + toLatex(num(yv)), {}});
    E xv = simplify(substitute(exprX, Y, num(yv)));
    mth.steps.push_back({"Substitute back to find " + m(X), "", X + " = " + toLatex(xv), {}});
}

void elimination2(Mat aug, const std::vector<std::string>& vars, Method& mth) {
    // integer coefficients
    for (auto& row : aug) {
        BigInt l(1);
        for (auto& v : row) l = l / BigInt::gcd(l, v.den()) * v.den();
        for (auto& v : row) v *= Rational(l);
    }
    // eliminate the first variable with a non-zero coefficient in both rows
    int v = (!aug[0][0].isZero() && !aug[1][0].isZero()) ? 0 : 1;
    int o = 1 - v;
    Rational a1 = aug[0][v].abs(), a2 = aug[1][v].abs();
    BigInt l = a1.num() / BigInt::gcd(a1.num(), a2.num()) * a2.num();
    Rational k1 = Rational(l) / aug[0][v], k2 = Rational(l) / aug[1][v];
    // make coefficients opposite
    k2 = -k2;
    std::vector<Rational> r1(3), r2(3), s(3);
    for (int c = 0; c < 3; ++c) { r1[c] = aug[0][c] * k1; r2[c] = aug[1][c] * k2; s[c] = r1[c] + r2[c]; }
    std::string t;
    if (!k1.isOne()) t += "Multiply the first equation by " + m(toLatex(num(k1))) + ". ";
    if (!k2.isOne()) t += "Multiply the second equation by " + m(toLatex(num(k2))) + ". ";
    mth.steps.push_back({"Make the " + m(vars[v]) + " coefficients opposite", t, casesTex({r1, r2}, vars), {}});
    mth.steps.push_back({"Add the equations", "The " + m(vars[v]) + " terms cancel.", linTex(s, vars), {}});
    if (s[o].isZero()) return;
    Rational yv = s[2] / s[o];
    mth.steps.push_back({"Solve for " + m(vars[o]), "", vars[o] + " = " + toLatex(num(yv)), {}});
    // back-substitute into first equation
    Rational xv = (aug[0][2] - aug[0][o] * yv) / aug[0][v];
    mth.steps.push_back({"Substitute back", "Put " + m(vars[o] + " = " + toLatex(num(yv))) + " into the first equation.", vars[v] + " = " + toLatex(num(xv)), {}});
}

// linear coefficients of f in vars (nullopt if not linear with rational coefficients)
std::optional<std::vector<Rational>> linearRow(const E& f, const std::vector<std::string>& vars) {
    std::vector<Rational> row;
    E rest = expand(f);
    for (auto& v : vars) {
        E c = simplify(derivativeOf(rest, v));
        if (!isNum(c)) return std::nullopt;
        row.push_back(c->num);
    }
    E k = rest;
    for (auto& v : vars) k = substitute(k, v, num(0));
    k = simplify(k);
    if (!isNum(k)) return std::nullopt;
    // verify linearity
    E rebuilt = num(k->num);
    std::vector<E> t{rebuilt};
    for (size_t i = 0; i < vars.size(); ++i) t.push_back(mul(num(row[i]), sym(vars[i])));
    if (!isNumVal(simplify(sub(expand(add(t)), rest)), 0)) return std::nullopt;
    row.push_back(-k->num);  // constant moved to the right-hand side
    return row;
}

}  // namespace

SystemResult solveSystemCore(const std::vector<std::pair<E, E>>& eqs, std::vector<std::string> vars) {
    SystemResult res;
    std::vector<E> fs;
    for (auto& [l, r] : eqs) fs.push_back(simplify(sub(l, r)));
    if (vars.empty()) {
        for (auto& f : fs) collectVars(f, vars);
        std::ranges::sort(vars);
    }
    res.vars = vars;
    // ---- linear?
    Mat aug;
    bool linear = true;
    for (auto& f : fs) {
        auto row = linearRow(f, vars);
        if (!row) { linear = false; break; }
        aug.push_back(*row);
    }
    if (linear && !aug.empty()) {
        std::vector<Rational> sol;
        Method gj{"Gauss-Jordan elimination", {}};
        auto st = gaussJordan(aug, vars, gj, sol);
        res.status = st;
        if (st == SystemResult::Status::Unique) {
            std::vector<E> v;
            for (auto& r : sol) v.push_back(num(r));
            res.solutions.push_back(v);
            if (vars.size() == 2 && aug.size() == 2) {
                Method ms{"Substitution", {}}, me_{"Elimination", {}};
                substitution2(aug, vars, ms);
                elimination2(aug, vars, me_);
                res.methods.push_back(ms);
                res.methods.push_back(me_);
            }
        }
        res.methods.push_back(gj);
        if (vars.size() == aug.size() && vars.size() <= 3 && st == SystemResult::Status::Unique) {
            Method mc{"Cramer's rule", {}};
            cramer(aug, vars, mc);
            res.methods.push_back(mc);
        }
        return res;
    }
    // ---- nonlinear: substitution with an equation that is linear in some variable
    if (vars.size() == 2 && fs.size() == 2) {
        for (int e = 0; e < 2; ++e) {
            for (int vi = 0; vi < 2; ++vi) {
                const std::string& v = vars[vi];
                const std::string& w = vars[1 - vi];
                E f = expand(fs[e]);
                if (polyDegree(f, v) != 1) continue;
                E c1 = simplify(derivativeOf(f, v));
                if (containsSym(c1, v) || containsSym(c1, w)) continue;  // keep it simple: constant coefficient
                E c0 = simplify(substitute(f, v, num(0)));
                E vexpr = simplify(divide(neg(c0), c1));
                Method mth{"Substitution", {}};
                mth.steps.push_back({"Solve for " + m(v), "Use the equation " + m(toLatex(fs[e]) + " = 0") + ".", v + " = " + toLatex(vexpr), {}});
                E other = expand(substitute(fs[1 - e], v, vexpr));
                mth.steps.push_back({"Substitute into the other equation", "", toLatex(other) + " = 0", {}});
                std::vector<E> roots;
                Solution s1 = solveEquation(other, num(0), w, "", false, &roots);
                if (!s1.ok) continue;
                Step solveStep{"Solve for " + m(w), "", s1.answerLatex, {}};
                if (!s1.methods.empty()) solveStep.sub = s1.methods[0].steps;
                mth.steps.push_back(solveStep);
                std::string back;
                for (auto& r : roots) {
                    if (containsSym(r, "n")) continue;
                    E vv = simplify(substitute(vexpr, w, r));
                    std::vector<E> sol(2);
                    sol[vi] = vv;
                    sol[1 - vi] = r;
                    res.solutions.push_back(sol);
                    back += (back.empty() ? "" : ", \\quad ") + std::string("\\left(") + toLatex(sol[0]) + ", " + toLatex(sol[1]) + "\\right)";
                }
                mth.steps.push_back({"Substitute back", "Find " + m(v) + " for each value of " + m(w) + ".", back.empty() ? "\\text{no real solution}" : back, {}});
                res.methods.push_back(mth);
                res.status = res.solutions.empty() ? SystemResult::Status::None : SystemResult::Status::Unique;
                return res;
            }
        }
    }
    res.status = SystemResult::Status::Failed;
    return res;
}

Solution solveSystem(const std::vector<std::pair<E, E>>& eqs, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "system";
    sol.anim = "system";
    sol.title = "Solve the system of equations";
    sol.inputLatex = inputLatex;
    SystemResult r = solveSystemCore(eqs);
    using S = SystemResult::Status;
    if (r.status == S::Failed) {
        sol.ok = false;
        sol.error = "I can only solve linear systems or two equations where one is linear in a variable.";
        return sol;
    }
    sol.methods = r.methods;
    if (r.status == S::None) {
        sol.answerLatex = "\\text{No solution}";
        sol.answers.push_back(sol.answerLatex);
    } else if (r.status == S::Infinite) {
        sol.answerLatex = "\\text{Infinitely many solutions}";
        sol.answers.push_back(sol.answerLatex);
    } else {
        std::string ans;
        for (size_t k = 0; k < r.solutions.size(); ++k) {
            std::string one;
            for (size_t i = 0; i < r.vars.size(); ++i) {
                one += (i ? ", \\ " : "") + r.vars[i] + " = " + toLatex(r.solutions[k][i]);
                if (r.solutions.size() == 1) sol.answers.push_back(r.vars[i] + " = " + toLatex(r.solutions[k][i]));
            }
            if (r.solutions.size() > 1) sol.answers.push_back(one);  // one line per solution set
            ans += (k ? " \\quad \\text{or} \\quad " : "") + one;
        }
        sol.answerLatex = ans;
    }
    // graph for two variables x, y
    if (r.vars.size() == 2) {
        PlotSpec ps;
        std::string vx = r.vars[0], vy = r.vars[1];
        for (auto& [l, rr] : eqs) {
            E f = simplify(sub(l, rr));
            E g = substitute(substitute(f, vx, sym("x")), vy, sym("y"));
            E fe = expand(g);
            std::string label = toLatex(simplify(l)) + " = " + toLatex(simplify(rr));
            if (polyDegree(fe, "y") == 1) {
                E c1 = simplify(derivativeOf(fe, "y"));
                if (!containsSym(c1, "y") && isNum(c1)) {
                    E yx = simplify(divide(neg(substitute(fe, "y", num(0))), c1));
                    ps.curves.push_back({PlotCurve::Type::Explicit, toPlain(yx), label});
                    continue;
                }
            }
            if (!containsSym(fe, "y") && polyDegree(fe, "x") == 1) {
                E c1 = simplify(derivativeOf(fe, "x"));
                ps.curves.push_back({PlotCurve::Type::Vertical, toPlain(simplify(divide(neg(substitute(fe, "x", num(0))), c1))), label});
                continue;
            }
            ps.curves.push_back({PlotCurve::Type::Implicit, toPlain(g), label});
        }
        std::vector<double> xs;
        for (auto& s : r.solutions) {
            auto a = evalReal(s[0]), b = evalReal(s[1]);
            if (a && b) {
                ps.points.push_back({*a, *b, "\\left(" + toLatex(s[0]) + ", " + toLatex(s[1]) + "\\right)"});
                xs.push_back(*a);
                ps.ymin = std::min(ps.ymin, *b - 2);
                ps.ymax = std::max(ps.ymax, *b + 2);
            }
        }
        includeX(ps, xs);
        sol.plot = ps;
    }
    return sol;
}

}  // namespace me
