// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.components

import androidx.compose.ui.graphics.asAndroidPath
import androidx.compose.ui.graphics.asComposePath
import com.mathlens.app.engine.Curve
import com.mathlens.app.engine.CurveType
import com.mathlens.app.engine.MathEngine
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/** A polyline in world coordinates: x0,y0,x1,y1,... */
typealias Polyline = FloatArray

/** Samples curves in world space. Uses the engine's compiled bytecode for speed. */
class CurveSampler(val curve: Curve) : AutoCloseable {
    val program: MathEngine.Program? = MathEngine.compile(curve.expr)
    private val constant: Double? = if (curve.type == CurveType.Vertical) program?.eval(0.0) else null

    override fun close() { program?.close() }

    fun sample(xmin: Double, xmax: Double, ymin: Double, ymax: Double, pxWidth: Int, pxHeight: Int, thetaTurns: Double = 2.0): List<Polyline> {
        val p = program ?: return emptyList()
        return when (curve.type) {
            CurveType.Explicit -> explicit(p, xmin, xmax, ymin, ymax, (pxWidth / 1.5).toInt().coerceIn(200, 1600))
            CurveType.Vertical -> constant?.takeIf { it.isFinite() }?.let {
                listOf(floatArrayOf(it.toFloat(), ymin.toFloat() - 1e3f, it.toFloat(), ymax.toFloat() + 1e3f))
            } ?: emptyList()
            CurveType.Polar -> polar(p, thetaTurns, 2400)
            CurveType.Implicit -> implicit(p, xmin, xmax, ymin, ymax, (pxWidth / 6).coerceIn(60, 220), (pxHeight / 6).coerceIn(60, 360))
            CurveType.Surface -> emptyList()
        }
    }

    private fun explicit(p: MathEngine.Program, xmin: Double, xmax: Double, ymin: Double, ymax: Double, n: Int): List<Polyline> {
        val xs = DoubleArray(n + 1) { xmin + (xmax - xmin) * it / n }
        val ys = DoubleArray(n + 1)
        p.evalMany(xs, null, ys)
        val span = ymax - ymin
        val out = mutableListOf<Polyline>()
        val cur = ArrayList<Float>()
        fun flush() {
            if (cur.size >= 4) out.add(cur.toFloatArray())
            cur.clear()
        }
        for (i in 0..n) {
            val y = ys[i]
            if (!y.isFinite()) { flush(); continue }
            if (i > 0 && ys[i - 1].isFinite()) {
                val prev = ys[i - 1]
                // asymptote: huge jump with sign change of the slope region
                if (abs(y - prev) > span * 4 && (y > ymax || prev > ymax || y < ymin || prev < ymin)) flush()
            }
            val yc = y.coerceIn(ymin - span * 10, ymax + span * 10)
            cur.add(xs[i].toFloat()); cur.add(yc.toFloat())
        }
        flush()
        return out
    }

    private fun polar(p: MathEngine.Program, turns: Double, n: Int): List<Polyline> {
        val th = DoubleArray(n + 1) { 2 * Math.PI * turns * it / n }
        val r = DoubleArray(n + 1)
        p.evalMany(th, null, r)
        val out = mutableListOf<Polyline>()
        val cur = ArrayList<Float>()
        for (i in 0..n) {
            if (!r[i].isFinite()) { if (cur.size >= 4) out.add(cur.toFloatArray()); cur.clear(); continue }
            cur.add((r[i] * cos(th[i])).toFloat()); cur.add((r[i] * sin(th[i])).toFloat())
        }
        if (cur.size >= 4) out.add(cur.toFloatArray())
        return out
    }

    /** Marching squares on F(x, y) = 0; returns short 2-point segments. */
    private fun implicit(p: MathEngine.Program, xmin: Double, xmax: Double, ymin: Double, ymax: Double, nx: Int, ny: Int): List<Polyline> {
        val w = nx + 1
        val h = ny + 1
        val xs = DoubleArray(w * h)
        val ys = DoubleArray(w * h)
        for (j in 0 until h) for (i in 0 until w) {
            xs[j * w + i] = xmin + (xmax - xmin) * i / nx
            ys[j * w + i] = ymin + (ymax - ymin) * j / ny
        }
        val v = DoubleArray(w * h)
        p.evalMany(xs, ys, v)
        val segs = mutableListOf<Polyline>()
        val dx = (xmax - xmin) / nx
        val dy = (ymax - ymin) / ny
        fun lerp(a: Double, b: Double) = if (a == b) 0.5 else (a / (a - b)).coerceIn(0.0, 1.0)
        for (j in 0 until ny) for (i in 0 until nx) {
            val a = v[j * w + i]; val b = v[j * w + i + 1]
            val c = v[(j + 1) * w + i + 1]; val d = v[(j + 1) * w + i]
            if (!a.isFinite() || !b.isFinite() || !c.isFinite() || !d.isFinite()) continue
            val x0 = xmin + i * dx; val y0 = ymin + j * dy
            val pts = ArrayList<Float>(8)
            // edges: bottom (a-b), right (b-c), top (d-c), left (a-d)
            if ((a < 0) != (b < 0)) { pts.add((x0 + lerp(a, b) * dx).toFloat()); pts.add(y0.toFloat()) }
            if ((b < 0) != (c < 0)) { pts.add((x0 + dx).toFloat()); pts.add((y0 + lerp(b, c) * dy).toFloat()) }
            if ((d < 0) != (c < 0)) { pts.add((x0 + lerp(d, c) * dx).toFloat()); pts.add((y0 + dy).toFloat()) }
            if ((a < 0) != (d < 0)) { pts.add(x0.toFloat()); pts.add((y0 + lerp(a, d) * dy).toFloat()) }
            // reject sign changes across poles (values blow up)
            val mx = maxOf(abs(a), abs(b), abs(c), abs(d))
            if (mx > 1e6) continue
            if (pts.size == 4) segs.add(pts.toFloatArray())
            else if (pts.size == 8) {
                segs.add(floatArrayOf(pts[0], pts[1], pts[2], pts[3]))
                segs.add(floatArrayOf(pts[4], pts[5], pts[6], pts[7]))
            }
        }
        return chainSegments(segs, minOf(dx, dy) * 1e-4)
    }
}

/**
 * Joins marching-squares segments that share endpoints into continuous polylines, so an implicit
 * curve can be drawn like a pen stroke (and strokes with round joins instead of thousands of caps).
 */
fun chainSegments(segs: List<Polyline>, eps: Double): List<Polyline> {
    if (segs.isEmpty()) return segs
    val q = if (eps > 0) 1.0 / eps else 1e6
    fun key(x: Float, y: Float): Long = (Math.round(x * q) shl 32) xor (Math.round(y * q) and 0xffffffffL)
    val at = HashMap<Long, MutableList<Int>>(segs.size * 2)
    segs.forEachIndexed { i, s ->
        at.getOrPut(key(s[0], s[1])) { mutableListOf() }.add(i)
        at.getOrPut(key(s[2], s[3])) { mutableListOf() }.add(i)
    }
    val used = BooleanArray(segs.size)
    // walks from (x, y) through unused segments, appending the far end of each
    fun walk(x0: Float, y0: Float, out: ArrayList<Float>) {
        var x = x0; var y = y0
        while (true) {
            val next = at[key(x, y)]?.firstOrNull { !used[it] } ?: return
            used[next] = true
            val s = segs[next]
            val forward = key(s[0], s[1]) == key(x, y)
            x = if (forward) s[2] else s[0]; y = if (forward) s[3] else s[1]
            out.add(x); out.add(y)
        }
    }
    val out = ArrayList<Polyline>()
    for (i in segs.indices) {
        if (used[i]) continue
        used[i] = true
        val s = segs[i]
        val fwd = ArrayList<Float>().apply { add(s[0]); add(s[1]); add(s[2]); add(s[3]) }
        walk(s[2], s[3], fwd)
        val back = ArrayList<Float>()
        walk(s[0], s[1], back)
        // back holds points leading away from the start: reverse them in pairs, then the forward run
        val line = FloatArray(back.size + fwd.size)
        var k = 0
        var j = back.size - 2
        while (j >= 0) { line[k++] = back[j]; line[k++] = back[j + 1]; j -= 2 }
        for (v in fwd) line[k++] = v
        out.add(line)
    }
    // left to right, so a multi-part curve is drawn in reading order
    return out.sortedBy { pl -> var m = Float.MAX_VALUE; for (t in 0 until pl.size / 2) m = minOf(m, pl[2 * t]); m }
}

/**
 * Pen-style reveal: returns the first [progress] of every contour of [path] (all contours advance
 * together), for curves that should be drawn rather than appear.
 */
fun partialPath(path: androidx.compose.ui.graphics.Path, progress: Float): androidx.compose.ui.graphics.Path {
    if (progress >= 1f) return path
    val out = android.graphics.Path()
    if (progress <= 0f) return androidx.compose.ui.graphics.Path()
    val pm = android.graphics.PathMeasure(path.asAndroidPath(), false)
    do {
        val len = pm.length
        if (len > 0f) pm.getSegment(0f, len * progress, out, true)
    } while (pm.nextContour())
    return out.asComposePath()
}

/** "Nice" grid step (1, 2, 5 × 10^k) for roughly [targetLines] lines across [span]. */
fun niceStep(span: Double, targetLines: Int = 8): Double {
    val raw = span / targetLines
    val mag = Math.pow(10.0, Math.floor(Math.log10(raw)))
    val n = raw / mag
    return mag * when {
        n < 1.5 -> 1.0
        n < 3.5 -> 2.0
        n < 7.5 -> 5.0
        else -> 10.0
    }
}

/** A point's coordinate for display: 3 (not 3.00), 2.5, 1.41; decimals only when the value has them. */
fun formatCoord(v: Double): String {
    if (!v.isFinite()) return "?"
    val r = Math.round(v).toDouble()
    if (abs(v - r) < 1e-9 * maxOf(1.0, abs(v))) return r.toLong().toString()
    val s = String.format(java.util.Locale.US, "%.3f", v).trimEnd('0').trimEnd('.')
    return if (s == "-0") "0" else s
}

fun formatTick(v: Double, step: Double): String {
    if (abs(v) < step * 1e-6) return "0"
    val decimals = (-Math.floor(Math.log10(step))).toInt().coerceIn(0, 6)
    return if (decimals == 0) Math.round(v).toString() else String.format("%.${decimals}f", v)
}
