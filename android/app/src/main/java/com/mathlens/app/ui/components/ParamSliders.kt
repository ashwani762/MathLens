// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.ui.theme.Tone

private val SIDE = 40.dp

/** One slider per graph parameter (a, b, k, ...): -5 to 5 in steps of 0.1. */
@Composable
fun ParamSliders(params: List<String>, values: Map<String, Double>, onChange: (String, Double) -> Unit, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
        params.forEach { name ->
            val v = values[name] ?: 1.0
            Row(verticalAlignment = Alignment.CenterVertically) {
                // equal side columns keep the track centred under the graph, edge to edge with the card
                Text(name, style = MaterialTheme.typography.titleLarge.copy(fontStyle = FontStyle.Italic, fontSize = 22.sp), color = Tone.Ink,
                    textAlign = TextAlign.Start, modifier = Modifier.width(SIDE))
                HairlineSlider(v, -5.0, 5.0, onChange = { onChange(name, it) }, modifier = Modifier.weight(1f))
                Text(formatCoord(v), style = MaterialTheme.typography.labelLarge, color = Tone.Graphite,
                    textAlign = TextAlign.End, modifier = Modifier.width(SIDE))
            }
        }
    }
}

/**
 * Gallery-style slider: a hairline track with a tick at zero, a terracotta stroke from zero to the
 * value, and an ink dot for the thumb. Tap or drag anywhere on the row; values snap to 0.1.
 */
@Composable
fun HairlineSlider(value: Double, min: Double, max: Double, onChange: (Double) -> Unit, modifier: Modifier = Modifier) {
    val current = rememberUpdatedState(onChange)
    val density = androidx.compose.ui.platform.LocalDensity.current
    val padPx = with(density) { 9.dp.toPx() }  // the thumb's radius: at the ends the dot sits flush with the track
    fun valueAt(x: Float, width: Float): Double {
        val pad = padPx
        val t = ((x - pad) / (width - 2 * pad)).coerceIn(0f, 1f)
        return Math.round((min + (max - min) * t) * 10) / 10.0
    }
    Canvas(
        modifier
            .height(44.dp)
            .pointerInput(min, max) { detectTapGestures { current.value(valueAt(it.x, size.width.toFloat())) } }
            .pointerInput(min, max) {
                detectHorizontalDragGestures { change, _ ->
                    change.consume()
                    current.value(valueAt(change.position.x, size.width.toFloat()))
                }
            },
    ) {
        val pad = padPx
        val y = size.height / 2
        fun sx(v: Double) = pad + ((v - min) / (max - min)).toFloat() * (size.width - 2 * pad)
        drawLine(Tone.Hairline, Offset(pad, y), Offset(size.width - pad, y), 2.dp.toPx(), StrokeCap.Round)
        if (0.0 in min..max) drawLine(Tone.Stone, Offset(sx(0.0), y - 6.dp.toPx()), Offset(sx(0.0), y + 6.dp.toPx()), 1.dp.toPx())
        drawLine(Tone.Terracotta, Offset(sx(0.0.coerceIn(min, max)), y), Offset(sx(value), y), 2.5.dp.toPx(), StrokeCap.Round)
        drawCircle(Tone.Ink, 8.dp.toPx(), Offset(sx(value), y))
        drawCircle(Tone.Ivory, 2.5.dp.toPx(), Offset(sx(value), y))
    }
}
