// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.material.icons.outlined.Collections

import androidx.compose.material.icons.outlined.BookmarkBorder

import androidx.compose.material.icons.filled.Bookmark

import androidx.compose.runtime.setValue

import androidx.compose.runtime.getValue

import androidx.compose.runtime.mutableStateOf

import com.mathlens.app.ui.components.ParamSliders

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Wallpaper
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.CircleIconButton
import com.mathlens.app.ui.components.GradientButton
import com.mathlens.app.ui.components.GraphView
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground

@Composable
fun GraphScreen(
    equations: List<String>,
    onBack: () -> Unit,
    onAdd: () -> Unit,
    onRemove: (Int) -> Unit,
    onWallpaper: () -> Unit,
    isSaved: Boolean = false,
    onSave: () -> Unit = {},
    onOpenSaved: () -> Unit = {},
) {
    val result = remember(equations) { MathEngine.graph(equations.joinToString("; ")) }
    val params = result.getOrNull()?.params.orEmpty()
    var paramValues by remember(result) { mutableStateOf(params.associateWith { 1.0 }) }
    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar("Graph", onBack) {
            CircleIconButton(if (isSaved) Icons.Filled.Bookmark else Icons.Outlined.BookmarkBorder, if (isSaved) "Saved" else "Save graph", size = 48.dp,
                tint = if (isSaved) Tone.Terracotta else Tone.Ink, onClick = onSave)
            CircleIconButton(Icons.Outlined.Collections, "Saved graphs", size = 48.dp, onClick = onOpenSaved)
            CircleIconButton(Icons.Outlined.Wallpaper, "Make wallpaper", size = 48.dp, onClick = onWallpaper)
        }
        Box(Modifier.weight(1f).fillMaxWidth().padding(horizontal = 12.dp)) {
            PaperCard(Modifier.fillMaxSize()) {
                result.fold(
                    onSuccess = { spec -> GraphView(spec, Modifier.fillMaxSize(), paramValues = paramValues, showMarkers = true) },
                    onFailure = {
                        Text(it.message ?: "Can't graph this", color = Tone.Error, modifier = Modifier.align(Alignment.Center).padding(24.dp))
                    },
                )
            }
            Text(
                "Pinch to zoom · drag to pan · long-press to trace · double-tap to reset",
                style = MaterialTheme.typography.labelSmall, color = Tone.Stone,
                modifier = Modifier.align(Alignment.BottomCenter).padding(10.dp),
            )
        }
        PaperCard(Modifier.fillMaxWidth().navigationBarsPadding().padding(12.dp)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                if (params.isNotEmpty()) {
                    SectionLabel("Parameters")
                    ParamSliders(params, paramValues, onChange = { n, v -> paramValues = paramValues + (n to v) })
                }
                SectionLabel("Equations")
                equations.forEachIndexed { i, eq ->
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(12.dp).clip(CircleShape).background(Tone.curves[i % Tone.curves.size]))
                        Spacer(Modifier.width(12.dp))
                        val latex = remember(eq) { MathEngine.previewLatex(eq) }
                        Box(Modifier.weight(1f)) {
                            if (latex != null) MathView(latex, fontSize = 20.sp, fit = MathFit.Shrink, color = Tone.Ink)
                            else Text(eq, color = Tone.Graphite)
                        }
                        if (equations.size > 1) CircleIconButton(Icons.Outlined.Close, "Remove", size = 40.dp) { onRemove(i) }
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    GradientButton("Add equation", Modifier.weight(1f), icon = Icons.Outlined.Add, style = com.mathlens.app.ui.components.ButtonStyle.Quiet, onClick = onAdd)
                    GradientButton("Wallpaper", Modifier.weight(1f), icon = Icons.Outlined.Wallpaper, style = com.mathlens.app.ui.components.ButtonStyle.Outline, onClick = onWallpaper)
                }
            }
        }
    }
}
