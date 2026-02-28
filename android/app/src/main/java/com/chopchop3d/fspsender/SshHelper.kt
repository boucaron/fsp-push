package com.chopchop3d.fspsender

import android.util.Log
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.Properties

class SshHelper {
    /**
     * Tests SSH connectivity to a host.
     * Returns true if connection succeeds, false otherwise.
     */
    suspend fun testConnection(host: String, username: String, password: String, port: Int = 22): Boolean =
        withContext(Dispatchers.IO) {
            try {
                val jsch = JSch()
                val session: Session = jsch.getSession(username, host, port)
                session.setPassword(password)

                val config = Properties()
                config["StrictHostKeyChecking"] = "no"

                // Disable compression
                config["compression.s2c"] = "none"
                config["compression.c2s"] = "none"

                session.setConfig(config)
                session.timeout = 5000
                session.connect()
                session.disconnect()
                true
            } catch (e: Exception) {
                Log.e("SshHelper", "SSH connection failed", e)
                false
            }
        }
}