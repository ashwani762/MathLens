// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.mathlens.app.data.HistoryItem
import com.mathlens.app.data.HistoryStore
import com.mathlens.app.engine.MathEngine
import com.mathlens.app.engine.Solution
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class AppViewModel(app: Application) : AndroidViewModel(app) {
    private val store = HistoryStore(app)
    private val graphStore = com.mathlens.app.data.SavedGraphStore(app)

    private val _savedGraphs = MutableStateFlow(graphStore.load())
    val savedGraphs: StateFlow<List<com.mathlens.app.data.SavedGraph>> = _savedGraphs.asStateFlow()

    /** Saves the current graph; saving the same equations again just moves it to the front. */
    fun saveGraph(equations: List<String>) {
        if (equations.isEmpty()) return
        val list = listOf(com.mathlens.app.data.SavedGraph(equations, System.currentTimeMillis())) + _savedGraphs.value.filter { it.equations != equations }
        _savedGraphs.value = list
        viewModelScope.launch(Dispatchers.IO) { graphStore.save(list) }
    }

    fun deleteSavedGraph(g: com.mathlens.app.data.SavedGraph) {
        val list = _savedGraphs.value - g
        _savedGraphs.value = list
        viewModelScope.launch(Dispatchers.IO) { graphStore.save(list) }
    }

    private val _history = MutableStateFlow(store.load())
    val history: StateFlow<List<HistoryItem>> = _history.asStateFlow()

    private val _solution = MutableStateFlow<Solution?>(null)
    val solution: StateFlow<Solution?> = _solution.asStateFlow()

    private val _solving = MutableStateFlow(false)
    val solving: StateFlow<Boolean> = _solving.asStateFlow()

    /** Text handed to the editor (from OCR, history, examples). */
    var editorText: String = ""

    /** Equations shown on the graph screen. */
    private val _graphEquations = MutableStateFlow(listOf("y = sin(x)"))
    val graphEquations: StateFlow<List<String>> = _graphEquations.asStateFlow()

    fun solve(query: String) {
        val q = query.trim()
        if (q.isEmpty()) return
        _solving.value = true
        _solution.value = null
        viewModelScope.launch {
            val s = withContext(Dispatchers.Default) { MathEngine.solve(q) }
            _solution.value = s
            _solving.value = false
            if (s.ok) {
                val wasFavorite = _history.value.any { it.query == q && it.favorite }
                val item = HistoryItem(q, s.input, s.answer, s.kind, System.currentTimeMillis(), wasFavorite)
                val list = listOf(item) + _history.value.filter { it.query != q }
                _history.value = list
                withContext(Dispatchers.IO) { store.save(list) }
            }
        }
    }

    fun deleteHistory(item: HistoryItem) {
        val list = _history.value - item
        _history.value = list
        viewModelScope.launch(Dispatchers.IO) { store.save(list) }
    }

    /** Clears everything except favourites. */
    fun clearHistory() {
        val list = _history.value.filter { it.favorite }
        _history.value = list
        viewModelScope.launch(Dispatchers.IO) { store.save(list) }
    }

    fun toggleFavorite(item: HistoryItem) {
        val list = _history.value.map { if (it.time == item.time && it.query == item.query) it.copy(favorite = !it.favorite) else it }
        _history.value = list
        viewModelScope.launch(Dispatchers.IO) { store.save(list) }
    }

    fun setGraphEquations(eqs: List<String>) { _graphEquations.value = eqs.filter { it.isNotBlank() }.ifEmpty { listOf("y = x") } }
    fun addGraphEquation(eq: String) { if (eq.isNotBlank()) _graphEquations.value = _graphEquations.value + eq }
    fun removeGraphEquation(index: Int) {
        _graphEquations.value = _graphEquations.value.filterIndexed { i, _ -> i != index }
    }
}
