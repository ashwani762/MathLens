// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include "parse/parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

#include "calculus/calculus.h"
#include "parse/latex_import.h"

namespace me {

R rnum(const Rational& v) {
    auto n = std::make_shared<RNode>();
    n->t = RNode::T::Num;
    n->value = v;
    n->text = v.toString();
    return n;
}

R rbin(const std::string& op, R a, R b) {
    auto n = std::make_shared<RNode>();
    n->t = RNode::T::Bin;
    n->op = op;
    n->kids = {std::move(a), std::move(b)};
    return n;
}

namespace {

// ------------------------------------------------------------ lexer

enum class Tok { Num, Ident, Op, End };

struct Token {
    Tok type;
    std::string s;
    size_t pos;
    bool spaceBefore;
};

constexpr std::array kKnownWords = {
    // longest names first matters for greedy matching; sorted at runtime by length
    "arcsin", "arccos", "arctan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "sin", "cos", "tan", "sec", "csc", "cot",
    "ln", "log", "sqrt", "cbrt", "root", "abs", "exp", "fact", "factorial", "diff", "derivative", "integrate", "integral", "int",
    "lim", "limit", "solve", "factor", "expand", "simplify", "gcd", "lcm", "binom", "nCr", "nPr", "perm", "conj", "re", "im",
    "arg", "floor", "ceil", "round", "pi", "inf", "infinity", "theta", "alpha", "beta", "gamma", "lambda", "phi", "omega",
    "sigma", "tau", "mu", "rho", "delta", "polar", "roots", "isprime", "primefactors", "mod",
    "det", "inv", "inverse", "transpose", "rref", "rank", "trace", "eig", "eigenvalues", "identity", "shape", "slice", "sum",
    "taylor", "series", "mean", "median", "mode", "stdev", "variance", "stats", "dsolve",
    "fv", "pv", "pmt", "npv", "irr", "cagr", "bond", "sharpe", "normcdf", "bscall", "bsput", "cont", "graph3d", "surface", "bsmcall", "bsmput", "merton", "credit", "binomial", "ivol",
};

// Replace common unicode math glyphs by ascii spellings.
std::string normalizeUnicode(std::string_view in) {
    struct Rep { const char* from; const char* to; };
    static constexpr Rep reps[] = {
        {"\xCF\x80", " pi "},      // π
        {"\xE2\x88\x9A", " \x01 "},  // √  (marker)
        {"\xE2\x88\x9B", " cbrt "},  // ∛
        {"\xC3\x97", "*"},         // ×
        {"\xC2\xB7", "*"},         // ·
        {"\xE2\x8B\x85", "*"},     // ⋅
        {"\xE2\x88\x99", "*"},     // ∙
        {"\xC3\xB7", " \x02 "},    // ÷  (marker)
        {"\xE2\x88\x92", "-"},     // −
        {"\xE2\x80\x93", "-"},     // –
        {"\xC2\xB2", "^2"},        // ²
        {"\xC2\xB3", "^3"},        // ³
        {"\xE2\x81\xB4", "^4"},    // ⁴
        {"\xE2\x89\xA4", "<="},    // ≤
        {"\xE2\x89\xA5", ">="},    // ≥
        {"\xE2\x89\xA0", "!="},    // ≠
        {"\xE2\x88\x9E", " inf "}, // ∞
        {"\xE2\x88\xAB", " \x03 "},  // ∫ (marker)
        {"\xCE\xB8", " theta "},   // θ
        {"\xCE\xB1", " alpha "},   // α
        {"\xCE\xB2", " beta "},    // β
        {"\xE2\x86\x92", "->"},    // →
        {"\xE2\x80\xB2", "'"},     // ′
    };
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        bool matched = false;
        for (const auto& r : reps) {
            size_t len = std::strlen(r.from);
            if (in.substr(i, len) == r.from) { out += r.to; i += len; matched = true; break; }
        }
        if (!matched) out += in[i++];
    }
    return out;
}

Result<std::vector<Token>> lex(const std::string& s) {
    static std::vector<std::string> words = [] {
        std::vector<std::string> w(kKnownWords.begin(), kKnownWords.end());
        std::ranges::sort(w, [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        return w;
    }();
    std::vector<Token> out;
    size_t i = 0;
    bool space = false;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        if (std::isspace(c)) { space = true; ++i; continue; }
        if (std::isdigit(c) || (c == '.' && i + 1 < s.size() && std::isdigit((unsigned char)s[i + 1]))) {
            size_t j = i;
            bool dot = false;
            while (j < s.size() && (std::isdigit((unsigned char)s[j]) || (s[j] == '.' && !dot))) { if (s[j] == '.') dot = true; ++j; }
            out.push_back({Tok::Num, s.substr(i, j - i), i, space});
            i = j;
        } else if (std::isalpha(c)) {
            size_t j = i;
            while (j < s.size() && std::isalpha((unsigned char)s[j])) ++j;
            std::string word = s.substr(i, j - i);
            // split word into known names / single letters
            size_t k = 0;
            bool first = true;
            while (k < word.size()) {
                std::string match;
                for (auto& w : words)
                    if (word.compare(k, w.size(), w) == 0) { match = w; break; }
                if (match.empty()) match = word.substr(k, 1);
                out.push_back({Tok::Ident, match, i + k, first ? space : false});
                first = false;
                k += match.size();
            }
            i = j;
        } else {
            static constexpr const char* ops2[] = {"<=", ">=", "!=", "->", "=="};
            std::string op(1, (char)c);
            for (auto o : ops2) if (s.compare(i, 2, o) == 0) { op = o; break; }
            if (op == "==") op = "=";
            static const std::string single = "+-*/^()[]{}|,;=<>!_%'\x01\x02\x03";
            if (op.size() == 1 && single.find((char)c) == std::string::npos)
                return std::unexpected(Error{"Unexpected character '" + op + "'", i});
            out.push_back({Tok::Op, op, i, space});
            i += op.size();
        }
        space = false;
    }
    out.push_back({Tok::End, "", s.size(), false});
    return out;
}

// ------------------------------------------------------------ parser

bool isFunctionName(const std::string& s) {
    static const char* fns[] = {"sin", "cos", "tan", "sec", "csc", "cot", "asin", "acos", "atan", "arcsin", "arccos", "arctan",
                                "sinh", "cosh", "tanh", "ln", "log", "sqrt", "cbrt", "root", "abs", "exp", "fact", "factorial",
                                "diff", "derivative", "integrate", "integral", "int", "lim", "limit", "solve", "factor", "expand",
                                "simplify", "gcd", "lcm", "binom", "nCr", "nPr", "perm", "conj", "re", "im", "arg", "floor",
                                "ceil", "round", "polar", "roots", "isprime", "primefactors",
                                "det", "inv", "inverse", "transpose", "rref", "rank", "trace", "eig", "eigenvalues", "identity", "shape", "slice", "sum",
                                "taylor", "series", "mean", "median", "mode", "stdev", "variance", "stats", "dsolve",
                                "fv", "pv", "pmt", "npv", "irr", "cagr", "bond", "sharpe", "normcdf", "bscall", "bsput", "cont", "graph3d", "surface", "bsmcall", "bsmput", "merton", "credit", "binomial", "ivol"};
    for (auto f : fns) if (s == f) return true;
    return false;
}

std::string canonicalFuncName(const std::string& s) {
    if (s == "arcsin") return "asin";
    if (s == "arccos") return "acos";
    if (s == "arctan") return "atan";
    if (s == "factorial") return "fact";
    if (s == "derivative") return "diff";
    if (s == "integrate" || s == "integral") return "int";
    if (s == "limit") return "lim";
    if (s == "nCr") return "binom";
    if (s == "nPr") return "perm";
    if (s == "inverse") return "inv";
    if (s == "eigenvalues") return "eig";
    if (s == "series") return "taylor";
    return s;
}

std::string canonicalSymName(const std::string& s) {
    if (s == "infinity") return "inf";
    return s;
}

R mk(RNode::T t, std::string op = {}, std::vector<R> kids = {}) {
    auto n = std::make_shared<RNode>();
    n->t = t;
    n->op = std::move(op);
    n->kids = std::move(kids);
    return n;
}

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : t_(std::move(toks)) {}

    Result<R> parseAll() {
        std::vector<R> items;
        while (true) {
            auto r = parseRelation();
            if (!r) return r;
            items.push_back(*r);
            if (isOp(";") || isOp(",")) { ++p_; if (peek().type == Tok::End) break; continue; }
            break;
        }
        if (peek().type != Tok::End) return fail("Unexpected '" + peek().s + "'");
        if (items.size() == 1) return items[0];
        return mk(RNode::T::List, "", items);
    }

private:
    std::vector<Token> t_;
    size_t p_ = 0;
    int absDepth_ = 0;
    int intDepth_ = 0;
    int parenDepth_ = 0;

    const Token& peek(size_t k = 0) const { return t_[std::min(p_ + k, t_.size() - 1)]; }
    bool isOp(const char* s, size_t k = 0) const { return peek(k).type == Tok::Op && peek(k).s == s; }
    bool isIdent(const char* s, size_t k = 0) const { return peek(k).type == Tok::Ident && peek(k).s == s; }
    std::unexpected<Error> fail(const std::string& m) const { return std::unexpected(Error{m, peek().pos}); }
    bool expect(const char* s) {
        if (isOp(s)) { ++p_; return true; }
        return false;
    }

    Result<R> parseRelation() {
        auto lhs = parseExpr();
        if (!lhs) return lhs;
        static const char* rels[] = {"=", "<", ">", "<=", ">=", "!="};
        for (auto r : rels) {
            if (isOp(r)) {
                ++p_;
                auto rhs = parseExpr();
                if (!rhs) return rhs;
                R node = mk(RNode::T::Rel, r, {*lhs, *rhs});
                // chained: a < x < b  -> keep only as list of two relations
                for (auto r2 : rels) {
                    if (isOp(r2)) {
                        ++p_;
                        auto rhs2 = parseExpr();
                        if (!rhs2) return rhs2;
                        return mk(RNode::T::List, "chain", {node, mk(RNode::T::Rel, r2, {*rhs, *rhs2})});
                    }
                }
                return node;
            }
        }
        return lhs;
    }

    bool atIntegralEnd() const {
        // "dx" terminating an integral body
        return intDepth_ > 0 && isIdent("d") && peek(1).type == Tok::Ident && peek(1).s.size() <= 5 && !peek(1).spaceBefore &&
               !isFunctionName(peek(1).s);
    }

    Result<R> parseExpr() {
        auto lhs = parseTerm();
        if (!lhs) return lhs;
        R acc = *lhs;
        while ((isOp("+") || isOp("-")) && !atIntegralEnd()) {
            std::string op = peek().s;
            ++p_;
            auto rhs = parseTerm();
            if (!rhs) return rhs;
            acc = rbin(op, acc, *rhs);
        }
        return acc;
    }

    bool startsPrimary() const {
        const Token& k = peek();
        if (k.type == Tok::Num || k.type == Tok::Ident) return !atIntegralEnd();
        if (k.type == Tok::Op) {
            if (k.s == "(" || k.s == "[" || k.s == "{" || k.s == "\x01" || k.s == "\x03") return true;
            if (k.s == "|") return absDepth_ == 0;
        }
        return false;
    }

    Result<R> parseTerm() {
        auto lhs = parseUnary();
        if (!lhs) return lhs;
        R acc = *lhs;
        while (true) {
            if (atIntegralEnd()) break;
            if (isOp("*") || isOp("/") || isOp("\x02")) {
                std::string op = peek().s == "\x02" ? "div" : peek().s;
                ++p_;
                auto rhs = parseUnary();
                if (!rhs) return rhs;
                acc = rbin(op, acc, *rhs);
            } else if (isIdent("mod")) {
                ++p_;
                auto rhs = parseUnary();
                if (!rhs) return rhs;
                acc = mk(RNode::T::Func, "mod", {acc, *rhs});
            } else if (startsPrimary()) {
                auto rhs = parsePower();
                if (!rhs) return rhs;
                R b = rbin("*", acc, *rhs);
                b->implicit = true;
                acc = b;
            } else break;
        }
        return acc;
    }

    Result<R> parseUnary() {
        if (isOp("-")) {
            ++p_;
            auto a = parseUnary();
            if (!a) return a;
            return mk(RNode::T::Neg, "-", {*a});
        }
        if (isOp("+")) { ++p_; return parseUnary(); }
        return parsePower();
    }

    Result<R> parsePower() {
        auto base = parsePostfix();
        if (!base) return base;
        if (isOp("^")) {
            ++p_;
            auto ex = parseUnary();  // right associative, allows 2^-1
            if (!ex) return ex;
            return rbin("^", *base, *ex);
        }
        return base;
    }

    Result<R> parsePostfix() {
        auto a = parsePrimary();
        if (!a) return a;
        R acc = *a;
        while (true) {
            if (isOp("!")) { ++p_; acc = mk(RNode::T::Fact, "!", {acc}); }
            else if (isOp("'")) {
                int order = 0;
                while (isOp("'")) { ++p_; ++order; }
                acc = mk(RNode::T::Func, "prime", {acc, rnum(order)});
            }
            else if (isOp("%")) { ++p_; acc = mk(RNode::T::Pct, "%", {acc}); }
            else break;
        }
        return acc;
    }

    Result<std::vector<R>> parseArgs() {
        // assumes current token is '('
        ++p_;
        ++parenDepth_;
        std::vector<R> args;
        if (isOp(")")) { ++p_; --parenDepth_; return args; }
        while (true) {
            auto r = parseRelation();
            if (!r) return std::unexpected(r.error());
            args.push_back(*r);
            if (isOp(",") || isOp(";")) { ++p_; continue; }
            if (isOp(")")) { ++p_; break; }
            return fail("Expected ')'");
        }
        --parenDepth_;
        return args;
    }

    // [a, b, c] is a row vector, [[a, b], [c, d]] a matrix, [expr] a plain group
    Result<R> parseBracket() {
        ++p_;
        ++parenDepth_;
        std::vector<R> items;
        while (true) {
            auto e = parseExpr();
            if (!e) return e;
            items.push_back(*e);
            if (isOp(",") || isOp(";")) { ++p_; continue; }
            break;
        }
        if (!expect("]")) return fail("Expected ']'");
        --parenDepth_;
        bool allRows = std::ranges::all_of(items, [](const R& r) { return r->t == RNode::T::Func && r->op == "matrix"; });
        if (items.size() == 1 && !allRows) { items[0]->paren = true; return items[0]; }
        // [[[..],[..]], [[..],[..]]]: every item is itself a nested matrix -> a 3D array (stack of slices)
        bool allNested = allRows && std::ranges::all_of(items, [](const R& r) { return r->text == "2d"; });
        if (allNested) {
            size_t rr = items[0]->kids.size(), cc = items[0]->kids[0]->kids.size();
            for (auto& it : items)
                if (it->kids.size() != rr || it->kids[0]->kids.size() != cc) return fail("All slices of a 3D matrix must have the same size");
            return mk(RNode::T::Func, "tensor", items);
        }
        std::vector<R> rows;
        if (allRows) {
            for (auto& r : items) {
                if (r->kids.size() != 1) return fail("Matrix rows must be written as [a, b, ...]");
                rows.push_back(r->kids[0]);
            }
        } else {
            rows.push_back(mk(RNode::T::Func, "row", items));
        }
        size_t cols = rows[0]->kids.size();
        for (auto& r : rows) if (r->kids.size() != cols) return fail("All matrix rows must have the same length");
        R mat = mk(RNode::T::Func, "matrix", rows);
        if (allRows) mat->text = "2d";  // written as [[...], ...]; lets an outer bracket recognise a 3D array
        return mat;
    }

    Result<R> parseGroup(const char* close) {
        ++p_;
        ++parenDepth_;
        auto e = parseRelation();
        if (!e) return e;
        if (!expect(close)) return fail(std::string("Expected '") + close + "'");
        --parenDepth_;
        (*e)->paren = true;
        return e;
    }

    // Argument of a function written without parentheses: sin 2x, ln x
    Result<R> parseBareArg() {
        auto a = parsePower();
        if (!a) return a;
        R acc = *a;
        while (startsPrimary() && !(peek().type == Tok::Ident && isFunctionName(peek().s)) && !isOp("(") && !isOp("\x03")) {
            auto b = parsePower();
            if (!b) return b;
            R m = rbin("*", acc, *b);
            m->implicit = true;
            acc = m;
        }
        return acc;
    }

    // log base / integral bounds: _2, _{2}, _(2)
    Result<R> parseSubSup() {
        if (isOp("{")) return parseGroup("}");
        if (isOp("(")) return parseGroup(")");
        if (isOp("-")) { ++p_; auto a = parsePostfix(); if (!a) return a; return mk(RNode::T::Neg, "-", {*a}); }
        return parsePostfix();
    }

    Result<R> parseIntegral() {
        // after the ∫ marker or "int" without parens
        R lo, hi;
        if (isOp("_")) { ++p_; auto a = parseSubSup(); if (!a) return a; lo = *a; }
        if (isOp("^")) { ++p_; auto a = parseSubSup(); if (!a) return a; hi = *a; }
        ++intDepth_;
        auto body = parseExpr();
        --intDepth_;
        if (!body) return body;
        std::string var = "x";
        if (isIdent("d") && peek(1).type == Tok::Ident) { var = peek(1).s; p_ += 2; }
        R v = mk(RNode::T::Sym, var);
        std::vector<R> kids{*body, v};
        if (lo && hi) { kids.push_back(lo); kids.push_back(hi); }
        return mk(RNode::T::Func, "int", kids);
    }

    Result<R> parsePrimary() {
        const Token tk = peek();
        if (tk.type == Tok::End) return fail("Unexpected end of input");
        if (tk.type == Tok::Num) {
            ++p_;
            auto n = std::make_shared<RNode>();
            n->t = RNode::T::Num;
            n->text = tk.s;
            n->value = Rational::fromDecimal(tk.s);
            return R(n);
        }
        if (tk.type == Tok::Op) {
            if (tk.s == "(") return parseGroup(")");
            if (tk.s == "[") return parseBracket();
            if (tk.s == "{") return parseGroup("}");
            if (tk.s == "|") {
                ++p_;
                ++absDepth_;
                auto e = parseExpr();
                --absDepth_;
                if (!e) return e;
                if (!expect("|")) return fail("Expected closing '|'");
                return mk(RNode::T::Abs, "abs", {*e});
            }
            if (tk.s == "\x01") {  // √
                ++p_;
                auto a = parsePostfix();
                if (!a) return a;
                return mk(RNode::T::Func, "sqrt", {*a});
            }
            if (tk.s == "\x03") { ++p_; return parseIntegral(); }
            return fail("Unexpected '" + tk.s + "'");
        }
        // identifiers
        std::string name = tk.s;
        // dy/dx  and  d^2y/dx^2  (derivative of an unknown function, for differential equations)
        if (name == "d" && peek(1).type == Tok::Ident && peek(1).s == "y" && isOp("/", 2) && isIdent("d", 3) && peek(4).type == Tok::Ident) {
            p_ += 5;
            return mk(RNode::T::Func, "prime", {mk(RNode::T::Sym, "y"), rnum(1)});
        }
        if (name == "d" && isOp("^", 1) && peek(2).type == Tok::Num && isIdent("y", 3) && isOp("/", 4) && isIdent("d", 5)) {
            int order = std::atoi(peek(2).s.c_str());
            p_ += 7;  // d ^ n y / d x
            if (isOp("^") && peek(1).type == Tok::Num) p_ += 2;
            return mk(RNode::T::Func, "prime", {mk(RNode::T::Sym, "y"), rnum(order)});
        }
        // d/dx  and  d^2/dx^2
        if (name == "d" && (isOp("/", 1) || isOp("^", 1))) {
            size_t save = p_;
            int order = 1;
            ++p_;
            if (isOp("^") && peek(1).type == Tok::Num) { order = std::atoi(peek(1).s.c_str()); p_ += 2; }
            if (isOp("/") && isIdent("d", 1) && peek(2).type == Tok::Ident) {
                std::string var = peek(2).s;
                p_ += 3;
                if (isOp("^") && peek(1).type == Tok::Num) p_ += 2;
                Result<R> body = isOp("(") ? parseGroup(")") : parseExpr();
                if (!body) return body;
                return mk(RNode::T::Func, "diff", {*body, mk(RNode::T::Sym, var), rnum(order)});
            }
            p_ = save;
        }
        ++p_;
        if (isFunctionName(name)) {
            std::string fn = canonicalFuncName(name);
            // integral without parentheses: int x^2 dx
            if (fn == "int" && !isOp("(")) return parseIntegral();
            // lim x->a f  |  lim_(x->a) f
            if (fn == "lim" && !isOp("(")) {
                bool wrapped = false;
                if (isOp("_")) { ++p_; if (isOp("(") || isOp("{")) { ++p_; wrapped = true; } }
                if (peek().type != Tok::Ident) return fail("Expected limit variable");
                std::string var = peek().s;
                ++p_;
                if (!expect("->")) return fail("Expected '->'");
                auto pt = parseExpr();
                if (!pt) return pt;
                if (wrapped && !(expect(")") || expect("}"))) return fail("Expected ')'");
                auto body = parseExpr();
                if (!body) return body;
                return mk(RNode::T::Func, "lim", {*body, mk(RNode::T::Sym, var), *pt});
            }
            // log base: log_2(x)
            R base;
            if (fn == "log" && isOp("_")) {
                ++p_;
                auto b = parseSubSup();
                if (!b) return b;
                base = *b;
                base->paren = false;
            }
            // power on function name: sin^2 x
            R power;
            if (isOp("^") && fn != "sqrt") {
                ++p_;
                auto pw = parseSubSup();
                if (!pw) return pw;
                power = *pw;
            }
            std::vector<R> args;
            if (isOp("(")) {
                auto a = parseArgs();
                if (!a) return std::unexpected(a.error());
                args = *a;
            } else {
                auto a = parseBareArg();
                if (!a) return a;
                args.push_back(*a);
            }
            if (args.empty()) return fail("Missing argument for " + name);
            if (base) args.insert(args.begin(), base);
            else if (fn == "log" && args.size() == 1) { /* base 10 */ }
            // sin^{-1} x means arcsin x (not 1/sin x), as everywhere in handwriting and textbooks
            auto isMinusOne = [](const R& pw) {
                R q = pw;
                if (q->t == RNode::T::Neg && q->kids.size() == 1) q = q->kids[0];
                else return q->t == RNode::T::Num && q->value == Rational(-1);
                return q->t == RNode::T::Num && q->value == Rational(1);
            };
            if (power && isMinusOne(power) && (fn == "sin" || fn == "cos" || fn == "tan")) return mk(RNode::T::Func, "a" + fn, args);
            R f = mk(RNode::T::Func, fn, args);
            if (power) return rbin("^", f, power);
            return f;
        }
        return mk(RNode::T::Sym, canonicalSymName(name));
    }
};

// ------------------------------------------------------------ raw printer

std::string symTex(const std::string& n) {
    static const char* greek[] = {"alpha", "beta", "gamma", "delta", "theta", "lambda", "mu", "phi", "omega", "sigma", "tau", "rho"};
    if (n == "pi") return "\\pi";
    if (n == "inf") return "\\infty";
    for (auto g : greek) if (n == g) return std::string("\\") + g;
    return n;
}

std::string rl(const R& r, bool stripParen = false);

std::string relTex(const std::string& op) {
    if (op == "<=") return "\\le";
    if (op == ">=") return "\\ge";
    if (op == "!=") return "\\ne";
    return op;
}

std::string fnTex(const std::string& n) {
    if (n == "asin") return "\\arcsin";
    if (n == "acos") return "\\arccos";
    if (n == "atan") return "\\arctan";
    static const char* std_[] = {"sin", "cos", "tan", "sec", "csc", "cot", "sinh", "cosh", "tanh", "ln", "log", "gcd", "exp", "arg"};
    for (auto s : std_) if (n == s) return std::string("\\") + n;
    return "\\operatorname{" + n + "}";
}

std::string args(const R& r, size_t from = 0) {
    std::string s;
    for (size_t i = from; i < r->kids.size(); ++i) s += (i > from ? ", " : "") + rl(r->kids[i], true);
    return s;
}

std::string rl(const R& r, bool stripParen) {
    std::string s;
    using T = RNode::T;
    switch (r->t) {
        case T::Num: s = r->text; break;
        case T::Sym: s = symTex(r->op); break;
        case T::Bin: {
            const std::string& op = r->op;
            if (op == "+") s = rl(r->kids[0]) + " + " + rl(r->kids[1]);
            else if (op == "-") s = rl(r->kids[0]) + " - " + rl(r->kids[1]);
            else if (op == "*") {
                std::string a = rl(r->kids[0]), b = rl(r->kids[1]);
                bool bDigit = !b.empty() && (std::isdigit((unsigned char)b[0]) || b[0] == '.');
                if (r->implicit && !bDigit) s = a + " " + b;
                else s = a + " \\cdot " + b;
            } else if (op == "/") s = "\\frac{" + rl(r->kids[0], true) + "}{" + rl(r->kids[1], true) + "}";
            else if (op == "div") s = rl(r->kids[0]) + " \\div " + rl(r->kids[1]);
            else if (op == "^") {
                const R& b = r->kids[0];
                std::string bs = rl(b);
                if (b->t == T::Func && !b->paren && b->kids.size() == 1 &&
                    (b->op == "sin" || b->op == "cos" || b->op == "tan" || b->op == "sec" || b->op == "csc" || b->op == "cot")) {
                    s = fnTex(b->op) + "^{" + rl(r->kids[1], true) + "}\\left(" + rl(b->kids[0], true) + "\\right)";
                } else {
                    if (!b->paren && (b->t == T::Neg || b->t == T::Bin)) bs = "\\left(" + bs + "\\right)";
                    s = bs + "^{" + rl(r->kids[1], true) + "}";
                }
            }
            break;
        }
        case T::Neg: s = "-" + rl(r->kids[0]); break;
        case T::Fact: s = rl(r->kids[0]) + "!"; break;
        case T::Pct: s = rl(r->kids[0]) + "\\%"; break;
        case T::Abs: s = "\\left|" + rl(r->kids[0], true) + "\\right|"; break;
        case T::Rel: s = rl(r->kids[0], true) + " " + relTex(r->op) + " " + rl(r->kids[1], true); break;
        case T::List: {
            if (r->op == "chain" && r->kids.size() == 2) {
                s = rl(r->kids[0]) + " " + relTex(r->kids[1]->op) + " " + rl(r->kids[1]->kids[1], true);
                break;
            }
            s = "\\begin{cases} ";
            for (size_t i = 0; i < r->kids.size(); ++i) s += (i ? " \\\\ " : "") + rl(r->kids[i], true);
            s += " \\end{cases}";
            break;
        }
        case T::Func: {
            const std::string& n = r->op;
            auto k = [&](size_t i) { return rl(r->kids[i], true); };
            if (n == "prime" && r->kids.size() == 2) {
                int order = (int)r->kids[1]->value.num().toInt64();
                s = rl(r->kids[0]) + std::string(order, '\'');
            } else if (n == "matrix") {
                s = "\\begin{bmatrix} ";
                for (size_t i = 0; i < r->kids.size(); ++i) {
                    if (i) s += " \\\\ ";
                    for (size_t j = 0; j < r->kids[i]->kids.size(); ++j) s += (j ? " & " : "") + rl(r->kids[i]->kids[j], true);
                }
                s += " \\end{bmatrix}";
            } else if (n == "tensor") {
                s = "\\left[\\,";
                for (size_t i = 0; i < r->kids.size(); ++i) s += (i ? ",\\ " : "") + rl(r->kids[i], true);
                s += "\\,\\right]";
            } else if (n == "det" && r->kids.size() == 1 && r->kids[0]->op == "matrix") {
                std::string inner = rl(r->kids[0], true);
                s = "\\det" + inner;
            } else if (n == "transpose") s = rl(r->kids[0]) + "^{T}";
            else if (n == "inv") s = (r->kids[0]->t == T::Func && r->kids[0]->op == "matrix" ? rl(r->kids[0]) : "\\left(" + k(0) + "\\right)") + "^{-1}";
            else if (n == "sqrt") s = "\\sqrt{" + k(0) + "}";
            else if (n == "cbrt") s = "\\sqrt[3]{" + k(0) + "}";
            else if (n == "root" && r->kids.size() == 2) s = "\\sqrt[" + k(0) + "]{" + k(1) + "}";
            else if (n == "abs") s = "\\left|" + k(0) + "\\right|";
            else if (n == "fact") s = rl(r->kids[0]) + "!";
            else if (n == "conj") s = "\\overline{" + k(0) + "}";
            else if (n == "binom" && r->kids.size() == 2) s = "\\binom{" + k(0) + "}{" + k(1) + "}";
            else if (n == "perm" && r->kids.size() == 2) s = "P\\left(" + k(0) + ", " + k(1) + "\\right)";
            else if (n == "log" && r->kids.size() == 2) s = "\\log_{" + k(0) + "}\\left(" + k(1) + "\\right)";
            else if (n == "exp") s = "e^{" + k(0) + "}";
            else if (n == "mod" && r->kids.size() == 2) s = rl(r->kids[0]) + " \\bmod " + rl(r->kids[1]);
            else if (n == "diff") {
                std::string var = r->kids.size() > 1 ? k(1) : "x";
                std::string ord = r->kids.size() > 2 ? k(2) : "1";
                std::string body = "\\left(" + k(0) + "\\right)";
                if (ord == "1") s = "\\frac{d}{d" + var + "}" + body;
                else s = "\\frac{d^{" + ord + "}}{d" + var + "^{" + ord + "}}" + body;
            } else if (n == "int") {
                std::string var = r->kids.size() > 1 ? k(1) : "x";
                if (r->kids.size() >= 4) s = "\\int_{" + k(2) + "}^{" + k(3) + "} " + k(0) + " \\, d" + var;
                else s = "\\int " + k(0) + " \\, d" + var;
            } else if (n == "lim" && r->kids.size() >= 3) {
                s = "\\lim_{" + k(1) + " \\to " + k(2) + "} " + (r->kids[0]->t == T::Bin && (r->kids[0]->op == "+" || r->kids[0]->op == "-")
                                                                        ? "\\left(" + k(0) + "\\right)"
                                                                        : rl(r->kids[0]));
            } else if (n == "solve" || n == "factor" || n == "expand" || n == "simplify" || n == "roots" || n == "polar" ||
                       n == "isprime" || n == "primefactors") {
                s = "\\text{" + n + "}\\left(" + args(r) + "\\right)";
            } else s = fnTex(n) + "\\left(" + args(r) + "\\right)";
            break;
        }
    }
    if (r->paren && !stripParen) return "\\left(" + s + "\\right)";
    return s;
}

// ------------------------------------------------------------ raw -> expr

Result<E> conv(const R& r);

Result<std::vector<E>> convKids(const R& r) {
    std::vector<E> out;
    for (auto& k : r->kids) {
        auto e = conv(k);
        if (!e) return std::unexpected(e.error());
        out.push_back(*e);
    }
    return out;
}

std::string varName(const R& r, const std::string& def = "x") {
    if (r->t == RNode::T::Sym) return r->op;
    return def;
}

Result<E> conv(const R& r) {
    using T = RNode::T;
    switch (r->t) {
        case T::Num: return num(r->value);
        case T::Sym: {
            if (r->op == "inf") return INF();
            return sym(r->op);
        }
        case T::Neg: {
            auto a = conv(r->kids[0]);
            if (!a) return a;
            return neg(*a);
        }
        case T::Fact: { auto a = conv(r->kids[0]); if (!a) return a; return func("fact", *a); }
        case T::Pct: { auto a = conv(r->kids[0]); if (!a) return a; return mul(*a, num(Rational(1, 100))); }
        case T::Abs: { auto a = conv(r->kids[0]); if (!a) return a; return func("abs", *a); }
        case T::Bin: {
            auto a = conv(r->kids[0]);
            if (!a) return a;
            auto b = conv(r->kids[1]);
            if (!b) return b;
            const std::string& op = r->op;
            if (op == "+") return add(*a, *b);
            if (op == "-") return sub(*a, *b);
            if (op == "*") return mul(*a, *b);
            if (op == "/" || op == "div") return divide(*a, *b);
            if (op == "^") return pow(*a, *b);
            return std::unexpected(Error{"Unknown operator " + op});
        }
        case T::Rel:
        case T::List: return std::unexpected(Error{"Equation used where an expression was expected"});
        case T::Func: {
            const std::string& n = r->op;
            if (n == "diff") {
                auto f = conv(r->kids[0]);
                if (!f) return f;
                std::string v = r->kids.size() > 1 ? varName(r->kids[1]) : "x";
                int order = (r->kids.size() > 2 && r->kids[2]->t == T::Num) ? (int)r->kids[2]->value.num().toInt64() : 1;
                return derivativeOf(*f, v, order);
            }
            if (n == "int") {
                auto f = conv(r->kids[0]);
                if (!f) return f;
                std::string v = r->kids.size() > 1 ? varName(r->kids[1]) : "x";
                auto F = antiderivativeOf(*f, v);
                if (!F) return std::unexpected(Error{"Could not integrate"});
                if (r->kids.size() >= 4) {
                    auto a = conv(r->kids[2]);
                    auto b = conv(r->kids[3]);
                    if (!a || !b) return std::unexpected(Error{"Bad bounds"});
                    return simplify(sub(substitute(*F, v, *b), substitute(*F, v, *a)));
                }
                return *F;
            }
            if (n == "prime") {
                int order = (r->kids.size() > 1 && r->kids[1]->t == T::Num) ? (int)r->kids[1]->value.num().toInt64() : 1;
                std::string base = r->kids[0]->t == T::Sym ? r->kids[0]->op : "y";
                return sym(base + std::string(order, '\''));
            }
            if (n == "taylor" || n == "mean" || n == "median" || n == "mode" || n == "stdev" || n == "variance" || n == "stats" || n == "dsolve" ||
                n == "fv" || n == "pv" || n == "pmt" || n == "npv" || n == "irr" || n == "cagr" || n == "bond" || n == "sharpe" ||
                n == "normcdf" || n == "bscall" || n == "bsput" || n == "cont" || n == "graph3d" || n == "surface" || n == "bsmcall" || n == "bsmput" || n == "merton" || n == "credit" || n == "binomial" || n == "ivol")
                return std::unexpected(Error{"'" + n + "' can only be used at the top level"});
            if (n == "lim" || n == "solve" || n == "roots" || n == "isprime" || n == "primefactors" || n == "polar")
                return std::unexpected(Error{"'" + n + "' can only be used at the top level"});
            if (n == "factor" || n == "expand" || n == "simplify") {
                auto a = conv(r->kids[0]);
                if (!a) return a;
                if (n == "expand") return expand(*a);
                return simplify(*a);
            }
            auto ks = convKids(r);
            if (!ks) return std::unexpected(ks.error());
            auto& a = *ks;
            if (n == "sqrt") return sqrtE(a[0]);
            if (n == "cbrt") return pow(a[0], num(Rational(1, 3)));
            if (n == "root") {
                if (a.size() != 2) return std::unexpected(Error{"root(n, x) needs two arguments"});
                return pow(a[1], pow(a[0], num(-1)));
            }
            if (n == "exp") return pow(EUL(), a[0]);
            if (n == "log") {
                if (a.size() == 1) return func("log", {num(10), a[0]});
                return func("log", {a[0], a[1]});
            }
            if (n == "mod") return func("mod", a);
            return func(n, a);
        }
    }
    return std::unexpected(Error{"Unsupported input"});
}

}  // namespace

Result<R> parse(std::string_view input) {
    // LaTeX input (typed or from the recognition model) is converted first
    std::string converted;
    if (looksLikeLatex(input)) { converted = latexToEngine(input); input = converted; }
    std::string norm = normalizeUnicode(input);
    auto toks = lex(norm);
    if (!toks) return std::unexpected(toks.error());
    if (toks->size() <= 1) return std::unexpected(Error{"Empty input", 0});
    Parser p(std::move(*toks));
    return p.parseAll();
}

std::string rawLatex(const R& r) { return rl(r, true); }

bool isEngineFunctionName(const std::string& s) { return isFunctionName(s); }

Result<E> toExpr(const R& r) { return conv(r); }

}  // namespace me
