// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import android.app.WallpaperManager
import android.graphics.Bitmap
import android.widget.Toast
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Download
import androidx.compose.material.icons.rounded.Check
import androidx.compose.material.icons.outlined.Wallpaper
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.mathlens.app.ui.components.bouncyClick
import com.mathlens.app.ui.components.GradientButton
import com.mathlens.app.ui.components.HairlineSlider
import com.mathlens.app.ui.components.Pill
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.components.formatCoord
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground
import com.mathlens.app.wallpaper.ArtRenderer
import com.mathlens.app.wallpaper.DailyWallpaper
import com.mathlens.app.wallpaper.WallOptions
import com.mathlens.app.wallpaper.WallpaperRenderer
import com.mathlens.app.wallpaper.artDesigns
import com.mathlens.app.wallpaper.wallStyles
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.math.absoluteValue

/**
 * Wallpaper studio. Swipe the preview to change palette; pick your own equations or one of the
 * mathematical artworks and shape it with sliders; save it, set it, or let a new one arrive every day.
 */
@Composable
fun WallpaperScreen(initialEquations: List<String>, onBack: () -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val metrics = context.resources.displayMetrics
    val fullW = metrics.widthPixels.coerceAtMost(1440)
    val fullH = (fullW * metrics.heightPixels.toFloat() / metrics.widthPixels).toInt()
    val aspect = fullW.toFloat() / fullH

    // -1 = the user's equations; otherwise an index into artDesigns
    var design by remember { mutableStateOf(if (initialEquations.isEmpty()) 0 else -1) }
    var values by remember(design) { mutableStateOf(if (design >= 0) ArtRenderer.defaults(artDesigns[design]) else emptyMap()) }
    var options by remember { mutableStateOf(WallOptions()) }
    val pager = rememberPagerState { wallStyles.size }
    val previews = remember { mutableStateMapOf<Int, ImageBitmap>() }
    var rendering by remember { mutableStateOf(false) }
    var failed by remember { mutableStateOf(false) }
    var askTarget by remember { mutableStateOf<String?>(null) }  // "set" or "daily"
    var daily by remember { mutableStateOf(DailyWallpaper.enabled(context)) }

    fun render(styleIndex: Int, w: Int, h: Int): Bitmap? {
        val o = options.copy(style = wallStyles[styleIndex])
        return if (design >= 0) ArtRenderer.render(context, artDesigns[design], values, o, w, h)
        else WallpaperRenderer.render(context, initialEquations, o, w, h)
    }

    // re-render the visible palette first, then its neighbours; everything else is stale once inputs change
    LaunchedEffect(design, values, options) { previews.clear() }
    LaunchedEffect(design, values, options, pager.currentPage) {
        delay(90)
        val order = listOf(pager.currentPage, pager.currentPage + 1, pager.currentPage - 1).filter { it in wallStyles.indices }
        for (i in order) {
            if (previews.containsKey(i)) continue
            if (i == pager.currentPage) rendering = true
            val bmp = withContext(Dispatchers.Default) { runCatching { render(i, fullW / 2, fullH / 2) }.getOrNull() }
            if (i == pager.currentPage) { rendering = false; failed = bmp == null }
            if (bmp != null) previews[i] = bmp.asImageBitmap()
        }
    }

    fun full(block: (Bitmap) -> Unit) {
        scope.launch {
            rendering = true
            val bmp = withContext(Dispatchers.Default) { runCatching { render(pager.currentPage, fullW, fullH) }.getOrNull() }
            rendering = false
            if (bmp != null) block(bmp)
        }
    }

    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar("Wallpaper", onBack)
        // swipe between palettes; neighbours peek in at the sides
        HorizontalPager(
            pager,
            Modifier.weight(1f).fillMaxWidth(),
            contentPadding = PaddingValues(horizontal = 72.dp),
            pageSpacing = 14.dp,
        ) { page ->
            val offset = ((pager.currentPage - page) + pager.currentPageOffsetFraction).absoluteValue.coerceIn(0f, 1f)
            Box(Modifier.fillMaxSize().padding(vertical = 8.dp), contentAlignment = Alignment.Center) {
                Box(
                    Modifier.fillMaxHeight().aspectRatio(aspect, matchHeightConstraintsFirst = true)
                        .graphicsLayer { scaleX = 1f - 0.08f * offset; scaleY = 1f - 0.08f * offset; alpha = 1f - 0.35f * offset }
                        .clip(RoundedCornerShape(24.dp)).background(wallStyles[page].background.first())
                        .border(1.dp, Tone.Hairline, RoundedCornerShape(24.dp)),
                    contentAlignment = Alignment.Center,
                ) {
                    previews[page]?.let { Image(it, "Wallpaper preview", Modifier.fillMaxSize(), contentScale = ContentScale.Crop) }
                    if (page == pager.currentPage && rendering && previews[page] == null) CircularProgressIndicator(color = Tone.Terracotta, strokeWidth = 1.5.dp, modifier = Modifier.size(28.dp))
                    if (page == pager.currentPage && failed && !rendering) Text("Can't draw these equations", color = Tone.Error, textAlign = TextAlign.Center, modifier = Modifier.padding(16.dp))
                }
            }
        }
        // palette name and position
        Row(Modifier.fillMaxWidth().padding(top = 4.dp, bottom = 6.dp), horizontalArrangement = Arrangement.Center, verticalAlignment = Alignment.CenterVertically) {
            Text(wallStyles[pager.currentPage].name, style = MaterialTheme.typography.titleMedium, color = Tone.Ink)
            Spacer(Modifier.width(14.dp))
            wallStyles.indices.forEach { i ->
                Box(Modifier.padding(horizontal = 3.dp).size(if (i == pager.currentPage) 7.dp else 5.dp).clip(CircleShape).background(if (i == pager.currentPage) Tone.Ink else Tone.Hairline))
            }
        }
        // controls: the panel scrolls within 330dp so the preview keeps the rest of the screen
        Column(
            Modifier.fillMaxWidth().navigationBarsPadding().heightIn(max = 330.dp).verticalScroll(rememberScrollState()).padding(horizontal = 20.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            SectionLabel("Design")
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (initialEquations.isNotEmpty()) Pill("Yours", selected = design == -1) { design = -1 }
                artDesigns.forEachIndexed { i, d -> Pill(d.name, selected = design == i) { design = i } }
            }
            if (design >= 0) artDesigns[design].params.forEach { p ->
                SliderRow(p.label, values[p.key] ?: p.default, p.min, p.max, p.whole) { v -> values = values + (p.key to v) }
            }
            SliderRow("Size", options.zoom, 0.4f, 2f, whole = false) { options = options.copy(zoom = it) }
            SliderRow("Line", options.lineWidth, 0.4f, 3f, whole = false) { options = options.copy(lineWidth = it) }
            CheckRow("Show the formula", "Written at the bottom of the wallpaper", options.showEquation) { options = options.copy(showEquation = !options.showEquation) }
            if (design == -1) CheckRow("Show the grid", "Faint graph paper behind the curve", options.showGrid) { options = options.copy(showGrid = !options.showGrid) }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("A new artwork every day", style = MaterialTheme.typography.bodyLarge, color = Tone.Ink)
                    Text("Changes your wallpaper once a day", style = MaterialTheme.typography.bodySmall, color = Tone.Stone)
                }
                Switch(
                    daily, { on -> if (on) askTarget = "daily" else { DailyWallpaper.disable(context); daily = false } },
                    colors = SwitchDefaults.colors(checkedTrackColor = Tone.Ink, checkedThumbColor = Tone.Ivory, uncheckedTrackColor = Tone.Bone, uncheckedBorderColor = Tone.Hairline),
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                GradientButton("Save", Modifier.weight(1f), icon = Icons.Outlined.Download, style = com.mathlens.app.ui.components.ButtonStyle.Outline) {
                    full { bmp ->
                        val ok = WallpaperRenderer.saveToGallery(context, bmp)
                        Toast.makeText(context, if (ok) "Saved to Pictures/MathLens" else "Couldn't save", Toast.LENGTH_SHORT).show()
                    }
                }
                GradientButton("Set wallpaper", Modifier.weight(1f), icon = Icons.Outlined.Wallpaper) { askTarget = "set" }
            }
            Spacer(Modifier.height(4.dp))
        }
    }

    askTarget?.let { purpose ->
        AlertDialog(
            onDismissRequest = { askTarget = null },
            containerColor = Tone.Ivory,
            title = { Text(if (purpose == "daily") "A new artwork every day" else "Set as wallpaper", color = Tone.Ink) },
            text = { Text("Where should it appear?", color = Tone.Graphite) },
            confirmButton = {
                Row {
                    listOf(
                        "Home" to WallpaperManager.FLAG_SYSTEM,
                        "Lock" to WallpaperManager.FLAG_LOCK,
                        "Both" to (WallpaperManager.FLAG_SYSTEM or WallpaperManager.FLAG_LOCK),
                    ).forEach { (label, flag) ->
                        TextButton({
                            askTarget = null
                            if (purpose == "daily") {
                                DailyWallpaper.enable(context, flag)
                                daily = true
                                Toast.makeText(context, "A new artwork will arrive every day", Toast.LENGTH_SHORT).show()
                            } else full { bmp ->
                                scope.launch {
                                    val ok = withContext(Dispatchers.IO) { runCatching { WallpaperManager.getInstance(context).setBitmap(bmp, null, true, flag) }.isSuccess }
                                    Toast.makeText(context, if (ok) "Wallpaper updated" else "Couldn't set wallpaper", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }) { Text(label, color = Tone.Ink) }
                    }
                }
            },
        )
    }
}

/** A settings row with a checkbox; the whole row toggles. */
@Composable
private fun CheckRow(title: String, subtitle: String, checked: Boolean, onToggle: () -> Unit) {
    Row(Modifier.fillMaxWidth().bouncyClick(onClick = onToggle).padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge, color = Tone.Ink)
            Text(subtitle, style = MaterialTheme.typography.bodySmall, color = Tone.Stone)
        }
        val shape = RoundedCornerShape(6.dp)
        Box(
            Modifier.size(26.dp).clip(shape).background(if (checked) Tone.Ink else androidx.compose.ui.graphics.Color.Transparent)
                .border(1.5.dp, if (checked) Tone.Ink else Tone.Stone, shape),
            contentAlignment = Alignment.Center,
        ) {
            if (checked) androidx.compose.material3.Icon(androidx.compose.material.icons.Icons.Rounded.Check, null, tint = Tone.Ivory, modifier = Modifier.size(18.dp))
        }
    }
}

@Composable
private fun SliderRow(label: String, value: Float, min: Float, max: Float, whole: Boolean, onChange: (Float) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(label, style = MaterialTheme.typography.labelLarge, color = Tone.Graphite, modifier = Modifier.width(92.dp))
        HairlineSlider(value.toDouble(), min.toDouble(), max.toDouble(), onChange = { v ->
            onChange(if (whole) Math.round(v).toFloat() else v.toFloat())
        }, modifier = Modifier.weight(1f))
        Text(if (whole) Math.round(value).toString() else formatCoord(Math.round(value * 100) / 100.0), style = MaterialTheme.typography.labelLarge, color = Tone.Graphite,
            textAlign = TextAlign.End, modifier = Modifier.width(44.dp))
    }
}
