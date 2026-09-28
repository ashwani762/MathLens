// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Symbolic integration with explained steps.
// Strategy (in order): constants, linearity, table, linear substitution, polynomial expansion,
// partial fractions, u-substitution, trig identities, integration by parts, special forms.
// Every antiderivative is verified numerically by differentiation before being reported.
#include <algorithm>
#include <cmath>
#include <functional>

#include "calculus/calculus.h"
#include "solution/plotutil.h"
#include "core/poly.h"

namespace me {

namespace {

std::string intTex(const std::string& f, const std::string& var) { return "\\int " + f + " \\, d" + var; }

class Integrator {
public:
    Integrator(std::string var, bool record) : x_(std::move(var)), rec_(record) {}

    std::optional<E> I(const E& f0, Step* st, int depth = 0) {
        if (depth > 7) return std::nullopt;
        E f = simplify(f0);
        auto r = attempt(f, st, depth);
        if (r) {
            *r = simplify(*r);
            if (st && rec_) st->latex = intTex(toLatex(f), x_) + " = " + toLatex(*r);
        }
        return r;
    }

private:
    std::string x_;
    bool rec_;
    E X() const { return sym(x_); }

    void set(Step* st, const std::string& title, const std::string& text) {
        if (st && rec_) { st->title = title; st->text = text; }
    }

    std::optional<E> sub(const E& f, Step* st, int depth) {
        if (!st || !rec_) return I(f, nullptr, depth + 1);
        Step s;
        auto r = I(f, &s, depth + 1);
        if (r) st->sub.push_back(std::move(s));
        return r;
    }

    bool isX(const E& e) const { return isSym(e, x_); }
    bool hasX(const E& e) const { return containsSym(e, x_); }

    // u = a x + b  -> returns a (nullopt when not linear)
    std::optional<E> linearCoeff(const E& u) const {
        if (!hasX(u)) return std::nullopt;
        E du = simplify(derivativeOf(u, x_));
        if (hasX(du)) return std::nullopt;
        return du;
    }

    // ---- table of basic antiderivatives in terms of u (u = x here) ----
    std::optional<std::pair<E, std::string>> table(const E& f, const E& u) {
        // f is expressed with u in place of x
        if (eq(f, u)) return std::pair{divide(pow(u, num(2)), num(2)), "\\int u \\, du = \\frac{u^{2}}{2}"};
        if (f->kind == Kind::Pow) {
            const E& b = f->args[0];
            const E& n = f->args[1];
            if (eq(b, u) && !hasX(n) && !containsSym(n, "u_")) {
                if (isNumVal(n, -1)) return std::pair{func("ln", func("abs", u)), "\\int \\frac{1}{u} \\, du = \\ln\\left|u\\right|"};
                E n1 = simplify(add(n, num(1)));
                return std::pair{divide(pow(u, n1), n1), "\\int u^{n} \\, du = \\frac{u^{n+1}}{n+1}"};
            }
            if (!containsSym(b, x_) && !containsSym(b, "u_") && eq(n, u)) {
                if (isSym(b, "e")) return std::pair{f, "\\int e^{u} \\, du = e^{u}"};
                return std::pair{divide(f, func("ln", b)), "\\int a^{u} \\, du = \\frac{a^{u}}{\\ln(a)}"};
            }
            // sec^2, csc^2, 1/cos^2, 1/sin^2
            if (isInteger(n)) {
                long long k = n->num.num().toInt64();
                if (isFunc(b, "sec") && k == 2 && eq(b->args[0], u)) return std::pair{func("tan", u), "\\int \\sec^{2}(u) \\, du = \\tan(u)"};
                if (isFunc(b, "csc") && k == 2 && eq(b->args[0], u)) return std::pair{neg(func("cot", u)), "\\int \\csc^{2}(u) \\, du = -\\cot(u)"};
                if (isFunc(b, "cos") && k == -2 && eq(b->args[0], u)) return std::pair{func("tan", u), "\\int \\frac{1}{\\cos^{2}(u)} \\, du = \\tan(u)"};
                if (isFunc(b, "sin") && k == -2 && eq(b->args[0], u)) return std::pair{neg(func("cot", u)), "\\int \\frac{1}{\\sin^{2}(u)} \\, du = -\\cot(u)"};
                if (isFunc(b, "cosh") && k == -2 && eq(b->args[0], u)) return std::pair{func("tanh", u), "\\int \\frac{1}{\\cosh^{2}(u)} \\, du = \\tanh(u)"};
            }
        }
        if (f->kind == Kind::Func && f->args.size() == 1 && eq(f->args[0], u)) {
            const std::string& n = f->name;
            if (n == "sin") return std::pair{neg(func("cos", u)), "\\int \\sin(u) \\, du = -\\cos(u)"};
            if (n == "cos") return std::pair{func("sin", u), "\\int \\cos(u) \\, du = \\sin(u)"};
            if (n == "tan") return std::pair{neg(func("ln", func("abs", func("cos", u)))), "\\int \\tan(u) \\, du = -\\ln\\left|\\cos(u)\\right|"};
            if (n == "cot") return std::pair{func("ln", func("abs", func("sin", u))), "\\int \\cot(u) \\, du = \\ln\\left|\\sin(u)\\right|"};
            if (n == "sec") return std::pair{func("ln", func("abs", add(func("sec", u), func("tan", u)))), "\\int \\sec(u) \\, du = \\ln\\left|\\sec(u)+\\tan(u)\\right|"};
            if (n == "csc") return std::pair{neg(func("ln", func("abs", add(func("csc", u), func("cot", u))))), "\\int \\csc(u) \\, du = -\\ln\\left|\\csc(u)+\\cot(u)\\right|"};
            if (n == "sinh") return std::pair{func("cosh", u), "\\int \\sinh(u) \\, du = \\cosh(u)"};
            if (n == "cosh") return std::pair{func("sinh", u), "\\int \\cosh(u) \\, du = \\sinh(u)"};
            if (n == "tanh") return std::pair{func("ln", func("cosh", u)), "\\int \\tanh(u) \\, du = \\ln(\\cosh(u))"};
        }
        if (f->kind == Kind::Mul && f->args.size() == 2) {
            auto isF = [&](const E& e, const char* n) { return isFunc(e, n) && eq(e->args[0], u); };
            const E& a = f->args[0];
            const E& b = f->args[1];
            if ((isF(a, "sec") && isF(b, "tan")) || (isF(b, "sec") && isF(a, "tan"))) return std::pair{func("sec", u), "\\int \\sec(u)\\tan(u) \\, du = \\sec(u)"};
            if ((isF(a, "csc") && isF(b, "cot")) || (isF(b, "csc") && isF(a, "cot"))) return std::pair{neg(func("csc", u)), "\\int \\csc(u)\\cot(u) \\, du = -\\csc(u)"};
        }
        return std::nullopt;
    }

    // Quadratic forms under roots and in denominators: (c0 + c2 x^2)^p with p in {-1, -1/2, 1/2}
    std::optional<E> specialQuadratic(const E& f, Step* st) {
        if (f->kind != Kind::Pow || !isNum(f->args[1])) return std::nullopt;
        Rational p = f->args[1]->num;
        auto q = Poly::from(f->args[0], x_);
        if (!q || q->deg() != 2 || !q->coeff(1).isZero()) return std::nullopt;
        Rational c0 = q->coeff(0), c2 = q->coeff(2);
        E x = X();
        if (p == Rational(-1) && c0.sign() > 0 && c2.sign() > 0) {
            // 1/(c0 + c2 x^2) = (1/sqrt(c0 c2)) atan(x sqrt(c2/c0))
            E k = simplify(sqrtE(num(c2 / c0)));
            E r = simplify(mul(divide(num(1), sqrtE(num(c0 * c2))), func("atan", mul(k, x))));
            set(st, "Arctangent form", m("\\int \\frac{1}{a^{2}+x^{2}} \\, dx = \\frac{1}{a}\\arctan\\left(\\frac{x}{a}\\right)"));
            return r;
        }
        if (p == Rational(-1, 2)) {
            if (c0.sign() > 0 && c2.sign() < 0) {
                E k = simplify(sqrtE(num(-c2 / c0)));
                set(st, "Arcsine form", m("\\int \\frac{1}{\\sqrt{a^{2}-x^{2}}} \\, dx = \\arcsin\\left(\\frac{x}{a}\\right)"));
                return simplify(mul(divide(num(1), sqrtE(num(-c2))), func("asin", mul(k, x))));
            }
            if (c2.sign() > 0) {
                set(st, "Logarithmic form", m("\\int \\frac{1}{\\sqrt{x^{2}+a}} \\, dx = \\ln\\left|x+\\sqrt{x^{2}+a}\\right|"));
                E s = sqrtE(add(num(c0 / c2), pow(x, num(2))));
                return simplify(mul(divide(num(1), sqrtE(num(c2))), func("ln", func("abs", add(x, s)))));
            }
        }
        if (p == Rational(1, 2) && c0.sign() > 0 && c2.sign() < 0) {
            // sqrt(c0 + c2 x^2) = k sqrt(a^2 - x^2), k = sqrt(-c2), a^2 = c0/(-c2)
            Rational a2 = c0 / (-c2);
            E k = sqrtE(num(-c2));
            E a = sqrtE(num(a2));
            E s = sqrtE(sub(num(a2), pow(x, num(2))));
            set(st, "Trigonometric substitution", "Substitute " + m("x = a\\sin\\theta") + ": " +
                                                      m("\\int \\sqrt{a^{2}-x^{2}} \\, dx = \\frac{x}{2}\\sqrt{a^{2}-x^{2}} + \\frac{a^{2}}{2}\\arcsin\\left(\\frac{x}{a}\\right)"));
            return simplify(mul(k, add(mul(divide(x, num(2)), s), mul(num(a2 / Rational(2)), func("asin", divide(x, a))))));
        }
        return std::nullopt;
    }

    // ---- partial fractions ----
    std::optional<E> rational(const E& f, Step* st, int depth) {
        auto rf = toRationalFunction(f, x_);
        if (!rf || rf->second.deg() < 1) return std::nullopt;
        Poly P = rf->first, Q = rf->second;
        E x = X();
        std::vector<Step> subs;
        std::vector<E> pieces;
        // polynomial part
        if (P.deg() >= Q.deg()) {
            Poly S, Rm;
            Poly::divmod(P, Q, S, Rm);
            Step s{"Polynomial long division", "The numerator's degree is at least the denominator's, so divide first.",
                   toLatex(S.toExpr(x_)) + (Rm.isZero() ? "" : " + " + toLatex(simplify(divide(Rm.toExpr(x_), Q.toExpr(x_))))), {}};
            subs.push_back(s);
            pieces.push_back(integratePoly(S));
            P = Rm;
        }
        if (!P.isZero()) {
            Factorization fz = factorQ(Q);
            for (auto& pf : fz.factors) if (pf.p.deg() > 2 || (pf.p.deg() == 2 && pf.mult > 1)) return std::nullopt;
            // unknowns: for linear factor l^m: A_1..A_m ; for quadratic q: (B x + C)
            struct Term { Poly base; int power; bool quad; };
            std::vector<Term> terms;
            for (auto& pf : fz.factors) {
                if (pf.p.deg() == 1) for (int j = 1; j <= pf.mult; ++j) terms.push_back({pf.p, j, false});
                else terms.push_back({pf.p, 1, true});
            }
            int nUnknown = 0;
            for (auto& t : terms) nUnknown += t.quad ? 2 : 1;
            int n = Q.deg();
            if (nUnknown != n) return std::nullopt;
            // Q = content * prod(pf^mult). Build Q/(base^power) for each term
            auto powPoly = [](const Poly& p, int k) { Poly r = Poly::constant(Rational(1)); for (int i = 0; i < k; ++i) r = r * p; return r; };
            Poly Qn = Poly::constant(Rational(1));
            for (auto& pf : fz.factors) Qn = Qn * powPoly(pf.p, pf.mult);
            // P/Q = (P/content) / Qn
            Poly Pn = P.scale(fz.content.inv());
            std::vector<std::vector<Rational>> A(n, std::vector<Rational>(nUnknown));
            int col = 0;
            for (auto& t : terms) {
                Poly q, r;
                Poly::divmod(Qn, powPoly(t.base, t.power), q, r);
                if (t.quad) {
                    Poly xq = q * Poly::linear(Rational(1), Rational(0));
                    for (int i = 0; i < n; ++i) { A[i][col] = xq.coeff(i); A[i][col + 1] = q.coeff(i); }
                    col += 2;
                } else {
                    for (int i = 0; i < n; ++i) A[i][col] = q.coeff(i);
                    col += 1;
                }
            }
            std::vector<Rational> b(n);
            for (int i = 0; i < n; ++i) b[i] = Pn.coeff(i);
            auto sol = solveLinearSystem(A, b);
            if (!sol) return std::nullopt;
            // build decomposition and integrate each piece
            std::string decomp, generic;
            col = 0;
            std::vector<E> fracs;
            const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            int li = 0;
            for (auto& t : terms) {
                E base = t.base.toExpr(x_);
                E den = t.power == 1 ? base : pow(base, num(t.power));
                E numer;
                std::string gnum;
                if (t.quad) {
                    numer = simplify(add(mul(num((*sol)[col]), x), num((*sol)[col + 1])));
                    gnum = std::string(1, letters[li % 26]) + x_ + " + " + std::string(1, letters[(li + 1) % 26]);
                    col += 2;
                    li += 2;
                } else {
                    numer = num((*sol)[col]);
                    gnum = std::string(1, letters[li % 26]);
                    col += 1;
                    li += 1;
                }
                generic += (generic.empty() ? "" : " + ") + std::string("\\frac{") + gnum + "}{" + toLatex(den) + "}";
                if (isNumVal(numer, 0)) continue;
                fracs.push_back(simplify(divide(numer, den)));
            }
            for (size_t i = 0; i < fracs.size(); ++i) {
                std::string tl = toLatex(fracs[i]);
                decomp += (i && tl[0] != '-' ? " + " : (i ? " " : "")) + tl;
            }
            subs.push_back({"Factor the denominator", "", toLatex(factorizationToExpr(fz, x_)), {}});
            subs.push_back({"Set up partial fractions", "Write the fraction as a sum of simpler fractions with unknown numerators.",
                            toLatex(simplify(divide(P.toExpr(x_), Q.toExpr(x_)))) + " = " + generic, {}});
            subs.push_back({"Solve for the unknowns", "Multiply both sides by the denominator and match coefficients.", decomp, {}});
            for (auto& fr : fracs) {
                auto r = partialPiece(fr, depth);
                if (!r) return std::nullopt;
                Step ps{"Integrate " + m(toLatex(fr)), "", intTex(toLatex(fr), x_) + " = " + toLatex(simplify(*r)), {}};
                subs.push_back(ps);
                pieces.push_back(*r);
            }
        }
        set(st, "Partial fractions", "Split the rational function into simpler fractions and integrate each one.");
        if (st && rec_) for (auto& s : subs) st->sub.push_back(s);
        return add(pieces);
    }

    E integratePoly(const Poly& p) {
        std::vector<E> t;
        for (int k = 0; k <= p.deg(); ++k) {
            if (p.coeff(k).isZero()) continue;
            t.push_back(mul(num(p.coeff(k) / Rational(k + 1)), pow(X(), num(k + 1))));
        }
        return add(t);
    }

    // A/(ax+b)^k  or (Bx+C)/(x^2+px+q)
    std::optional<E> partialPiece(const E& fr, int depth) {
        auto rf = toRationalFunction(fr, x_);
        if (!rf) return std::nullopt;
        Poly N = rf->first, D = rf->second;
        E x = X();
        auto fz = factorQ(D);
        if (fz.factors.size() == 1 && fz.factors[0].p.deg() == 2 && fz.factors[0].mult == 1) {
            // (Bx + C)/(k*(x^2 + px + q))
            Poly qd = fz.factors[0].p.monic();
            Rational k = D.lead();
            Rational B = N.coeff(1) / k, C = N.coeff(0) / k;
            Rational p = qd.coeff(1), q = qd.coeff(0);
            E qe = qd.toExpr(x_);
            std::vector<E> parts;
            if (!B.isZero()) parts.push_back(mul(num(B / Rational(2)), func("ln", func("abs", qe))));
            Rational C2 = C - B * p / Rational(2);
            if (!C2.isZero()) {
                Rational disc = Rational(4) * q - p * p;  // > 0
                E s = sqrtE(num(disc));
                parts.push_back(mul({num(C2 * Rational(2)), pow(s, num(-1)), func("atan", divide(add(mul(num(2), x), num(p)), s))}));
            }
            return add(parts);
        }
        return I(fr, nullptr, depth + 1);
    }

    // ---- u-substitution ----
    void candidates(const E& e, std::vector<E>& out) const {
        if (!hasX(e) || isX(e)) return;
        auto push = [&](const E& c) {
            if (!hasX(c) || isX(c)) return;
            for (auto& o : out) if (eq(o, c)) return;
            out.push_back(c);
        };
        if (e->kind == Kind::Func) { push(e); for (auto& a : e->args) push(a); }
        if (e->kind == Kind::Pow) { push(e->args[0]); push(e->args[1]); if (isSym(e->args[0], "e")) push(e); }
        for (auto& a : e->args) candidates(a, out);
    }

    std::optional<E> uSub(const E& f, Step* st, int depth) {
        std::vector<E> cands;
        candidates(f, cands);
        // larger (more complex) candidates first
        std::ranges::stable_sort(cands, [](const E& a, const E& b) { return toPlain(a).size() > toPlain(b).size(); });
        E U = sym("u_");
        for (auto& u : cands) {
            if (linearCoeff(u)) continue;  // handled by linear substitution
            E du = simplify(derivativeOf(u, x_));
            if (isNumVal(du, 0)) continue;
            E q = simplify(divide(f, du));
            E qs = simplify(replaceSub(q, u, U));
            // also try replacing powers of u, e.g. u = x^2 inside x^4
            if (hasX(qs)) {
                // try expressing via x = ... is too general; attempt expanded form
                E qe = expand(q);
                qs = simplify(replaceSub(qe, u, U));
                if (hasX(qs)) continue;
            }
            Integrator inner("u_", rec_);
            Step s;
            auto r = inner.I(qs, rec_ ? &s : nullptr, depth + 1);
            if (!r) continue;
            E back = simplify(substitute(*r, "u_", u));
            if (st && rec_) {
                set(st, "Substitution", "Let " + m("u = " + toLatex(u)) + ", so " + m("du = " + toLatex(du) + " \\, d" + x_) + ".");
                auto uTex = [](std::string t) {
                    for (size_t p = 0; (p = t.find("u_", p)) != std::string::npos;) t.replace(p, 2, "u");
                    return t;
                };
                st->sub.push_back({"Rewrite in terms of u", "", uTex(intTex(toLatex(qs), "u_")), {}});
                s.latex = uTex(s.latex);
                s.title = uTex(s.title);
                s.text = uTex(s.text);
                st->sub.push_back(std::move(s));
                st->sub.push_back({"Substitute back", m("u = " + toLatex(u)), toLatex(back), {}});
            }
            return back;
        }
        return std::nullopt;
    }

    // ---- trig identities ----
    std::optional<E> trigIdentity(const E& f, Step* st, int depth) {
        // gather sin^m(u) cos^n(u) with the same linear argument
        std::vector<E> fs = f->kind == Kind::Mul ? f->args : std::vector<E>{f};
        E arg;
        long long ms = 0, nc = 0, tn = 0;
        for (auto& g : fs) {
            E b = g;
            long long k = 1;
            if (g->kind == Kind::Pow && isInteger(g->args[1])) { b = g->args[0]; k = g->args[1]->num.num().toInt64(); }
            if (!(isFunc(b, "sin") || isFunc(b, "cos") || isFunc(b, "tan"))) return std::nullopt;
            if (arg && !eq(arg, b->args[0])) return std::nullopt;
            arg = b->args[0];
            if (k < 0) return std::nullopt;
            if (b->name == "sin") ms += k; else if (b->name == "cos") nc += k; else tn += k;
        }
        if (!arg || !linearCoeff(arg)) return std::nullopt;
        E s = func("sin", arg), c = func("cos", arg);
        E rewritten;
        std::string why;
        if (tn == 2 && ms == 0 && nc == 0) {
            rewritten = sub(pow(func("sec", arg), num(2)), num(1));
            why = m("\\tan^{2}(u) = \\sec^{2}(u) - 1");
        } else if (tn > 0) return std::nullopt;
        else if (ms + nc < 2) return std::nullopt;
        else if (ms % 2 == 1) {
            rewritten = mul({s, pow(sub(num(1), pow(c, num(2))), num((ms - 1) / 2)), pow(c, num(nc))});
            why = "Save one sine and use " + m("\\sin^{2}(u) = 1 - \\cos^{2}(u)");
        } else if (nc % 2 == 1) {
            rewritten = mul({c, pow(sub(num(1), pow(s, num(2))), num((nc - 1) / 2)), pow(s, num(ms))});
            why = "Save one cosine and use " + m("\\cos^{2}(u) = 1 - \\sin^{2}(u)");
        } else {
            E arg2 = simplify(mul(num(2), arg));
            E s2 = mul(num(Rational(1, 2)), sub(num(1), func("cos", arg2)));
            E c2 = mul(num(Rational(1, 2)), add(num(1), func("cos", arg2)));
            rewritten = mul(pow(s2, num(ms / 2)), pow(c2, num(nc / 2)));
            why = "Use the power-reducing identities " + m("\\sin^{2}(u) = \\frac{1-\\cos(2u)}{2}") + " and " + m("\\cos^{2}(u) = \\frac{1+\\cos(2u)}{2}");
        }
        E ex = expand(rewritten);
        if (eq(ex, f)) return std::nullopt;
        set(st, "Trigonometric identity", why);
        if (st && rec_) st->sub.push_back({"Rewrite", "", intTex(toLatex(ex), x_), {}});
        return sub(ex, st, depth);
    }

    // ---- integration by parts ----
    static int liate(const E& f, const std::string& x) {
        E b = f->kind == Kind::Pow && isInteger(f->args[1]) && f->args[1]->num.sign() > 0 ? f->args[0] : f;
        if (isFunc(b, "ln") || isFunc(b, "log")) return 5;
        if (isFunc(b, "asin") || isFunc(b, "acos") || isFunc(b, "atan")) return 4;
        if (polyDegree(f, x) >= 1) return 3;
        if (isFunc(b, "sin") || isFunc(b, "cos") || isFunc(b, "sinh") || isFunc(b, "cosh")) return 2;
        if (f->kind == Kind::Pow && containsSym(f->args[1], x)) return 1;
        return 0;
    }

    std::optional<E> byParts(const E& f, Step* st, int depth) {
        if (depth > 5) return std::nullopt;
        std::vector<E> fs = f->kind == Kind::Mul ? f->args : std::vector<E>{f};
        // e^{ax} * sin/cos(bx): closed form
        if (fs.size() == 2) {
            for (int i = 0; i < 2; ++i) {
                const E& ex = fs[i];
                const E& tr = fs[1 - i];
                if (ex->kind == Kind::Pow && isSym(ex->args[0], "e") && (isFunc(tr, "sin") || isFunc(tr, "cos"))) {
                    auto a = linearCoeff(ex->args[1]);
                    auto b = linearCoeff(tr->args[0]);
                    if (!a || !b) break;
                    E den = add(pow(*a, num(2)), pow(*b, num(2)));
                    E sb = func("sin", tr->args[0]), cb = func("cos", tr->args[0]);
                    E inner = tr->name == "sin" ? sub(mul(*a, sb), mul(*b, cb)) : add(mul(*a, cb), mul(*b, sb));
                    set(st, "Integration by parts (twice)",
                        "Apply " + m("\\int u \\, dv = uv - \\int v \\, du") + " two times. The original integral reappears, so solve for it.");
                    return mul({ex, inner, pow(den, num(-1))});
                }
            }
        }
        // choose u by LIATE
        int best = -1, bi = -1;
        for (size_t i = 0; i < fs.size(); ++i) {
            int l = liate(fs[i], x_);
            if (l > best) { best = l; bi = (int)i; }
        }
        if (bi < 0 || best <= 0) return std::nullopt;
        E u = fs[bi];
        std::vector<E> rest;
        for (size_t i = 0; i < fs.size(); ++i) if ((int)i != bi) rest.push_back(fs[i]);
        E dv = simplify(mul(rest));
        if (fs.size() == 1 && best < 4 && !(best == 3 && false)) {
            if (best != 5 && best != 4) return std::nullopt;  // only ln / inverse trig alone
        }
        if (best == 3 && fs.size() == 1) return std::nullopt;
        // v = ∫dv
        Integrator vi(x_, false);
        auto v = vi.I(dv, nullptr, depth + 1);
        if (!v) return std::nullopt;
        E vv = simplify(*v);
        E du = simplify(derivativeOf(u, x_));
        E w = simplify(mul(vv, du));
        Step ws;
        auto W = sub(w, st ? &ws : nullptr, depth + 1);
        if (!W) return std::nullopt;
        if (st && rec_) {
            set(st, "Integration by parts", m("\\int u \\, dv = uv - \\int v \\, du") + " with " + m("u = " + toLatex(u)) + " and " +
                                                m("dv = " + toLatex(dv) + " \\, d" + x_));
            st->sub.insert(st->sub.begin(), {"Find du and v", "", "du = " + toLatex(du) + " \\, d" + x_ + ", \\quad v = " + toLatex(vv), {}});
            st->sub.insert(st->sub.begin() + 1, {"Apply the formula", "", toLatex(simplify(mul(u, vv))) + " - " + intTex(toLatex(w), x_), {}});
        }
        if (st && rec_) for (auto& s : ws.sub) st->sub.push_back(s);
        return sub(mul(u, vv), *W);
    }

    static E sub(const E& a, const E& b) { return me::sub(a, b); }

    std::optional<E> attempt(const E& f, Step* st, int depth) {
        E x = X();
        if (!hasX(f)) {
            set(st, "Integral of a constant", m("\\int c \\, d" + x_ + " = c" + x_));
            return mul(f, x);
        }
        if (f->kind == Kind::Add) {
            set(st, "Sum rule", "Integrate each term separately.");
            std::vector<E> parts;
            for (auto& t : f->args) {
                auto r = sub(t, st, depth);
                if (!r) return fallbackWhole(f, st, depth);
                parts.push_back(*r);
            }
            return add(parts);
        }
        if (f->kind == Kind::Mul) {
            std::vector<E> cs, vs;
            for (auto& a : f->args) (hasX(a) ? vs : cs).push_back(a);
            if (!cs.empty()) {
                E c = simplify(mul(cs));
                set(st, "Constant multiple rule", "Move the constant " + m(toLatex(c)) + " outside the integral.");
                auto r = sub(simplify(mul(vs)), st, depth);
                if (!r) return std::nullopt;
                return mul(c, *r);
            }
        }
        // direct table in x
        if (auto t = table(f, x)) {
            set(st, "Basic integral", m(t->second));
            return t->first;
        }
        // linear substitution u = a x + b
        if (auto r = linearSub(f, st, depth)) return r;
        if (auto r = specialQuadratic(f, st)) return r;
        // polynomial: expand
        if (polyDegree(f, x_) >= 0) {
            E ex = expand(f);
            if (!eq(ex, f)) {
                set(st, "Expand", "Multiply out the expression, then integrate term by term.");
                if (st && rec_) st->sub.push_back({"Expanded", "", toLatex(ex), {}});
                return sub(ex, st, depth);
            }
        }
        if (auto r = rational(f, st, depth)) return r;
        if (auto r = trigIdentity(f, st, depth)) return r;
        if (auto r = uSub(f, st, depth)) return r;
        if (auto r = byParts(f, st, depth)) return r;
        // expand products like (x+1) e^x
        E ex = expand(f);
        if (!eq(ex, f) && ex->kind == Kind::Add) {
            set(st, "Expand", "Distribute, then integrate each term.");
            return sub(ex, st, depth);
        }
        return std::nullopt;
    }

    std::optional<E> fallbackWhole(const E& f, Step* st, int depth) {
        // e.g. sums that only integrate as a whole (rational functions)
        if (st && rec_) st->sub.clear();
        if (auto r = rational(f, st, depth)) return r;
        return std::nullopt;
    }

    std::optional<E> linearSub(const E& f, Step* st, int depth) {
        // find the argument
        E u;
        if (f->kind == Kind::Pow) {
            if (!hasX(f->args[1]) && hasX(f->args[0])) u = f->args[0];
            else if (!hasX(f->args[0]) && hasX(f->args[1])) u = f->args[1];
            else if (isFunc(f->args[0], "sec") || isFunc(f->args[0], "csc") || isFunc(f->args[0], "cos") || isFunc(f->args[0], "sin")) u = f->args[0]->args[0];
        } else if (f->kind == Kind::Func && f->args.size() == 1) {
            u = f->args[0];
        } else if (f->kind == Kind::Mul && f->args.size() == 2 && f->args[0]->kind == Kind::Func && f->args[0]->args.size() == 1) {
            u = f->args[0]->args[0];
        }
        if (!u || isX(u)) return std::nullopt;
        auto a = linearCoeff(u);
        if (!a) return std::nullopt;
        E U = sym("u_");
        E g = simplify(replaceSub(f, u, U));
        if (hasX(g)) return std::nullopt;
        auto t = table(g, U);
        if (!t) return std::nullopt;
        E r = simplify(mul(pow(*a, num(-1)), substitute(t->first, "u_", u)));
        set(st, "Linear substitution", "Let " + m("u = " + toLatex(u)) + ", so " + m("du = " + toLatex(*a) + " \\, d" + x_) + ". Then use " +
                                           m(t->second) + ".");
        return r;
    }
};

bool verifyAntiderivative(const E& F, const E& f, const std::string& var) {
    E dF = derivativeOf(F, var);
    int good = 0, tested = 0;
    for (double x : {0.37, 1.23, 2.71, -0.61, 0.83, 3.3, -1.7, 0.11}) {
        auto a = evalReal(dF, {{var, x}});
        auto b = evalReal(f, {{var, x}});
        if (!a || !b || !std::isfinite(*a) || !std::isfinite(*b)) continue;
        ++tested;
        if (std::fabs(*a - *b) <= 1e-6 * std::max(1.0, std::fabs(*b))) ++good;
    }
    if (tested == 0) {
        // try complex evaluation (e.g. ln of negative values)
        for (double x : {0.37, 1.23, 2.71}) {
            auto a = evalComplex(dF, {{var, Cx(x, 0.1)}});
            auto b = evalComplex(f, {{var, Cx(x, 0.1)}});
            if (!a || !b) continue;
            ++tested;
            if (std::abs(*a - *b) <= 1e-6 * std::max(1.0, std::abs(*b))) ++good;
        }
    }
    return tested > 0 && good == tested;
}

// adaptive Simpson
double simpson(const std::function<double(double)>& f, double a, double b, double eps, double whole, double fa, double fb, double fm, int depth) {
    double m = (a + b) / 2, lm = (a + m) / 2, rm = (m + b) / 2;
    double flm = f(lm), frm = f(rm);
    double left = (m - a) / 6 * (fa + 4 * flm + fm), right = (b - m) / 6 * (fm + 4 * frm + fb);
    if (depth <= 0 || std::fabs(left + right - whole) <= 15 * eps) return left + right + (left + right - whole) / 15;
    return simpson(f, a, m, eps / 2, left, fa, fm, flm, depth - 1) + simpson(f, m, b, eps / 2, right, fm, fb, frm, depth - 1);
}

}  // namespace

double numericIntegral(const E& f, const std::string& var, double a, double b, bool* ok) {
    bool good = true;
    auto F = [&](double x) {
        auto v = evalReal(f, {{var, x}});
        if (!v || !std::isfinite(*v)) { good = false; return 0.0; }
        return *v;
    };
    // map infinite bounds with x = t/(1-t^2)
    double r;
    if (std::isinf(a) || std::isinf(b)) {
        auto G = [&](double t) {
            double x = t / (1 - t * t);
            double dx = (1 + t * t) / ((1 - t * t) * (1 - t * t));
            return F(x) * dx;
        };
        double ta = std::isinf(a) ? -0.999999 : (a == 0 ? 0 : (-1 + std::sqrt(1 + 4 * a * a)) / (2 * a));
        double tb = std::isinf(b) ? 0.999999 : (b == 0 ? 0 : (-1 + std::sqrt(1 + 4 * b * b)) / (2 * b));
        double fa = G(ta), fb = G(tb), fm = G((ta + tb) / 2);
        r = simpson(G, ta, tb, 1e-10, (tb - ta) / 6 * (fa + 4 * fm + fb), fa, fb, fm, 40);
    } else {
        double fa = F(a), fb = F(b), fm = F((a + b) / 2);
        r = simpson(F, a, b, 1e-11, (b - a) / 6 * (fa + 4 * fm + fb), fa, fb, fm, 40);
    }
    if (ok) *ok = good && std::isfinite(r);
    return r;
}

std::optional<E> antiderivativeOf(const E& f, const std::string& var) {
    Integrator in(var, false);
    auto r = in.I(f, nullptr);
    if (!r || !verifyAntiderivative(*r, simplify(f), var)) return std::nullopt;
    return r;
}

Solution solveIntegral(const E& f0, const std::string& var, const std::optional<std::pair<E, E>>& bounds, const std::string& inputLatex) {
    Solution sol;
    sol.kind = "integral";
    sol.anim = "integral";
    sol.inputLatex = inputLatex;
    sol.title = bounds ? "Evaluate the definite integral" : "Find the integral";
    E f = simplify(f0);
    Method meth{"Integration", {}};
    Integrator in(var, true);
    std::optional<E> F;
    if (f->kind == Kind::Add) {
        std::string split;
        auto terms = displayOrder(f);
        for (size_t i = 0; i < terms.size(); ++i) split += (i ? " + " : "") + intTex(toLatex(terms[i]), var);
        std::vector<Step> termSteps;
        std::vector<E> parts;
        bool ok = true;
        for (auto& t : terms) {
            Step s;
            auto r = in.I(t, &s);
            if (!r) { ok = false; break; }
            if (s.title.empty()) s.title = "Integrate";
            s.title += ": " + m(toLatex(t));
            termSteps.push_back(std::move(s));
            parts.push_back(*r);
        }
        if (ok) {
            meth.steps.push_back({"Use the sum rule", "Integrate each term separately.", split, {}});
            for (auto& s : termSteps) meth.steps.push_back(std::move(s));
            F = simplify(add(parts));
        } else {
            Step s;
            F = in.I(f, &s);
            if (F) meth.steps.push_back(std::move(s));
        }
    } else {
        Step s;
        F = in.I(f, &s);
        if (F) meth.steps.push_back(std::move(s));
    }
    if (F && !verifyAntiderivative(*F, f, var)) F.reset();

    if (!F) {
        if (bounds) {
            auto a = evalReal(bounds->first), b = evalReal(bounds->second);
            if (a && b) {
                bool ok;
                double v = numericIntegral(f, var, *a, *b, &ok);
                if (ok) {
                    sol.methods.push_back({"Numerical integration",
                                           {{"No elementary antiderivative found",
                                             "Use adaptive Simpson's rule to approximate the area under the curve.",
                                             inputLatex + " \\approx " + numLatex(v, 10),
                                             {}}}});
                    sol.answerLatex = inputLatex + " \\approx " + numLatex(v, 10);
                    sol.answers.push_back(numLatex(v, 10));
                    PlotSpec ps = plotFunctions({{f, "f(" + var + ")"}}, var);
                    ps.shade = {*a, *b};
                    sol.plot = ps;
                    return sol;
                }
            }
        }
        sol.ok = false;
        sol.error = "I couldn't find an antiderivative for this integral.";
        return sol;
    }
    E Fs = simplify(*F);
    if (!bounds) {
        meth.steps.push_back({"Add the constant of integration", "Every antiderivative differs by a constant " + m("C") + ".",
                              toLatex(Fs) + " + C", {}});
        sol.answerLatex = inputLatex + " = " + toLatex(Fs) + " + C";
        sol.answers.push_back(toLatex(Fs) + " + C");
        sol.methods.push_back(std::move(meth));
        sol.plot = plotFunctions({{f, "f(" + var + ")"}, {Fs, "F(" + var + ")"}}, var);
        return sol;
    }
    // definite integral
    const E& a = bounds->first;
    const E& b = bounds->second;
    auto av = evalReal(a), bv = evalReal(b);
    // check the integrand is finite on [a, b]
    bool singular = false;
    if (av && bv && std::isfinite(*av) && std::isfinite(*bv)) {
        for (int k = 0; k <= 400; ++k) {
            double x = *av + (*bv - *av) * k / 400.0;
            auto v = evalReal(f, {{var, x}});
            if (!v || !std::isfinite(*v)) { singular = true; break; }
        }
    }
    std::string bracket = "\\left[" + toLatex(Fs) + "\\right]_{" + toLatex(a) + "}^{" + toLatex(b) + "}";
    meth.steps.push_back({"Evaluate between the bounds", "Use the Fundamental Theorem of Calculus: " + m("\\int_{a}^{b} f(x)\\,dx = F(b) - F(a)"),
                          bracket, {}});
    E Fb, Fa;
    if (isSym(b, "inf") || isSym(a, "inf") || (a->kind == Kind::Mul && containsSym(a, "inf"))) {
        auto lim = [&](const E& pt) -> E {
            if (!containsSym(pt, "inf")) return simplify(substitute(Fs, var, pt));
            double sgn = isSym(pt, "inf") ? 1 : -1;
            auto v1 = evalReal(Fs, {{var, sgn * 1e6}});
            auto v2 = evalReal(Fs, {{var, sgn * 1e8}});
            if (v1 && v2 && std::fabs(*v1 - *v2) < 1e-4) {
                // try to recognise the value
                double v = *v2;
                for (int k = -8; k <= 8; ++k)
                    for (int d = 1; d <= 8; ++d) {
                        if (std::fabs(v - k * M_PI / d) < 1e-5) return simplify(mul(num(Rational(k, d)), PI()));
                        if (std::fabs(v - (double)k / d) < 1e-6) return num(Rational(k, d));
                    }
                return num(Rational::fromDecimal(fmtDouble(v, 10)));
            }
            return INF();
        };
        Fb = lim(b);
        Fa = lim(a);
    } else {
        Fb = simplify(substitute(Fs, var, b));
        Fa = simplify(substitute(Fs, var, a));
    }
    E result = simplify(sub(Fb, Fa));
    meth.steps.push_back({"Substitute the bounds", "", "\\left(" + toLatex(Fb) + "\\right) - \\left(" + toLatex(Fa) + "\\right)", {}});
    if (singular || isUndefined(result) || containsSym(result, "inf")) {
        bool ok;
        double v = (av && bv) ? numericIntegral(f, var, *av, *bv, &ok) : NAN;
        if (singular || !std::isfinite(v)) {
            sol.ok = false;
            sol.error = "The integrand is not finite on the interval, so this improper integral diverges.";
            return sol;
        }
    }
    meth.steps.push_back({"Simplify", "", toLatex(result), {}});
    sol.answerLatex = inputLatex + " = " + toLatex(result);
    sol.answers.push_back(toLatex(result));
    if (auto v = evalReal(result); v && hasFreeVars(result) == false && !isNum(result))
        sol.facts.push_back({"Decimal", numLatex(*v, 10)});
    sol.methods.push_back(std::move(meth));
    PlotSpec ps = plotFunctions({{f, "f(" + var + ")"}}, var);
    if (av && bv && std::isfinite(*av) && std::isfinite(*bv)) {
        ps.shade = {*av, *bv};
        double w = std::fabs(*bv - *av);
        ps.xmin = std::min(*av, *bv) - 0.5 * w - 1;
        ps.xmax = std::max(*av, *bv) + 0.5 * w + 1;
        fitY(ps, f, var);
    }
    sol.plot = ps;
    return sol;
}

}  // namespace me
