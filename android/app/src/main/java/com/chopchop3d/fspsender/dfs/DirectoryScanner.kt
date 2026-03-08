package com.chopchop3d.fspsender.dfs

import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.activity.ComponentActivity
import com.chopchop3d.fspsender.SshConfig
import com.chopchop3d.fspsender.SshSender
import com.chopchop3d.fspsender.protocol.FSPProtocol
import com.chopchop3d.fspsender.protocol.FSPSendDirectory
import com.chopchop3d.fspsender.protocol.FSPSendFileList
import com.chopchop3d.fspsender.protocol.FSPSendMode
import com.chopchop3d.fspsender.protocol.FSPSendStatBytes
import com.chopchop3d.fspsender.protocol.FSPSendStatFiles
import com.chopchop3d.fspsender.protocol.FSPSendVersion
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kotlinx.coroutines.yield
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest

/**
 * Scans directories and updates the FSPWalkerState.
 */
class DirectoryScanner(
    private val context: ComponentActivity,
    private val walkerState: FSPWalkerState,
    private val sshConfig: SshConfig?
) {

    private val visitedDirs = mutableSetOf<String>()

    private var sshSender: SshSender? = null

    /**
     * Starts a DFS scan on the given tree URI.
     *
     * @param treeUri URI of the root directory.
     * @param dryRun If true, do not compute SHA256 hashes.
     * @param onProgress Callback invoked after each file, providing the full walker state.
     */
    suspend fun scan(
        treeUri: Uri,
        dryRun: Boolean,
        onProgress: ((walkerState: FSPWalkerState) -> Unit)? = null
    ): FSPWalkerState = withContext(Dispatchers.IO) {

        if (!dryRun && sshConfig != null) {
            sshSender = SshSender()

            val connected = sshSender!!.connect(
                host = sshConfig.host,
                username = sshConfig.username,
                password = sshConfig.password,
                port = sshConfig.port
            )


            if (!connected) {
                throw RuntimeException("SSH connection failed")
            }


            // TODO: FIXME target directory here
            sshSender!!.startProcess("fsp-recv tests") { output ->
                walkerState.stderrServer = output
            }
            delay(100)
            sshSender!!.sendText(FSPSendVersion.sendCommand())
            // TODO: Handle various modes
            sshSender!!.sendText(FSPSendMode.sendCommandStatic(FSPSendMode.FSP_APPEND))
            sshSender!!.sendText(FSPSendStatBytes.sendCommand(walkerState.totalBytes))
            sshSender!!.sendText(FSPSendStatFiles.sendCommand(walkerState.totalFiles))



        }

        var tenMB = 10*1024*1024

        suspend fun dfs(docId: String, name: String) {


            var rname = name;
            if ( rname.isEmpty()) {
                rname = "."
            }
            Log.e("dfs", "start : docId $docId rname $rname ")

            if (!visitedDirs.add(docId)) return

            // Respect max depth if configured
            if (walkerState.maxDepth > 0 && walkerState.currentDepth >= walkerState.maxDepth) {
                return
            }

            // Save previous paths (stack behaviour)
            val previousRel = walkerState.relPath
            val previousFull = walkerState.fullPath

            // Update paths
            walkerState.relPath = if (previousRel.isEmpty()) rname else "$previousRel/$rname"
            walkerState.fullPath = if (previousFull.isEmpty()) rname else "$previousFull/$rname"

            Log.e("dfs", "walkerState.relPath ${walkerState.relPath}")

            // Reset current entries for this folder
            walkerState.entries = mutableListOf()
            walkerState.currentDepth++

            val filesList = mutableListOf<Triple<String, String, Long>>()
            val dirsList = mutableListOf<Pair<String, String>>()

            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, docId)
            val projection = arrayOf(
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME
            )

            context.contentResolver.query(childrenUri, projection, null, null, null)?.use { c ->
                while (c.moveToNext()) {
                    val childDocId = c.getString(0)
                    val mime = c.getString(1)
                    val size = c.getLong(2)
                    val childName = c.getString(3)

                    if (DocumentsContract.Document.MIME_TYPE_DIR == mime) {
                        dirsList.add(childDocId to childName)
                    } else {
                        filesList.add(Triple(childDocId, childName, size))
                    }
                }
            }

            // Sort alphabetically
            filesList.sortBy { it.second.lowercase() }
            dirsList.sortBy { it.second.lowercase() }

            // Process files
            for ((childDocId, fileName, size) in filesList) {
                walkerState.currentFiles++
                walkerState.currentBytes += size


                val fileEntry = FSPFileEntry(name = fileName, size = size, treeUri = treeUri, childDocId = childDocId)
                (walkerState.entries as MutableList).add(fileEntry)


                if ( dryRun) {
                  walkerState.totalFiles++
                }



                // UI slow speed refresh
                if ( walkerState.totalBytes % 10L == 0L ) {
                    walkerState.triggerDisplay ++;
                }
                else if ( (walkerState.totalBytes + size)/ tenMB  > (walkerState.totalBytes/tenMB)) {
                    walkerState.triggerDisplay ++;
                }

                walkerState.totalBytes += size


                onProgress?.invoke(walkerState)
            }

            if (!dryRun) {
                processDirectory(walkerState);
            }


            // Recurse into directories
            for ((dirId, dirName) in dirsList) {
                dfs(dirId, dirName)
            }

            walkerState.currentDepth--
            // Restore previous paths (pop stack)
            walkerState.relPath = previousRel
            walkerState.fullPath = previousFull
        }

        val rootId = DocumentsContract.getTreeDocumentId(treeUri)
        dfs(rootId, "")

        if ( walkerState.currentDepth == 0 ) {
            walkerState.triggerDisplay ++;
            walkerState.dryRun.computeSimulationThroughput(walkerState.totalBytes.toDouble())

            if ( !dryRun ) {


                Log.e("dfs", "finished with success")

                sshSender?.sendText("END\n")
                sshSender?.disconnect()
                sshSender = null
            }
        }

        walkerState
    }


    private suspend fun processDirectory(walkerState: FSPWalkerState) {

        // Send Directory CMD
        // Create File Batch
        //   For each File Batch
        //   Send File Count
        //   Send Metadata File - NO SHA
        //   Send File Data & Compute SHA
        //   Send Metadata File - SHA

        sshSender!!.sendText(FSPSendDirectory.sendCommand(walkerState.relPath))

        val batches = createBatches(
            walkerState.entries,
            maxFiles = FSPProtocol.FSP_MAX_FILES_PER_LIST,
            maxBytes = FSPProtocol.FSP_MAX_FILE_LIST_BYTES
        )

        for ((batchIndex, batch) in batches.withIndex()) {
            Log.e("Batch", "Processing batch #$batchIndex with ${batch.size} files")
            processFileBatch(walkerState, batch)
        }


    }


    /**
     * Create file batches based on count and size constraints.
     */
    private fun createBatches(
        entries: List<FSPFileEntry>,
        maxFiles: Long,
        maxBytes: Long
    ): List<List<FSPFileEntry>> {
        val batches = mutableListOf<MutableList<FSPFileEntry>>()
        var currentBatch = mutableListOf<FSPFileEntry>()
        var currentBatchSize = 0L

        for (file in entries) {
            val fileSize = file.size

            val batchFull = currentBatch.size >= maxFiles || (currentBatchSize + fileSize) > maxBytes
            if (batchFull) {
                // Save current batch and start a new one
                batches.add(currentBatch)
                currentBatch = mutableListOf()
                currentBatchSize = 0
            }

            currentBatch.add(file)
            currentBatchSize += fileSize
        }

        // Add the last batch if not empty
        if (currentBatch.isNotEmpty()) {
            batches.add(currentBatch)
        }

        return batches
    }



    private suspend fun processFileBatch(
        walkerState: FSPWalkerState,
        batch: List<FSPFileEntry>
    ) = withContext(Dispatchers.IO) {

        //  Send Metadata File - NO SHA
        //  Send File Data & Compute SHA
        //  Send Metadata File - SHA

        sshSender!!.sendText(FSPSendFileList.sendCommand())
        sshSender!!.sendText("FILES: ${batch.size}\n")
        sendFileMetadataBinary(batch)

        sendFileData(batch, walkerState)

        sshSender!!.sendText("HASH FILES: ${batch.size}\n")
        sendFileMetadataBinary(batch)



    }




    private suspend fun sendFileMetadataBinary(entries: List<FSPFileEntry>)  {
        for (entry in entries) {
            // 1️⃣ Filename length (uint16_t, big endian)
            val nameBytes = entry.name.toByteArray(Charsets.UTF_8)
            if (nameBytes.size > 0xFFFF) throw IllegalArgumentException("Filename too long: ${entry.name}")
            val nameLenBuf = ByteBuffer.allocate(2)
                .order(ByteOrder.BIG_ENDIAN)
                .putShort(nameBytes.size.toShort())
                .array()
            sshSender!!.sendBinary(nameLenBuf, flush = false)

            // 2️⃣ Filename bytes
            sshSender!!.sendBinary(nameBytes, flush = false)

            // 3️⃣ File size (uint64_t, big endian)
            val sizeBuf = ByteBuffer.allocate(8)
                .order(ByteOrder.BIG_ENDIAN)
                .putLong(entry.size)
                .array()
            sshSender!!.sendBinary(sizeBuf, flush = false)

            // 4️⃣ File hash (32 bytes placeholder or actual SHA256)
            if (entry.fileHash.size != 32) throw IllegalArgumentException("File hash must be 32 bytes")
            sshSender!!.sendBinary(entry.fileHash, flush = false)

            // 5️⃣ Number of chunks (uint64_t, big endian)
            val chunksBuf = ByteBuffer.allocate(8)
                .order(ByteOrder.BIG_ENDIAN)
                .putLong(entry.numChunks)
                .array()
            sshSender!!.sendBinary(chunksBuf, flush = false)

            // 6️⃣ Chunk hashes if present
            if (entry.numChunks > 0) {

                val hashes = entry.chunkHashes
                if (hashes.isEmpty()) {
                    throw IllegalStateException("numChunks > 0 but chunkHashes empty")
                }

                for (h in hashes) {
                    if (h.size != 32) {
                        throw IllegalStateException("Invalid chunk hash length")
                    }

                    sshSender!!.sendBinary(h, flush = false)
                }
            }

            // 7️⃣ Flush after sending one file metadata
            sshSender!!.flush()
        }
    }

    private suspend fun sendFileData(entries: List<FSPFileEntry>,
                                     walkerState: FSPWalkerState)  {
        for (entry in entries) {
            if ( entry.size > FSPProtocol.FSP_CHUNK_SIZE ) {
                sendFileDataChunk(entry, walkerState)
            }  else {
                sendFileDataSmall(entry, walkerState)
            }
        }
    }

    private suspend fun sendFileDataSmall(entry: FSPFileEntry, walkerState: FSPWalkerState) {
        val digest = MessageDigest.getInstance("SHA-256")
        val fileUri = DocumentsContract.buildDocumentUriUsingTree(entry.treeUri, entry.childDocId)
        val stream = context.contentResolver.openInputStream(fileUri)
            ?: throw Exception("Cannot open $fileUri")
        // TODO: Test after by using the file descriptor => may be a 2 throughput increase ???
        //   val pfd = contentResolver.openFileDescriptor(uri, "r")!!
        //  val fis = FileInputStream(pfd.fileDescriptor)


        stream.use { input ->
            var read: Int
            while (input.read(walkerState.fileBuf).also { read = it } != -1) {
                // Update SHA256
                digest.update(walkerState.fileBuf, 0, read)
                // Send data chunk over SSH
                sshSender!!.sendBinary(walkerState.fileBuf.copyOfRange(0, read), flush = false)
            }
        }

        // Compute final SHA256 hash and store it in binary form
        entry.fileHash = digest.digest()

        sshSender!!.flush()
        yield()
    }

    private suspend fun sendFileDataChunk(entry: FSPFileEntry, walkerState: FSPWalkerState) {

        val fileUri = DocumentsContract.buildDocumentUriUsingTree(entry.treeUri, entry.childDocId)
        val stream = context.contentResolver.openInputStream(fileUri)
            ?: throw Exception("Cannot open $fileUri")

        val chunkSize = FSPProtocol.FSP_CHUNK_SIZE
        val buf = walkerState.fileBuf

        val chunkHashes = mutableListOf<ByteArray>()

        stream.use { input ->

            var bytesInChunk = 0L
            var chunkDigest = MessageDigest.getInstance("SHA-256")

            var read: Int

            while (input.read(buf).also { read = it } != -1) {

                var offset = 0

                while (offset < read) {

                    val remainingChunk = chunkSize - bytesInChunk
                    val toProcess = minOf(remainingChunk.toInt(), read - offset)

                    // Update chunk SHA
                    chunkDigest.update(buf, offset, toProcess)

                    // Send data
                    sshSender!!.sendBinary(buf.copyOfRange(offset, offset + toProcess), flush = false)

                    bytesInChunk += toProcess
                    offset += toProcess

                    // Chunk completed
                    if (bytesInChunk == chunkSize) {
                        chunkHashes.add(chunkDigest.digest())
                        chunkDigest = MessageDigest.getInstance("SHA-256")
                        bytesInChunk = 0

                        sshSender!!.flush()
                        yield()
                    }
                }
            }

            // Final partial chunk
            if (bytesInChunk > 0) {
                chunkHashes.add(chunkDigest.digest())
            }
        }

        // Store chunk info
        entry.numChunks = chunkHashes.size.toLong()
        entry.chunkHashes = chunkHashes.toTypedArray()

        // Compute Merkle-0 root
        val fileDigest = MessageDigest.getInstance("SHA-256")
        for (h in entry.chunkHashes) {
            fileDigest.update(h)
        }

        entry.fileHash = fileDigest.digest()

        sshSender!!.flush()
        yield()
    }



}