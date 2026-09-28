// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.wallpaper

import android.app.WallpaperManager
import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import java.util.Calendar
import java.util.concurrent.TimeUnit

/**
 * A new artwork every day: rotates through the designs (and palettes) by day, with gently varied parameters,
 * renders at screen size and sets it as the wallpaper (home, lock or both, as chosen in the studio).
 */
class DailyWallpaperWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {
    override suspend fun doWork(): Result = runCatching {
        val ctx = applicationContext
        val day = Calendar.getInstance().get(Calendar.DAY_OF_YEAR)
        val design = artDesigns[day % artDesigns.size]
        val style = wallStyles[(day / artDesigns.size + day) % wallStyles.size]
        val rnd = java.util.Random(day.toLong() * 7919)
        val values = design.params.associate { p ->
            val v = p.min + rnd.nextFloat() * (p.max - p.min)
            p.key to if (p.whole) Math.round(v).toFloat() else v
        }
        val m = ctx.resources.displayMetrics
        val w = m.widthPixels.coerceAtMost(1440); val h = (w * m.heightPixels.toFloat() / m.widthPixels).toInt()
        val bmp = ArtRenderer.render(ctx, design, values, WallOptions(style = style, showEquation = false), w, h)
        WallpaperManager.getInstance(ctx).setBitmap(bmp, null, true, DailyWallpaper.target(ctx))
        Result.success()
    }.getOrElse { Result.retry() }
}

object DailyWallpaper {
    private const val WORK = "daily-wallpaper"
    private fun prefs(c: Context) = c.getSharedPreferences("wallpaper", Context.MODE_PRIVATE)

    fun enabled(c: Context) = prefs(c).getBoolean("daily", false)
    fun target(c: Context) = prefs(c).getInt("target", WallpaperManager.FLAG_SYSTEM or WallpaperManager.FLAG_LOCK)

    fun enable(c: Context, target: Int) {
        prefs(c).edit().putBoolean("daily", true).putInt("target", target).apply()
        val req = PeriodicWorkRequestBuilder<DailyWallpaperWorker>(1, TimeUnit.DAYS).build()
        WorkManager.getInstance(c).enqueueUniquePeriodicWork(WORK, ExistingPeriodicWorkPolicy.UPDATE, req)
    }

    fun disable(c: Context) {
        prefs(c).edit().putBoolean("daily", false).apply()
        WorkManager.getInstance(c).cancelUniqueWork(WORK)
    }
}
