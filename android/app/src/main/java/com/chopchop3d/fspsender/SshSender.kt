package com.chopchop3d.fspsender

import android.util.Log
import com.jcraft.jsch.ChannelShell
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import kotlinx.coroutines.*
import java.io.BufferedOutputStream
import java.io.InputStream
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
    private var inputStream: InputStream? = null
    private var errorJob: Job? = null
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private fun createConfig(): Properties {
        val config = Properties()
        config["compression.s2c"] = "none"
        config["compression.c2s"] = "none"
        config["StrictHostKeyChecking"] = "no"
        return config
    }

    /**
     * Connect to SSH and open a persistent shell channel
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

            // Open persistent shell channel
            channel = session!!.openChannel(CHANNEL_SHELL) as ChannelShell
            channel!!.connect()

            outputStream = BufferedOutputStream(channel!!.outputStream, OUTPUT_BUFFER_SIZE)
            inputStream = channel!!.inputStream

            // Launch coroutine to continuously read server output (stdout + stderr)
            errorJob = scope.launch {
                val buffer = ByteArray(4096)
                try {
                    while (true) {
                        val read = inputStream!!.read(buffer)
                        if (read > 0) {
                            val line = String(buffer, 0, read)
                            Log.e(LOG_TAG, "Server output: $line")
                        } else if (read == -1) break
                    }
                } catch (_: Exception) {}
            }

            Log.d(LOG_TAG, "SSH connected and shell channel initialized")
            true
        } catch (e: Exception) {
            Log.e(LOG_TAG, "SSH connect failed", e)
            false
        }
    }

    /**
     * Start fsp-recv on the server with the target directory
     * Sends the command via shell channel so sendText/sendBinary can write to stdin
     */
    suspend fun startFspRecv(targetDirectory: String): Boolean = withContext(Dispatchers.IO) {
        if (channel == null || !channel!!.isConnected) {
            Log.e(LOG_TAG, "SSH shell channel is not connected")
            return@withContext false
        }

        return@withContext try {
            val command = "fsp-recv \"$targetDirectory\" 2>&1\n"
            outputStream?.write(command.toByteArray(Charsets.UTF_8))
            outputStream?.flush()
            Log.d(LOG_TAG, "fsp-recv started on server: $targetDirectory")
            true
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to start fsp-recv", e)
            false
        }
    }

    /**
     * Send textual data to the server process
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
     * Send binary data to the server process
     */
    suspend fun sendBinary(data: ByteArray, flush: Boolean = false) = withContext(Dispatchers.IO) {
        try {
            outputStream?.write(data)
            if (flush) outputStream?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send binary", e)
        }
    }

    /**
     * Flush output stream explicitly
     */
    suspend fun flush() = withContext(Dispatchers.IO) {
        try {
            outputStream?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Flush failed", e)
        }
    }

    /**
     * Disconnect SSH and cancel reading job
     */
    suspend fun disconnect() = withContext(Dispatchers.IO) {
        try { outputStream?.flush() } catch (_: Exception) {}
        try { outputStream?.close() } catch (_: Exception) {}
        try { channel?.disconnect() } catch (_: Exception) {}
        try { session?.disconnect() } catch (_: Exception) {}
        errorJob?.cancelAndJoin()
        outputStream = null
        inputStream = null
        channel = null
        session = null
        errorJob = null
        Log.d(LOG_TAG, "SSH disconnected")
    }
}