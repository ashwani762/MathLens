// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.camera

import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import com.mathlens.app.engine.MathEngine
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import java.nio.FloatBuffer
import java.nio.LongBuffer

/**
 * On-device handwritten + printed math recognition: pix2text-MFR 1.5 (MIT), quantised to int8,
 * run with ONNX Runtime (MIT). Image -> LaTeX -> engine syntax, entirely offline.
 *
 * Pipeline: split into text lines (horizontal ink profile) -> tight crop per line ->
 * 384x384 RGB, normalised to [-1, 1] -> encoder once -> greedy decoder (no KV cache) ->
 * byte-level BPE detokenise -> strip spacing commands -> LaTeX import in the C++ engine.
 */
class MfrRecognizer private constructor(
    private val env: OrtEnvironment,
    private val encoder: OrtSession,
    private val decoder: OrtSession,
    private val vocab: Array<String>,
) : MathRecognizer {

    companion object {
        private const val SIZE = 384
        private const val BOS = 1L
        private const val EOS = 2L
        private const val MAX_TOKENS = 160

        @Volatile private var shared: MfrRecognizer? = null

        /** Loads the model once (about 32 MB) and reuses it. Returns null if assets are missing. */
        fun get(context: Context): MfrRecognizer? = shared ?: synchronized(this) {
            shared ?: runCatching { create(context.applicationContext) }.getOrNull()?.also { shared = it }
        }

        private fun create(context: Context): MfrRecognizer {
            val env = OrtEnvironment.getEnvironment()
            val opts = OrtSession.SessionOptions().apply {
                setIntraOpNumThreads(Runtime.getRuntime().availableProcessors().coerceIn(2, 4))
                setOptimizationLevel(OrtSession.SessionOptions.OptLevel.ALL_OPT)
            }
            val enc = env.createSession(context.assets.open("ocr/mfr_encoder.onnx").use { it.readBytes() }, opts)
            val dec = env.createSession(context.assets.open("ocr/mfr_decoder.onnx").use { it.readBytes() }, opts)
            val arr = JSONArray(context.assets.open("ocr/mfr_vocab.json").bufferedReader().readText())
            val vocab = Array(arr.length()) { arr.getString(it) }
            return MfrRecognizer(env, enc, dec, vocab)
        }

        // GPT-2 byte-level BPE: unicode char -> byte
        private val byteDecoder: Map<Char, Int> by lazy {
            val bs = (('!'.code..'~'.code) + ('¡'.code..'¬'.code) + ('®'.code..'ÿ'.code)).toMutableList()
            val cs = bs.toMutableList()
            var n = 0
            for (b in 0 until 256) if (b !in bs) { bs.add(b); cs.add(256 + n); n++ }
            bs.indices.associate { cs[it].toChar() to bs[it] }
        }
    }

    /** Text lines in engine syntax, top to bottom (several lines = a system of equations). */
    override suspend fun lines(bitmap: Bitmap): List<String> = withContext(Dispatchers.Default) {
        splitLines(bitmap).mapNotNull { band ->
            val latex = recognizeLatex(band)
            if (latex.isBlank()) null else MathEngine.fromLatex(latex).ifBlank { null }
        }
    }

    /** Recognises one formula image into cleaned LaTeX. */
    fun recognizeLatex(img: Bitmap): String {
        val pixels = preprocess(img)
        val hidden = OnnxTensor.createTensor(env, FloatBuffer.wrap(pixels), longArrayOf(1, 3, SIZE.toLong(), SIZE.toLong())).use { input ->
            encoder.run(mapOf("pixel_values" to input)).use { out ->
                val t = out[0] as OnnxTensor
                val shape = t.info.shape
                val buf = t.floatBuffer
                val copy = FloatArray(buf.remaining()).also { buf.get(it) }
                copy to shape
            }
        }
        val ids = ArrayList<Long>().apply { add(BOS) }
        OnnxTensor.createTensor(env, FloatBuffer.wrap(hidden.first), hidden.second).use { hs ->
            repeat(MAX_TOKENS) {
                val next = OnnxTensor.createTensor(env, LongBuffer.wrap(ids.toLongArray()), longArrayOf(1, ids.size.toLong())).use { inp ->
                    decoder.run(mapOf("input_ids" to inp, "encoder_hidden_states" to hs)).use { out ->
                        val logits = (out[0] as OnnxTensor).floatBuffer
                        val vocabSize = vocab.size
                        val offset = (ids.size - 1) * vocabSize
                        var best = 0; var bestV = Float.NEGATIVE_INFINITY
                        for (v in 0 until vocabSize) { val x = logits.get(offset + v); if (x > bestV) { bestV = x; best = v } }
                        best.toLong()
                    }
                }
                if (next == EOS) return@use
                ids.add(next)
                // repetition guard: stop on a long run of the same token (e.g. \quad \quad ...)
                if (ids.size > 8 && ids.takeLast(6).toSet().size == 1) return@use
            }
        }
        return clean(detokenize(ids.drop(1)))
    }

    private fun detokenize(ids: List<Long>): String {
        val text = StringBuilder()
        for (id in ids) if (id > 4 && id < vocab.size) text.append(vocab[id.toInt()])
        val bytes = text.map { (byteDecoder[it] ?: '?'.code).toByte() }.toByteArray()
        return bytes.toString(Charsets.UTF_8)
    }

    private fun clean(latex: String): String = latex
        .replace(Regex("\\\\(quad|qquad|,|;|:|!)\\s*"), " ")
        .replace(Regex("\\\\(displaystyle|textstyle)"), "")
        .replace(Regex("\\s+"), " ")
        .trim()

    /** Crop to ink (with a margin relative to the ink height), stretch to 384x384, normalise. */
    private fun preprocess(src: Bitmap): FloatArray {
        val img = autocrop(src)
        val scaled = Bitmap.createScaledBitmap(img, SIZE, SIZE, true)
        val px = IntArray(SIZE * SIZE)
        scaled.getPixels(px, 0, SIZE, 0, 0, SIZE, SIZE)
        val out = FloatArray(3 * SIZE * SIZE)
        val plane = SIZE * SIZE
        for (i in 0 until plane) {
            val c = px[i]
            out[i] = ((c shr 16 and 0xFF) / 255f - 0.5f) / 0.5f
            out[plane + i] = ((c shr 8 and 0xFF) / 255f - 0.5f) / 0.5f
            out[2 * plane + i] = ((c and 0xFF) / 255f - 0.5f) / 0.5f
        }
        return out
    }

    private fun gray(c: Int) = (0.299f * Color.red(c) + 0.587f * Color.green(c) + 0.114f * Color.blue(c))

    /** Tight crop to the dark strokes with a uniform white border. */
    private fun autocrop(src: Bitmap): Bitmap {
        val w = src.width; val h = src.height
        val px = IntArray(w * h)
        src.getPixels(px, 0, w, 0, 0, w, h)
        var sum = 0.0
        for (c in px) sum += gray(c)
        val thr = minOf(200f, (sum / px.size).toFloat() - 25f)
        var x0 = w; var y0 = h; var x1 = -1; var y1 = -1
        for (y in 0 until h) for (x in 0 until w) if (gray(px[y * w + x]) < thr) {
            if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y
        }
        if (x1 < 0) return src
        val m = maxOf(4, ((y1 - y0) * 0.12f).toInt())
        val rect = Rect((x0 - m).coerceAtLeast(0), (y0 - m).coerceAtLeast(0), (x1 + m).coerceAtMost(w - 1), (y1 + m).coerceAtMost(h - 1))
        // paint onto white so any transparency becomes paper
        val out = Bitmap.createBitmap(rect.width(), rect.height(), Bitmap.Config.ARGB_8888)
        Canvas(out).apply { drawColor(Color.WHITE); drawBitmap(src, rect, Rect(0, 0, rect.width(), rect.height()), Paint(Paint.FILTER_BITMAP_FLAG)) }
        return out
    }

    /** Splits a page region into horizontal bands of ink separated by blank rows (one equation per band). */
    private fun splitLines(src: Bitmap): List<Bitmap> {
        val w = src.width; val h = src.height
        val px = IntArray(w * h)
        src.getPixels(px, 0, w, 0, 0, w, h)
        var sum = 0.0
        for (c in px) sum += gray(c)
        val thr = minOf(200f, (sum / px.size).toFloat() - 25f)
        val rowInk = IntArray(h) { y -> var n = 0; for (x in 0 until w) if (gray(px[y * w + x]) < thr) n++; n }
        val minInk = maxOf(1, w / 400)
        val bands = mutableListOf<IntRange>()
        var start = -1
        var gap = 0
        val minGap = maxOf(6, h / 40)
        for (y in 0 until h) {
            if (rowInk[y] >= minInk) { if (start < 0) start = y; gap = 0 }
            else if (start >= 0) { gap++; if (gap >= minGap) { bands += start until y - gap + 1; start = -1; gap = 0 } }
        }
        if (start >= 0) bands += start until h
        val tall = bands.filter { it.last - it.first > h / 30 }
        if (tall.size <= 1) return listOf(src)
        return tall.map { r ->
            val pad = maxOf(6, (r.last - r.first) / 6)
            val top = (r.first - pad).coerceAtLeast(0); val bottom = (r.last + pad).coerceAtMost(h - 1)
            Bitmap.createBitmap(src, 0, top, w, bottom - top + 1)
        }
    }

    override fun close() { /* shared for the app's lifetime */ }
}
