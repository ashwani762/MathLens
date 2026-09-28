// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Symbolic differentiation with explained steps.
#include <algorithm>
#include <cmath>

#include "calculus/calculus.h"
#include "solution/plotutil.h"

namespace me {

std::string ddx(const std::string& var, const std::string& latex) {
    return "\\frac{d}{d" + var + "}\\left(" + latex + "\\right)";
}

namespace {

class Differ {
public:
    Differ(std::string var, bool record) : x_(std::move(var)), rec_(record) {}
    bool failed = false;

    E d(const E& f0, Step* st) {
        E f = simplify(f0);
        E r = rule(f, st);
        r = simplify(r);
        if (st && rec_) st->latex = ddx(x_, toLatex(f)) + " = " + toLatex(r);
        return r;
    }

private:
    std::string x_;
    bool rec_;

    E X() const { return sym(x_); }
    std::string xs() const { return x_; }

    void set(Step* st, const std::string& title, const std::string& text) {
        if (st && rec_) { st->title = title; st->text = text; }
    }

    // derivative of inner u with a nested chain rule sub-step
    E inner(const E& u, Step* st) {
        if (!st || !rec_) return d(u, nullptr);
        Step s;
        E du = d(u, &s);
        st->sub.push_back(std::move(s));
        return du;
    }

    // outer'(u) for elementary functions, nullopt if unknown
    std::optional<std::pair<E, std::string>> outer(const std::string& fn, const E& u) {
        std::string U = "u";
        if (fn == "sin") return std::pair{func("cos", u), "\\frac{d}{du}\\sin(u) = \\cos(u)"};
        if (fn == "cos") return std::pair{neg(func("sin", u)), "\\frac{d}{du}\\cos(u) = -\\sin(u)"};
        if (fn == "tan") return std::pair{pow(func("sec", u), num(2)), "\\frac{d}{du}\\tan(u) = \\sec^{2}(u)"};
        if (fn == "cot") return std::pair{neg(pow(func("csc", u), num(2))), "\\frac{d}{du}\\cot(u) = -\\csc^{2}(u)"};
        if (fn == "sec") return std::pair{mul(func("sec", u), func("tan", u)), "\\frac{d}{du}\\sec(u) = \\sec(u)\\tan(u)"};
        if (fn == "csc") return std::pair{neg(mul(func("csc", u), func("cot", u))), "\\frac{d}{du}\\csc(u) = -\\csc(u)\\cot(u)"};
        if (fn == "asin") return std::pair{pow(sub(num(1), pow(u, num(2))), num(Rational(-1, 2))), "\\frac{d}{du}\\arcsin(u) = \\frac{1}{\\sqrt{1-u^{2}}}"};
        if (fn == "acos") return std::pair{neg(pow(sub(num(1), pow(u, num(2))), num(Rational(-1, 2)))), "\\frac{d}{du}\\arccos(u) = -\\frac{1}{\\sqrt{1-u^{2}}}"};
        if (fn == "atan") return std::pair{pow(add(num(1), pow(u, num(2))), num(-1)), "\\frac{d}{du}\\arctan(u) = \\frac{1}{1+u^{2}}"};
        if (fn == "sinh") return std::pair{func("cosh", u), "\\frac{d}{du}\\sinh(u) = \\cosh(u)"};
        if (fn == "cosh") return std::pair{func("sinh", u), "\\frac{d}{du}\\cosh(u) = \\sinh(u)"};
        if (fn == "tanh") return std::pair{pow(func("cosh", u), num(-2)), "\\frac{d}{du}\\tanh(u) = \\frac{1}{\\cosh^{2}(u)}"};
        if (fn == "ln") return std::pair{pow(u, num(-1)), "\\frac{d}{du}\\ln(u) = \\frac{1}{u}"};
        if (fn == "abs") return std::pair{divide(u, func("abs", u)), "\\frac{d}{du}\\left|u\\right| = \\frac{u}{\\left|u\\right|}"};
        return std::nullopt;
    }

    static std::string fnWord(const std::string& fn) {
        if (fn == "ln") return "natural logarithm";
        if (fn == "asin") return "arcsine";
        if (fn == "acos") return "arccosine";
        if (fn == "atan") return "arctangent";
        if (fn == "abs") return "absolute value";
        return fn;
    }

    E rule(const E& f, Step* st) {
        const E x = X();
        if (!containsSym(f, x_)) {
            set(st, "Constant rule", "The derivative of a constant is " + m("0") + ".");
            return num(0);
        }
        switch (f->kind) {
            case Kind::Sym:
                set(st, "Derivative of " + xs(), m("\\frac{d}{d" + xs() + "}(" + xs() + ") = 1"));
                return num(1);
            case Kind::Add: {
                set(st, "Sum rule", "Differentiate each term separately: " + m("(f+g)' = f' + g'"));
                std::vector<E> parts;
                for (auto& t : f->args) parts.push_back(inner(t, st));
                return add(parts);
            }
            case Kind::Mul: return mulRule(f, st);
            case Kind::Pow: return powRule(f, st);
            case Kind::Func: return funcRule(f, st);
            default: break;
        }
        failed = true;
        return func("diff", {f, x});
    }

    E mulRule(const E& f, Step* st) {
        std::vector<E> consts, vars;
        for (auto& a : f->args) (containsSym(a, x_) ? vars : consts).push_back(a);
        if (!consts.empty()) {
            E c = simplify(mul(consts));
            E rest = simplify(mul(vars));
            set(st, "Constant multiple rule", "Move the constant " + m(toLatex(c)) + " outside: " + m("(c\\cdot f)' = c\\cdot f'"));
            return mul(c, inner(rest, st));
        }
        // quotient rule when there is a non-constant denominator
        auto [n, dnm] = numDen(f);
        if (containsSym(dnm, x_) && !isNumVal(n, 1) && containsSym(n, x_)) {
            set(st, "Quotient rule", m("\\left(\\frac{u}{v}\\right)' = \\frac{u'v - uv'}{v^{2}}") + " with " + m("u = " + toLatex(n)) +
                                         " and " + m("v = " + toLatex(dnm)));
            E du = inner(n, st);
            E dv = inner(dnm, st);
            return divide(sub(mul(du, dnm), mul(n, dv)), pow(dnm, num(2)));
        }
        // product rule u*v
        E u = f->args[0];
        std::vector<E> rest(f->args.begin() + 1, f->args.end());
        E v = simplify(mul(rest));
        set(st, "Product rule", m("(uv)' = u'v + uv'") + " with " + m("u = " + toLatex(u)) + " and " + m("v = " + toLatex(v)));
        E du = inner(u, st);
        E dv = inner(v, st);
        return add(mul(du, v), mul(u, dv));
    }

    E powRule(const E& f, Step* st) {
        const E& b = f->args[0];
        const E& ex = f->args[1];
        bool bx = containsSym(b, x_), ex_x = containsSym(ex, x_);
        if (bx && !ex_x) {
            E n1 = simplify(sub(ex, num(1)));
            if (isSym(b, x_)) {
                std::string what = isNum(ex) && ex->num == Rational(1, 2) ? "Rewrite the root as a power and apply the power rule: "
                                   : isNum(ex) && ex->num.isNeg() ? "Rewrite as a negative power and apply the power rule: "
                                                                  : "Apply the power rule: ";
                set(st, "Power rule", what + m("\\frac{d}{d" + xs() + "}" + xs() + "^{n} = n" + xs() + "^{n-1}"));
                return mul(ex, pow(b, n1));
            }
            set(st, "Chain rule", "Use the power rule with the chain rule: " + m("\\frac{d}{dx}\\left[u^{n}\\right] = n u^{n-1} \\cdot u'") +
                                      " where " + m("u = " + toLatex(b)));
            E du = inner(b, st);
            return mul({ex, pow(b, n1), du});
        }
        if (!bx && ex_x) {
            if (isSym(b, "e")) {
                if (isSym(ex, x_)) {
                    set(st, "Exponential rule", m("\\frac{d}{dx}e^{x} = e^{x}"));
                    return f;
                }
                set(st, "Chain rule", m("\\frac{d}{dx}e^{u} = e^{u}\\cdot u'") + " where " + m("u = " + toLatex(ex)));
                E du = inner(ex, st);
                return mul(f, du);
            }
            set(st, "Exponential rule", m("\\frac{d}{dx}a^{u} = a^{u}\\ln(a)\\cdot u'") + " with " + m("a = " + toLatex(b)));
            E du = isSym(ex, x_) ? num(1) : inner(ex, st);
            return mul({f, func("ln", b), du});
        }
        // x^x style: logarithmic differentiation
        set(st, "Logarithmic differentiation", "Write " + m("f = e^{" + toLatex(ex) + "\\ln(" + toLatex(b) + ")}") + " and differentiate the exponent");
        E g = mul(ex, func("ln", b));
        E dg = inner(g, st);
        return mul(f, dg);
    }

    E funcRule(const E& f, Step* st) {
        const std::string& fn = f->name;
        if (fn == "log" && f->args.size() == 2) {
            const E& base = f->args[0];
            const E& u = f->args[1];
            set(st, "Logarithm rule", m("\\frac{d}{dx}\\log_{b}(u) = \\frac{u'}{u\\ln(b)}"));
            E du = isSym(u, x_) ? num(1) : inner(u, st);
            return divide(du, mul(u, func("ln", base)));
        }
        if (f->args.size() != 1) { failed = true; return func("diff", {f, X()}); }
        const E& u = f->args[0];
        auto o = outer(fn, u);
        if (!o) { failed = true; return func("diff", {f, X()}); }
        if (isSym(u, x_)) {
            std::string formula = o->second;
            // replace u by the variable in the formula for readability
            for (size_t p = 0; (p = formula.find("u", p)) != std::string::npos;) {
                bool cmd = p > 0 && std::isalpha((unsigned char)formula[p - 1]);
                if (!cmd) formula.replace(p, 1, x_);
                p += 1;
            }
            set(st, "Derivative of " + fnWord(fn), m(formula));
            return o->first;
        }
        set(st, "Chain rule", "Differentiate the outer function and multiply by the derivative of the inside: " + m(o->second) +
                                  " with " + m("u = " + toLatex(u)));
        E du = inner(u, st);
        return mul(o->first, du);
    }
};

}  // namespace

E derivativeOf(const E& f, const std::string& var, int order) {
    E r = f;
    for (int k = 0; k < order; ++k) {
        Differ d(var, false);
        r = d.d(r, nullptr);
    }
    return r;
}

Solution solveDerivative(const E& f0, const std::string& var, int order, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "derivative";
    sol.anim = "derivative";
    sol.title = order == 1 ? "Find the derivative" : "Find the derivative of order " + std::to_string(order);
    sol.inputLatex = inputLatex;
    Method meth{"Differentiation rules", {}};
    E f = simplify(f0);
    E cur = f;
    for (int k = 1; k <= order; ++k) {
        if (order > 1) {
            meth.steps.push_back({"Derivative number " + std::to_string(k), "Differentiate " + m(toLatex(cur)), ddx(var, toLatex(cur)), {}});
        }
        Differ d(var, true);
        E prev = cur;
        if (cur->kind == Kind::Add) {
            // sum rule at top: split, then one step per term
            std::string split;
            for (size_t i = 0; i < cur->args.size(); ++i) {
                E t = cur->args[i];
                split += (i ? " + " : "") + ddx(var, toLatex(t));
            }
            meth.steps.push_back({"Use the sum rule", "The derivative of a sum is the sum of the derivatives.", split, {}});
            std::vector<E> parts;
            std::string combined;
            for (auto& t : displayOrder(cur)) {
                Step s;
                E dt = d.d(t, &s);
                parts.push_back(dt);
                if (s.title.empty()) s.title = "Differentiate";
                s.title = s.title + ": " + m(toLatex(t));
                meth.steps.push_back(std::move(s));
            }
            for (size_t i = 0; i < parts.size(); ++i) {
                std::string tl = toLatex(parts[i]);
                if (i && !tl.empty() && tl[0] == '-') combined += " " + tl;
                else combined += (i ? " + " : "") + tl;
            }
            cur = simplify(add(parts));
            meth.steps.push_back({"Combine the results", "", ddx(var, toLatex(prev)) + " = " + combined, {}});
        } else {
            Step s;
            cur = d.d(cur, &s);
            meth.steps.push_back(std::move(s));
        }
        if (d.failed) {
            sol.ok = false;
            sol.error = "This expression contains a function I can't differentiate yet.";
            return sol;
        }
        E expanded = simplify(cur);
        meth.steps.push_back({"Simplify", "", toLatex(expanded), {}});
        cur = expanded;
    }
    std::string prime = order == 1 ? "'" : (order == 2 ? "''" : "^{(" + std::to_string(order) + ")}");
    sol.answerLatex = "f" + prime + "(" + var + ") = " + toLatex(cur);
    sol.answers.push_back(toLatex(cur));
    sol.methods.push_back(std::move(meth));
    // factored alternative if it is a polynomial / rational function
    sol.plot = plotFunctions({{f, "f(" + var + ")"}, {cur, "f" + prime + "(" + var + ")"}}, var);
    return sol;
}

}  // namespace me
