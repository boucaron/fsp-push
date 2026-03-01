package com.chopchop3d.fspsender.dfs;

data class FSPDirEntry(
    // Directory name, limited to NAME_MAX characters
    var name: String = ""
            ) {
        companion object {
        const val NAME_MAX = 255
        }

        init {
            // Ensure name length does not exceed NAME_MAX
            if (name.length > NAME_MAX) {
                name = name.substring(0, NAME_MAX)
            }
        }
}
