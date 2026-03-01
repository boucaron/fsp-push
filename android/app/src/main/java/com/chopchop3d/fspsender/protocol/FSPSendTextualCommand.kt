package com.chopchop3d.fspsender.protocol;

interface FSPSendTextualCommand {

    /**
     * Sends a command and returns the response as a String.
     */
    fun sendCommand(): String

}
