// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import com.mathlens.app.ui.theme.MathLensTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        val splash = installSplashScreen()
        // quiet exit: the mark settles and fades into the page
        splash.setOnExitAnimationListener { provider ->
            provider.iconView.animate().alpha(0f).scaleX(0.92f).scaleY(0.92f).setDuration(420).start()
            provider.view.animate().alpha(0f).setStartDelay(180).setDuration(420).withEndAction { provider.remove() }.start()
        }
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.light(android.graphics.Color.TRANSPARENT, android.graphics.Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.light(android.graphics.Color.TRANSPARENT, android.graphics.Color.TRANSPARENT),
        )
        super.onCreate(savedInstanceState)
        pendingQuery.value = queryFrom(intent)
        setContent { MathLensTheme { AppNav(pendingQuery) } }
    }

    private val pendingQuery = androidx.compose.runtime.mutableStateOf<String?>(null)

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        queryFrom(intent)?.let { pendingQuery.value = it }
    }

    /** Text shared into the app, or the `query` extra (handy for testing: adb shell am start -e query "x^2=4"). */
    private fun queryFrom(intent: android.content.Intent?): String? = when {
        intent == null -> null
        intent.action == android.content.Intent.ACTION_SEND -> intent.getStringExtra(android.content.Intent.EXTRA_TEXT)
        intent.hasExtra("query64") -> String(android.util.Base64.decode(intent.getStringExtra("query64"), android.util.Base64.DEFAULT))
        else -> intent.getStringExtra("query")
    }?.trim()?.takeIf { it.isNotEmpty() }
}
