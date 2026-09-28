// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "core/expr.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace me {

// ======================= construction =======================


E num(const Rational& r) { auto n = std::make_shared<Node>(); n->kind = Kind::Num; n->num = r; return n; }
E num(long long v) { return num(Rational(v)); }
E sym(const std::string& name) { auto n = std::make_shared<Node>(); n->kind = Kind::Sym; n->name = name; return n; }
E add(std::vector<E> terms) {
    if (terms.empty()) return num(0);
    if (terms.size() == 1) return terms[0];
    auto n = std::make_shared<Node>(); n->kind = Kind::Add; n->args = std::move(terms); return n;
}
E add(E a, E b) { return add(std::vector<E>{a, b}); }
E mul(std::vector<E> factors) {
    if (factors.empty()) return num(1);
    if (factors.size() == 1) return factors[0];
    auto n = std::make_shared<Node>(); n->kind = Kind::Mul; n->args = std::move(factors); return n;
}
E mul(E a, E b) { return mul(std::vector<E>{a, b}); }
E pow(E base, E exp) { auto n = std::make_shared<Node>(); n->kind = Kind::Pow; n->args = {base, exp}; return n; }
E func(const std::string& name, std::vector<E> args) {
    auto n = std::make_shared<Node>(); n->kind = Kind::Func; n->name = name; n->args = std::move(args); return n;
}
E func(const std::string& name, E arg) { return func(name, std::vector<E>{arg}); }
E neg(E a) { return mul(num(-1), a); }
E sub(E a, E b) { return add(a, neg(b)); }
E divide(E a, E b) { return mul(a, pow(b, num(-1))); }
E sqrtE(E a) { return pow(a, num(Rational(1, 2))); }
E approxE(double v) {
    std::string s = fmtDouble(v, 10);
    if (s.find_first_of("eEn") != std::string::npos) return num(Rational((long long)std::llround(v)));
    return func("approx", num(Rational::fromDecimal(s)));
}

E PI() { static E e = sym("pi"); return e; }
E EUL() { static E e = sym("e"); return e; }
E IMAG() { static E e = sym("i"); return e; }
E INF() { static E e = sym("inf"); return e; }
E UNDEF() { static E e = sym("undefined"); return e; }

// ======================= inspection =======================

static bool isConstName(const std::string& n) {
    return n == "pi" || n == "e" || n == "i" || n == "inf" || n == "undefined";
}

bool isNumVal(const E& e, long long v) { return e->kind == Kind::Num && e->num == Rational(v); }
bool isInteger(const E& e) { return e->kind == Kind::Num && e->num.isInteger(); }
bool isUndefined(const E& e) {
    if (isSym(e, "undefined")) return true;
    for (auto& a : e->args) if (isUndefined(a)) return true;
    return false;
}

bool isNegativeTerm(const E& e) {
    if (e->kind == Kind::Num) return e->num.isNeg();
    if (e->kind == Kind::Mul && !e->args.empty() && e->args[0]->kind == Kind::Num) return e->args[0]->num.isNeg();
    return false;
}

bool containsSym(const E& e, const std::string& s) {
    if (e->kind == Kind::Sym) return e->name == s;
    for (auto& a : e->args) if (containsSym(a, s)) return true;
    return false;
}

bool hasFreeVars(const E& e) {
    if (e->kind == Kind::Sym) return !isConstName(e->name);
    for (auto& a : e->args) if (hasFreeVars(a)) return true;
    return false;
}

bool containsFunc(const E& e, const std::string& fn) {
    if (e->kind == Kind::Func && e->name == fn) return true;
    for (auto& a : e->args) if (containsFunc(a, fn)) return true;
    return false;
}

bool isConstantWrt(const E& e, const std::string& var) { return !containsSym(e, var); }

void collectVars(const E& e, std::vector<std::string>& out) {
    if (e->kind == Kind::Sym) {
        if (!isConstName(e->name) && std::find(out.begin(), out.end(), e->name) == out.end()) out.push_back(e->name);
        return;
    }
    for (auto& a : e->args) collectVars(a, out);
}

static int kindRank(Kind k) {
    switch (k) {
        case Kind::Num: return 0;
        case Kind::Sym: return 1;
        case Kind::Pow: return 2;
        case Kind::Func: return 3;
        case Kind::Mul: return 4;
        case Kind::Add: return 5;
    }
    return 6;
}

int cmpE(const E& a, const E& b) {
    if (a.get() == b.get()) return 0;
    int ra = kindRank(a->kind), rb = kindRank(b->kind);
    if (ra != rb) return ra < rb ? -1 : 1;
    switch (a->kind) {
        case Kind::Num: return cmp(a->num, b->num);
        case Kind::Sym: return a->name < b->name ? -1 : (a->name > b->name ? 1 : 0);
        case Kind::Func:
            if (a->name != b->name) return a->name < b->name ? -1 : 1;
            [[fallthrough]];
        default: {
            size_t n = std::min(a->args.size(), b->args.size());
            for (size_t i = 0; i < n; ++i) {
                int c = cmpE(a->args[i], b->args[i]);
                if (c) return c;
            }
            if (a->args.size() != b->args.size()) return a->args.size() < b->args.size() ? -1 : 1;
            return 0;
        }
    }
}

std::pair<Rational, E> splitCoeff(const E& e) {
    if (e->kind == Kind::Num) return {e->num, num(1)};
    if (e->kind == Kind::Mul && !e->args.empty() && e->args[0]->kind == Kind::Num) {
        std::vector<E> rest(e->args.begin() + 1, e->args.end());
        return {e->args[0]->num, mul(rest)};
    }
    return {Rational(1), e};
}

std::pair<E, E> numDen(const E& e) {
    std::vector<E> n, d;
    auto handle = [&](const E& f) {
        if (f->kind == Kind::Pow && isNum(f->args[1]) && f->args[1]->num.isNeg()) {
            Rational p = -f->args[1]->num;
            d.push_back(p.isOne() ? f->args[0] : pow(f->args[0], num(p)));
        } else if (f->kind == Kind::Num && !f->num.isInteger()) {
            if (!f->num.num().isOne()) n.push_back(num(f->num.num()));
            d.push_back(num(f->num.den()));
        } else {
            n.push_back(f);
        }
    };
    if (e->kind == Kind::Mul) for (auto& f : e->args) handle(f); else handle(e);
    return {simplify(mul(n)), simplify(mul(d))};
}

// ======================= simplification =======================

static E simplifyAdd(std::vector<E> args);
static E simplifyMul(std::vector<E> args, int depth = 0);
static E simplifyPow(const E& b, const E& ex);
static E simplifyFunc(const std::string& name, std::vector<E> args);

static E makeTerm(const Rational& c, const E& rest) {
    if (c.isZero()) return num(0);
    if (isNumVal(rest, 1)) return num(c);
    if (c.isOne()) return rest;
    std::vector<E> f{num(c)};
    if (rest->kind == Kind::Mul) f.insert(f.end(), rest->args.begin(), rest->args.end());
    else f.push_back(rest);
    return mul(f);
}

static E simplifyAdd(std::vector<E> in) {
    std::vector<E> flat;
    for (auto& a : in) {
        if (isSym(a, "undefined")) return UNDEF();
        if (a->kind == Kind::Add) flat.insert(flat.end(), a->args.begin(), a->args.end());
        else flat.push_back(a);
    }
    Rational c(0);
    std::vector<std::pair<E, Rational>> terms;
    int infSign = 0;
    for (auto& t : flat) {
        if (t->kind == Kind::Num) { c += t->num; continue; }
        auto [k, rest] = splitCoeff(t);
        if (isSym(rest, "inf")) { int s = k.sign(); if (infSign && infSign != s) return UNDEF(); infSign = s; continue; }
        bool found = false;
        for (auto& p : terms) if (eq(p.first, rest)) { p.second += k; found = true; break; }
        if (!found) terms.push_back({rest, k});
    }
    if (infSign) return infSign > 0 ? INF() : mul(num(-1), INF());
    std::vector<E> out;
    for (auto& [rest, k] : terms) if (!k.isZero()) out.push_back(makeTerm(k, rest));
    std::sort(out.begin(), out.end(), [](const E& a, const E& b) { return cmpE(a, b) < 0; });
    if (!c.isZero()) out.insert(out.begin(), num(c));
    if (out.empty()) return num(0);
    if (out.size() == 1) return out[0];
    return add(out);
}

static bool isPositiveConst(const E& e) {
    if (hasFreeVars(e) || containsSym(e, "i")) return false;
    auto v = evalReal(e);
    return v && *v > 0;
}

static E simplifyMul(std::vector<E> in, int depth) {
    std::vector<E> flat;
    for (auto& a : in) {
        if (isSym(a, "undefined")) return UNDEF();
        if (a->kind == Kind::Mul) flat.insert(flat.end(), a->args.begin(), a->args.end());
        else flat.push_back(a);
    }
    Rational c(1);
    std::vector<std::pair<E, E>> groups;  // base -> exponent
    bool hasInf = false;
    for (auto& f : flat) {
        if (f->kind == Kind::Num) { c *= f->num; continue; }
        if (isSym(f, "inf")) { hasInf = true; continue; }
        E b = f, ex = num(1);
        if (f->kind == Kind::Pow) { b = f->args[0]; ex = f->args[1]; }
        bool found = false;
        for (auto& g : groups) {
            if (eq(g.first, b)) { g.second = simplifyAdd({g.second, ex}); found = true; break; }
        }
        if (!found) groups.push_back({b, ex});
    }
    if (hasInf) {
        if (c.isZero()) return UNDEF();
        if (groups.empty()) return c.isNeg() ? mul(num(-1), INF()) : INF();
    }
    if (c.isZero()) return num(0);
    // Combine numeric bases that share the same fractional exponent: 2^(1/2)*3^(1/2) = 6^(1/2)
    for (size_t i = 0; i < groups.size(); ++i) {
        if (!isNum(groups[i].first) || !isNum(groups[i].second) || groups[i].second->num.isInteger()) continue;
        for (size_t j = i + 1; j < groups.size();) {
            if (isNum(groups[j].first) && isNum(groups[j].second) && groups[j].second->num == groups[i].second->num &&
                groups[i].first->num.sign() > 0 && groups[j].first->num.sign() > 0) {
                groups[i].first = num(groups[i].first->num * groups[j].first->num);
                groups.erase(groups.begin() + j);
            } else ++j;
        }
    }
    std::vector<E> factors;
    bool again = false;
    for (auto& [b, ex] : groups) {
        E p = simplifyPow(b, ex);
        if (p->kind == Kind::Num) { c *= p->num; continue; }
        if (isSym(p, "undefined")) return UNDEF();
        if (p->kind == Kind::Mul) {
            for (auto& f : p->args) {
                if (f->kind == Kind::Num) c *= f->num;
                else { factors.push_back(f); again = true; }
            }
            continue;
        }
        factors.push_back(p);
    }
    if (c.isZero()) return num(0);
    if (again && depth < 3) {
        std::vector<E> nf{num(c)};
        nf.insert(nf.end(), factors.begin(), factors.end());
        // Avoid infinite recursion: only resimplify if some base repeats
        bool repeat = false;
        for (size_t i = 0; i < factors.size() && !repeat; ++i)
            for (size_t j = i + 1; j < factors.size(); ++j) {
                E bi = factors[i]->kind == Kind::Pow ? factors[i]->args[0] : factors[i];
                E bj = factors[j]->kind == Kind::Pow ? factors[j]->args[0] : factors[j];
                if (eq(bi, bj)) { repeat = true; break; }
            }
        if (repeat) return simplifyMul(nf, depth + 1);
    }
    if (hasInf) {
        factors.push_back(INF());
    }
    std::sort(factors.begin(), factors.end(), [](const E& a, const E& b) { return cmpE(a, b) < 0; });
    // canonical form: distribute a numeric coefficient over a single sum, c*(a+b) = c*a + c*b
    if (!c.isOne() && factors.size() == 1 && factors[0]->kind == Kind::Add) {
        std::vector<E> terms;
        for (auto& t : factors[0]->args) terms.push_back(simplifyMul({num(c), t}, depth + 1));
        return simplifyAdd(terms);
    }
    std::vector<E> out;
    if (!c.isOne() || factors.empty()) out.push_back(num(c));
    out.insert(out.end(), factors.begin(), factors.end());
    E result = out.size() == 1 ? out[0] : mul(out);
    // exact complex arithmetic: (1+i)(2-i) etc.
    if (containsSym(result, "i") && !hasFreeVars(result)) {
        bool hasSum = false;
        for (auto& f : factors) if (f->kind == Kind::Add || (f->kind == Kind::Pow && containsSym(f, "i"))) hasSum = true;
        if (hasSum) if (auto z = asCRat(result)) return fromCRat(*z);
    }
    return result;
}

// Largest a with a^q | m (m > 0). Returns {a, m / a^q}
static std::pair<BigInt, BigInt> extractPower(BigInt m, unsigned q) {
    BigInt a(1);
    for (long long p = 2; p < 2000; ++p) {
        BigInt P(p);
        if (BigInt(p * p) > m && p > 2) break;
        unsigned cnt = 0;
        while ((m % P).isZero()) { m = m / P; ++cnt; }
        if (cnt >= q) a *= BigInt::pow(P, cnt / q);
        if (cnt % q) m *= BigInt::pow(P, cnt % q);
        if (cnt) {
            // m currently includes leftover factors of p; skip rescanning them
            // (they are < q copies so won't form a q-th power later)
            BigInt left = BigInt::pow(P, cnt % q);
            BigInt rest = m / left;
            // continue scanning on rest, then multiply left back
            auto sub = extractPower(rest, q);
            // sub handles primes >= 2 again but rest has no factor p now
            return {a * sub.first, sub.second * left};
        }
    }
    // remaining m may itself be a perfect q-th power of a large prime
    BigInt r = BigInt::iroot(m, q);
    if (!r.isZero() && BigInt::pow(r, q) == m) return {a * r, BigInt(1)};
    return {a, m};
}

// b^(p/q), b rational, q > 1
static E rootRational(const Rational& b, const Rational& ex) {
    BigInt p = ex.num(), qb = ex.den();
    if (!qb.fitsInt64() || qb.toInt64() > 64 || !p.fitsInt64() || std::llabs(p.toInt64()) > 64) return pow(num(b), num(ex));
    long long q = qb.toInt64(), pp = p.toInt64();
    if (b.isNeg()) {
        if (q % 2 == 1) {
            E r = rootRational(-b, ex);
            return (pp % 2 != 0) ? simplifyMul({num(-1), r}) : r;
        }
        if (q == 2) {
            E r = rootRational(-b, ex);
            return simplifyMul({r, simplifyPow(IMAG(), num(pp))});
        }
        return pow(num(b), num(ex));
    }
    // b > 0
    long long k = pp >= 0 ? pp / q : -((-pp + q - 1) / q);
    long long r = pp - k * q;  // 0 <= r < q
    Rational coeff = b.pow(k);
    if (r == 0) return num(coeff);
    BigInt n = b.num(), d = b.den();
    BigInt M = BigInt::pow(n, (unsigned long)r);
    if (!d.isOne()) M *= BigInt::pow(d, (unsigned long)(q - r));
    if (M.digits() > 80) return pow(num(b), num(ex));
    auto [a, rest] = extractPower(M, (unsigned)q);
    coeff *= Rational(a);
    if (!d.isOne()) coeff /= Rational(d);
    if (rest.isOne()) return num(coeff);
    // reduce radical index: rest = s^g with g | q
    long long qq = q;
    for (long long g = q; g >= 2; --g) {
        if (q % g) continue;
        BigInt s = BigInt::iroot(rest, (unsigned)g);
        if (BigInt::pow(s, (unsigned long)g) == rest) { rest = s; qq = q / g; break; }
    }
    E rad = qq == 1 ? num(rest) : pow(num(rest), num(Rational(1, qq)));
    if (qq == 1) return num(coeff * Rational(rest));
    if (coeff.isOne()) return rad;
    return mul(num(coeff), rad);
}

static E simplifyPow(const E& b, const E& ex) {
    if (isSym(b, "undefined") || isSym(ex, "undefined")) return UNDEF();
    if (isNumVal(ex, 0)) return isNumVal(b, 0) ? UNDEF() : num(1);
    if (isNumVal(ex, 1)) return b;
    if (isNumVal(b, 1)) return num(1);
    if (isNumVal(b, 0)) {
        if (isNum(ex)) return ex->num.sign() > 0 ? num(0) : UNDEF();
        return pow(b, ex);
    }
    if (isSym(b, "inf") && isNum(ex)) return ex->num.sign() > 0 ? INF() : num(0);
    if (isSym(b, "i") && isInteger(ex)) {
        BigInt m = ex->num.num() % BigInt(4);
        long long r = m.toInt64();
        if (r < 0) r += 4;
        switch (r) { case 0: return num(1); case 1: return IMAG(); case 2: return num(-1); default: return mul(num(-1), IMAG()); }
    }
    if (isNum(b) && isNum(ex)) {
        const Rational& e = ex->num;
        if (e.isInteger()) {
            if (e.num().fitsInt64() && std::llabs(e.num().toInt64()) <= 2048) {
                Rational base = b->num;
                // limit size of the result
                double approxDigits = std::fabs(std::log10(std::max(std::fabs(base.toDouble()), 1e-300))) * std::fabs(e.toDouble());
                if (approxDigits < 3000) return num(base.pow(e.num().toInt64()));
            }
            return pow(b, ex);
        }
        return rootRational(b->num, e);
    }
    if (b->kind == Kind::Pow) {
        E e1 = b->args[1];
        if (isInteger(ex)) return simplifyPow(b->args[0], simplifyMul({e1, ex}));
        if (isNum(e1) && isNum(ex) && isPositiveConst(b->args[0])) return simplifyPow(b->args[0], simplifyMul({e1, ex}));
        // (x^2)^(1/2) keep; (sqrt(x))^2 -> x handled by integer case
    }
    if (b->kind == Kind::Mul) {
        if (isInteger(ex)) {
            std::vector<E> f;
            for (auto& a : b->args) f.push_back(simplifyPow(a, ex));
            return simplifyMul(f);
        }
        // split off positive numeric / constant factors
        std::vector<E> pos, rest;
        for (auto& a : b->args) {
            if ((isNum(a) && a->num.sign() > 0) || isPositiveConst(a)) pos.push_back(a); else rest.push_back(a);
        }
        if (!pos.empty() && !rest.empty() && isNum(ex)) {
            std::vector<E> f;
            for (auto& a : pos) f.push_back(simplifyPow(a, ex));
            f.push_back(pow(mul(rest), ex));
            return simplifyMul(f);
        }
    }
    if (isSym(b, "e")) {
        if (isFunc(ex, "ln")) return ex->args[0];
        if (ex->kind == Kind::Mul && ex->args.size() == 2 && isNum(ex->args[0]) && isFunc(ex->args[1], "ln"))
            return simplifyPow(ex->args[1]->args[0], ex->args[0]);
    }
    if (isNum(b) && isFunc(ex, "log") && ex->args[0]->kind == Kind::Num && ex->args[0]->num == b->num) return ex->args[1];
    // complex base with integer exponent
    if (isInteger(ex) && containsSym(b, "i") && !hasFreeVars(b)) {
        if (auto z = asCRat(pow(b, ex))) return fromCRat(*z);
    }
    return pow(b, ex);
}

// ---- exact trig tables ----
static E sqrtN(long long n) { return pow(num(n), num(Rational(1, 2))); }
static E half(E x) { return simplifyMul({num(Rational(1, 2)), x}); }

// returns exact sin(c*pi) for c in [0,2) with den in {1,2,3,4,6}
static std::optional<E> sinPiExact(Rational c) {
    // reduce to [0, 2)
    Rational two(2);
    BigInt fl = (c / two).floor();
    c = c - Rational(fl) * two;
    if (!(c.den() == BigInt(1) || c.den() == BigInt(2) || c.den() == BigInt(3) || c.den() == BigInt(4) || c.den() == BigInt(6)))
        return std::nullopt;
    // quadrant reduction
    int sign = 1;
    if (c >= Rational(1)) { c = c - Rational(1); sign = -1; }
    if (c > Rational(1, 2)) c = Rational(1) - c;
    E v;
    if (c.isZero()) v = num(0);
    else if (c == Rational(1, 6)) v = num(Rational(1, 2));
    else if (c == Rational(1, 4)) v = half(sqrtN(2));
    else if (c == Rational(1, 3)) v = half(sqrtN(3));
    else if (c == Rational(1, 2)) v = num(1);
    else return std::nullopt;
    return sign < 0 ? simplifyMul({num(-1), v}) : v;
}

static std::optional<Rational> piCoeff(const E& a) {
    if (isNumVal(a, 0)) return Rational(0);
    if (isSym(a, "pi")) return Rational(1);
    if (a->kind == Kind::Mul && a->args.size() == 2 && isNum(a->args[0]) && isSym(a->args[1], "pi")) return a->args[0]->num;
    return std::nullopt;
}

static E negTerm(const E& a) { return simplifyMul({num(-1), a}); }

static std::optional<E> inverseTrigExact(const std::string& fn, const E& arg) {
    if (hasFreeVars(arg)) return std::nullopt;
    auto v = evalReal(arg);
    if (!v) return std::nullopt;
    double x = *v;
    struct Entry { double val; Rational c; };
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-12; };
    // only accept "nice" algebraic arguments to avoid false positives
    if (arg->kind == Kind::Func) return std::nullopt;
    std::vector<std::pair<double, Rational>> table;
    if (fn == "asin") table = {{0, 0}, {0.5, Rational(1, 6)}, {std::sqrt(2) / 2, Rational(1, 4)}, {std::sqrt(3) / 2, Rational(1, 3)}, {1, Rational(1, 2)}};
    else if (fn == "acos") table = {{1, 0}, {std::sqrt(3) / 2, Rational(1, 6)}, {std::sqrt(2) / 2, Rational(1, 4)}, {0.5, Rational(1, 3)}, {0, Rational(1, 2)},
                                    {-0.5, Rational(2, 3)}, {-std::sqrt(2) / 2, Rational(3, 4)}, {-std::sqrt(3) / 2, Rational(5, 6)}, {-1, Rational(1)}};
    else if (fn == "atan") table = {{0, 0}, {std::sqrt(3) / 3, Rational(1, 6)}, {1, Rational(1, 4)}, {std::sqrt(3), Rational(1, 3)}};
    else return std::nullopt;
    for (auto& [val, c] : table) {
        if (near(x, val)) return simplifyMul({num(c), PI()});
        if (fn != "acos" && near(x, -val)) return simplifyMul({num(-c), PI()});
    }
    return std::nullopt;
}

static bool isTrig(const std::string& n) {
    return n == "sin" || n == "cos" || n == "tan" || n == "sec" || n == "csc" || n == "cot";
}

static E simplifyFunc(const std::string& name, std::vector<E> args) {
    for (auto& a : args) if (isSym(a, "undefined")) return UNDEF();
    if (args.empty()) return func(name, args);
    const E& a = args[0];
    if (name == "exp") return simplifyPow(EUL(), a);
    if (name == "sqrt") return simplifyPow(a, num(Rational(1, 2)));
    if (name == "abs") {
        if (isNum(a)) return num(a->num.abs());
        if (!hasFreeVars(a)) {
            if (auto z = asCRat(a)) {
                if (!z->im.isZero()) return simplifyPow(num(z->re * z->re + z->im * z->im), num(Rational(1, 2)));
                return num(z->re.abs());
            }
            if (auto v = evalReal(a)) if (!containsSym(a, "i")) return *v >= 0 ? a : negTerm(a);
        }
        if (isNegativeTerm(a)) return func("abs", negTerm(a));
        if (isFunc(a, "abs")) return a;
        return func("abs", a);
    }
    if (name == "fact") {
        if (isInteger(a) && a->num.sign() >= 0 && a->num.num().fitsInt64() && a->num.num().toInt64() <= 3000) {
            BigInt r(1);
            for (long long k = 2; k <= a->num.num().toInt64(); ++k) r *= BigInt(k);
            return num(Rational(r));
        }
        if (isNum(a) && a->num.isNeg() && a->num.isInteger()) return UNDEF();
        return func("fact", a);
    }
    if (name == "ln") {
        if (isNumVal(a, 1)) return num(0);
        if (isNumVal(a, 0)) return UNDEF();
        if (isSym(a, "e")) return num(1);
        if (a->kind == Kind::Pow && isSym(a->args[0], "e")) return a->args[1];
        return func("ln", a);
    }
    if (name == "log") {
        if (args.size() == 1) args.insert(args.begin(), num(10));
        const E& b = args[0];
        const E& x = args[1];
        if (isNumVal(x, 1)) return num(0);
        if (eq(b, x)) return num(1);
        if (x->kind == Kind::Pow && eq(x->args[0], b)) return x->args[1];
        if (isNum(b) && isNum(x) && b->num.sign() > 0 && x->num.sign() > 0 && !b->num.isOne()) {
            double k = std::log(x->num.toDouble()) / std::log(b->num.toDouble());
            long long kr = std::llround(k);
            if (std::fabs(k - kr) < 1e-9 && std::llabs(kr) < 4000 && b->num.pow(kr) == x->num) return num(kr);
            // fractional: log_4(2) = 1/2, log_8(4) = 2/3
            for (long long den = 2; den <= 6; ++den) {
                double kk = k * den;
                long long kn = std::llround(kk);
                if (std::fabs(kk - kn) < 1e-9 && kn != 0) {
                    // check b^kn == x^den
                    if (std::llabs(kn) < 200 && b->num.pow(kn) == x->num.pow(den)) return num(Rational(kn, den));
                }
            }
        }
        return func("log", args);
    }
    if (isTrig(name)) {
        E arg = a;
        if (isNegativeTerm(arg)) {
            E pa = negTerm(arg);
            if (name == "cos" || name == "sec") return simplifyFunc(name, {pa});
            return negTerm(simplifyFunc(name, {pa}));
        }
        if (auto c = piCoeff(arg)) {
            auto s = sinPiExact(*c);
            auto co = sinPiExact(*c + Rational(1, 2));
            if (s && co) {
                if (name == "sin") return *s;
                if (name == "cos") return *co;
                if (name == "tan") return isNumVal(*co, 0) ? UNDEF() : simplify(divide(*s, *co));
                if (name == "cot") return isNumVal(*s, 0) ? UNDEF() : simplify(divide(*co, *s));
                if (name == "sec") return isNumVal(*co, 0) ? UNDEF() : simplify(divide(num(1), *co));
                if (name == "csc") return isNumVal(*s, 0) ? UNDEF() : simplify(divide(num(1), *s));
            }
        }
        // sin(asin(x)) = x etc.
        if (name == "sin" && isFunc(arg, "asin")) return arg->args[0];
        if (name == "cos" && isFunc(arg, "acos")) return arg->args[0];
        if (name == "tan" && isFunc(arg, "atan")) return arg->args[0];
        return func(name, arg);
    }
    if (name == "asin" || name == "acos" || name == "atan") {
        if (auto r = inverseTrigExact(name, a)) return *r;
        if (name != "acos" && isNegativeTerm(a)) return negTerm(simplifyFunc(name, {negTerm(a)}));
        return func(name, a);
    }
    if (name == "sinh" || name == "tanh") { if (isNumVal(a, 0)) return num(0); }
    if (name == "cosh") { if (isNumVal(a, 0)) return num(1); }
    if (name == "conj" || name == "re" || name == "im") {
        if (auto z = asCRat(a)) {
            if (name == "conj") return fromCRat({z->re, -z->im});
            if (name == "re") return num(z->re);
            return num(z->im);
        }
        if (name == "conj" && !containsSym(a, "i") && !containsSym(a, "z")) return a;
    }
    if (name == "arg") {
        if (auto z = asCRat(a)) {
            if (z->re.isZero() && z->im.isZero()) return UNDEF();
            double th = std::atan2(z->im.toDouble(), z->re.toDouble());
            for (int k = -12; k <= 12; ++k) if (std::fabs(th - k * M_PI / 12) < 1e-12) return simplifyMul({num(Rational(k, 12)), PI()});
            if (z->re.sign() > 0) return simplifyFunc("atan", {num(z->im / z->re)});
        }
    }
    if ((name == "gcd" || name == "lcm") && args.size() >= 2) {
        bool allInt = true;
        for (auto& x : args) allInt = allInt && isInteger(x);
        if (allInt) {
            BigInt g = args[0]->num.num().abs();
            for (size_t k = 1; k < args.size(); ++k) {
                BigInt v = args[k]->num.num().abs();
                if (name == "gcd") g = BigInt::gcd(g, v);
                else { BigInt gg = BigInt::gcd(g, v); g = gg.isZero() ? BigInt(0) : g / gg * v; }
            }
            return num(Rational(g));
        }
    }
    if ((name == "binom" || name == "perm") && args.size() == 2 && isInteger(args[0]) && isInteger(args[1])) {
        BigInt n = args[0]->num.num(), k = args[1]->num.num();
        if (k.sign() < 0 || k > n || !n.fitsInt64() || n.toInt64() > 5000) return k > n ? num(0) : func(name, args);
        long long kk = k.toInt64(), nn = n.toInt64();
        if (name == "binom" && kk > nn - kk) kk = nn - kk;
        BigInt r(1);
        for (long long j = 0; j < kk; ++j) r *= BigInt(nn - j);
        if (name == "binom") { BigInt f(1); for (long long j = 2; j <= kk; ++j) f *= BigInt(j); r = r / f; }
        return num(Rational(r));
    }
    if ((name == "floor" || name == "ceil" || name == "round") && isNum(a)) {
        BigInt f = a->num.floor();
        if (name == "floor") return num(Rational(f));
        if (name == "ceil") return num(Rational(a->num.isInteger() ? f : f + BigInt(1)));
        return num(Rational((a->num + Rational(1, 2)).floor()));
    }
    return func(name, args);
}

E simplify(const E& e) {
    switch (e->kind) {
        case Kind::Num:
        case Kind::Sym: return e;
        case Kind::Add: {
            std::vector<E> a;
            for (auto& x : e->args) a.push_back(simplify(x));
            return simplifyAdd(a);
        }
        case Kind::Mul: {
            std::vector<E> a;
            for (auto& x : e->args) a.push_back(simplify(x));
            return simplifyMul(a);
        }
        case Kind::Pow: return simplifyPow(simplify(e->args[0]), simplify(e->args[1]));
        case Kind::Func: {
            std::vector<E> a;
            for (auto& x : e->args) a.push_back(simplify(x));
            return simplifyFunc(e->name, a);
        }
    }
    return e;
}

// ======================= expansion =======================

static E expandProduct(const std::vector<E>& factors) {
    std::vector<E> terms{num(1)};
    for (auto& f : factors) {
        std::vector<E> ft = f->kind == Kind::Add ? f->args : std::vector<E>{f};
        std::vector<E> next;
        next.reserve(terms.size() * ft.size());
        for (auto& t : terms) for (auto& u : ft) next.push_back(simplifyMul({t, u}));
        terms = std::move(next);
        if (terms.size() > 4000) break;
    }
    return simplifyAdd(terms);
}

E expand(const E& e0) {
    E e = simplify(e0);
    switch (e->kind) {
        case Kind::Num:
        case Kind::Sym: return e;
        case Kind::Add: {
            std::vector<E> a;
            for (auto& x : e->args) a.push_back(expand(x));
            return simplifyAdd(a);
        }
        case Kind::Mul: {
            std::vector<E> a;
            for (auto& x : e->args) a.push_back(expand(x));
            return expandProduct(a);
        }
        case Kind::Pow: {
            E b = expand(e->args[0]);
            E ex = e->args[1];
            if (b->kind == Kind::Add && isInteger(ex)) {
                long long n = ex->num.num().fitsInt64() ? ex->num.num().toInt64() : 0;
                if (n >= 2 && n <= 16) {
                    std::vector<E> f(n, b);
                    return expandProduct(f);
                }
                if (n <= -2 && n >= -16) {
                    std::vector<E> f(-n, b);
                    return simplifyPow(expandProduct(f), num(-1));
                }
            }
            return simplifyPow(b, expand(ex));
        }
        case Kind::Func: {
            std::vector<E> a;
            for (auto& x : e->args) a.push_back(expand(x));
            return simplifyFunc(e->name, a);
        }
    }
    return e;
}

E substitute(const E& e, const std::string& var, const E& value) {
    if (e->kind == Kind::Sym) return e->name == var ? value : e;
    if (e->args.empty()) return e;
    auto n = std::make_shared<Node>(*e);
    for (auto& a : n->args) a = substitute(a, var, value);
    return n;
}

E replaceSub(const E& e, const E& target, const E& with) {
    if (eq(e, target)) return with;
    if (e->args.empty()) return e;
    auto n = std::make_shared<Node>(*e);
    for (auto& a : n->args) a = replaceSub(a, target, with);
    return n;
}

int polyDegree(const E& e, const std::string& var) {
    switch (e->kind) {
        case Kind::Num: return 0;
        case Kind::Sym: return e->name == var ? 1 : 0;
        case Kind::Add: {
            int m = 0;
            for (auto& a : e->args) { int d = polyDegree(a, var); if (d < 0) return -1; m = std::max(m, d); }
            return m;
        }
        case Kind::Mul: {
            int s = 0;
            for (auto& a : e->args) { int d = polyDegree(a, var); if (d < 0) return -1; s += d; }
            return s;
        }
        case Kind::Pow: {
            if (!containsSym(e, var)) return 0;
            if (!isInteger(e->args[1]) || e->args[1]->num.sign() < 0) return -1;
            int d = polyDegree(e->args[0], var);
            if (d < 0) return -1;
            long long n = e->args[1]->num.num().toInt64();
            if (n > 1000) return -1;
            return d * (int)n;
        }
        case Kind::Func: return containsSym(e, var) ? -1 : 0;
    }
    return -1;
}

// ======================= evaluation =======================

static double factorialD(double x) { return std::tgamma(x + 1.0); }

std::optional<double> evalRealImpl(const E& e, const std::map<std::string, double>& vars, bool& ok);

static double ev(const E& e, const std::map<std::string, double>& vars, bool& ok) {
    if (!ok) return NAN;
    switch (e->kind) {
        case Kind::Num: return e->num.toDouble();
        case Kind::Sym: {
            if (e->name == "pi") return M_PI;
            if (e->name == "e") return M_E;
            if (e->name == "inf") return INFINITY;
            auto it = vars.find(e->name);
            if (it != vars.end()) return it->second;
            ok = false;
            return NAN;
        }
        case Kind::Add: { double s = 0; for (auto& a : e->args) s += ev(a, vars, ok); return s; }
        case Kind::Mul: { double s = 1; for (auto& a : e->args) s *= ev(a, vars, ok); return s; }
        case Kind::Pow: {
            double b = ev(e->args[0], vars, ok);
            const E& ex = e->args[1];
            if (isNum(ex) && !ex->num.isInteger() && b < 0) {
                // real odd roots of negative numbers
                if (!ex->num.den().isEven()) {
                    double r = std::pow(-b, ex->num.toDouble());
                    return ex->num.num().isEven() ? r : -r;
                }
                return NAN;
            }
            double x = ev(ex, vars, ok);
            return std::pow(b, x);
        }
        case Kind::Func: {
            const std::string& n = e->name;
            double x = ev(e->args[0], vars, ok);
            if (n == "sin") return std::sin(x);
            if (n == "cos") return std::cos(x);
            if (n == "tan") return std::tan(x);
            if (n == "sec") return 1 / std::cos(x);
            if (n == "csc") return 1 / std::sin(x);
            if (n == "cot") return 1 / std::tan(x);
            if (n == "asin") return std::asin(x);
            if (n == "acos") return std::acos(x);
            if (n == "atan") return std::atan(x);
            if (n == "sinh") return std::sinh(x);
            if (n == "cosh") return std::cosh(x);
            if (n == "tanh") return std::tanh(x);
            if (n == "ln") return x > 0 ? std::log(x) : NAN;
            if (n == "log") {
                double y = e->args.size() > 1 ? ev(e->args[1], vars, ok) : x;
                double b = e->args.size() > 1 ? x : 10.0;
                return (y > 0 && b > 0) ? std::log(y) / std::log(b) : NAN;
            }
            if (n == "abs") return std::fabs(x);
            if (n == "fact") return factorialD(x);
            if (n == "exp") return std::exp(x);
            if (n == "sqrt") return x >= 0 ? std::sqrt(x) : NAN;
            if (n == "floor") return std::floor(x);
            if (n == "ceil") return std::ceil(x);
            if (n == "round") return std::round(x);
            if (n == "conj" || n == "re" || n == "approx") return x;
            if (n == "im") return 0;
            if (n == "binom" && e->args.size() == 2) {
                double k = ev(e->args[1], vars, ok);
                return factorialD(x) / (factorialD(k) * factorialD(x - k));
            }
            if (n == "perm" && e->args.size() == 2) {
                double k = ev(e->args[1], vars, ok);
                return factorialD(x) / factorialD(x - k);
            }
            ok = false;
            return NAN;
        }
    }
    ok = false;
    return NAN;
}

std::optional<double> evalReal(const E& e, const std::map<std::string, double>& vars) {
    if (containsSym(e, "i") || containsSym(e, "undefined")) {
        // allow complex evaluation that lands on the real axis
        std::map<std::string, Cx> cv;
        for (auto& [k, v] : vars) cv[k] = v;
        auto z = evalComplex(e, cv);
        if (z && std::fabs(z->imag()) < 1e-9 * std::max(1.0, std::fabs(z->real()))) return z->real();
        return std::nullopt;
    }
    bool ok = true;
    double v = ev(e, vars, ok);
    if (!ok) return std::nullopt;
    return v;
}

static Cx evc(const E& e, const std::map<std::string, Cx>& vars, bool& ok) {
    if (!ok) return NAN;
    switch (e->kind) {
        case Kind::Num: return e->num.toDouble();
        case Kind::Sym: {
            if (e->name == "pi") return M_PI;
            if (e->name == "e") return M_E;
            if (e->name == "i") return Cx(0, 1);
            if (e->name == "inf") return INFINITY;
            auto it = vars.find(e->name);
            if (it != vars.end()) return it->second;
            ok = false;
            return NAN;
        }
        case Kind::Add: { Cx s = 0; for (auto& a : e->args) s += evc(a, vars, ok); return s; }
        case Kind::Mul: { Cx s = 1; for (auto& a : e->args) s *= evc(a, vars, ok); return s; }
        case Kind::Pow: {
            Cx b = evc(e->args[0], vars, ok);
            if (isInteger(e->args[1]) && e->args[1]->num.num().fitsInt64()) {
                long long n = e->args[1]->num.num().toInt64();
                if (std::llabs(n) < 64) {
                    Cx r = 1, bb = n < 0 ? Cx(1) / b : b;
                    for (long long k = 0; k < std::llabs(n); ++k) r *= bb;
                    return r;
                }
            }
            return std::pow(b, evc(e->args[1], vars, ok));
        }
        case Kind::Func: {
            const std::string& n = e->name;
            Cx x = evc(e->args[0], vars, ok);
            if (n == "sin") return std::sin(x);
            if (n == "cos") return std::cos(x);
            if (n == "tan") return std::tan(x);
            if (n == "sec") return 1.0 / std::cos(x);
            if (n == "csc") return 1.0 / std::sin(x);
            if (n == "cot") return 1.0 / std::tan(x);
            if (n == "asin") return std::asin(x);
            if (n == "acos") return std::acos(x);
            if (n == "atan") return std::atan(x);
            if (n == "sinh") return std::sinh(x);
            if (n == "cosh") return std::cosh(x);
            if (n == "tanh") return std::tanh(x);
            if (n == "ln") return std::log(x);
            if (n == "log") {
                if (e->args.size() > 1) return std::log(evc(e->args[1], vars, ok)) / std::log(x);
                return std::log10(x);
            }
            if (n == "abs") return std::abs(x);
            if (n == "exp") return std::exp(x);
            if (n == "sqrt") return std::sqrt(x);
            if (n == "conj") return std::conj(x);
            if (n == "approx") return x;
            if (n == "re") return x.real();
            if (n == "im") return x.imag();
            if (n == "arg") return std::arg(x);
            if (n == "fact") return factorialD(x.real());
            ok = false;
            return NAN;
        }
    }
    ok = false;
    return NAN;
}

std::optional<Cx> evalComplex(const E& e, const std::map<std::string, Cx>& vars) {
    bool ok = true;
    Cx v = evc(e, vars, ok);
    if (!ok) return std::nullopt;
    return v;
}

std::optional<CRat> asCRat(const E& e) {
    switch (e->kind) {
        case Kind::Num: return CRat{e->num, Rational(0)};
        case Kind::Sym:
            if (e->name == "i") return CRat{Rational(0), Rational(1)};
            return std::nullopt;
        case Kind::Add: {
            CRat s{Rational(0), Rational(0)};
            for (auto& a : e->args) {
                auto z = asCRat(a);
                if (!z) return std::nullopt;
                s.re += z->re; s.im += z->im;
            }
            return s;
        }
        case Kind::Mul: {
            CRat s{Rational(1), Rational(0)};
            for (auto& a : e->args) {
                auto z = asCRat(a);
                if (!z) return std::nullopt;
                s = CRat{s.re * z->re - s.im * z->im, s.re * z->im + s.im * z->re};
            }
            return s;
        }
        case Kind::Pow: {
            if (!isInteger(e->args[1])) return std::nullopt;
            auto z = asCRat(e->args[0]);
            if (!z) return std::nullopt;
            if (!e->args[1]->num.num().fitsInt64()) return std::nullopt;
            long long n = e->args[1]->num.num().toInt64();
            if (std::llabs(n) > 256) return std::nullopt;
            CRat b = *z;
            if (n < 0) {
                Rational d = b.re * b.re + b.im * b.im;
                if (d.isZero()) return std::nullopt;
                b = CRat{b.re / d, -b.im / d};
                n = -n;
            }
            CRat r{Rational(1), Rational(0)};
            for (long long k = 0; k < n; ++k) r = CRat{r.re * b.re - r.im * b.im, r.re * b.im + r.im * b.re};
            return r;
        }
        default: return std::nullopt;
    }
}

E fromCRat(const CRat& c) {
    if (c.im.isZero()) return num(c.re);
    E im = c.im.isOne() ? IMAG() : mul(num(c.im), IMAG());
    if (c.re.isZero()) return im;
    return add(num(c.re), im);
}

}  // namespace me
