// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.engine

import org.json.JSONArray
import org.json.JSONObject

data class Step(
    val title: String,
    val text: String,
    val latex: String,
    val sub: List<Step>,
)

data class Method(val name: String, val steps: List<Step>)

data class Fact(val label: String, val latex: String)

enum class CurveType { Explicit, Implicit, Polar, Vertical, Surface }

/**
 * [expr] is ready to compile (parameters already set to 1); [template] keeps the parameter names
 * (e.g. "a*sin(b*x)") so sliders can substitute other values.
 */
data class Curve(val type: CurveType, val expr: String, val label: String, val template: String = expr)

data class PlotPoint(val x: Double, val y: Double, val label: String)

/** Recognised geometry (see engine steps.h PlotShape for the parameter layout). */
data class PlotShape(val type: String, val params: List<Double>)

data class PlotSpec(
    val shape: PlotShape? = null,
    val curves: List<Curve>,
    val points: List<PlotPoint>,
    val shade: Pair<Double, Double>?,
    val view: DoubleArray,
    val autoRange: Boolean,
    val params: List<String> = emptyList(),
)

/** Replaces each whole-word parameter in [template] with its value (parameters are single letters). */
fun withParams(template: String, values: Map<String, Double>): String {
    if (values.isEmpty()) return template
    return Regex("(?<![A-Za-z_])([A-Za-z])(?![A-Za-z_(])").replace(template) { m ->
        values[m.value]?.let { v -> "(" + (if (v == Math.floor(v) && kotlin.math.abs(v) < 1e9) v.toLong().toString() else v.toString()) + ")" } ?: m.value
    }
}

data class Solution(
    val ok: Boolean,
    val error: String,
    val kind: String,
    val title: String,
    val input: String,
    val answer: String,
    val answers: List<String>,
    val facts: List<Fact>,
    val methods: List<Method>,
    val plot: PlotSpec?,
    val anim: String,
    val query: String = "",
    val story: List<String> = emptyList(),
)

object JsonParsing {
    private fun JSONArray?.objects(): List<JSONObject> =
        if (this == null) emptyList() else (0 until length()).map { getJSONObject(it) }

    private fun parseStep(o: JSONObject): Step = Step(
        title = o.optString("title"),
        text = o.optString("text"),
        latex = o.optString("latex"),
        sub = o.optJSONArray("sub").objects().map(::parseStep),
    )

    fun parsePlot(o: JSONObject): PlotSpec {
        val params = o.optJSONArray("params")?.let { a -> (0 until a.length()).map { a.optString(it) } } ?: emptyList()
        val defaults = params.associateWith { 1.0 }
        val curves = o.optJSONArray("curves").objects().map {
            val template = it.optString("expr")
            Curve(
                when (it.optString("type")) {
                    "implicit" -> CurveType.Implicit
                    "polar" -> CurveType.Polar
                    "vertical" -> CurveType.Vertical
                    "surface" -> CurveType.Surface
                    else -> CurveType.Explicit
                },
                withParams(template, defaults),
                it.optString("label"),
                template,
            )
        }
        val points = o.optJSONArray("points").objects().map {
            PlotPoint(it.optDouble("x"), it.optDouble("y"), it.optString("label"))
        }
        val shade = o.optJSONArray("shade")?.let { it.optDouble(0) to it.optDouble(1) }
        val v = o.optJSONArray("view")
        val view = DoubleArray(4) { i -> v?.optDouble(i) ?: doubleArrayOf(-10.0, 10.0, -7.0, 7.0)[i] }
        val shape = o.optJSONObject("shape")?.let { sh ->
            val ps = sh.optJSONArray("params")
            PlotShape(sh.optString("type"), (0 until (ps?.length() ?: 0)).map { ps!!.optDouble(it) })
        }
        return PlotSpec(shape, curves, points, shade, view, o.optBoolean("auto", true), params)
    }

    fun parseSolution(json: String, query: String): Solution {
        val o = runCatching { JSONObject(json) }.getOrElse {
            return Solution(false, "Engine returned invalid data", "", "", "", "", emptyList(), emptyList(), emptyList(), null, "", query)
        }
        return Solution(
            ok = o.optBoolean("ok"),
            error = o.optString("error"),
            kind = o.optString("kind"),
            title = o.optString("title"),
            input = o.optString("input"),
            answer = o.optString("answer"),
            answers = o.optJSONArray("answers")?.let { a -> (0 until a.length()).map { a.getString(it) } } ?: emptyList(),
            facts = o.optJSONArray("facts").objects().map { Fact(it.optString("label"), it.optString("latex")) },
            methods = o.optJSONArray("methods").objects().map { m ->
                Method(m.optString("name"), m.optJSONArray("steps").objects().map(::parseStep))
            },
            plot = o.optJSONObject("plot")?.let(::parsePlot),
            anim = o.optString("anim"),
            query = query,
            story = o.optJSONArray("story")?.let { a -> (0 until a.length()).map { a.getString(it) } } ?: emptyList(),
        )
    }
}
