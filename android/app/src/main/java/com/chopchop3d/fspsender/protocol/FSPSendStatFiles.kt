package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendStatFiles {

    companion object {
        private const val TAG = "FSPSendStatFiles"

        fun sendCommand(fileCount: Long): String {
            val command = "STAT_FILES: $fileCount\n"

            Log.d(TAG, "Sending command: $command")

            return command
        }
    }
}