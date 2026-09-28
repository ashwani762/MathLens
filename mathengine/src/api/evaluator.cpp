// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "api/evaluator.h"

#include <algorithm>
#include <cmath>

namespace me {

bool Program::emit(const E& e, size_t depth) {
    maxStack_ = std::max(maxStack_, depth + 1);
    switch (e->kind) {
        case Kind::Num: code_.push_back({Op::Const, Fn::Sin, 0, e->num.toDouble(), 0}); return true;
        case Kind::Sym: {
            const std::string& n = e->name;
            if (n == "x") code_.push_back({Op::VarX});
            else if (n == "y") { code_.push_back({Op::VarY}); usesY_ = true; }
            else if (n == "theta" || n == "t") code_.push_back({Op::VarT});
            else if (n == "pi") code_.push_back({Op::Const, Fn::Sin, 0, M_PI, 0});
            else if (n == "e") code_.push_back({Op::Const, Fn::Sin, 0, M_E, 0});
            else return false;
            return true;
        }
        case Kind::Add:
        case Kind::Mul: {
            for (size_t i = 0; i < e->args.size(); ++i)
                if (!emit(e->args[i], depth + i)) return false;
            code_.push_back({e->kind == Kind::Add ? Op::Add : Op::Mul, Fn::Sin, (unsigned short)e->args.size(), 0, 0});
            return true;
        }
        case Kind::Pow: {
            if (!emit(e->args[0], depth)) return false;
            const E& ex = e->args[1];
            if (isNum(ex) && !ex->num.isInteger()) {
                code_.push_back({Op::PowRat, Fn::Sin, 0, ex->num.num().toDouble(), ex->num.den().toDouble()});
                return true;
            }
            if (!emit(ex, depth + 1)) return false;
            code_.push_back({Op::Pow});
            return true;
        }
        case Kind::Func: {
            static const std::pair<const char*, Fn> table[] = {
                {"sin", Fn::Sin}, {"cos", Fn::Cos}, {"tan", Fn::Tan}, {"sec", Fn::Sec}, {"csc", Fn::Csc}, {"cot", Fn::Cot},
                {"asin", Fn::Asin}, {"acos", Fn::Acos}, {"atan", Fn::Atan}, {"sinh", Fn::Sinh}, {"cosh", Fn::Cosh}, {"tanh", Fn::Tanh},
                {"ln", Fn::Ln}, {"abs", Fn::Abs}, {"fact", Fn::Fact}, {"floor", Fn::Floor}, {"ceil", Fn::Ceil}, {"round", Fn::Round},
                {"sqrt", Fn::Sqrt}};
            if (e->name == "log" && e->args.size() == 2) {
                if (!emit(e->args[0], depth) || !emit(e->args[1], depth + 1)) return false;
                code_.push_back({Op::Log});
                return true;
            }
            if (e->args.size() != 1) return false;
            for (auto& [name, fn] : table) {
                if (e->name == name) {
                    if (!emit(e->args[0], depth)) return false;
                    code_.push_back({Op::Func, fn});
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

std::optional<Program> Program::fromExpr(const E& e) {
    Program p;
    if (!p.emit(e, 0)) return std::nullopt;
    return p;
}

Result<Program> Program::compile(std::string_view source) {
    auto r = parse(source);
    if (!r) return std::unexpected(r.error());
    auto e = toExpr(*r);
    if (!e) return std::unexpected(e.error());
    auto p = fromExpr(simplify(*e));
    if (!p) return std::unexpected(Error{"Expression can't be plotted"});
    return *p;
}

static double applyFn(Program::Fn f, double a) {
    using F = Program::Fn;
    switch (f) {
        case F::Sin: return std::sin(a);
        case F::Cos: return std::cos(a);
        case F::Tan: return std::tan(a);
        case F::Sec: return 1 / std::cos(a);
        case F::Csc: return 1 / std::sin(a);
        case F::Cot: return 1 / std::tan(a);
        case F::Asin: return std::asin(a);
        case F::Acos: return std::acos(a);
        case F::Atan: return std::atan(a);
        case F::Sinh: return std::sinh(a);
        case F::Cosh: return std::cosh(a);
        case F::Tanh: return std::tanh(a);
        case F::Ln: return a > 0 ? std::log(a) : NAN;
        case F::Abs: return std::fabs(a);
        case F::Fact: return std::tgamma(a + 1);
        case F::Floor: return std::floor(a);
        case F::Ceil: return std::ceil(a);
        case F::Round: return std::round(a);
        case F::Sqrt: return a >= 0 ? std::sqrt(a) : NAN;
    }
    return NAN;
}

double Program::eval(double x, double y, double t) const {
    double stackBuf[64];
    std::vector<double> heap;
    double* st = stackBuf;
    if (maxStack_ + 4 > 64) { heap.resize(maxStack_ + 4); st = heap.data(); }
    size_t sp = 0;
    for (const Instr& in : code_) {
        switch (in.op) {
            case Op::Const: st[sp++] = in.value; break;
            case Op::VarX: st[sp++] = x; break;
            case Op::VarY: st[sp++] = y; break;
            case Op::VarT: st[sp++] = t; break;
            case Op::Add: {
                double s = 0;
                for (unsigned k = 0; k < in.n; ++k) s += st[--sp];
                st[sp++] = s;
                break;
            }
            case Op::Mul: {
                double s = 1;
                for (unsigned k = 0; k < in.n; ++k) s *= st[--sp];
                st[sp++] = s;
                break;
            }
            case Op::Pow: {
                double b = st[--sp];
                double a = st[--sp];
                st[sp++] = std::pow(a, b);
                break;
            }
            case Op::PowRat: {
                double a = st[sp - 1];
                double p = in.value, q = in.value2;
                if (a < 0) {
                    bool oddDen = std::fmod(q, 2.0) != 0;
                    if (!oddDen) { st[sp - 1] = NAN; break; }
                    double r = std::pow(-a, p / q);
                    st[sp - 1] = std::fmod(std::fabs(p), 2.0) != 0 ? -r : r;
                } else st[sp - 1] = std::pow(a, p / q);
                break;
            }
            case Op::Neg: st[sp - 1] = -st[sp - 1]; break;
            case Op::Func: st[sp - 1] = applyFn(in.fn, st[sp - 1]); break;
            case Op::Log: {
                double v = st[--sp];
                double b = st[--sp];
                st[sp++] = (v > 0 && b > 0) ? std::log(v) / std::log(b) : NAN;
                break;
            }
        }
    }
    return sp ? st[sp - 1] : NAN;
}

void Program::evalMany(std::span<const double> xs, std::span<const double> ys, std::span<double> out) const {
    for (size_t i = 0; i < out.size() && i < xs.size(); ++i) out[i] = eval(xs[i], ys.empty() ? 0.0 : ys[i], xs[i]);
}

}  // namespace me
