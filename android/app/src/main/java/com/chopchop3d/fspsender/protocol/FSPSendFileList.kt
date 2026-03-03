package com.chopchop3d.fspsender.protocol

import android.util.Log

public class FSPSendFileList  : FSPSendTextualCommand {

    companion object {
        private const val TAG = "FSPSendFileList"
    }


    override fun sendCommand(): String {
        val command = "FILE_LIST\n"

        // Log the command being sent
        Log.d(TAG, "Sending command: $command")

        // Return the command string
        return command
    }
}