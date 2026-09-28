// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.staggeredgrid.LazyVerticalStaggeredGrid
import androidx.compose.foundation.lazy.staggeredgrid.StaggeredGridCells
import androidx.compose.foundation.lazy.staggeredgrid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.DeleteSweep
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material.icons.outlined.StarOutline
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.data.HistoryItem
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.Chip
import com.mathlens.app.ui.components.CircleIconButton
import com.mathlens.app.ui.components.GraphView
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground
import java.text.DateFormat
import java.util.Date

/** Kinds whose problems read best with a small curve on the card. */
private val graphKinds = setOf("graph", "function", "quadratic", "polynomial", "linear", "system", "trigonometric", "inequality", "derivative", "integral", "limit", "taylor", "ode")

private val kindNames = mapOf(
    "graph" to "Curve", "function" to "Function", "quadratic" to "Quadratic", "polynomial" to "Polynomial", "linear" to "Linear",
    "system" to "System", "derivative" to "Derivative", "integral" to "Integral", "limit" to "Limit", "matrix" to "Matrix",
    "eigenvalues" to "Eigenvalues", "determinant" to "Determinant", "complex" to "Complex", "arithmetic" to "Arithmetic",
    "finance" to "Finance", "statistics" to "Statistics", "taylor" to "Series", "ode" to "Differential equation",
    "inequality" to "Inequality", "trigonometric" to "Trigonometry", "tensor" to "3D matrix", "surface" to "Surface",
)

@Composable
fun HistoryScreen(
    items: List<HistoryItem>,
    onBack: () -> Unit,
    onOpen: (HistoryItem) -> Unit,
    onDelete: (HistoryItem) -> Unit,
    onClear: () -> Unit,
    onToggleFavorite: (HistoryItem) -> Unit,
) {
    var query by rememberSaveable { mutableStateOf("") }
    var onlyFavorites by rememberSaveable { mutableStateOf(false) }
    val shown = remember(items, query, onlyFavorites) {
        val q = query.trim().lowercase()
        items.filter { (!onlyFavorites || it.favorite) }
            .filter { q.isEmpty() || it.query.lowercase().contains(q) || (kindNames[it.kind] ?: it.kind).lowercase().contains(q) }
    }
    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar("History", onBack) {
            if (items.any { !it.favorite }) CircleIconButton(Icons.Outlined.DeleteSweep, "Clear all except favourites", size = 48.dp, onClick = onClear)
        }
        if (items.isEmpty()) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("Problems you solve will appear here", color = Tone.Stone, style = MaterialTheme.typography.bodyLarge)
            }
            return@Column
        }
        // search + filter
        Column(Modifier.padding(horizontal = 20.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Row(
                Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(Tone.Ivory).border(1.dp, Tone.Hairline, RoundedCornerShape(12.dp))
                    .padding(horizontal = 14.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(Icons.Outlined.Search, null, tint = Tone.Stone, modifier = Modifier.size(20.dp))
                Spacer(Modifier.width(10.dp))
                Box(Modifier.weight(1f)) {
                    if (query.isEmpty()) Text("Search problems or topics", color = Tone.Stone, style = MaterialTheme.typography.bodyLarge)
                    BasicTextField(
                        query, { query = it }, singleLine = true,
                        textStyle = TextStyle(fontSize = 16.sp, color = Tone.Ink), cursorBrush = SolidColor(Tone.Terracotta),
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(18.dp)) {
                Chip("All", !onlyFavorites) { onlyFavorites = false }
                Chip("Favourites", onlyFavorites) { onlyFavorites = true }
            }
        }
        if (shown.isEmpty()) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(if (onlyFavorites && query.isEmpty()) "Tap the star on a problem to keep it here" else "Nothing matches", color = Tone.Stone, style = MaterialTheme.typography.bodyLarge)
            }
            return@Column
        }
        LazyVerticalStaggeredGrid(
            columns = StaggeredGridCells.Fixed(2),
            contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 12.dp, bottom = 28.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalItemSpacing = 12.dp,
            modifier = Modifier.fillMaxSize(),
        ) {
            items(shown, key = { "${it.time}_${it.query}" }) { item ->
                HistoryCard(item, onOpen = { onOpen(item) }, onDelete = { onDelete(item) }, onToggleFavorite = { onToggleFavorite(item) })
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun HistoryCard(item: HistoryItem, onOpen: () -> Unit, onDelete: () -> Unit, onToggleFavorite: () -> Unit) {
    var menu by remember { mutableStateOf(false) }
    // a preview only when there is a real curve (a lone vertical line like x = 1 says nothing)
    val plot = remember(item.query) {
        if (item.kind !in graphKinds) null
        else MathEngine.graph(item.query).getOrNull()?.takeIf { p -> p.curves.any { it.type != com.mathlens.app.engine.CurveType.Vertical && it.type != com.mathlens.app.engine.CurveType.Surface } }
    }
    Box {
        PaperCard(
            Modifier.fillMaxWidth().combinedClickable(
                interactionSource = remember { MutableInteractionSource() }, indication = null,
                onClick = onOpen, onLongClick = { menu = true },
            ),
        ) {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    SectionLabel(kindNames[item.kind] ?: item.kind.replaceFirstChar { it.uppercase() }, Modifier.weight(1f))
                    Box(Modifier.size(32.dp).clip(RoundedCornerShape(16.dp)).combinedClickable(onClick = onToggleFavorite), contentAlignment = Alignment.Center) {
                        Icon(
                            if (item.favorite) Icons.Rounded.Star else Icons.Outlined.StarOutline,
                            if (item.favorite) "Remove from favourites" else "Add to favourites",
                            tint = if (item.favorite) Tone.Terracotta else Tone.Stone, modifier = Modifier.size(20.dp),
                        )
                    }
                }
                if (plot != null && plot.curves.isNotEmpty()) {
                    Box(Modifier.fillMaxWidth().height(86.dp).clip(RoundedCornerShape(8.dp)).background(Tone.Paper)) {
                        GraphView(plot, Modifier.fillMaxSize(), interactive = false, animateIn = false, showGrid = false, lineWidth = 2f)
                    }
                }
                MathView(item.inputLatex, fontSize = 18.sp, fit = MathFit.Shrink, color = Tone.Ink)
                MathView(item.answerLatex, fontSize = 15.sp, fit = MathFit.Shrink, color = Tone.Terracotta)
                Text(DateFormat.getDateInstance(DateFormat.MEDIUM).format(Date(item.time)), style = MaterialTheme.typography.labelMedium, color = Tone.Stone)
            }
        }
        DropdownMenu(menu, onDismissRequest = { menu = false }) {
            DropdownMenuItem(text = { Text(if (item.favorite) "Remove from favourites" else "Add to favourites") }, onClick = { menu = false; onToggleFavorite() })
            DropdownMenuItem(text = { Text("Delete", color = Tone.Error) }, onClick = { menu = false; onDelete() })
        }
    }
}
