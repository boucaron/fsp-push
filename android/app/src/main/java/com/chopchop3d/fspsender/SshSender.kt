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
    suspend fun connect(host: String, username: String, password: String, port: Int = SSH_DEFAULT_PORT): Boolean =
        withContext(Dispatchers.IO) {
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

    /** Start a remote process (like tee or fsp-recv) and keep stdin open */
    suspend fun startProcess(command: String): Boolean = withContext(Dispatchers.IO) {
        if (session == null || !session!!.isConnected) {
            Log.e(LOG_TAG, "SSH session not connected")
            return@withContext false
        }

        try {
            execChannel = session!!.openChannel("exec") as ChannelExec
            execChannel!!.setCommand(command)
            execChannel!!.inputStream = null // stdin will be via execChannel.outputStream
            stdin = execChannel!!.outputStream
            val stderr: InputStream = execChannel!!.errStream

            // Launch coroutine to read stderr continuously
            scope.launch {
                val buffer = ByteArray(4096)
                try {
                    while (true) {
                        val read = stderr.read(buffer)
                        if (read == -1) break
                        if (read > 0) {
                            Log.e(LOG_TAG, "Server stderr: ${String(buffer, 0, read)}")
                        }
                    }
                    Log.d(LOG_TAG, "Remote process exited with status: ${execChannel!!.exitStatus}")
                } catch (e: Exception) {
                    Log.e(LOG_TAG, "Error reading stderr", e)
                }
            }

            execChannel!!.connect()
            Log.d(LOG_TAG, "Remote process started: $command")
            true
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to start remote process", e)
            false
        }
    }

    /** Send text to the remote process stdin */
    suspend fun sendText(text: String) = withContext(Dispatchers.IO) {
        try {
            stdin?.write((text + "\n").toByteArray(Charsets.UTF_8))
            stdin?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send text", e)
        }
    }

    /** Send binary to the remote process stdin */
    suspend fun sendBinary(data: ByteArray, flush: Boolean = false) = withContext(Dispatchers.IO) {
        try {
            stdin?.write(data)
            if (flush) stdin?.flush()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send binary", e)
        }
    }

    /** Close stdin if remote process expects EOF */
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