// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Exact rational number over BigInt.
#pragma once
#include <string>

#include "core/bigint.h"

namespace me {

class Rational {
public:
    Rational() : n_(0), d_(1) {}
    Rational(long long n) : n_(n), d_(1) {}
    Rational(const BigInt& n) : n_(n), d_(1) {}
    Rational(const BigInt& n, const BigInt& d) : n_(n), d_(d) { normalize(); }
    static Rational fromDecimal(const std::string& s);  // "2.5" -> 5/2

    const BigInt& num() const { return n_; }
    const BigInt& den() const { return d_; }
    // A zero denominator marks an invalid value (e.g. x/0); it propagates through arithmetic.
    bool isValid() const { return !d_.isZero(); }
    bool isZero() const { return n_.isZero() && isValid(); }
    bool isOne() const { return n_.isOne() && d_.isOne(); }
    bool isMinusOne() const { return d_.isOne() && n_ == BigInt(-1); }
    bool isInteger() const { return d_.isOne(); }
    bool isNeg() const { return n_.isNeg(); }
    int sign() const { return n_.sign(); }
    double toDouble() const;
    std::string toString() const;  // "a" or "a/b"
    std::string toDecimalString(int maxDigits = 10) const;

    Rational abs() const { return Rational(n_.abs(), d_); }
    Rational operator-() const { Rational r = *this; r.n_ = -r.n_; return r; }
    Rational inv() const { return Rational(d_, n_); }
    static Rational invalid() { Rational r; r.d_ = BigInt(0); return r; }

    friend Rational operator+(const Rational& a, const Rational& b);
    friend Rational operator-(const Rational& a, const Rational& b);
    friend Rational operator*(const Rational& a, const Rational& b);
    friend Rational operator/(const Rational& a, const Rational& b);
    Rational& operator+=(const Rational& b) { return *this = *this + b; }
    Rational& operator-=(const Rational& b) { return *this = *this - b; }
    Rational& operator*=(const Rational& b) { return *this = *this * b; }
    Rational& operator/=(const Rational& b) { return *this = *this / b; }
    friend int cmp(const Rational& a, const Rational& b);
    friend bool operator==(const Rational& a, const Rational& b) { return a.n_ == b.n_ && a.d_ == b.d_; }
    friend std::strong_ordering operator<=>(const Rational& a, const Rational& b) { return cmp(a, b) <=> 0; }

    // integer power (negative allowed)
    Rational pow(long long e) const;
    BigInt floor() const;

private:
    BigInt n_, d_;
    void normalize();
};

inline Rational rgcd(const Rational& a, const Rational& b) {
    // gcd of rationals: gcd(numerators)/lcm(denominators)
    BigInt g = BigInt::gcd(a.num(), b.num());
    BigInt l = a.den() / BigInt::gcd(a.den(), b.den()) * b.den();
    if (g.isZero()) return Rational(0);
    return Rational(g, l);
}

}  // namespace me
