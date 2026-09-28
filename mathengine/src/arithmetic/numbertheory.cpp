// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Factorials, prime factorization, GCD/LCM (Euclid), combinations and permutations.
#include <algorithm>
#include <cmath>

#include "solvers.h"

namespace me {

Solution solveFactorial(const E& n, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "factorial";
    sol.anim = "factorial";
    sol.title = "Calculate the factorial";
    sol.inputLatex = inputLatex;
    Method mm{"Definition", {}};
    if (!isNum(n) || !n->num.isInteger() || n->num.isNeg()) {
        auto v = evalReal(n);
        if (!v) { sol.ok = false; sol.error = "Factorial needs a number."; return sol; }
        if (*v < 0 && std::fabs(*v - std::round(*v)) < 1e-12) { sol.ok = false; sol.error = "The factorial of a negative integer is undefined."; return sol; }
        double g = std::tgamma(*v + 1);
        mm.steps.push_back({"Use the Gamma function", "For non-integers the factorial extends to " + m("x! = \\Gamma(x + 1)") + ".",
                            toLatex(func("fact", n)) + " = \\Gamma\\left(" + fmtDouble(*v + 1, 8) + "\\right) \\approx " + numLatex(g, 10), {}});
        sol.methods.push_back(mm);
        sol.answerLatex = toLatex(func("fact", n)) + " \\approx " + numLatex(g, 10);
        sol.answers.push_back(numLatex(g, 10));
        return sol;
    }
    long long k = n->num.num().toInt64();
    if (k > 3000) { sol.ok = false; sol.error = "That factorial is too large to compute exactly."; return sol; }
    mm.steps.push_back({"Use the definition", m("n! = n \\cdot (n-1) \\cdot (n-2) \\cdots 2 \\cdot 1"), "", {}});
    std::string prod;
    if (k == 0) {
        mm.steps.push_back({"By convention", m("0! = 1") + " (there is exactly one way to arrange nothing).", "0! = 1", {}});
    } else {
        if (k <= 12) for (long long i = k; i >= 1; --i) prod += (i == k ? "" : " \\cdot ") + std::to_string(i);
        else prod = std::to_string(k) + " \\cdot " + std::to_string(k - 1) + " \\cdot " + std::to_string(k - 2) + " \\cdots 2 \\cdot 1";
        mm.steps.push_back({"Write out the product", "", std::to_string(k) + "! = " + prod, {}});
        // running products for small n
        if (k <= 10 && k >= 3) {
            Step s{"Multiply step by step", "", "", {}};
            BigInt acc(k);
            for (long long i = k - 1; i >= 2; --i) {
                BigInt next = acc * BigInt(i);
                s.sub.push_back({"", "", acc.toString() + " \\cdot " + std::to_string(i) + " = " + next.toString(), {}});
                acc = next;
            }
            mm.steps.push_back(s);
        }
    }
    E r = simplify(func("fact", n));
    mm.steps.push_back({"Result", "", std::to_string(k) + "! = " + toLatex(r), {}});
    sol.methods.push_back(mm);
    sol.answerLatex = std::to_string(k) + "! = " + toLatex(r);
    sol.answers.push_back(toLatex(r));
    if (isNum(r) && r->num.num().digits() > 12) {
        sol.facts.push_back({"Scientific notation", numLatex(r->num.toDouble(), 8)});
        sol.facts.push_back({"Number of digits", std::to_string(r->num.num().digits())});
    }
    // trailing zeros: count factors of 5
    long long z = 0;
    for (long long p = 5; p <= k; p *= 5) z += k / p;
    if (k >= 5) sol.facts.push_back({"Trailing zeros", std::to_string(z)});
    return sol;
}

Solution solvePrimeFactorization(const BigInt& n0, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "numbertheory";
    sol.anim = "factorial";
    sol.title = "Prime factorization";
    sol.inputLatex = inputLatex;
    BigInt n = n0.abs();
    if (n < BigInt(2)) {
        sol.ok = false;
        sol.error = "Prime factorization needs an integer greater than 1.";
        return sol;
    }
    Method mm{"Repeated division", {}};
    std::vector<std::pair<BigInt, int>> f;
    BigInt cur = n;
    Step divs{"Divide by the smallest prime repeatedly", "Start with 2, then 3, 5, 7, … until the quotient is 1.", "", {}};
    for (long long p = 2; p < 2000000; ++p) {
        BigInt P(p);
        if (P * P > cur) break;
        while ((cur % P).isZero()) {
            BigInt q = cur / P;
            if (divs.sub.size() < 40) divs.sub.push_back({"", "", cur.toString() + " \\div " + std::to_string(p) + " = " + q.toString(), {}});
            cur = q;
            if (!f.empty() && f.back().first == P) f.back().second++;
            else f.push_back({P, 1});
        }
    }
    if (cur > BigInt(1)) {
        if (divs.sub.size() < 40) divs.sub.push_back({"", m(cur.toString()) + " is prime.", cur.toString() + " \\div " + cur.toString() + " = 1", {}});
        if (!f.empty() && f.back().first == cur) f.back().second++;
        else f.push_back({cur, 1});
    }
    mm.steps.push_back(divs);
    std::string prod, flat;
    for (auto& [p, e] : f) {
        prod += (prod.empty() ? "" : " \\cdot ") + p.toString() + (e > 1 ? "^{" + std::to_string(e) + "}" : "");
        for (int i = 0; i < e; ++i) flat += (flat.empty() ? "" : " \\cdot ") + p.toString();
    }
    mm.steps.push_back({"Collect the prime factors", "", n.toString() + " = " + flat, {}});
    mm.steps.push_back({"Write with exponents", "", n.toString() + " = " + prod, {}});
    sol.methods.push_back(mm);
    sol.answerLatex = n.toString() + " = " + prod;
    sol.answers.push_back(prod);
    long long nd = 1;
    for (auto& [p, e] : f) nd *= (e + 1);
    sol.facts.push_back({"Is prime", f.size() == 1 && f[0].second == 1 ? "\\text{Yes}" : "\\text{No}"});
    sol.facts.push_back({"Number of divisors", std::to_string(nd)});
    return sol;
}

Solution solveGcdLcm(const std::vector<BigInt>& nums, bool lcm, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "numbertheory";
    sol.anim = "arithmetic";
    sol.title = lcm ? "Least common multiple" : "Greatest common divisor";
    sol.inputLatex = inputLatex;
    Method eu{"Euclidean algorithm", {}};
    BigInt g = nums[0].abs();
    for (size_t i = 1; i < nums.size(); ++i) {
        BigInt a = g, b = nums[i].abs();
        Step s{"Find " + m("\\gcd(" + a.toString() + ", " + b.toString() + ")"), "Replace the larger number by the remainder until it is 0.", "", {}};
        if (a < b) std::swap(a, b);
        while (!b.isZero()) {
            BigInt q, r;
            BigInt::divmod(a, b, q, r);
            s.sub.push_back({"", "", a.toString() + " = " + q.toString() + " \\cdot " + b.toString() + " + " + r.toString(), {}});
            a = b;
            b = r;
        }
        s.latex = "\\gcd = " + a.toString();
        eu.steps.push_back(s);
        g = a;
    }
    BigInt result = g;
    if (lcm) {
        BigInt l = nums[0].abs();
        for (size_t i = 1; i < nums.size(); ++i) {
            BigInt b = nums[i].abs();
            BigInt gg = BigInt::gcd(l, b);
            BigInt nl = gg.isZero() ? BigInt(0) : l / gg * b;
            eu.steps.push_back({"Use " + m("\\operatorname{lcm}(a, b) = \\frac{a \\cdot b}{\\gcd(a, b)}"), "",
                                "\\operatorname{lcm}(" + l.toString() + ", " + b.toString() + ") = \\frac{" + l.toString() + " \\cdot " + b.toString() + "}{" + gg.toString() + "} = " + nl.toString(), {}});
            l = nl;
        }
        result = l;
    }
    sol.methods.push_back(eu);
    std::string args;
    for (size_t i = 0; i < nums.size(); ++i) args += (i ? ", " : "") + nums[i].toString();
    sol.answerLatex = (lcm ? "\\operatorname{lcm}" : "\\gcd") + std::string("\\left(") + args + "\\right) = " + result.toString();
    sol.answers.push_back(result.toString());
    return sol;
}

Solution solveCombinatorics(const BigInt& n, const BigInt& k, bool binom, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "combinatorics";
    sol.anim = "factorial";
    sol.title = binom ? "Combinations" : "Permutations";
    sol.inputLatex = inputLatex;
    if (k.isNeg() || n.isNeg() || k > n) {
        sol.answerLatex = "0";
        sol.answers.push_back("0");
        sol.methods.push_back({"Definition", {{"No selections possible", m("k > n") + ", so there are 0 ways.", "0", {}}}});
        return sol;
    }
    E r = simplify(func(binom ? "binom" : "perm", {num(Rational(n)), num(Rational(k))}));
    Method mm{"Formula", {}};
    std::string ns = n.toString(), ks = k.toString(), nk = (n - k).toString();
    if (binom) {
        mm.steps.push_back({"Use the combinations formula", "Order doesn't matter: " + m("\\binom{n}{k} = \\frac{n!}{k!\\,(n-k)!}"),
                            "\\binom{" + ns + "}{" + ks + "} = \\frac{" + ns + "!}{" + ks + "! \\cdot " + nk + "!}", {}});
    } else {
        mm.steps.push_back({"Use the permutations formula", "Order matters: " + m("P(n, k) = \\frac{n!}{(n-k)!}"),
                            "P(" + ns + ", " + ks + ") = \\frac{" + ns + "!}{" + nk + "!}", {}});
    }
    if (n.fitsInt64() && n.toInt64() <= 30) {
        long long nn = n.toInt64(), kk = k.toInt64();
        long long top = binom ? std::min(kk, nn - kk) : kk;
        std::string t;
        for (long long i = 0; i < top; ++i) t += (i ? " \\cdot " : "") + std::to_string(nn - i);
        if (top == 0) t = "1";
        if (binom) {
            std::string b;
            for (long long i = top; i >= 1; --i) b += (i == top ? "" : " \\cdot ") + std::to_string(i);
            mm.steps.push_back({"Cancel the common factorial", "", top ? "\\frac{" + t + "}{" + b + "}" : "1", {}});
        } else {
            mm.steps.push_back({"Cancel the common factorial", "", t, {}});
        }
    }
    mm.steps.push_back({"Calculate", "", toLatex(r), {}});
    sol.methods.push_back(mm);
    sol.answerLatex = (binom ? "\\binom{" + ns + "}{" + ks + "}" : "P(" + ns + ", " + ks + ")") + " = " + toLatex(r);
    sol.answers.push_back(toLatex(r));
    return sol;
}

}  // namespace me
