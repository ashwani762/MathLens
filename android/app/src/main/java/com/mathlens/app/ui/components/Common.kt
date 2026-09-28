// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.components

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.ui.theme.Tone

/** Quiet press feedback: a slight settle rather than a bounce. */
@Composable
fun Modifier.bouncyClick(enabled: Boolean = true, onClick: () -> Unit): Modifier {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    return this
        .scale(if (pressed) 0.975f else 1f)
        .alpha(if (pressed) 0.82f else 1f)
        .clickable(interactionSource = source, indication = null, enabled = enabled, onClick = onClick)
}

enum class ButtonStyle { Ink, Outline, Quiet }

/** Primary actions are solid ink; secondary actions are a hairline outline. */
@Composable
fun GradientButton(
    text: String,
    modifier: Modifier = Modifier,
    icon: ImageVector? = null,
    style: ButtonStyle = ButtonStyle.Ink,
    height: Dp = 54.dp,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(height / 2)
    val bg = when (style) {
        ButtonStyle.Ink -> if (enabled) Tone.Ink else Tone.Stone
        ButtonStyle.Outline -> Color.Transparent
        ButtonStyle.Quiet -> Tone.Bone
    }
    val fg = if (style == ButtonStyle.Ink) Tone.Ivory else Tone.Ink
    Box(
        modifier
            .height(height)
            .clip(shape)
            .background(bg)
            .then(if (style == ButtonStyle.Outline) Modifier.border(1.dp, Tone.Ink, shape) else Modifier)
            .bouncyClick(enabled, onClick)
            .padding(horizontal = 24.dp),
        contentAlignment = Alignment.Center,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            if (icon != null) {
                Icon(icon, null, tint = fg, modifier = Modifier.size(18.dp))
                Spacer(Modifier.width(10.dp))
            }
            Text(text, style = MaterialTheme.typography.labelLarge, color = fg)
        }
    }
}

/** Round hairline icon button. */
@Composable
fun CircleIconButton(
    icon: ImageVector,
    contentDescription: String?,
    modifier: Modifier = Modifier,
    size: Dp = 50.dp,
    tint: Color = Tone.Ink,
    background: Color = Tone.Ivory,
    border: Color = Tone.Hairline,
    onClick: () -> Unit,
) {
    Box(
        modifier
            .size(size)
            .clip(CircleShape)
            .background(background)
            .border(1.dp, border, CircleShape)
            .bouncyClick(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(icon, contentDescription, tint = tint, modifier = Modifier.size(size * 0.48f))
    }
}

/** Text tab: selected state is ink with a short underline, like a catalogue index. */
@Composable
fun Chip(text: String, selected: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val color by animateColorAsState(if (selected) Tone.Ink else Tone.Stone, tween(250), label = "chip")
    val line by animateDpAsState(if (selected) 18.dp else 0.dp, tween(300), label = "chipLine")
    Column(
        modifier.bouncyClick(onClick = onClick).padding(horizontal = 2.dp, vertical = 6.dp),
        horizontalAlignment = Alignment.Start,
    ) {
        Text(text, style = MaterialTheme.typography.labelLarge, color = color)
        Spacer(Modifier.height(6.dp))
        Box(Modifier.width(line).height(1.5.dp).background(Tone.Ink))
    }
}

/** Outlined pill used for example problems and presets. */
@Composable
fun Pill(text: String, modifier: Modifier = Modifier, selected: Boolean = false, onClick: () -> Unit) {
    val shape = RoundedCornerShape(50)
    Box(
        modifier
            .clip(shape)
            .background(if (selected) Tone.Ink else Color.Transparent)
            .border(1.dp, if (selected) Tone.Ink else Tone.Hairline, shape)
            .bouncyClick(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 9.dp),
    ) {
        Text(text, style = MaterialTheme.typography.labelMedium, color = if (selected) Tone.Ivory else Tone.Graphite)
    }
}

@Composable
fun TopBar(title: String, onBack: () -> Unit, modifier: Modifier = Modifier, actions: @Composable RowScope.() -> Unit = {}) {
    Row(
        modifier
            .fillMaxWidth()
            .statusBarsPadding()
            .padding(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        CircleIconButton(Icons.AutoMirrored.Outlined.ArrowBack, "Back", size = 48.dp, onClick = onBack)
        Text(title, style = MaterialTheme.typography.labelSmall.copy(fontSize = 15.sp, letterSpacing = 1.6.sp), color = Tone.Graphite, modifier = Modifier.weight(1f).padding(start = 6.dp))
        actions()
    }
}

/** Small tracked capitals, the "eyebrow" label of editorial layouts. */
@Composable
fun SectionLabel(text: String, modifier: Modifier = Modifier, color: Color = Tone.Stone) {
    Text(text.uppercase(), style = MaterialTheme.typography.labelSmall, color = color, modifier = modifier)
}

@Composable
fun Hairline(modifier: Modifier = Modifier, color: Color = Tone.Hairline) {
    Box(modifier.fillMaxWidth().height(1.dp).background(color))
}

/** Slow breathing value for ambient motion. */
@Composable
fun rememberPulse(min: Float = 0.4f, max: Float = 1f, millis: Int = 1400): Float {
    val t = rememberInfiniteTransition(label = "pulse")
    val v by t.animateFloat(min, max, infiniteRepeatable(tween(millis), RepeatMode.Reverse), label = "pulseValue")
    return v
}
