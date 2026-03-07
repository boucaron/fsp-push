package com.chopchop3d.fspsender

import android.util.Log
import com.jcraft.jsch.ChannelExec
import com.jcraft.jsch.ChannelShell
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.BufferedOutputStream
import java.io.OutputStream
import java.util.Properties

class SshSender {

    companion object {
        const val TIMEOUT = 60 * 1000 // 60 s
        const val SSH_DEFAULT_PORT = 22

        const val CHANNEL_SHELL = "shell"
        const val LOG_TAG = "SshSender"

        const val OUTPUT_BUFFER_SIZE = 16 * 1024 * 1024 // 16 MB
    }

    private var session: Session? = null
    private var channel: ChannelShell? = null
    private var outputStream: OutputStream? = null

    /**
     * Create SSH configuration
     */
    private fun createConfig(): Properties {
        val config = Properties()
        config["compression.s2c"] = "none"
        config["compression.c2s"] = "none"
        config["StrictHostKeyChecking"] = "no"
        return config
    }

    /**
     * Connect and initialize persistent SSH channel
     */
    suspend fun connect(
        host: String,
        username: String,
        password: String,
        port: Int = SSH_DEFAULT_PORT
    ): Boolean = withContext(Dispatchers.IO) {
        try {
            val jsch = JSch()
            session = jsch.getSession(username, host, port)
            session!!.setPassword(password)
            session!!.setConfig(createConfig())
            session!!.timeout = TIMEOUT
            session!!.connect()

            channel = session!!.openChannel(CHANNEL_SHELL) as ChannelShell
            channel!!.connect()

            outputStream = BufferedOutputStream(channel!!.outputStream, OUTPUT_BUFFER_SIZE)

            Log.d(LOG_TAG, "SSH connected and shell channel initialized")
            true
        } catch (e: Exception) {
            Log.e(LOG_TAG, "SSH connect failed", e)
            false
        }
    }

    /**
     * Start fsp-recv on the server with the given target directory.
     * Keeps the channel open for sending commands/data.
     */
    suspend fun startFspRecv(targetDirectory: String): Boolean = withContext(Dispatchers.IO) {
        if (session == null || !session!!.isConnected) {
            Log.e(LOG_TAG, "SSH session is not connected")
            return@withContext false
        }

        try {
            // Open exec channel to start fsp-recv
            val execChannel = session!!.openChannel("exec") as ChannelExec
            execChannel.setCommand("fsp-recv \"$targetDirectory\"")
            execChannel.inputStream = null
            // execChannel.errStream = System.err // or capture errors

            execChannel.connect()
            Log.d(LOG_TAG, "fsp-recv started on server: $targetDirectory")

            // Keep the shell channel open to send further commands or binary data
            true
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to start fsp-recv", e)
            false
        }
    }

    /**
     * Send textual data to the server (stdin)
     */
    suspend fun sendText(text: String) = withContext(Dispatchers.IO) {
        try {
            val bytes = (text + "\n").toByteArray(Charsets.UTF_8)
            outputStream?.write(bytes)
            outputStream?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send text", e)
        }
    }

    /**
     * Send binary data to the server
     */
    suspend fun sendBinary(data: ByteArray, flush: Boolean = false) =
        withContext(Dispatchers.IO) {
            try {
                outputStream?.write(data)
                if (flush) outputStream?.flush()
            } catch (e: Exception) {
                Log.e(LOG_TAG, "Failed to send binary", e)
            }
        }

    /**
     * Explicit flush when sending many chunks
     */
    suspend fun flush() = withContext(Dispatchers.IO) {
        try {
            outputStream?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Flush failed", e)
        }
    }

    /**
     * Disconnect everything
     */
    suspend fun disconnect() = withContext(Dispatchers.IO) {
        try { outputStream?.flush() } catch (_: Exception) {}
        try { outputStream?.close() } catch (_: Exception) {}
        try { channel?.disconnect() } catch (_: Exception) {}
        try { session?.disconnect() } catch (_: Exception) {}
        outputStream = null
        channel = null
        session = null
        Log.d(LOG_TAG, "SSH disconnected")
    }
}