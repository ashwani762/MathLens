// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

data class HistoryItem(val query: String, val inputLatex: String, val answerLatex: String, val kind: String, val time: Long, val favorite: Boolean = false)

/** Small persistent history (most recent first) stored as JSON in SharedPreferences. */
class HistoryStore(context: Context) {
    private val prefs = context.getSharedPreferences("history", Context.MODE_PRIVATE)

    fun load(): List<HistoryItem> {
        val raw = prefs.getString("items", null) ?: return emptyList()
        return runCatching {
            val a = JSONArray(raw)
            (0 until a.length()).map {
                val o = a.getJSONObject(it)
                HistoryItem(o.getString("q"), o.optString("in"), o.optString("ans"), o.optString("kind"), o.optLong("t"), o.optBoolean("fav"))
            }
        }.getOrDefault(emptyList())
    }

    fun save(items: List<HistoryItem>) {
        val a = JSONArray()
        // favourites are always kept; the rest is capped at the 100 most recent
        val keep = items.filter { it.favorite } + items.filter { !it.favorite }.take(100)
        items.filter { it in keep }.forEach {
            a.put(JSONObject().put("q", it.query).put("in", it.inputLatex).put("ans", it.answerLatex).put("kind", it.kind).put("t", it.time).put("fav", it.favorite))
        }
        prefs.edit().putString("items", a.toString()).apply()
    }
}
