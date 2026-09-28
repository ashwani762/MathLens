// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.components

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Backspace
import androidx.compose.material.icons.automirrored.outlined.KeyboardReturn
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Popup
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.math.MathFit
import androidx.compose.foundation.layout.fillMaxSize
import com.mathlens.app.ui.theme.Tone
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/** A key: [label] is LaTeX (rendered) unless [plain]; inserts [insert] and puts the cursor at [cursor]. */
data class Key(
    val label: String,
    val insert: String,
    val cursor: Int = insert.length,
    val plain: Boolean = false,
    val kind: KeyKind = KeyKind.Normal,
    val alternates: List<Key> = emptyList(),
)

enum class KeyKind { Normal, Operator, Accent }

private fun k(label: String, insert: String = label, cursor: Int = insert.length, alt: List<Key> = emptyList()) = Key(label, insert, cursor, false, KeyKind.Normal, alt)
private fun t(label: String, insert: String, cursor: Int = insert.length, alt: List<Key> = emptyList()) = Key(label, insert, cursor, true, KeyKind.Normal, alt)
private fun op(label: String, insert: String, alt: List<Key> = emptyList()) = Key(label, insert, insert.length, false, KeyKind.Operator, alt)

private val xKey = k("x", alt = listOf(k("y"), k("z"), k("t"), k("\\theta", "theta"), k("n")))
private val powKey = op("x^{\\square}", "^", alt = listOf(k("x^{2}", "^2"), k("x^{3}", "^3"), k("x^{-1}", "^-1"), k("e^{\\square}", "e^()", 3)))
private val sinKey = k("\\sin", "sin()", 4, listOf(k("\\arcsin", "asin()", 5), k("\\sinh", "sinh()", 5), k("\\csc", "csc()", 4)))
private val cosKey = k("\\cos", "cos()", 4, listOf(k("\\arccos", "acos()", 5), k("\\cosh", "cosh()", 5), k("\\sec", "sec()", 4)))
private val tanKey = k("\\tan", "tan()", 4, listOf(k("\\arctan", "atan()", 5), k("\\tanh", "tanh()", 5), k("\\cot", "cot()", 4)))
private val sqrtKey = k("\\sqrt{\\square}", "sqrt()", 5, listOf(k("\\sqrt[3]{\\square}", "cbrt()", 5), k("\\sqrt[n]{x}", "root(,)", 5)))
private val fracKey = k("\\frac{\\square}{\\square}", "()/()", 1)

/** Tabs: the grid on the left changes (swipe or tap); the operator rail on the right never does. */
private val tabs: List<Pair<String, List<List<Key>>>> = listOf(
    "123" to listOf(
        listOf(k("7"), k("8"), k("9"), xKey, k("\\pi", "pi", alt = listOf(k("e"), k("\\infty", "inf"), k("i")))),
        listOf(k("4"), k("5"), k("6"), k("y", alt = listOf(k("z"), k("r"))), sqrtKey),
        listOf(k("1"), k("2"), k("3"), k("x^{2}", "^2", alt = listOf(k("x^{3}", "^3"))), fracKey),
        listOf(k("0"), k(".", alt = listOf(k(","))), k(","), k("n!", "!"), k("\\%", "%")),
    ),
    "f(x)" to listOf(
        listOf(sinKey, cosKey, tanKey, k("\\ln", "ln()", 3), k("\\log", "log()", 4, listOf(k("\\log_{\\square}", "log_()()", 5)))),
        // inverse trig in plain sight (hyperbolic and reciprocal functions stay on long-press of sin/cos/tan)
        listOf(k("\\sin^{-1}", "asin()", 5), k("\\cos^{-1}", "acos()", 5), k("\\tan^{-1}", "atan()", 5), k("e^{\\square}", "e^()", 3), k("\\left|x\\right|", "abs()", 4)),
        listOf(k("i"), k("z"), k("\\overline{z}", "conj()", 5), k("\\theta", "theta"), k("\\infty", "inf")),
        listOf(op("<", "<"), op(">", ">"), op("\\le", "<="), op("\\ge", ">="), k("\\binom{n}{r}", "nCr(,)", 4, listOf(k("P(n,r)", "nPr(,)", 4)))),
    ),
    "∫" to listOf(
        listOf(k("\\frac{d}{dx}", "d/dx()", 5), k("\\int", "int(, x)", 4), k("\\int_{a}^{b}", "int(, x, 0, 1)", 4), k("\\lim", "lim(, x, 0)", 4), k("\\to", "->")),
        listOf(k("y'", "y'"), k("y''", "y''"), k("\\frac{dy}{dx}", "dy/dx"), t("taylor", "taylor(, x, 0, 5)", 7), k("z")),
        listOf(t("solve", "solve()", 6), t("factor", "factor()", 7), t("expand", "expand()", 7), t("simplify", "simplify()", 9), t("surface", "surface()", 8)),
        listOf(t("gcd", "gcd(,)", 4), t("lcm", "lcm(,)", 4), t("mean", "mean()", 5), t("stats", "stats()", 6), t("mod", " mod ")),
    ),
    // Digits live only on the 123 page (switching is one tap); function pages never show a partial number pad.
    "[ ]" to listOf(
        listOf(k("\\begin{bmatrix}a&b\\\\c&d\\end{bmatrix}", "[[,],[,]]", 2), k("\\begin{bmatrix}\\cdot&\\cdot&\\cdot\\\\\\cdot&\\cdot&\\cdot\\\\\\cdot&\\cdot&\\cdot\\end{bmatrix}", "[[,,],[,,],[,,]]", 2),
            t("3D", "[[[,],[,]],[[,],[,]]]", 3), k("I_{n}", "identity()", 9), k("A^{T}", "^T")),
        listOf(t("det", "det()", 4), t("inv", "inv()", 4), t("eig", "eig()", 4), t("rref", "rref()", 5), t("rank", "rank()", 5)),
        listOf(t("trace", "trace()", 6), t("shape", "shape()", 6), t("sum", "sum()", 4), t("slice", "slice(,)", 6), k("\\lambda", "lambda")),
        listOf(k("["), k("]"), k(","), xKey, k("y")),
    ),
    "$" to listOf(
        listOf(t("fv", "fv(,,)", 3), t("pv", "pv(,,)", 3), t("pmt", "pmt(,,)", 4), t("npv", "npv(,)", 4), t("irr", "irr(,)", 4)),
        listOf(t("cagr", "cagr(,,)", 5), t("bond", "bond(,,,)", 5), t("sharpe", "sharpe(,,)", 7), t("call", "bscall(,,,,)", 7), t("put", "bsput(,,,,)", 6)),
        listOf(t("\u03A6(x)", "normcdf()", 8), t("e^rt", "cont(,,)", 5), t("BSM", "bsmcall(,,,,,)", 8), t("jump", "merton(,,,,,,,)", 7), t("ivol", "ivol(,,,,)", 5)),
        listOf(t("mean", "mean()", 5), t("median", "median()", 7), t("stdev", "stdev()", 6), t("var", "variance()", 9), t("tree", "binomial(,,,,,)", 9)),
    ),
    "abc" to listOf(
        "qwertyuiop".map { k(it.toString()) },
        "asdfghjkl".map { k(it.toString()) },
        "zxcvbnm".map { k(it.toString()) } + listOf(k("\\_", "_"), k("'", "'")),
        // operators stay one tap away while typing letters
        listOf(op("(", "("), op(")", ")"), powKey, op("+", "+"), op("-", "-"), op("\\times", "*"), op("\\div", "\u00F7"), op("=", "=")),
    ),
)

/** The operator rail: always visible, whatever the tab. */
private val rail: List<List<Key>> = listOf(
    listOf(op("(", "("), op(")", ")")),
    listOf(powKey, op("\\div", "÷", listOf(k("/", "/")))),
    listOf(op("\\times", "*"), op("-", "-", listOf(k("\\pm", "+-")))),
    listOf(op("=", "=", listOf(k("<", "<"), k(">", ">"), k("\\le", "<="), k("\\ge", ">="))), op("+", "+")),
)

private val functionNames = listOf("sin", "cos", "tan", "ln", "log", "sqrt", "abs", "det", "inv", "eig")


/** Everything the engine understands by name: (name, label with argument hints, insert template, cursor in template). */
private val completions: List<Triple<String, String, Pair<String, Int>>> = listOf(
    Triple("sin", "sin", "sin()" to 4), Triple("cos", "cos", "cos()" to 4), Triple("tan", "tan", "tan()" to 4),
    Triple("asin", "asin (sin\u207B\u00B9)", "asin()" to 5), Triple("acos", "acos (cos\u207B\u00B9)", "acos()" to 5), Triple("atan", "atan (tan\u207B\u00B9)", "atan()" to 5),
    Triple("sinh", "sinh", "sinh()" to 5), Triple("cosh", "cosh", "cosh()" to 5), Triple("tanh", "tanh", "tanh()" to 5),
    Triple("sec", "sec", "sec()" to 4), Triple("csc", "csc", "csc()" to 4), Triple("cot", "cot", "cot()" to 4),
    Triple("ln", "ln", "ln()" to 3), Triple("log", "log", "log()" to 4), Triple("exp", "exp", "exp()" to 4),
    Triple("sqrt", "sqrt", "sqrt()" to 5), Triple("cbrt", "cbrt", "cbrt()" to 5), Triple("abs", "abs", "abs()" to 4),
    Triple("int", "int(f, x)", "int(, x)" to 4), Triple("lim", "lim(f, x, a)", "lim(, x, 0)" to 4),
    Triple("solve", "solve", "solve()" to 6), Triple("factor", "factor", "factor()" to 7), Triple("expand", "expand", "expand()" to 7),
    Triple("simplify", "simplify", "simplify()" to 9), Triple("taylor", "taylor(f, x, a, n)", "taylor(, x, 0, 5)" to 7),
    Triple("surface", "surface(f)", "surface()" to 8), Triple("dsolve", "dsolve", "dsolve()" to 7),
    Triple("det", "det", "det()" to 4), Triple("inv", "inv", "inv()" to 4), Triple("eig", "eig", "eig()" to 4),
    Triple("rref", "rref", "rref()" to 5), Triple("rank", "rank", "rank()" to 5), Triple("trace", "trace", "trace()" to 6),
    Triple("identity", "identity(n)", "identity()" to 9), Triple("shape", "shape", "shape()" to 6),
    Triple("slice", "slice(A, i)", "slice(,)" to 6), Triple("sum", "sum(A, axis)", "sum()" to 4),
    Triple("gcd", "gcd(a, b)", "gcd(,)" to 4), Triple("lcm", "lcm(a, b)", "lcm(,)" to 4), Triple("mod", "mod", " mod " to 5),
    Triple("nCr", "nCr(n, r)", "nCr(,)" to 4), Triple("nPr", "nPr(n, r)", "nPr(,)" to 4),
    Triple("conj", "conj", "conj()" to 5), Triple("floor", "floor", "floor()" to 6), Triple("ceil", "ceil", "ceil()" to 5), Triple("round", "round", "round()" to 6),
    Triple("mean", "mean", "mean()" to 5), Triple("median", "median", "median()" to 7), Triple("mode", "mode", "mode()" to 5),
    Triple("stdev", "stdev", "stdev()" to 6), Triple("variance", "variance", "variance()" to 9), Triple("stats", "stats", "stats()" to 6),
    Triple("fv", "fv(rate, n, pv)", "fv(,,)" to 3), Triple("pv", "pv(rate, n, fv)", "pv(,,)" to 3), Triple("pmt", "pmt(rate, n, loan)", "pmt(,,)" to 4),
    Triple("npv", "npv(rate, cf\u2080, \u2026)", "npv(,)" to 4), Triple("irr", "irr(cf\u2080, cf\u2081, \u2026)", "irr(,)" to 4),
    Triple("cagr", "cagr(start, end, years)", "cagr(,,)" to 5), Triple("cont", "cont(P, r, t)", "cont(,,)" to 5),
    Triple("bond", "bond(face, coupon, yield, years)", "bond(,,,)" to 5), Triple("sharpe", "sharpe(ret, rf, vol)", "sharpe(,,)" to 7),
    Triple("normcdf", "normcdf(x)", "normcdf()" to 8), Triple("bscall", "bscall(S, K, T, r, \u03C3)", "bscall(,,,,)" to 7),
    Triple("bsput", "bsput(S, K, T, r, \u03C3)", "bsput(,,,,)" to 6), Triple("bsmcall", "bsmcall(S, K, T, r, \u03C3, q)", "bsmcall(,,,,,)" to 8),
    Triple("bsmput", "bsmput(S, K, T, r, \u03C3, q)", "bsmput(,,,,,)" to 7), Triple("merton", "merton(S, K, T, r, \u03C3, \u03BB, \u03BC, \u03B4)", "merton(,,,,,,,)" to 7),
    Triple("credit", "credit(V, D, T, r, \u03C3)", "credit(,,,,)" to 7), Triple("binomial", "binomial(S, K, T, r, \u03C3, steps)", "binomial(,,,,,)" to 9),
    Triple("ivol", "ivol(price, S, K, T, r)", "ivol(,,,,)" to 5),
    Triple("theta", "\u03B8 theta", "theta" to 5), Triple("pi", "\u03C0 pi", "pi" to 2), Triple("inf", "\u221E inf", "inf" to 3), Triple("lambda", "\u03BB lambda", "lambda" to 6),
)

/** Completions for the letters just before the cursor ("ta" -> tan, tanh, taylor). Inserts only the missing part. */
private fun completionsFor(before: String): List<Key> {
    val prefix = before.takeLastWhile { it.isLetter() }
    if (prefix.isEmpty()) return emptyList()
    return completions
        .filter { (name, _, _) -> name.startsWith(prefix, ignoreCase = true) && !(name.equals(prefix, true) && name.length <= 2) }
        .map { (name, label, tpl) -> Key(label, tpl.first.substring(prefix.length.coerceAtMost(tpl.first.length)), (tpl.second - prefix.length).coerceAtLeast(0), plain = true) }
}

/** Context suggestions for the next token, based on what sits before the cursor. */
fun suggestionsFor(text: String, cursor: Int): List<Key> {
    val before = text.substring(0, cursor.coerceIn(0, text.length)).trimEnd()
    val open = before.count { it == '(' } - before.count { it == ')' }
    val out = mutableListOf<Key>()
    if (open > 0) out += Key(")", ")", plain = true)
    val last = before.lastOrNull()
    // typing letters: offer every function whose name starts with them
    val typed = completionsFor(text.substring(0, cursor.coerceIn(0, text.length)))
    if (typed.isNotEmpty()) return (typed + out).distinctBy { it.label }.take(8)
    when {
        before.isEmpty() -> out += listOf(k("\\frac{d}{dx}", "d/dx()", 5), k("\\int", "int(, x)", 4), k("\\sqrt{\\square}", "sqrt()", 5), k("\\lim", "lim(, x, 0)", 4), t("solve", "solve()", 6))
        functionNames.any { before.endsWith(it) } -> out += Key("(", "(", plain = true)
        last != null && last.isDigit() -> out += listOf(k("x^{2}", "^2"), k("("), k("="), k("\\%", "%"))
        last != null && last.isLetter() -> out += listOf(k("^{2}", "^2"), k("^{3}", "^3"), k("="), k("+"), k("-"))
        last == ')' -> out += listOf(k("^{2}", "^2"), k("="), k("+"), k("\\cdot", "*"))
        last == '=' -> out += listOf(k("0"))
    }
    return out.distinctBy { it.insert }.take(7)
}

@Composable
fun MathKeyboard(
    text: String,
    cursor: Int,
    onKey: (Key) -> Unit,
    onBackspace: () -> Unit,
    onClear: () -> Unit,
    onLeft: () -> Unit,
    onRight: () -> Unit,
    onUp: (() -> Unit)? = null,
    onDown: (() -> Unit)? = null,
    onSolve: () -> Unit,
    onGraph: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val pager = rememberPagerState { tabs.size }
    val scope = rememberCoroutineScope()
    val haptic = LocalHapticFeedback.current
    val tap: (Key) -> Unit = { haptic.performHapticFeedback(HapticFeedbackType.TextHandleMove); onKey(it) }
    Column(
        modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(topStart = 22.dp, topEnd = 22.dp))
            .background(Tone.Bone)
            .border(1.dp, Tone.Hairline, RoundedCornerShape(topStart = 22.dp, topEnd = 22.dp))
            .navigationBarsPadding()
            .padding(horizontal = 8.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        // suggestion strip + cursor controls
        Row(verticalAlignment = Alignment.CenterVertically) {
            Row(Modifier.weight(1f).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                suggestionsFor(text, cursor).forEach { s ->
                    Box(
                        Modifier.height(32.dp).clip(RoundedCornerShape(16.dp)).background(Tone.Ivory).border(1.dp, Tone.Hairline, RoundedCornerShape(16.dp))
                            .bouncyClick { tap(s) }.padding(horizontal = 12.dp),
                        contentAlignment = Alignment.Center,
                    ) {
                        if (s.plain) Text(s.label, style = MaterialTheme.typography.labelMedium, color = Tone.Ink)
                        else MathView(s.label, Modifier.height(28.dp), fontSize = 15.sp, color = Tone.Ink, center = true, fit = MathFit.Shrink)
                    }
                }
            }
            // line jumps appear once there is more than one line (systems)
            if (onUp != null && text.contains('\n')) SmallKey("\u2191", onUp)
            if (onDown != null && text.contains('\n')) SmallKey("\u2193", onDown)
            SmallKey("‹", onLeft)
            SmallKey("›", onRight)
        }
        // grid (swipe between tabs) + operator rail. The pager keeps a constant width (a resizing pager
        // leaves pages straddling); the rail is drawn inside every page except abc, which has its own operators.
        HorizontalPager(pager, Modifier.fillMaxWidth(), pageSpacing = 6.dp) { page ->
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Column(Modifier.weight(5f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    tabs[page].second.forEach { row ->
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) { row.forEach { key -> KeyCap(key, Modifier.weight(1f), tap) } }
                    }
                }
                if (tabs[page].first != "abc") Column(Modifier.weight(2f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    rail.forEach { row -> Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) { row.forEach { key -> KeyCap(key, Modifier.weight(1f), tap) } } }
                }
            }
        }
        // bottom bar: tabs over the 5 page columns; delete and new-line as key caps under the 2 rail columns
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(Modifier.weight(5f).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                tabs.forEachIndexed { i, (name, _) ->
                    val selected = pager.currentPage == i
                    Box(
                        Modifier.height(40.dp).clip(RoundedCornerShape(20.dp)).background(if (selected) Tone.Ink else Color.Transparent)
                            .bouncyClick { scope.launch { pager.scrollToPage(i) } }.padding(horizontal = 11.dp),
                        contentAlignment = Alignment.Center,
                    ) { Text(name, style = MaterialTheme.typography.labelLarge, color = if (selected) Tone.Ivory else Tone.Graphite) }
                }
            }
            Row(Modifier.weight(2f), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                DeleteKey(onBackspace, onClear, Modifier.weight(1f))
                Box(
                    Modifier.weight(1f).height(46.dp).clip(RoundedCornerShape(10.dp)).background(Tone.Linen).border(1.dp, Tone.Hairline, RoundedCornerShape(10.dp))
                        .bouncyClick { tap(Key("next", "\n")) },
                    contentAlignment = Alignment.Center,
                ) { Icon(Icons.AutoMirrored.Outlined.KeyboardReturn, "New line (next equation)", tint = Tone.Ink, modifier = Modifier.size(22.dp)) }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            GradientButton("Graph", Modifier.weight(1f), style = ButtonStyle.Outline, height = 48.dp, onClick = onGraph)
            GradientButton("Solve", Modifier.weight(1f), height = 48.dp, onClick = onSolve)
        }
    }
}

@Composable
private fun SmallKey(label: String, onClick: () -> Unit) {
    Box(
        Modifier.padding(start = 6.dp).size(32.dp).clip(RoundedCornerShape(16.dp)).background(Tone.Ivory).border(1.dp, Tone.Hairline, RoundedCornerShape(16.dp))
            .bouncyClick(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { Text(label, style = MaterialTheme.typography.titleMedium, color = Tone.Ink) }
}

/** Delete: tap removes one character; hold to repeat (accelerating); keep holding to clear everything. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun DeleteKey(onBackspace: () -> Unit, onClear: () -> Unit, modifier: Modifier = Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val haptic = LocalHapticFeedback.current
    LaunchedEffect(pressed) {
        if (!pressed) return@LaunchedEffect
        delay(420)
        var d = 110L
        var count = 0
        while (true) {
            onBackspace(); count++
            if (count > 24) { onClear(); haptic.performHapticFeedback(HapticFeedbackType.LongPress); break }
            delay(d); d = (d * 0.9).toLong().coerceAtLeast(40)
        }
    }
    Box(
        modifier.height(46.dp).scale(if (pressed) 0.95f else 1f).clip(RoundedCornerShape(10.dp)).background(Tone.Linen)
            .border(1.dp, Tone.Hairline, RoundedCornerShape(10.dp))
            .combinedClickable(interactionSource = source, indication = null, onClick = { onBackspace() }),
        contentAlignment = Alignment.Center,
    ) { Icon(Icons.AutoMirrored.Outlined.Backspace, "Delete (hold to repeat)", tint = Tone.Ink, modifier = Modifier.size(22.dp)) }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun KeyCap(key: Key, modifier: Modifier, onKey: (Key) -> Unit) {
    var showAlt by remember { mutableStateOf(false) }
    val haptic = LocalHapticFeedback.current
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val bg = when (key.kind) {
        KeyKind.Operator -> Tone.Linen
        KeyKind.Accent -> Tone.Ink
        KeyKind.Normal -> Tone.Ivory
    }
    Box(
        modifier
            .height(46.dp)
            .scale(if (pressed) 0.95f else 1f)
            .clip(RoundedCornerShape(10.dp))
            .background(bg)
            .border(1.dp, Tone.Hairline, RoundedCornerShape(10.dp))
            .combinedClickable(
                interactionSource = source, indication = null,
                onClick = { onKey(key) },
                onLongClick = if (key.alternates.isNotEmpty()) ({ haptic.performHapticFeedback(HapticFeedbackType.LongPress); showAlt = true }) else null,
            ),
        contentAlignment = Alignment.Center,
    ) {
        if (key.plain) Text(
            key.label,
            style = MaterialTheme.typography.labelLarge.copy(fontSize = when { key.label.length <= 4 -> 14.sp; key.label.length <= 6 -> 12.5.sp; else -> 11.sp }),
            color = if (key.kind == KeyKind.Accent) Tone.Ivory else Tone.Ink,
            maxLines = 1, softWrap = false,
        )
        else MathView(key.label, Modifier.fillMaxSize().padding(horizontal = 4.dp, vertical = 5.dp), fontSize = if (key.label.length > 12) 10.sp else 17.sp,
            color = Tone.Ink, center = true, fit = MathFit.Shrink)
        if (key.alternates.isNotEmpty()) {
            // a tiny corner mark hints at long-press alternates
            Box(Modifier.align(Alignment.TopEnd).padding(5.dp).size(3.dp).clip(RoundedCornerShape(2.dp)).background(Tone.Stone))
        }
        if (showAlt) {
            Popup(alignment = Alignment.TopCenter, offset = IntOffset(0, -150), onDismissRequest = { showAlt = false }) {
                Row(
                    Modifier.clip(RoundedCornerShape(14.dp)).background(Tone.Ivory).border(1.dp, Tone.Hairline, RoundedCornerShape(14.dp)).padding(6.dp),
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    key.alternates.forEach { alt ->
                        Box(
                            Modifier.size(width = 52.dp, height = 44.dp).clip(RoundedCornerShape(10.dp)).background(Tone.Bone)
                                .bouncyClick { showAlt = false; onKey(alt) },
                            contentAlignment = Alignment.Center,
                        ) {
                            if (alt.plain) Text(alt.label, style = MaterialTheme.typography.labelMedium, color = Tone.Ink)
                            else MathView(alt.label, Modifier.fillMaxSize().padding(4.dp), fontSize = 16.sp, color = Tone.Ink, center = true, fit = MathFit.Shrink)
                        }
                    }
                }
            }
        }
    }
}
