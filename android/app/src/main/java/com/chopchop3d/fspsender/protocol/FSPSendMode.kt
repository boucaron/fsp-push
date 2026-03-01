package com.chopchop3d.fspsender.protocol

import android.util.Log

class FSPSendMode : FSPSendTextualCommand {

    companion object {
        const val FSP_APPEND = "append"
        const val FSP_FORCE = "force"
        const val FSP_SAFE = "safe"

        private val VALID_MODES = setOf(FSP_APPEND, FSP_FORCE, FSP_SAFE)
        private const val TAG = "FSPSendMode"
    }

    // Backing field for mode
    private var _mode: String = FSP_APPEND

    var mode: String
        get() = _mode
        set(value) {
            if (VALID_MODES.contains(value)) {
                _mode = value
            } else {
                Log.w(TAG, "Invalid mode '$value'. Keeping previous mode '$_mode'.")
            }
        }

    override fun sendCommand(): String {
        val command = "MODE: $_mode\n"
        // Simulate sending the command
        Log.d(TAG, "Sending command: $command")
        return command
    }
}