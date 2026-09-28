// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Symbolic expression tree: immutable nodes shared via shared_ptr.
#pragma once
#include <complex>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/rational.h"

namespace me {

enum class Kind { Num, Sym, Add, Mul, Pow, Func };

struct Node;
using E = std::shared_ptr<const Node>;

struct Node {
    Kind kind;
    Rational num;           // Num
    std::string name;       // Sym / Func
    std::vector<E> args;    // Add / Mul / Pow(base, exp) / Func
};

// ---------- construction (raw, not simplified) ----------
E num(const Rational& r);
E num(long long v);
E sym(const std::string& name);
E add(std::vector<E> terms);
E add(E a, E b);
E mul(std::vector<E> factors);
E mul(E a, E b);
E pow(E base, E exp);
E func(const std::string& name, std::vector<E> args);
E func(const std::string& name, E arg);
E neg(E a);
E sub(E a, E b);
E divide(E a, E b);
E sqrtE(E a);
// numeric approximation: prints as a decimal, evaluates as its value
E approxE(double v);

// Named constants
E PI();
E EUL();   // Euler's number e
E IMAG();  // imaginary unit i
E INF();   // +infinity (used by limits)
E UNDEF(); // undefined result

// ---------- inspection ----------
inline bool isNum(const E& e) { return e->kind == Kind::Num; }
inline bool isSym(const E& e, const std::string& n) { return e->kind == Kind::Sym && e->name == n; }
inline bool isFunc(const E& e, const std::string& n) { return e->kind == Kind::Func && e->name == n; }
bool isNumVal(const E& e, long long v);
bool isInteger(const E& e);
bool isNegativeTerm(const E& e);  // leading numeric coefficient < 0
bool containsSym(const E& e, const std::string& s);
bool hasFreeVars(const E& e);     // any symbol other than constants
bool containsFunc(const E& e, const std::string& fn);
bool isConstantWrt(const E& e, const std::string& var);
void collectVars(const E& e, std::vector<std::string>& out);
bool isUndefined(const E& e);
int cmpE(const E& a, const E& b);  // total structural ordering
inline bool eq(const E& a, const E& b) { return cmpE(a, b) == 0; }

// split term into numeric coefficient * rest
std::pair<Rational, E> splitCoeff(const E& e);
// split into numerator / denominator by exponent sign
std::pair<E, E> numDen(const E& e);

// ---------- transformation ----------
E simplify(const E& e);
E expand(const E& e);
E substitute(const E& e, const std::string& var, const E& value);
E replaceSub(const E& e, const E& target, const E& with);  // structural replace
// Degree of a polynomial-like expression in var (-1 if not polynomial)
int polyDegree(const E& e, const std::string& var);

// ---------- evaluation ----------
using Cx = std::complex<double>;
std::optional<double> evalReal(const E& e, const std::map<std::string, double>& vars = {});
std::optional<Cx> evalComplex(const E& e, const std::map<std::string, Cx>& vars = {});

// Exact complex rational value if e only contains numbers, i and + - * / integer ^
struct CRat { Rational re, im; };
std::optional<CRat> asCRat(const E& e);
E fromCRat(const CRat& c);

// ---------- printing ----------
std::string toLatex(const E& e);
std::string toPlain(const E& e);   // re-parseable plain text
std::string numLatex(double v, int sig = 6);
std::string fmtDouble(double v, int sig = 6);

// Sort terms of a sum for display (descending degree)
std::vector<E> displayOrder(const E& addNode);

}  // namespace me
