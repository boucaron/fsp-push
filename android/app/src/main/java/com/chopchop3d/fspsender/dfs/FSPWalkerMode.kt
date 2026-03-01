package com.chopchop3d.fspsender.dfs

/**
 * Mode for the DFS walker.
 */
enum class FSPWalkerMode(val value: Int) {
    DRY_RUN(0), // Only scan, populate dry-run stats
    RUN(1)      // Walk, compute SHA256, build batches, send data
}