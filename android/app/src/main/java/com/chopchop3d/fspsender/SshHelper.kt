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

    companion object {
        const val TEST_TIMEOUT = 5000 // 5 s
        const val TEST_DELAY = 50L // 50 ms
        const val SSH_DEFAULT_PORT = 22

        const val CHANNEL_EXEC = "exec"
        const val LOG_TAG = "SshHelper"
    }


    fun createTestConfig(): Properties {
        val config = Properties()

        // Disable compression
        config["compression.s2c"] = "none"
        config["compression.c2s"] = "none"

        return config
    }


    /**
     * Tests SSH connectivity to a host.
     * Returns true if connection succeeds, false otherwise.
     */
    suspend fun testConnection(host: String, username: String, password: String, port: Int = SSH_DEFAULT_PORT): Boolean =
        withContext(Dispatchers.IO) {
            try {
                val jsch = JSch()
                val session: Session = jsch.getSession(username, host, port)
                session.setPassword(password)

                val config = createTestConfig()
                session.setConfig(config)
                session.timeout = TEST_TIMEOUT
                session.connect()
                session.disconnect()
                true
            } catch (e: Exception) {
                Log.e(LOG_TAG, "SSH connection failed", e)
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
        port: Int = SSH_DEFAULT_PORT
    ): Boolean = withContext(Dispatchers.IO) {

        var session: Session? = null
        var channel: ChannelExec? = null

        try {
            val jsch = JSch()
            session = jsch.getSession(username, host, port)
            session.setPassword(password)

            val config = createTestConfig()
            session.setConfig(config)
            session.timeout = TEST_TIMEOUT
            session.connect()

            channel = session.openChannel(CHANNEL_EXEC) as ChannelExec

            // Safe quoting of directory
            val command = "test -d \"${targetDirectory.replace("\"", "\\\"")}\""
            channel.setCommand(command)

            channel.inputStream = null
            val errorStream = channel.errStream

            channel.connect()

            // Wait until command finishes
            while (!channel.isClosed) {
                delay(TEST_DELAY)
            }

            val exitStatus = channel.exitStatus

            errorStream?.bufferedReader()?.use { reader ->
                val errorOutput = reader.readText()
                if (errorOutput.isNotBlank()) {
                    Log.e(LOG_TAG, "SSH stderr: $errorOutput")
                }
            }

            exitStatus == 0  // 0 = directory exists

        } catch (e: Exception) {
            Log.e(LOG_TAG, "Directory check failed", e)
            false
        } finally {
            try {
                channel?.disconnect()
                session?.disconnect()
            } catch (_: Exception) {}
        }
    }


    private suspend fun executeCommand(
        session: Session,
        command: String
    ): Int {

        val channel = session.openChannel(CHANNEL_EXEC) as ChannelExec
        channel.setCommand(command)

        val errorStream = channel.errStream
        channel.connect()

        while (!channel.isClosed) {
            delay(TEST_DELAY)
        }

        val exitStatus = channel.exitStatus

        errorStream?.bufferedReader()?.use { reader ->
            val errorOutput = reader.readText()
            if (errorOutput.isNotBlank()) {
                Log.e(LOG_TAG, "SSH stderr: $errorOutput")
            }
        }

        channel.disconnect()

        return exitStatus
    }

    suspend fun checkFSPReceiverExists(
        host: String,
        username: String,
        password: String,
        port: Int = SSH_DEFAULT_PORT
    ): Boolean = withContext(Dispatchers.IO) {

        var session: Session? = null

        try {
            val jsch = JSch()
            session = jsch.getSession(username, host, port)
            session.setPassword(password)

            val config = createTestConfig()

            session.setConfig(config)
            session.timeout = TEST_TIMEOUT
            session.connect()

            // ---------- First command: which ----------
            val whichExit = executeCommand(session, "which fsp-recv")
            if (whichExit != 0) {
                Log.e(LOG_TAG, "fsp-recv not found in PATH")
            }
            val whichExit2 = executeCommand(session, "which fsp-recv.exe")
            if (whichExit2 != 0) {
                Log.e(LOG_TAG, "fsp-recv.exe not found in PATH")
            }

            // ---------- Second command: version ----------
            val versionExit = executeCommand(session, "fsp-recv --version")
            if (versionExit != 0) {
                Log.e(LOG_TAG, "fsp-recv exists but --version failed")
                return@withContext false
            }

            true

        } catch (e: Exception) {
            Log.e(LOG_TAG, "FSP check failed", e)
            false
        } finally {
            try {
                session?.disconnect()
            } catch (_: Exception) {}
        }
    }
}