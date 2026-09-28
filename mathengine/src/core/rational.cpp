// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "core/rational.h"

#include <algorithm>

namespace me {

void Rational::normalize() {
    if (d_.isZero()) [[unlikely]] { n_ = BigInt(0); return; }  // invalid
    if (d_.isNeg()) { n_ = -n_; d_ = -d_; }
    BigInt g = BigInt::gcd(n_, d_);
    if (!g.isZero() && !g.isOne()) { n_ = n_ / g; d_ = d_ / g; }
    if (n_.isZero()) d_ = BigInt(1);
}

Rational Rational::fromDecimal(const std::string& s) {
    auto dot = s.find('.');
    if (dot == std::string::npos) return Rational(BigInt::fromString(s));
    std::string ip = s.substr(0, dot), fp = s.substr(dot + 1);
    BigInt n = BigInt::fromString(ip + fp);
    BigInt d = BigInt::pow(BigInt(10), (unsigned long)fp.size());
    return Rational(n, d);
}

double Rational::toDouble() const {
    if (!isValid()) return 0.0 / 0.0;
    // guard against overflow for huge values
    if (n_.digits() > 300 || d_.digits() > 300) {
        size_t shift = std::max(n_.digits(), d_.digits()) - 250;
        BigInt p = BigInt::pow(BigInt(10), (unsigned long)shift);
        return (n_ / p).toDouble() / (d_ / p).toDouble();
    }
    return n_.toDouble() / d_.toDouble();
}

std::string Rational::toString() const {
    if (!isValid()) return "undefined";
    if (d_.isOne()) return n_.toString();
    return n_.toString() + "/" + d_.toString();
}

std::string Rational::toDecimalString(int maxDigits) const {
    if (!isValid()) return "undefined";
    BigInt q, r;
    BigInt::divmod(n_.abs(), d_, q, r);
    std::string s = (n_.isNeg() ? "-" : "") + q.toString();
    if (r.isZero()) return s;
    s += ".";
    for (int i = 0; i < maxDigits && !r.isZero(); ++i) {
        r = r * BigInt(10);
        BigInt digit;
        BigInt::divmod(r, d_, digit, r);
        s += digit.toString();
    }
    return s;
}

Rational operator+(const Rational& a, const Rational& b) {
    return Rational(a.n_ * b.d_ + b.n_ * a.d_, a.d_ * b.d_);
}
Rational operator-(const Rational& a, const Rational& b) {
    return Rational(a.n_ * b.d_ - b.n_ * a.d_, a.d_ * b.d_);
}
Rational operator*(const Rational& a, const Rational& b) {
    return Rational(a.n_ * b.n_, a.d_ * b.d_);
}
Rational operator/(const Rational& a, const Rational& b) {
    return Rational(a.n_ * b.d_, a.d_ * b.n_);
}
int cmp(const Rational& a, const Rational& b) { return cmp(a.n_ * b.d_, b.n_ * a.d_); }

Rational Rational::pow(long long e) const {
    if (e == 0) return Rational(1);
    if (e < 0) return inv().pow(-e);
    return Rational(BigInt::pow(n_, (unsigned long)e), BigInt::pow(d_, (unsigned long)e));
}

BigInt Rational::floor() const {
    BigInt q, r;
    BigInt::divmod(n_, d_, q, r);
    if (r.isNeg()) q -= BigInt(1);
    return q;
}

}  // namespace me
