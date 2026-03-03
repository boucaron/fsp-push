package com.chopchop3d.fspsender.protocol

import com.chopchop3d.fspsender.dfs.FSPWalkerState

class FSPSendFiles {


    fun sendCommand(index: Long, batchCount: Long, walkerState: FSPWalkerState ) {
        var i = index;
        var lastIndex = index + batchCount;
        while( i < lastIndex ) {
            var current = walkerState.entries[i.toInt()];

            // Open File
            // Send data on protocol & Compute SHA256 together

            // TODO

            i++
        }

        // Flush Protocol
    }
}