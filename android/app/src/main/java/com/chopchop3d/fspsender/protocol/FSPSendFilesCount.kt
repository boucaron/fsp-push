package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendFilesCount {

    companion object {
        private const val TAG = "FSPSendFilesCount"

        fun sendCommand(files: Long): String {
            val command = "FILES: $files\n"

            Log.d(TAG, "Sending command: $command")

            return command
        }
    }
}