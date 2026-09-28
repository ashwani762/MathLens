// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Matrices: literals [[a,b],[c,d]], + - * ^, scalar multiples, det, inv, transpose (A^T),
// rref, rank, trace, identity(n) and eigenvalues, with step-by-step explanations.
// Entries may be symbolic (e.g. [[a, 1], [0, b]]).
#include <regex>
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>
#include <functional>

#include "matrix/matrix.h"
#include "solvers.h"

namespace me {

namespace {

using T = RNode::T;
using Mat = std::vector<std::vector<E>>;

std::string matTex(const Mat& A, const char* env = "bmatrix") {
    std::string s = std::string("\\begin{") + env + "} ";
    for (size_t i = 0; i < A.size(); ++i) {
        if (i) s += " \\\\ ";
        for (size_t j = 0; j < A[i].size(); ++j) s += (j ? " & " : "") + toLatex(A[i][j]);
    }
    return s + " \\end{" + env + "}";
}

std::string augTex(const Mat& A, size_t split) {
    std::string spec;
    for (size_t j = 0; j < A[0].size(); ++j) spec += (j == split ? "|c" : "c");
    std::string s = "\\left[\\begin{array}{" + spec + "} ";
    for (size_t i = 0; i < A.size(); ++i) {
        if (i) s += " \\\\ ";
        for (size_t j = 0; j < A[i].size(); ++j) s += (j ? " & " : "") + toLatex(A[i][j]);
    }
    return s + " \\end{array}\\right]";
}

std::string dims(const Mat& A) { return std::to_string(A.size()) + " \\times " + std::to_string(A.empty() ? 0 : A[0].size()); }

bool isZero(const E& e) { return isNumVal(simplify(e), 0); }

Mat identity(size_t n) {
    Mat I(n, std::vector<E>(n, num(0)));
    for (size_t i = 0; i < n; ++i) I[i][i] = num(1);
    return I;
}

struct Value {
    bool isMat = false;
    E scalar;
    Mat m;
};

std::string par(const E& e) {
    std::string s = toLatex(e);
    return (e->kind == Kind::Add || isNegativeTerm(e)) ? "\\left(" + s + "\\right)" : s;
}


// Eigenvalues of a real matrix, numerically: Householder reduction to Hessenberg form, then complex single-shift QR
// (Wilkinson shifts, Givens rotations, deflation). Stable for large n where the characteristic
// polynomial is badly conditioned.
std::vector<std::complex<double>> eigNumeric(std::vector<std::vector<double>> a) {
    using C = std::complex<double>;
    int n = (int)a.size();
    // Upper Hessenberg form by Householder reflections: for each column k, a mirror P = I - 2vv^T/(v^T v)
    // maps the entries below the subdiagonal to zero; A <- P A P keeps the eigenvalues.
    for (int k = 0; k + 2 < n; ++k) {
        std::vector<double> v(n, 0.0);
        double norm2 = 0;
        for (int r = k + 1; r < n; ++r) { v[r] = a[r][k]; norm2 += v[r] * v[r]; }
        double norm = std::sqrt(norm2);
        if (norm < 1e-300) continue;                       // column already reduced
        double alpha = v[k + 1] > 0 ? -norm : norm;        // sign chosen to avoid cancellation
        v[k + 1] -= alpha;
        double vtv = 0;
        for (int r = k + 1; r < n; ++r) vtv += v[r] * v[r];
        if (vtv < 1e-300) continue;
        // left: A <- A - v (2 v^T A / v^T v)
        for (int col = 0; col < n; ++col) {
            double dot = 0;
            for (int r = k + 1; r < n; ++r) dot += v[r] * a[r][col];
            double f = 2 * dot / vtv;
            for (int r = k + 1; r < n; ++r) a[r][col] -= f * v[r];
        }
        // right: A <- A - (2 A v / v^T v) v^T
        for (int row = 0; row < n; ++row) {
            double dot = 0;
            for (int c2 = k + 1; c2 < n; ++c2) dot += a[row][c2] * v[c2];
            double f = 2 * dot / vtv;
            for (int c2 = k + 1; c2 < n; ++c2) a[row][c2] -= f * v[c2];
        }
    }
    std::vector<std::vector<C>> H(n, std::vector<C>(n));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) H[i][j] = (j >= i - 1) ? a[i][j] : 0.0;
    std::vector<C> ev;
    int hi = n - 1, iter = 0;
    std::vector<C> cs(n), sn(n);
    while (hi >= 0) {
        if (hi == 0) { ev.push_back(H[0][0]); break; }
        int lo = hi;
        while (lo > 0 && std::abs(H[lo][lo - 1]) > 1e-14 * (std::abs(H[lo][lo]) + std::abs(H[lo - 1][lo - 1]) + 1e-300)) --lo;
        if (lo == hi) { ev.push_back(H[hi][hi]); --hi; iter = 0; continue; }
        if (++iter > 400) return {};  // no convergence
        C A_ = H[hi - 1][hi - 1], B_ = H[hi - 1][hi], C_ = H[hi][hi - 1], D_ = H[hi][hi];
        C tr = (A_ + D_) / 2.0, disc = std::sqrt((A_ - D_) * (A_ - D_) / 4.0 + B_ * C_);
        C mu = std::abs(tr + disc - D_) < std::abs(tr - disc - D_) ? tr + disc : tr - disc;
        if (iter % 11 == 10) mu = H[hi][hi] + std::abs(H[hi][hi - 1]) * 0.75;  // exceptional shift
        for (int k = lo; k <= hi; ++k) H[k][k] -= mu;
        for (int k = lo; k < hi; ++k) {
            C x = H[k][k], y = H[k + 1][k];
            double r = std::sqrt(std::norm(x) + std::norm(y));
            C c = r == 0 ? 1.0 : x / r, s_ = r == 0 ? 0.0 : y / r;
            cs[k] = c; sn[k] = s_;
            for (int j = k; j <= hi; ++j) {
                C u = H[k][j], v = H[k + 1][j];
                H[k][j] = std::conj(c) * u + std::conj(s_) * v;
                H[k + 1][j] = -s_ * u + c * v;
            }
        }
        for (int k = lo; k < hi; ++k) {
            C c = cs[k], s_ = sn[k];
            for (int i = lo; i <= std::min(k + 2, hi); ++i) {
                C u = H[i][k], v = H[i][k + 1];
                H[i][k] = u * c + v * s_;
                H[i][k + 1] = -u * std::conj(s_) + v * std::conj(c);
            }
        }
        for (int k = lo; k <= hi; ++k) H[k][k] += mu;
    }
    std::sort(ev.begin(), ev.end(), [](C x, C y) { return x.real() != y.real() ? x.real() < y.real() : x.imag() < y.imag(); });
    return ev;
}

std::string fmtNum(double v) {
    if (std::fabs(v) < 5e-9) v = 0;
    char b[40];
    snprintf(b, sizeof b, "%.8g", v);
    std::string s = b;
    if (s.find('e') != std::string::npos) {  // 1.5e-05 -> 1.5 \cdot 10^{-5}
        auto e = s.find('e');
        s = s.substr(0, e) + " \\cdot 10^{" + std::to_string(std::stoi(s.substr(e + 1))) + "}";
    }
    return s;
}


class MatrixEval {
public:
    std::vector<Step> steps;
    std::string error;
    std::string kind = "matrix";

    std::optional<Value> eval(const R& r) {
        if (!containsMatrix(r)) {
            auto e = toExpr(r);
            if (!e) { error = e.error().message; return std::nullopt; }
            return Value{false, simplify(*e), {}};
        }
        switch (r->t) {
            case T::Func: return func(r);
            case T::Neg: {
                auto a = eval(r->kids[0]);
                if (!a) return std::nullopt;
                return scale(num(-1), *a, false);
            }
            case T::Bin: return bin(r);
            default: error = "Unsupported matrix expression"; return std::nullopt;
        }
    }

    // ---- determinant with steps (cofactor expansion up to 4x4, elimination beyond)
    E det(const Mat& A, std::vector<Step>* st, int depth = 0) {
        size_t n = A.size();
        if (n == 1) return A[0][0];
        if (n == 2) {
            E d = simplify(sub(mul(A[0][0], A[1][1]), mul(A[0][1], A[1][0])));
            if (st) st->push_back({"Use the 2×2 formula", m("\\det\\begin{bmatrix} a & b \\\\ c & d \\end{bmatrix} = ad - bc"),
                                   par(A[0][0]) + " \\cdot " + par(A[1][1]) + " - " + par(A[0][1]) + " \\cdot " + par(A[1][0]) + " = " + toLatex(d), {}});
            return d;
        }
        if (n <= 4) {
            std::vector<E> terms;
            std::string expansion;
            std::vector<Step> sub;
            for (size_t j = 0; j < n; ++j) {
                if (isZero(A[0][j])) continue;
                Mat minor;
                for (size_t i = 1; i < n; ++i) {
                    std::vector<E> row;
                    for (size_t k = 0; k < n; ++k) if (k != j) row.push_back(A[i][k]);
                    minor.push_back(row);
                }
                std::vector<Step> ms;
                E md = det(minor, depth == 0 ? &ms : nullptr, depth + 1);
                E sign = num(j % 2 ? -1 : 1);
                terms.push_back(mul({sign, A[0][j], md}));
                expansion += (expansion.empty() ? (j % 2 ? "-" : "") : (j % 2 ? " - " : " + ")) + par(A[0][j]) + matTex(minor, "vmatrix");
                if (depth == 0) {
                    Step s{"Minor for entry " + m(toLatex(A[0][j])), "Delete row 1 and column " + std::to_string(j + 1) + ".", matTex(minor, "vmatrix") + " = " + toLatex(md), ms};
                    sub.push_back(s);
                }
            }
            E d = simplify(add(terms));
            if (st) {
                st->push_back({"Expand along the first row", "Multiply each entry by its minor, alternating signs " + m("+, -, +, \\dots"), expansion.empty() ? "0" : expansion, {}});
                for (auto& s : sub) st->push_back(s);
                st->push_back({"Combine", "", toLatex(d), {}});
            }
            return d;
        }
        // elimination
        Mat U = A;
        E d = num(1);
        for (size_t c = 0; c < n; ++c) {
            size_t p = c;
            while (p < n && isZero(U[p][c])) ++p;
            if (p == n) return num(0);
            if (p != c) { std::swap(U[p], U[c]); d = simplify(neg(d)); }
            d = simplify(mul(d, U[c][c]));
            for (size_t i = c + 1; i < n; ++i) {
                E f = simplify(divide(U[i][c], U[c][c]));
                for (size_t k = c; k < n; ++k) U[i][k] = simplify(sub(U[i][k], mul(f, U[c][k])));
            }
        }
        if (st) st->push_back({"Row reduce to triangular form", "The determinant is the product of the diagonal (sign flips on row swaps).", toLatex(d), {}});
        return d;
    }

    // Gauss-Jordan to RREF; records steps; returns rank
    size_t rref(Mat& A, size_t cols, std::vector<Step>* st, size_t augSplit = 0) {
        bool numeric = true;
        for (auto& r : A) for (auto& x : r) if (!isNum(x)) numeric = false;
        if (numeric) return rrefExact(A, cols, st, augSplit);
        size_t n = A.size(), row = 0;
        auto show = [&]() { return augSplit ? augTex(A, augSplit) : matTex(A); };
        for (size_t c = 0; c < cols && row < n; ++c) {
            size_t p = row;
            while (p < n && isZero(A[p][c])) ++p;
            if (p == n) continue;
            if (p != row) {
                std::swap(A[p], A[row]);
                if (st) st->push_back({"Swap " + m("R_{" + std::to_string(row + 1) + "}") + " and " + m("R_{" + std::to_string(p + 1) + "}"), "", show(), {}});
            }
            E piv = simplify(A[row][c]);
            if (!isNumVal(piv, 1)) {
                for (auto& x : A[row]) x = simplify(divide(x, piv));
                if (st) st->push_back({"Make the pivot 1", m("R_{" + std::to_string(row + 1) + "} \\to \\frac{1}{" + toLatex(piv) + "} R_{" + std::to_string(row + 1) + "}"), show(), {}});
            }
            std::string ops;
            for (size_t i = 0; i < n; ++i) {
                if (i == row || isZero(A[i][c])) continue;
                E f = simplify(A[i][c]);
                for (size_t k = 0; k < A[i].size(); ++k) A[i][k] = simplify(sub(A[i][k], mul(f, A[row][k])));
                ops += (ops.empty() ? "" : ", \\ ") + std::string("R_{") + std::to_string(i + 1) + "} \\to R_{" + std::to_string(i + 1) + "} - " + par(f) + "R_{" + std::to_string(row + 1) + "}";
            }
            if (st && !ops.empty()) st->push_back({"Clear column " + std::to_string(c + 1), m(ops), show(), {}});
            ++row;
        }
        return row;
    }


    // Exact rational Gauss-Jordan for numeric matrices (fast up to 20x20 and beyond). Every row operation is
    // recorded for small matrices; large ones get a single summary step so the solution stays readable.
    size_t rrefExact(Mat& A, size_t cols, std::vector<Step>* st, size_t augSplit) {
        size_t n = A.size(), w = A[0].size(), row = 0, opsCount = 0;
        std::vector<std::vector<Rational>> Q(n, std::vector<Rational>(w));
        for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < w; ++j) Q[i][j] = A[i][j]->num;
        bool record = st && n <= 6;
        auto sync = [&]() { for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < w; ++j) A[i][j] = num(Q[i][j]); };
        auto show = [&]() { sync(); return augSplit ? augTex(A, augSplit) : matTex(A); };
        for (size_t c = 0; c < cols && row < n; ++c) {
            size_t p = row;
            while (p < n && Q[p][c].isZero()) ++p;
            if (p == n) continue;
            if (p != row) {
                std::swap(Q[p], Q[row]); ++opsCount;
                if (record) st->push_back({"Swap " + m("R_{" + std::to_string(row + 1) + "}") + " and " + m("R_{" + std::to_string(p + 1) + "}"), "", show(), {}});
            }
            Rational piv = Q[row][c];
            if (!(piv.num() == BigInt(1) && piv.den() == BigInt(1))) {
                Rational iv = piv.inv();
                for (auto& x : Q[row]) x = x * iv;
                ++opsCount;
                if (record) st->push_back({"Make the pivot 1", m("R_{" + std::to_string(row + 1) + "} \\to \\frac{1}{" + toLatex(num(piv)) + "} R_{" + std::to_string(row + 1) + "}"), show(), {}});
            }
            std::string ops;
            for (size_t i = 0; i < n; ++i) {
                if (i == row || Q[i][c].isZero()) continue;
                Rational f = Q[i][c];
                for (size_t k = 0; k < w; ++k) if (!Q[row][k].isZero()) Q[i][k] = Q[i][k] - f * Q[row][k];
                ++opsCount;
                if (record) ops += (ops.empty() ? "" : ", \\ ") + std::string("R_{") + std::to_string(i + 1) + "} \\to R_{" + std::to_string(i + 1) + "} - " + par(num(f)) + "R_{" + std::to_string(row + 1) + "}";
            }
            if (record && !ops.empty()) st->push_back({"Clear column " + std::to_string(c + 1), m(ops), show(), {}});
            ++row;
        }
        sync();
        if (st && !record)
            st->push_back({"Gauss–Jordan elimination", "For each column: swap a non-zero pivot up, scale it to 1, then clear the rest of the column (" +
                           std::to_string(opsCount) + " row operations in exact fractions).", augSplit ? augTex(A, augSplit) : matTex(A), {}});
        return row;
    }

private:
    static bool containsMatrix(const R& r) {
        if (r->t == T::Func && (r->op == "matrix" || r->op == "identity" || r->op == "tensor")) return true;
        for (auto& k : r->kids) if (containsMatrix(k)) return true;
        return false;
    }

    std::optional<Value> scale(const E& s, const Value& v, bool record = true) {
        if (!v.isMat) return Value{false, simplify(mul(s, v.scalar)), {}};
        Mat R2 = v.m;
        for (auto& row : R2) for (auto& x : row) x = simplify(mul(s, x));
        if (record) steps.push_back({"Multiply every entry by " + m(toLatex(s)), "", toLatex(s) + matTex(v.m) + " = " + matTex(R2), {}});
        return Value{true, nullptr, R2};
    }

    std::optional<Value> matmul(const Mat& A, const Mat& B) {
        if (A[0].size() != B.size()) {
            error = "Can't multiply a " + std::string(dims(A)) + " matrix by a " + dims(B) + " matrix: the inner sizes must match.";
            return std::nullopt;
        }
        size_t n = A.size(), p = B[0].size(), k = B.size();
        Mat C(n, std::vector<E>(p));
        Step s{"Multiply rows by columns", "Entry " + m("c_{ij}") + " is row " + m("i") + " of the first matrix times column " + m("j") + " of the second.", "", {}};
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < p; ++j) {
                std::vector<E> t;
                std::string expr;
                for (size_t q = 0; q < k; ++q) {
                    t.push_back(mul(A[i][q], B[q][j]));
                    expr += (q ? " + " : "") + par(A[i][q]) + " \\cdot " + par(B[q][j]);
                }
                C[i][j] = simplify(add(t));
                if (n * p <= 9) s.sub.push_back({"", "", "c_{" + std::to_string(i + 1) + std::to_string(j + 1) + "} = " + expr + " = " + toLatex(C[i][j]), {}});
            }
        s.latex = matTex(A) + matTex(B) + " = " + matTex(C);
        steps.push_back(s);
        return Value{true, nullptr, C};
    }

    std::optional<Value> inverse(const Mat& A) {
        size_t n = A.size();
        if (n != A[0].size()) { error = "Only square matrices have an inverse."; return std::nullopt; }
        std::vector<Step> ds;
        E d = det(A, &ds);
        Step detStep{"Compute the determinant", "", "\\det(A) = " + toLatex(d), ds};
        steps.push_back(detStep);
        if (isZero(d)) {
            error = "The determinant is 0, so the matrix is singular and has no inverse.";
            return std::nullopt;
        }
        if (n == 2) {
            Mat adj{{A[1][1], simplify(neg(A[0][1]))}, {simplify(neg(A[1][0])), A[0][0]}};
            Mat R2 = adj;
            for (auto& row : R2) for (auto& x : row) x = simplify(divide(x, d));
            steps.push_back({"Use the 2×2 inverse formula", "Swap the diagonal, negate the off-diagonal, and divide by the determinant.",
                             "A^{-1} = \\frac{1}{" + toLatex(d) + "}" + matTex(adj) + " = " + matTex(R2), {}});
            return Value{true, nullptr, R2};
        }
        Mat aug = A;
        Mat I = identity(n);
        for (size_t i = 0; i < n; ++i) aug[i].insert(aug[i].end(), I[i].begin(), I[i].end());
        steps.push_back({"Augment with the identity matrix", "Row reduce " + m("[A \\mid I]") + " until the left side becomes " + m("I") + ".", augTex(aug, n), {}});
        std::vector<Step> rs;
        rref(aug, n, &rs, n);
        Step rr{"Row reduce", "", augTex(aug, n), rs};
        steps.push_back(rr);
        Mat R2(n);
        for (size_t i = 0; i < n; ++i) R2[i] = std::vector<E>(aug[i].begin() + n, aug[i].end());
        steps.push_back({"Read off the inverse", "The right half is " + m("A^{-1}") + ".", matTex(R2), {}});
        return Value{true, nullptr, R2};
    }

    std::optional<Value> bin(const R& r) {
        const std::string& op = r->op;
        auto a = eval(r->kids[0]);
        if (!a) return std::nullopt;
        if (op == "^") {
            if (!a->isMat) { error = "Unsupported power"; return std::nullopt; }
            const R& ex = r->kids[1];
            if (ex->t == T::Sym && ex->op == "T") return transpose(a->m);
            auto b = eval(ex);
            if (!b || b->isMat || !isInteger(b->scalar)) { error = "Matrix powers must be integers (or T for transpose)"; return std::nullopt; }
            long long n = b->scalar->num.num().toInt64();
            if (a->m.size() != a->m[0].size()) { error = "Only square matrices can be raised to a power."; return std::nullopt; }
            if (n == 0) return Value{true, nullptr, identity(a->m.size())};
            Mat base = a->m;
            if (n < 0) {
                auto inv = inverse(a->m);
                if (!inv) return std::nullopt;
                base = inv->m;
                n = -n;
                if (n == 1) return inv;
            }
            if (n > 20) { error = "Power too large"; return std::nullopt; }
            Mat cur = base;
            for (long long k = 1; k < n; ++k) {
                auto p = matmul(cur, base);
                if (!p) return std::nullopt;
                cur = p->m;
            }
            return Value{true, nullptr, cur};
        }
        auto b = eval(r->kids[1]);
        if (!b) return std::nullopt;
        if (op == "+" || op == "-") {
            if (a->isMat != b->isMat) { error = "Can't add a matrix and a number."; return std::nullopt; }
            if (!a->isMat) return Value{false, simplify(op == "+" ? add(a->scalar, b->scalar) : sub(a->scalar, b->scalar)), {}};
            if (a->m.size() != b->m.size() || a->m[0].size() != b->m[0].size()) { error = "Matrices must have the same size to be added."; return std::nullopt; }
            Mat C = a->m;
            for (size_t i = 0; i < C.size(); ++i)
                for (size_t j = 0; j < C[i].size(); ++j) C[i][j] = simplify(op == "+" ? add(a->m[i][j], b->m[i][j]) : sub(a->m[i][j], b->m[i][j]));
            steps.push_back({op == "+" ? "Add corresponding entries" : "Subtract corresponding entries", "", matTex(a->m) + " " + op + " " + matTex(b->m) + " = " + matTex(C), {}});
            return Value{true, nullptr, C};
        }
        if (op == "*") {
            if (!a->isMat) return scale(a->scalar, *b);
            if (!b->isMat) return scale(b->scalar, *a);
            return matmul(a->m, b->m);
        }
        if (op == "/" || op == "div") {
            if (b->isMat) { error = "Dividing by a matrix isn't defined; multiply by its inverse instead."; return std::nullopt; }
            return scale(simplify(pow(b->scalar, num(-1))), *a);
        }
        error = "Unsupported operator";
        return std::nullopt;
    }

    std::optional<Value> transpose(const Mat& A) {
        Mat R2(A[0].size(), std::vector<E>(A.size()));
        for (size_t i = 0; i < A.size(); ++i)
            for (size_t j = 0; j < A[0].size(); ++j) R2[j][i] = A[i][j];
        steps.push_back({"Transpose", "Rows become columns.", matTex(A) + "^{T} = " + matTex(R2), {}});
        return Value{true, nullptr, R2};
    }

    std::optional<Value> func(const R& r) {
        const std::string& n = r->op;
        if (n == "matrix") {
            Mat A;
            for (auto& row : r->kids) {
                std::vector<E> er;
                for (auto& x : row->kids) {
                    auto e = toExpr(x);
                    if (!e) { error = e.error().message; return std::nullopt; }
                    er.push_back(simplify(*e));
                }
                A.push_back(er);
            }
            return Value{true, nullptr, A};
        }
        if (n == "identity") {
            auto k = toExpr(r->kids[0]);
            if (!k || !isInteger(simplify(*k))) { error = "identity(n) needs an integer"; return std::nullopt; }
            long long sz = simplify(*k)->num.num().toInt64();
            if (sz < 1 || sz > 12) { error = "Size must be between 1 and 12"; return std::nullopt; }
            return Value{true, nullptr, identity((size_t)sz)};
        }
        auto a = eval(r->kids[0]);
        if (!a) return std::nullopt;
        if (!a->isMat) { error = n + " needs a matrix"; return std::nullopt; }
        const Mat& A = a->m;
        bool square = A.size() == A[0].size();
        if (n == "det") {
            if (!square) { error = "The determinant is only defined for square matrices."; return std::nullopt; }
            kind = "determinant";
            std::vector<Step> ds;
            E d = det(A, &ds);
            for (auto& s : ds) steps.push_back(s);
            return Value{false, d, {}};
        }
        if (n == "inv") return inverse(A);
        if (n == "transpose") return transpose(A);
        if (n == "trace") {
            if (!square) { error = "The trace needs a square matrix."; return std::nullopt; }
            std::vector<E> d;
            std::string t;
            for (size_t i = 0; i < A.size(); ++i) { d.push_back(A[i][i]); t += (i ? " + " : "") + par(A[i][i]); }
            E s = simplify(add(d));
            steps.push_back({"Add the diagonal entries", "", t + " = " + toLatex(s), {}});
            return Value{false, s, {}};
        }
        if (n == "rref" || n == "rank") {
            Mat B = A;
            std::vector<Step> rs;
            steps.push_back({"Start row reduction", "", matTex(B), {}});
            size_t rk = rref(B, B[0].size(), &rs);
            for (auto& s : rs) steps.push_back(s);
            if (n == "rank") {
                steps.push_back({"Count the pivots", "The rank is the number of non-zero rows in reduced row echelon form.", "\\operatorname{rank} = " + std::to_string(rk), {}});
                return Value{false, num((long long)rk), {}};
            }
            return Value{true, nullptr, B};
        }
        if (n == "eig") {
            if (!square) { error = "Eigenvalues need a square matrix."; return std::nullopt; }
            kind = "eigenvalues";
            E lam = sym("lambda");
            bool numericA = true;
            for (auto& r : A) for (auto& x : r) if (!isNum(x)) numericA = false;
            if (numericA && A.size() >= 7) {
                std::vector<std::vector<double>> D(A.size(), std::vector<double>(A.size()));
                for (size_t i = 0; i < A.size(); ++i) for (size_t j = 0; j < A.size(); ++j) D[i][j] = A[i][j]->num.toDouble();
                auto ev = eigNumeric(D);
                if (ev.empty()) { error = "The eigenvalue iteration did not converge."; return std::nullopt; }
                steps.push_back({"Why not the characteristic polynomial?", "For a " + std::to_string(A.size()) + "\u00d7" + std::to_string(A.size()) +
                                 " matrix, " + m("\\det(A - \\lambda I)") + " has degree " + std::to_string(A.size()) + ", and its roots are extremely sensitive to rounding. A stable numerical method is used instead.", "", {}});
                steps.push_back({"Reduce to Hessenberg form", "Similarity transforms keep the eigenvalues but zero everything below the first subdiagonal.", "", {}});
                steps.push_back({"QR iteration", "Repeatedly factor " + m("H - \\mu I = QR") + " and form " + m("RQ + \\mu I") + ". The subdiagonal shrinks to zero and the eigenvalues appear on the diagonal.", "", {}});
                for (auto& z : ev) {
                    double re = z.real(), im = std::fabs(z.imag()) < 1e-9 * std::max(1.0, std::abs(z)) ? 0 : z.imag();
                    std::string t = fmtNum(re);
                    if (im != 0) t = (re == 0 ? "" : t + (im > 0 ? " + " : " - ")) + fmtNum(re == 0 ? im : std::fabs(im)) + "i";
                    eigenTex.push_back(t);
                }
                return Value{false, num(0), {}};
            }
            Mat L = A;
            for (size_t i = 0; i < L.size(); ++i) L[i][i] = simplify(sub(A[i][i], lam));
            std::vector<Step> ds;
            E p;
            size_t n = A.size();
            if (n <= 4) p = expand(det(L, &ds));
            else {
                // Faddeev-LeVerrier: exact coefficients from traces, no symbolic elimination (which explodes for n >= 5)
                // M_0 = 0, c_n = 1;  M_k = A M_{k-1} + c_{n-k+1} I;  c_{n-k} = -tr(A M_k) / k
                std::vector<E> c(n + 1, num(0));
                c[n] = num(1);
                Mat Mk(n, std::vector<E>(n, num(0)));
                for (size_t k = 1; k <= n; ++k) {
                    Mat AM(n, std::vector<E>(n, num(0)));
                    for (size_t i = 0; i < n; ++i)
                        for (size_t j = 0; j < n; ++j) {
                            std::vector<E> t;
                            for (size_t q = 0; q < n; ++q) t.push_back(mul(A[i][q], Mk[q][j]));
                            AM[i][j] = simplify(add(t));
                        }
                    for (size_t i = 0; i < n; ++i) AM[i][i] = simplify(add(AM[i][i], c[n - k + 1]));
                    Mk = AM;
                    std::vector<E> tr;
                    for (size_t i = 0; i < n; ++i) {
                        std::vector<E> t;
                        for (size_t q = 0; q < n; ++q) t.push_back(mul(A[i][q], Mk[q][i]));
                        tr.push_back(add(t));
                    }
                    c[n - k] = simplify(divide(neg(add(tr)), num((long long)k)));
                }
                std::vector<E> terms;
                for (size_t k = 0; k <= n; ++k) terms.push_back(mul(c[k], pow(lam, num((long long)k))));
                p = expand(simplify(add(terms)));
                if (n % 2) p = expand(simplify(neg(p)));  // det(A - lambda I) = (-1)^n * charpoly
                ds.push_back({"Use the Faddeev–LeVerrier method", "Exact coefficients from traces of matrix powers, no cofactor expansion needed.", toLatex(p), {}});
            }
            Step cs{"Set up the characteristic equation", m("\\det(A - \\lambda I) = 0"), matTex(L, "vmatrix") + " = 0", ds};
            steps.push_back(cs);
            steps.push_back({"Expand the determinant", "", toLatex(p) + " = 0", {}});
            std::vector<E> roots;
            Solution s = solveEquation(p, num(0), "lambda", "", true, &roots);
            std::string ansTex = std::regex_replace(s.answerLatex, std::regex("(^|[^\\\\])lambda"), "$1\\lambda");
            Step solve{"Solve for " + m("\\lambda"), "", ansTex, s.methods.empty() ? std::vector<Step>{} : s.methods[0].steps};
            steps.push_back(solve);
            eigen = roots;
            return Value{false, num(0), {}};
        }
        error = "Unknown matrix function " + n;
        return std::nullopt;
    }

public:
    std::optional<Value> evalPublic(const R& r) { return eval(r); }
    std::optional<Value> matmulPublic(const Mat& A, const Mat& B) { return matmul(A, B); }
    std::vector<E> eigen;
    std::vector<std::string> eigenTex;  // numeric eigenvalues for large matrices
};

}  // namespace

bool isMatrixProblem(const R& r) {
    if (r->t == T::Func && (r->op == "matrix" || r->op == "identity" || r->op == "tensor")) return true;
    for (auto& k : r->kids) if (isMatrixProblem(k)) return true;
    return false;
}

namespace {

// ============================================================ n-dimensional arrays (2D matrices and 3D stacks)
// 3D literal [[[..],[..]], [[..],[..]]] = k slices of r x c. Element-wise + - * /, scalar multiples,
// shape(), sum() (all entries or along an axis) and slice(). Axes are 1-based: axis 1 runs across slices,
// axis 2 down the rows, axis 3 along each row (for 2D: axis 1 = down the rows, axis 2 = along each row).

struct NdArray {
    std::vector<size_t> dims;  // empty = scalar
    std::vector<E> v;          // row-major
    std::string text;          // for results that are words (shape)
    size_t rank() const { return dims.size(); }
    E& at3(size_t a, size_t b, size_t c) { return v[(a * dims[1] + b) * dims[2] + c]; }
    const E& at3(size_t a, size_t b, size_t c) const { return v[(a * dims[1] + b) * dims[2] + c]; }
    const E& at2(size_t r, size_t c) const { return v[r * dims[1] + c]; }
};

Mat toMat(const NdArray& a, size_t slice = 0) {
    size_t r = a.dims[a.rank() - 2], c = a.dims[a.rank() - 1];
    Mat m(r, std::vector<E>(c));
    size_t off = a.rank() == 3 ? slice * r * c : 0;
    for (size_t i = 0; i < r; ++i) for (size_t j = 0; j < c; ++j) m[i][j] = a.v[off + i * c + j];
    return m;
}

NdArray fromMat(const Mat& m) {
    NdArray a;
    a.dims = {m.size(), m.empty() ? 0 : m[0].size()};
    for (auto& row : m) for (auto& x : row) a.v.push_back(x);
    return a;
}

std::string ndTex(const NdArray& a) {
    if (!a.text.empty()) return a.text;
    if (a.rank() == 0) return toLatex(a.v[0]);
    if (a.rank() == 2) return matTex(toMat(a));
    std::string s = "\\left[\\,";
    for (size_t k = 0; k < a.dims[0]; ++k) s += (k ? ",\\ " : "") + matTex(toMat(a, k));
    return s + "\\,\\right]";
}

std::string shapeTex(const std::vector<size_t>& d) {
    std::string s;
    for (size_t i = 0; i < d.size(); ++i) s += (i ? " \\times " : "") + std::to_string(d[i]);
    return s;
}

bool hasArrayFn(const R& r) {
    if (r->t == T::Func && (r->op == "tensor" || r->op == "shape" || r->op == "slice" || r->op == "sum")) return true;
    for (auto& k : r->kids) if (hasArrayFn(k)) return true;
    return false;
}

class ArrayEval {
public:
    std::vector<Step> steps;
    std::string error;
    std::string title = "3D matrix";

    std::optional<NdArray> eval(const R& r) {
        if (!isMatrixProblem(r)) {
            auto e = toExpr(r);
            if (!e) { error = e.error().message; return std::nullopt; }
            NdArray a; a.v = {simplify(*e)};
            return a;
        }
        switch (r->t) {
            case T::Func: return func(r);
            case T::Neg: {
                auto a = eval(r->kids[0]);
                if (!a) return std::nullopt;
                for (auto& x : a->v) x = simplify(neg(x));
                return a;
            }
            case T::Bin: return bin(r);
            default: error = "Unsupported 3D matrix expression"; return std::nullopt;
        }
    }

private:
    bool small(const NdArray& a) const { return a.v.size() <= 64; }

    std::optional<NdArray> func(const R& r) {
        const std::string& n = r->op;
        if (n == "matrix") {
            NdArray a;
            a.dims = {r->kids.size(), r->kids[0]->kids.size()};
            for (auto& row : r->kids) for (auto& x : row->kids) {
                auto e = toExpr(x);
                if (!e) { error = e.error().message; return std::nullopt; }
                a.v.push_back(simplify(*e));
            }
            return a;
        }
        if (n == "tensor") {
            NdArray a;
            a.dims = {r->kids.size(), r->kids[0]->kids.size(), r->kids[0]->kids[0]->kids.size()};
            for (auto& sl : r->kids) {
                auto m = func(sl);
                if (!m) return std::nullopt;
                a.v.insert(a.v.end(), m->v.begin(), m->v.end());
            }
            return a;
        }
        if (n == "shape") {
            if (r->kids.size() != 1) { error = "shape takes one matrix"; return std::nullopt; }
            auto a = eval(r->kids[0]);
            if (!a) return std::nullopt;
            if (a->rank() == 0) { error = "shape needs a matrix"; return std::nullopt; }
            title = "Shape";
            NdArray out; out.text = shapeTex(a->dims);
            std::string words = a->rank() == 3
                ? std::to_string(a->dims[0]) + " slices, each with " + std::to_string(a->dims[1]) + " rows and " + std::to_string(a->dims[2]) + " columns."
                : std::to_string(a->dims[0]) + " rows and " + std::to_string(a->dims[1]) + " columns.";
            steps.push_back({"Count along each axis", words + " " + std::to_string(a->v.size()) + " entries in total.", out.text, {}});
            return out;
        }
        if (n == "sum") {
            if (r->kids.empty() || r->kids.size() > 2) { error = "Use sum(A) or sum(A, axis)"; return std::nullopt; }
            auto a = eval(r->kids[0]);
            if (!a) return std::nullopt;
            if (a->rank() == 0) { error = "sum needs a matrix"; return std::nullopt; }
            if (r->kids.size() == 1) {
                title = "Sum of all entries";
                E tot = simplify(add(a->v));
                std::string ex;
                if (a->v.size() <= 24) { for (size_t i = 0; i < a->v.size(); ++i) ex += (i ? " + " : "") + par(a->v[i]); ex += " = "; }
                steps.push_back({"Add every entry", "All " + std::to_string(a->v.size()) + " entries, in any order.", ex + toLatex(tot), {}});
                NdArray out; out.v = {tot};
                return out;
            }
            auto ax = axisArg(r->kids[1], a->rank());
            if (!ax) return std::nullopt;
            return sumAxis(*a, *ax);
        }
        if (n == "slice") {
            if (r->kids.size() < 2 || r->kids.size() > 3) { error = "Use slice(A, i) or slice(A, i, axis)"; return std::nullopt; }
            auto a = eval(r->kids[0]);
            if (!a) return std::nullopt;
            if (a->rank() == 0) { error = "slice needs a matrix"; return std::nullopt; }
            size_t axis = 1;
            if (r->kids.size() == 3) { auto ax = axisArg(r->kids[2], a->rank()); if (!ax) return std::nullopt; axis = *ax; }
            auto ie = toExpr(r->kids[1]);
            if (!ie || !isInteger(simplify(*ie))) { error = "The slice index must be a whole number"; return std::nullopt; }
            long long i = simplify(*ie)->num.num().toInt64();
            if (i < 1 || (size_t)i > a->dims[axis - 1]) { error = "Index " + std::to_string(i) + " is outside 1 to " + std::to_string(a->dims[axis - 1]); return std::nullopt; }
            return slice(*a, (size_t)i - 1, axis);
        }
        // matrix functions (det, inv, ...) of a slice etc.: evaluate with the 2D engine
        MatrixEval me2;
        auto v = me2.evalPublic(r);
        if (!v) { error = me2.error; return std::nullopt; }
        steps.insert(steps.end(), me2.steps.begin(), me2.steps.end());
        if (v->isMat) return fromMat(v->m);
        NdArray out; out.v = {v->scalar};
        return out;
    }

    std::optional<size_t> axisArg(const R& r, size_t rank) {
        auto e = toExpr(r);
        if (!e || !isInteger(simplify(*e))) { error = "The axis must be a whole number"; return std::nullopt; }
        long long ax = simplify(*e)->num.num().toInt64();
        if (ax < 1 || (size_t)ax > rank) { error = "The axis must be between 1 and " + std::to_string(rank); return std::nullopt; }
        return (size_t)ax;
    }

    NdArray sumAxis(const NdArray& a, size_t axis) {
        NdArray out;
        std::string how;
        if (a.rank() == 2) {
            size_t R_ = a.dims[0], C_ = a.dims[1];
            if (axis == 1) {  // down the rows: one total per column
                out.dims = {1, C_};
                for (size_t j = 0; j < C_; ++j) { std::vector<E> t; for (size_t i = 0; i < R_; ++i) t.push_back(a.at2(i, j)); out.v.push_back(simplify(add(t))); }
                how = "Add down each column: one total per column.";
            } else {
                out.dims = {R_, 1};
                for (size_t i = 0; i < R_; ++i) { std::vector<E> t; for (size_t j = 0; j < C_; ++j) t.push_back(a.at2(i, j)); out.v.push_back(simplify(add(t))); }
                how = "Add along each row: one total per row.";
            }
        } else {
            size_t K = a.dims[0], R_ = a.dims[1], C_ = a.dims[2];
            if (axis == 1) {
                out.dims = {R_, C_};
                for (size_t i = 0; i < R_; ++i) for (size_t j = 0; j < C_; ++j) { std::vector<E> t; for (size_t k = 0; k < K; ++k) t.push_back(a.at3(k, i, j)); out.v.push_back(simplify(add(t))); }
                how = "Stack the slices and add them entry by entry: the " + std::to_string(K) + " slices collapse into one " + shapeTex({R_, C_}) + " matrix.";
            } else if (axis == 2) {
                out.dims = {K, C_};
                for (size_t k = 0; k < K; ++k) for (size_t j = 0; j < C_; ++j) { std::vector<E> t; for (size_t i = 0; i < R_; ++i) t.push_back(a.at3(k, i, j)); out.v.push_back(simplify(add(t))); }
                how = "In each slice, add down every column. Row " + m("k") + " of the result holds the column totals of slice " + m("k") + ".";
            } else {
                out.dims = {K, R_};
                for (size_t k = 0; k < K; ++k) for (size_t i = 0; i < R_; ++i) { std::vector<E> t; for (size_t j = 0; j < C_; ++j) t.push_back(a.at3(k, i, j)); out.v.push_back(simplify(add(t))); }
                how = "In each slice, add along every row. Row " + m("k") + " of the result holds the row totals of slice " + m("k") + ".";
            }
        }
        title = "Sum along axis " + std::to_string(axis);
        steps.push_back({"Sum along axis " + std::to_string(axis), how, ndTex(out), {}});
        return out;
    }

    NdArray slice(const NdArray& a, size_t i, size_t axis) {
        NdArray out;
        std::string how;
        if (a.rank() == 2) {
            size_t R_ = a.dims[0], C_ = a.dims[1];
            if (axis == 1) { out.dims = {1, C_}; for (size_t j = 0; j < C_; ++j) out.v.push_back(a.at2(i, j)); how = "Row " + std::to_string(i + 1) + "."; }
            else { out.dims = {R_, 1}; for (size_t r = 0; r < R_; ++r) out.v.push_back(a.at2(r, i)); how = "Column " + std::to_string(i + 1) + "."; }
        } else {
            size_t K = a.dims[0], R_ = a.dims[1], C_ = a.dims[2];
            if (axis == 1) { out.dims = {R_, C_}; for (size_t r = 0; r < R_; ++r) for (size_t c = 0; c < C_; ++c) out.v.push_back(a.at3(i, r, c)); how = "Slice " + std::to_string(i + 1) + " of the stack."; }
            else if (axis == 2) { out.dims = {K, C_}; for (size_t k = 0; k < K; ++k) for (size_t c = 0; c < C_; ++c) out.v.push_back(a.at3(k, i, c)); how = "Row " + std::to_string(i + 1) + " from every slice, one per line."; }
            else { out.dims = {K, R_}; for (size_t k = 0; k < K; ++k) for (size_t r = 0; r < R_; ++r) out.v.push_back(a.at3(k, r, i)); how = "Column " + std::to_string(i + 1) + " from every slice, written as a row."; }
        }
        title = "Slice";
        steps.push_back({"Take index " + std::to_string(i + 1) + " along axis " + std::to_string(axis), how, ndTex(out), {}});
        return out;
    }

    std::optional<NdArray> bin(const R& r) {
        const std::string& op = r->op;
        auto a = eval(r->kids[0]);
        if (!a) return std::nullopt;
        auto b = eval(r->kids[1]);
        if (!b) return std::nullopt;
        bool as = a->rank() == 0, bs = b->rank() == 0;
        if (op == "^") {
            if (as || !bs) { error = "Raise a 3D matrix to a number, entry by entry"; return std::nullopt; }
            NdArray out = *a;
            for (auto& x : out.v) x = simplify(pow(x, b->v[0]));
            steps.push_back({"Raise every entry to " + m(toLatex(b->v[0])), "", small(out) ? ndTex(out) : "", {}});
            return out;
        }
        if (as && bs) {
            NdArray out;
            E x = a->v[0], y = b->v[0];
            out.v = {simplify(op == "+" ? add(x, y) : op == "-" ? sub(x, y) : op == "/" || op == "÷" ? divide(x, y) : mul(x, y))};
            return out;
        }
        if (as || bs) {  // number with array: every entry
            if ((op == "+" || op == "-")) { error = "Add or subtract arrays of the same shape (a number can multiply or divide one)"; return std::nullopt; }
            if (as && (op == "/" || op == "÷")) { error = "A number can't be divided by a matrix"; return std::nullopt; }
            const NdArray& arr = as ? *b : *a;
            E k = as ? a->v[0] : b->v[0];
            NdArray out = arr;
            bool div = op == "/" || op == "÷";
            for (auto& x : out.v) x = simplify(div ? divide(x, k) : mul(k, x));
            steps.push_back({(div ? "Divide every entry by " : "Multiply every entry by ") + m(toLatex(k)), "",
                             small(out) ? (div ? ndTex(arr) + " \\div " + par(k) : par(k) + " \\cdot " + ndTex(arr)) + " = " + ndTex(out) : ndTex(out), {}});
            return out;
        }
        if (a->rank() == 2 && b->rank() == 2 && op == "*") {  // ordinary matrix product
            MatrixEval me2;
            auto v = me2.matmulPublic(toMat(*a), toMat(*b));
            if (!v) { error = me2.error; return std::nullopt; }
            steps.insert(steps.end(), me2.steps.begin(), me2.steps.end());
            return fromMat(v->m);
        }
        if (a->dims != b->dims) {
            error = "Shapes differ: " + shapeTex(a->dims) + " and " + shapeTex(b->dims) + ". Entry-by-entry operations need the same shape.";
            return std::nullopt;
        }
        NdArray out = *a;
        const char* verb;
        std::string sym;
        if (op == "+") { verb = "Add entry by entry"; sym = " + "; }
        else if (op == "-") { verb = "Subtract entry by entry"; sym = " - "; }
        else if (op == "*") { verb = "Multiply entry by entry (Hadamard product)"; sym = " \\odot "; }
        else { verb = "Divide entry by entry"; sym = " \\oslash "; }
        for (size_t i = 0; i < out.v.size(); ++i) {
            E x = a->v[i], y = b->v[i];
            out.v[i] = simplify(op == "+" ? add(x, y) : op == "-" ? sub(x, y) : op == "*" ? mul(x, y) : divide(x, y));
        }
        std::string text = a->rank() == 3 ? "Each slice combines with the matching slice; each entry with the entry in the same place." : "";
        steps.push_back({verb, text, small(out) ? ndTex(*a) + sym + ndTex(*b) + " = " + ndTex(out) : ndTex(out), {}});
        return out;
    }
};

Solution solveArray(const R& raw, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "tensor";
    sol.anim = "matrix";
    sol.inputLatex = inputLatex;
    ArrayEval ev;
    auto v = ev.eval(raw);
    if (!v) {
        sol.ok = false;
        sol.error = ev.error.empty() ? "Couldn't evaluate the 3D matrix expression." : ev.error;
        return sol;
    }
    sol.title = ev.title;
    sol.answerLatex = ndTex(*v);
    sol.answers.push_back(sol.answerLatex);
    if (v->rank() > 0 && v->text.empty()) {
        sol.facts.push_back({"Shape", shapeTex(v->dims)});
        sol.facts.push_back({"Entries", std::to_string(v->v.size())});
    }
    if (ev.steps.empty()) {
        std::string d = v->rank() == 3 ? std::to_string(v->dims[0]) + " slices, each " + shapeTex({v->dims[1], v->dims[2]}) + "." : "";
        ev.steps.push_back({"A 3D matrix", "A stack of equal-sized matrices. " + d, sol.answerLatex, {}});
    }
    sol.methods.push_back({"Array operations", ev.steps});
    return sol;
}

}  // namespace

namespace {

// First 2x2 numeric matrix literal in the input: its geometry drives the Explain scene
// (unit square -> parallelogram, area x det, eigenvector lines that stay put).
std::optional<std::array<double, 4>> first2x2(const R& r) {
    if (r->t == T::Func && r->op == "matrix" && r->kids.size() == 2 && r->kids[0]->kids.size() == 2 && r->kids[1]->kids.size() == 2) {
        std::array<double, 4> v{};
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j) {
                auto e = toExpr(r->kids[i]->kids[j]);
                if (!e) return std::nullopt;
                E x = simplify(*e);
                if (!isNum(x)) return std::nullopt;
                v[i * 2 + j] = x->num.toDouble();
            }
        return v;
    }
    for (auto& k : r->kids) if (auto v = first2x2(k)) return v;
    return std::nullopt;
}

void attachMatrixGeometry(Solution& sol, const R& raw) {
    auto m2 = first2x2(raw);
    if (!m2) return;
    auto [a, b, c, d] = *m2;
    double det = a * d - b * c, tr = a + d, disc = tr * tr - 4 * det;
    std::vector<double> p{a, b, c, d, det, 0, 0, 0, 0, 0, 0, 0};
    if (disc >= -1e-12) {
        double sq = std::sqrt(std::max(0.0, disc));
        double ls[2] = {(tr - sq) / 2, (tr + sq) / 2};
        p[5] = 1;
        for (int k = 0; k < 2; ++k) {
            double l = ls[k], vx, vy;
            if (std::fabs(b) > 1e-12) { vx = b; vy = l - a; }
            else if (std::fabs(c) > 1e-12) { vx = l - d; vy = c; }
            else if (std::fabs(l - a) < 1e-12 && !(k == 1 && std::fabs(a - d) < 1e-12)) { vx = 1; vy = 0; }
            else { vx = 0; vy = 1; }
            double n = std::hypot(vx, vy);
            if (n > 0) { vx /= n; vy /= n; }
            p[6 + 3 * k] = l; p[7 + 3 * k] = vx; p[8 + 3 * k] = vy;
        }
    }
    PlotSpec ps;
    ps.shape = PlotShape{"matrix2", p};
    ps.xmin = -3; ps.xmax = 3; ps.ymin = -3; ps.ymax = 3; ps.autoRange = false;
    sol.plot = ps;
}
}  // namespace

Solution solveMatrix(const R& raw, const std::string& inputLatex) {
    if (hasArrayFn(raw)) return solveArray(raw, inputLatex);
    Solution sol;
    sol.kind = "matrix";
    sol.anim = "matrix";
    sol.title = "Matrix calculation";
    sol.inputLatex = inputLatex;
    MatrixEval ev;
    auto v = ev.eval(raw);
    if (!v) {
        sol.ok = false;
        sol.error = ev.error.empty() ? "Couldn't evaluate the matrix expression." : ev.error;
        return sol;
    }
    sol.kind = ev.kind;
    if (ev.kind == "eigenvalues") {
        sol.title = "Find the eigenvalues";
        std::string ans;
        for (size_t i = 0; i < ev.eigenTex.size(); ++i) {
            std::string t = "\\lambda_{" + std::to_string(i + 1) + "} \\approx " + ev.eigenTex[i];
            sol.answers.push_back(t);
            ans += (i ? ", \\quad " : "") + t;
        }
        for (size_t i = 0; i < ev.eigen.size(); ++i) {
            std::string v = toLatex(ev.eigen[i]);
            bool approx = v.find('.') != std::string::npos;  // numeric root of the characteristic polynomial
            std::string t = "\\lambda" + (ev.eigen.size() > 1 ? "_{" + std::to_string(i + 1) + "}" : std::string("")) + (approx ? " \\approx " : " = ") + v;
            sol.answers.push_back(t);
            ans += (i ? ", \\quad " : "") + t;
        }
        sol.answerLatex = ans;
    } else if (v->isMat) {
        sol.answerLatex = matTex(v->m);
        sol.answers.push_back(sol.answerLatex);
        sol.facts.push_back({"Size", dims(v->m)});
        if (v->m.size() == v->m[0].size() && v->m.size() <= 4) {
            E d = MatrixEval().det(v->m, nullptr);
            sol.facts.push_back({"Determinant", toLatex(d)});
        }
    } else {
        if (ev.kind == "determinant") sol.title = "Find the determinant";
        sol.answerLatex = toLatex(v->scalar);
        sol.answers.push_back(sol.answerLatex);
    }
    if (ev.steps.empty()) ev.steps.push_back({"Result", "", sol.answerLatex, {}});
    sol.methods.push_back({"Matrix operations", ev.steps});
    attachMatrixGeometry(sol, raw);
    return sol;
}

}  // namespace me
