// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.math

import android.graphics.Paint
import android.util.LruCache
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private object LayoutCache {
    val cache = LruCache<String, Box>(256)
}

enum class MathFit { Scroll, Shrink }

/**
 * Renders LaTeX natively. When the formula is wider than the available space it either
 * scrolls horizontally or shrinks (down to 45%) depending on [fit].
 */
@Composable
fun MathView(
    latex: String,
    modifier: Modifier = Modifier,
    fontSize: TextUnit = 22.sp,
    color: Color = LocalContentColor.current,
    fit: MathFit = MathFit.Scroll,
    center: Boolean = false,
) {
    val context = LocalContext.current
    val density = LocalDensity.current
    val sizePx = with(density) { fontSize.toPx() }
    val box = remember(latex, sizePx) {
        val key = "$sizePx|$latex"
        LayoutCache.cache.get(key) ?: runCatching { MathLayout(MathFonts.get(context)).layout(latex, sizePx) }
            .getOrElse { MathLayout(MathFonts.get(context)).layout("\\text{$latex}", sizePx) }
            .also { LayoutCache.cache.put(key, it) }
    }
    val paint = remember { Paint(Paint.ANTI_ALIAS_FLAG or Paint.SUBPIXEL_TEXT_FLAG) }
    val pad = with(density) { 2.dp.toPx() }
    BoxWithConstraints(modifier, contentAlignment = if (center) Alignment.Center else Alignment.CenterStart) {
        val maxW = constraints.maxWidth.toFloat()
        val maxH = if (constraints.hasBoundedHeight) constraints.maxHeight.toFloat() else Float.POSITIVE_INFINITY
        val naturalW = box.width + 2 * pad
        val naturalH = box.height + 2 * pad
        // shrink to fit both dimensions (tall glyphs like ∫ inside small keys)
        val scale = if (fit == MathFit.Shrink) minOf(1f, if (maxW > 0) maxW / naturalW else 1f, maxH / naturalH).coerceAtLeast(0.3f) else 1f
        val wDp = with(density) { (naturalW * scale).toDp() }
        val hDp = with(density) { ((box.height + 2 * pad) * scale).toDp() }
        val canvas = @Composable {
            Canvas(Modifier.size(wDp, hDp)) {
                paint.color = color.toArgb()
                val c = drawContext.canvas.nativeCanvas
                c.save()
                c.scale(scale, scale)
                box.draw(c, pad, pad + box.ascent, paint)
                c.restore()
            }
        }
        if (fit == MathFit.Scroll && naturalW > maxW) {
            Box(Modifier.horizontalScroll(rememberScrollState())) { canvas() }
        } else canvas()
    }
}

/** Text that may contain inline math between `$...$`. Words wrap naturally. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun RichMathText(
    text: String,
    style: TextStyle,
    color: Color,
    modifier: Modifier = Modifier,
    mathSize: TextUnit = (style.fontSize.value * 1.05f).sp,
) {
    val segments = remember(text) { splitMath(text) }
    FlowRow(modifier, horizontalArrangement = Arrangement.Start, verticalArrangement = Arrangement.Center) {
        segments.forEach { (isMath, s) ->
            if (isMath) {
                MathView(s, fontSize = mathSize, color = color, modifier = Modifier.align(Alignment.CenterVertically))
                Text(" ", style = style, color = color)
            } else {
                s.split(" ").filter { it.isNotEmpty() }.forEach { w ->
                    Text("$w ", style = style, color = color, modifier = Modifier.align(Alignment.CenterVertically))
                }
            }
        }
    }
}

private fun splitMath(text: String): List<Pair<Boolean, String>> {
    val out = mutableListOf<Pair<Boolean, String>>()
    var inMath = false
    val sb = StringBuilder()
    for (c in text) {
        if (c == '$') {
            if (sb.isNotEmpty()) out.add(inMath to sb.toString())
            sb.clear()
            inMath = !inMath
        } else sb.append(c)
    }
    if (sb.isNotEmpty()) out.add(inMath to sb.toString())
    return out
}
