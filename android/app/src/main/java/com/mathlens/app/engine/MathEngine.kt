// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.engine

import org.json.JSONObject

/**
 * Kotlin facade over the C++ engine (see mathengine/ and src/main/cpp/jni_bridge.cpp).
 * All calls are thread-safe; the engine keeps no global mutable state.
 */
object MathEngine {
    init {
        System.loadLibrary("mathlens")
    }

    private external fun nativeSolve(input: String): String
    private external fun nativeLatex(input: String): String
    private external fun nativeGraph(input: String): String
    private external fun nativeFromLatex(latex: String): String
    private external fun nativeCompile(expr: String): Long
    private external fun nativeRelease(handle: Long)
    private external fun nativeUsesY(handle: Long): Boolean
    private external fun nativeEvalMany(handle: Long, xs: DoubleArray, ys: DoubleArray?, out: DoubleArray)
    private external fun nativeEval(handle: Long, x: Double, y: Double, t: Double): Double

    fun solve(input: String): Solution = JsonParsing.parseSolution(nativeSolve(input), input)

    /** LaTeX preview of what the user typed, or null while the input is incomplete. */
    fun previewLatex(input: String): String? {
        if (input.isBlank()) return null
        val o = JSONObject(nativeLatex(input))
        return if (o.optBoolean("ok")) o.optString("latex") else null
    }

    /** LaTeX (e.g. from the recognition model) -> engine syntax. */
    fun fromLatex(latex: String): String = nativeFromLatex(latex)

    fun graph(input: String): Result<PlotSpec> {
        val o = JSONObject(nativeGraph(input))
        return if (o.optBoolean("ok")) Result.success(JsonParsing.parsePlot(o))
        else Result.failure(IllegalArgumentException(o.optString("error", "Can't graph this")))
    }

    /** Compiled expression for fast repeated evaluation (graphs, wallpapers). Call [Program.close]. */
    class Program internal constructor(private var handle: Long) : AutoCloseable {
        val usesY: Boolean = handle != 0L && nativeUsesY(handle)
        fun eval(x: Double, y: Double = 0.0, t: Double = x): Double = if (handle == 0L) Double.NaN else nativeEval(handle, x, y, t)
        fun evalMany(xs: DoubleArray, ys: DoubleArray?, out: DoubleArray) {
            if (handle != 0L) nativeEvalMany(handle, xs, ys, out) else out.fill(Double.NaN)
        }
        override fun close() {
            if (handle != 0L) nativeRelease(handle)
            handle = 0L
        }
    }

    fun compile(expr: String): Program? {
        val h = nativeCompile(expr)
        return if (h == 0L) null else Program(h)
    }
}
