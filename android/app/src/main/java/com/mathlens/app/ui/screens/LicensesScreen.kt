// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mathlens.app.ui.components.SectionLabel
import com.mathlens.app.ui.components.TopBar
import com.mathlens.app.ui.components.bouncyClick
import com.mathlens.app.ui.theme.PaperCard
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.theme.paperBackground

/** One work the app includes, with its license (full texts live in assets/licenses). */
private data class Notice(val name: String, val what: String, val copyright: String, val license: String, val file: String, val url: String)

private val notices = listOf(
    Notice("MathLens app", "The Android app: screens, camera, graphs and art", "Copyright (c) 2026 Ashwani", "MIT License", "MIT.txt", "https://opensource.org/license/mit"),
    Notice("MathLens math engine", "The portable C++ solver behind every answer", "Copyright (c) 2026 Ashwani", "GNU GPL v3 or later", "GPL-3.0.txt", "https://www.gnu.org/licenses/gpl-3.0.html"),
    Notice("pix2text-MFR 1.5", "On-device handwriting and print math recognition model, quantised to int8",
        "Copyright (c) breezedeus", "MIT License", "MIT.txt", "https://huggingface.co/breezedeus/pix2text-mfr-1.5"),
    Notice("ONNX Runtime", "Runs the recognition model on the device", "Copyright (c) Microsoft Corporation", "MIT License", "MIT.txt", "https://github.com/microsoft/onnxruntime"),
    Notice("KaTeX fonts", "Typefaces for rendering mathematics",
        "Copyright (c) 2009-2010 Design Science, Inc.\nCopyright (c) 2014-2018 Khan Academy", "SIL Open Font License 1.1", "OFL-1.1.txt", "https://katex.org"),
    Notice("Instrument Serif", "Display typeface", "Copyright 2022 The Instrument Serif Project Authors", "SIL Open Font License 1.1", "OFL-1.1.txt", "https://github.com/Instrument/instrument-serif"),
    Notice("Inter", "Interface typeface", "Copyright 2016 The Inter Project Authors", "SIL Open Font License 1.1", "OFL-1.1.txt", "https://github.com/rsms/inter"),
    Notice("Android Jetpack", "Compose, CameraX, Navigation, Lifecycle, WorkManager, Core", "Copyright The Android Open Source Project", "Apache License 2.0", "Apache-2.0.txt", "https://developer.android.com/jetpack"),
    Notice("Material Symbols", "Icons", "Copyright Google LLC", "Apache License 2.0", "Apache-2.0.txt", "https://fonts.google.com/icons"),
    Notice("Kotlin and kotlinx.coroutines", "Language runtime and coroutines", "Copyright JetBrains s.r.o. and Kotlin contributors", "Apache License 2.0", "Apache-2.0.txt", "https://kotlinlang.org"),
    Notice("LLVM libc++", "C++ standard library in the native engine", "Copyright the LLVM Project contributors", "Apache License 2.0 with LLVM exception", "Apache-2.0.txt", "https://libcxx.llvm.org"),
)

@Composable
fun LicensesScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    LazyColumn(
        Modifier.fillMaxSize().paperBackground(),
        contentPadding = PaddingValues(bottom = 32.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item { TopBar("Licenses", onBack) }
        item {
            Column(Modifier.padding(horizontal = 24.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Open source, and grateful for it.", style = MaterialTheme.typography.headlineSmall, color = Tone.Ink)
                Text(
                    "MathLens is free software. The app's code is under the MIT License and its math engine under the " +
                        "GNU General Public License v3, so the app as a whole is shared under the GPL v3: you may use, study, " +
                        "change and share it, and anything built on it stays open. Everything runs on your phone: no account, " +
                        "no cloud, no tracking. It builds on the work below.",
                    style = MaterialTheme.typography.bodyMedium, color = Tone.Graphite,
                )
            }
        }
        items(notices) { n ->
            var open by remember { mutableStateOf(false) }
            // reflow: join hard-wrapped lines inside each paragraph so the text wraps to the screen
            val text = remember(n.file, open) {
                if (!open) "" else runCatching { context.assets.open("licenses/" + n.file).bufferedReader().readText() }.getOrDefault("")
                    .split(Regex("\n\\s*\n")).joinToString("\n\n") { para -> para.lines().joinToString(" ") { it.trim() }.trim() }
            }
            PaperCard(Modifier.fillMaxWidth().padding(horizontal = 16.dp).bouncyClick { open = !open }) {
                Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    SectionLabel(n.license)
                    Text(n.name, style = MaterialTheme.typography.titleLarge, color = Tone.Ink)
                    Text(n.what, style = MaterialTheme.typography.bodyMedium, color = Tone.Graphite)
                    Text(n.copyright, style = MaterialTheme.typography.bodySmall, color = Tone.Stone)
                    Text(n.url, style = MaterialTheme.typography.bodySmall, color = Tone.Terracotta)
                    Text(if (open) "Hide the license" else "Read the license",
                        style = MaterialTheme.typography.labelLarge.copy(fontStyle = FontStyle.Italic), color = Tone.Terracotta)
                    AnimatedVisibility(open, enter = expandVertically() + fadeIn(), exit = shrinkVertically() + fadeOut()) {
                        Text(
                            (if (n.file == "MIT.txt") n.copyright + "\n\n" else "") + text,
                            style = MaterialTheme.typography.bodySmall.copy(fontSize = 12.sp, lineHeight = 17.sp),
                            color = Tone.Graphite, modifier = Modifier.padding(top = 8.dp),
                        )
                    }
                }
            }
        }
        item { Column(Modifier.navigationBarsPadding()) {} }
    }
}
