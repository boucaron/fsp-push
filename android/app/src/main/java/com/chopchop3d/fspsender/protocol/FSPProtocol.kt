package com.chopchop3d.fspsender.protocol

object  FSPProtocol {
    const val FSP_CHUNK_SIZE = 128L * 1024L * 1024L // 128MB
    const val FSP_MAX_FILES_PER_LIST = 1024L // 1024 Files max
    const val FSP_MAX_FILE_LIST_BYTES = 16L * 1024L * 1024L * 1024L // 16 GB
}