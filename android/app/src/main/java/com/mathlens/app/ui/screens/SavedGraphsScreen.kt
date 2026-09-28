// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.staggeredgrid.LazyVerticalStaggeredGrid
import androidx.compose.foundation.lazy.staggeredgrid.StaggeredGridCells
import androidx.compose.foundation.lazy.staggeredgrid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.data.SavedGraph
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.GraphView
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground
import java.text.DateFormat
import java.util.Date

/** Graphs the user kept, as a gallery: tap to open one on the graph screen, long-press to delete. */
@Composable
fun SavedGraphsScreen(items: List<SavedGraph>, onBack: () -> Unit, onOpen: (SavedGraph) -> Unit, onDelete: (SavedGraph) -> Unit) {
    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar("Saved graphs", onBack)
        if (items.isEmpty()) {
            Box(Modifier.fillMaxSize().padding(40.dp), contentAlignment = Alignment.Center) {
                Text("Tap the bookmark on a graph to keep it here", color = Tone.Stone, style = MaterialTheme.typography.bodyLarge, textAlign = TextAlign.Center)
            }
            return@Column
        }
        LazyVerticalStaggeredGrid(
            columns = StaggeredGridCells.Fixed(2),
            contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 28.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalItemSpacing = 12.dp,
            modifier = Modifier.fillMaxSize(),
        ) {
            items(items, key = { it.time }) { g -> SavedGraphCard(g, onOpen = { onOpen(g) }, onDelete = { onDelete(g) }) }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun SavedGraphCard(g: SavedGraph, onOpen: () -> Unit, onDelete: () -> Unit) {
    var menu by remember { mutableStateOf(false) }
    val spec = remember(g.equations) { MathEngine.graph(g.equations.joinToString("; ")).getOrNull() }
    Box {
        PaperCard(
            Modifier.fillMaxWidth().combinedClickable(
                interactionSource = remember { MutableInteractionSource() }, indication = null,
                onClick = onOpen, onLongClick = { menu = true },
            ),
        ) {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Box(Modifier.fillMaxWidth().height(120.dp).clip(RoundedCornerShape(8.dp)).background(Tone.Paper)) {
                    if (spec != null) GraphView(spec, Modifier.fillMaxSize(), interactive = false, animateIn = false, showGrid = false, lineWidth = 2f)
                }
                g.equations.take(3).forEach { eq ->
                    val latex = remember(eq) { MathEngine.previewLatex(eq) }
                    if (latex != null) MathView(latex, fontSize = 15.sp, fit = MathFit.Shrink, color = Tone.Ink)
                    else Text(eq, style = MaterialTheme.typography.bodySmall, color = Tone.Graphite)
                }
                if (g.equations.size > 3) Text("+ ${g.equations.size - 3} more", style = MaterialTheme.typography.labelMedium, color = Tone.Stone)
                Text(DateFormat.getDateInstance(DateFormat.MEDIUM).format(Date(g.time)), style = MaterialTheme.typography.labelMedium, color = Tone.Stone)
            }
        }
        DropdownMenu(menu, onDismissRequest = { menu = false }) {
            DropdownMenuItem(text = { Text("Delete", color = Tone.Error) }, onClick = { menu = false; onDelete() })
        }
    }
}
