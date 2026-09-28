// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Descriptive statistics and quantitative-finance primitives, with worked steps.
#include <algorithm>
#include <cmath>
#include <numeric>

#include "solvers.h"

namespace me {

namespace {

std::string f6(double v) { return numLatex(v, 6); }
std::string list(const std::vector<double>& xs) {
    std::string s;
    for (size_t i = 0; i < xs.size(); ++i) s += (i ? ", " : "") + fmtDouble(xs[i], 8);
    return s;
}

Solution base(const std::string& kind, const std::string& title, const std::string& input) {
    Solution s;
    s.kind = kind;
    s.title = title;
    s.inputLatex = input;
    return s;
}

double normCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }
double normPdf(double x) { return std::exp(-x * x / 2) / std::sqrt(2 * M_PI); }

}  // namespace

// ------------------------------------------------------------------ statistics

Solution solveStatistics(const std::string& fn, const std::vector<double>& xs, const std::string& inputLatex) {
    Solution sol = base("statistics", "Describe the data", inputLatex);
    if (xs.empty()) { sol.ok = false; sol.error = "Give at least one number."; return sol; }
    std::vector<double> s = xs;
    std::ranges::sort(s);
    size_t n = s.size();
    double sum = std::accumulate(s.begin(), s.end(), 0.0);
    double mean = sum / n;
    double median = n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2;
    double ss = 0;
    for (double v : s) ss += (v - mean) * (v - mean);
    double varP = ss / n, varS = n > 1 ? ss / (n - 1) : 0;
    // mode(s)
    std::vector<double> modes;
    size_t best = 1;
    for (size_t i = 0; i < n;) {
        size_t j = i;
        while (j < n && s[j] == s[i]) ++j;
        if (j - i > best) { best = j - i; modes = {s[i]}; } else if (j - i == best && best > 1) modes.push_back(s[i]);
        i = j;
    }
    Method meth{"Statistics", {}};
    meth.steps.push_back({"Order the data", m(std::to_string(n)) + " values, smallest to largest.", list(s), {}});
    meth.steps.push_back({"Mean", "Add everything and divide by the count.", "\\bar{x} = \\frac{" + fmtDouble(sum, 10) + "}{" + std::to_string(n) + "} = " + f6(mean), {}});
    meth.steps.push_back({"Median", n % 2 ? "The middle value." : "The average of the two middle values.", "\\tilde{x} = " + f6(median), {}});
    meth.steps.push_back({"Mode", modes.empty() ? "No value repeats." : "The most frequent value.", modes.empty() ? "\\text{none}" : list(modes), {}});
    meth.steps.push_back({"Spread", "Variance is the average squared distance from the mean; the standard deviation is its square root.",
                       "\\sigma^{2} = " + f6(varP) + ", \\quad \\sigma = " + f6(std::sqrt(varP)) + ", \\quad s = " + f6(std::sqrt(varS)), {}});
    sol.methods.push_back(meth);
    auto set = [&](const std::string& l, double v) { sol.answerLatex = l + " = " + f6(v); sol.answers.push_back(sol.answerLatex); };
    if (fn == "mean") set("\\bar{x}", mean);
    else if (fn == "median") set("\\tilde{x}", median);
    else if (fn == "mode") { sol.answerLatex = modes.empty() ? "\\text{No mode}" : "\\text{mode} = " + list(modes); sol.answers.push_back(sol.answerLatex); }
    else if (fn == "variance") set("s^{2}", varS);
    else if (fn == "stdev") set("s", std::sqrt(varS));
    else { sol.answerLatex = "\\bar{x} = " + f6(mean) + ", \\quad \\tilde{x} = " + f6(median) + ", \\quad s = " + f6(std::sqrt(varS)); sol.answers.push_back(sol.answerLatex); }
    sol.facts = {{"Count", std::to_string(n)}, {"Sum", fmtDouble(sum, 10)}, {"Min", fmtDouble(s.front(), 8)}, {"Max", fmtDouble(s.back(), 8)},
                 {"Range", fmtDouble(s.back() - s.front(), 8)}, {"Population σ", f6(std::sqrt(varP))}};
    // dot plot as points on a line
    PlotSpec ps;
    std::map<double, int> stack;
    for (double v : s) { int k = ++stack[v]; ps.points.push_back({v, (double)k, ""}); }
    ps.points.push_back({mean, 0.0, "\\bar{x}"});
    ps.xmin = s.front() - 1; ps.xmax = s.back() + 1; ps.ymin = -1; ps.ymax = best + 2; ps.autoRange = false;
    sol.plot = ps;
    return sol;
}

// ------------------------------------------------------------------ quantitative finance

Solution solveFinance(const std::string& fn, const std::vector<double>& a, const std::string& inputLatex) {
    Solution sol = base("finance", "Quantitative finance", inputLatex);
    Method meth{"Formula", {}};
    auto need = [&](size_t k, const char* usage) {
        if (a.size() < k) { sol.ok = false; sol.error = std::string("Usage: ") + usage; return false; }
        return true;
    };
    // balance over time, for the Explain animation
    auto growth = [&](double p0, double r, double n, bool continuous) {
        PlotSpec ps;
        std::string expr = continuous ? fmtDouble(p0, 12) + "*e^(" + fmtDouble(r, 10) + "*x)" : fmtDouble(p0, 12) + "*(1+" + fmtDouble(r, 10) + ")^x";
        ps.curves.push_back({PlotCurve::Type::Explicit, expr, "\\text{balance}"});
        double end = continuous ? p0 * std::exp(r * n) : p0 * std::pow(1 + r, n);
        ps.points.push_back({0.0, p0, "\\text{start}"});
        ps.points.push_back({n, end, "\\text{end}"});
        ps.xmin = -n * 0.08; ps.xmax = n * 1.1; ps.ymin = -end * 0.05; ps.ymax = end * 1.15; ps.autoRange = false;
        sol.plot = ps;
    };
    auto answer = [&](const std::string& l, double v) {
        sol.answerLatex = l + " = " + numLatex(v, 8);
        sol.answers.push_back(sol.answerLatex);
    };
    if (fn == "fv") {  // fv(rate, periods, pv[, pmt])
        if (!need(3, "fv(rate, periods, present value [, payment])")) return sol;
        double r = a[0], n = a[1], pv = a[2], pmt = a.size() > 3 ? a[3] : 0;
        double g = std::pow(1 + r, n);
        double fv = pv * g + (r == 0 ? pmt * n : pmt * (g - 1) / r);
        sol.title = "Future value";
        meth.steps.push_back({"Compound growth", "Money grows by a factor " + m("(1+r)") + " each period.", "FV = PV(1+r)^{n}" + std::string(pmt ? " + PMT\\frac{(1+r)^{n}-1}{r}" : ""), {}});
        meth.steps.push_back({"Substitute", "", "FV = " + fmtDouble(pv, 10) + "(1 + " + fmtDouble(r, 6) + ")^{" + fmtDouble(n, 6) + "}" + (pmt ? " + \\dots" : ""), {}});
        meth.steps.push_back({"Growth factor", "", "(1+r)^{n} = " + numLatex(g, 8), {}});
        answer("FV", fv);
        growth(pv, r, n, false);
    } else if (fn == "pv") {  // pv(rate, periods, fv)
        if (!need(3, "pv(rate, periods, future value)")) return sol;
        double r = a[0], n = a[1], fv = a[2];
        sol.title = "Present value";
        meth.steps.push_back({"Discount", "A future amount is worth less today: divide by the growth factor.", "PV = \\frac{FV}{(1+r)^{n}}", {}});
        meth.steps.push_back({"Substitute", "", "PV = \\frac{" + fmtDouble(fv, 10) + "}{(1 + " + fmtDouble(r, 6) + ")^{" + fmtDouble(n, 6) + "}}", {}});
        answer("PV", fv / std::pow(1 + r, n));
        growth(fv / std::pow(1 + r, n), r, n, false);
    } else if (fn == "cont") {  // continuous compounding cont(P, r, t)
        if (!need(3, "cont(principal, rate, years)")) return sol;
        sol.title = "Continuous compounding";
        meth.steps.push_back({"Compounding infinitely often", "The limit of " + m("(1 + r/n)^{nt}") + " as " + m("n \\to \\infty") + " is " + m("e^{rt}") + ".", "A = Pe^{rt}", {}});
        answer("A", a[0] * std::exp(a[1] * a[2]));
        growth(a[0], a[1], a[2], true);
    } else if (fn == "pmt") {  // pmt(rate, periods, principal)
        if (!need(3, "pmt(rate per period, periods, principal)")) return sol;
        double r = a[0], n = a[1], P = a[2];
        double pay = r == 0 ? P / n : P * r / (1 - std::pow(1 + r, -n));
        sol.title = "Loan payment";
        meth.steps.push_back({"Amortisation formula", "Equal payments whose present values add up to the principal.", "PMT = \\frac{P\\,r}{1 - (1+r)^{-n}}", {}});
        meth.steps.push_back({"Substitute", "", "PMT = \\frac{" + fmtDouble(P, 10) + " \\cdot " + fmtDouble(r, 6) + "}{1 - (1 + " + fmtDouble(r, 6) + ")^{-" + fmtDouble(n, 6) + "}}", {}});
        answer("PMT", pay);
        sol.facts = {{"Total paid", numLatex(pay * n, 8)}, {"Total interest", numLatex(pay * n - P, 8)}};
    } else if (fn == "npv") {  // npv(rate, cf0, cf1, ...)
        if (!need(2, "npv(rate, cf0, cf1, ...)")) return sol;
        double r = a[0], v = 0;
        Step s{"Discount each cash flow", "", "", {}};
        for (size_t k = 1; k < a.size(); ++k) {
            double d = a[k] / std::pow(1 + r, (double)(k - 1));
            v += d;
            s.sub.push_back({"", "", "\\frac{" + fmtDouble(a[k], 10) + "}{(1+" + fmtDouble(r, 6) + ")^{" + std::to_string(k - 1) + "}} = " + numLatex(d, 8), {}});
        }
        sol.title = "Net present value";
        meth.steps.push_back({"Definition", "Sum of all cash flows, each discounted back to today.", "NPV = \\sum_{t} \\frac{CF_{t}}{(1+r)^{t}}", {}});
        meth.steps.push_back(s);
        answer("NPV", v);
        sol.facts = {{"Decision", v >= 0 ? "\\text{Accept (adds value)}" : "\\text{Reject (destroys value)}"}};
    } else if (fn == "irr") {  // irr(cf0, cf1, ...)
        if (!need(2, "irr(cf0, cf1, ...)")) return sol;
        auto npv = [&](double r) { double v = 0; for (size_t k = 0; k < a.size(); ++k) v += a[k] / std::pow(1 + r, (double)k); return v; };
        double lo = -0.99, hi = 10;
        if (npv(lo) * npv(hi) > 0) { sol.ok = false; sol.error = "No internal rate of return: the cash flows never change sign."; return sol; }
        for (int it = 0; it < 200; ++it) { double mid = (lo + hi) / 2; ((npv(lo) < 0) == (npv(mid) < 0) ? lo : hi) = mid; }
        double r = (lo + hi) / 2;
        sol.title = "Internal rate of return";
        meth.steps.push_back({"Definition", "The rate that makes the net present value exactly zero.", "\\sum_{t} \\frac{CF_{t}}{(1+IRR)^{t}} = 0", {}});
        meth.steps.push_back({"Solve numerically", "Bisection on the rate until NPV crosses zero.", "NPV(" + numLatex(r, 6) + ") \\approx 0", {}});
        answer("IRR", r);
        sol.facts = {{"As a percentage", numLatex(r * 100, 6) + "\\%"}};
    } else if (fn == "cagr") {  // cagr(start, end, years)
        if (!need(3, "cagr(start value, end value, years)")) return sol;
        double g = std::pow(a[1] / a[0], 1.0 / a[2]) - 1;
        sol.title = "Compound annual growth rate";
        meth.steps.push_back({"Definition", "The constant yearly rate that turns the start value into the end value.", "CAGR = \\left(\\frac{V_{end}}{V_{start}}\\right)^{1/t} - 1", {}});
        answer("CAGR", g);
        sol.facts = {{"As a percentage", numLatex(g * 100, 6) + "\\%"}};
    } else if (fn == "bond") {  // bond(face, coupon rate, yield, years[, freq])
        if (!need(4, "bond(face, coupon rate, yield, years [, payments per year])")) return sol;
        double F = a[0], c = a[1], y = a[2], T = a[3], k = a.size() > 4 ? a[4] : 1;
        double n = T * k, cpn = F * c / k, yy = y / k;
        double price = cpn * (1 - std::pow(1 + yy, -n)) / yy + F * std::pow(1 + yy, -n);
        sol.title = "Bond price";
        meth.steps.push_back({"Price = present value of coupons + face value", "", "P = C\\frac{1-(1+y)^{-n}}{y} + \\frac{F}{(1+y)^{n}}", {}});
        meth.steps.push_back({"Per-period values", "", "C = " + numLatex(cpn, 8) + ", \\quad y = " + numLatex(yy, 6) + ", \\quad n = " + fmtDouble(n, 6), {}});
        answer("P", price);
        sol.facts = {{"Trades at", price > F ? "\\text{a premium}" : (price < F ? "\\text{a discount}" : "\\text{par}")}};
    } else if (fn == "sharpe") {  // sharpe(return, riskfree, stdev)
        if (!need(3, "sharpe(expected return, risk-free rate, volatility)")) return sol;
        sol.title = "Sharpe ratio";
        meth.steps.push_back({"Excess return per unit of risk", "", "S = \\frac{R_{p} - R_{f}}{\\sigma_{p}}", {}});
        answer("S", (a[0] - a[1]) / a[2]);
    } else if (fn == "normcdf") {
        if (!need(1, "normcdf(x)")) return sol;
        sol.title = "Standard normal CDF";
        meth.steps.push_back({"Area under the bell curve to the left of x", "", "\\Phi(x) = \\frac{1}{2}\\left(1 + \\operatorname{erf}\\frac{x}{\\sqrt{2}}\\right)", {}});
        answer("\\Phi(" + fmtDouble(a[0], 6) + ")", normCdf(a[0]));
        PlotSpec ps;
        ps.curves.push_back({PlotCurve::Type::Explicit, "e^(-x^2/2)/sqrt(2*pi)", "\\varphi(x)"});
        ps.shade = std::pair{-6.0, a[0]};
        ps.xmin = -4; ps.xmax = 4; ps.ymin = -0.1; ps.ymax = 0.5;
        sol.plot = ps;
    } else if (fn == "bscall" || fn == "bsput") {  // bscall(S, K, T, r, sigma)
        if (!need(5, "bscall(spot, strike, years, rate, volatility)")) return sol;
        double S = a[0], K = a[1], T = a[2], r = a[3], v = a[4];
        double d1 = (std::log(S / K) + (r + v * v / 2) * T) / (v * std::sqrt(T)), d2 = d1 - v * std::sqrt(T);
        bool call = fn == "bscall";
        double price = call ? S * normCdf(d1) - K * std::exp(-r * T) * normCdf(d2) : K * std::exp(-r * T) * normCdf(-d2) - S * normCdf(-d1);
        sol.title = call ? "Black–Scholes call price" : "Black–Scholes put price";
        meth.steps.push_back({"Compute d₁ and d₂", "", "d_{1} = \\frac{\\ln(S/K) + (r + \\sigma^{2}/2)T}{\\sigma\\sqrt{T}} = " + numLatex(d1, 6) + ", \\quad d_{2} = d_{1} - \\sigma\\sqrt{T} = " + numLatex(d2, 6), {}});
        meth.steps.push_back({"Normal probabilities", "", "\\Phi(d_{1}) = " + numLatex(normCdf(d1), 6) + ", \\quad \\Phi(d_{2}) = " + numLatex(normCdf(d2), 6), {}});
        meth.steps.push_back({"Price", call ? "Expected payoff under the risk-neutral measure, discounted." : "Put price via the same probabilities.",
                           call ? "C = S\\,\\Phi(d_{1}) - Ke^{-rT}\\Phi(d_{2})" : "P = Ke^{-rT}\\Phi(-d_{2}) - S\\,\\Phi(-d_{1})", {}});
        answer(call ? "C" : "P", price);
        double delta = call ? normCdf(d1) : normCdf(d1) - 1;
        double gamma = normPdf(d1) / (S * v * std::sqrt(T));
        double vega = S * normPdf(d1) * std::sqrt(T) / 100;
        double theta = (call ? (-S * normPdf(d1) * v / (2 * std::sqrt(T)) - r * K * std::exp(-r * T) * normCdf(d2))
                             : (-S * normPdf(d1) * v / (2 * std::sqrt(T)) + r * K * std::exp(-r * T) * normCdf(-d2))) / 365;
        sol.facts = {{"Delta Δ", numLatex(delta, 5)}, {"Gamma Γ", numLatex(gamma, 5)}, {"Vega (per 1%)", numLatex(vega, 5)}, {"Theta (per day)", numLatex(theta, 5)}};
        // payoff vs price curve over spot
        PlotSpec ps;
        std::string payoff = call ? "(abs(x-" + fmtDouble(K, 10) + ")+(x-" + fmtDouble(K, 10) + "))/2" : "(abs(" + fmtDouble(K, 10) + "-x)+(" + fmtDouble(K, 10) + "-x))/2";
        ps.curves.push_back({PlotCurve::Type::Explicit, payoff, "\\text{payoff at expiry}"});
        ps.points.push_back({S, price, "\\text{today}"});
        ps.xmin = K * 0.4; ps.xmax = K * 1.6; ps.ymin = -K * 0.05; ps.ymax = K * 0.6; ps.autoRange = false;
        sol.plot = ps;
    } else if (fn == "bsmcall" || fn == "bsmput") {  // Black-Scholes-Merton with dividend yield q
        if (!need(6, "bsmcall(spot, strike, years, rate, volatility, dividend yield)")) return sol;
        double S = a[0], K = a[1], T = a[2], r = a[3], v = a[4], q = a[5];
        double d1 = (std::log(S / K) + (r - q + v * v / 2) * T) / (v * std::sqrt(T)), d2 = d1 - v * std::sqrt(T);
        bool call = fn == "bsmcall";
        double price = call ? S * std::exp(-q * T) * normCdf(d1) - K * std::exp(-r * T) * normCdf(d2)
                            : K * std::exp(-r * T) * normCdf(-d2) - S * std::exp(-q * T) * normCdf(-d1);
        sol.title = call ? "Black–Scholes–Merton call" : "Black–Scholes–Merton put";
        meth.steps.push_back({"Merton's extension", "A continuous dividend yield " + m("q") + " lowers the forward price: replace " + m("S") + " by " + m("Se^{-qT}") + ".",
                              "d_{1} = \\frac{\\ln(S/K) + (r - q + \\sigma^{2}/2)T}{\\sigma\\sqrt{T}} = " + numLatex(d1, 6) + ", \\quad d_{2} = " + numLatex(d2, 6), {}});
        meth.steps.push_back({"Price", "", call ? "C = Se^{-qT}\\Phi(d_{1}) - Ke^{-rT}\\Phi(d_{2})" : "P = Ke^{-rT}\\Phi(-d_{2}) - Se^{-qT}\\Phi(-d_{1})", {}});
        answer(call ? "C" : "P", price);
        sol.facts = {{"Delta Δ", numLatex(call ? std::exp(-q * T) * normCdf(d1) : std::exp(-q * T) * (normCdf(d1) - 1), 5)}};
    } else if (fn == "merton") {  // Merton (1976) jump-diffusion call: merton(S,K,T,r,sigma,lambda,muJ,sigmaJ)
        if (!need(8, "merton(spot, strike, years, rate, volatility, jump intensity, mean jump, jump volatility)")) return sol;
        double S = a[0], K = a[1], T = a[2], r = a[3], v = a[4], lam = a[5], muJ = a[6], sJ = a[7];
        double kbar = std::exp(muJ + sJ * sJ / 2) - 1;
        double lamP = lam * (1 + kbar);
        double price = 0, weight = std::exp(-lamP * T);
        Step terms{"Sum over the number of jumps", "Weight each Black–Scholes price by the Poisson probability of " + m("n") + " jumps.", "", {}};
        for (int n = 0; n < 60; ++n) {
            if (n > 0) weight *= lamP * T / n;
            double vn = std::sqrt(v * v + n * sJ * sJ / T);
            double rn = r - lam * kbar + n * (muJ + sJ * sJ / 2) / T;
            double d1 = (std::log(S / K) + (rn + vn * vn / 2) * T) / (vn * std::sqrt(T)), d2 = d1 - vn * std::sqrt(T);
            double bs = S * normCdf(d1) - K * std::exp(-rn * T) * normCdf(d2);
            price += weight * bs;
            if (n < 5) terms.sub.push_back({"", "", "n = " + std::to_string(n) + ": \\quad P(N=n) = " + numLatex(weight, 5) + ", \\ C_{n} = " + numLatex(bs, 6), {}});
            if (n > 5 && weight < 1e-12) break;
        }
        sol.title = "Merton jump-diffusion call";
        meth.steps.push_back({"Jumps on top of diffusion", "The price follows a random walk plus sudden log-normal jumps arriving at rate " + m("\\lambda") + ".",
                              "C = \\sum_{n=0}^{\\infty} \\frac{e^{-\\lambda' T}(\\lambda' T)^{n}}{n!} \\, C_{BS}(\\sigma_{n}, r_{n})", {}});
        meth.steps.push_back({"Adjusted parameters", "", "\\bar{k} = e^{\\mu_{J} + \\sigma_{J}^{2}/2} - 1 = " + numLatex(kbar, 5) + ", \\quad \\lambda' = \\lambda(1+\\bar{k}) = " + numLatex(lamP, 5), {}});
        meth.steps.push_back(terms);
        answer("C", price);
    } else if (fn == "credit") {  // Merton credit model: credit(assets, debt face, years, rate, asset volatility)
        if (!need(5, "credit(asset value, debt face value, years, rate, asset volatility)")) return sol;
        double V = a[0], D = a[1], T = a[2], r = a[3], v = a[4];
        double d1 = (std::log(V / D) + (r + v * v / 2) * T) / (v * std::sqrt(T)), d2 = d1 - v * std::sqrt(T);
        double E = V * normCdf(d1) - D * std::exp(-r * T) * normCdf(d2);
        double debt = V - E;
        double y = -std::log(debt / D) / T;
        sol.title = "Merton credit model";
        meth.steps.push_back({"Equity is a call option on the firm", "Shareholders receive " + m("\\max(V_{T} - D, 0)") + " at maturity; below " + m("D") + " the firm defaults.",
                              "E = V\\Phi(d_{1}) - De^{-rT}\\Phi(d_{2})", {}});
        meth.steps.push_back({"Distance to default", "", "d_{1} = " + numLatex(d1, 5) + ", \\quad d_{2} = " + numLatex(d2, 5), {}});
        meth.steps.push_back({"Default probability", "The risk-neutral chance the assets end below the debt.", "P(\\text{default}) = \\Phi(-d_{2}) = " + numLatex(normCdf(-d2), 5), {}});
        answer("E", E);
        sol.facts = {{"Debt value", numLatex(debt, 8)}, {"Default probability", numLatex(normCdf(-d2), 5)},
                     {"Credit spread", numLatex((y - r) * 10000, 5) + "\\text{ bp}"}};
    } else if (fn == "binomial") {  // CRR tree: binomial(S,K,T,r,sigma,steps[,type]) type 0 EU call,1 EU put,2 US put
        if (!need(6, "binomial(spot, strike, years, rate, volatility, steps [, 0 call | 1 put | 2 American put])")) return sol;
        double S = a[0], K = a[1], T = a[2], r = a[3], v = a[4];
        int N = (int)std::clamp(a[5], 1.0, 2000.0), type = a.size() > 6 ? (int)a[6] : 0;
        double dt = T / N, u = std::exp(v * std::sqrt(dt)), dd = 1 / u, p = (std::exp(r * dt) - dd) / (u - dd), disc = std::exp(-r * dt);
        std::vector<double> val(N + 1);
        for (int j = 0; j <= N; ++j) {
            double ST = S * std::pow(u, j) * std::pow(dd, N - j);
            val[j] = type == 0 ? std::max(ST - K, 0.0) : std::max(K - ST, 0.0);
        }
        for (int i = N - 1; i >= 0; --i)
            for (int j = 0; j <= i; ++j) {
                double cont = disc * (p * val[j + 1] + (1 - p) * val[j]);
                if (type == 2) cont = std::max(cont, K - S * std::pow(u, j) * std::pow(dd, i - j));
                val[j] = cont;
            }
        sol.title = type == 2 ? "American put (binomial tree)" : type == 1 ? "European put (binomial tree)" : "European call (binomial tree)";
        meth.steps.push_back({"Build the tree", "Each step the price moves up by " + m("u") + " or down by " + m("d = 1/u") + ".",
                              "u = e^{\\sigma\\sqrt{\\Delta t}} = " + numLatex(u, 6) + ", \\quad p = \\frac{e^{r\\Delta t} - d}{u - d} = " + numLatex(p, 6), {}});
        meth.steps.push_back({"Work backwards", type == 2 ? "At every node keep the larger of holding and exercising early." : "Discount the expected value one step at a time.",
                              "V = e^{-r\\Delta t}\\left(p V_{u} + (1-p) V_{d}\\right)", {}});
        answer("V", val[0]);
        sol.facts = {{"Steps", std::to_string(N)}};
    } else if (fn == "ivol") {  // implied volatility: ivol(price, S, K, T, r [, 1 = put])
        if (!need(5, "ivol(option price, spot, strike, years, rate [, 1 for put])")) return sol;
        double P = a[0], S = a[1], K = a[2], T = a[3], r = a[4];
        bool put = a.size() > 5 && a[5] > 0.5;
        auto bs = [&](double v) {
            double d1 = (std::log(S / K) + (r + v * v / 2) * T) / (v * std::sqrt(T)), d2 = d1 - v * std::sqrt(T);
            return put ? K * std::exp(-r * T) * normCdf(-d2) - S * normCdf(-d1) : S * normCdf(d1) - K * std::exp(-r * T) * normCdf(d2);
        };
        double v = 0.2;
        Step it{"Newton's method", "Adjust " + m("\\sigma") + " by (model − market) / vega until prices match.", "", {}};
        for (int i = 0; i < 50; ++i) {
            double d1 = (std::log(S / K) + (r + v * v / 2) * T) / (v * std::sqrt(T));
            double vega = S * normPdf(d1) * std::sqrt(T);
            double diff = bs(v) - P;
            if (i < 6) it.sub.push_back({"", "", "\\sigma_{" + std::to_string(i) + "} = " + numLatex(v, 6) + ", \\quad C(\\sigma) - C_{mkt} = " + numLatex(diff, 5), {}});
            if (std::fabs(diff) < 1e-10 || vega < 1e-12) break;
            v = std::clamp(v - diff / vega, 1e-4, 5.0);
        }
        if (std::fabs(bs(v) - P) > 1e-6) { sol.ok = false; sol.error = "No volatility reproduces that price (check it is within no-arbitrage bounds)."; return sol; }
        sol.title = "Implied volatility";
        meth.steps.push_back({"Invert Black–Scholes", "Find the volatility that makes the model price equal the market price.", "C_{BS}(\\sigma) = " + numLatex(P, 8), {}});
        meth.steps.push_back(it);
        answer("\\sigma_{implied}", v);
        sol.facts = {{"As a percentage", numLatex(v * 100, 5) + "\\%"}};
    } else {
        sol.ok = false;
        sol.error = "Unknown finance function.";
        return sol;
    }
    sol.methods.push_back(meth);
    return sol;
}

bool isFinanceFn(const std::string& n) {
    static const char* fns[] = {"fv", "pv", "pmt", "npv", "irr", "cagr", "bond", "sharpe", "normcdf", "bscall", "bsput", "cont",
                                "bsmcall", "bsmput", "merton", "credit", "binomial", "ivol"};
    for (auto f : fns) if (n == f) return true;
    return false;
}

bool isStatsFn(const std::string& n) {
    return n == "mean" || n == "median" || n == "mode" || n == "stdev" || n == "variance" || n == "stats";
}

}  // namespace me
