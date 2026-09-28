// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.wallpaper

import android.content.ContentValues
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Shader
import android.provider.MediaStore
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import com.mathlens.app.engine.CurveType
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.CurveSampler
import com.mathlens.app.ui.components.niceStep
import com.mathlens.app.ui.math.MathFonts
import com.mathlens.app.ui.math.MathLayout
import kotlin.math.max

data class WallStyle(
    val name: String,
    val background: List<Color>,
    val curves: List<Color>,
    val grid: Color,
    val text: Color,
    val glow: Boolean,
)

val wallStyles = listOf(
    WallStyle("Paper", listOf(Color(0xFFF6F3EE), Color(0xFFEFEAE2)), listOf(Color(0xFF151413), Color(0xFFB4533A)), Color(0x14151413), Color(0xCC151413), false),
    WallStyle("Bone", listOf(Color(0xFFEDE7DD), Color(0xFFE2D9CB)), listOf(Color(0xFF4A4642), Color(0xFF7C8A6E)), Color(0x1A4A4642), Color(0xCC4A4642), false),
    WallStyle("Klein", listOf(Color(0xFF1F3FBF), Color(0xFF16309A)), listOf(Color(0xFFFBFAF7)), Color(0x22FBFAF7), Color(0xE6FBFAF7), false),
    WallStyle("Terracotta", listOf(Color(0xFFB4533A), Color(0xFF9E4631)), listOf(Color(0xFFFBFAF7), Color(0xFFE9CDBE)), Color(0x1FFBFAF7), Color(0xE6FBFAF7), false),
    WallStyle("Sage", listOf(Color(0xFF7C8A6E), Color(0xFF6C7A5F)), listOf(Color(0xFFFBFAF7)), Color(0x1FFBFAF7), Color(0xE6FBFAF7), false),
    WallStyle("Ink", listOf(Color(0xFF151413), Color(0xFF0E0D0C)), listOf(Color(0xFFEDE7DD), Color(0xFFC8962E)), Color(0x14EDE7DD), Color(0xB3EDE7DD), true),
)

data class WallOptions(
    val style: WallStyle = wallStyles[0],
    val zoom: Float = 1f,
    val lineWidth: Float = 1f,
    val repeats: Int = 1,
    val turns: Double = 2.0,
    val showEquation: Boolean = true,
    val showGrid: Boolean = true,
)

object WallpaperRenderer {

    /** Renders the equations into a phone-sized bitmap. Runs off the main thread. */
    fun render(context: Context, equations: List<String>, o: WallOptions, width: Int, height: Int): Bitmap? {
        val spec = MathEngine.graph(equations.joinToString("; ")).getOrNull() ?: return null
        val bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp)
        val w = width.toFloat()
        val h = height.toFloat()

        // background
        val bg = Paint()
        val stops = o.style.background.map { it.toArgb() }.toIntArray()
        bg.shader = LinearGradient(0f, 0f, w * 0.4f, h, if (stops.size == 1) intArrayOf(stops[0], stops[0]) else stops, null, Shader.TileMode.CLAMP)
        c.drawRect(0f, 0f, w, h, bg)
        if (o.style.glow && false) {
            val halo = Paint().apply {
                shader = RadialGradient(w / 2, h * 0.45f, w * 0.9f, intArrayOf(o.style.curves[0].copy(alpha = 0.14f).toArgb(), 0), null, Shader.TileMode.CLAMP)
            }
            c.drawRect(0f, 0f, w, h, halo)
        }

        // world window: equal aspect, centred at the origin
        val baseSpan = (spec.view[1] - spec.view[0]).coerceIn(2.0, 60.0)
        val xSpan = baseSpan / o.zoom
        val upp = xSpan / w
        val ySpan = upp * h
        val xmin = -xSpan / 2
        val xmax = xSpan / 2
        val ymin = -ySpan / 2
        val ymax = ySpan / 2
        fun sx(x: Double) = ((x - xmin) / upp).toFloat()
        fun sy(y: Double) = ((ymax - y) / upp).toFloat()

        if (o.showGrid && o.style.grid.alpha > 0f) {
            val gp = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = o.style.grid.toArgb(); strokeWidth = max(1f, w / 900f) }
            val step = niceStep(xSpan, 10)
            var x = Math.floor(xmin / step) * step
            while (x <= xmax) { c.drawLine(sx(x), 0f, sx(x), h, gp); x += step }
            var y = Math.floor(ymin / step) * step
            while (y <= ymax) { c.drawLine(0f, sy(y), w, sy(y), gp); y += step }
            gp.strokeWidth *= 2
            c.drawLine(0f, sy(0.0), w, sy(0.0), gp)
            c.drawLine(sx(0.0), 0f, sx(0.0), h, gp)
        }

        // curves
        val baseWidth = w / 320f * o.lineWidth
        val samplers = spec.curves.map { CurveSampler(it) }
        try {
            val repeats = o.repeats.coerceIn(1, 24)
            for (rep in 0 until repeats) {
                c.save()
                c.rotate(360f / repeats * rep, sx(0.0), sy(0.0))
                samplers.forEachIndexed { idx, s ->
                    val color = o.style.curves[(idx + rep) % o.style.curves.size]
                    val lines = s.sample(xmin * 1.5, xmax * 1.5, ymin * 1.2, ymax * 1.2, width, height, o.turns)
                    val path = Path()
                    for (pl in lines) {
                        path.moveTo(sx(pl[0].toDouble()), sy(pl[1].toDouble()))
                        var k = 2
                        while (k < pl.size) { path.lineTo(sx(pl[k].toDouble()), sy(pl[k + 1].toDouble())); k += 2 }
                    }
                    val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                        style = Paint.Style.STROKE
                        strokeCap = Paint.Cap.ROUND
                        strokeJoin = Paint.Join.ROUND
                    }
                    if (o.style.glow) {
                        p.color = color.copy(alpha = 0.25f).toArgb()
                        p.strokeWidth = baseWidth * 3f
                        p.maskFilter = BlurMaskFilter(baseWidth * 3f, BlurMaskFilter.Blur.NORMAL)
                        c.drawPath(path, p)
                        p.maskFilter = null
                    }
                    p.color = color.toArgb()
                    p.strokeWidth = baseWidth
                    c.drawPath(path, p)
                }
                c.restore()
            }
        } finally {
            samplers.forEach { it.close() }
        }

        // equation caption
        if (o.showEquation) {
            val latex = equations.mapNotNull { MathEngine.previewLatex(it) }.joinToString(" \\quad ")
            if (latex.isNotEmpty()) {
                val layout = MathLayout(MathFonts.get(context))
                var size = w / 16f
                var box = layout.layout(latex, size)
                if (box.width > w * 0.84f) { size *= w * 0.84f / box.width; box = layout.layout(latex, size) }
                val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = o.style.text.toArgb() }
                box.draw(c, (w - box.width) / 2, h * 0.86f, tp)
            }
        }
        return bmp
    }

    fun saveToGallery(context: Context, bmp: Bitmap): Boolean = runCatching {
        val values = ContentValues().apply {
            put(MediaStore.Images.Media.DISPLAY_NAME, "MathLens_${System.currentTimeMillis()}.png")
            put(MediaStore.Images.Media.MIME_TYPE, "image/png")
            put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/MathLens")
        }
        val uri = context.contentResolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values) ?: return false
        context.contentResolver.openOutputStream(uri)?.use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
        true
    }.getOrDefault(false)
}
