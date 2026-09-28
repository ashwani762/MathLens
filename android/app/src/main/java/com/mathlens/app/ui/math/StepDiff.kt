// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.math

/**
 * Step highlighting: wraps what changed since the previous line of working in \hl{...}.
 *
 * LaTeX is split into safe units so a highlight can never break the markup: one character (digits
 * grouped into numbers), a command with its brace arguments (\frac{a}{b}, \sqrt[n]{x}), a brace group,
 * a whole \left...\right pair, a whole \begin...\end environment; scripts (^ _ and primes) stay attached
 * to their base. Units that are not part of the longest common subsequence with the previous step are new.
 */
object StepDiff {
    fun highlightChanges(prev: String?, cur: String): String {
        if (prev.isNullOrBlank() || cur.isBlank() || prev == cur) return cur
        if ("\\begin" in cur || "\\begin" in prev) return cur  // matrices and systems: too coarse to be useful
        val a = units(prev); val b = units(cur)
        if (a.isEmpty() || b.isEmpty() || a.size > 160 || b.size > 160) return cur
        // compare ignoring typesetting-only differences (explicit \\cdot, spacing), keep the original text
        fun key(u: String) = u.replace("\\cdot", "").replace(" ", "")
        val keep = lcsMask(a.map(::key), b.map(::key))
        val changed = keep.count { !it }
        if (changed == 0 || changed > b.size * 0.6) return cur  // nothing, or almost everything, is new
        val out = StringBuilder()
        var i = 0
        while (i < b.size) {
            if (keep[i]) { out.append(b[i]).append(' '); i++; continue }
            var j = i
            while (j < b.size && !keep[j]) j++
            val run = b.subList(i, j)
            // a lone relation or spacing is not a meaningful change
            val trivial = run.all { it == "=" || it.startsWith("\\quad") || it == "," || it == "\\," || it == "\\cdot" }
            if (trivial) run.forEach { out.append(it).append(' ') }
            else out.append("\\hl{").append(run.joinToString(" ")).append("} ")
            i = j
        }
        return out.toString().trim()
    }

    internal fun units(s: String): List<String> {
        val out = ArrayList<String>()
        var i = 0
        val n = s.length
        fun group(open: Char, close: Char): String {  // s[i] == open; returns the balanced group, advancing i
            val start = i
            var depth = 0
            while (i < n) {
                val c = s[i]
                if (c == '\\' && i + 1 < n) { i += 2; continue }
                if (c == open) depth++
                if (c == close) { depth--; if (depth == 0) { i++; break } }
                i++
            }
            return s.substring(start, i)
        }
        fun command(): String {  // s[i] == '\\'
            val start = i
            i++
            if (i < n && s[i].isLetter()) { while (i < n && s[i].isLetter()) i++ } else if (i < n) i++
            return s.substring(start, i)
        }
        fun skipSpace() { while (i < n && s[i] == ' ') i++ }
        fun scriptArg(): String {
            skipSpace()
            if (i >= n) return ""
            return when {
                s[i] == '{' -> group('{', '}')
                s[i] == '\\' -> command()
                else -> s[i++].toString()
            }
        }
        while (i < n) {
            val c = s[i]
            when {
                c == ' ' -> i++
                c == '^' || c == '_' -> {
                    i++
                    val arg = c + scriptArg()
                    if (out.isEmpty()) out.add(arg) else out[out.size - 1] = out.last() + arg
                }
                c == '\'' -> { i++; if (out.isEmpty()) out.add("'") else out[out.size - 1] = out.last() + "'" }
                c == '{' -> out.add(group('{', '}'))
                c.isDigit() || (c == '.' && i + 1 < n && s[i + 1].isDigit()) -> {
                    val start = i
                    while (i < n && (s[i].isDigit() || s[i] == '.')) i++
                    out.add(s.substring(start, i))
                }
                c == '\\' -> {
                    val start = i
                    val cmd = command()
                    when (cmd) {
                        "\\left" -> {  // take everything up to the matching \right and its delimiter
                            var depth = 1
                            while (i < n && depth > 0) {
                                if (s.startsWith("\\left", i)) { depth++; i += 5; continue }
                                if (s.startsWith("\\right", i)) { depth--; i += 6; if (depth == 0) { if (i < n && s[i] == '\\') command() else if (i < n) i++ }; continue }
                                i++
                            }
                            out.add(s.substring(start, i))
                        }
                        "\\begin" -> {
                            val env = if (i < n && s[i] == '{') group('{', '}') else ""
                            val end = s.indexOf("\\end$env", i)
                            i = if (end < 0) n else end + 4 + env.length
                            out.add(s.substring(start, i))
                        }
                        else -> {
                            // arguments: optional [..] then brace groups directly after the command
                            while (i < n && (s[i] == '{' || (s[i] == '[' && cmd == "\\sqrt"))) {
                                if (s[i] == '{') group('{', '}') else group('[', ']')
                            }
                            out.add(s.substring(start, i))
                        }
                    }
                }
                else -> { out.add(c.toString()); i++ }
            }
        }
        return out
    }

    /** true where b[k] belongs to a longest common subsequence of a and b. */
    private fun lcsMask(a: List<String>, b: List<String>): BooleanArray {
        val dp = Array(a.size + 1) { IntArray(b.size + 1) }
        for (x in a.size - 1 downTo 0) for (y in b.size - 1 downTo 0)
            dp[x][y] = if (a[x] == b[y]) dp[x + 1][y + 1] + 1 else maxOf(dp[x + 1][y], dp[x][y + 1])
        val keep = BooleanArray(b.size)
        var x = 0; var y = 0
        while (x < a.size && y < b.size) {
            when {
                a[x] == b[y] -> { keep[y] = true; x++; y++ }
                dp[x + 1][y] >= dp[x][y + 1] -> x++
                else -> y++
            }
        }
        return keep
    }
}
