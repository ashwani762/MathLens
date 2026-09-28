// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.camera

/**
 * Cleans raw OCR text into engine syntax. On-device text recognition reads printed / neatly
 * handwritten math line by line; it can't see superscripts or fraction bars, so a few
 * heuristics recover the most common layouts ("x2" -> "x^2").
 */
object OcrNormalizer {
    private val functionNames = listOf("sin", "cos", "tan", "cot", "sec", "csc", "log", "ln", "sqrt", "lim", "int", "exp", "abs")

    fun normalize(raw: String): String {
        var s = raw.trim()
        val replacements = mapOf(
            "×" to "*", "·" to "*", "•" to "*", "−" to "-", "–" to "-", "—" to "-", "÷" to "÷",
            "＝" to "=", "≤" to "<=", "≥" to ">=", "√" to "√", "π" to "pi", " " to " ",
            "X" to "x", "Y" to "y",
        )
        replacements.forEach { (a, b) -> s = s.replace(a, b) }
        // letter O inside numbers -> zero, l / I between digits -> 1
        s = s.replace(Regex("(?<=\\d)[oO](?=\\d)"), "0")
        s = s.replace(Regex("(?<=\\d)[lI|](?=\\d)"), "1")
        // x2 -> x^2 (single variable letter directly followed by a digit, not inside a function name)
        s = Regex("(?<![a-zA-Z])([a-zA-Z])(\\d+)").replace(s) { m ->
            val v = m.groupValues[1]
            if (v.lowercase() in listOf("x", "y", "z", "a", "b", "n", "t")) "$v^${m.groupValues[2]}" else m.value
        }
        // )2 -> )^2
        s = Regex("\\)(\\d)(?![\\d.])").replace(s) { ")^${it.groupValues[1]}" }
        // remove stray trailing punctuation from OCR
        s = s.trimEnd('.', ',', ';', ':')
        return s
    }

    /** Score a candidate line: prefer lines that look like math. */
    fun mathScore(line: String): Int {
        var score = 0
        if (line.any { it in "=+-*/^<>" }) score += 3
        if (line.any { it.isDigit() }) score += 2
        if (functionNames.any { line.contains(it) }) score += 2
        if (line.count { it.isLetter() } > line.length * 0.7) score -= 3
        return score
    }
}
