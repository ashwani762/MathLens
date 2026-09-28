// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Arbitrary precision signed integer (base 1e9). Small, dependency free.
#pragma once
#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace me {

class BigInt {
public:
    BigInt() = default;
    BigInt(long long v);
    static BigInt fromString(const std::string& s);

    std::string toString() const;
    double toDouble() const;
    bool fitsInt64() const;
    long long toInt64() const;

    bool isZero() const { return d_.empty(); }
    bool isNeg() const { return neg_; }
    int sign() const { return d_.empty() ? 0 : (neg_ ? -1 : 1); }
    bool isOne() const { return !neg_ && d_.size() == 1 && d_[0] == 1; }
    bool isEven() const { return d_.empty() || (d_[0] % 2 == 0); }
    size_t digits() const { return toString().size(); }

    BigInt abs() const { BigInt r = *this; r.neg_ = false; return r; }
    BigInt operator-() const { BigInt r = *this; if (!r.isZero()) r.neg_ = !r.neg_; return r; }

    friend BigInt operator+(const BigInt& a, const BigInt& b);
    friend BigInt operator-(const BigInt& a, const BigInt& b);
    friend BigInt operator*(const BigInt& a, const BigInt& b);
    friend BigInt operator/(const BigInt& a, const BigInt& b);  // truncating
    friend BigInt operator%(const BigInt& a, const BigInt& b);
    BigInt& operator+=(const BigInt& b) { return *this = *this + b; }
    BigInt& operator-=(const BigInt& b) { return *this = *this - b; }
    BigInt& operator*=(const BigInt& b) { return *this = *this * b; }

    friend int cmp(const BigInt& a, const BigInt& b);
    friend bool operator==(const BigInt& a, const BigInt& b) { return cmp(a, b) == 0; }
    friend std::strong_ordering operator<=>(const BigInt& a, const BigInt& b) { return cmp(a, b) <=> 0; }

    // Division by zero yields q = r = 0 (callers must guard; the engine is built without exceptions).
    static void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r);
    static BigInt gcd(BigInt a, BigInt b);
    static BigInt pow(BigInt base, unsigned long e);
    // floor(a^(1/n)) for a >= 0
    static BigInt iroot(const BigInt& a, unsigned n);

private:
    static constexpr uint32_t BASE = 1000000000u;
    bool neg_ = false;
    std::vector<uint32_t> d_;  // little endian limbs

    void trim();
    static int cmpAbs(const BigInt& a, const BigInt& b);
    static BigInt addAbs(const BigInt& a, const BigInt& b);
    static BigInt subAbs(const BigInt& a, const BigInt& b);  // |a| >= |b|
    static BigInt mulSmall(const BigInt& a, uint32_t m);
    static uint32_t divSmall(BigInt& a, uint32_t m);
};

}  // namespace me
