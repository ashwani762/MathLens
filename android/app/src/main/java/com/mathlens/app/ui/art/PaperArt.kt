// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.art

import android.content.Context
import android.graphics.BlurMaskFilter
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.asAndroidPath
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.drawscope.translate
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import com.mathlens.app.ui.theme.Tone
import java.util.Random
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

/**
 * 2.5D paper-cut compositions. Flat shapes in gallery pigments are stacked in depth; each
 * layer casts a soft warm shadow onto the layers below and drifts in parallax as the phone
 * tilts (or, without a sensor, very slowly on its own). Think cut-paper dioramas, Ellsworth
 * Kelly's shapes, Barragán's arches.
 */
enum class PaperScene { Hero, Lens, Steps, Curve, Generated }

/** Optional function to cut as a paper layer (the user's own curve), sampled on [0, 1] -> [0, 1]. */
typealias UnitCurve = (Float) -> Float

enum class Cut { Circle, Arch, HalfDisc, Pill, Ring, Blob, Hill, Slab }

class Layer(
    val cut: Cut,
    val color: Color,
    val cx: Float, val cy: Float,      // centre in fractions of the canvas
    val w: Float, val h: Float,        // size in fractions of the shorter side
    val rotation: Float = 0f,
    val depth: Float = 0.5f,           // 0 = on the page, 1 = closest to the viewer
    val blob: List<Offset>? = null,
    val sway: Float = 0f,              // gentle rocking in degrees
    val unitSpace: Boolean = false,    // cx, cy measured in multiples of the shorter side from the centre
)

@Composable
fun PaperArt(
    modifier: Modifier = Modifier,
    scene: PaperScene = PaperScene.Generated,
    seed: Int = 0,
    background: Color = Tone.Paper,
    parallax: Boolean = true,
    curve: UnitCurve? = null,
) {
    val layers = remember(scene, seed) { compose(scene, seed) }
    val enter = remember(scene, seed) { Animatable(0f) }
    LaunchedEffect(scene, seed) { enter.animateTo(1f, tween(1600, easing = FastOutSlowInEasing)) }
    val inf = rememberInfiniteTransition(label = "paper")
    val drift by inf.animateFloat(0f, 1f, infiniteRepeatable(tween(18000, easing = LinearEasing), RepeatMode.Restart), label = "drift")
    val tilt = if (parallax) rememberTilt() else Offset.Zero
    Canvas(modifier) {
        drawRect(background)
        val m = min(size.width, size.height)
        val n = layers.size
        layers.sortedBy { it.depth }.forEachIndexed { i, l ->
            val start = i.toFloat() / (n + 1)
            val a = ((enter.value - start) / (1f - start)).coerceIn(0f, 1f).let { 1f - (1f - it) * (1f - it) * (1f - it) }
            if (a <= 0f) return@forEachIndexed
            val ph = (drift * 2 * PI + i * 1.3).toFloat()
            // tiles that belong to a drawn figure (the golden rectangle) move as one sheet with its ink
            val par = if (l.unitSpace) Offset(tilt.x * 30f, tilt.y * 30f) else Offset(
                (tilt.x * 26f + sin(ph) * 3f) * (0.3f + l.depth),
                (tilt.y * 26f + cos(ph) * 2f) * (0.3f + l.depth),
            )
            val base = if (l.unitSpace) Offset(size.width / 2 + l.cx * m, size.height / 2 + l.cy * m) else Offset(l.cx * size.width, l.cy * size.height)
            val center = base + Offset(0f, if (l.unitSpace) 0f else (1f - a) * 50f) + par
            val rot = l.rotation + l.sway * sin(ph)
            drawLayer(l, center, m, rot, a)
        }
        // mathematics drawn in ink over the paper
        val ink = ((enter.value - 0.45f) / 0.55f).coerceIn(0f, 1f)
        val par = Offset(tilt.x * 30f, tilt.y * 30f)
        when (scene) {
            PaperScene.Hero -> goldenSpiral(ink, drift, par)
            PaperScene.Lens -> rose(ink, drift, par)
            PaperScene.Curve -> harmonograph(ink, drift, par)
            PaperScene.Generated -> curve?.let { userCurve(it, ink, par, seed) }
            else -> {}
        }
    }
}

/** Smoothed device tilt in [-1, 1] from the game rotation vector sensor. */
@Composable
fun rememberTilt(): Offset {
    val context = LocalContext.current
    var tilt by remember { mutableStateOf(Offset.Zero) }
    DisposableEffect(Unit) {
        val sm = context.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
        val sensor = sm?.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR) ?: sm?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        val rot = FloatArray(9)
        val ori = FloatArray(3)
        var base: FloatArray? = null
        val listener = object : SensorEventListener {
            override fun onSensorChanged(e: SensorEvent) {
                val (pitch, roll) = if (e.sensor.type == Sensor.TYPE_ACCELEROMETER) {
                    (e.values[1] / 9.81f) to (e.values[0] / 9.81f)
                } else {
                    SensorManager.getRotationMatrixFromVector(rot, e.values)
                    SensorManager.getOrientation(rot, ori)
                    ori[1] to ori[2]
                }
                val b = base ?: floatArrayOf(pitch, roll).also { base = it }
                val target = Offset(((roll - b[1]) * 1.6f).coerceIn(-1f, 1f), ((pitch - b[0]) * -1.6f).coerceIn(-1f, 1f))
                tilt += (target - tilt) * 0.12f  // low-pass for a calm response
            }
            override fun onAccuracyChanged(s: Sensor?, a: Int) {}
        }
        if (sensor != null) sm?.registerListener(listener, sensor, SensorManager.SENSOR_DELAY_GAME)
        onDispose { sm?.unregisterListener(listener) }
    }
    return tilt
}

// ------------------------------------------------------------------ drawing

private val shadowPaint = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG)

private fun layerPath(l: Layer, m: Float): Path {
    val w = l.w * m
    val h = l.h * m
    val p = Path()
    when (l.cut) {
        Cut.Circle -> p.addOval(Rect(-w / 2, -w / 2, w / 2, w / 2))
        Cut.Ring -> {
            p.addOval(Rect(-w / 2, -w / 2, w / 2, w / 2))
            p.addOval(Rect(-w * 0.3f, -w * 0.3f, w * 0.3f, w * 0.3f))
            p.fillType = androidx.compose.ui.graphics.PathFillType.EvenOdd
        }
        Cut.HalfDisc -> {
            p.moveTo(-w / 2, 0f)
            p.arcTo(Rect(-w / 2, -w / 2, w / 2, w / 2), 180f, 180f, false)
            p.close()
        }
        Cut.Arch -> {
            // a doorway: rectangle with a semicircular top
            p.moveTo(-w / 2, h / 2)
            p.lineTo(-w / 2, -h / 2 + w / 2)
            p.arcTo(Rect(-w / 2, -h / 2, w / 2, -h / 2 + w), 180f, 180f, false)
            p.lineTo(w / 2, h / 2)
            p.close()
        }
        Cut.Pill -> p.addRoundRect(RoundRect(-w / 2, -h / 2, w / 2, h / 2, CornerRadius(min(w, h) / 2)))
        Cut.Slab -> p.addRoundRect(RoundRect(-w / 2, -h / 2, w / 2, h / 2, CornerRadius(m * 0.012f)))
        Cut.Hill -> {
            p.moveTo(-w / 2, h / 2)
            p.cubicTo(-w * 0.25f, -h / 2, w * 0.2f, -h / 2, w / 2, h * 0.1f)
            p.lineTo(w / 2, h / 2)
            p.close()
        }
        Cut.Blob -> {
            val pts = l.blob ?: return p
            val n = pts.size
            fun q(i: Int) = Offset(pts[(i + n) % n].x * w / 2, pts[(i + n) % n].y * h / 2)
            for (i in 0 until n) {
                val p0 = q(i - 1); val p1 = q(i); val p2 = q(i + 1); val p3 = q(i + 2)
                if (i == 0) p.moveTo(p1.x, p1.y)
                p.cubicTo(p1.x + (p2.x - p0.x) / 6f, p1.y + (p2.y - p0.y) / 6f, p2.x - (p3.x - p1.x) / 6f, p2.y - (p3.y - p1.y) / 6f, p2.x, p2.y)
            }
            p.close()
        }
    }
    return p
}

/** Draws a paper layer: soft contact shadow, the cut with a faint top-light, and a hairline edge. */
private fun DrawScope.drawLayer(l: Layer, center: Offset, m: Float, rotation: Float, alpha: Float) {
    val path = layerPath(l, m)
    val lift = 0.35f + l.depth
    translate(center.x, center.y) {
        rotate(rotation, pivot = Offset.Zero) {
            // shadow: offset down-right, blurred, warm
            drawIntoCanvas { c ->
                shadowPaint.color = Color(0xFF3B2A1C).copy(alpha = 0.20f * alpha).toArgb()
                shadowPaint.maskFilter = BlurMaskFilter(m * 0.022f * lift, BlurMaskFilter.Blur.NORMAL)
                c.nativeCanvas.save()
                c.nativeCanvas.translate(m * 0.010f * lift, m * 0.018f * lift)
                c.nativeCanvas.drawPath(path.asAndroidPath(), shadowPaint)
                c.nativeCanvas.restore()
            }
            val b = path.getBounds()
            drawPath(
                path,
                Brush.verticalGradient(listOf(lerp(l.color, Color.White, 0.10f), l.color, lerp(l.color, Color.Black, 0.04f)), b.top, b.bottom),
                alpha = alpha,
            )
            drawPath(path, Color.White.copy(alpha = 0.18f * alpha), style = Stroke(1.2f))
        }
    }
}

// ------------------------------------------------------------------ compositions

private fun compose(scene: PaperScene, seed: Int): List<Layer> = when (scene) {
    PaperScene.Hero -> fibonacciTiles()
    PaperScene.Lens -> listOf(
        Layer(Cut.Circle, Tone.Bone, 0.5f, 0.5f, 0.82f, 0.82f, depth = 0.05f),
        Layer(Cut.Ring, Tone.Ivory, 0.5f, 0.5f, 0.62f, 0.62f, depth = 0.4f),
        Layer(Cut.Circle, Tone.Ink, 0.5f, 0.5f, 0.3f, 0.3f, depth = 0.7f),
        Layer(Cut.Circle, Tone.Terracotta, 0.56f, 0.44f, 0.08f, 0.08f, depth = 1f),
        Layer(Cut.Pill, Tone.Sage, 0.24f, 0.2f, 0.26f, 0.08f, rotation = -18f, depth = 0.9f, sway = 4f),
    )
    PaperScene.Steps -> listOf(
        Layer(Cut.Slab, Tone.Bone, 0.5f, 0.66f, 0.7f, 0.2f, rotation = -4f, depth = 0.1f),
        Layer(Cut.Slab, Tone.Sand, 0.52f, 0.5f, 0.62f, 0.2f, rotation = 3f, depth = 0.4f),
        Layer(Cut.Slab, Tone.Ivory, 0.48f, 0.34f, 0.56f, 0.2f, rotation = -2f, depth = 0.7f),
        Layer(Cut.Circle, Tone.Terracotta, 0.72f, 0.24f, 0.12f, 0.12f, depth = 1f),
        Layer(Cut.Pill, Tone.Ink, 0.4f, 0.34f, 0.3f, 0.03f, depth = 0.85f),
    )
    PaperScene.Curve -> listOf(
        Layer(Cut.Circle, Tone.Ochre, 0.7f, 0.3f, 0.34f, 0.34f, depth = 0.1f),
        Layer(Cut.Hill, Tone.Sage, 0.34f, 0.72f, 0.9f, 0.5f, depth = 0.35f),
        Layer(Cut.Hill, Tone.Bone, 0.66f, 0.84f, 0.95f, 0.42f, rotation = 0f, depth = 0.6f),
        Layer(Cut.Hill, Tone.Terracotta, 0.3f, 0.92f, 0.8f, 0.26f, depth = 0.85f),
        Layer(Cut.Circle, Tone.Ink, 0.56f, 0.56f, 0.06f, 0.06f, depth = 1f),
    )
    PaperScene.Generated -> generated(seed)
}

private fun generated(seed: Int): List<Layer> {
    val r = Random(seed.toLong() * 31 + 7)
    val pig = listOf(Tone.Terracotta, Tone.Ochre, Tone.Sage, Tone.Klein, Tone.Sand, Tone.Blush, Tone.Ink).shuffled(r)
    fun f(a: Float, b: Float) = a + (b - a) * r.nextFloat()
    val family = Math.floorMod(seed, 3)
    return when (family) {
        0 -> listOf( // arches
            Layer(Cut.Arch, Tone.Bone, f(0.35f, 0.45f), 0.56f, 0.46f, 0.8f, depth = 0.05f),
            Layer(Cut.Arch, pig[0], f(0.55f, 0.65f), 0.62f, 0.34f, 0.62f, depth = 0.4f),
            Layer(Cut.Circle, pig[1], f(0.25f, 0.75f), f(0.2f, 0.35f), 0.18f, 0.18f, depth = 0.7f),
            Layer(Cut.Pill, pig[2], f(0.3f, 0.7f), 0.86f, 0.5f, 0.06f, depth = 0.9f),
        )
        1 -> listOf( // sun over hills
            Layer(Cut.Circle, pig[0], f(0.3f, 0.7f), f(0.28f, 0.4f), 0.36f, 0.36f, depth = 0.05f),
            Layer(Cut.Hill, pig[1], 0.4f, 0.78f, 0.95f, 0.46f, depth = 0.35f),
            Layer(Cut.Hill, Tone.Bone, 0.66f, 0.88f, 0.9f, 0.34f, depth = 0.65f),
            Layer(Cut.HalfDisc, pig[2], f(0.2f, 0.8f), 0.9f, 0.2f, 0.2f, depth = 0.95f, sway = 3f),
        )
        else -> listOf( // cut-outs
            Layer(Cut.Blob, pig[0], f(0.3f, 0.45f), f(0.35f, 0.5f), 0.46f, 0.46f, depth = 0.1f, blob = blob(r), sway = 4f),
            Layer(Cut.Blob, pig[1], f(0.55f, 0.7f), f(0.5f, 0.65f), 0.36f, 0.36f, depth = 0.5f, blob = blob(r), sway = -5f),
            Layer(Cut.Circle, pig[2], f(0.2f, 0.8f), f(0.2f, 0.3f), 0.12f, 0.12f, depth = 0.8f),
            Layer(Cut.Pill, Tone.Ink, f(0.3f, 0.7f), 0.85f, 0.28f, 0.035f, rotation = f(-10f, 10f), depth = 1f),
        )
    }
}

// ------------------------------------------------------------------ mathematical drawings

/** Fibonacci squares (1,1,2,3,5,8) laid out around the golden spiral, in unit space [0,13]x[0,8]. */
private val fibSquares: List<Triple<Float, Float, Float>> by lazy {
    // (x, y, size) with y downward; classic arrangement spiralling inward
    listOf(
        Triple(0f, 0f, 8f), Triple(8f, 0f, 5f), Triple(10f, 5f, 3f), Triple(8f, 6f, 2f), Triple(8f, 5f, 1f), Triple(9f, 5f, 1f),
    )
}

private fun fibonacciTiles(): List<Layer> {
    val colors = listOf(Tone.Bone, Tone.Sand, Tone.Terracotta, Tone.Sage, Tone.Ochre, Tone.Ink)
    return fibSquares.mapIndexed { i, (x, y, sz) ->
        // the 13 x 8 golden rectangle, centred, spanning 88% of the shorter side
        val u = 0.88f / 13f
        Layer(Cut.Slab, colors[i], (x + sz / 2 - 6.5f) * u, (y + sz / 2 - 4f) * u, sz * u - 0.004f, sz * u - 0.004f,
            depth = 0.1f + i * 0.17f, unitSpace = true)
    }
}

private fun DrawScope.inkStroke(path: Path, progress: Float, color: Color = Tone.Ink, width: Float = 2.2f) {
    if (progress <= 0f) return
    val pm = androidx.compose.ui.graphics.PathMeasure()
    pm.setPath(path, false)
    val out = Path()
    pm.getSegment(0f, pm.length * progress, out, true)
    drawPath(out, color, style = Stroke(width, cap = androidx.compose.ui.graphics.StrokeCap.Round))
}

private fun DrawScope.goldenSpiral(p: Float, drift: Float, par: Offset) {
    val unit = min(size.width, size.height) * 0.88f / 13f
    val ox = size.width / 2 - 6.5f * unit + par.x
    val oy = size.height / 2 - 4f * unit + par.y
    // quarter arcs through each square, turning inward; (centre, radius, start angle, sweep)
    val arcs = listOf(
        Triple(Offset(8f, 8f), 8f, 180f), Triple(Offset(8f, 5f), 5f, 270f), Triple(Offset(10f, 5f), 3f, 0f),
        Triple(Offset(10f, 6f), 2f, 90f), Triple(Offset(9f, 6f), 1f, 180f), Triple(Offset(9f, 6f), 1f, 270f),
    )
    val path = Path()
    arcs.forEachIndexed { i, (c, r, start) ->
        val rect = Rect(ox + (c.x - r) * unit, oy + (c.y - r) * unit, ox + (c.x + r) * unit, oy + (c.y + r) * unit)
        // the last arc stops early so the spiral ends inside the smallest (ink) square
        val sweep = if (i == arcs.lastIndex) 60f else 90f
        path.arcTo(rect, start, sweep, i == 0)
    }
    // never draw outside the golden rectangle
    clipRect(ox, oy, ox + 13f * unit, oy + 8f * unit) {
        inkStroke(path, p, if (p < 1f) Tone.Ink else Tone.Ink, 2.2f)
        if (p >= 1f) {
            val pm = androidx.compose.ui.graphics.PathMeasure()
            pm.setPath(path, false)
            val pos = pm.getPosition(pm.length * ((drift * 2f) % 1f))
            drawCircle(Tone.Terracotta, 5f, pos)
        }
    }
}

private fun DrawScope.rose(p: Float, drift: Float, par: Offset) {
    val c = Offset(size.width / 2, size.height / 2) + par * 1.2f
    val r = min(size.width, size.height) * 0.28f
    val k = 5f / 3f
    val path = Path()
    val n = 720
    for (i in 0..n) {
        val th = 6 * PI * i / n
        val rr = r * cos(k * th)
        val x = c.x + (rr * cos(th + drift * 2 * PI * 0.05)).toFloat()
        val y = c.y + (rr * sin(th + drift * 2 * PI * 0.05)).toFloat()
        if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    inkStroke(path, p, Tone.Terracotta, 1.5f)
}

private fun DrawScope.harmonograph(p: Float, drift: Float, par: Offset) {
    // two damped pendulums per axis: x = A sin(f t + φ) e^{-dt} + ...
    val c = Offset(size.width / 2, size.height * 0.42f) + par
    val a = min(size.width, size.height) * 0.3f
    val path = Path()
    val n = 1800
    val ph = (drift * 2 * PI).toFloat()
    for (i in 0..n) {
        val t = i / 40f
        val decay = kotlin.math.exp(-0.012f * t)
        val x = a * decay * (sin(2.01f * t + ph * 0.1f) * 0.7f + sin(3f * t + 1.2f) * 0.3f)
        val y = a * decay * (sin(3.0f * t) * 0.6f + sin(2f * t + 0.7f) * 0.4f)
        if (i == 0) path.moveTo(c.x + x, c.y + y) else path.lineTo(c.x + x, c.y + y)
    }
    inkStroke(path, p, Tone.Ink, 1.1f)
}

/** The user's own function, cut from paper: the region under it becomes a layer. */
private fun DrawScope.userCurve(f: UnitCurve, p: Float, par: Offset, seed: Int) {
    val base = size.height * 0.92f
    val fill = Path()
    val line = Path()
    val n = 160
    for (i in 0..n) {
        val u = i.toFloat() / n
        val y = size.height * (0.85f - 0.62f * f(u).coerceIn(0f, 1f))
        val x = size.width * (0.06f + 0.88f * u)
        if (i == 0) { fill.moveTo(x, base); fill.lineTo(x, y); line.moveTo(x, y) } else { fill.lineTo(x, y); line.lineTo(x, y) }
    }
    fill.lineTo(size.width * 0.94f, base); fill.close()
    val colors = listOf(Tone.Terracotta, Tone.Sage, Tone.Ochre, Tone.Klein)
    translate(par.x * 1.3f, par.y * 1.3f) {
        drawIntoCanvas { c ->
            shadowPaint.color = Color(0xFF3B2A1C).copy(alpha = 0.22f * p).toArgb()
            shadowPaint.maskFilter = BlurMaskFilter(size.minDimension * 0.03f, BlurMaskFilter.Blur.NORMAL)
            c.nativeCanvas.save(); c.nativeCanvas.translate(size.minDimension * 0.015f, size.minDimension * 0.025f)
            c.nativeCanvas.drawPath(fill.asAndroidPath(), shadowPaint); c.nativeCanvas.restore()
        }
        drawPath(fill, colors[Math.floorMod(seed, colors.size)].copy(alpha = 0.92f * p))
        inkStroke(line, p, Tone.Ink, 2f)
    }
}

private fun blob(r: Random): List<Offset> {
    val n = 7 + r.nextInt(3)
    return List(n) { i ->
        val rad = 0.6f + 0.4f * r.nextFloat()
        val a = 2 * PI * i / n
        Offset((rad * cos(a)).toFloat(), (rad * sin(a)).toFloat())
    }
}
