package com.chopchop3d.fspsender

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map

// Extension property for DataStore
val Context.fspDataStore by preferencesDataStore(name = "fsp_settings")

data class SSHConfigSnapshot(
    val hostname: String?,
    val username: String?,
    val targetDirectory: String?,
    var simulationThroughput: String?
)

object FSPSettings {

    // Keys
    private val HOSTNAME_KEY = stringPreferencesKey("hostname")
    private val USERNAME_KEY = stringPreferencesKey("username")
    private val TARGET_DIR_KEY = stringPreferencesKey("target_directory")
    private val SIMULATION_THROUGHPUT = stringPreferencesKey("simulation_throughput")

    // Save config (example)
    suspend fun saveConfig(
        context: Context,
        hostname: String,
        username: String,
        targetDirectory: String,
        simulationThroughput: String
    ) {
        context.fspDataStore.edit { prefs ->
            prefs[HOSTNAME_KEY] = hostname
            prefs[USERNAME_KEY] = username
            prefs[TARGET_DIR_KEY] = targetDirectory
            prefs[SIMULATION_THROUGHPUT] = simulationThroughput
        }
    }

    // Read config as snapshot
    suspend fun getConfigSnapshot(context: Context): SSHConfigSnapshot {
        val prefs: Preferences = context.fspDataStore.data.first()
        return SSHConfigSnapshot(
            hostname = prefs[HOSTNAME_KEY],
            username = prefs[USERNAME_KEY],
            targetDirectory = prefs[TARGET_DIR_KEY],
            simulationThroughput = prefs[SIMULATION_THROUGHPUT]
        )
    }

    // Read config as Flows
    fun getHostname(context: Context): Flow<String?> = context.fspDataStore.data
        .map { it[HOSTNAME_KEY] }

    fun getUsername(context: Context): Flow<String?> = context.fspDataStore.data
        .map { it[USERNAME_KEY] }

    fun getTargetDirectory(context: Context): Flow<String?> = context.fspDataStore.data
        .map { it[TARGET_DIR_KEY] }

    fun getSimulationThroughput(context: Context): Flow<String?> = context.fspDataStore.data
        .map { it[SIMULATION_THROUGHPUT] }
}