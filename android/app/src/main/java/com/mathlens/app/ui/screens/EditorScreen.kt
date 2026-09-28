// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.foundation.layout.widthIn

import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.History
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.Pill
import com.mathlens.app.ui.components.CircleIconButton
import com.mathlens.app.ui.components.Key
import com.mathlens.app.ui.components.MathKeyboard
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.components.rememberPulse
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground
import kotlinx.coroutines.delay

private val examples = listOf(
    "x^2-5x+6=0", "d/dx(sin(x^2))", "int(x*e^x, x)", "z^3=1", "x+y=5; x-y=1",
    "lim(sin(x)/x, x, 0)", "det([[1,2],[3,4]])", "12!", "(3+2i)/(1-i)", "2x+1>5",
)

@Composable
fun EditorScreen(
    initial: String,
    onBack: () -> Unit,
    onSolve: (String) -> Unit,
    onGraph: (String) -> Unit,
    onHistory: () -> Unit,
) {
    val start = remember(initial) { initial.split(';').joinToString("\n") { it.trim() } }
    var text by remember { mutableStateOf(start) }
    var cursor by remember { mutableStateOf(start.length) }
    var lastLatex by remember { mutableStateOf<String?>(null) }
    var preview by remember { mutableStateOf<String?>(null) }
    var valid by remember { mutableStateOf(true) }
    var layout by remember { mutableStateOf<TextLayoutResult?>(null) }

    LaunchedEffect(text) {
        delay(60)
        val l = runCatching { MathEngine.previewLatex(text.toEngine()) }.getOrNull()
        valid = l != null || text.isBlank()
        if (l != null) { preview = l; lastLatex = l }
        else preview = null
    }

    fun insert(key: Key) {
        val c = cursor.coerceIn(0, text.length)
        text = text.substring(0, c) + key.insert + text.substring(c)
        cursor = c + key.cursor
    }

    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar("Write a problem", onBack) {
            CircleIconButton(Icons.Outlined.History, "History", size = 48.dp, onClick = onHistory)
        }
        Column(Modifier.weight(1f).padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            PaperCard(Modifier.fillMaxWidth().animateContentSize()) {
                Column(Modifier.padding(20.dp).heightIn(min = 150.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                    SectionLabel(if (valid) "As written" else "Keep writing…")
                    val shown = preview ?: lastLatex
                    Box(Modifier.fillMaxWidth().heightIn(min = 64.dp), contentAlignment = Alignment.CenterStart) {
                        if (shown != null) {
                            MathView(
                                shown, fontSize = 30.sp, fit = MathFit.Shrink,
                                color = if (preview != null) Tone.Ink else Tone.Stone,
                            )
                        } else {
                            Column {
                                Text("Begin with the keyboard below", style = MaterialTheme.typography.titleLarge, color = Tone.Stone)
                                Text("For a system, press ↵ to start the next equation on a new line", style = MaterialTheme.typography.bodySmall, color = Tone.Stone)
                            }
                        }
                    }
                    // raw input with a blinking cursor, tap to move it
                    val blink = rememberPulse(0f, 1f, 520)
                    androidx.compose.foundation.layout.BoxWithConstraints(
                        Modifier
                            .fillMaxWidth()
                            .background(Tone.Bone, androidx.compose.foundation.shape.RoundedCornerShape(8.dp))
                            .padding(horizontal = 14.dp, vertical = 12.dp)
                            .animateContentSize(),
                    ) {
                        val boxWidth = maxWidth
                        Text(
                            text.ifEmpty { " " },
                            style = TextStyle(fontFamily = FontFamily.Monospace, fontSize = 18.sp, color = Tone.Graphite),
                            onTextLayout = { layout = it },
                            modifier = Modifier
                                .horizontalScroll(rememberScrollState())
                                // the whole row is tappable, not just the characters: taps right of a short line land at its end
                                .widthIn(min = boxWidth)
                                .pointerInput(text) {
                                    detectTapGestures { pos ->
                                        layout?.let { cursor = it.getOffsetForPosition(pos).coerceIn(0, text.length) }
                                    }
                                }
                                .drawWithContent {
                                    drawContent()
                                    val l = layout ?: return@drawWithContent
                                    val r = l.getCursorRect(cursor.coerceIn(0, text.length))
                                    drawLine(
                                        Tone.Terracotta.copy(alpha = if (blink > 0.5f) 1f else 0f),
                                        Offset(r.left + 1f, r.top), Offset(r.left + 1f, r.bottom), strokeWidth = 5f,
                                    )
                                },
                        )
                    }
                }
            }
            SectionLabel("Or begin with")
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                examples.forEach { ex -> Pill(ex) { text = ex; cursor = ex.length } }
            }
            Spacer(Modifier.weight(1f))
        }
        MathKeyboard(
            text = text,
            cursor = cursor,
            onKey = ::insert,
            onBackspace = {
                val c = cursor.coerceIn(0, text.length)
                if (c > 0) { text = text.removeRange(c - 1, c); cursor = c - 1 }
            },
            onClear = { text = ""; cursor = 0 },
            onUp = { cursor = moveLine(text, cursor, -1) },
            onDown = { cursor = moveLine(text, cursor, +1) },
            onLeft = { cursor = (cursor - 1).coerceAtLeast(0) },
            onRight = { cursor = (cursor + 1).coerceAtMost(text.length) },
            onSolve = { if (text.isNotBlank()) onSolve(text.toEngine()) },
            onGraph = { if (text.isNotBlank()) onGraph(text.toEngine()) },
        )
    }
}

/** Each line of the editor is one equation; the engine separates a system's equations with ';'. */
private fun String.toEngine(): String =
    lines().map { it.trim() }.filter { it.isNotEmpty() }.joinToString("; ")

/** Cursor offset on the previous (-1) or next (+1) line, keeping the column where possible. */
internal fun moveLine(text: String, cursor: Int, dir: Int): Int {
    val c = cursor.coerceIn(0, text.length)
    val lineStart = text.lastIndexOf('\n', c - 1) + 1
    val col = c - lineStart
    return if (dir < 0) {
        if (lineStart == 0) 0
        else {
            val prevStart = text.lastIndexOf('\n', lineStart - 2) + 1
            prevStart + minOf(col, lineStart - 1 - prevStart)
        }
    } else {
        val lineEnd = text.indexOf('\n', c).let { if (it < 0) text.length else it }
        if (lineEnd == text.length) text.length
        else {
            val nextStart = lineEnd + 1
            val nextEnd = text.indexOf('\n', nextStart).let { if (it < 0) text.length else it }
            nextStart + minOf(col, nextEnd - nextStart)
        }
    }
}
