package com.chopchop3d.fspsender.protocol

class FSPSendVersion : FSPSendTextualCommand {
    override fun sendCommand(): String {
        val command = "VERSION: 0\n";
       return command
    }
}