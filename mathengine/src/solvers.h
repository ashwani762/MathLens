// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Entry points of the individual solvers. engine.cpp dispatches to these.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/expr.h"
#include "parse/parser.h"
#include "solution/steps.h"

namespace me {

// algebra.cpp
Solution solveEquation(const E& lhs, const E& rhs, const std::string& var, const std::string& inputLatex, bool complexMode,
                       std::vector<E>* rootsOut = nullptr);
Solution solveInequality(const E& lhs, const E& rhs, const std::string& op, const std::string& var, const std::string& inputLatex);
Solution simplifyExpression(const E& e, const std::string& inputLatex);
Solution factorExpression(const E& e, const std::string& inputLatex);
Solution expandExpression(const E& e, const std::string& inputLatex);

// conic.cpp
struct ConicAnalysis {
    PlotShape shape;
    Method method;
    std::string answer;
    std::vector<std::pair<std::string, std::string>> facts;
};
std::optional<ConicAnalysis> analyzeConic(const E& lhs, const E& rhs);

// system.cpp
struct SystemResult {
    enum class Status { Unique, Infinite, None, Failed } status = Status::Failed;
    std::vector<std::string> vars;
    std::vector<std::vector<E>> solutions;  // each: one value per var
    std::vector<Method> methods;
};
SystemResult solveSystemCore(const std::vector<std::pair<E, E>>& equations, std::vector<std::string> vars = {});
Solution solveSystem(const std::vector<std::pair<E, E>>& equations, const std::string& inputLatex);

// arithmetic.cpp
Solution solveArithmetic(const R& raw, const std::string& inputLatex);

// complex.cpp
Solution solveComplexArithmetic(const R& raw, const std::string& inputLatex);
Solution solveComplexEquation(const E& lhs, const E& rhs, const std::string& var, const std::string& inputLatex);
// polar / exponential form facts for an exact complex value
void addComplexFacts(Solution& sol, const E& z);
// all n-th roots of a complex number w (exact where possible)
std::vector<E> complexRoots(const E& w, int n, std::vector<Step>* steps);

// numbertheory.cpp
Solution solveFactorial(const E& n, const std::string& inputLatex);
Solution solvePrimeFactorization(const BigInt& n, const std::string& inputLatex);
Solution solveGcdLcm(const std::vector<BigInt>& nums, bool lcm, const std::string& inputLatex);
Solution solveCombinatorics(const BigInt& n, const BigInt& k, bool binom, const std::string& inputLatex);

// surface.cpp
Solution solveSurface(const E& f, const std::string& inputLatex);

// ode.cpp
bool isODE(const E& e);
Solution solveODE(const E& lhs, const E& rhs, const std::string& inputLatex);
Solution solveTaylor(const E& f, const std::string& var, const E& a, int n, const std::string& inputLatex);

// statistics.cpp
Solution solveStatistics(const std::string& fn, const std::vector<double>& xs, const std::string& inputLatex);
Solution solveFinance(const std::string& fn, const std::vector<double>& args, const std::string& inputLatex);
bool isFinanceFn(const std::string& n);
bool isStatsFn(const std::string& n);

// shared helpers
std::string eqTex(const E& l, const E& r, const std::string& rel = "=");
std::string varPick(const std::vector<std::string>& vars);

}  // namespace me
