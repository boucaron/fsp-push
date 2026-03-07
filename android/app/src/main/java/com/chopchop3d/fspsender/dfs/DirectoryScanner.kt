package com.chopchop3d.fspsender.dfs

import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.activity.ComponentActivity
import com.chopchop3d.fspsender.SshConfig
import com.chopchop3d.fspsender.SshSender
import com.chopchop3d.fspsender.protocol.FSPSendMode
import com.chopchop3d.fspsender.protocol.FSPSendVersion
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
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

            sshSender!!.startFspRecv("tests")
            sshSender!!.sendText(FSPSendVersion.sendCommand())
            // TODO: Handle various modes
            sshSender!!.sendText(FSPSendMode.sendCommandStatic(FSPSendMode.FSP_APPEND))


        }

        var tenMB = 10*1024*1024

        suspend fun dfs(docId: String, name: String) {
            if (!visitedDirs.add(docId)) return

            // Respect max depth if configured
            if (walkerState.maxDepth > 0 && walkerState.currentDepth >= walkerState.maxDepth) {
                return
            }

            // Save previous paths (stack behaviour)
            val previousRel = walkerState.relPath
            val previousFull = walkerState.fullPath

            // Update paths
            walkerState.relPath = if (previousRel.isEmpty()) name else "$previousRel/$name"
            walkerState.fullPath = if (previousFull.isEmpty()) name else "$previousFull/$name"

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

                /*
                if (!dryRun) {
                    val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocId)
                    try {
                        val sha256 = computeSHA256(fileUri, walkerState)
                        Log.v("FSPSender", "File: $fileUri, SHA256: $sha256")
                    } catch (e: Exception) {
                        Log.e("FSPSender", "Error hashing file $fileUri", e)
                    }
                } else { */
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

            processDirectory(walkerState);


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



                sshSender?.flush()
                sshSender?.disconnect()
                sshSender = null
            }
        }

        walkerState
    }


    private fun processDirectory(walkerState: FSPWalkerState) {
        // Send Directory CMD
        // For each File Batch
        // Send Metadata File - NO SHA
        // Send File Data & Compute SHA
        // Send Metadata File - SHA



    }

    /**
     * Computes SHA-256 hash of a file using the walkerState's file buffer.
     */
    private suspend fun computeSHA256(fileUri: Uri, walkerState: FSPWalkerState): String =
        withContext(Dispatchers.IO) {
            val digest = MessageDigest.getInstance("SHA-256")
            val stream = context.contentResolver.openInputStream(fileUri)
                ?: throw Exception("Cannot open $fileUri")

            stream.use {
                var read: Int
                while (it.read(walkerState.fileBuf).also { read = it } != -1) {
                    digest.update(walkerState.fileBuf, 0, read)
                }
            }

            digest.digest().joinToString("") { "%02x".format(it) }
        }
}