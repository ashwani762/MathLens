// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// LaTeX -> engine syntax. Handles the LaTeX that humans type and that the on-device
// recognition model emits (letter-spaced \operatorname{s i n}, \underset limits, arrays...).
#include <cstring>
#include <regex>
#include <cctype>
#include <algorithm>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "parse/latex_import.h"
#include "parse/parser.h"

namespace me {

namespace {

// "(x+y=5)" -> "x+y=5" when one balanced pair wraps the whole text
std::string unwrap(std::string t) {
    while (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
        int depth = 0; bool wraps = true;
        for (size_t i = 0; i < t.size(); ++i) {
            depth += t[i] == '(' ? 1 : t[i] == ')' ? -1 : 0;
            if (depth == 0 && i + 1 < t.size()) { wraps = false; break; }
        }
        if (!wraps) break;
        t = t.substr(1, t.size() - 2);
    }
    return t;
}

bool isKnownFunctionWord(const std::string& w) {
    static const char* ks[] = {"sin", "cos", "tan", "sec", "csc", "cot", "sinh", "cosh", "tanh", "arcsin", "arccos", "arctan", "log", "ln", "exp",
                               "sqrt", "det", "lim", "limit", "max", "min", "gcd", "lcm", "mod", "sgn", "arg", "tr", "trace", "rank", "diag"};
    for (auto k : ks) if (w == k) return true;
    return false;
}

struct Tok {
    enum class T { Cmd, Char, Open, Close, End } t;
    std::string s;
};

std::vector<Tok> tokenize(std::string_view in) {
    std::vector<Tok> out;
    for (size_t i = 0; i < in.size();) {
        char c = in[i];
        if (std::isspace((unsigned char)c) || c == '~') { ++i; continue; }
        if (c == '\\') {
            size_t j = i + 1;
            if (j < in.size() && std::isalpha((unsigned char)in[j])) {
                while (j < in.size() && std::isalpha((unsigned char)in[j])) ++j;
                if (j < in.size() && in[j] == '*') ++j;  // \operatorname*
            } else if (j < in.size()) ++j;
            out.push_back({Tok::T::Cmd, std::string(in.substr(i + 1, j - i - 1))});
            i = j;
            continue;
        }
        if (c == '{') { out.push_back({Tok::T::Open, "{"}); ++i; continue; }
        if (c == '}') { out.push_back({Tok::T::Close, "}"}); ++i; continue; }
        out.push_back({Tok::T::Char, std::string(1, c)});
        ++i;
    }
    out.push_back({Tok::T::End, ""});
    return out;
}

bool isSpacing(const std::string& c) {
    static const char* sp[] = {",", ";", ":", "!", " ", "quad", "qquad", "displaystyle", "textstyle", "scriptstyle", "limits", "nolimits", "left.", "right.", "big", "Big", "bigl", "bigr", "Bigl", "Bigr"};
    for (auto s : sp) if (c == s) return true;
    return false;
}

std::string mapCommand(const std::string& c) {
    static const std::pair<const char*, const char*> m[] = {
        {"cdot", "*"}, {"times", "*"}, {"ast", "*"}, {"star", "+"},  // a handwritten + is often read as \\star
         {"div", "\xC3\xB7"}, {"pm", "+-"}, {"mp", "-+"},
        {"le", "<="}, {"leq", "<="}, {"leqslant", "<="}, {"ge", ">="}, {"geq", ">="}, {"geqslant", ">="}, {"ne", "!="}, {"neq", "!="},
        {"lt", "<"}, {"gt", ">"}, {"to", "->"}, {"rightarrow", "->"}, {"longrightarrow", "->"}, {"infty", "inf"}, {"pi", "pi"}, {"theta", "theta"},
        {"alpha", "alpha"}, {"beta", "beta"}, {"gamma", "gamma"}, {"lambda", "lambda"}, {"mu", "mu"}, {"sigma", "sigma"}, {"phi", "phi"},
        {"varphi", "phi"}, {"omega", "omega"}, {"delta", "delta"}, {"tau", "tau"}, {"rho", "rho"}, {"epsilon", "epsilon"}, {"varepsilon", "epsilon"},
        {"sin", "sin"}, {"cos", "cos"}, {"tan", "tan"}, {"sec", "sec"}, {"csc", "csc"}, {"cot", "cot"}, {"arcsin", "asin"}, {"arccos", "acos"},
        {"arctan", "atan"}, {"sinh", "sinh"}, {"cosh", "cosh"}, {"tanh", "tanh"}, {"ln", "ln"}, {"log", "log"}, {"exp", "exp"}, {"det", "det"},
        {"gcd", "gcd"}, {"%", "%"}, {"{", "("}, {"}", ")"}, {"|", "|"}, {"vert", "|"}, {"lvert", "|"}, {"rvert", "|"}, {"cdots", ""}, {"ldots", ""},
        {"prime", "'"}, {"circ", ""}, {"degree", ""},
    };
    for (auto& [a, b] : m) if (c == a) return b;
    return "";
}

class Converter {
public:
    explicit Converter(std::vector<Tok> t) : t_(std::move(t)) {}
    std::string run() { return seq(false); }

private:
    std::vector<Tok> t_;
    size_t p_ = 0;

    const Tok& peek(size_t k = 0) const { return t_[std::min(p_ + k, t_.size() - 1)]; }
    bool isCmd(const char* c, size_t k = 0) const { return peek(k).t == Tok::T::Cmd && peek(k).s == c; }

    // A braced group or a single token, converted.
    std::string arg() {
        skipSpacing();
        if (peek().t == Tok::T::Open) {
            ++p_;
            std::string s = seq(true);
            return s;
        }
        return atom();
    }

    // Raw text of a braced group with spaces removed: \operatorname{s i n} -> "sin"
    std::string rawGroup() {
        skipSpacing();
        std::string s;
        if (peek().t != Tok::T::Open) { s = peek().s; ++p_; return s; }
        ++p_;
        int depth = 1;
        while (peek().t != Tok::T::End) {
            if (peek().t == Tok::T::Open) ++depth;
            if (peek().t == Tok::T::Close && --depth == 0) { ++p_; break; }
            if (peek().t == Tok::T::Char) s += peek().s;
            else if (peek().t == Tok::T::Cmd) {
                // nested \mathrm etc. inside operatorname: keep letters only
                std::string c = peek().s;
                if (c.size() == 1 && std::isalpha((unsigned char)c[0])) s += c;
            }
            ++p_;
        }
        return s;
    }

    void skipSpacing() {
        while (peek().t == Tok::T::Cmd && isSpacing(peek().s)) ++p_;
    }

    // Converts until a closing brace (inGroup) or the end.
    std::string seq(bool inGroup) {
        std::string out;
        while (true) {
            skipSpacing();
            const Tok& k = peek();
            if (k.t == Tok::T::End) break;
            if (k.t == Tok::T::Close) { ++p_; if (inGroup) break; continue; }
            if (k.t == Tok::T::Cmd && (k.s == "right" || k.s == "end")) break;
            out += atom();
        }
        return out;
    }

    // An integral body runs until "d<var>" at this level.
    std::string integralBody(std::string& var) {
        std::string out;
        while (true) {
            skipSpacing();
            const Tok& k = peek();
            if (k.t == Tok::T::End || k.t == Tok::T::Close) break;
            if (k.t == Tok::T::Char && k.s == "d" && peek(1).t == Tok::T::Char && std::isalpha((unsigned char)peek(1).s[0])) {
                var = peek(1).s;
                p_ += 2;
                break;
            }
            if (k.t == Tok::T::Cmd && k.s == "mathrm" && peek(1).t == Tok::T::Open && peek(2).s == "d") {
                // \mathrm{d} x
                p_ += 3;
                if (peek().t == Tok::T::Close) ++p_;
                skipSpacing();
                if (peek().t == Tok::T::Char) { var = peek().s; ++p_; }
                break;
            }
            out += atom();
        }
        return out;
    }

    std::string environment(const std::string& env) {
        if (env == "array") rawGroup();  // column spec
        std::vector<std::vector<std::string>> rows(1);
        std::string cell;
        while (peek().t != Tok::T::End) {
            skipSpacing();
            if (isCmd("end")) { ++p_; rawGroup(); break; }
            if (peek().t == Tok::T::Char && peek().s == "&") { rows.back().push_back(cell); cell.clear(); ++p_; continue; }
            if (isCmd("\\")) { rows.back().push_back(cell); cell.clear(); rows.emplace_back(); ++p_; continue; }
            if (peek().t == Tok::T::Close) { ++p_; continue; }
            cell += atom();
        }
        rows.back().push_back(cell);
        // empty sub/superscripts left behind by dropped prose ("_{\\textsc{...}}")
        for (auto& r : rows) for (auto& c : r) {
            for (const char* e : {"_()", "^()", "()"}) for (size_t k; (k = c.find(e)) != std::string::npos;) c.erase(k, std::strlen(e));
        }
        // drop empty rows
        std::vector<std::vector<std::string>> r2;
        for (auto& r : rows) {
            bool any = false;
            for (auto& c : r) if (!c.empty()) any = true;
            if (any) r2.push_back(r);
        }
        // rows that are relations form a system, whatever the environment ({ matrix } is common from OCR)
        bool allRelations = !r2.empty() && std::ranges::all_of(r2, [](const std::vector<std::string>& r) {
            std::string line;
            for (auto& c : r) line += c;
            return line.find_first_of("=<>") != std::string::npos;
        });
        if (!allRelations && (env == "bmatrix" || env == "pmatrix" || env == "matrix" || env == "vmatrix")) {
            std::string s = "[";
            for (size_t i = 0; i < r2.size(); ++i) {
                s += (i ? ",[" : "[");
                for (size_t j = 0; j < r2[i].size(); ++j) s += (j ? "," : "") + r2[i][j];
                s += "]";
            }
            s += "]";
            return env == "vmatrix" ? "det(" + s + ")" : s;
        }
        // cases / array / aligned / gathered: each row is an equation of a system
        std::string s;
        for (auto& r : r2) {
            std::string line;
            for (auto& c : r) line += c;
            line = unwrap(line);
            if (line.empty()) continue;
            s += (s.empty() ? "" : "; ") + line;
        }
        return s;
    }

    std::string atom() {
        skipSpacing();
        Tok k = peek();
        if (k.t == Tok::T::End || k.t == Tok::T::Close) return "";
        if (k.t == Tok::T::Open) { ++p_; return "(" + seq(true) + ")"; }
        if (k.t == Tok::T::Char) {
            ++p_;
            if (k.s == "^") return "^(" + arg() + ")";
            if (k.s == "_") { arg(); return ""; }  // stray subscripts (indices) are dropped
            if (k.s == "&") return "";
            return k.s;
        }
        // commands
        ++p_;
        const std::string& c = k.s;
        if (c == "frac" || c == "dfrac" || c == "tfrac") {
            std::string a = unwrap(arg()), b = unwrap(arg());  // OCR often doubles braces: \frac{d}{{dx}}
            // Leibniz derivative operator: \frac{d}{dx} f
            if (a == "d" && b.size() == 2 && b[0] == 'd') {
                std::string body = atomsUntilOperator();
                return "d/d" + b.substr(1) + "(" + body + ")";
            }
            if (a == "dy" && b.size() == 2 && b[0] == 'd') return "dy/dx";
            return "((" + a + ")/(" + b + "))";
        }
        if (c == "sqrt") {
            skipSpacing();
            std::string idx;
            if (peek().t == Tok::T::Char && peek().s == "[") {
                ++p_;
                while (peek().t != Tok::T::End && !(peek().t == Tok::T::Char && peek().s == "]")) idx += atom();
                ++p_;
            }
            std::string body = arg();
            return idx.empty() ? "sqrt(" + body + ")" : "root(" + idx + "," + body + ")";
        }
        if (c == "left") {
            // \left X ... \right Y as one matched group
            auto delim = [&](bool open) -> std::string {
                skipSpacing();
                Tok d = peek();
                ++p_;
                if (d.t == Tok::T::Cmd) {
                    if (d.s == "{" || d.s == "}") return open ? "(" : ")";
                    if (d.s == "|" || d.s == "vert" || d.s == "lvert" || d.s == "rvert") return "|";
                    if (d.s == "lfloor") return "floor(";
                    if (d.s == "lceil") return "ceil(";
                    if (d.s == "rfloor" || d.s == "rceil") return ")";
                    return open ? "(" : ")";
                }
                if (d.s == ".") return "";
                if (d.s == "[" ) return "(";
                if (d.s == "]") return ")";
                return d.s;
            };
            std::string open = delim(true);
            std::string inner;
            while (peek().t != Tok::T::End && !isCmd("right")) {
                if (peek().t == Tok::T::Close) { ++p_; continue; }
                inner += atom();
            }
            std::string close;
            if (isCmd("right")) { ++p_; close = delim(false); }
            if (open == "|" ) return "abs(" + inner + ")";
            if (open.empty()) return inner + close;
            if (close.empty()) return open + inner + ")";
            return open + inner + close;
        }
        if (c == "right") { skipSpacing(); ++p_; return ""; }
        if (c == "hspace" || c == "vspace" || c == "hspace*" || c == "kern" || c == "mkern" || c == "hskip" || c == "mskip" || c == "phantom" || c == "hphantom" || c == "vphantom") {
            rawGroup();
            return "";
        }
        if (c == "operatorname" || c == "operatorname*" || c == "mathrm" || c == "text" || c == "textrm" || c == "mathit" || c == "mathbf" ||
            c == "textsc" || c == "textbf" || c == "textit" || c == "textsf" || c == "texttt" || c == "mathsf" || c == "mathtt") {
            std::string name = rawGroup();
            if (name == "lim" || name == "limit") return limit();
            if (name == "d") return "d";
            // prose ("Solve the system of equations") is not math: drop words that aren't function names
            bool word = name.size() > 3 && std::ranges::all_of(name, [](char ch) { return std::isalpha((unsigned char)ch) || ch == '\\' || ch == '{' || ch == '}'; });
            if (word) {
                std::string low;
                for (char ch : name) low += (char)std::tolower((unsigned char)ch);
                if (isKnownFunctionWord(low)) return low;
                if (isEngineFunctionName(low)) return low;  // engine commands: taylor, factor, simplify, ...
                return "";
            }
            if (name.find('\\') != std::string::npos) return "";  // nested formatting inside text
            return name;
        }
        if (c == "underset" || c == "overset") {
            std::string under = arg();
            std::string base = rawGroup();
            if (base == "lim") return limitWith(under);
            return base;
        }
        if (c == "lim") return limit();
        if (c == "int" || c == "iint") {
            std::string lo, hi;
            skipSpacing();
            for (int pass = 0; pass < 2; ++pass) {
                if (peek().t == Tok::T::Char && peek().s == "_") { ++p_; lo = arg(); }
                else if (peek().t == Tok::T::Char && peek().s == "^") { ++p_; hi = arg(); }
            }
            std::string var = "x";
            std::string body = integralBody(var);
            if (!lo.empty() && !hi.empty()) return "int(" + body + ", " + var + ", " + lo + ", " + hi + ")";
            return "int(" + body + ", " + var + ")";
        }
        if (c == "log") {
            skipSpacing();
            if (peek().t == Tok::T::Char && peek().s == "_") { ++p_; return "log_(" + arg() + ")"; }
            return "log";
        }
        if (c == "begin") return environment(rawGroup());
        if (c == "binom") { std::string a = arg(), b = arg(); return "nCr(" + a + "," + b + ")"; }
        if (c == "overline" || c == "bar") return "conj(" + arg() + ")";
        if (c == "mathbb" || c == "mathcal") return rawGroup();
        if (c == "hat" || c == "tilde" || c == "vec" || c == "dot") return arg();
        std::string m = mapCommand(c);
        if (!m.empty() || c == "cdots" || c == "ldots") return m;
        return "";  // unknown command: ignore rather than inventing symbols
    }

    // f in "\frac{d}{dx} f": a parenthesised group or the following factors up to + - =
    std::string atomsUntilOperator() {
        skipSpacing();
        std::string s;
        while (true) {
            skipSpacing();
            const Tok& k = peek();
            if (k.t == Tok::T::End || k.t == Tok::T::Close) break;
            if (k.t == Tok::T::Char && (k.s == "+" || k.s == "-" || k.s == "=" || k.s == ",")) break;
            if (k.t == Tok::T::Cmd && (k.s == "right" || k.s == "end")) break;
            s += atom();
        }
        return s;
    }

    std::string limitWith(const std::string& under) {
        // under: "x->a"
        std::string var = "x", point = "0";
        auto pos = under.find("->");
        if (pos != std::string::npos) { var = under.substr(0, pos); point = under.substr(pos + 2); }
        std::string body = atomsUntilOperator();
        return "lim(" + body + ", " + var + ", " + point + ")";
    }

    std::string limit() {
        skipSpacing();
        std::string under;
        if (peek().t == Tok::T::Char && peek().s == "_") { ++p_; under = arg(); }
        return limitWith(under);
    }
};

}  // namespace

bool looksLikeLatex(std::string_view s) {
    return s.find('\\') != std::string_view::npos || s.find("^{") != std::string_view::npos || s.find("_{") != std::string_view::npos;
}

std::string latexToEngine(std::string_view latex) {
    std::string s = Converter(tokenize(latex)).run();
    // tidy: collapse doubled operators produced by spacing removal
    std::string out;
    for (char c : s) {
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        out += c;
    }
    while (!out.empty() && (out.back() == '.' || out.back() == ',' || out.back() == ' ')) out.pop_back();
    // headings ("S O L V E \\cdot T H E ...", "\\mathrm{S \\hspace{1mm} I M P ...}") are prose, not math:
    // drop spacing units, then reject output that is only letters and joiners with no operator or digit
    {
        std::string t = std::regex_replace(out, std::regex("\\d+(mm|pt|em|ex|cm|mu|in)"), "");
        t = std::regex_replace(t, std::regex("arcsin|arccos|arctan|sinh|cosh|tanh|sin|cos|tan|sec|csc|cot|log|ln|exp|sqrt|lim"), "(");  // math words
        size_t letters = 0;
        bool mathy = false;
        for (char ch : t) {
            if (std::isalpha((unsigned char)ch)) ++letters;
            else if (std::isdigit((unsigned char)ch) || std::strchr("=+-/^<>()[],;|!", ch)) mathy = true;
        }
        if (!mathy && letters >= 6) return "";
    }
    // a brace around a system ("\\left\\{ ... \\right.") must not wrap the equations in parentheses
    if (out.find(';') != std::string::npos) out = unwrap(out);
    return out;
}

}  // namespace me
