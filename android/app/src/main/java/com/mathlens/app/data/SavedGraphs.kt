// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

package com.mathlens.app.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/** A graph the user kept: its equations (one per curve) and when it was saved. */
data class SavedGraph(val equations: List<String>, val time: Long)

/** Saved graphs (most recent first) as JSON in SharedPreferences. */
class SavedGraphStore(context: Context) {
    private val prefs = context.getSharedPreferences("saved_graphs", Context.MODE_PRIVATE)

    fun load(): List<SavedGraph> = runCatching {
        val a = JSONArray(prefs.getString("items", "[]"))
        (0 until a.length()).map { i ->
            val o = a.getJSONObject(i)
            val eqs = o.getJSONArray("eqs")
            SavedGraph((0 until eqs.length()).map { eqs.getString(it) }, o.optLong("t"))
        }
    }.getOrDefault(emptyList())

    fun save(items: List<SavedGraph>) {
        val a = JSONArray()
        items.take(200).forEach { g -> a.put(JSONObject().put("eqs", JSONArray(g.equations)).put("t", g.time)) }
        prefs.edit().putString("items", a.toString()).apply()
    }
}
