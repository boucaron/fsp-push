package com.chopchop3d.fspsender.dfs

import com.chopchop3d.fspsender.protocol.FSPProtocol
import java.time.Instant

/**
 * Walker state for batching and dry-run stats.
 *
 * Defaults initialized according to the original C code where possible.
 */
data class FSPWalkerState(
    // Current directory paths
    var fullPath: String = "",
    var relPath: String = "",

    // Current file entries (directories are handled by the caller)
    var entries: List<FSPFileEntry> = emptyList(),

    // Current batch tracking
    var currentFiles: Long = 0L,
    var currentBytes: Long = 0L,
    var flushNeeded: Boolean = false,

    // Dry-run stats (mandatory)
    // var dryRun: FSPDryRunStats, // TODO: FIXME

    // Depth tracking
    var currentDepth: Int = 0,
    var maxDepth: Int = FSP_MAX_WALK_DEPTH, // Protocol limit

    // Batching thresholds
    var maxFiles: Long = FSP_MAX_FILES_PER_LIST,
    var maxBytes: Long = FSP_MAX_FILE_LIST_BYTES,

    // Mode of operation
    var mode: FSPWalkerMode = FSPWalkerMode.DRY_RUN,

    // Sender mode
    // var senderMode: FSPSendMode? = null, // TODO: FIXME

    // Optional user data (e.g., sender context)
    var userData: Any? = null,

    // File buffer
    var fileBuf: ByteArray? = ByteArray(FILE_BUF_SIZE), // Allocate 16 MB
    var fileBufSize: Int = FILE_BUF_SIZE,

    // Protocol writer buffer
    // var protoWriteBuf: FspBufWriter? = null, // TODO: FIXME

    // Stats across all runs
    var totalFiles: Long = 0L,
    var totalBytes: Long = 0L,
    var previousTotalBytes: Long = 0L,
    var lastSpeedTimestamp: Instant = Instant.now(),
    var lastSpeedBytes: Long = 0L,
    var lastThroughput: Double = 0.0
) {
    companion object {
        const val FSP_MAX_WALK_DEPTH = 1024          // Example default from protocol
        const val FSP_MAX_FILES_PER_LIST = FSPProtocol.FSP_MAX_FILES_PER_LIST
        const val FSP_MAX_FILE_LIST_BYTES = FSPProtocol.FSP_MAX_FILE_LIST_BYTES
        const val FILE_BUF_SIZE = 16 * 1024 * 1024   // 16 MB
    }

    init {
        // Optional: set userData to self by default, like in C code
        if (userData == null) {
            userData = this
        }
    }
}