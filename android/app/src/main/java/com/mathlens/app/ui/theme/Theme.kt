// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.theme

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.text.ExperimentalTextApi
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontVariation
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.R

/**
 * Gallery palette: warm paper neutrals, ink, and a few pigments used sparingly:
 * the way a white-cube gallery lets a single painting speak.
 */
object Tone {
    val Paper = Color(0xFFF6F3EE)      // page
    val Ivory = Color(0xFFFBFAF7)      // cards
    val Bone = Color(0xFFEDE7DD)       // quiet fills
    val Linen = Color(0xFFE4DCCF)      // pressed / grid
    val Hairline = Color(0xFFDAD2C5)   // 1px rules
    val Stone = Color(0xFF8F887E)      // tertiary text
    val Graphite = Color(0xFF4A4642)   // secondary text
    val Ink = Color(0xFF151413)        // primary text / actions

    val Terracotta = Color(0xFFB4533A)
    val Ochre = Color(0xFFC8962E)
    val Sage = Color(0xFF7C8A6E)
    val Klein = Color(0xFF1F3FBF)      // International Klein Blue, used as a rare accent
    val Blush = Color(0xFFE9CDBE)
    val Sand = Color(0xFFD9C4A3)
    val Error = Color(0xFFA23B2A)

    /** Curve colours for graphs: ink first, then pigments. */
    val curves = listOf(Ink, Terracotta, Klein, Ochre, Sage)
    /** Pigments for abstract compositions. */
    val pigments = listOf(Terracotta, Ochre, Sage, Klein, Blush, Sand, Ink)
}

@OptIn(ExperimentalTextApi::class)
private fun inter(weight: Int) = Font(
    R.font.inter_variable,
    weight = FontWeight(weight),
    variationSettings = FontVariation.Settings(FontVariation.weight(weight)),
)

val Serif = FontFamily(
    Font(R.font.instrument_serif, FontWeight.Normal),
    Font(R.font.instrument_serif_italic, FontWeight.Normal, FontStyle.Italic),
)
val Sans = FontFamily(inter(300), inter(400), inter(500), inter(600))

private val AppTypography = Typography(
    displayLarge = TextStyle(fontFamily = Serif, fontSize = 60.sp, lineHeight = 60.sp, letterSpacing = (-1.2).sp),
    displayMedium = TextStyle(fontFamily = Serif, fontSize = 44.sp, lineHeight = 46.sp, letterSpacing = (-0.8).sp),
    headlineLarge = TextStyle(fontFamily = Serif, fontSize = 36.sp, lineHeight = 40.sp, letterSpacing = (-0.5).sp),
    headlineMedium = TextStyle(fontFamily = Serif, fontSize = 30.sp, lineHeight = 34.sp, letterSpacing = (-0.3).sp),
    titleLarge = TextStyle(fontFamily = Serif, fontSize = 26.sp, lineHeight = 30.sp),
    titleMedium = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Medium, fontSize = 16.sp, lineHeight = 22.sp),
    titleSmall = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Medium, fontSize = 15.sp, lineHeight = 21.sp),
    bodyLarge = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Light, fontSize = 16.sp, lineHeight = 25.sp),
    bodyMedium = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Light, fontSize = 14.sp, lineHeight = 22.sp),
    bodySmall = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Normal, fontSize = 12.sp, lineHeight = 18.sp),
    labelLarge = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Medium, fontSize = 14.sp, letterSpacing = 0.3.sp),
    labelMedium = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Normal, fontSize = 13.sp, letterSpacing = 0.2.sp),
    labelSmall = TextStyle(fontFamily = Sans, fontWeight = FontWeight.Medium, fontSize = 12.sp, letterSpacing = 1.8.sp),
)

@Composable
fun MathLensTheme(content: @Composable () -> Unit) {
    ProportionalDensity { MathLensThemeInner(content) }
}

/** Design reference width: the layout was tuned on a 411dp-wide phone. */
private const val REFERENCE_WIDTH_DP = 411f

/**
 * Keeps the UI in the same proportion to the screen on every phone. A phone whose display-size setting
 * makes it 461dp wide (e.g. density 375 on 1080px) would otherwise show everything ~11% smaller; here
 * dp scale with the width instead, clamped so small phones and tablets stay sensible. fontScale is kept,
 * so the user's font-size accessibility setting still applies on top.
 */
@Composable
private fun ProportionalDensity(content: @Composable () -> Unit) {
    val base = androidx.compose.ui.platform.LocalDensity.current
    val config = androidx.compose.ui.platform.LocalConfiguration.current
    val widthDp = config.screenWidthDp.toFloat().coerceAtLeast(1f)
    val scale = (widthDp / REFERENCE_WIDTH_DP).coerceIn(0.9f, 1.2f)
    val scaled = androidx.compose.ui.unit.Density(base.density * scale, base.fontScale)
    androidx.compose.runtime.CompositionLocalProvider(androidx.compose.ui.platform.LocalDensity provides scaled) { content() }
}

@Composable
private fun MathLensThemeInner(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = lightColorScheme(
            primary = Tone.Ink,
            onPrimary = Tone.Ivory,
            secondary = Tone.Terracotta,
            tertiary = Tone.Klein,
            background = Tone.Paper,
            onBackground = Tone.Ink,
            surface = Tone.Ivory,
            onSurface = Tone.Ink,
            surfaceVariant = Tone.Bone,
            onSurfaceVariant = Tone.Graphite,
            outline = Tone.Hairline,
            error = Tone.Error,
        ),
        typography = AppTypography,
        content = content,
    )
}

/** A sheet of heavier paper: ivory, a single hairline, barely rounded corners. */
@Composable
fun PaperCard(
    modifier: Modifier = Modifier,
    shape: Shape = RoundedCornerShape(6.dp),
    color: Color = Tone.Ivory,
    border: Color = Tone.Hairline,
    borderWidth: Dp = 1.dp,
    content: @Composable BoxScope.() -> Unit,
) {
    Box(modifier.clip(shape).background(color).border(borderWidth, border, shape), content = content)
}

fun Modifier.paperBackground(): Modifier = this.background(Tone.Paper)
