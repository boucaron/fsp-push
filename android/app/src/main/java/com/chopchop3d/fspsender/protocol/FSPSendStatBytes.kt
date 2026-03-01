package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendStatBytes : FSPSendTextualCommand {

    companion object {
        private const val TAG = "FSPSendStatBytes"
    }

    // Member to hold the file size
    var fileTotalSize: Long = 0

    override fun sendCommand(): String {
        val command = "STAT_BYTES: $fileTotalSize\n"

        // Log the command being sent
        Log.d(TAG, "Sending command: $command")

        // Return the command string
        return command
    }
}