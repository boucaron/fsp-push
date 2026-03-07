package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendFileList {

    companion object {
        private const val TAG = "FSPSendFileList"

        fun sendCommand(): String {
            val command = "FILE_LIST\n"

            Log.d(TAG, "Sending command: $command")

            return command
        }
    }
}