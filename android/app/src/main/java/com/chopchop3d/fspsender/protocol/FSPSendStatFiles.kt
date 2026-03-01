package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendStatFiles : FSPSendTextualCommand {

    companion object {
        private const val TAG = "FSPSendStatFiles"
    }

    // Member to hold the number of files
    var fileCount: Long = 0

    override fun sendCommand(): String {
        val command = "STAT_FILES: $fileCount\n"

        // Log the command being sent
        Log.d(TAG, "Sending command: $command")

        // Return the command string
        return command
    }
}