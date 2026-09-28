// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Step-by-step arithmetic on the raw syntax tree, following the order of operations
// (parentheses, exponents/functions, multiplication/division, addition/subtraction)
// with detailed fraction sub-steps.
#include <algorithm>
#include <cmath>

#include "solution/plotutil.h"
#include "solvers.h"

namespace me {

namespace {

using T = RNode::T;

R clone(const R& r) {
    auto n = std::make_shared<RNode>(*r);
    for (auto& k : n->kids) k = clone(k);
    return n;
}

std::string ratTex(const Rational& v) { return toLatex(num(v)); }

R numNode(const Rational& v) {
    auto n = std::make_shared<RNode>();
    n->t = T::Num;
    n->value = v;
    n->text = ratTex(v);
    return n;
}

// precedence class used to order the steps
int opClass(const R& r) {
    if (r->paren) return 5;
    switch (r->t) {
        case T::Func:
        case T::Fact:
        case T::Abs:
        case T::Pct: return 4;
        case T::Neg: return 4;
        case T::Bin:
            if (r->op == "^") return 4;
            if (r->op == "*" || r->op == "/" || r->op == "div") return 3;
            return 2;
        default: return 0;
    }
}

bool allNumKids(const R& r) {
    for (auto& k : r->kids) if (k->t != T::Num) return false;
    return !r->kids.empty();
}

struct Eval {
    Rational value;
    std::vector<Step> detail;
    std::string what;  // verb for titles
};

std::optional<Eval> evalNode(const R& r, std::string& err) {
    const auto& k = r->kids;
    auto v = [&](size_t i) { return k[i]->value; };
    Eval e;
    switch (r->t) {
        case T::Neg: e.value = -v(0); e.what = "Negate"; return e;
        case T::Pct: e.value = v(0) / Rational(100); e.what = "Convert the percentage";
            e.detail.push_back({"", m("p\\% = \\frac{p}{100}"), "", {}});
            return e;
        case T::Abs: e.value = v(0).abs(); e.what = "Take the absolute value"; return e;
        case T::Fact: {
            if (!v(0).isInteger() || v(0).isNeg() || v(0) > Rational(3000)) return std::nullopt;
            long long n = v(0).num().toInt64();
            BigInt f(1);
            std::string prod;
            for (long long i = n; i >= 1; --i) {
                f *= BigInt(i);
                if (n <= 10) prod += (i == n ? "" : " \\cdot ") + std::to_string(i);
            }
            e.value = Rational(f);
            e.what = "Calculate the factorial";
            if (n >= 2 && n <= 10) e.detail.push_back({"", m("n! = n \\cdot (n-1) \\cdots 1"), std::to_string(n) + "! = " + prod, {}});
            return e;
        }
        case T::Func: {
            const std::string& n = r->op;
            if (n == "sqrt" || n == "cbrt" || (n == "root" && k.size() == 2)) {
                Rational x = n == "root" ? v(1) : v(0);
                long long idx = n == "sqrt" ? 2 : n == "cbrt" ? 3 : (v(0).isInteger() ? v(0).num().toInt64() : 0);
                if (idx < 2 || idx > 64) return std::nullopt;
                E s = simplify(pow(num(x), num(Rational(1, idx))));
                if (!isNum(s)) return std::nullopt;
                e.value = s->num;
                e.what = idx == 2 ? "Calculate the square root" : "Calculate the root";
                e.detail.push_back({"", m(ratTex(s->num) + (idx == 2 ? "^{2}" : "^{" + std::to_string(idx) + "}") + " = " + ratTex(x)), "", {}});
                return e;
            }
            if ((n == "gcd" || n == "lcm" || n == "binom" || n == "perm" || n == "mod") && k.size() == 2) {
                if (!v(0).isInteger() || !v(1).isInteger()) return std::nullopt;
                if (n == "mod") {
                    if (v(1).isZero()) { err = "Division by zero"; return std::nullopt; }
                    BigInt q, rr;
                    BigInt::divmod(v(0).num(), v(1).num(), q, rr);
                    if (rr.isNeg()) rr += v(1).num().abs();
                    e.value = Rational(rr);
                    e.what = "Find the remainder";
                    return e;
                }
                E s = simplify(func(n, {num(v(0)), num(v(1))}));
                if (!isNum(s)) return std::nullopt;
                e.value = s->num;
                e.what = n == "gcd" ? "Find the greatest common divisor" : n == "lcm" ? "Find the least common multiple" : "Count the arrangements";
                return e;
            }
            if ((n == "floor" || n == "ceil" || n == "round" || n == "abs") && k.size() == 1) {
                E s = simplify(func(n, num(v(0))));
                if (!isNum(s)) return std::nullopt;
                e.value = s->num;
                e.what = "Round";
                return e;
            }
            return std::nullopt;
        }
        case T::Bin: {
            Rational a = v(0), b = v(1);
            const std::string& op = r->op;
            if (op == "+" || op == "-") {
                e.value = op == "+" ? a + b : a - b;
                e.what = op == "+" ? "Add" : "Subtract";
                if (!a.isInteger() || !b.isInteger()) {
                    BigInt l = a.den() / BigInt::gcd(a.den(), b.den()) * b.den();
                    Rational an = a * Rational(l), bn = b * Rational(l);
                    if (a.den() != b.den())
                        e.detail.push_back({"Find a common denominator", "The least common denominator of " + m(a.den().toString()) + " and " + m(b.den().toString()) + " is " + m(l.toString()) + ".",
                                            "\\frac{" + an.toString() + "}{" + l.toString() + "} " + op + " \\frac{" + (op == "-" ? bn.toString() : bn.toString()) + "}{" + l.toString() + "}", {}});
                    Rational sumNum = op == "+" ? an + bn : an - bn;
                    e.detail.push_back({"Combine the numerators", "", "\\frac{" + sumNum.toString() + "}{" + l.toString() + "}", {}});
                    if (Rational(sumNum.num(), l) != Rational(sumNum.num()) / Rational(l) || BigInt::gcd(sumNum.num(), l) != BigInt(1))
                        e.detail.push_back({"Simplify the fraction", "Divide numerator and denominator by " + m(BigInt::gcd(sumNum.num(), l).toString()) + ".", ratTex(e.value), {}});
                }
                return e;
            }
            if (op == "*") {
                e.value = a * b;
                e.what = "Multiply";
                if (!a.isInteger() || !b.isInteger())
                    e.detail.push_back({"Multiply the numerators and the denominators", "",
                                        "\\frac{" + (a.num() * b.num()).toString() + "}{" + (a.den() * b.den()).toString() + "} = " + ratTex(e.value), {}});
                return e;
            }
            if (op == "/" || op == "div") {
                if (b.isZero()) { err = "Division by zero is undefined"; return std::nullopt; }
                e.value = a / b;
                e.what = "Divide";
                if (a.isInteger() && b.isInteger()) {
                    BigInt g = BigInt::gcd(a.num(), b.num());
                    if (!e.value.isInteger() && !g.isOne())
                        e.detail.push_back({"Simplify the fraction", "The greatest common divisor of " + m(a.num().toString()) + " and " + m(b.num().toString()) + " is " + m(g.toString()) + ".",
                                            "\\frac{" + a.num().toString() + " \\div " + g.toString() + "}{" + b.num().toString() + " \\div " + g.toString() + "} = " + ratTex(e.value), {}});
                } else {
                    e.detail.push_back({"Multiply by the reciprocal", m("\\frac{a}{b} \\div \\frac{c}{d} = \\frac{a}{b} \\cdot \\frac{d}{c}"),
                                        ratTex(a) + " \\cdot " + ratTex(b.inv()) + " = " + ratTex(e.value), {}});
                }
                return e;
            }
            if (op == "^") {
                if (b.isInteger()) {
                    if (!b.num().fitsInt64() || std::llabs(b.num().toInt64()) > 4096) return std::nullopt;
                    long long n = b.num().toInt64();
                    if (a.isZero() && n <= 0) { err = "0 to a non-positive power is undefined"; return std::nullopt; }
                    double digits = std::fabs(std::log10(std::max(std::fabs(a.toDouble()), 1e-300)) * n);
                    if (digits > 2000) return std::nullopt;
                    e.value = a.pow(n);
                    e.what = "Evaluate the power";
                    if (n >= 2 && n <= 6) {
                        std::string rep;
                        for (long long i = 0; i < n; ++i) rep += (i ? " \\cdot " : "") + (a.isNeg() || !a.isInteger() ? "\\left(" + ratTex(a) + "\\right)" : ratTex(a));
                        e.detail.push_back({"", "Multiply the base by itself " + m(std::to_string(n)) + " times.", rep + " = " + ratTex(e.value), {}});
                    } else if (n < 0) {
                        e.detail.push_back({"", m("a^{-n} = \\frac{1}{a^{n}}"), "\\frac{1}{" + ratTex(a) + "^{" + std::to_string(-n) + "}} = " + ratTex(e.value), {}});
                    } else if (n == 0) {
                        e.detail.push_back({"", "Any non-zero number to the power 0 is 1.", "", {}});
                    }
                    return e;
                }
                E s = simplify(pow(num(a), num(b)));
                if (!isNum(s)) return std::nullopt;
                e.value = s->num;
                e.what = "Evaluate the power";
                e.detail.push_back({"", m("a^{\\frac{p}{q}} = \\sqrt[q]{a^{p}}"), "", {}});
                return e;
            }
            return std::nullopt;
        }
        default: return std::nullopt;
    }
}

struct Pending {
    R* slot;
    int cls;
};

void collect(R& r, std::vector<Pending>& out) {
    if (r->t == T::Num || r->t == T::Sym) return;
    if (allNumKids(r)) { out.push_back({&r, opClass(r)}); return; }
    for (auto& k : r->kids) collect(k, out);
}

// numbers that must be parenthesised when they replace a node
void fixParen(R& slot, bool root) {
    if (root) { slot->paren = false; return; }
    slot->paren = slot->value.isNeg() || false;
}

std::string titleFor(const std::vector<std::string>& verbs, int cls) {
    bool same = true;
    for (auto& v : verbs) same = same && v == verbs[0];
    if (cls == 5 && verbs.size() == 1) return verbs[0] + " inside the parentheses";
    if (same) return verbs[0];
    if (cls == 3) return "Multiply and divide from left to right";
    if (cls == 2) return "Add and subtract from left to right";
    return "Evaluate";
}

}  // namespace

Solution solveArithmetic(const R& raw, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "arithmetic";
    sol.anim = "arithmetic";
    sol.title = "Calculate";
    sol.inputLatex = inputLatex;
    R tree = clone(raw);
    tree->paren = false;
    Method mm{"Order of operations", {}};
    std::string err;
    for (int guard = 0; guard < 200; ++guard) {
        if (tree->t == T::Num) break;
        std::vector<Pending> pend;
        collect(tree, pend);
        if (pend.empty()) break;
        int best = 0;
        for (auto& p : pend) best = std::max(best, p.cls);
        std::vector<std::string> verbs;
        std::vector<Step> details;
        bool any = false;
        for (auto& p : pend) {
            if (p.cls != best) continue;
            // left-to-right: for + - * / only the leftmost of each class to keep order clear
            auto ev = evalNode(*p.slot, err);
            if (!err.empty()) break;
            if (!ev) continue;
            verbs.push_back(ev->what);
            for (auto& d : ev->detail) details.push_back(d);
            bool isRoot = p.slot == &tree;
            bool wasParen = (*p.slot)->paren;
            *p.slot = numNode(ev->value);
            (void)wasParen;
            fixParen(*p.slot, isRoot);
            any = true;
            if (best == 2 || best == 3) break;  // one operation at a time for left-to-right classes
        }
        if (!err.empty()) break;
        if (!any) {
            // try lower classes
            bool progressed = false;
            for (auto& p : pend) {
                auto ev = evalNode(*p.slot, err);
                if (!err.empty()) break;
                if (!ev) continue;
                verbs.push_back(ev->what);
                for (auto& d : ev->detail) details.push_back(d);
                bool isRoot = p.slot == &tree;
                *p.slot = numNode(ev->value);
                fixParen(*p.slot, isRoot);
                progressed = true;
                best = p.cls;
                break;
            }
            if (!progressed) break;
        }
        Step s{titleFor(verbs, best), "", rawLatex(tree), details};
        mm.steps.push_back(s);
    }
    if (!err.empty()) {
        sol.ok = false;
        sol.error = err;
        return sol;
    }
    E result;
    if (tree->t == T::Num) {
        result = num(tree->value);
    } else {
        auto e = toExpr(tree);
        if (!e) { sol.ok = false; sol.error = e.error().message; return sol; }
        result = simplify(*e);
        if (isUndefined(result)) { sol.ok = false; sol.error = "The expression is undefined."; return sol; }
        mm.steps.push_back({"Simplify", "Use exact values and simplify the radicals.", toLatex(result), {}});
    }
    if (mm.steps.empty()) mm.steps.push_back({"The expression is already a number", "", toLatex(result), {}});
    sol.methods.push_back(mm);
    sol.answerLatex = toLatex(result);
    sol.answers.push_back(sol.answerLatex);
    if (isNum(result)) {
        const Rational& v = result->num;
        if (!v.isInteger()) {
            sol.facts.push_back({"Decimal", v.toDecimalString(12)});
            if (v.abs() > Rational(1)) {
                BigInt whole = v.abs().floor();
                Rational rest = v.abs() - Rational(whole);
                sol.facts.push_back({"Mixed number", std::string(v.isNeg() ? "-" : "") + whole.toString() + "\\frac{" + rest.num().toString() + "}{" + rest.den().toString() + "}"});
            }
        } else if (v.num().digits() > 12) {
            sol.facts.push_back({"Scientific notation", numLatex(v.toDouble(), 10)});
            sol.facts.push_back({"Digits", std::to_string(v.num().abs().digits())});
        }
    } else if (auto d = evalReal(result)) {
        sol.facts.push_back({"Decimal", numLatex(*d, 12)});
    }
    return sol;
}

}  // namespace me
