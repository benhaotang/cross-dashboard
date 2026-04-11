package com.crossdashboard.app.widget

import android.content.Context
import androidx.datastore.core.CorruptionException
import androidx.datastore.core.DataStore
import androidx.datastore.core.Serializer
import androidx.datastore.dataStore
import androidx.glance.state.GlanceStateDefinition
import kotlinx.serialization.Serializable
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json
import java.io.File
import java.io.InputStream
import java.io.OutputStream

// ─── State data class ────────────────────────────────────────────────────────

@Serializable
data class DashboardWidgetState(
    val eventRows: List<String> = emptyList(),
    val taskRows: List<String> = emptyList(),
    val issuesCount: Int = 0,
    val lastSync: String = "",
)

// ─── DataStore serializer ────────────────────────────────────────────────────

internal object DashboardWidgetStateSerializer : Serializer<DashboardWidgetState> {
    override val defaultValue: DashboardWidgetState = DashboardWidgetState()

    override suspend fun readFrom(input: InputStream): DashboardWidgetState =
        try {
            Json.decodeFromString(input.readBytes().decodeToString())
        } catch (e: SerializationException) {
            throw CorruptionException("Cannot deserialize widget state", e)
        }

    override suspend fun writeTo(t: DashboardWidgetState, output: OutputStream) {
        output.write(Json.encodeToString(t).encodeToByteArray())
    }
}

// Single shared DataStore extension — must be declared at file/package level once
internal val Context.widgetDataStore: DataStore<DashboardWidgetState> by dataStore(
    fileName = "dashboard_widget_state.json",
    serializer = DashboardWidgetStateSerializer,
)

// ─── GlanceStateDefinition ───────────────────────────────────────────────────

/**
 * Glance state definition for [DashboardWidget].
 * Backed by a single DataStore<DashboardWidgetState> file shared across all widget instances.
 */
object DashboardWidgetStateDefinition : GlanceStateDefinition<DashboardWidgetState> {

    override suspend fun getDataStore(
        context: Context,
        fileKey: String,
    ): DataStore<DashboardWidgetState> = context.widgetDataStore

    override fun getLocation(context: Context, fileKey: String): File =
        File(context.filesDir, "datastore/dashboard_widget_state.json")
}

// ─── Helper for SyncWorker ───────────────────────────────────────────────────

/**
 * Writes fresh widget state from non-Glance code (e.g. [SyncWorker]).
 */
object WidgetStateStore {
    suspend fun update(context: Context, state: DashboardWidgetState) {
        context.widgetDataStore.updateData { state }
    }
}
