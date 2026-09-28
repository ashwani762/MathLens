// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.components

import androidx.compose.animation.core.LinearEasing

import androidx.compose.ui.graphics.asAndroidPath

import androidx.compose.ui.layout.onSizeChanged

import android.graphics.Paint
import android.graphics.Typeface
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.mathlens.app.engine.CurveType
import com.mathlens.app.engine.PlotSpec
import com.mathlens.app.ui.theme.Tone
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/** Viewport in world coordinates. */
data class Viewport(val cx: Double, val cy: Double, val unitsPerPxX: Double, val unitsPerPxY: Double)

fun needsEqualAspect(spec: PlotSpec) = !spec.autoRange || spec.curves.any { it.type == CurveType.Implicit || it.type == CurveType.Polar }

fun initialViewport(spec: PlotSpec, w: Float, h: Float): Viewport {
    val (x0, x1, y0, y1) = spec.view.toList()
    var ux = (x1 - x0) / w
    var uy = (y1 - y0) / h
    if (needsEqualAspect(spec)) { val u = max(ux, uy); ux = u; uy = u }
    // polar curves: fit the curve extent
    return Viewport((x0 + x1) / 2, (y0 + y1) / 2, ux, uy)
}

@Composable
fun GraphView(
    spec: PlotSpec,
    modifier: Modifier = Modifier,
    interactive: Boolean = true,
    animateIn: Boolean = true,
    showGrid: Boolean = true,
    lineWidth: Float = 2.6f,
    colors: List<Color> = Tone.curves,
    background: Color = Color.Transparent,
    thetaTurns: Double = 2.0,
    paramValues: Map<String, Double> = emptyMap(),
    showMarkers: Boolean = false,
) {
    spec.curves.firstOrNull { it.type == CurveType.Surface }?.let { surf ->
        Surface3D(surf.expr, spec.points, modifier)
        return
    }
    // parameters (sliders) are substituted into each curve's template before compiling
    val samplers = remember(spec, paramValues) {
        spec.curves.map { c -> CurveSampler(if (paramValues.isEmpty()) c else c.copy(expr = com.mathlens.app.engine.withParams(c.template, paramValues))) }
    }
    DisposableEffect(samplers) { onDispose { samplers.forEach { it.close() } } }
    var viewport by remember(spec) { mutableStateOf<Viewport?>(null) }
    var trace by remember { mutableStateOf<Offset?>(null) }
    // the pen draws once per equation; moving a slider redraws instantly
    val reveal = remember(spec) { Animatable(if (animateIn) 0f else 1f) }
    val pointsIn = remember(spec) { Animatable(if (animateIn) 0f else 1f) }
    // the pen moves at a constant speed along what is visible: duration follows the curves' on-screen length
    var canvasSize by remember(spec) { mutableStateOf(androidx.compose.ui.unit.IntSize.Zero) }
    val density = androidx.compose.ui.platform.LocalDensity.current
    val visibleLengths = remember(spec, canvasSize) {
        if (canvasSize.width == 0) null else {
            val w = canvasSize.width.toFloat(); val h = canvasSize.height.toFloat()
            val v = initialViewport(spec, w, h)
            val x0 = v.cx - w / 2 * v.unitsPerPxX; val x1 = v.cx + w / 2 * v.unitsPerPxX
            val y0 = v.cy - h / 2 * v.unitsPerPxY; val y1 = v.cy + h / 2 * v.unitsPerPxY
            samplers.map { sm ->
                val path = visiblePath(sm.sample(x0, x1, y0, y1, w.toInt(), h.toInt(), thetaTurns), { ((it - x0) / v.unitsPerPxX).toFloat() }, { ((y1 - it) / v.unitsPerPxY).toFloat() }, h)
                pathLength(path)
            }
        }
    }
    LaunchedEffect(spec, visibleLengths) {
        if (!animateIn || visibleLengths == null || reveal.value >= 1f) return@LaunchedEffect
        val total = visibleLengths.sum()
        val speed = with(density) { 180.dp.toPx() }  // pixels per second: slow enough to follow the pen
        val ms = (total / speed * 1000f).toInt().coerceIn(2200, 7000) + 250 * (visibleLengths.size - 1)
        reveal.animateTo(1f, tween(ms, easing = PenEasing))
        // then the points and their labels fade in, one after another
        pointsIn.animateTo(1f, tween(400 + 250 * (spec.points.size + 2).coerceAtMost(8), easing = LinearEasing))
    }
    val labelPaint = remember {
        Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.create("sans-serif", Typeface.NORMAL) }
    }

    var gestures = Modifier.pointerInput(spec) {}
    if (interactive) {
        gestures = Modifier
            .pointerInput(spec) {
                detectTransformGestures { centroid, pan, zoom, _ ->
                    val v = viewport ?: return@detectTransformGestures
                    val z = zoom.toDouble().coerceIn(0.5, 2.0)
                    // keep the world point under the centroid fixed while zooming
                    val wx = v.cx + (centroid.x - size.width / 2) * v.unitsPerPxX
                    val wy = v.cy - (centroid.y - size.height / 2) * v.unitsPerPxY
                    val ux = (v.unitsPerPxX / z).coerceIn(1e-7, 1e5)
                    val uy = (v.unitsPerPxY / z).coerceIn(1e-7, 1e5)
                    val ncx = wx - (centroid.x - size.width / 2) * ux - pan.x * ux
                    val ncy = wy + (centroid.y - size.height / 2) * uy + pan.y * uy
                    viewport = Viewport(ncx, ncy, ux, uy)
                }
            }
            .pointerInput(spec) {
                detectTapGestures(onDoubleTap = { viewport = null; trace = null }, onTap = { trace = null })
            }
            .pointerInput(spec) {
                detectDragGesturesAfterLongPress(
                    onDragStart = { trace = it },
                    onDrag = { change, _ -> trace = change.position },
                    onDragEnd = {},
                )
            }
    }

    Canvas(modifier.then(gestures).onSizeChanged { canvasSize = it }) {
        if (background != Color.Transparent) drawRect(background)
        val v = viewport ?: initialViewport(spec, size.width, size.height).also { viewport = it }
        val xmin = v.cx - size.width / 2 * v.unitsPerPxX
        val xmax = v.cx + size.width / 2 * v.unitsPerPxX
        val ymin = v.cy - size.height / 2 * v.unitsPerPxY
        val ymax = v.cy + size.height / 2 * v.unitsPerPxY
        fun sx(x: Double) = ((x - xmin) / v.unitsPerPxX).toFloat()
        fun sy(y: Double) = ((ymax - y) / v.unitsPerPxY).toFloat()

        if (showGrid) drawGrid(xmin, xmax, ymin, ymax, ::sx, ::sy, labelPaint)

        // shaded area (definite integrals)
        spec.shade?.let { (a, b) ->
            val s = samplers.firstOrNull { it.curve.type == CurveType.Explicit } ?: return@let
            val p = s.program ?: return@let
            val path = Path()
            val n = 200
            path.moveTo(sx(a), sy(0.0))
            for (i in 0..n) {
                val x = a + (b - a) * i / n
                val y = p.eval(x)
                if (y.isFinite()) path.lineTo(sx(x), sy(y.coerceIn(ymin - 1e3, ymax + 1e3)))
            }
            path.lineTo(sx(b), sy(0.0))
            path.close()
            drawPath(path, Tone.Terracotta.copy(alpha = 0.22f), alpha = reveal.value)
        }

        // curves are drawn like a pen, one after another, at one speed: each gets a share of the reveal
        // proportional to its visible length (a wipe would make vertical lines and closed curves pop in)
        val lens = visibleLengths ?: List(samplers.size) { 1f }
        val totalLen = lens.sum().coerceAtLeast(1f)
        var start = 0f
        samplers.forEachIndexed { idx, s ->
            val color = colors[idx % colors.size]
            val share = lens.getOrElse(idx) { 1f } / totalLen
            val prog = if (share <= 0f) 1f else ((reveal.value - start) / share).coerceIn(0f, 1f)
            start += share
            if (prog <= 0f) return@forEachIndexed
            val lines = s.sample(xmin, xmax, ymin, ymax, size.width.toInt(), size.height.toInt(), thetaTurns)
            val path = visiblePath(lines, { sx(it) }, { sy(it) }, size.height)
            drawPath(partialPath(path, prog), color, style = Stroke(width = lineWidth, cap = StrokeCap.Round, join = StrokeJoin.Round))
        }

        // roots, turning points and intersections of the visible explicit curves
        if (showMarkers && reveal.value >= 1f && pointsIn.value > 0f) {
            val ma = pointsIn.value
            // nearest the middle of the view first, so a crowded (periodic) curve is marked where you look
            val cx = (xmin + xmax) / 2; val cy = (ymin + ymax) / 2
            val marks = findMarkers(samplers, xmin, xmax, ymin, ymax).sortedBy { (x, y) -> (x - cx) * (x - cx) / ((xmax - xmin) * (xmax - xmin)) + (y - cy) * (y - cy) / ((ymax - ymin) * (ymax - ymin)) }
            val known = spec.points.filter { it.x.isFinite() && it.y.isFinite() }.map { Offset(sx(it.x), sy(it.y)) }
            val placed = ArrayList<Offset>()
            val labelBoxes = ArrayList<androidx.compose.ui.geometry.Rect>()
            // only a handful of labelled key points help; a crowded (periodic) curve gets none
            if (marks.size <= 6) for ((mx, my) in marks) {
                val c = Offset(sx(mx), sy(my))
                if (known.any { (it - c).getDistance() < 16f } || placed.any { (it - c).getDistance() < 16f }) continue
                placed += c
                if (placed.size > 10) break
                // a small solid dot with a paper ring, so it sits cleanly on the line
                drawCircle(Tone.Ivory.copy(alpha = ma), radius = 5.dp.toPx(), center = c)
                drawCircle(Tone.Ink.copy(alpha = ma), radius = 3.5.dp.toPx(), center = c)
                run {
                    labelPaint.color = Tone.Graphite.copy(alpha = ma).toArgb()
                    labelPaint.textSize = 11.dp.toPx()
                    val text = ("(" + formatCoord(mx) + ", " + formatCoord(my) + ")").replace('-', '\u2212')
                    val w = labelPaint.measureText(text); val h = 11.dp.toPx(); val g = 7.dp.toPx()
                    // try below-right, above-right, below-left, above-left; skip the label if all collide
                    val spots = listOf(Offset(c.x + g, c.y + g + h), Offset(c.x + g, c.y - g), Offset(c.x - g - w, c.y + g + h), Offset(c.x - g - w, c.y - g))
                    val spot = spots.firstOrNull { o ->
                        val r = androidx.compose.ui.geometry.Rect(o.x - 2, o.y - h - 2, o.x + w + 2, o.y + 4)
                        r.left >= 0 && r.right <= size.width && r.top >= 0 && r.bottom <= size.height && labelBoxes.none { it.overlaps(r) } &&
                            placed.none { p -> r.inflate(4f).contains(p) }
                    }
                    if (spot != null) {
                        labelBoxes += androidx.compose.ui.geometry.Rect(spot.x - 2, spot.y - h - 2, spot.x + w + 2, spot.y + 4)
                        labelBackdrop(text, spot.x, spot.y, labelPaint, ma)
                        drawContext.canvas.nativeCanvas.drawText(text, spot.x, spot.y, labelPaint)
                    }
                }
            }
        }

        // points of interest: each fades in after the pen, one after another
        if (reveal.value >= 1f) {
            val np = spec.points.size.coerceAtLeast(1)
            spec.points.forEachIndexed { pi, pt ->
                if (!pt.x.isFinite() || !pt.y.isFinite()) return@forEachIndexed
                val a = ((pointsIn.value * (np + 2) - pi) / 3f).coerceIn(0f, 1f)
                if (a <= 0f) return@forEachIndexed
                val c = Offset(sx(pt.x), sy(pt.y))
                val grow = 0.6f + 0.4f * a
                drawCircle(Tone.Ivory.copy(alpha = a), radius = 5.5.dp.toPx() * grow, center = c)
                drawCircle(Tone.Terracotta.copy(alpha = a), radius = 4.dp.toPx() * grow, center = c)
                labelPaint.color = Tone.Ink.copy(alpha = a).toArgb()
                labelPaint.textSize = 11.dp.toPx()
                val label = ("(" + formatCoord(pt.x) + ", " + formatCoord(pt.y) + ")").replace('-', '\u2212')
                labelBackdrop(label, c.x + 9.dp.toPx(), c.y - 9.dp.toPx(), labelPaint, a)
                drawContext.canvas.nativeCanvas.drawText(label, c.x + 9.dp.toPx(), c.y - 9.dp.toPx(), labelPaint)
            }
        }

        // trace crosshair on long-press
        trace?.let { t ->
            val x = xmin + t.x * v.unitsPerPxX
            drawLine(Tone.Stone, Offset(t.x, 0f), Offset(t.x, size.height), strokeWidth = 1.dp.toPx())
            samplers.forEachIndexed { idx, s ->
                if (s.curve.type != CurveType.Explicit) return@forEachIndexed
                val y = s.program?.eval(x) ?: return@forEachIndexed
                if (!y.isFinite()) return@forEachIndexed
                val c = Offset(t.x, sy(y))
                val color = colors[idx % colors.size]
                drawCircle(color, radius = 6.dp.toPx(), center = c)
                labelPaint.color = color.toArgb()
                labelPaint.textSize = 12.dp.toPx()
                val txt = "x = ${formatTick(x, 0.001)}   y = ${formatTick(y, 0.001)}"
                drawContext.canvas.nativeCanvas.drawText(txt, min(t.x + 10.dp.toPx(), size.width - labelPaint.measureText(txt) - 8), c.y - 12.dp.toPx(), labelPaint)
            }
        }
    }
}

private fun DrawScope.drawGrid(
    xmin: Double, xmax: Double, ymin: Double, ymax: Double,
    sx: (Double) -> Float, sy: (Double) -> Float, paint: Paint,
) {
    val stepX = niceStep(xmax - xmin, max(4, (size.width / 110).toInt()))
    val stepY = niceStep(ymax - ymin, max(4, (size.height / 110).toInt()))
    val minor = Tone.Linen.copy(alpha = 0.45f)
    val major = Tone.Linen
    // minor lines
    var x = floor(xmin / (stepX / 5)) * (stepX / 5)
    while (x <= xmax) { drawLine(minor, Offset(sx(x), 0f), Offset(sx(x), size.height), 1f); x += stepX / 5 }
    var y = floor(ymin / (stepY / 5)) * (stepY / 5)
    while (y <= ymax) { drawLine(minor, Offset(0f, sy(y)), Offset(size.width, sy(y)), 1f); y += stepY / 5 }
    // major lines + labels
    paint.color = Tone.Stone.toArgb()
    paint.textSize = 10.dp.toPx()
    val ax = sy(0.0).coerceIn(0f, size.height)
    val ay = sx(0.0).coerceIn(0f, size.width)
    x = ceil(xmin / stepX) * stepX
    while (x <= xmax) {
        drawLine(major, Offset(sx(x), 0f), Offset(sx(x), size.height), 1f)
        if (abs(x) > stepX * 1e-6) {
            val t = formatTick(x, stepX)
            drawContext.canvas.nativeCanvas.drawText(t, sx(x) - paint.measureText(t) / 2, (ax + 14.dp.toPx()).coerceAtMost(size.height - 4), paint)
        }
        x += stepX
    }
    y = ceil(ymin / stepY) * stepY
    while (y <= ymax) {
        drawLine(major, Offset(0f, sy(y)), Offset(size.width, sy(y)), 1f)
        if (abs(y) > stepY * 1e-6) {
            val t = formatTick(y, stepY)
            drawContext.canvas.nativeCanvas.drawText(t, (ay + 5.dp.toPx()).coerceAtMost(size.width - paint.measureText(t) - 4), sy(y) + 4.dp.toPx(), paint)
        }
        y += stepY
    }
    // axes
    val axis = Tone.Stone
    if (0.0 in ymin..ymax) drawLine(axis, Offset(0f, sy(0.0)), Offset(size.width, sy(0.0)), 1.5f)
    if (0.0 in xmin..xmax) drawLine(axis, Offset(sx(0.0), 0f), Offset(sx(0.0), size.height), 1.5f)
}


/** A soft translucent paper chip behind a label, so it reads over grid lines and curves. */
private fun androidx.compose.ui.graphics.drawscope.DrawScope.labelBackdrop(text: String, x: Float, baseline: Float, paint: android.graphics.Paint, alpha: Float) {
    val w = paint.measureText(text)
    val padX = 4.dp.toPx(); val padY = 2.5.dp.toPx()
    val top = baseline - paint.textSize * 0.95f - padY
    val bottom = baseline + paint.textSize * 0.28f + padY
    drawRoundRect(
        Tone.Ivory.copy(alpha = 0.86f * alpha),
        topLeft = Offset(x - padX, top),
        size = androidx.compose.ui.geometry.Size(w + 2 * padX, bottom - top),
        cornerRadius = androidx.compose.ui.geometry.CornerRadius(4.dp.toPx()),
    )
}

/**
 * Screen path of the polylines, clipped to the canvas height (with a small margin): parts far above or below
 * the view would otherwise count toward the pen's length and make the visible part race.
 */
internal fun visiblePath(lines: List<Polyline>, sx: (Double) -> Float, sy: (Double) -> Float, height: Float): Path {
    val top = -height * 0.05f; val bottom = height * 1.05f
    val path = Path()
    for (pl in lines) {
        var penDown = false
        var px = 0f; var py = 0f
        for (j in 0 until pl.size / 2) {
            val x = sx(pl[2 * j].toDouble()); val y = sy(pl[2 * j + 1].toDouble())
            val inside = y in top..bottom
            if (j > 0) {
                val prevInside = py in top..bottom
                when {
                    inside && prevInside -> path.lineTo(x, y)
                    inside && !prevInside -> {  // entering: start at the boundary crossing
                        val by = if (py < top) top else bottom
                        val t = (by - py) / (y - py)
                        path.moveTo(px + (x - px) * t, by); path.lineTo(x, y)
                    }
                    !inside && prevInside -> {  // leaving: end at the boundary crossing
                        val by = if (y < top) top else bottom
                        val t = (by - py) / (y - py)
                        path.lineTo(px + (x - px) * t, by)
                    }
                    else -> {}
                }
            } else if (inside) path.moveTo(x, y)
            penDown = inside
            px = x; py = y
        }
    }
    return path
}

internal fun pathLength(path: Path): Float {
    val pm = android.graphics.PathMeasure(path.asAndroidPath(), false)
    var total = 0f
    do { total += pm.length } while (pm.nextContour())
    return total
}

/** An even pen: gentle start and finish, steady in the middle (FastOutSlowIn drew half the curve in the first beat). */
private val PenEasing = androidx.compose.animation.core.CubicBezierEasing(0.33f, 0.12f, 0.67f, 0.88f)

/**
 * Roots (sign changes, refined by bisection), turning points (slope sign changes, refined with a
 * parabola through three samples) and pairwise intersections of the explicit curves in view.
 */
internal fun findMarkers(samplers: List<CurveSampler>, xmin: Double, xmax: Double, ymin: Double, ymax: Double): List<Pair<Double, Double>> {
    val progs = samplers.filter { it.curve.type == com.mathlens.app.engine.CurveType.Explicit }.mapNotNull { it.program }
    if (progs.isEmpty()) return emptyList()
    val n = 480
    val xs = DoubleArray(n + 1) { xmin + (xmax - xmin) * it / n }
    val ys = progs.map { p -> DoubleArray(n + 1).also { p.evalMany(xs, null, it) } }
    val span = ymax - ymin
    val out = ArrayList<Pair<Double, Double>>()
    fun bisect(f: (Double) -> Double, a0: Double, b0: Double): Double {
        var a = a0; var b = b0; var fa = f(a)
        repeat(40) {
            val m = (a + b) / 2; val fm = f(m)
            if (!fm.isFinite()) return m
            if ((fa < 0) == (fm < 0)) { a = m; fa = fm } else b = m
        }
        return (a + b) / 2
    }
    fun ok(v: Double) = v.isFinite() && kotlin.math.abs(v) < span * 3 + kotlin.math.abs(ymax) + kotlin.math.abs(ymin)
    progs.forEachIndexed { ci, p ->
        val y = ys[ci]
        for (i in 0 until n) {
            val a = y[i]; val b = y[i + 1]
            if (!ok(a) || !ok(b)) continue
            if (a == 0.0) { out += xs[i] to 0.0; continue }
            if ((a < 0) != (b < 0) && kotlin.math.abs(a - b) < span) {  // a real crossing, not a jump across an asymptote
                val r = bisect({ p.eval(it) }, xs[i], xs[i + 1]); out += r to 0.0
            }
            if (i > 0 && ok(y[i - 1])) {
                val d0 = a - y[i - 1]; val d1 = b - a
                if (d0 != 0.0 && d1 != 0.0 && (d0 < 0) != (d1 < 0)) {
                    val h = xs[1] - xs[0]
                    val den = y[i - 1] - 2 * a + b
                    val xv = if (den != 0.0) xs[i] + 0.5 * h * (y[i - 1] - b) / den else xs[i]
                    val yv = p.eval(xv)
                    if (yv.isFinite()) out += xv to yv
                }
            }
        }
    }
    // intersections of every pair
    for (u in progs.indices) for (v in u + 1 until progs.size) {
        val fu = ys[u]; val fv = ys[v]
        for (i in 0 until n) {
            val a = fu[i] - fv[i]; val b = fu[i + 1] - fv[i + 1]
            if (!ok(fu[i]) || !ok(fv[i]) || !ok(fu[i + 1]) || !ok(fv[i + 1])) continue
            if ((a < 0) != (b < 0) && kotlin.math.abs(a - b) < span) {
                val r = bisect({ progs[u].eval(it) - progs[v].eval(it) }, xs[i], xs[i + 1])
                val yr = progs[u].eval(r)
                if (yr.isFinite()) out += r to yr
            }
        }
    }
    return out.filter { (x, y) -> x in xmin..xmax && y in ymin..ymax }
}
