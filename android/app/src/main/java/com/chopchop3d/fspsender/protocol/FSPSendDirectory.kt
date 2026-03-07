package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendDirectory {

    companion object {
        private const val TAG = "FSPSendDirectory"

        fun sendCommand(directory: String): String {
            val command = "DIRECTORY: $directory\n"

            Log.d(TAG, "Sending command: $command")

            return command
        }
    }
}