// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.ui.screens

import androidx.compose.foundation.layout.offset

import androidx.compose.ui.input.pointer.pointerInput

import androidx.compose.foundation.gestures.detectVerticalDragGestures

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.ImageDecoder
import android.graphics.Matrix
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageCaptureException
import androidx.camera.core.ImageProxy
import androidx.camera.view.LifecycleCameraController
import androidx.camera.view.PreviewView
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.FlashOff
import androidx.compose.material.icons.outlined.FlashOn
import androidx.compose.material.icons.outlined.History
import androidx.compose.material.icons.outlined.Keyboard
import androidx.compose.material.icons.outlined.PhotoLibrary
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import com.mathlens.app.camera.Candidate
import com.mathlens.app.camera.Candidates
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.ui.components.CircleIconButton
import com.mathlens.app.ui.components.GradientButton
import com.mathlens.app.ui.components.bouncyClick
import com.mathlens.app.ui.math.MathFit
import com.mathlens.app.ui.math.MathView
import com.mathlens.app.ui.theme.Tone
import com.mathlens.app.ui.components.ButtonStyle
import com.mathlens.app.ui.components.Hairline
import com.mathlens.app.ui.components.SectionLabel
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.withStyle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlin.coroutines.resume


@Composable
fun ScanScreen(
    onSolve: (String) -> Unit,
    onGraph: (String) -> Unit,
    onEdit: (String) -> Unit,
    onKeyboard: () -> Unit,
    onHistory: () -> Unit,
    onGraphStudio: () -> Unit,
    onExplore: () -> Unit,
) {
    val context = LocalContext.current
    val lifecycle = LocalLifecycleOwner.current
    val scope = rememberCoroutineScope()
    var hasPermission by remember {
        mutableStateOf(ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED)
    }
    val permissionLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { hasPermission = it }
    LaunchedEffect(Unit) { if (!hasPermission) permissionLauncher.launch(Manifest.permission.CAMERA) }

    val hasAnyCamera = remember { context.packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY) }
    var cameraError by remember { mutableStateOf<String?>(null) }
    // light status bar icons over the dark viewfinder; restore dark icons when leaving
    val view = androidx.compose.ui.platform.LocalView.current
    androidx.compose.runtime.DisposableEffect(Unit) {
        val window = (view.context as? android.app.Activity)?.window
        val ctl = window?.let { androidx.core.view.WindowCompat.getInsetsController(it, view) }
        ctl?.isAppearanceLightStatusBars = false
        onDispose { ctl?.isAppearanceLightStatusBars = true }
    }
    val controller = remember {
        LifecycleCameraController(context).apply {
            setEnabledUseCases(LifecycleCameraController.IMAGE_CAPTURE or LifecycleCameraController.IMAGE_ANALYSIS)
            imageCaptureMode = ImageCapture.CAPTURE_MODE_MINIMIZE_LATENCY
            imageAnalysisBackpressureStrategy = androidx.camera.core.ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST
            // Prefer the back camera but fall back to whatever exists (tablets, emulators).
            initializationFuture.addListener({
                runCatching {
                    cameraSelector = when {
                        hasCamera(CameraSelector.DEFAULT_BACK_CAMERA) -> CameraSelector.DEFAULT_BACK_CAMERA
                        hasCamera(CameraSelector.DEFAULT_FRONT_CAMERA) -> CameraSelector.DEFAULT_FRONT_CAMERA
                        else -> { cameraError = "No camera found. Use the keyboard or pick a photo instead."; cameraSelector }
                    }
                }.onFailure { cameraError = "Camera unavailable. Use the keyboard or pick a photo instead." }
            }, ContextCompat.getMainExecutor(context))
        }
    }
    var torch by remember { mutableStateOf(false) }
    var busy by remember { mutableStateOf(false) }
    var candidates by remember { mutableStateOf<List<Candidate>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var viewSize by remember { mutableStateOf(IntSize.Zero) }

    // scan frame in view coordinates (fractions of the preview)
    val frameTop = 0.24f
    // taller by default; drag the bottom edge to fit a longer system (up to half the screen)
    var frameHeight by remember { mutableStateOf(0.30f) }
    val frameInset = 0.07f

    // on-device math recognition only (pix2text-MFR, MIT, run with ONNX Runtime, MIT): no cloud, no proprietary fallback
    val mathModel = remember { com.mathlens.app.camera.MfrRecognizer.get(context) }

    suspend fun recognize(bitmap: Bitmap) {
        val mathLines = mathModel?.let { runCatching { it.lines(bitmap) }.getOrNull() }.orEmpty()
        val list = withContext(Dispatchers.Default) { Candidates.from(mathLines) }
        if (list.isEmpty()) error = "No math found. Center the problem inside the frame."
        else candidates = list
    }

    // ---- live recognition. Watches the framed area on every analysis frame (cheap thumbnail): when the phone
    // is held steady on something with ink that hasn't been read yet, reads it once with the math model and
    // shows the result. Moving to a new problem re-arms it. The shutter stays as a fallback.
    var live by remember { mutableStateOf<Candidate?>(null) }
    var liveHint by remember { mutableStateOf(LiveHint.Idle) }
    val latestSize = androidx.compose.runtime.rememberUpdatedState(viewSize)
    val paused = androidx.compose.runtime.rememberUpdatedState(busy || candidates != null)
    androidx.compose.runtime.DisposableEffect(controller) {
        var prevThumb: FloatArray? = null
        var readThumb: FloatArray? = null     // the scene last read; not read again until the view changes
        var steadySince = 0L
        var reading = false
        val executor = java.util.concurrent.Executors.newSingleThreadExecutor()
        fun post(block: () -> Unit) = ContextCompat.getMainExecutor(context).execute(block)
        controller.setImageAnalysisAnalyzer(executor) { proxy ->
            if (paused.value || reading || latestSize.value == IntSize.Zero) { proxy.close(); return@setImageAnalysisAnalyzer }
            val bmp = runCatching {
                val raw = proxy.toBitmap()
                val m = Matrix().apply { postRotate(proxy.imageInfo.rotationDegrees.toFloat()) }
                Bitmap.createBitmap(raw, 0, 0, raw.width, raw.height, m, true)
            }.getOrNull()
            proxy.close()
            if (bmp == null) return@setImageAnalysisAnalyzer
            val crop = cropToFrame(bmp, latestSize.value, frameTop, frameHeight, frameInset)
            val thumb = grayThumb(crop)
            val now = System.currentTimeMillis()
            val motion = prevThumb?.let { meanDiff(it, thumb) } ?: 1f
            prevThumb = thumb
            if (!hasInk(thumb)) { steadySince = 0L; post { if (liveHint != LiveHint.Idle) liveHint = LiveHint.Idle }; return@setImageAnalysisAnalyzer }
            if (motion > 0.035f) { steadySince = 0L; post { liveHint = LiveHint.Steady }; return@setImageAnalysisAnalyzer }
            if (steadySince == 0L) steadySince = now
            val seen = readThumb?.let { meanDiff(it, thumb) < 0.06f } ?: false
            if (seen || now - steadySince < 450) return@setImageAnalysisAnalyzer
            // steady on something new: read it once
            reading = true
            post { liveHint = LiveHint.Reading }
            val lines = kotlinx.coroutines.runBlocking {
                mathModel?.let { runCatching { it.lines(crop) }.getOrNull() }.orEmpty()
            }
            readThumb = thumb
            reading = false
            val best = Candidates.from(lines).firstOrNull { it.latex != null }
            post {
                liveHint = LiveHint.Idle
                live = best ?: live
            }
        }
        onDispose { controller.clearImageAnalysisAnalyzer(); executor.shutdown() }
    }

    val galleryLauncher = rememberLauncherForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri: Uri? ->
        if (uri != null) scope.launch {
            busy = true
            val bmp = withContext(Dispatchers.IO) { decodeUri(context, uri) }
            if (bmp != null) recognize(bmp) else error = "Couldn't open that image."
            busy = false
        }
    }

    fun capture() {
        if (busy) return
        if (!hasPermission || !hasAnyCamera || cameraError != null) { onKeyboard(); return }
        busy = true
        error = null
        scope.launch {
            val bmp = takePicture(context, controller)
            if (bmp == null) { error = "Camera capture failed."; busy = false; return@launch }
            // crop to the scan frame; PreviewView uses FILL_CENTER so map view fractions to the image
            val cropped = withContext(Dispatchers.Default) { cropToFrame(bmp, viewSize, frameTop, frameHeight, frameInset) }
            recognize(cropped)
            busy = false
        }
    }

    Column(Modifier.fillMaxSize().background(Tone.Paper)) {
    Box(Modifier.weight(1f).fillMaxWidth().clip(RoundedCornerShape(bottomStart = 28.dp, bottomEnd = 28.dp)).background(Tone.Ink).onSizeChanged { viewSize = it }) {
        if (hasPermission && hasAnyCamera && cameraError == null) {
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { ctx ->
                    PreviewView(ctx).apply {
                        scaleType = PreviewView.ScaleType.FILL_CENTER
                        this.controller = controller
                        controller.bindToLifecycle(lifecycle)
                    }
                },
            )
        } else {
            Column(Modifier.align(Alignment.Center).padding(36.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                val msg = cameraError ?: if (!hasAnyCamera) "This device has no camera. Write the problem instead, or choose a photograph."
                else "Allow the camera to read problems from paper, a notebook, or a screen."
                Text(msg, color = Tone.Ivory.copy(alpha = 0.85f), style = MaterialTheme.typography.bodyLarge)
                Spacer(Modifier.height(20.dp))
                if (!hasPermission && hasAnyCamera) GradientButton("Allow camera", style = ButtonStyle.Quiet) { permissionLauncher.launch(Manifest.permission.CAMERA) }
                else GradientButton("Write it instead", style = ButtonStyle.Quiet, onClick = onKeyboard)
            }
        }

        ScanOverlay(frameTop, frameHeight, frameInset, busy || liveHint == LiveHint.Reading)
        // resize handle on the frame's bottom edge
        val density = androidx.compose.ui.platform.LocalDensity.current
        if (viewSize.height > 0) Box(
            Modifier
                .align(Alignment.TopCenter)
                .offset { androidx.compose.ui.unit.IntOffset(0, (viewSize.height * (frameTop + frameHeight)).toInt() - with(density) { 24.dp.roundToPx() }) }
                .size(width = 120.dp, height = 48.dp)
                .pointerInput(viewSize) {
                    detectVerticalDragGestures { change, dy ->
                        change.consume()
                        frameHeight = (frameHeight + dy / viewSize.height).coerceIn(0.16f, 0.50f)
                    }
                },
            contentAlignment = Alignment.Center,
        ) {
            Box(Modifier.size(width = 44.dp, height = 4.dp).clip(RoundedCornerShape(2.dp)).background(Tone.Ivory.copy(alpha = 0.9f)))
        }

        // wordmark and quiet controls over the image
        Row(
            Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 22.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                // the same quiet tracked capitals as onboarding: a label, not a logo competing with the view
                Text("MATHLENS", style = MaterialTheme.typography.labelSmall.copy(fontSize = 13.sp, letterSpacing = 3.sp), color = Tone.Ivory.copy(alpha = 0.9f))
            }
            CircleIconButton(if (torch) Icons.Outlined.FlashOn else Icons.Outlined.FlashOff, "Flash", background = Color(0x33FFFFFF), border = Color(0x44FFFFFF), tint = Tone.Ivory) {
                torch = !torch
                controller.enableTorch(torch)
            }
        }
        Text(
            when (liveHint) {
                LiveHint.Idle -> "Point at a problem"
                LiveHint.Steady -> "Hold steady…"
                LiveHint.Reading -> "Reading…"
            },
            color = Tone.Ivory.copy(alpha = 0.9f),
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 14.sp, letterSpacing = 1.4.sp),
            modifier = Modifier.align(Alignment.TopCenter).padding(top = with(androidx.compose.ui.platform.LocalDensity.current) { ((viewSize.height * frameTop).toDp() - 30.dp).coerceAtLeast(0.dp) }),
        )
        androidx.compose.animation.AnimatedVisibility(
            visible = live != null && candidates == null && !busy,
            enter = fadeIn(tween(350)) + slideInVertically(tween(350)) { it / 3 },
            exit = fadeOut(tween(250)),
            modifier = Modifier.align(Alignment.TopCenter).padding(top = with(androidx.compose.ui.platform.LocalDensity.current) {
                (viewSize.height * (frameTop + frameHeight)).toDp() + 18.dp
            }),
        ) {
            val c = live
            if (c != null) {
                Row(
                    Modifier.padding(horizontal = 24.dp).fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Tone.Ivory)
                        .bouncyClick { onSolve(c.text) }.padding(start = 18.dp, end = 8.dp, top = 10.dp, bottom = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        SectionLabel(if (c.isSystem) "System detected" else "Detected")
                        Box(Modifier.padding(top = 4.dp)) { MathView(c.latex ?: c.text, fontSize = 20.sp, fit = MathFit.Shrink, color = Tone.Ink) }
                    }
                    CircleIconButton(Icons.Outlined.Edit, "Edit", size = 48.dp) { onEdit(c.text) }
                    Spacer(Modifier.width(6.dp))
                    GradientButton("Solve", height = 42.dp) { onSolve(c.text) }
                }
            }
        }
        error?.let {
            Text(it, color = Tone.Ink, style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.align(Alignment.BottomCenter).padding(20.dp).clip(RoundedCornerShape(4.dp)).background(Tone.Ivory).padding(horizontal = 16.dp, vertical = 10.dp))
        }
    }

    // paper control deck
    Row(
        Modifier.fillMaxWidth().navigationBarsPadding().padding(horizontal = 28.dp, vertical = 22.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            CircleIconButton(Icons.Outlined.PhotoLibrary, "Gallery") {
                galleryLauncher.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
            }
            CircleIconButton(Icons.Outlined.History, "History", onClick = onHistory)
        }
        ShutterButton(busy) { capture() }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            CircleIconButton(Icons.Outlined.Keyboard, "Keyboard", onClick = onKeyboard)
            CircleIconButton(Icons.AutoMirrored.Outlined.MenuBook, "Examples", onClick = onExplore)
        }
    }
    }

    // results sheet
    AnimatedVisibility(
        visible = candidates != null,
        enter = slideInVertically(tween(500)) { it } + fadeIn(),
        exit = slideOutVertically(tween(400)) { it } + fadeOut(),
    ) {
        Box(Modifier.fillMaxSize().background(Color(0x66151413))) {
            val list = candidates ?: emptyList()
            var selected by remember(list) { mutableStateOf(0) }
            Column(
                Modifier.align(Alignment.BottomCenter).fillMaxWidth()
                    .clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp)).background(Tone.Ivory)
                    .navigationBarsPadding().padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(if (list.firstOrNull()?.isSystem == true) "A system of equations" else "What we read", style = MaterialTheme.typography.headlineMedium, color = Tone.Ink, modifier = Modifier.weight(1f))
                    CircleIconButton(Icons.Outlined.Close, "Close", size = 46.dp) { candidates = null }
                }
                Spacer(Modifier.height(8.dp))
                list.forEachIndexed { i, c ->
                    val isSel = i == selected
                    Row(Modifier.fillMaxWidth().bouncyClick { selected = i }.padding(vertical = 14.dp), verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(8.dp).clip(CircleShape).background(if (isSel) Tone.Terracotta else Tone.Hairline))
                        Spacer(Modifier.width(16.dp))
                        Box(Modifier.weight(1f)) {
                            if (c.latex != null) MathView(c.latex, fontSize = 24.sp, fit = MathFit.Shrink, color = if (isSel) Tone.Ink else Tone.Stone)
                            else Text(c.text.split(';').joinToString("\n") { it.trim() }, color = Tone.Stone, style = MaterialTheme.typography.bodyLarge)
                        }
                    }
                    Hairline()
                }
                Spacer(Modifier.height(16.dp))
                val chosen = list.getOrNull(selected)?.text ?: ""
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CircleIconButton(Icons.Outlined.Edit, "Edit", size = 54.dp) { candidates = null; onEdit(chosen) }
                    GradientButton("Graph", Modifier.weight(1f), style = ButtonStyle.Outline) { candidates = null; onGraph(chosen) }
                    GradientButton("Solve", Modifier.weight(1.3f)) { candidates = null; onSolve(chosen) }
                }
            }
        }
    }
}

@Composable
private fun ScanOverlay(top: Float, height: Float, inset: Float, busy: Boolean) {
    val t = rememberInfiniteTransition(label = "scan")
    val breathe by t.animateFloat(0f, 1f, infiniteRepeatable(tween(2400, easing = LinearEasing), RepeatMode.Reverse), label = "breathe")
    Canvas(Modifier.fillMaxSize().graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)) {
        val rect = Rect(Offset(size.width * inset, size.height * top), Size(size.width * (1 - 2 * inset), size.height * height))
        val r = 6.dp.toPx()
        drawRect(Color(0x73151413))
        drawRoundRect(Color.Transparent, rect.topLeft, rect.size, CornerRadius(r), blendMode = BlendMode.Clear)
        // fine ivory corners
        val len = 26.dp.toPx()
        val sw = 1.5.dp.toPx()
        val c = Tone.Ivory
        fun corner(x: Float, y: Float, dx: Float, dy: Float) {
            drawLine(c, Offset(x, y), Offset(x + dx * len, y), sw, StrokeCap.Square)
            drawLine(c, Offset(x, y), Offset(x, y + dy * len), sw, StrokeCap.Square)
        }
        corner(rect.left, rect.top, 1f, 1f)
        corner(rect.right, rect.top, -1f, 1f)
        corner(rect.left, rect.bottom, 1f, -1f)
        corner(rect.right, rect.bottom, -1f, -1f)
        // a single terracotta hairline drifting while reading
        if (busy) {
            val y = rect.top + rect.height * breathe
            drawLine(Tone.Terracotta, Offset(rect.left + 12.dp.toPx(), y), Offset(rect.right - 12.dp.toPx(), y), 1.5.dp.toPx())
        }
    }
}

@Composable
private fun ShutterButton(busy: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.size(78.dp).clip(CircleShape).border(1.dp, Tone.Ink, CircleShape).bouncyClick(!busy, onClick),
        contentAlignment = Alignment.Center,
    ) {
        Box(Modifier.size(62.dp).clip(CircleShape).background(Tone.Ink), contentAlignment = Alignment.Center) {
            if (busy) CircularProgressIndicator(color = Tone.Ivory, strokeWidth = 1.5.dp, modifier = Modifier.size(26.dp))
        }
    }
}

private suspend fun takePicture(context: Context, controller: LifecycleCameraController): Bitmap? =
    suspendCancellableCoroutine { cont ->
        controller.takePicture(ContextCompat.getMainExecutor(context), object : ImageCapture.OnImageCapturedCallback() {
            override fun onCaptureSuccess(image: ImageProxy) {
                val bmp = runCatching {
                    val raw = image.toBitmap()
                    val m = Matrix().apply { postRotate(image.imageInfo.rotationDegrees.toFloat()) }
                    Bitmap.createBitmap(raw, 0, 0, raw.width, raw.height, m, true)
                }.getOrNull()
                image.close()
                cont.resume(bmp)
            }
            override fun onError(exception: ImageCaptureException) { cont.resume(null) }
        })
    }

/** Maps the on-screen frame (view fractions, FILL_CENTER) to the captured bitmap and crops it. */
private fun cropToFrame(bmp: Bitmap, view: IntSize, top: Float, height: Float, inset: Float): Bitmap {
    if (view.width == 0 || view.height == 0) return bmp
    val scale = maxOf(view.width.toFloat() / bmp.width, view.height.toFloat() / bmp.height)
    val shownW = view.width / scale
    val shownH = view.height / scale
    val offX = (bmp.width - shownW) / 2
    val offY = (bmp.height - shownH) / 2
    // add a little margin around the frame
    val l = (offX + shownW * (inset - 0.03f)).toInt().coerceIn(0, bmp.width - 1)
    val t = (offY + shownH * (top - 0.03f)).toInt().coerceIn(0, bmp.height - 1)
    val r = (offX + shownW * (1 - inset + 0.03f)).toInt().coerceIn(l + 1, bmp.width)
    val b = (offY + shownH * (top + height + 0.03f)).toInt().coerceIn(t + 1, bmp.height)
    return Bitmap.createBitmap(bmp, l, t, r - l, b - t)
}

private fun decodeUri(context: Context, uri: Uri): Bitmap? = runCatching {
    ImageDecoder.decodeBitmap(ImageDecoder.createSource(context.contentResolver, uri)) { d, _, _ ->
        d.allocator = ImageDecoder.ALLOCATOR_SOFTWARE
        d.isMutableRequired = false
    }
}.getOrNull()

private enum class LiveHint { Idle, Steady, Reading }

private const val THUMB_W = 96
private const val THUMB_H = 48

/** Grayscale 96x48 thumbnail of the framed area, 0..1. */
private fun grayThumb(src: Bitmap): FloatArray {
    val small = Bitmap.createScaledBitmap(src, THUMB_W, THUMB_H, true)
    val px = IntArray(THUMB_W * THUMB_H)
    small.getPixels(px, 0, THUMB_W, 0, 0, THUMB_W, THUMB_H)
    return FloatArray(px.size) { i ->
        val c = px[i]
        (0.299f * ((c shr 16) and 0xFF) + 0.587f * ((c shr 8) and 0xFF) + 0.114f * (c and 0xFF)) / 255f
    }
}

/** How much the view changed between two thumbnails (mean absolute difference, 0..1). */
private fun meanDiff(a: FloatArray, b: FloatArray): Float {
    var d = 0f
    for (i in a.indices) d += kotlin.math.abs(a[i] - b[i])
    return d / a.size
}

/** Writing on paper: a small share of clearly darker pixels than the page around them. */
private fun hasInk(t: FloatArray): Boolean {
    val mean = t.average().toFloat()
    val dark = t.count { it < mean - 0.16f }
    val share = dark.toFloat() / t.size
    return share in 0.004f..0.35f
}
