// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import android.Manifest
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import com.mathlens.app.ui.art.PaperArt
import com.mathlens.app.ui.art.PaperScene
import com.mathlens.app.ui.components.GradientButton
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.bouncyClick
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground
import kotlinx.coroutines.launch
import kotlin.math.absoluteValue

private data class Page(val scene: PaperScene, val plain: String, val italic: String, val body: String)

private val pages = listOf(
    Page(PaperScene.Lens, "Point at", "a problem.", "Printed, handwritten in a notebook, or on a screen. Hold the camera steady and it reads by itself."),
    Page(PaperScene.Steps, "Every step,", "explained.", "Several methods side by side. Ask why at any line and the reasoning unfolds."),
    Page(PaperScene.Hero, "See the", "mathematics.", "Your problem, drawn: tangents that glide, areas that fill, curves that become wallpaper."),
)

@Composable
fun OnboardingScreen(onDone: () -> Unit) {
    val pager = rememberPagerState { pages.size }
    val scope = rememberCoroutineScope()
    val permission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { onDone() }
    val last = pager.currentPage == pages.lastIndex
    Column(Modifier.fillMaxSize().paperBackground().statusBarsPadding().navigationBarsPadding()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 28.dp, vertical = 18.dp), verticalAlignment = Alignment.CenterVertically) {
            SectionLabel("MathLens", Modifier.weight(1f))
            Text(
                if (last) "" else "Skip", style = MaterialTheme.typography.labelLarge, color = Tone.Stone,
                modifier = Modifier.bouncyClick(enabled = !last) { permission.launch(Manifest.permission.CAMERA) },
            )
        }
        HorizontalPager(pager, Modifier.weight(1f)) { index ->
            val page = pages[index]
            val offset = (pager.currentPage - index + pager.currentPageOffsetFraction)
            Column(Modifier.fillMaxSize().padding(horizontal = 28.dp)) {
                Box(
                    Modifier.fillMaxWidth().aspectRatio(0.92f).clip(RoundedCornerShape(4.dp))
                        .graphicsLayer { translationX = offset * size.width * 0.35f; alpha = 1f - offset.absoluteValue * 0.6f },
                ) {
                    PaperArt(Modifier.fillMaxSize(), scene = page.scene, background = Tone.Bone)
                }
                Spacer(Modifier.height(30.dp))
                Text(
                    buildAnnotatedString {
                        append(page.plain + "\n")
                        withStyle(SpanStyle(fontStyle = FontStyle.Italic, color = Tone.Terracotta)) { append(page.italic) }
                    },
                    style = MaterialTheme.typography.displayMedium, color = Tone.Ink,
                    modifier = Modifier.graphicsLayer { translationX = offset * size.width * 0.15f },
                )
                Spacer(Modifier.height(14.dp))
                Text(page.body, style = MaterialTheme.typography.bodyLarge, color = Tone.Graphite)
            }
        }
        Row(Modifier.fillMaxWidth().padding(28.dp), verticalAlignment = Alignment.CenterVertically) {
            Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                pages.indices.forEach { i ->
                    val w by animateDpAsState(if (i == pager.currentPage) 28.dp else 10.dp, tween(350), label = "dot")
                    Box(Modifier.width(w).height(2.dp).background(if (i == pager.currentPage) Tone.Ink else Tone.Hairline))
                }
            }
            GradientButton(if (last) "Open the camera" else "Continue") {
                if (last) permission.launch(Manifest.permission.CAMERA)
                else scope.launch { pager.animateScrollToPage(pager.currentPage + 1) }
            }
        }
    }
}
