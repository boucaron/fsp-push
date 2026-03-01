package com.chopchop3d.fspsender.dfs

/**
 * Represents a single file in the DFS walker.
 */
data class FSPFileEntry(
    // Entry name, fixed max length similar to C's NAME_MAX + 1
    var name: String = "",

    // File size in bytes
    var size: Long = 0L,

    // SHA256 hash of the entire file if there is no chunk,
    // otherwise the SHA256 hash of all chunk hashes
    var fileHash: ByteArray = ByteArray(32), // SHA256_DIGEST_LENGTH = 32 bytes

    // Chunks information
    var numChunks: Long = 0L,            // Chunks actually used
    var capChunks: Long = 0L,            // Allocated capacity of chunk_hashes
    var chunkHashes: Array<ByteArray> = emptyArray(), // Each ByteArray is SHA256_DIGEST_LENGTH

) {
    companion object {
        const val NAME_MAX = 255   // Same as typical NAME_MAX
        const val SHA256_DIGEST_LENGTH = 32
    }

    init {
        // Ensure name length does not exceed NAME_MAX
        if (name.length > NAME_MAX) {
            name = name.substring(0, NAME_MAX)
        }

    }
}