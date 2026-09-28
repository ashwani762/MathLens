// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Univariate polynomials with exact rational coefficients.
#pragma once
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/expr.h"

namespace me {

class Poly {
public:
    Poly() = default;
    explicit Poly(std::vector<Rational> coeffs) : c_(std::move(coeffs)) { trim(); }
    static Poly constant(const Rational& r) { return Poly({r}); }
    static Poly monomial(const Rational& a, int k) { std::vector<Rational> c(k + 1); c[k] = a; return Poly(c); }
    static Poly linear(const Rational& a, const Rational& b) { return Poly({b, a}); }  // a x + b

    // Extract polynomial from expression (expands). nullopt if not a polynomial with rational coefficients.
    static std::optional<Poly> from(const E& e, const std::string& var);

    int deg() const { return (int)c_.size() - 1; }  // -1 for zero polynomial
    bool isZero() const { return c_.empty(); }
    Rational lead() const { return c_.empty() ? Rational(0) : c_.back(); }
    Rational coeff(int k) const { return (k >= 0 && k < (int)c_.size()) ? c_[k] : Rational(0); }
    std::span<const Rational> coeffs() const { return c_; }

    E toExpr(const std::string& var) const;

    Poly operator+(const Poly& o) const;
    Poly operator-(const Poly& o) const;
    Poly operator*(const Poly& o) const;
    Poly scale(const Rational& r) const;
    bool operator==(const Poly& o) const { return c_ == o.c_; }
    static void divmod(const Poly& a, const Poly& b, Poly& q, Poly& r);
    static Poly gcd(Poly a, Poly b);  // monic gcd

    Poly derivative() const;
    Poly monic() const;
    Rational eval(const Rational& x) const;
    Cx evalC(Cx x) const;

    // content c and primitive integer polynomial p such that this = c * p, lead(p) > 0
    std::pair<Rational, Poly> primitive() const;

    std::vector<Rational> rationalRoots() const;  // distinct
    std::vector<Cx> numericRoots() const;         // all roots with multiplicity

private:
    std::vector<Rational> c_;
    void trim();
};

struct PolyFactor {
    Poly p;    // primitive, positive leading coefficient
    int mult;
};

struct Factorization {
    Rational content;
    std::vector<PolyFactor> factors;  // linear factors first, then irreducible remainder(s)
    bool complete = true;             // false if a degree >= 3 factor could not be split
};

Factorization factorQ(const Poly& p);
E factorizationToExpr(const Factorization& f, const std::string& var);

// numerator / denominator polynomials for a rational function (nullopt otherwise)
std::optional<std::pair<Poly, Poly>> toRationalFunction(const E& e, const std::string& var);

// Solve A x = b over the rationals (Gauss-Jordan). nullopt if singular.
std::optional<std::vector<Rational>> solveLinearSystem(std::vector<std::vector<Rational>> A, std::vector<Rational> b);

// exact expression for a real root of a quadratic with rational coefficients, both roots
std::vector<E> quadraticRoots(const Rational& a, const Rational& b, const Rational& c);

std::vector<BigInt> divisors(const BigInt& n, size_t limit = 20000);

}  // namespace me
