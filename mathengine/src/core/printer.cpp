// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// LaTeX and plain-text printers for the canonical expression tree.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/expr.h"

namespace me {

namespace {

enum Prec { P_ADD = 1, P_MUL = 2, P_POW = 3, P_ATOM = 4 };

std::string symLatex(const std::string& n) {
    static const char* greek[] = {"alpha", "beta", "gamma", "delta", "theta", "lambda", "mu", "phi", "omega", "sigma", "tau", "rho"};
    if (n == "pi") return "\\pi";
    if (n == "inf") return "\\infty";
    if (n == "undefined") return "\\text{undefined}";
    for (auto g : greek) if (n == g) return std::string("\\") + g;
    return n;
}

// "total degree" used for ordering terms in a sum
double termWeight(const E& t) {
    switch (t->kind) {
        case Kind::Num: return 0;
        case Kind::Sym: return (t->name == "pi" || t->name == "e" || t->name == "i") ? 0 : 1;
        case Kind::Pow: {
            if (!hasFreeVars(t)) return 0;
            if (isNum(t->args[1])) return termWeight(t->args[0]) * t->args[1]->num.toDouble();
            return 0.5;
        }
        case Kind::Mul: { double s = 0; for (auto& a : t->args) s += termWeight(a); return s; }
        case Kind::Func: return hasFreeVars(t) ? 0.5 : 0;
        case Kind::Add: return 0.5;
    }
    return 0;
}

bool isTrigName(const std::string& n) {
    return n == "sin" || n == "cos" || n == "tan" || n == "sec" || n == "csc" || n == "cot" || n == "sinh" || n == "cosh" ||
           n == "tanh";
}

std::string funcLatexName(const std::string& n) {
    if (n == "asin") return "\\arcsin";
    if (n == "acos") return "\\arccos";
    if (n == "atan") return "\\arctan";
    if (isTrigName(n) || n == "ln" || n == "log" || n == "gcd" || n == "exp") return "\\" + n;
    if (n == "lcm") return "\\operatorname{lcm}";
    if (n == "re") return "\\operatorname{Re}";
    if (n == "im") return "\\operatorname{Im}";
    if (n == "arg") return "\\arg";
    return "\\operatorname{" + n + "}";
}

std::string paren(const std::string& s) { return "\\left(" + s + "\\right)"; }

E negateTerm(const E& t) {
    auto [c, rest] = splitCoeff(t);
    c = -c;
    if (isNumVal(rest, 1)) return num(c);
    if (c.isOne()) return rest;
    std::vector<E> f{num(c)};
    if (rest->kind == Kind::Mul) f.insert(f.end(), rest->args.begin(), rest->args.end());
    else f.push_back(rest);
    return mul(f);
}

class LatexPrinter {
public:
    std::string print(const E& e, int ctx = 0) {
        int p = prec(e);
        std::string s = raw(e);
        if (p < ctx) return paren(s);
        return s;
    }

private:
    int prec(const E& e) {
        switch (e->kind) {
            case Kind::Num: return (e->num.isNeg() || !e->num.isInteger()) ? P_MUL : P_ATOM;
            case Kind::Sym: return P_ATOM;
            case Kind::Add: return P_ADD;
            case Kind::Mul: return isNegativeTerm(e) ? P_ADD : P_MUL;
            case Kind::Pow:
                if (isNum(e->args[1]) && e->args[1]->num.isNeg()) return P_MUL;
                if (isNum(e->args[1]) && !e->args[1]->num.isInteger() && e->args[1]->num.num().isOne()) return P_ATOM;  // radical
                return P_POW;
            case Kind::Func: return P_ATOM;
        }
        return P_ATOM;
    }

    std::string raw(const E& e) {
        switch (e->kind) {
            case Kind::Num: {
                const Rational& r = e->num;
                if (!r.isValid()) return "\\text{undefined}";
                if (r.isInteger()) return r.num().toString();
                std::string f = "\\frac{" + r.num().abs().toString() + "}{" + r.den().toString() + "}";
                return r.isNeg() ? "-" + f : f;
            }
            case Kind::Sym: return symLatex(e->name);
            case Kind::Add: {
                auto terms = displayOrder(e);
                // prefer a positive leading term: -x^2 + 25 -> 25 - x^2
                if (isNegativeTerm(terms[0])) {
                    for (size_t k = 1; k < terms.size(); ++k)
                        if (!isNegativeTerm(terms[k])) { E t = terms[k]; terms.erase(terms.begin() + k); terms.insert(terms.begin(), t); break; }
                }
                std::string s;
                for (size_t i = 0; i < terms.size(); ++i) {
                    if (i == 0) s += print(terms[i], P_ADD);
                    else if (isNegativeTerm(terms[i])) s += " - " + print(negateTerm(terms[i]), P_MUL);
                    else s += " + " + print(terms[i], P_ADD + 1);
                }
                return s;
            }
            case Kind::Mul: return mulLatex(e);
            case Kind::Pow: return powLatex(e);
            case Kind::Func: return funcLatex(e);
        }
        return "?";
    }

    // group used to order factors for display
    static int factorGroup(const E& f) {
        if (f->kind == Kind::Num) return 0;
        if (f->kind == Kind::Pow && isNum(f->args[0])) return 1;                  // numeric radicals
        if (isSym(f, "pi") || isSym(f, "e")) return 2;
        if (f->kind == Kind::Sym && f->name != "i") return 3;
        if (f->kind == Kind::Pow && f->args[0]->kind == Kind::Sym) return 3;
        if (isSym(f, "i")) return 4;
        if (f->kind == Kind::Pow && f->args[0]->kind == Kind::Func) return 5;
        if (f->kind == Kind::Func) return 5;
        return 6;  // sums, other powers
    }

    std::string joinFactors(std::vector<E> fs, const BigInt& coeff) {
        if (fs.size() == 1 && coeff.isOne() && fs[0]->kind == Kind::Add) return print(fs[0]);
        std::stable_sort(fs.begin(), fs.end(), [](const E& a, const E& b) { return factorGroup(a) < factorGroup(b); });
        std::string s;
        bool prevNumeric = false;
        if (!coeff.isOne()) { s = coeff.toString(); prevNumeric = true; }
        for (auto& f : fs) {
            std::string fstr = f->kind == Kind::Add ? paren(print(f)) : print(f, P_MUL + 1);
            bool startsDigit = !fstr.empty() && (std::isdigit((unsigned char)fstr[0]) || fstr[0] == '-');
            if (!s.empty()) {
                if (prevNumeric && startsDigit) s += " \\cdot ";
                else if (startsDigit) s += " \\cdot ";
                else s += " ";
            }
            s += fstr;
            prevNumeric = f->kind == Kind::Num;
        }
        if (s.empty()) s = "1";
        return s;
    }

    std::string mulLatex(const E& e) {
        Rational c(1);
        std::vector<E> nums, dens;
        for (auto& f : e->args) {
            if (f->kind == Kind::Num) { c *= f->num; continue; }
            if (f->kind == Kind::Pow && isNum(f->args[1]) && f->args[1]->num.isNeg()) {
                Rational p = -f->args[1]->num;
                dens.push_back(p.isOne() ? f->args[0] : pow(f->args[0], num(p)));
            } else nums.push_back(f);
        }
        bool negative = c.isNeg();
        c = c.abs();
        std::string out;
        if (dens.empty() && c.isInteger()) {
            out = joinFactors(nums, c.num());
        } else {
            std::string n = joinFactors(nums, c.num());
            std::string d = joinFactors(dens, c.den());
            out = "\\frac{" + n + "}{" + d + "}";
        }
        return negative ? "-" + out : out;
    }

    std::string powLatex(const E& e) {
        const E& b = e->args[0];
        const E& x = e->args[1];
        if (isNum(x) && x->num.isNeg()) {
            auto wrap = std::make_shared<Node>();
            wrap->kind = Kind::Mul;
            wrap->args = {e};
            return mulLatex(wrap);
        }
        if (isNum(x) && !x->num.isInteger() && x->num.num().isOne()) {
            if (x->num.den() == BigInt(2)) return "\\sqrt{" + print(b) + "}";
            return "\\sqrt[" + x->num.den().toString() + "]{" + print(b) + "}";
        }
        if (b->kind == Kind::Func && isTrigName(b->name) && isInteger(x) && x->num.sign() > 0) {
            return funcLatexName(b->name) + "^{" + print(x) + "}" + paren(print(b->args[0]));
        }
        std::string bs;
        bool needParen = b->kind == Kind::Add || b->kind == Kind::Mul || b->kind == Kind::Pow ||
                         (b->kind == Kind::Num && (b->num.isNeg() || !b->num.isInteger())) || isFunc(b, "fact");
        bs = needParen ? paren(print(b)) : print(b);
        return bs + "^{" + print(x) + "}";
    }

    std::string funcLatex(const E& e) {
        const std::string& n = e->name;
        auto arg = [&](size_t i) { return print(e->args[i]); };
        if (n == "abs") return "\\left|" + arg(0) + "\\right|";
        if (n == "fact") {
            const E& a = e->args[0];
            bool atom = a->kind == Kind::Sym || (a->kind == Kind::Num && a->num.isInteger() && !a->num.isNeg());
            return (atom ? arg(0) : paren(arg(0))) + "!";
        }
        if (n == "conj") return "\\overline{" + arg(0) + "}";
        if (n == "approx" && isNum(e->args[0])) return fmtDouble(e->args[0]->num.toDouble(), 8);
        if (n == "floor") return "\\left\\lfloor " + arg(0) + "\\right\\rfloor";
        if (n == "ceil") return "\\left\\lceil " + arg(0) + "\\right\\rceil";
        if (n == "binom" && e->args.size() == 2) return "\\binom{" + arg(0) + "}{" + arg(1) + "}";
        if (n == "perm" && e->args.size() == 2) return "P" + paren(arg(0) + ", " + arg(1));
        if (n == "log" && e->args.size() == 2) {
            if (isNumVal(e->args[0], 10)) return "\\log" + paren(arg(1));
            return "\\log_{" + arg(0) + "}" + paren(arg(1));
        }
        std::string s;
        for (size_t i = 0; i < e->args.size(); ++i) s += (i ? ", " : "") + arg(i);
        return funcLatexName(n) + paren(s);
    }
};

class PlainPrinter {
public:
    std::string print(const E& e, int ctx = 0) {
        int p = prec(e);
        std::string s = raw(e);
        return p < ctx ? "(" + s + ")" : s;
    }

private:
    int prec(const E& e) {
        switch (e->kind) {
            case Kind::Num: return (e->num.isNeg() || !e->num.isInteger()) ? P_ADD : P_ATOM;
            case Kind::Sym: return P_ATOM;
            case Kind::Add: return P_ADD;
            case Kind::Mul: return P_ADD;  // may start with '-'
            case Kind::Pow: return P_POW;
            case Kind::Func: return P_ATOM;
        }
        return P_ATOM;
    }
    std::string raw(const E& e) {
        switch (e->kind) {
            case Kind::Num: return e->num.toString();
            case Kind::Sym: return e->name == "inf" ? "inf" : e->name;
            case Kind::Add: {
                std::string s;
                for (size_t i = 0; i < e->args.size(); ++i) s += (i ? " + " : "") + print(e->args[i], P_ADD + 1);
                return s;
            }
            case Kind::Mul: {
                std::string s;
                for (size_t i = 0; i < e->args.size(); ++i) s += (i ? "*" : "") + print(e->args[i], P_MUL + 1);
                return s;
            }
            case Kind::Pow: return print(e->args[0], P_ATOM) + "^" + print(e->args[1], P_ATOM);
            case Kind::Func: {
                std::string n = e->name;
                std::string s;
                for (size_t i = 0; i < e->args.size(); ++i) s += (i ? "," : "") + print(e->args[i]);
                return n + "(" + s + ")";
            }
        }
        return "?";
    }
};

}  // namespace

std::vector<E> displayOrder(const E& addNode) {
    std::vector<E> terms = addNode->args;
    bool numeric = !hasFreeVars(addNode);
    std::stable_sort(terms.begin(), terms.end(), [numeric](const E& a, const E& b) {
        if (numeric) {
            // real part first, then imaginary part
            bool ia = containsSym(a, "i"), ib = containsSym(b, "i");
            if (ia != ib) return !ia;
            bool na = isNum(a), nb = isNum(b);
            if (na != nb) return na;
            return false;
        }
        double wa = termWeight(a), wb = termWeight(b);
        if (wa != wb) return wa > wb;
        // same degree: alphabetical by first variable (x before y before z)
        std::vector<std::string> va, vb;
        collectVars(a, va);
        collectVars(b, vb);
        if (!va.empty() && !vb.empty() && va[0] != vb[0]) return va[0] < vb[0];
        return false;
    });
    return terms;
}

std::string toLatex(const E& e) { return LatexPrinter().print(e); }
std::string toPlain(const E& e) { return PlainPrinter().print(e); }

std::string fmtDouble(double v, int sig) {
    if (std::isnan(v)) return "undefined";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    if (std::fabs(v) < 1e-12) return "0";
    char buf[64];
    snprintf(buf, sizeof buf, "%.*g", sig, v);
    return buf;
}

std::string numLatex(double v, int sig) {
    if (std::isnan(v)) return "\\text{undefined}";
    if (std::isinf(v)) return v > 0 ? "\\infty" : "-\\infty";
    if (std::fabs(v) < 1e-12) return "0";
    double av = std::fabs(v);
    if (av >= 1e9 || av < 1e-5) {
        int ex = (int)std::floor(std::log10(av));
        double m = v / std::pow(10.0, ex);
        char buf[64];
        snprintf(buf, sizeof buf, "%.*g", sig, m);
        return std::string(buf) + " \\times 10^{" + std::to_string(ex) + "}";
    }
    return fmtDouble(v, sig);
}

}  // namespace me
