package com.chopchop3d.fspsender.protocol

object FSPSendVersion : FSPSendTextualCommand {

    override fun sendCommand(): String {
        return "VERSION: 0\n"
    }

}