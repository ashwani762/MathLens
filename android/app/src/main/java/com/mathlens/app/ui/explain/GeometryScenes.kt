// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.explain

import androidx.compose.foundation.layout.heightIn

import androidx.compose.animation.animateContentSize

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asAndroidPath
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.engine.CurveType
import com.mathlens.app.engine.PlotShape
import com.mathlens.app.engine.PlotSpec
import com.mathlens.app.ui.components.CurveSampler
import com.mathlens.app.ui.components.formatTick
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.Tone
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.cosh
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sinh
import kotlin.math.sqrt

/**
 * Geometry explanations: each conic is drawn from its *definition*, so the animation shows why
 * the curve has its shape (circle: constant distance; ellipse: constant sum; hyperbola: constant
 * difference; parabola: focus = directrix; line: constant slope).
 */

@Composable
private fun geoTime(period: Float): Float {
    val t = rememberInfiniteTransition(label = "geo")
    val v by t.animateFloat(0f, period, infiniteRepeatable(tween((period * 1000 * TEMPO).toInt(), easing = LinearEasing), RepeatMode.Restart), label = "geoT")
    return v
}

private fun ease(t: Float, start: Float, dur: Float) = ((t - start) / dur).coerceIn(0f, 1f).let { it * it * (3 - 2 * it) }

/** Equal-aspect world window around given bounds. */
private class Win(xmin: Double, xmax: Double, ymin: Double, ymax: Double, val w: Float, val h: Float) {
    val x0: Double; val x1: Double; val y0: Double; val y1: Double
    init {
        val cx = (xmin + xmax) / 2; val cy = (ymin + ymax) / 2
        val u = max((xmax - xmin) / w, (ymax - ymin) / h)
        x0 = cx - u * w / 2; x1 = cx + u * w / 2; y0 = cy - u * h / 2; y1 = cy + u * h / 2
    }
    fun p(x: Double, y: Double) = Offset(((x - x0) / (x1 - x0) * w).toFloat(), ((y1 - y) / (y1 - y0) * h).toFloat())
}

private val shadow = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
    style = android.graphics.Paint.Style.STROKE; strokeCap = android.graphics.Paint.Cap.ROUND
}

private fun DrawScope.ink(path: Path, color: Color = Tone.Ink, width: Float = 3f) {
    drawIntoCanvas { c ->
        shadow.strokeWidth = width * 1.6f
        shadow.color = android.graphics.Color.argb(44, 59, 42, 28)
        shadow.maskFilter = android.graphics.BlurMaskFilter(width * 2.4f, android.graphics.BlurMaskFilter.Blur.NORMAL)
        c.nativeCanvas.save(); c.nativeCanvas.translate(width, width * 2f)
        c.nativeCanvas.drawPath(path.asAndroidPath(), shadow); c.nativeCanvas.restore()
    }
    drawPath(path, color, style = Stroke(width, cap = StrokeCap.Round))
}

private fun DrawScope.seg(a: Offset, b: Offset, color: Color, width: Float = 2.4f, dashed: Boolean = false) =
    drawLine(color, a, b, width, cap = StrokeCap.Round, pathEffect = if (dashed) PathEffect.dashPathEffect(floatArrayOf(8f, 8f)) else null)

private fun DrawScope.dot(c: Offset, r: Float, color: Color) {
    drawCircle(Color(0x33_3B2A1C), r, c + Offset(r * 0.3f, r * 0.6f))
    drawCircle(color, r, c)
}

private fun DrawScope.axes(win: Win) {
    val c = Tone.Stone.copy(alpha = 0.5f)
    if (0.0 in win.y0..win.y1) seg(win.p(win.x0, 0.0), win.p(win.x1, 0.0), c, 1.1f)
    if (0.0 in win.x0..win.x1) seg(win.p(0.0, win.y0), win.p(0.0, win.y1), c, 1.1f)
}

/** Parametric curve path for t in [t0, t1] (only the first `progress` fraction). */
private fun paramPath(win: Win, t0: Double, t1: Double, progress: Float, n: Int = 240, f: (Double) -> Pair<Double, Double>): Path {
    val path = Path()
    val steps = max(2, (n * progress).toInt())
    for (i in 0..steps) {
        val t = t0 + (t1 - t0) * progress * i / steps
        val (x, y) = f(t)
        val o = win.p(x, y)
        if (i == 0) path.moveTo(o.x, o.y) else path.lineTo(o.x, o.y)
    }
    return path
}

@Composable
private fun GeoLayout(caption: String, readout: String?, modifier: Modifier, draw: DrawScope.() -> Unit) {
    Column(modifier.padding(horizontal = 18.dp, vertical = 16.dp)) {
        Box(Modifier.weight(1f).fillMaxWidth().clipToBounds()) {
            Canvas(Modifier.fillMaxSize()) { draw() }
            if (readout != null) Box(Modifier.align(Alignment.TopStart).padding(4.dp).labelBg()) { MathView(readout, fontSize = 16.sp, color = Tone.Terracotta) }
        }
        AnimatedContent(caption, transitionSpec = { fadeIn(tween(700, delayMillis = 550)) togetherWith fadeOut(tween(450)) }, label = "geoCaption",
            modifier = Modifier.padding(top = 10.dp).fillMaxWidth().heightIn(min = CaptionMinHeight).animateContentSize(tween(450)),
            contentAlignment = Alignment.Center) { t ->
            Box(Modifier.fillMaxWidth().heightIn(min = CaptionMinHeight), contentAlignment = Alignment.Center) { CaptionText(t) }
        }
    }
}

@Composable
fun ShapeScene(shape: PlotShape, modifier: Modifier) {
    val p = shape.params
    when (shape.type) {
        "circle" -> CircleScene(p[0], p[1], p[2], modifier)
        "ellipse" -> EllipseScene(p[0], p[1], p[2], p[3], modifier)
        "hyperbola" -> HyperbolaScene(p[0], p[1], p[2], p[3], p[4] > 0.5, modifier)
        "parabola" -> ParabolaScene(p[0], p[1], p[2], p[3] > 0.5, modifier)
        "line" -> LineScene(p[0], p[1], modifier)
        "roots" -> RootsScene(p, modifier)
        else -> Box(modifier)
    }
}

@Composable
private fun CircleScene(h: Double, k: Double, r: Double, modifier: Modifier) {
    val t = geoTime(10f)
    val trace = ease(t, 1.0f, 5.5f)
    val ang = 2 * PI * (if (t < 6.5f) trace.toDouble() else 1.0 + (t - 6.5) / 14.0)
    val px = h + r * cos(ang); val py = k + r * sin(ang)
    val dx = px - h; val dy = py - k
    val caption = when {
        t < 1.0f -> "Fix a centre"
        t < 4.0f -> "Keep every point exactly ${formatTick(r, 0.01)} away"
        t < 6.5f -> "Pythagoras: horizontal² + vertical² = radius²"
        else -> "All such points form a circle"
    }
    val readout = if (t > 1f) "${sq(dx)} + ${sq(dy)} = ${formatTick(dx * dx + dy * dy, 0.01)}" else null
    GeoLayout(storyAt(if (t < 1.0f) 0 else if (t < 6.5f) 1 else 2, caption), readout, modifier) {
        val win = Win(h - r * 1.5, h + r * 1.5, k - r * 1.5, k + r * 1.5, size.width, size.height)
        axes(win)
        val c = win.p(h, k)
        dot(c, 6f, Tone.Ink)
        ink(paramPath(win, 0.0, 2 * PI, if (t < 6.5f) trace else 1f) { a -> (h + r * cos(a)) to (k + r * sin(a)) }, Tone.Ink, 3f)
        if (t > 1f) {
            val pt = win.p(px, py)
            val corner = win.p(px, k)
            seg(c, corner, Tone.Terracotta, 2.6f)
            seg(corner, pt, Tone.Klein, 2.6f)
            seg(c, pt, Tone.Ink, 1.6f, dashed = true)
            dot(pt, 8f, Tone.Terracotta)
        }
    }
}

@Composable
private fun EllipseScene(h: Double, k: Double, a: Double, b: Double, modifier: Modifier) {
    val t = geoTime(10f)
    val tr = ease(t, 1.0f, 6f)
    val ang = 2 * PI * (if (t < 7f) tr.toDouble() else 1.0 + (t - 7) / 12.0)
    val horizontal = a >= b
    val c = sqrt(abs(a * a - b * b))
    val f1 = if (horizontal) (h - c) to k else h to (k - c)
    val f2 = if (horizontal) (h + c) to k else h to (k + c)
    val px = h + a * cos(ang); val py = k + b * sin(ang)
    val d1 = hypot(px - f1.first, py - f1.second); val d2 = hypot(px - f2.first, py - f2.second)
    val caption = when {
        t < 1.0f -> "Pin a string at two foci"
        t < 7f -> "Pull it taut and walk around"
        else -> "The two distances always add to ${formatTick(2 * max(a, b), 0.01)}"
    }
    GeoLayout(storyAt(if (t < 1.0f) 0 else if (t < 7f) 1 else 2, caption), if (t > 1f) "d_{1} + d_{2} = ${formatTick(d1, 0.01)} + ${formatTick(d2, 0.01)} = ${formatTick(d1 + d2, 0.01)}" else null, modifier) {
        val m = max(a, b) * 1.35
        val win = Win(h - m, h + m, k - m, k + m, size.width, size.height)
        axes(win)
        val F1 = win.p(f1.first, f1.second); val F2 = win.p(f2.first, f2.second)
        ink(paramPath(win, 0.0, 2 * PI, if (t < 7f) tr else 1f) { u -> (h + a * cos(u)) to (k + b * sin(u)) })
        if (t > 1f) {
            val pt = win.p(px, py)
            seg(F1, pt, Tone.Terracotta, 2.4f)
            seg(F2, pt, Tone.Klein, 2.4f)
            dot(pt, 8f, Tone.Ink)
        }
        dot(F1, 6f, Tone.Terracotta); dot(F2, 6f, Tone.Klein)
    }
}

@Composable
private fun HyperbolaScene(h: Double, k: Double, a: Double, b: Double, alongY: Boolean, modifier: Modifier) {
    val t = geoTime(11f)
    val c = sqrt(a * a + b * b)
    val branch = if (t < 5.5f) 1 else -1
    val u = -1.6 + 3.2 * ease(t % 5.5f, 0.8f, 4.2f)
    val f1 = if (!alongY) (h - c) to k else h to (k - c)
    val f2 = if (!alongY) (h + c) to k else h to (k + c)
    val (px, py) = if (!alongY) (h + branch * a * cosh(u)) to (k + b * sinh(u)) else (h + b * sinh(u)) to (k + branch * a * cosh(u))
    val d1 = hypot(px - f1.first, py - f1.second); val d2 = hypot(px - f2.first, py - f2.second)
    val caption = if (t % 5.5f < 0.8f) "Two foci, two branches" else "The difference of distances stays ${formatTick(2 * a, 0.01)}"
    GeoLayout(storyAt(if (t % 5.5f < 0.8f) 0 else 1, caption), "\\left|d_{1} - d_{2}\\right| = ${formatTick(abs(d1 - d2), 0.01)}", modifier) {
        val m = (c + a) * 1.5
        val win = Win(h - m, h + m, k - m, k + m, size.width, size.height)
        axes(win)
        // asymptotes
        val slope = if (!alongY) b / a else a / b
        seg(win.p(h - m, k - slope * m), win.p(h + m, k + slope * m), Tone.Hairline, 1.2f, dashed = true)
        seg(win.p(h - m, k + slope * m), win.p(h + m, k - slope * m), Tone.Hairline, 1.2f, dashed = true)
        for (br in listOf(1, -1)) {
            val prog = if (br == 1) (if (t < 5.5f) ease(t, 0.8f, 4.2f) else 1f) else (if (t >= 5.5f) ease(t - 5.5f, 0.8f, 4.2f) else 0f)
            if (prog > 0f) ink(paramPath(win, -1.6, 1.6, prog) { uu ->
                if (!alongY) (h + br * a * cosh(uu)) to (k + b * sinh(uu)) else (h + b * sinh(uu)) to (k + br * a * cosh(uu))
            })
        }
        val F1 = win.p(f1.first, f1.second); val F2 = win.p(f2.first, f2.second)
        val P = win.p(px, py)
        seg(F1, P, Tone.Terracotta, 2.2f); seg(F2, P, Tone.Klein, 2.2f)
        dot(P, 8f, Tone.Ink); dot(F1, 6f, Tone.Terracotta); dot(F2, 6f, Tone.Klein)
    }
}

@Composable
private fun ParabolaScene(vx: Double, vy: Double, p: Double, alongX: Boolean, modifier: Modifier) {
    val t = geoTime(10f)
    val span = max(3.0, abs(p) * 8)
    val s = -span / 2 + span * ease(t, 1.0f, 7f).toDouble().let { if (t > 8f) 1.0 - (t - 8f) / 2.0 else it }.coerceIn(0.0, 1.0)
    // point on the parabola at parameter s
    fun at(sv: Double) = if (!alongX) (vx + sv) to (vy + sv * sv / (4 * p)) else (vx + sv * sv / (4 * p)) to (vy + sv)
    val focus = if (!alongX) vx to (vy + p) else (vx + p) to vy
    val (px, py) = at(s)
    val dFocus = hypot(px - focus.first, py - focus.second)
    val dLine = if (!alongX) abs(py - (vy - p)) else abs(px - (vx - p))
    val caption = if (t < 1f) "A focus and a straight directrix" else "Every point is as far from the focus as from the line"
    GeoLayout(storyAt(if (t < 1f) 0 else 1, caption), if (t > 1f) "d_{focus} = ${formatTick(dFocus, 0.01)} = d_{line}" else null, modifier) {
        val m = span * 0.75
        val win = Win(vx - m, vx + m, vy - m * 0.6 + p, vy + m * 1.2, size.width, size.height)
        axes(win)
        val prog = ease(t, 0.8f, 2.4f)
        ink(paramPath(win, -span / 2, span / 2, prog) { sv -> at(sv) })
        // directrix
        if (!alongX) seg(win.p(win.x0, vy - p), win.p(win.x1, vy - p), Tone.Graphite, 1.6f, dashed = true)
        else seg(win.p(vx - p, win.y0), win.p(vx - p, win.y1), Tone.Graphite, 1.6f, dashed = true)
        val F = win.p(focus.first, focus.second)
        if (t > 1f) {
            val P = win.p(px, py)
            val foot = if (!alongX) win.p(px, vy - p) else win.p(vx - p, py)
            seg(P, F, Tone.Terracotta, 2.4f)
            seg(P, foot, Tone.Klein, 2.4f)
            dot(P, 8f, Tone.Ink)
        }
        dot(F, 6f, Tone.Terracotta)
    }
}

@Composable
private fun LineScene(m: Double, b: Double, modifier: Modifier) {
    val t = geoTime(8f)
    val x0 = -3.0 + 6.0 * ((t / 8f).toDouble())
    val caption = if (t < 1.5f) "A straight line keeps the same steepness" else "Step 1 right, rise ${formatTick(m, 0.01)}: that ratio is the slope"
    GeoLayout(storyAt(if (t < 1.5f) 0 else 1, caption), "\\frac{\\text{rise}}{\\text{run}} = ${formatTick(m, 0.01)}", modifier) {
        val win = Win(-5.0, 5.0, b - 5.0, b + 5.0, size.width, size.height)
        axes(win)
        ink(paramPath(win, -6.0, 6.0, ease(t, 0f, 1.4f)) { x -> x to (m * x + b) })
        dot(win.p(0.0, b), 6f, Tone.Ochre)
        if (t > 1.5f) {
            val a = win.p(x0, m * x0 + b); val c = win.p(x0 + 1, m * x0 + b); val d = win.p(x0 + 1, m * (x0 + 1) + b)
            seg(a, c, Tone.Terracotta, 2.6f); seg(c, d, Tone.Klein, 2.6f)
            dot(a, 7f, Tone.Ink)
        }
    }
}

/** Any other implicit or polar curve: trace it as the pen would draw it. */
@Composable
fun TraceScene(plot: PlotSpec, modifier: Modifier) {
    val samplers = remember(plot) { plot.curves.filter { it.type == CurveType.Implicit || it.type == CurveType.Polar }.map { CurveSampler(it) } }
    DisposableEffect(samplers) { onDispose { samplers.forEach { it.close() } } }
    val t = geoTime(8f)
    val prog = ease(t, 0.3f, 5.5f)
    val polar = samplers.any { it.curve.type == CurveType.Polar }
    val caption = if (polar) "The radius changes as the angle turns" else "Every point (x, y) that makes the equation true"
    // polar curves: frame the curve itself (the default view is often far larger than a small rose)
    val polarBox = remember(samplers) {
        if (!polar) null else {
            val v = plot.view
            var r = 0.0
            samplers.filter { it.curve.type == CurveType.Polar }.forEach { s ->
                s.sample(v[0], v[1], v[2], v[3], 600, 600).forEach { pl ->
                    for (j in 0 until pl.size / 2) { val x = pl[2 * j].toDouble(); val y = pl[2 * j + 1].toDouble(); if (x.isFinite() && y.isFinite()) r = max(r, max(abs(x), abs(y))) }
                }
            }
            if (r > 0) r * 1.15 else null
        }
    }
    GeoLayout(storyAt(if (t < 3f) 0 else 1, caption), null, modifier) {
        val v = plot.view
        val win = polarBox?.let { Win(-it, it, -it, it, size.width, size.height) } ?: Win(v[0], v[1], v[2], v[3], size.width, size.height)
        axes(win)
        samplers.forEachIndexed { i, s ->
            val lines = s.sample(win.x0, win.x1, win.y0, win.y1, size.width.toInt(), size.height.toInt())
            val color = if (i == 0) Tone.Ink else Tone.Terracotta
            // polar: a radius arm sweeps the angle; implicit: the chained curve is drawn like a pen
            val path = Path()
            lines.forEach { pl ->
                path.moveTo(win.p(pl[0].toDouble(), pl[1].toDouble()).x, win.p(pl[0].toDouble(), pl[1].toDouble()).y)
                for (j in 1 until pl.size / 2) { val o = win.p(pl[2 * j].toDouble(), pl[2 * j + 1].toDouble()); path.lineTo(o.x, o.y) }
            }
            if (s.curve.type == CurveType.Polar && prog > 0f && prog < 1f) {
                // the radius arm points at the pen tip (same arc-length measure as the reveal)
                val pm = android.graphics.PathMeasure(path.asAndroidPath(), false)
                val pos = FloatArray(2)
                if (pm.length > 0f && pm.getPosTan(pm.length * prog, pos, null)) seg(win.p(0.0, 0.0), Offset(pos[0], pos[1]), Tone.Terracotta, 1.6f)
            }
            ink(com.mathlens.app.ui.components.partialPath(path, prog), color, 2.6f)
        }
    }
}

private fun sq(v: Double) = "(" + formatTick(v, 0.01) + ")^{2}"



/** Surfaces: a ball rolls against the gradient (gradient descent) and settles where the surface is flat. */
@Composable
fun SurfaceScene(plot: PlotSpec, modifier: Modifier) {
    val expr = plot.curves.first { it.type == CurveType.Surface }.expr
    val mesh = remember(expr) { com.mathlens.app.ui.components.SurfaceMesh(expr) }
    DisposableEffect(mesh) { onDispose { mesh.close() } }
    val t = geoTime(12f)
    // precompute a descent path from a corner
    val path = remember(mesh) {
        val pts = ArrayList<Pair<Float, Float>>()
        var x = 2.3; var y = 1.9
        val h = 1e-3
        repeat(260) {
            pts.add(x.toFloat() to y.toFloat())
            val gx = (mesh.eval(x + h, y) - mesh.eval(x - h, y)) / (2 * h)
            val gy = (mesh.eval(x, y + h) - mesh.eval(x, y - h)) / (2 * h)
            if (!gx.isFinite() || !gy.isFinite()) return@repeat
            val g = sqrt(gx * gx + gy * gy)
            val step = min(0.06, 0.04 * g + 0.004)
            if (g > 1e-9) { x -= step * gx / g; y -= step * gy / g }
            x = x.coerceIn(-2.95, 2.95); y = y.coerceIn(-2.95, 2.95)
        }
        pts
    }
    val k = ((ease(t, 1.5f, 8f)) * (path.size - 1)).toInt()
    val caption = when {
        t < 1.5f -> "A height above every point (x, y)"
        t < 7f -> "A ball rolls against the gradient ∇z"
        else -> "Flat spots, where ∇z = 0, are the critical points"
    }
    val (bx, by) = path[k.coerceIn(0, path.size - 1)]
    val bz = mesh.eval(bx.toDouble(), by.toDouble())
    GeoLayout(storyAt(if (t < 1.5f) 0 else if (t < 7f) 1 else 2, caption), "z(" + formatTick(bx.toDouble(), 0.01) + ", " + formatTick(by.toDouble(), 0.01) + ") = " + formatTick(bz, 0.01), modifier) {
        val cam = com.mathlens.app.ui.components.Camera3D(0.7f + t * 0.05f, 0.6f)
        val markers = plot.points.map { Triple(it.x.toFloat(), it.y.toFloat(), Tone.Ink) } +
            listOf(Triple(bx, by, Tone.Terracotta))
        with(com.mathlens.app.ui.components.SurfaceMeshDraw) { drawIt(mesh, cam, markers) }
    }
}


/** Complex solutions on the Argand plane: real part across, imaginary part up. */
@Composable
private fun RootsScene(flat: List<Double>, modifier: Modifier) {
    val roots = flat.chunked(2).filter { it.size == 2 }.map { it[0] to it[1] }
    val t = geoTime(9f)
    val r = max(1.0, roots.maxOf { max(abs(it.first), abs(it.second)) }) * 1.6
    val onCircle = roots.size >= 3 && roots.map { hypot(it.first, it.second) }.let { m -> m.max() - m.min() < 1e-6 }
    GeoLayout(storyAt(if (t < 2f) 0 else if (t < 6f) 1 else 2, "Complex solutions on the plane"), null, modifier) {
        val win = Win(-r, r, -r, r, size.width, size.height)
        axes(win)
        val o = win.p(0.0, 0.0)
        if (onCircle) {
            val rad = hypot(roots[0].first, roots[0].second)
            ink(paramPath(win, 0.0, 2 * PI, ease(t, 0.3f, 1.6f)) { a -> rad * cos(a) to rad * sin(a) }, Tone.Hairline, 1.4f)
        }
        roots.forEachIndexed { i, (re, im) ->
            val a = ease(t, 2f + i * 0.6f, 0.8f)
            if (a <= 0f) return@forEachIndexed
            val pt = win.p(re * a, im * a)
            seg(o, win.p(re * a, 0.0), Tone.Terracotta, 2f)
            seg(win.p(re * a, 0.0), pt, Tone.Klein, 2f)
            dot(pt, 8f, Tone.Ink)
        }
        if (onCircle && t > 2f + roots.size * 0.6f) {
            // the roots of z^n = w form a regular polygon
            val path = Path()
            roots.sortedBy { atan2(it.second, it.first) }.forEachIndexed { i, (re, im) -> val q = win.p(re, im); if (i == 0) path.moveTo(q.x, q.y) else path.lineTo(q.x, q.y) }
            path.close()
            drawPath(path, Tone.Terracotta.copy(alpha = 0.12f))
            drawPath(path, Tone.Terracotta, style = Stroke(1.6f))
        }
    }
}
