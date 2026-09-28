// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Regression tests: every input must solve and produce exactly the recorded answer.
#include <cstdio>
#include <string>
#include <string_view>

#include "mathengine/mathengine.h"

namespace {

struct Case {
    const char* input;
    const char* answer;
};

constexpr Case kCases[] = {
    {"2+3*4", "14"},
    {"(1+2)*3-4/2", "7"},
    {"1/2+1/3", "\\frac{5}{6}"},
    {"3/4 \u00f7 2/5", "\\frac{3}{40}"},
    {"2^10", "1024"},
    {"sqrt(72)", "6 \\sqrt{2}"},
    {"5!", "5! = 120"},
    {"20!", "20! = 2432902008176640000"},
    {"18/24", "\\frac{3}{4}"},
    {"2x+3=7", "x = 2"},
    {"3(x-2)=2x+5", "x = 11"},
    {"x/2+1/3=5/6", "x = 1"},
    {"x^2-5x+6=0", "x_{1} = 2, \\quad x_{2} = 3"},
    {"2x^2+3x-2=0", "x_{1} = -2, \\quad x_{2} = \\frac{1}{2}"},
    {"x^2+4=0", "x_{1} = -2 i, \\quad x_{2} = 2 i"},
    {"x^2+x+1=0", "x_{1} = -\\frac{1}{2} - \\frac{\\sqrt{3} i}{2}, \\quad x_{2} = \\frac{\\sqrt{3} i}{2} - \\frac{1}{2}"},
    {"x^2-2=0", "x_{1} = -\\sqrt{2}, \\quad x_{2} = \\sqrt{2}"},
    {"x^3-6x^2+11x-6=0", "x_{1} = 1, \\quad x_{2} = 2, \\quad x_{3} = 3"},
    {"x^4-5x^2+4=0", "x_{1} = -2, \\quad x_{2} = -1, \\quad x_{3} = 1, \\quad x_{4} = 2"},
    {"x^3=8", "x = 2"},
    {"x^5-x-1=0", "x \\approx 1.167304"},
    {"d/dx(x^3+2x^2-5)", "f'(x) = 3 x^{2} + 4 x"},
    {"diff(sin(x^2),x)", "f'(x) = 2 x \\cos\\left(x^{2}\\right)"},
    {"diff(x*e^x, x)", "f'(x) = x e^{x} + e^{x}"},
    {"diff(ln(x)/x, x)", "f'(x) = \\frac{1 - \\ln\\left(x\\right)}{x^{2}}"},
    {"diff(x^x,x)", "f'(x) = x^{x} \\left(\\ln\\left(x\\right) + 1\\right)"},
    {"int(x^2+3x, x)", "\\int x^{2} + 3 x \\, dx = \\frac{x^{3}}{3} + \\frac{3 x^{2}}{2} + C"},
    {"int(x*e^x, x)", "\\int x \\cdot e^{x} \\, dx = x e^{x} - e^{x} + C"},
    {"int(sin(x)^2, x)", "\\int \\sin^{2}\\left(x\\right) \\, dx = \\frac{x}{2} - \\frac{\\sin\\left(2 x\\right)}{4} + C"},
    {"int(1/(x^2-1), x)", "\\int \\frac{1}{x^{2} - 1} \\, dx = \\frac{\\ln\\left(\\left|x - 1\\right|\\right)}{2} - \\frac{\\ln\\left(\\left|x + 1\\right|\\right)}{2} + C"},
    {"int(x*e^(x^2), x)", "\\int x \\cdot e^{x^{2}} \\, dx = \\frac{e^{x^{2}}}{2} + C"},
    {"int(ln(x), x)", "\\int \\ln\\left(x\\right) \\, dx = x \\ln\\left(x\\right) - x + C"},
    {"int(x^2, x, 0, 3)", "\\int_{0}^{3} x^{2} \\, dx = 9"},
    {"\u222b cos x dx", "\\int \\cos\\left(x\\right) \\, dx = \\sin\\left(x\\right) + C"},
    {"int(e^(-x^2), x, 0, 1)", "\\int_{0}^{1} e^{-x^{2}} \\, dx \\approx 0.7468241328"},
    {"lim(sin(x)/x, x, 0)", "\\lim_{x \\to 0} \\frac{\\sin\\left(x\\right)}{x} = 1"},
    {"lim((x^2-1)/(x-1), x, 1)", "\\lim_{x \\to 1} \\frac{x^{2} - 1}{x - 1} = 2"},
    {"lim((3x^2+1)/(2x^2-x), x, inf)", "\\lim_{x \\to \\infty} \\frac{3 x^{2} + 1}{2 x^{2} - x} = \\frac{3}{2}"},
    {"(3+2i)(1-i)", "5 - i"},
    {"(2+3i)/(1-i)", "\\frac{5 i}{2} - \\frac{1}{2}"},
    {"i^23", "-i"},
    {"z^3=1", "z_{1} = 1, \\quad z_{2} = \\frac{\\sqrt{3} i}{2} - \\frac{1}{2}, \\quad z_{3} = -\\frac{1}{2} - \\frac{\\sqrt{3} i}{2}"},
    {"z^2+2z+5=0", "z_{1} = -1 - 2 i, \\quad z_{2} = 2 i - 1"},
    {"z+2conj(z)=3-i", "z = 1 + i"},
    {"x+y=5; x-y=1", "x = 3, \\ y = 2"},
    {"2x+3y=12, x-y=1", "x = 3, \\ y = 2"},
    {"x+y+z=6; 2x-y+z=3; x+2y-z=2", "x = 1, \\ y = 2, \\ z = 3"},
    {"x+y=5; x*y=6", "x = 3, \\ y = 2 \\quad \\text{or} \\quad x = 2, \\ y = 3"},
    {"2^(x+1)=16", "x = 3"},
    {"ln(2x)=3", "x = \\frac{e^{3}}{2}"},
    {"sqrt(x+3)=x-3", "x = 6"},
    {"sin(x)=1/2", "x_{1} = 2 \\pi n + \\frac{\\pi}{6}, \\quad x_{2} = 2 \\pi n + \\frac{5 \\pi}{6}, \\quad n \\in \\mathbb{Z}"},
    {"e^(2x)-3e^x+2=0", "x_{1} = 0, \\quad x_{2} = \\ln\\left(2\\right)"},
    {"2x+1>5", "x > 2"},
    {"x^2-4<0", "x \\in \\left(-2, 2\\right)"},
    {"(x-1)/(x+2)>=0", "x \\in \\left(-\\infty, -2\\right) \\cup \\left[1, \\infty\\right)"},
    {"factor(x^2-5x+6)", "\\left(x - 2\\right) \\left(x - 3\\right)"},
    {"factor(360)", "360 = 2^{3} \\cdot 3^{2} \\cdot 5"},
    {"expand((x+1)^3)", "x^{3} + 3 x^{2} + 3 x + 1"},
    {"simplify((x^2-1)/(x-1))", "x + 1"},
    {"1/x+1/(x+1)", "\\frac{2 x + 1}{x \\left(x + 1\\right)}"},
    {"gcd(48,18)", "\\gcd\\left(48, 18\\right) = 6"},
    {"lcm(4,6,10)", "\\operatorname{lcm}\\left(4, 6, 10\\right) = 60"},
    {"nCr(10,3)", "\\binom{10}{3} = 120"},
    {"y=x^2-3", "\\text{Parabola: vertex } \\left(0, -3\\right)"},
    {"x^2+y^2=25", "\\text{Circle: centre } \\left(0, 0\\right), \\ r = 5"},
    {"|2x-1|=5", "x_{1} = -2, \\quad x_{2} = 3"},
    {"log_2(8)", "3"},
    {"sin(pi/6)+cos(pi/3)", "1"},
    {"(x+1)/(x-2)=3", "x = \\frac{7}{2}"},
    {"x^2/(x-1)=1/(x-1)", "x = -1"},
    {"x/(x-1)=1/(x-1)", "\\text{No solution}"},
    {"nPr(5,2)", "P(5, 2) = 20"},
    {"int(1/x, x, 1, e)", "\\int_{1}^{e} \\frac{1}{x} \\, dx = 1"},
    {"int(e^(-x), x, 0, inf)", "\\int_{0}^{\\infty} e^{-x} \\, dx = 1"},
    {"sqrt(2x+1)=3", "x = 4"},
    {"log_2(x)=5", "x = 32"},
    {"cos(2x)=1/2", "x_{1} = \\pi n + \\frac{\\pi}{6}, \\quad x_{2} = \\pi n - \\frac{\\pi}{6}, \\quad n \\in \\mathbb{Z}"},
    {"int(sqrt(1-x^2),x)", "\\int \\sqrt{1 - x^{2}} \\, dx = \\frac{x \\sqrt{1 - x^{2}}}{2} + \\frac{\\arcsin\\left(x\\right)}{2} + C"},
    {"int(tan(x),x)", "\\int \\tan\\left(x\\right) \\, dx = -\\ln\\left(\\left|\\cos\\left(x\\right)\\right|\\right) + C"},
    {"lim((1+1/x)^x, x, inf)", "\\lim_{x \\to \\infty} \\left(1 + \\frac{1}{x}\\right)^{x} = e"},
    {"|x-3|<2", "x \\in \\left(1, 5\\right)"},
    {"[[1,2],[3,4]]*[[5,6],[7,8]]", "\\begin{bmatrix} 19 & 22 \\\\ 43 & 50 \\end{bmatrix}"},
    {"det([[1,2,3],[4,5,6],[7,8,10]])", "-3"},
    {"inv([[2,1],[5,3]])", "\\begin{bmatrix} 3 & -1 \\\\ -5 & 2 \\end{bmatrix}"},
    {"[[1,2,3],[0,1,4],[5,6,0]]^-1", "\\begin{bmatrix} -24 & 18 & 5 \\\\ 20 & -15 & -4 \\\\ -5 & 4 & 1 \\end{bmatrix}"},
    {"eig([[2,1],[1,2]])", "\\lambda_{1} = 1, \\quad \\lambda_{2} = 3"},
    {"rank([[1,2],[2,4]])", "1"},
    {"det([[a,b],[c,d]])", "a d - b c"},
    {"x^2/9+y^2/4=1", "\\text{Ellipse: centre } \\left(0, 0\\right), \\ a = 3, \\ b = 2"},
    {"x^2-y^2=1", "\\text{Hyperbola: centre } \\left(0, 0\\right), \\ a = 1, \\ b = 1"},
    {"x=y^2", "\\text{Parabola: vertex } \\left(0, 0\\right)"},
    {"2x+3y=6", "\\text{Line: } y = 2 - \\frac{2 x}{3}"},
    {"x^2+y^2-4x+6y-3=0", "\\text{Circle: centre } \\left(2, -3\\right), \\ r = 4"},
    {"y'=2y", "y = C e^{2 x}"},
    {"dy/dx = x*y", "y = C e^{\\frac{x^{2}}{2}}"},
    {"y' + y = x", "y = C e^{-x} + x - 1"},
    {"y'=x^2", "y = \\frac{x^{3}}{3} + C"},
    {"y''+y=0", "y = C_1 \\cos\\left(x\\right) + C_2 \\sin\\left(x\\right)"},
    {"y''-3y'+2y=0", "y = C_1 e^{x} + C_2 e^{2 x}"},
    {"y''+2y'+y=0", "y = e^{-x} \\left(C_2 x + C_1\\right)"},
    {"taylor(sin(x),x,0,7)", "\\sin\\left(x\\right) \\approx x - \\frac{x^{3}}{6} + \\frac{x^{5}}{120} - \\frac{x^{7}}{5040}"},
    {"taylor(e^x, x, 0, 4)", "e^{x} \\approx 1 + x + \\frac{x^{2}}{2} + \\frac{x^{3}}{6} + \\frac{x^{4}}{24}"},
    {"mean(2,4,4,4,5,5,7,9)", "\\bar{x} = 5"},
    {"stats(3,1,4,1,5,9,2,6)", "\\bar{x} = 3.875, \\quad \\tilde{x} = 3.5, \\quad s = 2.74838"},
    {"median(5,1,3)", "\\tilde{x} = 3"},
    {"fv(0.05,10,1000)", "FV = 1628.8946"},
    {"pv(0.05,10,1628.89)", "PV = 999.99716"},
    {"pmt(0.005,360,300000)", "PMT = 1798.6516"},
    {"npv(0.1,-1000,300,400,500)", "NPV = -21.036814"},
    {"irr(-1000,300,400,500)", "IRR = 0.088963395"},
    {"bscall(100,100,1,0.05,0.2)", "C = 10.450584"},
    {"bsput(100,100,1,0.05,0.2)", "P = 5.573526"},
    {"bond(1000,0.05,0.06,10,2)", "P = 925.61263"},
    {"cagr(100,200,5)", "CAGR = 0.14869835"},
    {"normcdf(1.96)", "\\Phi(1.96) = 0.9750021"},
    {"sharpe(0.12,0.03,0.15)", "S = 0.6"},
    {"cont(1000,0.05,10)", "A = 1648.7213"},
    {"z=x^2+y^2", "\\text{local minimum at } \\left(0, 0\\right)"},
    {"z = x^2 - y^2", "\\text{saddle point at } \\left(0, 0\\right)"},
    {"z=x^3-3x+y^2", "\\text{saddle point at } \\left(-1, 0\\right), \\ \\text{local minimum at } \\left(1, 0\\right)"},
    {"surface(x*y)", "\\text{saddle point at } \\left(0, 0\\right)"},
    {"y=x^2-4x+3", "\\text{Parabola: vertex } \\left(2, -1\\right)"},
    {"(x-1)^2+(y+2)^2=9", "\\text{Circle: centre } \\left(1, -2\\right), \\ r = 3"},
    {"bsmcall(100,100,1,0.05,0.2,0.02)", "C = 9.2270055"},
    {"bsmput(100,100,1,0.05,0.2,0.02)", "P = 6.3300806"},
    {"merton(100,100,1,0.05,0.2,1,-0.1,0.15)", "C = 12.761289"},
    {"credit(120,100,5,0.05,0.25)", "E = 48.773617"},
    {"binomial(100,100,1,0.05,0.2,500,2)", "V = 6.0888101"},
    {"ivol(10.450584,100,100,1,0.05)", "\\sigma_{implied} = 0.20000001"},
    {"e^x = -x^2 - 1", "\\text{No solution found}"},
    {"x ^ { 2 } - 5 x + 6 = 0", "x_{1} = 2, \\quad x_{2} = 3"},
    {"\\frac { x + 1 } { x - 2 } = 3", "x = \\frac{7}{2}"},
    {"\\int _ { 0 } ^ { 3 } x ^ { 2 } d x", "\\int_{0}^{3} x^{2} \\, dx = 9"},
    {"\\frac { d } { d x } \\sin \\left( x ^ { 2 } \\right)", "f'(x) = 2 x \\cos\\left(x^{2}\\right)"},
    {"\\underset { x \\to 0 } { \\operatorname* { l i m } } \\frac { \\operatorname { s i n } x } { x }", "\\lim_{x \\to 0} \\left(\\frac{\\sin\\left(x\\right)}{x}\\right) = 1"},
    {"\\begin{cases} 2x+3y=12 \\\\ x-y=1 \\end{cases}", "x = 3, \\ y = 2"},
    {"\\begin{bmatrix} 1 & 2 \\\\ 3 & 4 \\end{bmatrix}^{-1}", "\\begin{bmatrix} -2 & 1 \\\\ \\frac{3}{2} & -\\frac{1}{2} \\end{bmatrix}"},
    {"\\left| 2x - 1 \\right| = 5", "x_{1} = -2, \\quad x_{2} = 3"},
    // large matrices, exact big integers, systems with several solutions
    {"eig([[2,1,0,0,1],[1,3,1,0,0],[0,1,4,1,0],[0,0,1,5,1],[1,0,0,1,6]])", "\\lambda_{1} \\approx 1.1347478, \\quad \\lambda_{2} \\approx 2.6182345, \\quad \\lambda_{3} \\approx 4.1450316, \\quad \\lambda_{4} \\approx 5.2004416, \\quad \\lambda_{5} \\approx 6.9015444"},
    {"eig([[2,1,0,0,0,0,0],[1,2,1,0,0,0,0],[0,1,2,1,0,0,0],[0,0,1,2,1,0,0],[0,0,0,1,2,1,0],[0,0,0,0,1,2,1],[0,0,0,0,0,1,2]])", "\\lambda_{1} \\approx 0.15224093, \\quad \\lambda_{2} \\approx 0.58578644, \\quad \\lambda_{3} \\approx 1.2346331, \\quad \\lambda_{4} \\approx 2, \\quad \\lambda_{5} \\approx 2.7653669, \\quad \\lambda_{6} \\approx 3.4142136, \\quad \\lambda_{7} \\approx 3.8477591"},
    {"gcd(123456789012345678901234567890, 987654321098765432109876543210)", "\\gcd\\left(123456789012345678901234567890, 987654321098765432109876543210\\right) = 9000000000900000000090"},
    {"det([[2,1,0,0,1,0],[1,3,1,0,0,1],[0,1,4,1,0,0],[0,0,1,5,1,0],[1,0,0,1,6,1],[0,1,0,0,1,7]])", "2774"},
    {"x+y=5; x*y=6", "x = 3, \\ y = 2 \\quad \\text{or} \\quad x = 2, \\ y = 3"},
    // 3D matrices and polar curves
    {"sum([[[1,2],[3,4]],[[5,6],[7,8]]],1)", "\\begin{bmatrix} 6 & 8 \\\\ 10 & 12 \\end{bmatrix}"},
    {"sum([[[1,2],[3,4]],[[5,6],[7,8]]],3)", "\\begin{bmatrix} 3 & 7 \\\\ 11 & 15 \\end{bmatrix}"},
    {"sum([[[1,2],[3,4]],[[5,6],[7,8]]])", "36"},
    {"slice([[[1,2],[3,4]],[[5,6],[7,8]]],1,3)", "\\begin{bmatrix} 1 & 3 \\\\ 5 & 7 \\end{bmatrix}"},
    {"shape([[[1,2,3],[4,5,6]],[[7,8,9],[1,2,3]]])", "2 \\times 2 \\times 3"},
    {"[[[1,2],[3,4]],[[5,6],[7,8]]]*[[[1,2],[3,4]],[[5,6],[7,8]]]", "\\left[\\,\\begin{bmatrix} 1 & 4 \\\\ 9 & 16 \\end{bmatrix},\\ \\begin{bmatrix} 25 & 36 \\\\ 49 & 64 \\end{bmatrix}\\,\\right]"},
    {"r = cos(5*theta)", "\\text{A rose with 5 petals}"},
    {"r=1-sin(theta)", "\\text{A cardioid}"},
    {"sin^(-1)(1/2)", "\\frac{\\pi}{6}"},
    {"tan^{-1}(1)", "\\frac{\\pi}{4}"},
};

// Extract a string field from the flat JSON produced by the engine.
std::string field(const std::string& json, std::string_view key) {
    std::string k = "\"" + std::string(key) + "\":\"";
    auto p = json.find(k);
    if (p == std::string::npos) return {};
    p += k.size();
    std::string out;
    for (; p < json.size() && json[p] != '"'; ++p) {
        if (json[p] == '\\' && p + 1 < json.size()) {
            ++p;
            out += json[p] == 'n' ? '\n' : json[p];
        } else out += json[p];
    }
    return out;
}

}  // namespace

int main() {
    int failed = 0;
    for (const auto& c : kCases) {
        std::string j = me::solveJson(c.input);
        bool ok = j.find("\"ok\":true") != std::string::npos;
        std::string ans = field(j, "answer");
        if (!ok || ans != c.answer) {
            ++failed;
            std::printf("FAIL  %s\n  expected: %s\n  got:      %s\n", c.input, c.answer, ok ? ans.c_str() : field(j, "error").c_str());
        }
    }
    // evaluator sanity
    me_program* p = me_compile("x^2 + sin(pi*x) + cbrt(-8)");
    double v = me_eval(p, 3, 0, 0);
    me_program_free(p);
    if (v < 6.999 || v > 7.001) { ++failed; std::printf("FAIL evaluator: %f\n", v); }
    // LaTeX import (what the camera model emits): systems, prose headings, doubled braces, command words
    struct LCase { const char* latex; const char* engine; };
    static const LCase kLatex[] = {
        {"\\left\\{ \\begin{matrix} { x ^ { 2 } - 5 x + 6 = 0 } \\\\ { x + y = 5 } \\\\ \\end{matrix} \\right.", "x^(2)-5x+6=0; x+y=5"},
        {"\\begin{cases} 2x+3y=12 \\\\ x-y=1 \\end{cases}", "2x+3y=12; x-y=1"},
        {"S O L V E \\cdot T H E \\cdot S Y S T E M \\cdot O F \\cdot E Q U A T I O N S", ""},
        {"\\mathrm { S \\hspace { 1 m m } I M P \\hspace { 1 m m } L I F Y }", ""},
        {"\\frac { d } { { d x } } \\left( \\sin x \\right)", "d/dx((sinx))"},  // the parser reads sinx as sin(x),
        {"\\mathrm { t a y l o r } \\big ( e ^ { x } , x , 0 , 4 \\big )", "taylor(e^(x),x,0,4)"},
        {"\\begin{bmatrix} 1 & 2 \\\\ 3 & 4 \\end{bmatrix}", "[[1,2],[3,4]]"},
        {"\\sin x \\cos x", "sinxcosx"},
        {"x \\star y \\star z = 6", "x+y+z=6"},
    };
    int lfail = 0;
    for (auto& c : kLatex) {
        char* o = me_from_latex(c.latex);
        std::string got = o ? o : "";
        me_free(o);
        if (got != c.engine) { ++lfail; std::printf("FAIL latex %s\n  expected: %s\n  got:      %s\n", c.latex, c.engine, got.c_str()); }
    }
    failed += lfail;
    std::printf("%d / %zu cases passed\n", (int)(sizeof kCases / sizeof kCases[0]) - failed, sizeof kCases / sizeof kCases[0]);
    return failed ? 1 : 0;
}
