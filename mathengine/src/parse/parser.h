// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Parser: text -> raw syntax tree (keeps the user's structure for display
// and for step-by-step arithmetic) -> canonical expression (expr.h).
#pragma once
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/expr.h"

namespace me {

struct Error {
    std::string message;
    size_t position = 0;
};

template <class T>
using Result = std::expected<T, Error>;

struct RNode;
using R = std::shared_ptr<RNode>;

// Raw syntax node.
//  Num   : value (text keeps the typed digits)
//  Sym   : name
//  Bin   : op in {+,-,*,/,÷,^} (implicit multiplication uses op "*" with implicit = true)
//  Neg   : unary minus
//  Func  : name(args...)   -- also calculus commands: diff, int, lim, solve, factor, expand, ...
//  Fact  : postfix !
//  Pct   : postfix %
//  Abs   : |x|
//  Rel   : op in {=,<,>,<=,>=,!=}, kids = [lhs, rhs]
//  List  : system of relations / expressions separated by ; or ,
struct RNode {
    enum class T { Num, Sym, Bin, Neg, Func, Fact, Pct, Abs, Rel, List } t;
    std::string op;    // operator / function / symbol name
    Rational value;    // Num
    std::string text;  // Num literal as typed
    std::vector<R> kids;
    bool implicit = false;
    bool paren = false;  // was written inside parentheses
};

Result<R> parse(std::string_view input);

// True for names the parser treats as functions/commands (sin, factor, taylor, det, ...).
bool isEngineFunctionName(const std::string& s);

// Raw tree -> LaTeX (faithful to the input)
std::string rawLatex(const R& r);
// Raw tree -> canonical expression. Calculus commands are evaluated silently.
Result<E> toExpr(const R& r);

R rnum(const Rational& v);
R rbin(const std::string& op, R a, R b);

}  // namespace me
