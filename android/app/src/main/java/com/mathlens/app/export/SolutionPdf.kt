// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.export

import android.content.Context
import android.content.Intent
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.graphics.pdf.PdfDocument
import androidx.core.content.FileProvider
import com.mathlens.app.engine.CurveType
import com.mathlens.app.engine.PlotSpec
import com.mathlens.app.engine.Solution
import com.mathlens.app.engine.Step
import com.mathlens.app.ui.components.CurveSampler
import com.mathlens.app.ui.math.MathFonts
import com.mathlens.app.ui.math.MathLayout
import java.io.File

/**
 * A solution as an A4 PDF: problem, answer, facts, the graph (when there is one) and every step, with math
 * drawn by the app's own renderer. Pages break automatically; each page carries a quiet footer.
 */
object SolutionPdf {
    private const val W = 595
    private const val H = 842
    private const val M = 48f
    private val INK = Color.rgb(0x17, 0x16, 0x14)
    private val GRAPHITE = Color.rgb(0x4A, 0x46, 0x40)
    private val STONE = Color.rgb(0x8C, 0x86, 0x7C)
    private val TERRACOTTA = Color.rgb(0xB4, 0x53, 0x3A)
    private val HAIRLINE = Color.rgb(0xDD, 0xD6, 0xCA)

    fun share(context: Context, s: Solution) {
        val file = write(context, s)
        val uri = FileProvider.getUriForFile(context, context.packageName + ".files", file)
        val send = Intent(Intent.ACTION_SEND).setType("application/pdf").putExtra(Intent.EXTRA_STREAM, uri).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        context.startActivity(Intent.createChooser(send, "Share PDF"))
    }

    fun write(context: Context, s: Solution): File {
        val doc = PdfDocument()
        val w = Writer(doc, MathLayout(MathFonts(context)))
        w.eyebrow(s.title.ifEmpty { "Solution" }.replace(Regex("\\$[^$]*\\$"), "").trim().ifEmpty { "Solution" })
        w.math(s.input, 20f, INK)
        w.gap(10f)
        w.eyebrow("Answer", TERRACOTTA)
        w.math(s.answer, 18f, INK)
        if (s.facts.isNotEmpty()) {
            w.gap(8f)
            s.facts.take(8).forEach { f -> w.fact(f.label, f.latex) }
        }
        s.plot?.let { p -> if (p.curves.any { it.type != CurveType.Surface }) { w.gap(14f); w.graph(p) } }
        s.methods.firstOrNull()?.let { m ->
            w.gap(16f)
            w.eyebrow("The method")
            w.heading(m.name)
            m.steps.forEachIndexed { i, st -> w.step(i, st) }
        }
        w.finish()
        val dir = File(context.cacheDir, "pdf").apply { mkdirs() }
        dir.listFiles()?.forEach { it.delete() }  // keep only the latest export
        val out = File(dir, "MathLens-${System.currentTimeMillis()}.pdf")
        out.outputStream().use { doc.writeTo(it) }
        doc.close()
        return out
    }

    private class Writer(val doc: PdfDocument, val layout: MathLayout) {
        private var pageNo = 0
        private var page: PdfDocument.Page? = null
        private lateinit var c: Canvas
        private var y = 0f
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val sans = Typeface.create("sans-serif", Typeface.NORMAL)
        private val sansMedium = Typeface.create("sans-serif-medium", Typeface.NORMAL)
        private val serif = Typeface.create(Typeface.SERIF, Typeface.NORMAL)
        private val roman = listOf("I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV")

        init { newPage() }

        // Text and math are drawn at K x size inside a canvas scaled by 1/K: small PDF text otherwise gets
        // hinted glyph advances that don't match their measurements (uneven spacing, overlaps).
        private val K = 4f
        private fun text(t: String, x: Float, baseline: Float, p: Paint) {
            p.flags = p.flags or Paint.LINEAR_TEXT_FLAG or Paint.SUBPIXEL_TEXT_FLAG or Paint.ANTI_ALIAS_FLAG
            val size = p.textSize
            c.save(); c.scale(1 / K, 1 / K)
            p.textSize = size * K
            c.drawText(t, x * K, baseline * K, p)
            p.textSize = size
            c.restore()
        }
        private fun measure(t: String, p: Paint): Float {
            val size = p.textSize
            p.flags = p.flags or Paint.LINEAR_TEXT_FLAG or Paint.SUBPIXEL_TEXT_FLAG
            p.textSize = size * K
            val w = p.measureText(t) / K
            p.textSize = size
            return w
        }
        private class Laid(val box: com.mathlens.app.ui.math.Box, val k: Float) {
            val width get() = box.width / k
            val ascent get() = box.ascent / k
            val descent get() = box.descent / k
            val height get() = box.height / k
        }
        private fun lay(latex: String, size: Float) = Laid(layout.layout(latex, size * K), K)
        /** Formulas as vector text, drawn at K x in a canvas scaled by 1/K with a clean paint (no leftover spacing). */
        private fun drawLaid(l: Laid, x: Float, baseline: Float, p: Paint) {
            val mp = Paint(Paint.ANTI_ALIAS_FLAG or Paint.LINEAR_TEXT_FLAG or Paint.SUBPIXEL_TEXT_FLAG).apply { color = p.color; style = Paint.Style.FILL }
            c.save(); c.scale(1 / K, 1 / K)
            l.box.draw(c, x * K, baseline * K, mp)
            c.restore()
        }

        private fun newPage() {
            page?.let { footer(); doc.finishPage(it) }
            pageNo++
            page = doc.startPage(PdfDocument.PageInfo.Builder(W, H, pageNo).create())
            c = page!!.canvas
            y = M
        }

        private fun room(h: Float) { if (y + h > H - M - 20) newPage() }

        private fun footer() {
            paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.color = STONE; paint.textSize = 8f; paint.typeface = sans; paint.letterSpacing = 0.2f
            text("MATHLENS", M, H - M / 2, paint)
            val n = pageNo.toString()
            text(n, W - M - measure(n, paint), H - M / 2, paint)
        }

        fun finish() { page?.let { footer(); doc.finishPage(it) }; page = null }

        fun gap(h: Float) { y += h }

        fun eyebrow(t: String, color: Int = STONE) {
            room(18f)
            paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.color = color; paint.textSize = 8.5f; paint.typeface = sansMedium; paint.letterSpacing = 0.18f
            text(t.uppercase(), M, y + 9f, paint)
            y += 18f
        }

        fun heading(t: String) {
            room(26f)
            paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.color = INK; paint.textSize = 17f; paint.typeface = serif
            text(t, M, y + 16f, paint)
            y += 26f
            rule()
        }

        private fun rule() {
            paint.reset(); paint.letterSpacing = 0f; paint.color = HAIRLINE; paint.strokeWidth = 0.6f
            c.drawLine(M, y, W - M, y, paint)
            y += 10f
        }

        /** A formula, shrunk to fit the text width when needed. */
        fun math(latex: String, size: Float, color: Int, x: Float = M, maxW: Float = W - 2 * M) {
            if (latex.isBlank()) return
            var box = lay(latex, size)
            if (box.width > maxW) box = lay(latex, size * maxW / box.width)
            room(box.height + 6f)
            paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.color = color; paint.style = Paint.Style.FILL
            drawLaid(box, x, y + box.ascent, paint)
            y += box.height + 6f
        }

        fun fact(label: String, latex: String) {
            paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.color = STONE; paint.textSize = 7.5f; paint.typeface = sansMedium; paint.letterSpacing = 0.16f
            room(28f)
            text(label.uppercase(), M, y + 8f, paint)
            y += 11f
            math(latex, 11f, GRAPHITE)
        }

        /** Words and inline $math$ flowed across lines. */
        fun rich(text: String, size: Float, color: Int, x0: Float, maxW: Float, typeface: Typeface) {
            data class Piece(val w: Float, val asc: Float, val desc: Float, val draw: (Float, Float) -> Unit)
            val pieces = ArrayList<Piece>()
            val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color; textSize = size; this.typeface = typeface }
            text.split('$').forEachIndexed { i, part ->
                if (i % 2 == 1 && part.isNotBlank()) {
                    val b = lay(part, size * 1.05f)
                    pieces += Piece(b.width + size * 0.25f, b.ascent, b.descent) { px, base ->
                        val mp = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = INK; style = Paint.Style.FILL }
                        drawLaid(b, px, base, mp)
                    }
                } else part.split(' ').filter { it.isNotEmpty() }.forEach { word ->
                    val t = "$word "
                    pieces += Piece(measure(t, tp), size * 0.8f, size * 0.25f) { px, base -> text(t, px, base, tp) }
                }
            }
            var i = 0
            while (i < pieces.size) {
                var j = i; var lw = 0f; var asc = size * 0.8f; var desc = size * 0.25f
                while (j < pieces.size && (j == i || lw + pieces[j].w <= maxW)) { lw += pieces[j].w; asc = maxOf(asc, pieces[j].asc); desc = maxOf(desc, pieces[j].desc); j++ }
                room(asc + desc + 3f)
                var px = x0
                val base = y + asc
                for (k in i until j) { pieces[k].draw(px, base); px += pieces[k].w }
                y += asc + desc + 3f
                i = j
            }
        }

        fun step(i: Int, st: Step) {
            room(40f)
            val top = y
            paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.color = TERRACOTTA; paint.textSize = 15f; paint.typeface = Typeface.create(Typeface.SERIF, Typeface.ITALIC)
            text(roman.getOrElse(i) { "${i + 1}" }, M, top + 14f, paint)
            val x = M + 34f; val w = W - M - x
            if (st.title.isNotEmpty()) rich(st.title, 11.5f, INK, x, w, sansMedium)
            if (st.text.isNotEmpty()) rich(st.text, 9.5f, GRAPHITE, x, w, sans)
            if (st.latex.isNotEmpty()) { y += 2f; math(st.latex, 14f, INK, x, w) }
            st.sub.forEach { sub ->
                if (sub.title.isNotEmpty()) rich(sub.title, 9.5f, INK, x + 12f, w - 12f, sansMedium)
                if (sub.text.isNotEmpty()) rich(sub.text, 8.5f, GRAPHITE, x + 12f, w - 12f, sans)
                if (sub.latex.isNotEmpty()) math(sub.latex, 11f, INK, x + 12f, w - 12f)
            }
            y += 6f
            rule()
        }

        fun graph(p: PlotSpec) {
            val gw = W - 2 * M; val gh = 210f
            room(gh + 10f)
            val left = M; val top = y
            val v = p.view
            val x0 = v[0]; val x1 = v[1]; val y0 = v[2]; val y1 = v[3]
            fun sx(x: Double) = left + ((x - x0) / (x1 - x0) * gw).toFloat()
            fun sy(yy: Double) = top + ((y1 - yy) / (y1 - y0) * gh).toFloat()
            paint.reset(); paint.letterSpacing = 0f; paint.color = HAIRLINE; paint.strokeWidth = 0.6f; paint.style = Paint.Style.STROKE
            c.drawRect(left, top, left + gw, top + gh, paint)
            paint.color = STONE; paint.strokeWidth = 0.5f
            if (0.0 in y0..y1) c.drawLine(left, sy(0.0), left + gw, sy(0.0), paint)
            if (0.0 in x0..x1) c.drawLine(sx(0.0), top, sx(0.0), top + gh, paint)
            c.save(); c.clipRect(left, top, left + gw, top + gh)
            val colors = listOf(INK, TERRACOTTA, Color.rgb(0x1F, 0x3F, 0xBF), Color.rgb(0x7C, 0x8A, 0x6E))
            p.curves.filter { it.type != CurveType.Surface }.forEachIndexed { idx, curve ->
                val sampler = CurveSampler(curve)
                val path = Path()
                sampler.sample(x0, x1, y0, y1, gw.toInt() * 2, gh.toInt() * 2).forEach { pl ->
                    path.moveTo(sx(pl[0].toDouble()), sy(pl[1].toDouble()))
                    var k = 2
                    while (k < pl.size) { path.lineTo(sx(pl[k].toDouble()), sy(pl[k + 1].toDouble())); k += 2 }
                }
                sampler.close()
                paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.style = Paint.Style.STROKE; paint.strokeWidth = 1.4f
                paint.strokeCap = Paint.Cap.ROUND; paint.strokeJoin = Paint.Join.ROUND; paint.color = colors[idx % colors.size]
                c.drawPath(path, paint)
            }
            p.points.filter { it.x.isFinite() && it.y.isFinite() }.forEach { pt ->
                paint.reset(); paint.letterSpacing = 0f; paint.isAntiAlias = true; paint.style = Paint.Style.FILL; paint.color = TERRACOTTA
                c.drawCircle(sx(pt.x), sy(pt.y), 2.6f, paint)
            }
            c.restore()
            y += gh + 10f
        }
    }
}
