// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Ashwani

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdio>

#include "solution/steps.h"

namespace me {

std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o;
}

namespace {

class Writer {
public:
    std::string out;

    void str(const std::string& s) { out += '"'; out += jsonEscape(s); out += '"'; }
    template <std::floating_point F>
    void number(F v) {
        if (!std::isfinite(v)) { out += "null"; return; }
        char b[40];
        snprintf(b, sizeof b, "%.12g", (double)v);
        out += b;
    }
    void boolean(bool b) { out += b ? "true" : "false"; fresh_ = false; }
    void key(const char* k) { comma(); str(k); out += ':'; fresh_ = true; }
    void begin(char c) { comma(); out += c; fresh_ = true; }
    void end(char c) { out += c; fresh_ = false; }
    void item() { comma(); }
    // call before writing a value in an array
    void comma() {
        if (!fresh_) out += ',';
        fresh_ = false;
    }
    void valueStr(const std::string& s) { str(s); fresh_ = false; }
    template <std::floating_point F>
    void valueNum(F v) { number(v); fresh_ = false; }

private:
    bool fresh_ = true;
};

void writeStep(Writer& w, const Step& s) {
    w.begin('{');
    w.key("title"); w.valueStr(s.title);
    if (!s.text.empty()) { w.key("text"); w.valueStr(s.text); }
    if (!s.latex.empty()) { w.key("latex"); w.valueStr(s.latex); }
    if (!s.sub.empty()) {
        w.key("sub");
        w.begin('[');
        for (auto& c : s.sub) writeStep(w, c);
        w.end(']');
    }
    w.end('}');
}

void writePlot(Writer& w, const PlotSpec& p) {
    w.begin('{');
    w.key("curves");
    w.begin('[');
    for (auto& c : p.curves) {
        w.begin('{');
        w.key("type");
        const char* t = c.type == PlotCurve::Type::Explicit ? "explicit"
                        : c.type == PlotCurve::Type::Implicit ? "implicit"
                        : c.type == PlotCurve::Type::Polar ? "polar"
                        : c.type == PlotCurve::Type::Surface ? "surface" : "vertical";
        w.valueStr(t);
        w.key("expr"); w.valueStr(c.expr);
        w.key("label"); w.valueStr(c.label);
        w.end('}');
    }
    w.end(']');
    w.key("points");
    w.begin('[');
    for (auto& pt : p.points) {
        w.begin('{');
        w.key("x"); w.valueNum(pt.x);
        w.key("y"); w.valueNum(pt.y);
        w.key("label"); w.valueStr(pt.label);
        w.end('}');
    }
    w.end(']');
    if (p.shade) {
        w.key("shade");
        w.begin('[');
        w.item(); w.number(p.shade->first);
        w.item(); w.number(p.shade->second);
        w.end(']');
    }
    if (p.shape) {
        w.key("shape");
        w.begin('{');
        w.key("type"); w.valueStr(p.shape->type);
        w.key("params");
        w.begin('[');
        for (double v : p.shape->params) { w.item(); w.number(v); }
        w.end(']');
        w.end('}');
    }
    w.key("view");
    w.begin('[');
    for (double v : {p.xmin, p.xmax, p.ymin, p.ymax}) { w.item(); w.number(v); }
    w.end(']');
    w.key("auto"); w.boolean(p.autoRange);
    if (!p.params.empty()) {
        w.key("params");
        w.begin('[');
        for (auto& n : p.params) { w.item(); w.str(n); }
        w.end(']');
    }
    w.end('}');
}

}  // namespace

std::string toJson(const PlotSpec& p) {
    Writer w;
    writePlot(w, p);
    return w.out;
}

std::string toJson(const Solution& s) {
    Writer w;
    w.begin('{');
    w.key("ok"); w.boolean(s.ok);
    if (!s.ok) { w.key("error"); w.valueStr(s.error); }
    w.key("kind"); w.valueStr(s.kind);
    w.key("title"); w.valueStr(s.title);
    w.key("input"); w.valueStr(s.inputLatex);
    w.key("answer"); w.valueStr(s.answerLatex);
    w.key("answers");
    w.begin('[');
    for (auto& a : s.answers) { w.item(); w.valueStr(a); }
    w.end(']');
    w.key("facts");
    w.begin('[');
    for (auto& [k, v] : s.facts) {
        w.begin('{');
        w.key("label"); w.valueStr(k);
        w.key("latex"); w.valueStr(v);
        w.end('}');
    }
    w.end(']');
    w.key("methods");
    w.begin('[');
    for (auto& m : s.methods) {
        w.begin('{');
        w.key("name"); w.valueStr(m.name);
        w.key("steps");
        w.begin('[');
        for (auto& st : m.steps) writeStep(w, st);
        w.end(']');
        w.end('}');
    }
    w.end(']');
    if (s.plot) { w.key("plot"); writePlot(w, *s.plot); }
    w.key("anim"); w.valueStr(s.anim);
    w.key("story");
    w.begin('[');
    for (auto& c : s.story) { w.item(); w.valueStr(c); }
    w.end(']');
    w.end('}');
    return w.out;
}

}  // namespace me
