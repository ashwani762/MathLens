// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.input.pointer.pointerInput
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.engine.PlotPoint
import com.mathlens.app.ui.theme.Tone
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * A surface z = f(x, y) drawn purely from mathematics: the function is sampled on a grid,
 * projected with a small perspective, shaded by height and by a soft key light, and depth-sorted.
 * Drag to turn it; it also drifts slowly on its own.
 */
class SurfaceMesh(expr: String, val range: Float = 3f, val n: Int = 30) : AutoCloseable {
    private val program = MathEngine.compile(expr)
    val z = Array(n + 1) { FloatArray(n + 1) }
    var zmin = 0f; var zmax = 1f

    init {
        val xs = DoubleArray((n + 1) * (n + 1)); val ys = DoubleArray(xs.size); val out = DoubleArray(xs.size)
        for (i in 0..n) for (j in 0..n) {
            xs[i * (n + 1) + j] = -range + 2.0 * range * i / n
            ys[i * (n + 1) + j] = -range + 2.0 * range * j / n
        }
        program?.evalMany(xs, ys, out)
        val finite = out.filter { it.isFinite() }.sorted()
        if (finite.isNotEmpty()) {
            zmin = finite[(finite.size * 0.02).toInt()].toFloat()
            zmax = finite[((finite.size - 1) * 0.98).toInt()].toFloat()
            if (zmax - zmin < 1e-6f) zmax = zmin + 1f
        }
        for (i in 0..n) for (j in 0..n) {
            val v = out[i * (n + 1) + j]
            z[i][j] = if (v.isFinite()) v.toFloat().coerceIn(zmin, zmax) else zmin
        }
    }

    fun eval(x: Double, y: Double): Double = program?.eval(x, y) ?: Double.NaN

    /** Height normalised to [-1, 1]. */
    fun nz(v: Float) = (v - zmin) / (zmax - zmin) * 2f - 1f

    override fun close() { program?.close() }
}

class Camera3D(var yaw: Float, var pitch: Float) {
    /** Project unit-cube coordinates (x, y in [-1,1], z in [-1,1]) to screen, returning (screen, depth). */
    fun project(x: Float, y: Float, z: Float, w: Float, h: Float): Pair<Offset, Float> {
        val cy = cos(yaw); val sy = sin(yaw); val cp = cos(pitch); val sp = sin(pitch)
        val x1 = x * cy - y * sy
        val y1 = x * sy + y * cy
        val depth = y1 * cp + z * sp
        val up = z * cp - y1 * sp
        val persp = 1f / (1f + depth * 0.18f)
        val s = min(w, h) * 0.34f * persp
        return Offset(w / 2 + x1 * s, h * 0.55f - up * s * 0.9f) to depth
    }
}

private val low = Tone.Bone
private val mid = Tone.Sand
private val high = Tone.Terracotta

fun heightColor(t: Float): Color = if (t < 0.5f) lerp(low, mid, t * 2) else lerp(mid, high, (t - 0.5f) * 2)

fun DrawScope.drawSurface(mesh: SurfaceMesh, cam: Camera3D, markers: List<Triple<Float, Float, Color>> = emptyList()) {
    val n = mesh.n
    val w = size.width; val h = size.height
    val pts = Array(n + 1) { i -> Array(n + 1) { j ->
        val x = -1f + 2f * i / n; val y = -1f + 2f * j / n
        cam.project(x, y, mesh.nz(mesh.z[i][j]) * 0.7f, w, h)
    } }
    // floor grid for grounding
    val floor = Tone.Linen
    for (k in 0..6) {
        val t = -1f + 2f * k / 6
        drawLine(floor, cam.project(t, -1f, -0.85f, w, h).first, cam.project(t, 1f, -0.85f, w, h).first, 1f)
        drawLine(floor, cam.project(-1f, t, -0.85f, w, h).first, cam.project(1f, t, -0.85f, w, h).first, 1f)
    }
    data class Quad(val i: Int, val j: Int, val depth: Float)
    val quads = ArrayList<Quad>(n * n)
    for (i in 0 until n) for (j in 0 until n) {
        val d = (pts[i][j].second + pts[i + 1][j].second + pts[i][j + 1].second + pts[i + 1][j + 1].second) / 4
        quads.add(Quad(i, j, d))
    }
    quads.sortByDescending { it.depth }  // far to near
    val path = Path()
    val light = floatArrayOf(-0.4f, -0.5f, 0.75f)
    for (q in quads) {
        val (i, j) = q.i to q.j
        val z00 = mesh.nz(mesh.z[i][j]); val z10 = mesh.nz(mesh.z[i + 1][j]); val z01 = mesh.nz(mesh.z[i][j + 1]); val z11 = mesh.nz(mesh.z[i + 1][j + 1])
        // face normal from the grid differences
        val dx = (z10 + z11 - z00 - z01) / 2 * 0.7f; val dy = (z01 + z11 - z00 - z10) / 2 * 0.7f
        val step = 2f / n
        var nx = -dx / step; var ny = -dy / step; var nzv = 1f
        val len = sqrt(nx * nx + ny * ny + nzv * nzv); nx /= len; ny /= len; nzv /= len
        val lambert = max(0f, nx * light[0] + ny * light[1] + nzv * light[2])
        val t = ((z00 + z10 + z01 + z11) / 4 + 1f) / 2f
        val base = heightColor(t.coerceIn(0f, 1f))
        val shaded = lerp(lerp(base, Color(0xFF3B2A1C), 0.28f), base, 0.55f + 0.45f * lambert)
        path.reset()
        path.moveTo(pts[i][j].first.x, pts[i][j].first.y)
        path.lineTo(pts[i + 1][j].first.x, pts[i + 1][j].first.y)
        path.lineTo(pts[i + 1][j + 1].first.x, pts[i + 1][j + 1].first.y)
        path.lineTo(pts[i][j + 1].first.x, pts[i][j + 1].first.y)
        path.close()
        drawPath(path, shaded)
        drawPath(path, Tone.Ink.copy(alpha = 0.10f), style = Stroke(0.6f))
    }
    markers.forEach { (x, y, color) ->
        val gi = ((x / mesh.range + 1f) / 2f * n).coerceIn(0f, n.toFloat())
        val gj = ((y / mesh.range + 1f) / 2f * n).coerceIn(0f, n.toFloat())
        val zz = mesh.eval(x.toDouble(), y.toDouble()).toFloat().coerceIn(mesh.zmin, mesh.zmax)
        val (p, _) = cam.project(-1f + 2f * gi / n, -1f + 2f * gj / n, mesh.nz(zz) * 0.7f + 0.03f, w, h)
        drawCircle(Color(0x44_3B2A1C), 9f, p + Offset(2f, 4f))
        drawCircle(color, 8f, p)
        drawCircle(Tone.Ivory, 3f, p)
    }
}

@Composable
fun Surface3D(expr: String, points: List<PlotPoint>, modifier: Modifier = Modifier) {
    val mesh = remember(expr) { SurfaceMesh(expr) }
    DisposableEffect(mesh) { onDispose { mesh.close() } }
    var yaw by remember { mutableFloatStateOf(0.7f) }
    var pitch by remember { mutableFloatStateOf(0.55f) }
    val inf = rememberInfiniteTransition(label = "spin")
    val drift by inf.animateFloat(0f, (2 * Math.PI).toFloat(), infiniteRepeatable(tween(40000, easing = LinearEasing), RepeatMode.Restart), label = "drift")
    Canvas(modifier.pointerInput(Unit) {
        detectDragGestures { _, d ->
            yaw -= d.x * 0.008f
            pitch = (pitch + d.y * 0.006f).coerceIn(0.1f, 1.35f)
        }
    }) {
        val cam = Camera3D(yaw + drift, pitch)
        drawSurface(mesh, cam, points.map { Triple(it.x.toFloat(), it.y.toFloat(), Tone.Ink) })
    }
}

/** Bridge so other files can call the DrawScope extension without importing it by name clash. */
object SurfaceMeshDraw {
    fun DrawScope.drawIt(mesh: SurfaceMesh, cam: Camera3D, markers: List<Triple<Float, Float, Color>>) = drawSurface(mesh, cam, markers)
}
