// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.math

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import androidx.core.content.res.ResourcesCompat
import com.mathlens.app.R
import kotlin.math.max

/**
 * A small TeX-like math typesetter. It supports the LaTeX subset emitted by the C++ engine:
 * \frac \sqrt[n] ^ _ \left \right \text \operatorname \mathbb \overline \binom \int \lim
 * matrices (bmatrix, vmatrix, array with |), cases, greek letters and common symbols.
 * Parsing produces [Atom]s; layout turns them into [Box]es measured in pixels which draw themselves.
 */
class MathFonts(context: Context) {
    val main: Typeface = font(context, R.font.katex_main_regular)
    val bold: Typeface = font(context, R.font.katex_main_bold)
    val italic: Typeface = font(context, R.font.katex_math_italic)
    val size1: Typeface = font(context, R.font.katex_size1_regular)
    val size2: Typeface = font(context, R.font.katex_size2_regular)
    val ams: Typeface = font(context, R.font.katex_ams_regular)

    private fun font(c: Context, id: Int) = ResourcesCompat.getFont(c, id) ?: Typeface.DEFAULT

    companion object {
        @Volatile private var instance: MathFonts? = null
        fun get(context: Context): MathFonts = instance ?: synchronized(this) {
            instance ?: MathFonts(context.applicationContext).also { instance = it }
        }
    }
}

// ------------------------------------------------------------------ atoms

enum class AtomClass { Ord, Op, Bin, Rel, Open, Close, Punct, Inner }

sealed class Atom {
    open val cls: AtomClass get() = AtomClass.Ord
}

data class Glyph(val text: String, val font: FontKind, override val cls: AtomClass = AtomClass.Ord) : Atom()
data class Group(val items: List<Atom>) : Atom()
data class Frac(val num: Group, val den: Group, val bar: Boolean = true) : Atom()
data class Sqrt(val body: Group, val index: Group?) : Atom()
data class Scripts(val base: Atom, val sup: Group?, val sub: Group?) : Atom() {
    override val cls: AtomClass get() = base.cls
}
data class Delimited(val left: String, val right: String, val body: Group) : Atom() {
    override val cls: AtomClass get() = AtomClass.Inner
}
data class BigOp(val symbol: String, val limitsBelow: Boolean, val isText: Boolean) : Atom() {
    override val cls: AtomClass get() = AtomClass.Op
}
data class Overline(val body: Group) : Atom()
/** \\hl{...}: what changed since the previous step (soft terracotta backing, terracotta ink). */
data class Highlight(val body: Group) : Atom()
data class Accent(val body: Group, val mark: String) : Atom()
data class Space(val em: Float) : Atom()
data class Matrix(val rows: List<List<Group>>, val env: String, val colSpec: String) : Atom() {
    override val cls: AtomClass get() = AtomClass.Inner
}

enum class FontKind { Main, Italic, Bold, Ams, Text }

// ------------------------------------------------------------------ parser

private object Symbols {
    val greek = mapOf(
        "alpha" to "α", "beta" to "β", "gamma" to "γ", "delta" to "δ", "epsilon" to "ϵ", "theta" to "θ",
        "lambda" to "λ", "mu" to "μ", "pi" to "π", "rho" to "ρ", "sigma" to "σ", "tau" to "τ", "phi" to "φ",
        "omega" to "ω", "Gamma" to "Γ", "Delta" to "Δ", "Theta" to "Θ", "Lambda" to "Λ", "Sigma" to "Σ",
        "Phi" to "Φ", "Omega" to "Ω", "Pi" to "Π",
    )
    val rel = mapOf(
        "le" to "≤", "leq" to "≤", "ge" to "≥", "geq" to "≥", "ne" to "≠", "neq" to "≠", "approx" to "≈",
        "to" to "→", "rightarrow" to "→", "Rightarrow" to "⇒", "iff" to "⇔", "in" to "∈", "equiv" to "≡",
        "mid" to "∣",
    )
    val bin = mapOf(
        "cdot" to "⋅", "times" to "×", "div" to "÷", "pm" to "±", "mp" to "∓", "cup" to "∪", "cap" to "∩",
        "setminus" to "∖", "bmod" to "mod",
    )
    val ord = mapOf(
        "infty" to "∞", "dots" to "…", "ldots" to "…", "cdots" to "⋯", "%" to "%", "partial" to "∂", "nabla" to "∇",
        "{" to "{", "}" to "}", "prime" to "′", "circ" to "∘", "angle" to "∠", "degree" to "°", "square" to "□", "_" to "_",
    )
    val functions = setOf(
        "sin", "cos", "tan", "sec", "csc", "cot", "arcsin", "arccos", "arctan", "sinh", "cosh", "tanh",
        "ln", "log", "exp", "gcd", "det", "arg", "max", "min", "deg",
    )
}

class LatexParser(private val src: String) {
    private var i = 0

    fun parse(): Group = Group(parseList(null))

    private fun peekChar(): Char? = if (i < src.length) src[i] else null

    private fun readCommand(): String {
        // after '\'
        val start = i
        if (i < src.length && !src[i].isLetter()) { i++; return src.substring(start, i) }
        while (i < src.length && src[i].isLetter()) i++
        return src.substring(start, i)
    }

    private fun skipSpaces() { while (i < src.length && src[i] == ' ') i++ }

    private fun readGroupArg(): Group {
        skipSpaces()
        if (i >= src.length) return Group(emptyList())
        if (src[i] == '{') {
            i++
            val items = parseList('}')
            return Group(items)
        }
        // single token argument
        val one = parseAtom() ?: return Group(emptyList())
        return Group(listOf(one))
    }

    private fun readRawBraced(): String {
        skipSpaces()
        if (i >= src.length || src[i] != '{') return ""
        i++
        var depth = 1
        val sb = StringBuilder()
        while (i < src.length) {
            val c = src[i++]
            if (c == '{') depth++
            if (c == '}') { depth--; if (depth == 0) break }
            sb.append(c)
        }
        return sb.toString()
    }

    private fun readDelimiter(): String {
        skipSpaces()
        if (i >= src.length) return "."
        val c = src[i]
        if (c == '\\') {
            i++
            return when (val cmd = readCommand()) {
                "{" -> "{"
                "}" -> "}"
                "|" -> "‖"
                "lfloor" -> "⌊"
                "rfloor" -> "⌋"
                "lceil" -> "⌈"
                "rceil" -> "⌉"
                "langle" -> "⟨"
                "rangle" -> "⟩"
                else -> cmd
            }
        }
        i++
        return c.toString()
    }

    /** Parse until [end] char (consumed) or end of input, or \right / \end / & / \\ when stopping inside environments. */
    private fun parseList(end: Char?, stopAtRight: Boolean = false, stopAtCell: Boolean = false): MutableList<Atom> {
        val out = mutableListOf<Atom>()
        while (i < src.length) {
            val c = src[i]
            if (end != null && c == end) { i++; return out }
            if (stopAtRight && src.startsWith("\\right", i)) return out
            if (stopAtCell && (c == '&' || src.startsWith("\\\\", i) || src.startsWith("\\end", i))) return out
            if (c == '\'') {
                // prime: a small raised mark attached to the previous atom, as in TeX
                i++
                val prime = Group(listOf(Glyph("\u2032", FontKind.Main)))
                val base = if (out.isNotEmpty()) out.removeAt(out.lastIndex) else Group(emptyList())
                out.add(if (base is Scripts && base.sup != null) base.copy(sup = Group(base.sup.items + prime.items)) else if (base is Scripts) base.copy(sup = prime) else Scripts(base, prime, null))
                continue
            }
            if (c == '^' || c == '_') {
                i++
                val arg = readGroupArg()
                val base = if (out.isNotEmpty()) out.removeAt(out.lastIndex) else Group(emptyList())
                val merged = if (base is Scripts) {
                    if (c == '^') base.copy(sup = arg) else base.copy(sub = arg)
                } else {
                    if (c == '^') Scripts(base, arg, null) else Scripts(base, null, arg)
                }
                out.add(merged)
                continue
            }
            val a = parseAtom() ?: continue
            out.add(a)
        }
        return out
    }

    private fun parseAtom(): Atom? {
        if (i >= src.length) return null
        val c = src[i]
        when {
            c == ' ' -> { i++; return null }
            c == '{' -> { i++; return Group(parseList('}')) }
            c == '}' -> { i++; return null }
            c.isDigit() || c == '.' -> {
                val start = i
                while (i < src.length && (src[i].isDigit() || src[i] == '.')) i++
                return Glyph(src.substring(start, i), FontKind.Main)
            }
            c.isLetter() -> { i++; return Glyph(c.toString(), FontKind.Italic) }
            c == '\\' -> { i++; return parseCommand(readCommand()) }
            c == '+' || c == '-' || c == '*' -> {
                i++
                return Glyph(if (c == '-') "−" else if (c == '*') "∗" else "+", FontKind.Main, AtomClass.Bin)
            }
            c == '=' || c == '<' || c == '>' -> { i++; return Glyph(c.toString(), FontKind.Main, AtomClass.Rel) }
            c == '(' || c == '[' -> { i++; return Glyph(c.toString(), FontKind.Main, AtomClass.Open) }
            c == ')' || c == ']' -> { i++; return Glyph(c.toString(), FontKind.Main, AtomClass.Close) }
            c == ',' || c == ';' -> { i++; return Glyph(c.toString(), FontKind.Main, AtomClass.Punct) }
            c == '|' -> { i++; return Glyph("|", FontKind.Main) }
            c == '!' || c == '\'' || c == '/' || c == ':' -> { i++; return Glyph(if (c == '\'') "′" else c.toString(), FontKind.Main) }
            c == '&' -> { i++; return null }
            else -> { i++; return Glyph(c.toString(), FontKind.Main) }
        }
    }

    private fun parseCommand(cmd: String): Atom? {
        Symbols.greek[cmd]?.let { return Glyph(it, if (cmd[0].isUpperCase()) FontKind.Main else FontKind.Italic) }
        Symbols.rel[cmd]?.let { return Glyph(it, FontKind.Main, AtomClass.Rel) }
        Symbols.bin[cmd]?.let { return Glyph(it, FontKind.Main, AtomClass.Bin) }
        Symbols.ord[cmd]?.let { return Glyph(it, FontKind.Main) }
        if (cmd in Symbols.functions) return Glyph(cmd, FontKind.Main, AtomClass.Op)
        return when (cmd) {
            "frac", "dfrac", "tfrac" -> Frac(readGroupArg(), readGroupArg())
            "hl" -> Highlight(readGroupArg())
            "binom" -> Delimited("(", ")", Group(listOf(Frac(readGroupArg(), readGroupArg(), bar = false))))
            "sqrt" -> {
                skipSpaces()
                var index: Group? = null
                if (i < src.length && src[i] == '[') {
                    i++
                    index = Group(parseList(']'))
                }
                Sqrt(readGroupArg(), index)
            }
            "left" -> {
                val l = readDelimiter()
                val body = parseList(null, stopAtRight = true)
                var r = "."
                if (src.startsWith("\\right", i)) { i += 6; r = readDelimiter() }
                Delimited(l, r, Group(body))
            }
            "right" -> { readDelimiter(); null }
            "text", "mathrm", "textrm" -> Glyph(readRawBraced(), FontKind.Text)
            "operatorname" -> Glyph(readRawBraced(), FontKind.Main, AtomClass.Op)
            "mathbb" -> Glyph(readRawBraced(), FontKind.Ams)
            "mathbf" -> Glyph(readRawBraced(), FontKind.Bold)
            "overline", "bar" -> Overline(readGroupArg())
            "tilde", "widetilde" -> Accent(readGroupArg(), "~")
            "hat", "widehat" -> Accent(readGroupArg(), "^")
            "vec" -> Accent(readGroupArg(), "→")
            "dot" -> Accent(readGroupArg(), "˙")
            "int" -> BigOp("∫", limitsBelow = false, isText = false)
            "sum" -> BigOp("∑", limitsBelow = true, isText = false)
            "prod" -> BigOp("∏", limitsBelow = true, isText = false)
            "lim" -> BigOp("lim", limitsBelow = true, isText = true)
            "quad" -> Space(1f)
            "qquad" -> Space(2f)
            "," -> Space(3f / 18f)
            ":" , ">" -> Space(4f / 18f)
            ";" -> Space(5f / 18f)
            "!" -> Space(-3f / 18f)
            " " -> Space(0.33f)
            "\\" -> Space(1f)
            "begin" -> parseEnvironment(readRawBraced())
            "end" -> { readRawBraced(); null }
            "lfloor" -> Glyph("⌊", FontKind.Main, AtomClass.Open)
            "rfloor" -> Glyph("⌋", FontKind.Main, AtomClass.Close)
            "lceil" -> Glyph("⌈", FontKind.Main, AtomClass.Open)
            "rceil" -> Glyph("⌉", FontKind.Main, AtomClass.Close)
            "vert", "|" -> Glyph("|", FontKind.Main)
            else -> Glyph(cmd, FontKind.Main)
        }
    }

    private fun parseEnvironment(env: String): Atom {
        var spec = ""
        if (env == "array") spec = readRawBraced()
        val rows = mutableListOf<MutableList<Group>>()
        var row = mutableListOf<Group>()
        while (i < src.length) {
            val cell = parseList(null, stopAtCell = true)
            row.add(Group(cell))
            if (i >= src.length) break
            when {
                src[i] == '&' -> i++
                src.startsWith("\\\\", i) -> { i += 2; rows.add(row); row = mutableListOf() }
                src.startsWith("\\end", i) -> { i += 4; readRawBraced(); break }
            }
        }
        if (row.isNotEmpty()) rows.add(row)
        return Matrix(rows, env, spec)
    }
}

// ------------------------------------------------------------------ layout

/** Laid out box. Coordinates: x grows right, baseline at y = 0, ascent above, descent below. */
abstract class Box {
    abstract val width: Float
    abstract val ascent: Float
    abstract val descent: Float
    val height get() = ascent + descent
    abstract fun draw(canvas: Canvas, x: Float, y: Float, paint: Paint)
}

private class TextBox(val text: String, val typeface: Typeface, val size: Float, italicCorrection: Float = 0f) : Box() {
    private val metricsPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.typeface = this@TextBox.typeface; textSize = size }
    override val width: Float = metricsPaint.measureText(text) + italicCorrection
    override val ascent: Float
    override val descent: Float

    init {
        val b = android.graphics.Rect()
        metricsPaint.getTextBounds(text, 0, text.length, b)
        // use font metrics for stable line heights, but tight bounds for tall glyphs
        ascent = max(-b.top.toFloat(), size * 0.68f)
        descent = max(b.bottom.toFloat(), size * 0.02f)
    }

    override fun draw(canvas: Canvas, x: Float, y: Float, paint: Paint) {
        paint.typeface = typeface
        paint.textSize = size
        paint.style = Paint.Style.FILL
        canvas.drawText(text, x, y, paint)
    }
}

private class HBox(val children: List<Pair<Float, Box>>, override val width: Float) : Box() {
    override val ascent: Float = children.maxOfOrNull { it.second.ascent } ?: 0f
    override val descent: Float = children.maxOfOrNull { it.second.descent } ?: 0f
    override fun draw(canvas: Canvas, x: Float, y: Float, paint: Paint) {
        for ((dx, b) in children) b.draw(canvas, x + dx, y, paint)
    }
}

private class Shifted(val inner: Box, val dx: Float, val dy: Float, override val width: Float) : Box() {
    override val ascent: Float = inner.ascent - dy
    override val descent: Float = inner.descent + dy
    override fun draw(canvas: Canvas, x: Float, y: Float, paint: Paint) = inner.draw(canvas, x + dx, y + dy, paint)
}

private class EmptyBox(override val width: Float, override val ascent: Float = 0f, override val descent: Float = 0f) : Box() {
    override fun draw(canvas: Canvas, x: Float, y: Float, paint: Paint) {}
}

/** Generic box composed of positioned children plus custom path drawing. */
private class Composite(
    override val width: Float,
    override val ascent: Float,
    override val descent: Float,
    val parts: List<Triple<Float, Float, Box>>,
    val extra: (Canvas, Float, Float, Paint) -> Unit = { _, _, _, _ -> },
) : Box() {
    override fun draw(canvas: Canvas, x: Float, y: Float, paint: Paint) {
        for ((dx, dy, b) in parts) b.draw(canvas, x + dx, y + dy, paint)
        extra(canvas, x, y, paint)
    }
}

private const val HIGHLIGHT_INK = 0xFFB4533A.toInt()   // Tone.Terracotta
private const val HIGHLIGHT_BG = 0x24B4533A            // Terracotta at 14%

class MathLayout(private val fonts: MathFonts) {

    fun layout(latex: String, sizePx: Float): Box = layoutGroup(LatexParser(latex).parse(), sizePx, 0)

    private fun typefaceFor(kind: FontKind): Typeface = when (kind) {
        FontKind.Main -> fonts.main
        FontKind.Italic -> fonts.italic
        FontKind.Bold -> fonts.bold
        FontKind.Ams -> fonts.ams
        FontKind.Text -> fonts.main
    }

    private fun spacing(prev: AtomClass?, cur: AtomClass, size: Float, depth: Int): Float {
        if (prev == null) return 0f
        val mu = size / 18f
        val script = depth > 0
        return when {
            cur == AtomClass.Bin || prev == AtomClass.Bin -> if (script) 0f else 4 * mu
            cur == AtomClass.Rel || prev == AtomClass.Rel -> if (script) mu else 5 * mu
            prev == AtomClass.Punct -> if (script) 0f else 3 * mu
            prev == AtomClass.Op && cur == AtomClass.Ord -> 2 * mu
            prev == AtomClass.Op && cur == AtomClass.Op -> 3 * mu
            cur == AtomClass.Op && (prev == AtomClass.Ord || prev == AtomClass.Close || prev == AtomClass.Inner) -> 3 * mu
            cur == AtomClass.Inner && prev == AtomClass.Ord -> mu
            prev == AtomClass.Inner && cur == AtomClass.Ord -> mu
            else -> 0f
        }
    }

    private fun layoutGroup(g: Group, size: Float, depth: Int): Box {
        val parts = mutableListOf<Pair<Float, Box>>()
        var x = 0f
        var prev: AtomClass? = null
        g.items.forEachIndexed { idx, atom ->
            var cls = atom.cls
            // unary minus / plus: a binary operator at the start or after an operator acts as ordinary
            if (cls == AtomClass.Bin && (prev == null || prev == AtomClass.Bin || prev == AtomClass.Rel || prev == AtomClass.Open ||
                    prev == AtomClass.Punct || prev == AtomClass.Op)) cls = AtomClass.Ord
            val box = layoutAtom(atom, size, depth, followsBigOp = idx > 0 && g.items[idx - 1] is BigOp)
            x += spacing(prev, cls, size, depth)
            parts.add(x to box)
            x += box.width
            prev = cls
        }
        return HBox(parts, x)
    }

    private fun layoutAtom(a: Atom, size: Float, depth: Int, followsBigOp: Boolean = false): Box = when (a) {
        is Glyph -> glyph(a, size)
        is Group -> layoutGroup(a, size, depth)
        is Space -> EmptyBox(a.em * size)
        is Frac -> frac(a, size, depth)
        is Sqrt -> sqrt(a, size, depth)
        is Scripts -> scripts(a, size, depth)
        is Delimited -> delimited(a, size, depth)
        is BigOp -> bigOp(a, size)
        is Overline -> overline(a, size, depth)
        is Highlight -> highlight(a, size, depth)
        is Accent -> accent(a, size, depth)
        is Matrix -> matrix(a, size, depth)
    }

    private fun glyph(g: Glyph, size: Float): Box {
        val tf = typefaceFor(g.font)
        return when {
            g.font == FontKind.Text -> TextBox(g.text, fonts.main, size)
            g.font == FontKind.Italic -> TextBox(g.text, tf, size, italicCorrection = size * 0.03f)
            else -> TextBox(g.text, tf, size)
        }
    }

    private fun axis(size: Float) = size * 0.25f
    private fun rule(size: Float) = max(1.5f, size * 0.05f)

    private fun frac(f: Frac, size: Float, depth: Int): Box {
        val inner = if (depth >= 1) max(size * 0.8f, 10f) else size * 0.92f
        val n = layoutGroup(f.num, inner, depth + 1)
        val d = layoutGroup(f.den, inner, depth + 1)
        val pad = size * 0.12f
        val w = max(n.width, d.width) + 2 * pad
        val gap = size * 0.14f
        val t = rule(size)
        val ax = axis(size)
        val numShift = -(ax + t / 2 + gap + n.descent)
        val denShift = -ax + t / 2 + gap + d.ascent
        val ascent = -numShift + n.ascent
        val descent = denShift + d.descent
        return Composite(
            w, ascent, descent,
            listOf(Triple((w - n.width) / 2, numShift, n), Triple((w - d.width) / 2, denShift, d)),
        ) { c, x, y, p ->
            if (f.bar) {
                p.style = Paint.Style.FILL
                c.drawRect(x + pad * 0.4f, y - ax - t / 2, x + w - pad * 0.4f, y - ax + t / 2, p)
            }
        }
    }

    private fun sqrt(s: Sqrt, size: Float, depth: Int): Box {
        val body = layoutGroup(s.body, size, depth)
        val t = rule(size)
        val gap = size * 0.16f
        val ascent = body.ascent + gap + t
        val descent = max(body.descent, size * 0.1f) + size * 0.05f
        val hookW = size * 0.55f
        val idx = s.index?.let { layoutGroup(it, size * 0.55f, depth + 2) }
        val idxShift = idx?.let { max(0f, it.width - hookW * 0.5f) } ?: 0f
        val left = idxShift + hookW
        val w = left + body.width + size * 0.1f
        val parts = mutableListOf(Triple(left, 0f, body))
        if (idx != null) parts.add(Triple(0f, -ascent * 0.45f, idx))
        return Composite(w, ascent, descent, parts) { c, x, y, p ->
            val path = Path()
            val top = y - ascent + t / 2
            val bottom = y + descent
            val x0 = x + idxShift
            path.moveTo(x0, y - ascent * 0.35f)
            path.lineTo(x0 + hookW * 0.22f, y - ascent * 0.45f)
            path.lineTo(x0 + hookW * 0.5f, bottom)
            path.lineTo(x0 + hookW * 0.95f, top)
            path.lineTo(x + w, top)
            p.style = Paint.Style.STROKE
            p.strokeWidth = t
            p.strokeJoin = Paint.Join.ROUND
            p.strokeCap = Paint.Cap.ROUND
            c.drawPath(path, p)
            p.style = Paint.Style.FILL
        }
    }

    private fun scripts(s: Scripts, size: Float, depth: Int): Box {
        val base = s.base
        if (base is BigOp && base.limitsBelow) return limitsOp(base, s, size, depth)
        val b = layoutAtom(base, size, depth)
        val ss = max(size * 0.68f, 9f)
        val sup = s.sup?.let { layoutGroup(it, ss, depth + 1) }
        val sub = s.sub?.let { layoutGroup(it, ss, depth + 1) }
        val isInt = base is BigOp
        // TeX-like placement: simple glyphs use fixed shifts, tall bases shift relative to their extent
        val simple = base is Glyph
        var supShift = if (simple) -size * 0.34f else -max(size * 0.36f, b.ascent - ss * 0.42f)
        var subShift = if (simple) size * 0.2f else max(size * 0.2f, b.descent + ss * 0.05f)
        if (isInt) {
            supShift = -b.ascent + (sup?.ascent ?: 0f) * 0.4f
            subShift = b.descent
        }
        if (sup != null && sub != null) {
            val minGap = size * 0.12f
            val gap = (subShift - sub.ascent) - (supShift + sup.descent)
            if (gap < minGap) { supShift -= (minGap - gap) / 2; subShift += (minGap - gap) / 2 }
        }
        val kern = if (isInt) size * 0.1f else size * 0.04f
        val supX = b.width + if (simple && !isInt) -size * 0.02f else kern
        val subX = b.width + (if (isInt) -size * 0.05f else kern * 0.3f)
        val w = b.width + max(sup?.width?.plus(kern) ?: 0f, sub?.width?.plus(kern) ?: 0f) + size * 0.03f
        val parts = mutableListOf(Triple(0f, 0f, b))
        sup?.let { parts.add(Triple(supX, supShift, it)) }
        sub?.let { parts.add(Triple(subX, subShift, it)) }
        val ascent = max(b.ascent, sup?.let { -supShift + it.ascent } ?: 0f)
        val descent = max(b.descent, sub?.let { subShift + it.descent } ?: 0f)
        return Composite(w, ascent, descent, parts)
    }

    private fun limitsOp(op: BigOp, s: Scripts, size: Float, depth: Int): Box {
        val b = bigOp(op, size)
        val ss = max(size * 0.68f, 9f)
        val sub = s.sub?.let { layoutGroup(it, ss, depth + 1) }
        val sup = s.sup?.let { layoutGroup(it, ss, depth + 1) }
        val w = maxOf(b.width, sub?.width ?: 0f, sup?.width ?: 0f)
        val gap = size * 0.12f
        val parts = mutableListOf(Triple((w - b.width) / 2, 0f, b))
        var descent = b.descent
        var ascent = b.ascent
        sub?.let {
            val dy = b.descent + gap + it.ascent
            parts.add(Triple((w - it.width) / 2, dy, it))
            descent = dy + it.descent
        }
        sup?.let {
            val dy = -(b.ascent + gap + it.descent)
            parts.add(Triple((w - it.width) / 2, dy, it))
            ascent = -dy + it.ascent
        }
        return Composite(w + size * 0.1f, ascent, descent, parts)
    }

    private fun bigOp(op: BigOp, size: Float): Box {
        if (op.isText) return TextBox(op.symbol, fonts.main, size)
        val s = size * 1.25f
        val tb = TextBox(op.symbol, fonts.size2, s)
        // center on the math axis
        val shift = (tb.ascent - tb.descent) / 2 - axis(size)
        return Shifted(tb, size * 0.05f, shift, tb.width + size * 0.12f)
    }

    private fun highlight(h: Highlight, size: Float, depth: Int): Box {
        val b = layoutGroup(h.body, size, depth)
        val padX = size * 0.12f
        val padY = size * 0.08f
        return Composite(b.width + 2 * padX, b.ascent + padY, b.descent + padY, emptyList()) { c, x, y, p ->
            val keep = p.color
            val style = p.style
            p.style = Paint.Style.FILL
            p.color = HIGHLIGHT_BG
            c.drawRoundRect(x, y - b.ascent - padY, x + b.width + 2 * padX, y + b.descent + padY, size * 0.18f, size * 0.18f, p)
            p.color = HIGHLIGHT_INK
            b.draw(c, x + padX, y, p)
            p.color = keep
            p.style = style
        }
    }

    private fun overline(o: Overline, size: Float, depth: Int): Box {
        val b = layoutGroup(o.body, size, depth)
        val t = rule(size)
        val gap = size * 0.12f
        return Composite(b.width, b.ascent + gap + t, b.descent, listOf(Triple(0f, 0f, b))) { c, x, y, p ->
            c.drawRect(x, y - b.ascent - gap - t, x + b.width, y - b.ascent - gap, p)
        }
    }

    private fun accent(a: Accent, size: Float, depth: Int): Box {
        val b = layoutGroup(a.body, size, depth)
        val gap = size * 0.1f
        val markH = size * 0.22f
        return Composite(b.width, b.ascent + gap + markH, b.descent, listOf(Triple(0f, 0f, b))) { c, x, y, p ->
            val cx = x + b.width / 2 + size * 0.04f
            val top = y - b.ascent - gap - markH
            val w = kotlin.math.max(size * 0.32f, b.width * 0.7f) / 2
            val path = Path()
            p.style = Paint.Style.STROKE
            p.strokeWidth = rule(size)
            p.strokeCap = Paint.Cap.ROUND
            when (a.mark) {
                "~" -> { path.moveTo(cx - w, top + markH * 0.6f); path.cubicTo(cx - w / 2, top, cx, top + markH, cx + w, top + markH * 0.3f) }
                "^" -> { path.moveTo(cx - w, top + markH); path.lineTo(cx, top); path.lineTo(cx + w, top + markH) }
                "→" -> { path.moveTo(cx - w, top + markH / 2); path.lineTo(cx + w, top + markH / 2); path.moveTo(cx + w * 0.6f, top); path.lineTo(cx + w, top + markH / 2); path.lineTo(cx + w * 0.6f, top + markH) }
                else -> { c.drawCircle(cx, top + markH / 2, rule(size) * 1.2f, p.apply { style = Paint.Style.FILL }) }
            }
            c.drawPath(path, p)
            p.style = Paint.Style.FILL
        }
    }

    private fun delimited(d: Delimited, size: Float, depth: Int): Box {
        val body = layoutGroup(d.body, size, depth)
        val ax = axis(size)
        // symmetric around the axis
        val half = max(max(body.ascent - ax, body.descent + ax), size * 0.5f) + size * 0.08f
        val ascent = ax + half
        val descent = half - ax
        val lw = delimWidth(d.left, half * 2, size)
        val rw = delimWidth(d.right, half * 2, size)
        val w = lw + body.width + rw
        return Composite(w, ascent, descent, listOf(Triple(lw, 0f, body))) { c, x, y, p ->
            drawDelimiter(c, d.left, x, y - ascent, lw, ascent + descent, size, p, left = true)
            drawDelimiter(c, d.right, x + lw + body.width, y - ascent, rw, ascent + descent, size, p, left = false)
        }
    }

    private fun delimWidth(del: String, h: Float, size: Float): Float = when (del) {
        "." -> size * 0.05f
        "|", "‖" -> size * 0.3f
        "{", "}" -> size * 0.5f
        else -> size * (0.32f + 0.05f * (h / size - 1).coerceIn(0f, 3f))
    }

    private fun drawDelimiter(c: Canvas, del: String, x: Float, top: Float, w: Float, h: Float, size: Float, p: Paint, left: Boolean) {
        if (del == ".") return
        val t = rule(size) * 1.05f
        p.style = Paint.Style.STROKE
        p.strokeWidth = t
        p.strokeCap = Paint.Cap.ROUND
        p.strokeJoin = Paint.Join.ROUND
        val path = Path()
        val bottom = top + h
        val inset = w * 0.22f
        when (del) {
            "(", ")" -> {
                val outer = if (del == "(") x + w - inset else x + inset
                val inner = if (del == "(") x + inset else x + w - inset
                path.moveTo(outer, top + t)
                path.cubicTo(inner, top + h * 0.2f, inner, bottom - h * 0.2f, outer, bottom - t)
            }
            "[", "]", "⌊", "⌋", "⌈", "⌉" -> {
                val isLeft = del == "[" || del == "⌊" || del == "⌈"
                val stem = if (isLeft) x + inset else x + w - inset
                val end = if (isLeft) x + w - inset else x + inset
                val topBar = del == "[" || del == "]" || del == "⌈" || del == "⌉"
                val botBar = del == "[" || del == "]" || del == "⌊" || del == "⌋"
                if (topBar) { path.moveTo(end, top + t / 2); path.lineTo(stem, top + t / 2) } else path.moveTo(stem, top + t / 2)
                path.lineTo(stem, bottom - t / 2)
                if (botBar) path.lineTo(end, bottom - t / 2)
            }
            "|" -> { path.moveTo(x + w / 2, top); path.lineTo(x + w / 2, bottom) }
            "‖" -> {
                path.moveTo(x + w * 0.35f, top); path.lineTo(x + w * 0.35f, bottom)
                path.moveTo(x + w * 0.65f, top); path.lineTo(x + w * 0.65f, bottom)
            }
            "{", "}" -> {
                val dir = if (del == "{") 1f else -1f
                val xs = if (del == "{") x + w - inset else x + inset
                val xm = if (del == "{") x + w / 2 else x + w / 2
                val xp = if (del == "{") x + inset else x + w - inset
                val mid = top + h / 2
                path.moveTo(xs, top + t / 2)
                path.quadTo(xm, top + t / 2, xm, top + h * 0.12f)
                path.lineTo(xm, mid - h * 0.08f)
                path.quadTo(xm, mid, xp, mid)
                path.quadTo(xm, mid, xm, mid + h * 0.08f)
                path.lineTo(xm, bottom - h * 0.12f)
                path.quadTo(xm, bottom - t / 2, xs, bottom - t / 2)
                @Suppress("UNUSED_VARIABLE") val unused = dir
            }
            "⟨", "⟩" -> {
                val a = if (del == "⟨") x + w - inset else x + inset
                val b = if (del == "⟨") x + inset else x + w - inset
                path.moveTo(a, top); path.lineTo(b, top + h / 2); path.lineTo(a, bottom)
            }
            else -> {
                p.style = Paint.Style.FILL
                p.typeface = fonts.main
                p.textSize = size
                c.drawText(del, x, top + h * 0.75f, p)
                return
            }
        }
        c.drawPath(path, p)
        p.style = Paint.Style.FILL
    }

    private fun matrix(m: Matrix, size: Float, depth: Int): Box {
        val cellSize = if (depth > 0) size * 0.85f else size
        val cells = m.rows.map { r -> r.map { layoutGroup(it, cellSize, depth) } }  // keep normal operator spacing in cells
        val nCols = cells.maxOfOrNull { it.size } ?: 0
        val colW = FloatArray(nCols)
        for (r in cells) r.forEachIndexed { j, b -> colW[j] = max(colW[j], b.width) }
        val rowGap = size * 0.35f
        val colGap = if (m.env == "cases") size * 1.2f else size * 0.9f
        val rowAsc = cells.map { r -> r.maxOfOrNull { it.ascent } ?: size * 0.7f }
        val rowDesc = cells.map { r -> r.maxOfOrNull { it.descent } ?: size * 0.2f }
        val totalH = rowAsc.sum() + rowDesc.sum() + rowGap * (cells.size - 1).coerceAtLeast(0)
        val bars = mutableSetOf<Int>()  // column indices preceded by a vertical bar
        run {
            var col = 0
            for (ch in m.colSpec) { if (ch == '|') bars.add(col) else if (ch.isLetter()) col++ }
        }
        val innerW = colW.sum() + colGap * (nCols - 1).coerceAtLeast(0)
        val parts = mutableListOf<Triple<Float, Float, Box>>()
        val ax = axis(size)
        val top = -(totalH / 2 + ax)
        var y = top
        val alignLeft = m.env == "cases"
        cells.forEachIndexed { i, r ->
            y += rowAsc[i]
            var x = 0f
            r.forEachIndexed { j, b ->
                val dx = if (alignLeft) x else x + (colW[j] - b.width) / 2
                parts.add(Triple(dx, y, b))
                x += colW[j] + colGap
            }
            y += rowDesc[i] + rowGap
        }
        val body = Composite(innerW, -top, totalH + top, parts) { c, x0, y0, p ->
            for (col in bars) {
                if (col == 0 || col >= nCols) continue
                var bx = x0
                for (j in 0 until col) bx += colW[j] + colGap
                bx -= colGap / 2
                p.strokeWidth = rule(size)
                c.drawLine(bx, y0 + top, bx, y0 + top + totalH, p)
            }
        }
        val (l, rDel) = when (m.env) {
            "bmatrix" -> "[" to "]"
            "pmatrix" -> "(" to ")"
            "vmatrix" -> "|" to "|"
            "cases" -> "{" to "."
            else -> "." to "."
        }
        if (l == "." && rDel == ".") return body
        val pad = size * 0.15f
        val bodyPadded = Composite(body.width + 2 * pad, body.ascent, body.descent, listOf(Triple(pad, 0f, body)))
        val half = totalH / 2 + size * 0.1f
        val ascent = ax + half
        val descent = half - ax
        val lw = delimWidth(l, half * 2, size)
        val rw = delimWidth(rDel, half * 2, size)
        return Composite(lw + bodyPadded.width + rw, ascent, descent, listOf(Triple(lw, 0f, bodyPadded))) { c, x, yy, p ->
            drawDelimiter(c, l, x, yy - ascent, lw, ascent + descent, size, p, true)
            drawDelimiter(c, rDel, x + lw + bodyPadded.width, yy - ascent, rw, ascent + descent, size, p, false)
        }
    }
}
