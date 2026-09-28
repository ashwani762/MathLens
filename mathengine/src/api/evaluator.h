// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Compiles an expression into a flat stack program for fast repeated evaluation
// (graph plotting, implicit curves, wallpapers). Variables: x, y, theta (alias t, r).
#pragma once
#include <span>
#include <string_view>
#include <vector>

#include "core/expr.h"
#include "parse/parser.h"

namespace me {

class Program {
public:
    enum class Op : unsigned char { Const, VarX, VarY, VarT, Add, Mul, Pow, PowRat, Neg, Func, Log };
    enum class Fn : unsigned char { Sin, Cos, Tan, Sec, Csc, Cot, Asin, Acos, Atan, Sinh, Cosh, Tanh, Ln, Abs, Fact, Floor, Ceil, Round, Sqrt };
    struct Instr {
        Op op;
        Fn fn = Fn::Sin;
        unsigned short n = 0;   // arity for Add / Mul
        double value = 0;       // Const; PowRat numerator
        double value2 = 0;      // PowRat denominator
    };

    static Result<Program> compile(std::string_view source);
    static std::optional<Program> fromExpr(const E& e);

    double eval(double x, double y = 0, double t = 0) const;
    void evalMany(std::span<const double> xs, std::span<const double> ys, std::span<double> out) const;
    bool usesY() const { return usesY_; }

private:
    std::vector<Instr> code_;
    bool usesY_ = false;
    size_t maxStack_ = 0;
    bool emit(const E& e, size_t depth);
};

}  // namespace me
