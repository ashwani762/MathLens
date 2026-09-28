// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

// Interactive command line front-end for the engine.
//   mathcli "2x+3=7"          -> readable steps
//   mathcli --json "x^2=4"    -> raw JSON
//   mathcli                   -> REPL
#include <cstdio>
#include <iostream>
#include <string>

#include "mathengine/mathengine.h"

namespace {

// Tiny JSON pretty printer for readable output (no external dependency).
std::string pretty(const std::string& j) {
    std::string out;
    int ind = 0;
    bool str = false;
    for (size_t i = 0; i < j.size(); ++i) {
        char c = j[i];
        if (str) {
            out += c;
            if (c == '\\') out += j[++i];
            else if (c == '"') str = false;
            continue;
        }
        switch (c) {
            case '"': str = true; out += c; break;
            case '{': case '[': out += c; out += '\n'; out += std::string(2 * ++ind, ' '); break;
            case '}': case ']': out += '\n'; out += std::string(2 * --ind, ' '); out += c; break;
            case ',': out += ",\n"; out += std::string(2 * ind, ' '); break;
            case ':': out += ": "; break;
            default: out += c;
        }
    }
    return out;
}

void run(const std::string& input, bool json) {
    std::string r = me::solveJson(input);
    std::cout << (json ? r : pretty(r)) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool json = false;
    std::string input;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") json = true;
        else if (a == "--graph" && i + 1 < argc) { std::cout << me::graphJson(argv[++i]) << "\n"; return 0; }
        else if (a == "--latex" && i + 1 < argc) { std::cout << me::latexJson(argv[++i]) << "\n"; return 0; }
        else input += (input.empty() ? "" : " ") + a;
    }
    if (!input.empty()) { run(input, json); return 0; }
    std::string line;
    std::cout << "MathEngine REPL. Type an expression, equation or command (Ctrl-D to quit).\n> " << std::flush;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) run(line, json);
        std::cout << "> " << std::flush;
    }
    return 0;
}
