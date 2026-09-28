// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app

import android.net.Uri
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.mathlens.app.ui.screens.EditorScreen
import com.mathlens.app.ui.screens.ExploreScreen
import com.mathlens.app.ui.screens.GraphScreen
import com.mathlens.app.ui.screens.HistoryScreen
import com.mathlens.app.ui.screens.OnboardingScreen
import com.mathlens.app.data.Prefs
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import com.mathlens.app.ui.screens.ScanScreen
import com.mathlens.app.ui.screens.SolutionScreen
import com.mathlens.app.ui.screens.WallpaperScreen

object Routes {
    const val Intro = "intro"
    const val Scan = "scan"
    const val Editor = "editor?text={text}&mode={mode}"
    const val Solution = "solution"
    const val Graph = "graph"
    const val Wallpaper = "wallpaper"
    const val History = "history"
    const val Explore = "explore"
    const val Licenses = "licenses"
    const val SavedGraphs = "saved-graphs"
    fun editor(text: String = "", mode: String = "solve") = "editor?text=${Uri.encode(text)}&mode=$mode"
}

@Composable
fun AppNav(pending: androidx.compose.runtime.MutableState<String?>, vm: AppViewModel = viewModel()) {
    val nav = rememberNavController()
    val solution by vm.solution.collectAsStateWithLifecycle()
    val solving by vm.solving.collectAsStateWithLifecycle()
    val history by vm.history.collectAsStateWithLifecycle()
    val savedGraphs by vm.savedGraphs.collectAsStateWithLifecycle()
    val equations by vm.graphEquations.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val prefs = remember { Prefs(context) }
    // onboarding is shown once; afterwards the app opens straight into the camera
    val start = remember { if (prefs.onboarded) Routes.Scan else Routes.Intro }

    fun solve(q: String) { vm.solve(q); nav.navigate(Routes.Solution) }
    fun graph(q: String) {
        val eqs = q.split(";").map { it.trim() }.filter { it.isNotEmpty() }
        vm.setGraphEquations(eqs)
        nav.navigate(Routes.Graph)
    }

    // a problem shared into the app opens straight into its solution
    androidx.compose.runtime.LaunchedEffect(pending.value) {
        val q = pending.value ?: return@LaunchedEffect
        pending.value = null
        if (!prefs.onboarded) prefs.onboarded = true
        vm.solve(q)
        nav.navigate(Routes.Solution) { launchSingleTop = true }
    }

    NavHost(
        nav,
        startDestination = start,
        enterTransition = { fadeIn(androidx.compose.animation.core.tween(420)) + slideInHorizontally(androidx.compose.animation.core.tween(420)) { it / 12 } },
        exitTransition = { fadeOut(androidx.compose.animation.core.tween(300)) },
        popEnterTransition = { fadeIn(androidx.compose.animation.core.tween(420)) },
        popExitTransition = { fadeOut(androidx.compose.animation.core.tween(300)) + slideOutHorizontally(androidx.compose.animation.core.tween(420)) { it / 12 } },
    ) {
        composable(Routes.Intro) {
            OnboardingScreen {
                prefs.onboarded = true
                nav.navigate(Routes.Scan) { popUpTo(Routes.Intro) { inclusive = true } }
            }
        }
        composable(Routes.Scan) {
            ScanScreen(
                onSolve = ::solve,
                onGraph = ::graph,
                onEdit = { nav.navigate(Routes.editor(it)) },
                onKeyboard = { nav.navigate(Routes.editor()) },
                onHistory = { nav.navigate(Routes.History) },
                onGraphStudio = { nav.navigate(Routes.Graph) },
                onExplore = { nav.navigate(Routes.Explore) },
            )
        }
        composable(
            Routes.Editor,
            arguments = listOf(
                navArgument("text") { type = NavType.StringType; defaultValue = "" },
                navArgument("mode") { type = NavType.StringType; defaultValue = "solve" },
            ),
        ) { entry ->
            val mode = entry.arguments?.getString("mode") ?: "solve"
            EditorScreen(
                initial = entry.arguments?.getString("text") ?: "",
                onBack = { nav.popBackStack() },
                onSolve = { q ->
                    if (mode == "graph") { vm.addGraphEquation(q); nav.popBackStack() } else solve(q)
                },
                onGraph = { q ->
                    if (mode == "graph") { vm.addGraphEquation(q); nav.popBackStack() } else graph(q)
                },
                onHistory = { nav.navigate(Routes.History) },
            )
        }
        composable(Routes.Solution) {
            SolutionScreen(
                solution, solving,
                onBack = { nav.popBackStack() },
                onEdit = { nav.navigate(Routes.editor(it)) },
                onOpenGraph = ::graph,
            )
        }
        composable(Routes.Graph) {
            GraphScreen(
                equations,
                onBack = { nav.popBackStack() },
                onAdd = { nav.navigate(Routes.editor(mode = "graph")) },
                onRemove = vm::removeGraphEquation,
                onWallpaper = { nav.navigate(Routes.Wallpaper) },
                isSaved = savedGraphs.any { it.equations == equations },
                onSave = { vm.saveGraph(equations) },
                onOpenSaved = { nav.navigate(Routes.SavedGraphs) },
            )
        }
        composable(Routes.SavedGraphs) {
            com.mathlens.app.ui.screens.SavedGraphsScreen(
                savedGraphs,
                onBack = { nav.popBackStack() },
                onOpen = { g -> vm.setGraphEquations(g.equations); nav.popBackStack() },
                onDelete = vm::deleteSavedGraph,
            )
        }
        composable(Routes.Wallpaper) {
            WallpaperScreen(equations, onBack = { nav.popBackStack() })
        }
        composable(Routes.History) {
            HistoryScreen(
                history,
                onBack = { nav.popBackStack() },
                onOpen = { solve(it.query) },
                onDelete = vm::deleteHistory,
                onClear = vm::clearHistory,
                onToggleFavorite = vm::toggleFavorite,
            )
        }
        composable(Routes.Explore) {
            ExploreScreen(onBack = { nav.popBackStack() }, onPick = ::solve, onLicenses = { nav.navigate(Routes.Licenses) })
        }
        composable(Routes.Licenses) {
            com.mathlens.app.ui.screens.LicensesScreen(onBack = { nav.popBackStack() })
        }
    }
}
