package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendStatBytes {

    companion object {
        private const val TAG = "FSPSendStatBytes"

        fun sendCommand(fileTotalSize: Long): String {
            val command = "STAT_BYTES: $fileTotalSize\n"

            Log.d(TAG, "Sending command: $command")

            return command
        }
    }
}