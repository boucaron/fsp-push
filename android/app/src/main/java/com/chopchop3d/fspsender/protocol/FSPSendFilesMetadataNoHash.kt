package com.chopchop3d.fspsender.protocol

import com.chopchop3d.fspsender.dfs.FSPWalkerState

class FSPSendFilesMetadataNoHash {

    fun sendCommand(index: Long, batchCount: Long, walkerState: FSPWalkerState ) {
        var i = index;
        var lastIndex = index + batchCount;
        while( i < lastIndex ) {
            var current = walkerState.entries[i.toInt()];

            // Send protocol
            // TODO

            i++
        }

        // Flush Protocol
    }
}