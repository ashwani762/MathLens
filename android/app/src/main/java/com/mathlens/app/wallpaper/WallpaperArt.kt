// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.wallpaper

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import androidx.compose.ui.graphics.toArgb
import com.mathlens.app.ui.math.MathFonts
import com.mathlens.app.ui.math.MathLayout
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

/** One adjustable number of a design (shown as a slider). */
data class ArtParam(val key: String, val label: String, val min: Float, val max: Float, val default: Float, val whole: Boolean = true)

/**
 * A mathematical artwork drawn directly (no equation input needed). [draw] strokes into a unit square
 * centred at the origin (radius ~1); [formula] is the LaTeX caption for the current parameters.
 */
data class ArtDesign(
    val name: String,
    val params: List<ArtParam>,
    val formula: (Map<String, Float>) -> String,
    val draw: (DrawKit, Map<String, Float>) -> Unit,
)

/** Drawing helpers handed to a design: world units, radius 1 fills ~40% of the short side. */
class DrawKit(val canvas: Canvas, val cx: Float, val cy: Float, val unit: Float, val stroke: (Path, Int) -> Unit) {
    fun path(points: Int, t0: Double, t1: Double, f: (Double) -> Pair<Double, Double>): Path {
        val p = Path()
        for (i in 0..points) {
            val t = t0 + (t1 - t0) * i / points
            val (x, y) = f(t)
            val sx = cx + (x * unit).toFloat(); val sy = cy - (y * unit).toFloat()
            if (i == 0) p.moveTo(sx, sy) else p.lineTo(sx, sy)
        }
        return p
    }
}

private fun p(m: Map<String, Float>, k: String) = m[k] ?: 0f

val artDesigns: List<ArtDesign> = listOf(
    ArtDesign(
        "Golden spiral",
        listOf(ArtParam("n", "Squares", 5f, 11f, 9f)),
        { _ -> "\\phi = \\frac{1 + \\sqrt{5}}{2}" },
    ) { k, m ->
        // Fibonacci squares added outward (right, up, left, down), a quarter arc in each: the golden spiral.
        // The arc's centre is the corner the new square shares with the tiles before it.
        val n = p(m, "n").roundToInt().coerceIn(3, 14)
        data class Sq(val x0: Double, val y0: Double, val x1: Double, val y1: Double, val cx: Double, val cy: Double, val start: Double, val r: Double)
        val sq = ArrayList<Sq>()
        sq += Sq(0.0, 0.0, 1.0, 1.0, 1.0, 1.0, PI, 1.0)
        var bx0 = 0.0; var by0 = 0.0; var bx1 = 1.0; var by1 = 1.0
        var a = 1.0; var b = 1.0  // previous two Fibonacci numbers
        for (i in 1 until n) {
            val s = if (i == 1) 1.0 else a + b
            if (i > 1) { a = b; b = s }
            when (i % 4) {
                1 -> { sq += Sq(bx1, by0, bx1 + s, by0 + s, bx1, by1, -PI / 2, s); bx1 += s }          // right, centre top-left
                2 -> { sq += Sq(bx0, by1, bx0 + s, by1 + s, bx0, by1, 0.0, s); by1 += s }              // up, centre bottom-left
                3 -> { sq += Sq(bx0 - s, by1 - s, bx0, by1, bx0, by0, PI / 2, s); bx0 -= s }           // left, centre bottom-right
                else -> { sq += Sq(bx1 - s, by0 - s, bx1, by0, bx1, by0, PI, s); by0 -= s }            // down, centre top-right
            }
        }
        // fit the tiling into the canvas (y up in the tiling, y down on screen)
        val bw = bx1 - bx0; val bh = by1 - by0
        val scale = kotlin.math.min(k.unit * 2.2 / bw, k.unit * 2.2 / bh)
        fun X(x: Double) = (k.cx + (x - (bx0 + bx1) / 2) * scale).toFloat()
        fun Y(y: Double) = (k.cy - (y - (by0 + by1) / 2) * scale).toFloat()
        val squares = Path(); val spiral = Path()
        sq.forEachIndexed { i, q ->
            squares.addRect(X(q.x0), Y(q.y1), X(q.x1), Y(q.y0), Path.Direction.CW)
            for (j in 0..24) {
                val t = q.start + (PI / 2) * j / 24
                val px = X(q.cx + q.r * cos(t)); val py = Y(q.cy + q.r * sin(t))
                if (i == 0 && j == 0) spiral.moveTo(px, py) else spiral.lineTo(px, py)
            }
        }
        k.stroke(squares, 1)
        k.stroke(spiral, 0)
    },
    ArtDesign(
        "Rose",
        listOf(ArtParam("k", "Petals", 2f, 9f, 5f)),
        { m -> "r = \\cos(" + p(m, "k").roundToInt() + "\\theta)" },
    ) { kit, m ->
        val k = p(m, "k").roundToInt()
        val turns = if (k % 2 == 1) 1.0 else 2.0
        kit.stroke(kit.path(1600, 0.0, PI * turns) { t -> val r = cos(k * t); r * cos(t) to r * sin(t) }, 0)
    },
    ArtDesign(
        "Lissajous",
        listOf(ArtParam("a", "a", 1f, 9f, 3f), ArtParam("b", "b", 1f, 9f, 4f)),
        { m -> "x = \\sin(" + p(m, "a").roundToInt() + "t + \\tfrac{\\pi}{2}),\\ y = \\sin(" + p(m, "b").roundToInt() + "t)" },
    ) { kit, m ->
        val a = p(m, "a").roundToInt().toDouble(); val b = p(m, "b").roundToInt().toDouble()
        kit.stroke(kit.path(2400, 0.0, 2 * PI) { t -> sin(a * t + PI / 2) to sin(b * t) }, 0)
    },
    ArtDesign(
        "Harmonograph",
        listOf(ArtParam("f", "Ratio", 1.5f, 4f, 2.01f, whole = false), ArtParam("d", "Damping", 0.2f, 3f, 1f, whole = false)),
        { m -> "x = e^{-dt}\\left(\\sin t + \\sin " + "%.2f".format(java.util.Locale.US, p(m, "f")) + "t\\right)" },
    ) { kit, m ->
        val f = p(m, "f").toDouble(); val d = p(m, "d").toDouble() * 0.004
        kit.stroke(kit.path(9000, 0.0, 240.0) { t ->
            val e = exp(-d * t)
            (e * (0.55 * sin(t + 0.3) + 0.45 * sin(f * t))) to (e * (0.55 * sin(1.003 * t + 1.7) + 0.45 * sin((f + 0.002) * t + 0.9)))
        }, 0)
    },
    ArtDesign(
        "Times table",
        listOf(ArtParam("m", "Multiplier", 2f, 34f, 2f), ArtParam("n", "Points", 60f, 300f, 200f)),
        { m -> "k \\to " + p(m, "m").roundToInt() + "k \\bmod " + p(m, "n").roundToInt() },
    ) { kit, m ->
        // chords from point k to point m*k on a circle: m = 2 draws a cardioid, 3 a nephroid, ...
        val mult = p(m, "m").roundToInt(); val n = p(m, "n").roundToInt()
        val chords = Path()
        fun pt(i: Int): Pair<Float, Float> { val a = 2 * PI * i / n - PI / 2; return (kit.cx + cos(a) * kit.unit).toFloat() to (kit.cy + sin(a) * kit.unit).toFloat() }
        for (i in 0 until n) { val (x0, y0) = pt(i); val (x1, y1) = pt((i * mult) % n); chords.moveTo(x0, y0); chords.lineTo(x1, y1) }
        kit.stroke(chords, 0)
        kit.stroke(kit.path(360, 0.0, 2 * PI) { t -> cos(t) to sin(t) }, 1)
    },
    ArtDesign(
        "Spirograph",
        listOf(ArtParam("k", "Lobes", 3f, 13f, 7f), ArtParam("r", "Depth", 0.2f, 0.9f, 0.55f, whole = false)),
        { m -> "r = 2 + \\sin(" + p(m, "k").roundToInt() + "\\theta)" },
    ) { kit, m ->
        // hypotrochoid: a circle rolling inside a circle
        val k = p(m, "k").roundToInt().toDouble(); val depth = p(m, "r").toDouble()
        val R = 1.0; val r = R / k; val dd = r * (1 + depth * 3)
        kit.stroke(kit.path(4000, 0.0, 2 * PI * k) { t -> val q = (R - r) / r; ((R - r) * cos(t) + dd * cos(q * t)) * 0.8 to ((R - r) * sin(t) - dd * sin(q * t)) * 0.8 }, 0)
    },
    ArtDesign(
        "Butterfly",
        listOf(ArtParam("t", "Turns", 4f, 24f, 12f)),
        { _ -> "r = e^{\\sin\\theta} - 2\\cos 4\\theta + \\sin^5\\tfrac{2\\theta - \\pi}{24}" },
    ) { kit, m ->
        val turns = p(m, "t").toDouble()
        kit.stroke(kit.path(9000, 0.0, 2 * PI * turns) { t ->
            val r = (exp(sin(t)) - 2 * cos(4 * t) + Math.pow(sin((2 * t - PI) / 24), 5.0)) * 0.3
            r * cos(t + PI / 2) to r * sin(t + PI / 2)
        }, 0)
    },
    ArtDesign(
        "Heart",
        listOf(ArtParam("s", "Size", 0.6f, 1.2f, 0.9f, whole = false)),
        { _ -> "(x^2 + y^2 - 1)^3 = x^2 y^3" },
    ) { kit, m ->
        val s = p(m, "s").toDouble() * 0.062
        kit.stroke(kit.path(1200, 0.0, 2 * PI) { t ->
            val x = 16 * Math.pow(sin(t), 3.0); val y = 13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t)
            x * s to y * s
        }, 0)
    },
    ArtDesign(
        "Spiral",
        listOf(ArtParam("t", "Turns", 2f, 12f, 6f)),
        { _ -> "r = \\theta" },
    ) { kit, m ->
        val turns = p(m, "t").toDouble()
        kit.stroke(kit.path(3000, 0.0, 2 * PI * turns) { t -> val r = t / (2 * PI * turns); r * cos(t) to r * sin(t) }, 0)
    },
)

object ArtRenderer {
    /** Renders a design in a style into a bitmap of the given size. */
    fun render(context: Context, design: ArtDesign, values: Map<String, Float>, o: WallOptions, width: Int, height: Int): Bitmap {
        val bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp)
        val w = width.toFloat(); val h = height.toFloat()
        val bg = Paint()
        val stops = o.style.background.map { it.toArgb() }.toIntArray()
        bg.shader = LinearGradient(0f, 0f, w * 0.4f, h, if (stops.size == 1) intArrayOf(stops[0], stops[0]) else stops, null, Shader.TileMode.CLAMP)
        c.drawRect(0f, 0f, w, h, bg)
        val unit = min(w, h) * 0.40f * o.zoom
        val base = w / 360f * o.lineWidth
        val kit = DrawKit(c, w / 2, h * 0.46f, unit) { path, tone ->
            val color = o.style.curves[tone % o.style.curves.size]
            val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
            val width1 = if (tone == 0) base else base * 0.6f
            if (o.style.glow) {
                paint.color = color.copy(alpha = 0.22f).toArgb(); paint.strokeWidth = width1 * 3f
                paint.maskFilter = BlurMaskFilter(width1 * 3f, BlurMaskFilter.Blur.NORMAL); c.drawPath(path, paint); paint.maskFilter = null
            }
            paint.color = (if (tone == 0) color else color.copy(alpha = 0.55f)).toArgb(); paint.strokeWidth = width1
            c.drawPath(path, paint)
        }
        val repeats = o.repeats.coerceIn(1, 24)
        for (rep in 0 until repeats) {
            c.save(); c.rotate(360f / repeats * rep, w / 2, h * 0.46f)
            design.draw(kit, values)
            c.restore()
        }
        if (o.showEquation) {
            val layout = MathLayout(MathFonts.get(context))
            var size = w / 18f
            var box = layout.layout(design.formula(values), size)
            if (box.width > w * 0.84f) { size *= w * 0.84f / box.width; box = layout.layout(design.formula(values), size) }
            val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = o.style.text.toArgb() }
            box.draw(c, (w - box.width) / 2, h * 0.86f, tp)
        }
        return bmp
    }

    fun defaults(d: ArtDesign) = d.params.associate { it.key to it.default }
}
