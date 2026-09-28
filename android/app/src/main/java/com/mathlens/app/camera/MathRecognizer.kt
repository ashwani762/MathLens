// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.camera

import android.graphics.Bitmap
import com.mathlens.app.engine.MathEngine

/** One recognised candidate with its engine preview (null when it doesn't parse yet). */
data class Candidate(val text: String, val latex: String?, val isSystem: Boolean = false)

/** Pluggable recogniser: image -> text lines in engine syntax. The UI only depends on this interface. */
interface MathRecognizer : AutoCloseable {
    suspend fun lines(bitmap: Bitmap): List<String>
}

object Candidates {
    // words that belong in math input; anything else of 3+ letters is prose ("Solve the system of equations")
    private val mathWords = setOf(
        "sin", "cos", "tan", "sec", "csc", "cot", "sinh", "cosh", "tanh", "asin", "acos", "atan", "arcsin", "arccos", "arctan",
        "log", "ln", "exp", "sqrt", "cbrt", "abs", "lim", "diff", "int", "det", "inv", "gcd", "lcm", "mod", "max", "min", "pi",
        "theta", "alpha", "beta", "gamma", "lambda", "sigma", "mu", "phi", "omega", "inf",
        "factor", "expand", "simplify", "taylor", "shape", "slice", "sum", "eig", "rref", "rank", "trace", "identity",
        "mean", "median", "mode", "stdev", "variance", "stats", "fv", "pv", "pmt", "npv", "irr", "cagr", "bond", "sharpe",
        "normcdf", "bscall", "bsput", "bsmcall", "bsmput", "merton", "credit", "binomial", "ivol", "cont", "dsolve", "surface",
        "nCr", "nPr", "binom", "conj", "floor", "ceil", "round", "roots", "polar",
    )

    private val stopWords = setOf("the", "of", "and", "for", "to", "by", "is", "an", "at", "if", "or", "its", "find", "use")

    /** Removes prose words; returns "" when nothing mathematical is left. */
    fun stripProse(line: String): String {
        // prose = long words (4+ letters) or heading stopwords; short runs like "xyz" are products of variables
        val cleaned = Regex("[A-Za-z]{4,}|\\b[A-Za-z]{2,3}\\b").replace(line) { m ->
            val w = m.value.lowercase()
            when {
                w in mathWords -> m.value
                m.value.length >= 4 -> " "
                w in stopWords || (m.value.length >= 2 && m.value.all { it.isUpperCase() }) -> " "
                else -> m.value
            }
        }
            .replace(Regex("^[\\s:.,*·]+|[\\s:.,]+$"), "")
            .replace(Regex("\\s{2,}"), " ")
            .trim()
        return if (cleaned.any { it.isDigit() || it in "=+-*/^<>()[]" }) cleaned else ""
    }

    /**
     * Ranks what we read. Several lines that each contain '=' are also offered together as a
     * system of equations (joined with ';'), placed first when it parses.
     */
    fun from(rawLines: List<String>): List<Candidate> {
        val lines = rawLines.map { stripProse(OcrNormalizer.normalize(it)) }.filter { it.isNotBlank() && OcrNormalizer.mathScore(it) > 0 }
        val out = mutableListOf<Candidate>()
        val equations = lines.filter { it.contains('=') }
        // a line the model already read as a braced system ("a=b; c=d")
        lines.filter { it.contains(';') && it.contains('=') }.forEach {
            out += Candidate(it, runCatching { MathEngine.previewLatex(it) }.getOrNull(), isSystem = true)
        }
        if (equations.size >= 2) {
            // only lines that parse on their own join the system, so one misread line can't sink it
            val good = equations.filter { runCatching { MathEngine.previewLatex(it) }.getOrNull() != null }
            val sys = (if (good.size >= 2) good else equations).joinToString("; ")
            out += Candidate(sys, runCatching { MathEngine.previewLatex(sys) }.getOrNull(), isSystem = true)
        }
        // one expression split over two OCR lines: offer the lines joined, but never glue separate equations together
        val joined = lines.joinToString(" ")
        val relations = Regex("<=|>=|!=|[=<>≤≥]").findAll(joined).count()
        val offerJoined = lines.size > 1 && equations.size < 2 && relations <= 1
        val singles = (lines + if (offerJoined) listOf(joined) else emptyList())
            .distinct()
            .sortedByDescending { OcrNormalizer.mathScore(it) }
            .map { Candidate(it, runCatching { MathEngine.previewLatex(it) }.getOrNull()) }
        out += singles
        val unique = out.distinctBy { it.text }
        // once something parses, unparseable leftovers (stray words, symbols) are only noise
        val valid = unique.filter { it.latex != null }
        return (valid.ifEmpty { unique }).take(5)
    }
}
