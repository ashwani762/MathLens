// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import com.mathlens.app.engine.MathEngine

import com.mathlens.app.ui.components.ParamSliders

import android.content.Intent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.OpenInFull
import androidx.compose.material.icons.outlined.Share
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.engine.CurveType
import com.mathlens.app.engine.Solution
import com.mathlens.app.engine.Step
import com.mathlens.app.ui.art.PaperArt
import com.mathlens.app.ui.components.Chip
import com.mathlens.app.ui.components.CircleIconButton
import com.mathlens.app.ui.components.GradientButton
import com.mathlens.app.ui.components.GraphView
import com.mathlens.app.ui.components.Hairline
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.components.bouncyClick
import com.mathlens.app.ui.explain.ConceptAnimation
import com.mathlens.app.ui.explain.hasConcept
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.math.RichMathText
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground

fun kindTitle(kind: String) = when (kind) {
    "arithmetic" -> "Arithmetic"
    "linear" -> "Linear equation"
    "quadratic" -> "Quadratic equation"
    "polynomial" -> "Polynomial equation"
    "rational" -> "Rational equation"
    "radical" -> "Radical equation"
    "trigonometric" -> "Trigonometry"
    "inequality" -> "Inequality"
    "derivative" -> "Derivative"
    "integral" -> "Integral"
    "limit" -> "Limit"
    "system" -> "System of equations"
    "complex" -> "Complex numbers"
    "factorial" -> "Factorial"
    "numbertheory" -> "Number theory"
    "combinatorics" -> "Combinatorics"
    "matrix", "determinant", "eigenvalues" -> "Matrices"
    "graph" -> "Graph"
    "function" -> "Function"
    "simplify", "factor", "expand" -> "Algebra"
    else -> "Solution"
}

private val roman = listOf("I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX", "XX")

@Composable
fun SolutionScreen(
    solution: Solution?,
    solving: Boolean,
    onBack: () -> Unit,
    onEdit: (String) -> Unit,
    onOpenGraph: (String) -> Unit,
) {
    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar(solution?.let { kindTitle(it.kind) } ?: "Solving", onBack) {
            solution?.let { s ->
                val context = LocalContext.current
                CircleIconButton(Icons.Outlined.Edit, "Edit", size = 48.dp) { onEdit(s.query) }
                Box {
                    var shareMenu by remember { mutableStateOf(false) }
                    CircleIconButton(Icons.Outlined.Share, "Share", size = 48.dp) { shareMenu = true }
                    androidx.compose.material3.DropdownMenu(shareMenu, onDismissRequest = { shareMenu = false }) {
                        androidx.compose.material3.DropdownMenuItem(text = { Text("Share as text") }, onClick = {
                            shareMenu = false
                            val text = s.query.split(';').joinToString("\n") { it.trim() } + "\n\n= " + s.answers.joinToString("\n").ifEmpty { s.answer } + "\n\nMathLens"
                            context.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text), "Share"))
                        })
                        androidx.compose.material3.DropdownMenuItem(text = { Text("Export as PDF") }, onClick = {
                            shareMenu = false
                            runCatching { com.mathlens.app.export.SolutionPdf.share(context, s) }
                        })
                    }
                }
            }
        }
        when {
            solving || solution == null -> Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator(color = Tone.Ink, strokeWidth = 1.5.dp, modifier = Modifier.size(28.dp))
            }
            !solution.ok -> ErrorState(solution, onEdit)
            else -> SolutionContent(solution, onOpenGraph)
        }
    }
}

@Composable
private fun ErrorState(s: Solution, onEdit: (String) -> Unit) {
    Column(Modifier.fillMaxSize().padding(28.dp), verticalArrangement = Arrangement.Center) {
        PaperArt(Modifier.fillMaxWidth().aspectRatio(1.4f).clip(RoundedCornerShape(4.dp)), seed = s.query.hashCode())
        Spacer(Modifier.height(32.dp))
        if (s.input.isNotEmpty()) MathView(s.input, fontSize = 24.sp, fit = MathFit.Shrink, color = Tone.Ink)
        Spacer(Modifier.height(16.dp))
        Text("Not yet within reach.", style = MaterialTheme.typography.headlineMedium, color = Tone.Ink)
        Spacer(Modifier.height(8.dp))
        Text(s.error, style = MaterialTheme.typography.bodyLarge, color = Tone.Graphite)
        Spacer(Modifier.height(28.dp))
        GradientButton("Edit the problem", icon = Icons.Outlined.Edit) { onEdit(s.query) }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun SolutionContent(s: Solution, onOpenGraph: (String) -> Unit) {
    var methodIndex by remember(s) { mutableIntStateOf(0) }
    val method = s.methods.getOrNull(methodIndex)
    LazyColumn(
        Modifier.fillMaxSize(),
        contentPadding = PaddingValues(start = 22.dp, end = 22.dp, bottom = 48.dp),
    ) {
        // the problem, set like an exhibition title
        item {
            Column(Modifier.padding(top = 18.dp, bottom = 26.dp)) {
                SectionLabel(s.title.replace("$", ""))
                Spacer(Modifier.height(14.dp))
                MathView(s.input, fontSize = 30.sp, fit = MathFit.Shrink, color = Tone.Ink)
            }
        }
        item { AnswerBlock(s) }
        if (s.facts.isNotEmpty()) item {
            FlowRow(Modifier.padding(top = 20.dp), horizontalArrangement = Arrangement.spacedBy(24.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                s.facts.forEach { f ->
                    Column {
                        SectionLabel(f.label)
                        Spacer(Modifier.height(6.dp))
                        MathView(f.latex, fontSize = 16.sp, color = Tone.Ink)
                    }
                }
            }
        }
        // only when the problem can actually be drawn or animated
        if (hasConcept(s)) item { Spacer(Modifier.height(36.dp)); VisualsBlock(s, onOpenGraph) }
        item {
            Column(Modifier.padding(top = 40.dp, bottom = 8.dp)) {
                SectionLabel("The method")
                Spacer(Modifier.height(10.dp))
                if (s.methods.size > 1) {
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(22.dp)) {
                        s.methods.forEachIndexed { i, m -> Chip(m.name, selected = i == methodIndex) { methodIndex = i } }
                    }
                } else {
                    Text(method?.name ?: "", style = MaterialTheme.typography.titleLarge, color = Tone.Ink)
                }
                Spacer(Modifier.height(12.dp))
                Hairline()
            }
        }
        method?.let { m ->
            itemsIndexed(m.steps, key = { i, _ -> "${methodIndex}_$i" }) { i, step ->
                // compare with the last line of working before this one (the problem itself for step 1)
                val prev = m.steps.take(i).lastOrNull { it.latex.isNotEmpty() }?.latex ?: s.input
                StepRow(i, step, delayMs = (i * 60).coerceAtMost(600), prevLatex = prev)
            }
        }
        item { Spacer(Modifier.navigationBarsPadding()) }
    }
}

@Composable
private fun AnswerBlock(s: Solution) {
    val appear = remember(s) { Animatable(0f) }
    LaunchedEffect(s) { appear.animateTo(1f, tween(900, easing = FastOutSlowInEasing)) }
    val multi = s.answers.size in 2..6 && s.answer.contains("\\quad")
    Row(
        Modifier
            .fillMaxWidth()
            .graphicsLayer { alpha = appear.value; translationY = (1 - appear.value) * 24f }
            .drawBehind { drawLine(Tone.Terracotta, Offset(0f, 0f), Offset(0f, size.height), 2.dp.toPx()) }
            .padding(start = 20.dp, top = 2.dp, bottom = 2.dp),
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            SectionLabel("Answer", color = Tone.Terracotta)
            if (multi) s.answers.forEach { MathView(it, fontSize = 30.sp, fit = MathFit.Shrink, color = Tone.Ink) }
            else MathView(s.answer, fontSize = 32.sp, fit = MathFit.Shrink, color = Tone.Ink)
        }
    }
}

private enum class Visual { Explain, Graph }

@Composable
private fun VisualsBlock(s: Solution, onOpenGraph: (String) -> Unit) {
    val hasGraph = s.plot != null && s.plot.curves.isNotEmpty()
    var mode by remember(s) { mutableStateOf(Visual.Explain) }
    var paramValues by remember(s) { mutableStateOf(s.plot?.params.orEmpty().associateWith { 1.0 }) }
    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(verticalAlignment = Alignment.Bottom) {
            SectionLabel("See it", Modifier.weight(1f).padding(bottom = 12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(18.dp)) {
                Chip("Explain", mode == Visual.Explain) { mode = Visual.Explain }
                if (hasGraph) Chip("Graph", mode == Visual.Graph) { mode = Visual.Graph }
            }
        }
        PaperCard(Modifier.fillMaxWidth().height(340.dp)) {
            when (mode) {
                Visual.Explain -> ConceptAnimation(s, Modifier.fillMaxSize())
                Visual.Graph -> {
                    val plot = s.plot
                    if (plot != null) {
                        GraphView(plot, Modifier.fillMaxSize().padding(1.dp), interactive = true, paramValues = paramValues)
                        val graphQuery = plot.curves.joinToString("; ") { c ->
                            when (c.type) {
                                CurveType.Explicit -> "y = ${c.template}"
                                CurveType.Implicit -> "${c.expr} = 0"
                                CurveType.Polar -> "r = ${c.expr}"
                                CurveType.Vertical -> "x = ${c.expr}"
                                CurveType.Surface -> "z = ${c.expr}"
                            }
                        }
                        // the user's own input reads best in the graph's equation list; fall back to the curves
                        val openQuery = remember(s) { if (MathEngine.graph(s.query).isSuccess) s.query else graphQuery }
                        if (graphQuery.isNotEmpty()) {
                            CircleIconButton(Icons.Outlined.OpenInFull, "Open graph", Modifier.align(Alignment.TopEnd).padding(10.dp), size = 42.dp) {
                                onOpenGraph(openQuery)
                            }
                        }
                    }
                }
            }
        }
        val params = s.plot?.params.orEmpty()
        if (mode == Visual.Graph && params.isNotEmpty()) {
            ParamSliders(params, paramValues, onChange = { n, v -> paramValues = paramValues + (n to v) })
        }
    }
}

@Composable
private fun StepRow(index: Int, step: Step, delayMs: Int, prevLatex: String? = null) {
    val appear = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        kotlinx.coroutines.delay(delayMs.toLong())
        appear.animateTo(1f, tween(600, easing = FastOutSlowInEasing))
    }
    var expanded by remember { mutableStateOf(false) }
    Column(
        Modifier
            .fillMaxWidth()
            .graphicsLayer { alpha = appear.value; translationY = (1 - appear.value) * 28f }
            .animateContentSize(),
    ) {
        Row(Modifier.padding(vertical = 22.dp)) {
            Text(
                roman.getOrElse(index) { "${index + 1}" },
                style = MaterialTheme.typography.titleLarge.copy(fontStyle = FontStyle.Italic),
                color = Tone.Terracotta,
                modifier = Modifier.width(48.dp),
            )
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                if (step.title.isNotEmpty()) RichMathText(step.title, MaterialTheme.typography.titleMedium, Tone.Ink)
                if (step.text.isNotEmpty()) RichMathText(step.text, MaterialTheme.typography.bodyMedium, Tone.Graphite)
                if (step.latex.isNotEmpty()) {
                    val shown = remember(step.latex, prevLatex) { com.mathlens.app.ui.math.StepDiff.highlightChanges(prevLatex, step.latex) }
                    Box(Modifier.padding(top = 4.dp)) { MathView(shown, fontSize = 21.sp, color = Tone.Ink) }
                }
                if (step.sub.isNotEmpty()) {
                    Text(
                        if (expanded) "Close the explanation" else "Why? Explain this step",
                        style = MaterialTheme.typography.labelLarge.copy(fontStyle = FontStyle.Italic),
                        color = Tone.Terracotta,
                        modifier = Modifier.padding(top = 2.dp).bouncyClick { expanded = !expanded },
                    )
                    AnimatedVisibility(expanded, enter = expandVertically(tween(400)) + fadeIn(), exit = shrinkVertically(tween(300)) + fadeOut()) {
                        Column(Modifier.padding(top = 6.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                            step.sub.forEachIndexed { k, sub -> SubStep(sub, 0, step.sub.take(k).lastOrNull { it.latex.isNotEmpty() }?.latex) }
                        }
                    }
                }
            }
        }
        Hairline()
    }
}

@Composable
private fun SubStep(step: Step, depth: Int, prevLatex: String? = null) {
    Row(
        Modifier.fillMaxWidth().padding(start = (depth * 12).dp).drawBehind {
            drawLine(Tone.Hairline, Offset(0f, 0f), Offset(0f, size.height), 2.dp.toPx())
        },
    ) {
        Column(Modifier.padding(start = 14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (step.title.isNotEmpty()) RichMathText(step.title, MaterialTheme.typography.labelLarge, Tone.Ink)
            if (step.text.isNotEmpty()) RichMathText(step.text, MaterialTheme.typography.bodySmall, Tone.Graphite, mathSize = 14.sp)
            if (step.latex.isNotEmpty()) {
                val shown = remember(step.latex, prevLatex) { com.mathlens.app.ui.math.StepDiff.highlightChanges(prevLatex, step.latex) }
                MathView(shown, fontSize = 17.sp, color = Tone.Ink)
            }
            step.sub.forEachIndexed { k, sub -> SubStep(sub, depth + 1, step.sub.take(k).lastOrNull { it.latex.isNotEmpty() }?.latex) }
        }
    }
}

