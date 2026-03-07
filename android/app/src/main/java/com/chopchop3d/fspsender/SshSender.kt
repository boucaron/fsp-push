package com.chopchop3d.fspsender

import android.util.Log
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
        const val TEST_TIMEOUT = 60 * 1000 // 60 s
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

        // config["StrictHostKeyChecking"] = "no"

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

            val config = createConfig()
            session!!.setConfig(config)
            session!!.timeout = TEST_TIMEOUT

            session!!.connect()

            channel = session!!.openChannel(CHANNEL_SHELL) as ChannelShell
            channel!!.connect()

            outputStream = BufferedOutputStream(
                channel!!.outputStream,
                OUTPUT_BUFFER_SIZE
            )

            true

        } catch (e: Exception) {
            Log.e(LOG_TAG, "SSH connect failed", e)
            false
        }
    }

    /**
     * Disconnect everything
     */
    suspend fun disconnect() = withContext(Dispatchers.IO) {
        try {
            outputStream?.flush()
        } catch (_: Exception) {}

        try {
            outputStream?.close()
        } catch (_: Exception) {}

        try {
            channel?.disconnect()
        } catch (_: Exception) {}

        try {
            session?.disconnect()
        } catch (_: Exception) {}

        outputStream = null
        channel = null
        session = null
    }

    /**
     * Send textual data
     */
    suspend fun sendText(text: String) = withContext(Dispatchers.IO) {
        try {
            val bytes = text.toByteArray(Charsets.UTF_8)

            outputStream?.write(bytes)
            outputStream?.write('\n'.code) // optional newline
            outputStream?.flush()

        } catch (e: Exception) {
            Log.e(LOG_TAG, "Failed to send text", e)
        }
    }

    /**
     * Send binary data
     */
    suspend fun sendBinary(data: ByteArray, flush: Boolean = false) =
        withContext(Dispatchers.IO) {

            try {
                outputStream?.write(data)

                if (flush) {
                    outputStream?.flush()
                }

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
}