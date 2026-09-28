// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "core/bigint.h"

#include <algorithm>
#include <cmath>

namespace me {

BigInt::BigInt(long long v) {
    unsigned long long u;
    if (v < 0) { neg_ = true; u = 0ull - (unsigned long long)v; } else u = (unsigned long long)v;
    while (u) { d_.push_back((uint32_t)(u % BASE)); u /= BASE; }
    trim();
}

BigInt BigInt::fromString(const std::string& s) {
    BigInt r;
    size_t i = 0;
    bool n = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) { n = s[i] == '-'; ++i; }
    std::string digits;
    for (; i < s.size(); ++i) if (s[i] >= '0' && s[i] <= '9') digits += s[i];
    for (int end = (int)digits.size(); end > 0; end -= 9) {
        int start = std::max(0, end - 9);
        uint32_t limb = 0;
        for (int k = start; k < end; ++k) limb = limb * 10 + (uint32_t)(digits[k] - '0');
        r.d_.push_back(limb);
    }
    r.neg_ = n;
    r.trim();
    return r;
}

void BigInt::trim() {
    while (!d_.empty() && d_.back() == 0) d_.pop_back();
    if (d_.empty()) neg_ = false;
}

std::string BigInt::toString() const {
    if (d_.empty()) return "0";
    std::string s = std::to_string(d_.back());
    char buf[16];
    for (int i = (int)d_.size() - 2; i >= 0; --i) {
        snprintf(buf, sizeof buf, "%09u", d_[i]);
        s += buf;
    }
    return neg_ ? "-" + s : s;
}

double BigInt::toDouble() const {
    double r = 0;
    for (int i = (int)d_.size() - 1; i >= 0; --i) r = r * BASE + d_[i];
    return neg_ ? -r : r;
}

bool BigInt::fitsInt64() const {
    if (d_.size() <= 2) return true;
    if (d_.size() > 3) return false;
    return cmpAbs(*this, BigInt(9000000000000000000LL)) <= 0;
}

long long BigInt::toInt64() const {
    long long r = 0;
    for (int i = (int)d_.size() - 1; i >= 0; --i) r = r * BASE + d_[i];
    return neg_ ? -r : r;
}

int BigInt::cmpAbs(const BigInt& a, const BigInt& b) {
    if (a.d_.size() != b.d_.size()) return a.d_.size() < b.d_.size() ? -1 : 1;
    for (int i = (int)a.d_.size() - 1; i >= 0; --i)
        if (a.d_[i] != b.d_[i]) return a.d_[i] < b.d_[i] ? -1 : 1;
    return 0;
}

int cmp(const BigInt& a, const BigInt& b) {
    if (a.neg_ != b.neg_) return a.neg_ ? -1 : 1;
    int c = BigInt::cmpAbs(a, b);
    return a.neg_ ? -c : c;
}

BigInt BigInt::addAbs(const BigInt& a, const BigInt& b) {
    BigInt r;
    uint64_t carry = 0;
    size_t n = std::max(a.d_.size(), b.d_.size());
    for (size_t i = 0; i < n || carry; ++i) {
        uint64_t s = carry;
        if (i < a.d_.size()) s += a.d_[i];
        if (i < b.d_.size()) s += b.d_[i];
        r.d_.push_back((uint32_t)(s % BASE));
        carry = s / BASE;
    }
    r.trim();
    return r;
}

BigInt BigInt::subAbs(const BigInt& a, const BigInt& b) {
    BigInt r;
    int64_t borrow = 0;
    for (size_t i = 0; i < a.d_.size(); ++i) {
        int64_t s = (int64_t)a.d_[i] - borrow - (i < b.d_.size() ? b.d_[i] : 0);
        borrow = 0;
        if (s < 0) { s += BASE; borrow = 1; }
        r.d_.push_back((uint32_t)s);
    }
    r.trim();
    return r;
}

BigInt operator+(const BigInt& a, const BigInt& b) {
    if (a.neg_ == b.neg_) { BigInt r = BigInt::addAbs(a, b); r.neg_ = a.neg_; r.trim(); return r; }
    int c = BigInt::cmpAbs(a, b);
    if (c == 0) return BigInt();
    if (c > 0) { BigInt r = BigInt::subAbs(a, b); r.neg_ = a.neg_; r.trim(); return r; }
    BigInt r = BigInt::subAbs(b, a); r.neg_ = b.neg_; r.trim(); return r;
}

BigInt operator-(const BigInt& a, const BigInt& b) { return a + (-b); }

BigInt operator*(const BigInt& a, const BigInt& b) {
    if (a.isZero() || b.isZero()) return BigInt();
    std::vector<uint64_t> t(a.d_.size() + b.d_.size() + 1, 0);
    for (size_t i = 0; i < a.d_.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.d_.size() || carry; ++j) {
            uint64_t cur = t[i + j] + carry + (j < b.d_.size() ? (uint64_t)a.d_[i] * b.d_[j] : 0);
            t[i + j] = cur % BigInt::BASE;
            carry = cur / BigInt::BASE;
        }
    }
    BigInt r;
    r.d_.assign(t.begin(), t.end());
    r.neg_ = a.neg_ != b.neg_;
    r.trim();
    return r;
}

BigInt BigInt::mulSmall(const BigInt& a, uint32_t m) {
    BigInt r;
    uint64_t carry = 0;
    for (size_t i = 0; i < a.d_.size() || carry; ++i) {
        uint64_t cur = carry + (i < a.d_.size() ? (uint64_t)a.d_[i] * m : 0);
        r.d_.push_back((uint32_t)(cur % BASE));
        carry = cur / BASE;
    }
    r.trim();
    return r;
}

uint32_t BigInt::divSmall(BigInt& a, uint32_t m) {
    uint64_t rem = 0;
    for (int i = (int)a.d_.size() - 1; i >= 0; --i) {
        uint64_t cur = a.d_[i] + rem * BASE;
        a.d_[i] = (uint32_t)(cur / m);
        rem = cur % m;
    }
    a.trim();
    return (uint32_t)rem;
}

void BigInt::divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
    if (b.isZero()) [[unlikely]] { q = BigInt(); r = BigInt(); return; }
    BigInt A = a.abs(), B = b.abs();
    if (cmpAbs(A, B) < 0) { q = BigInt(); r = a; return; }
    if (B.d_.size() == 1) {
        q = A;
        uint32_t rem = divSmall(q, B.d_[0]);
        r = BigInt((long long)rem);
    } else {
        q = BigInt();
        q.d_.assign(A.d_.size(), 0);
        BigInt cur;
        for (int i = (int)A.d_.size() - 1; i >= 0; --i) {
            cur.d_.insert(cur.d_.begin(), A.d_[i]);
            cur.trim();
            // estimate the quotient digit from the leading limbs (long double), then correct by +-1
            uint32_t lo = 0;
            if (cmpAbs(cur, B) >= 0) {
                auto lead = [](const BigInt& x, size_t upto) {  // value of the top limbs, scaled to x's size
                    long double v = 0;
                    for (size_t k = 0; k < 3 && k < x.d_.size(); ++k) v = v * BASE + x.d_[x.d_.size() - 1 - k];
                    size_t used = std::min<size_t>(3, x.d_.size());
                    return std::pair<long double, long>(v, (long)x.d_.size() - (long)used - (long)upto);
                };
                auto [cv, ce] = lead(cur, 0);
                auto [bv, be] = lead(B, 0);
                long double est = cv / bv * std::pow((long double)BASE, (long double)(ce - be));
                long long qd = (long long)est;
                if (qd < 0) qd = 0;
                if (qd > (long long)BASE - 1) qd = BASE - 1;
                lo = (uint32_t)qd;
                while (lo > 0 && cmpAbs(mulSmall(B, lo), cur) > 0) --lo;
                while (lo < BASE - 1 && cmpAbs(mulSmall(B, lo + 1), cur) <= 0) ++lo;
            }
            q.d_[i] = lo;
            if (lo) cur = subAbs(cur, mulSmall(B, lo));
        }
        q.trim();
        r = cur;
    }
    q.neg_ = (a.neg_ != b.neg_);
    q.trim();
    r.neg_ = a.neg_;
    r.trim();
}

BigInt operator/(const BigInt& a, const BigInt& b) { BigInt q, r; BigInt::divmod(a, b, q, r); return q; }
BigInt operator%(const BigInt& a, const BigInt& b) { BigInt q, r; BigInt::divmod(a, b, q, r); return r; }

BigInt BigInt::gcd(BigInt a, BigInt b) {
    a = a.abs(); b = b.abs();
    while (!b.isZero()) { BigInt t = a % b; a = b; b = t; }
    return a;
}

BigInt BigInt::pow(BigInt base, unsigned long e) {
    BigInt r(1);
    while (e) {
        if (e & 1) r *= base;
        e >>= 1;
        if (e) base *= base;
    }
    return r;
}

BigInt BigInt::iroot(const BigInt& a, unsigned n) {
    if (a.sign() <= 0) return BigInt();
    if (n == 1) return a;
    // initial estimate with doubles, then fix up
    double est = std::pow(a.toDouble(), 1.0 / n);
    BigInt x;
    if (std::isfinite(est) && est < 9e15) x = BigInt((long long)std::llround(est));
    else {
        // Newton's method from a large start
        x = BigInt::pow(BigInt(10), (unsigned long)(a.digits() / n + 1));
        for (int it = 0; it < 2000; ++it) {
            BigInt xn1 = BigInt::pow(x, n - 1);
            BigInt nx = (BigInt((long long)(n - 1)) * x + a / xn1) / BigInt((long long)n);
            if (nx >= x) break;
            x = nx;
        }
    }
    while (x.sign() > 0 && BigInt::pow(x, n) > a) x -= BigInt(1);
    while (BigInt::pow(x + BigInt(1), n) <= a) x += BigInt(1);
    return x;
}

}  // namespace me
