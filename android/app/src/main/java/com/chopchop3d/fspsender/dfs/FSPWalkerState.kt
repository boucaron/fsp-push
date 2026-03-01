package com.chopchop3d.fspsender.dfs


import java.time.Instant

/**
 * Walker state for batching and dry-run stats.
 */
data class FSPWalkerState(
    // Current directory paths
    var fullPath: String = "",
    var relPath: String = "",

    // Current file entries (directories are handled by the caller)
    var entries: List<FSPFileEntry> = emptyList(),

    // Current batch tracking
    var currentFiles: Int = 0,
    var currentBytes: Long = 0L,
    var flushNeeded: Boolean = false,

    // Dry-run stats (mandatory)
    // var dryRun: FSPDryRunStats, // Non-nullable // TODO: FIXME

    // Depth tracking
    var currentDepth: Int = 0,
    var maxDepth: Int = 0, // 0 = no limit

    // Batching thresholds
    var maxFiles: Int = 0, // Max files per batch
    var maxBytes: Long = 0L, // Max bytes per batch

    // Mode of operation
    var mode: FSPWalkerMode = FSPWalkerMode.DRY_RUN,

    // Sender mode (reuse FSPSendMode class)
    // var senderMode: FSPSendMode? = null, // TODO: FIXME

    // Optional: user data (e.g., sender context)
    var userData: Any? = null,

    // File buffer
    var fileBuf: ByteArray? = null,
    var fileBufSize: Int = 0,

    // Protocol writer buffer
    // var protoWriteBuf: FspBufWriter? = null, // TODO: FIXME

    // Stats across all runs
    var totalFiles: Long = 0L,
    var totalBytes: Long = 0L,
    var previousTotalBytes: Long = 0L,
    var lastSpeedTimestamp: Instant = Instant.now(),
    var lastSpeedBytes: Long = 0L,
    var lastThroughput: Double = 0.0
)