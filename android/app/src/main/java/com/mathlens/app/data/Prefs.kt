// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.data

import android.content.Context

/** Tiny persistent flags. */
class Prefs(context: Context) {
    private val p = context.getSharedPreferences("prefs", Context.MODE_PRIVATE)
    var onboarded: Boolean
        get() = p.getBoolean("onboarded", false)
        set(v) = p.edit().putBoolean("onboarded", v).apply()
}
