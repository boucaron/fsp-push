package com.chopchop3d.fspsender

import android.util.Log
import com.jcraft.jsch.ChannelExec
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
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

    /**
     * Test if target directory exists on a host using SSH exec
     * Returns true if present and is a directory, false otherwise
     */
    suspend fun checkTargetDirectory(
        targetDirectory: String,
        host: String,
        username: String,
        password: String,
        port: Int = 22
    ): Boolean = withContext(Dispatchers.IO) {

        var session: Session? = null
        var channel: ChannelExec? = null

        try {
            val jsch = JSch()
            session = jsch.getSession(username, host, port)
            session.setPassword(password)

            val config = Properties().apply {
                put("StrictHostKeyChecking", "no")
            }

            session.setConfig(config)
            session.timeout = 5000
            session.connect()

            channel = session.openChannel("exec") as ChannelExec

            // Safe quoting of directory
            val command = "test -d \"${targetDirectory.replace("\"", "\\\"")}\""
            channel.setCommand(command)

            channel.inputStream = null
            val errorStream = channel.errStream

            channel.connect()

            // Wait until command finishes
            while (!channel.isClosed) {
                delay(50)
            }

            val exitStatus = channel.exitStatus

            errorStream?.bufferedReader()?.use { reader ->
                val errorOutput = reader.readText()
                if (errorOutput.isNotBlank()) {
                    Log.e("SshHelper", "SSH stderr: $errorOutput")
                }
            }

            exitStatus == 0  // 0 = directory exists

        } catch (e: Exception) {
            Log.e("SshHelper", "Directory check failed", e)
            false
        } finally {
            try {
                channel?.disconnect()
                session?.disconnect()
            } catch (_: Exception) {}
        }
    }
}