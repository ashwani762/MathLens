// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "core/poly.h"

#include <algorithm>
#include <cmath>

namespace me {

void Poly::trim() {
    while (!c_.empty() && c_.back().isZero()) c_.pop_back();
}

std::optional<Poly> Poly::from(const E& e0, const std::string& var) {
    E e = expand(e0);
    if (polyDegree(e, var) < 0) return std::nullopt;
    std::vector<E> terms = e->kind == Kind::Add ? e->args : std::vector<E>{e};
    std::vector<Rational> c;
    for (auto& t : terms) {
        Rational k(1);
        int d = 0;
        std::vector<E> fs = t->kind == Kind::Mul ? t->args : std::vector<E>{t};
        for (auto& f : fs) {
            if (f->kind == Kind::Num) k *= f->num;
            else if (isSym(f, var)) d += 1;
            else if (f->kind == Kind::Pow && isSym(f->args[0], var) && isInteger(f->args[1])) d += (int)f->args[1]->num.num().toInt64();
            else return std::nullopt;  // non-rational coefficient
        }
        if ((int)c.size() <= d) c.resize(d + 1);
        c[d] += k;
    }
    return Poly(c);
}

E Poly::toExpr(const std::string& var) const {
    std::vector<E> terms;
    for (int k = deg(); k >= 0; --k) {
        if (c_[k].isZero()) continue;
        E xk = k == 0 ? num(1) : (k == 1 ? sym(var) : pow(sym(var), num(k)));
        terms.push_back(k == 0 ? num(c_[k]) : (c_[k].isOne() ? xk : mul(num(c_[k]), xk)));
    }
    return simplify(add(terms));
}

Poly Poly::operator+(const Poly& o) const {
    std::vector<Rational> r(std::max(c_.size(), o.c_.size()));
    for (size_t i = 0; i < r.size(); ++i) r[i] = coeff((int)i) + o.coeff((int)i);
    return Poly(r);
}
Poly Poly::operator-(const Poly& o) const { return *this + o.scale(Rational(-1)); }
Poly Poly::operator*(const Poly& o) const {
    if (isZero() || o.isZero()) return Poly();
    std::vector<Rational> r(c_.size() + o.c_.size() - 1);
    for (size_t i = 0; i < c_.size(); ++i)
        for (size_t j = 0; j < o.c_.size(); ++j) r[i + j] += c_[i] * o.c_[j];
    return Poly(r);
}
Poly Poly::scale(const Rational& s) const {
    std::vector<Rational> r = c_;
    for (auto& x : r) x *= s;
    return Poly(r);
}

void Poly::divmod(const Poly& a, const Poly& b, Poly& q, Poly& r) {
    r = a;
    if (b.isZero()) { q = Poly(); return; }
    std::vector<Rational> qc(std::max(0, a.deg() - b.deg() + 1));
    while (!r.isZero() && r.deg() >= b.deg()) {
        int k = r.deg() - b.deg();
        Rational f = r.lead() / b.lead();
        qc[k] = f;
        r = r - Poly::monomial(f, k) * b;
    }
    q = Poly(qc);
}

Poly Poly::gcd(Poly a, Poly b) {
    while (!b.isZero()) {
        Poly q, r;
        divmod(a, b, q, r);
        a = b;
        b = r;
    }
    return a.isZero() ? a : a.monic();
}

Poly Poly::derivative() const {
    if (c_.size() <= 1) return Poly();
    std::vector<Rational> r(c_.size() - 1);
    for (size_t i = 1; i < c_.size(); ++i) r[i - 1] = c_[i] * Rational((long long)i);
    return Poly(r);
}

Poly Poly::monic() const { return isZero() ? *this : scale(lead().inv()); }

Rational Poly::eval(const Rational& x) const {
    Rational r(0);
    for (int i = deg(); i >= 0; --i) r = r * x + c_[i];
    return r;
}

Cx Poly::evalC(Cx x) const {
    Cx r = 0;
    for (int i = deg(); i >= 0; --i) r = r * x + c_[i].toDouble();
    return r;
}

std::pair<Rational, Poly> Poly::primitive() const {
    if (isZero()) return {Rational(0), Poly()};
    // lcm of denominators
    BigInt l(1);
    for (auto& x : c_) l = l / BigInt::gcd(l, x.den()) * x.den();
    BigInt g(0);
    for (auto& x : c_) g = BigInt::gcd(g, (x * Rational(l)).num());
    Rational content(g, l);
    if (lead().isNeg()) content = -content;
    return {content, scale(content.inv())};
}

std::vector<BigInt> divisors(const BigInt& n0, size_t limit) {
    BigInt n = n0.abs();
    std::vector<BigInt> ds;
    if (n.isZero()) return ds;
    // factor by trial division
    std::vector<std::pair<BigInt, int>> f;
    for (long long p = 2; p < 1000000; ++p) {
        BigInt P(p);
        if (P * P > n) break;
        int cnt = 0;
        while ((n % P).isZero()) { n = n / P; ++cnt; }
        if (cnt) f.push_back({P, cnt});
    }
    if (!n.isOne()) f.push_back({n, 1});
    ds.push_back(BigInt(1));
    for (auto& [p, cnt] : f) {
        size_t sz = ds.size();
        BigInt pk(1);
        for (int k = 1; k <= cnt; ++k) {
            pk *= p;
            for (size_t i = 0; i < sz; ++i) {
                ds.push_back(ds[i] * pk);
                if (ds.size() > limit) return ds;
            }
        }
    }
    std::ranges::sort(ds);
    return ds;
}

std::vector<Rational> Poly::rationalRoots() const {
    std::vector<Rational> roots;
    if (deg() < 1) return roots;
    auto [cont, p] = primitive();
    // zero root
    int shift = 0;
    while (shift <= p.deg() && p.coeff(shift).isZero()) ++shift;
    if (shift > 0) roots.push_back(Rational(0));
    BigInt a0 = p.coeff(shift).num(), an = p.lead().num();
    if (p.deg() - shift < 1) return roots;
    if (a0.digits() > 15 || an.digits() > 15) return roots;
    auto dp = divisors(a0), dq = divisors(an);
    for (auto& q : dq)
        for (auto& pp : dp)
            for (int s : {1, -1}) {
                Rational cand(pp * BigInt(s), q);
                if (std::ranges::find(roots, cand) != roots.end()) continue;
                if (p.eval(cand).isZero()) roots.push_back(cand);
            }
    std::ranges::sort(roots);
    return roots;
}

std::vector<Cx> Poly::numericRoots() const {
    int n = deg();
    std::vector<Cx> z;
    if (n < 1) return z;
    std::vector<Cx> a(n + 1);
    for (int i = 0; i <= n; ++i) a[i] = c_[i].toDouble() / lead().toDouble();
    auto f = [&](Cx x) { Cx r = 0; for (int i = n; i >= 0; --i) r = r * x + a[i]; return r; };
    // Aberth / Durand-Kerner initial guesses on a circle
    double R = 0;
    for (int i = 0; i < n; ++i) R = std::max(R, std::abs(a[i]));
    R = 1 + R;
    for (int k = 0; k < n; ++k) z.push_back(std::polar(0.5 * R, 2 * M_PI * k / n + 0.4));
    for (int it = 0; it < 800; ++it) {
        double delta = 0;
        for (int i = 0; i < n; ++i) {
            Cx den = 1;
            for (int j = 0; j < n; ++j) if (j != i) den *= (z[i] - z[j]);
            if (std::abs(den) < 1e-300) den = 1e-12;
            Cx dz = f(z[i]) / den;
            z[i] -= dz;
            delta = std::max(delta, std::abs(dz));
        }
        if (delta < 1e-15) break;
    }
    // Newton polish on the exact coefficients
    Poly d = derivative();
    for (auto& r : z) {
        for (int k = 0; k < 5; ++k) {
            Cx fd = d.evalC(r);
            if (std::abs(fd) < 1e-300) break;
            r -= evalC(r) / fd;
        }
        if (std::fabs(r.imag()) < 1e-9 * std::max(1.0, std::fabs(r.real()))) r = Cx(r.real(), 0);
        if (std::fabs(r.real()) < 1e-12) r = Cx(0, r.imag());
    }
    std::ranges::sort(z, [](Cx a, Cx b) {
        if (std::fabs(a.imag()) < 1e-12 && std::fabs(b.imag()) >= 1e-12) return true;
        if (std::fabs(b.imag()) < 1e-12 && std::fabs(a.imag()) >= 1e-12) return false;
        if (a.real() != b.real()) return a.real() < b.real();
        return a.imag() < b.imag();
    });
    return z;
}

Factorization factorQ(const Poly& p0) {
    Factorization out;
    auto [cont, p] = p0.primitive();
    out.content = cont;
    if (p.deg() < 1) { out.content = p0.lead(); return out; }
    Poly rest = p;
    for (auto& r : p.rationalRoots()) {
        // factor (q x - p) with integer coefficients
        Poly lin = Poly::linear(Rational(r.den()), Rational(-r.num()));
        int m = 0;
        while (rest.deg() >= 1) {
            Poly q, rem;
            Poly::divmod(rest, lin, q, rem);
            if (!rem.isZero()) break;
            rest = q;
            ++m;
        }
        if (m) out.factors.push_back({lin, m});
    }
    // what's left has no rational roots
    auto [c2, prim] = rest.primitive();
    out.content *= c2;
    if (prim.deg() >= 1) {
        // try to split a quartic into two quadratics with repeated factor via gcd with derivative
        Poly g = Poly::gcd(prim, prim.derivative());
        if (g.deg() >= 1 && prim.deg() >= 4) {
            auto [cg, gp] = g.primitive();
            Poly q, r;
            Poly::divmod(prim, gp, q, r);
            Poly q2, r2;
            Poly::divmod(q, gp, q2, r2);
            if (r.isZero() && r2.isZero() && q2.deg() == 0) {
                out.factors.push_back({gp, 2});
                out.content *= q2.lead();
                return out;
            }
        }
        out.factors.push_back({prim, 1});
        if (prim.deg() >= 3) out.complete = false;
        if (prim.deg() == 2) {
            // irreducible over Q by construction (no rational roots)
        }
    }
    return out;
}

E factorizationToExpr(const Factorization& f, const std::string& var) {
    std::vector<E> fs;
    if (!f.content.isOne()) fs.push_back(num(f.content));
    for (auto& pf : f.factors) {
        E b = pf.p.toExpr(var);
        fs.push_back(pf.mult == 1 ? b : pow(b, num(pf.mult)));
    }
    if (fs.empty()) return num(f.content);
    return mul(fs);  // intentionally not simplified to keep factored display
}

std::optional<std::pair<Poly, Poly>> toRationalFunction(const E& e, const std::string& var) {
    using PP = std::pair<Poly, Poly>;
    auto reduce = [](PP pp) -> PP {
        Poly g = Poly::gcd(pp.first, pp.second);
        if (g.deg() >= 1) {
            Poly q1, r1, q2, r2;
            Poly::divmod(pp.first, g, q1, r1);
            Poly::divmod(pp.second, g, q2, r2);
            pp = {q1, q2};
        }
        // normalize denominator leading coefficient to 1
        Rational l = pp.second.lead();
        return {pp.first.scale(l.inv()), pp.second.scale(l.inv())};
    };
    switch (e->kind) {
        case Kind::Num: return PP{Poly::constant(e->num), Poly::constant(Rational(1))};
        case Kind::Sym:
            if (e->name == var) return PP{Poly::linear(Rational(1), Rational(0)), Poly::constant(Rational(1))};
            return std::nullopt;
        case Kind::Add: {
            PP acc{Poly(), Poly::constant(Rational(1))};
            for (auto& a : e->args) {
                auto t = toRationalFunction(a, var);
                if (!t) return std::nullopt;
                acc = reduce({acc.first * t->second + t->first * acc.second, acc.second * t->second});
            }
            return acc;
        }
        case Kind::Mul: {
            PP acc{Poly::constant(Rational(1)), Poly::constant(Rational(1))};
            for (auto& a : e->args) {
                auto t = toRationalFunction(a, var);
                if (!t) return std::nullopt;
                acc = reduce({acc.first * t->first, acc.second * t->second});
            }
            return acc;
        }
        case Kind::Pow: {
            if (!isInteger(e->args[1])) return std::nullopt;
            long long n = e->args[1]->num.num().toInt64();
            if (std::llabs(n) > 64) return std::nullopt;
            auto b = toRationalFunction(e->args[0], var);
            if (!b) return std::nullopt;
            PP r{Poly::constant(Rational(1)), Poly::constant(Rational(1))};
            for (long long k = 0; k < std::llabs(n); ++k) r = {r.first * b->first, r.second * b->second};
            if (n < 0) {
                if (r.first.isZero()) return std::nullopt;
                std::swap(r.first, r.second);
            }
            return reduce(r);
        }
        case Kind::Func: return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::vector<Rational>> solveLinearSystem(std::vector<std::vector<Rational>> A, std::vector<Rational> b) {
    size_t n = A.size();
    if (n == 0) return std::vector<Rational>{};
    size_t m = A[0].size();
    for (size_t i = 0; i < n; ++i) A[i].push_back(b[i]);
    size_t row = 0;
    std::vector<int> pivCol(n, -1);
    for (size_t col = 0; col < m && row < n; ++col) {
        size_t piv = row;
        while (piv < n && A[piv][col].isZero()) ++piv;
        if (piv == n) continue;
        std::swap(A[piv], A[row]);
        Rational inv = A[row][col].inv();
        for (auto& x : A[row]) x *= inv;
        for (size_t r = 0; r < n; ++r) {
            if (r == row || A[r][col].isZero()) continue;
            Rational f = A[r][col];
            for (size_t c = col; c <= m; ++c) A[r][c] -= f * A[row][c];
        }
        pivCol[row] = (int)col;
        ++row;
    }
    // inconsistent or underdetermined?
    for (size_t r = row; r < n; ++r) if (!A[r][m].isZero()) return std::nullopt;
    if (row < m) return std::nullopt;
    std::vector<Rational> x(m);
    for (size_t r = 0; r < row; ++r) x[pivCol[r]] = A[r][m];
    return x;
}

std::vector<E> quadraticRoots(const Rational& a, const Rational& b, const Rational& c) {
    Rational D = b * b - Rational(4) * a * c;
    E sq = simplify(sqrtE(num(D)));
    E r1 = simplify(divide(add(num(-b), sq), num(Rational(2) * a)));
    E r2 = simplify(divide(sub(num(-b), sq), num(Rational(2) * a)));
    return {r1, r2};
}

}  // namespace me
