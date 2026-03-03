package com.chopchop3d.fspsender.protocol

import android.util.Log

public class FSPSendDirectory  : FSPSendTextualCommand {

    companion object {
        private const val TAG = "FSPSendDirectory"
    }


    var directory: String = "" // Relative Path !

    override fun sendCommand(): String {
        val command = "DIRECTORY: $directory\n"

        // Log the command being sent
        Log.d(TAG, "Sending command: $command")

        // Return the command string
        return command
    }
}
