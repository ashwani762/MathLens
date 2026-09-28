// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.material.icons.outlined.Info

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.components.bouncyClick
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground

private val topics = listOf(
    "Arithmetic & fractions" to listOf("(1+2)*3-4/2", "(3/4) ÷ (2/5)", "sqrt(72)", "2^10 - 5!"),
    "Equations" to listOf("3(x-2)=2x+5", "2x^2+3x-2=0", "x^3-6x^2+11x-6=0", "sqrt(x+3)=x-3", "2^(x+1)=16", "sin(x)=1/2", "(x+1)/(x-2)=3", "|2x-1|=5"),
    "Inequalities" to listOf("2x+1>5", "x^2-4<0", "(x-1)/(x+2)>=0", "|x-3|<2"),
    "Systems" to listOf("x+y=5; x-y=1", "x+y+z=6; 2x-y+z=3; x+2y-z=2", "x+y=5; x*y=6", "2x+3y=12; x-y=1"),
    "Curves & conics" to listOf("x^2+y^2=25", "(x-1)^2+(y+2)^2=9", "x^2/9+y^2/4=1", "x^2-y^2=1", "y=x^2-4x+3", "r = cos(5*theta)"),
    "Derivatives" to listOf("d/dx(x^3+2x^2-5)", "d/dx(sin(x^2))", "d/dx(ln(x)/x)", "d/dx(x^x)"),
    "Integrals" to listOf("int(x*e^x, x)", "int(sin(x)^2, x)", "int(1/(x^2-1), x)", "int(x^2, x, 0, 3)"),
    "Limits & series" to listOf("lim(sin(x)/x, x, 0)", "lim((x^2-1)/(x-1), x, 1)", "lim((1+1/x)^x, x, inf)", "taylor(sin(x),x,0,7)", "taylor(e^x,x,0,4)"),
    "Differential equations" to listOf("y'=2y", "dy/dx = x*y", "y' + y = x", "y''+y=0", "y''-3y'+2y=0", "y''+2y'+y=0"),
    "Surfaces (3D)" to listOf("z=x^2+y^2", "z=x^2-y^2", "z=x^3-3x+y^2", "surface(sin(x)*cos(y))"),
    "Complex numbers (z solver)" to listOf("(3+2i)(1-i)", "(2+3i)/(1-i)", "z^3=1", "z^2+2z+5=0", "z+2conj(z)=3-i", "i^23"),
    "Matrices" to listOf("[[1,2],[3,4]]*[[5,6],[7,8]]", "det([[1,2,3],[4,5,6],[7,8,10]])", "inv([[2,1],[5,3]])", "eig([[2,1],[1,2]])", "[[0,-1],[1,0]]", "det([[1,2],[2,4]])"),
    "3D matrices" to listOf(
        "[[[1,2],[3,4]],[[5,6],[7,8]]]*2",
        "[[[1,2],[3,4]],[[5,6],[7,8]]]+[[[1,0],[0,1]],[[2,2],[2,2]]]",
        "sum([[[1,2],[3,4]],[[5,6],[7,8]]],1)",
        "sum([[[1,2],[3,4]],[[5,6],[7,8]]],3)",
        "slice([[[1,2],[3,4]],[[5,6],[7,8]]],2)",
        "shape([[[1,2,3],[4,5,6]],[[7,8,9],[1,2,3]]])",
    ),
    "Quant finance" to listOf(
        "fv(0.05,10,1000)", "pv(0.07,5,10000)", "cont(1000,0.05,10)", "pmt(0.005,360,300000)",
        "npv(0.1,-1000,300,400,500)", "irr(-1000,300,400,500)", "cagr(100,200,5)", "bond(1000,0.05,0.06,10,2)",
        "bscall(100,100,1,0.05,0.2)", "bsput(100,100,1,0.05,0.2)", "sharpe(0.12,0.03,0.15)", "normcdf(1.96)",
    ),
    "Option pricing models" to listOf(
        "bsmcall(100,100,1,0.05,0.2,0.02)", "bsmput(100,100,1,0.05,0.2,0.02)", "merton(100,100,1,0.05,0.2,1,-0.1,0.15)",
        "credit(120,100,5,0.05,0.25)", "binomial(100,100,1,0.05,0.2,500,2)", "ivol(10.45,100,100,1,0.05)",
    ),
    "Statistics" to listOf("stats(3,1,4,1,5,9,2,6)", "mean(2,4,4,4,5,5,7,9)", "median(5,1,3,8)", "stdev(10,12,23,23,16,23,21,16)"),
    "Number theory" to listOf("20!", "factor(360)", "gcd(48,18)", "nCr(10,3)"),
    "Polynomials" to listOf("factor(x^2-5x+6)", "expand((x+1)^3)", "simplify((x^2-1)/(x-1))", "1/x+1/(x+1)"),
)

/** Plain-language captions for examples whose syntax isn't self-explanatory. */
private val captions = mapOf(
    "[[0,-1],[1,0]]" to "Rotation by 90°",
    "det([[1,2],[2,4]])" to "A matrix that flattens the plane",
    "[[[1,2],[3,4]],[[5,6],[7,8]]]*2" to "Scale every entry",
    "[[[1,2],[3,4]],[[5,6],[7,8]]]+[[[1,0],[0,1]],[[2,2],[2,2]]]" to "Add two 3D matrices",
    "sum([[[1,2],[3,4]],[[5,6],[7,8]]],1)" to "Collapse the slices (axis 1)",
    "sum([[[1,2],[3,4]],[[5,6],[7,8]]],3)" to "Row totals of every slice (axis 3)",
    "slice([[[1,2],[3,4]],[[5,6],[7,8]]],2)" to "Take the second slice",
    "shape([[[1,2,3],[4,5,6]],[[7,8,9],[1,2,3]]])" to "Size along each axis",
    "fv(0.05,10,1000)" to "Future value · $1,000 at 5% for 10 years",
    "pv(0.07,5,10000)" to "Present value · $10,000 in 5 years at 7%",
    "cont(1000,0.05,10)" to "Continuous compounding · $1,000 at 5%",
    "pmt(0.005,360,300000)" to "Mortgage payment · $300k, 30 years",
    "npv(0.1,-1000,300,400,500)" to "Net present value at 10%",
    "irr(-1000,300,400,500)" to "Internal rate of return",
    "cagr(100,200,5)" to "Growth rate · doubled in 5 years",
    "bond(1000,0.05,0.06,10,2)" to "Bond price · 5% coupon, 6% yield",
    "bscall(100,100,1,0.05,0.2)" to "Call option · Black–Scholes, 20% vol",
    "bsput(100,100,1,0.05,0.2)" to "Put option · Black–Scholes, 20% vol",
    "sharpe(0.12,0.03,0.15)" to "Sharpe ratio · 12% return, 15% vol",
    "normcdf(1.96)" to "Normal probability below 1.96",
    "stats(3,1,4,1,5,9,2,6)" to "Summary statistics",
    "bsmcall(100,100,1,0.05,0.2,0.02)" to "Black–Scholes–Merton call · 2% dividend",
    "bsmput(100,100,1,0.05,0.2,0.02)" to "Black–Scholes–Merton put · 2% dividend",
    "merton(100,100,1,0.05,0.2,1,-0.1,0.15)" to "Merton jump-diffusion call",
    "credit(120,100,5,0.05,0.25)" to "Merton credit model · default risk",
    "binomial(100,100,1,0.05,0.2,500,2)" to "American put · 500-step binomial tree",
    "ivol(10.45,100,100,1,0.05)" to "Implied volatility from a $10.45 call",
    "surface(sin(x)*cos(y))" to "Egg-crate surface",
    "y''+y=0" to "Simple harmonic motion",
    "y'=2y" to "Exponential growth",
)

@Composable
fun ExploreScreen(onBack: () -> Unit, onPick: (String) -> Unit, onLicenses: () -> Unit = {}) {
    Column(Modifier.fillMaxSize().paperBackground()) {
        TopBar("Examples", onBack) {
            com.mathlens.app.ui.components.CircleIconButton(androidx.compose.material.icons.Icons.Outlined.Info, "About and licenses", size = 48.dp, onClick = onLicenses)
        }
        LazyVerticalGrid(
            GridCells.Fixed(2),
            contentPadding = PaddingValues(horizontal = 22.dp, vertical = 12.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            item(span = { GridItemSpan(2) }) {
                Column(Modifier.padding(top = 8.dp, bottom = 12.dp)) {
                    Text("A small collection\nof problems.", style = MaterialTheme.typography.displayMedium, color = Tone.Ink)
                    Spacer(Modifier.height(12.dp))
                    Text("Tap any problem to see it solved step by step, then drawn.", style = MaterialTheme.typography.bodyLarge, color = Tone.Graphite)
                }
            }
            topics.forEach { (title, list) ->
                item(span = { GridItemSpan(2) }) { SectionLabel(title, Modifier.padding(top = 22.dp, bottom = 2.dp)) }
                items(list) { q ->
                    val latex = remember(q) { MathEngine.previewLatex(q) ?: q }
                    PaperCard(Modifier.fillMaxWidth().height(96.dp).bouncyClick { onPick(q) }) {
                        Column(Modifier.padding(14.dp), verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)) {
                            captions[q]?.let { Text(it, style = MaterialTheme.typography.labelMedium, color = Tone.Graphite, maxLines = 2) }
                            MathView(latex, fontSize = if (captions[q] != null) 13.sp else 17.sp, fit = MathFit.Shrink, color = if (captions[q] != null) Tone.Stone else Tone.Ink)
                        }
                    }
                }
            }
            item(span = { GridItemSpan(2) }) {
                Text("Tap any card to see the full step-by-step solution.", style = MaterialTheme.typography.bodySmall, color = Tone.Stone, modifier = Modifier.padding(8.dp))
            }
        }
    }
}
