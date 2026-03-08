package com.chopchop3d.fspsender

import android.util.Log
import com.jcraft.jsch.ChannelExec
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import kotlinx.coroutines.*
import java.io.InputStream
import java.io.OutputStream
import java.util.Properties

class SshSender {

    companion object {
        const val TIMEOUT = 60 * 1000
        const val SSH_DEFAULT_PORT = 22
        const val LOG_TAG = "SshSender"
    }

    private var session: Session? = null
    private var execChannel: ChannelExec? = null
    private var stdin: OutputStream? = null

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private fun createConfig(): Properties {
        return Properties().apply {
            this["compression.s2c"] = "none"
            this["compression.c2s"] = "none"
            this["StrictHostKeyChecking"] = "no"
        }
    }

    /** Connect to SSH server */
    suspend fun connect(
        host: String,
        username: String,
        password: String,
        port: Int = SSH_DEFAULT_PORT
    ): Boolean = withContext(Dispatchers.IO) {
        try {
            val jsch = JSch()

            session = jsch.getSession(username, host, port).apply {
                setPassword(password)
                setConfig(createConfig())
                timeout = TIMEOUT
                connect()
            }

            Log.d(LOG_TAG, "SSH connected")
            true

        } catch (e: Exception) {
            Log.e(LOG_TAG, "SSH connect failed", e)
            false
        }
    }

    /** Start remote process (fsp-recv, tee, etc.) */
    suspend fun startProcess(
        command: String,
        stdErrCallback: ((String) -> Unit)? = null  // optional callback
    ): Boolean = withContext(Dispatchers.IO) {

        val s = session
        if (s == null || !s.isConnected) {
            Log.e(LOG_TAG, "SSH session not connected")
            return@withContext false
        }

        try {

            execChannel = s.openChannel("exec") as ChannelExec
            execChannel!!.setCommand(command)

            execChannel!!.inputStream = null

            val stdout: InputStream = execChannel!!.inputStream
            val stderr: InputStream = execChannel!!.errStream

            execChannel!!.connect()

            stdin = execChannel!!.outputStream

            // Start readers
            startReader(stdout, "stdout", null)               // stdout ignored
            startReader(stderr, "stderr", stdErrCallback)    // stderr uses callback

            delay(50) // give remote process time to attach stdin

            Log.d(LOG_TAG, "Remote process started: $command")

            true

        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to start remote process", e)
            false
        }
    }

    /** Continuously drain streams (important to avoid SSH deadlocks) */
    private fun startReader(
        stream: InputStream,
        label: String,
        stdErrCallback: ((String) -> Unit)? = null // optional
    ) {
        scope.launch {
            val buffer = ByteArray(4096)
            try {
                while (true) {
                    val read = stream.read(buffer)
                    if (read == -1) break

                    if (read > 0) {
                        val output = "$label: ${String(buffer, 0, read)}"
                        Log.e(LOG_TAG, output)
                        stdErrCallback?.invoke(output) // send output to callback
                    }
                }

                Log.e(LOG_TAG, "Remote process exited: ${execChannel?.exitStatus}")

            } catch (e: Exception) {
                Log.e(LOG_TAG, "Stream reader error", e)
            }
        }
    }

    /** Send text command */
    suspend fun sendText(text: String) = withContext(Dispatchers.IO) {

        val ch = execChannel
        if (ch == null || ch.isClosed) {
            Log.e(LOG_TAG, "Cannot send, process already closed")
            return@withContext
        }

        try {

            val bytes = text.toByteArray(Charsets.UTF_8)

            stdin?.write(bytes)
            stdin?.flush()

            Log.d(LOG_TAG, "Sent: $text")

        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send text", e)
        }
    }

    /** Send binary data */
    suspend fun sendBinary(data: ByteArray, flush: Boolean = false) = withContext(Dispatchers.IO) {

        val ch = execChannel
        if (ch == null || ch.isClosed) {
            Log.e(LOG_TAG, "Cannot send binary, process closed")
            return@withContext
        }

        try {

            stdin?.write(data)

            if (flush) {
                stdin?.flush()
            }

        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send binary", e)
        }
    }

    suspend fun flush() = withContext(Dispatchers.IO) {

        val ch = execChannel
        if (ch == null || ch.isClosed) {
            Log.e(LOG_TAG, "Cannot send binary, process closed")
            return@withContext
        }

        try {
            stdin?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send binary", e)
        }
    }

    /** Close stdin (EOF) */
    suspend fun closeStdin() = withContext(Dispatchers.IO) {
        try {
            stdin?.close()
        } catch (_: Exception) {}
    }

    /** Disconnect everything */
    suspend fun disconnect() = withContext(Dispatchers.IO) {

        try { stdin?.flush() } catch (_: Exception) {}
        try { stdin?.close() } catch (_: Exception) {}
        try { execChannel?.disconnect() } catch (_: Exception) {}
        try { session?.disconnect() } catch (_: Exception) {}

        scope.cancel()

        stdin = null
        execChannel = null
        session = null

        Log.d(LOG_TAG, "SSH disconnected")
    }
}