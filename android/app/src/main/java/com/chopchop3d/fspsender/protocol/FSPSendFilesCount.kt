package com.chopchop3d.fspsender.protocol;

import android.util.Log

public class FSPSendFilesCount : FSPSendTextualCommand {

    companion object {
        private const val TAG = "FSPSendFilesCount"
    }

    var files: Long = 0L

    override fun sendCommand(): String {
        val command = "FILES: files\n"

        // Log the command being sent
        Log.d(TAG, "Sending command: $command")

        // Return the command string
        return command
    }
}

