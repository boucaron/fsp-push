package com.chopchop3d.fspsender.dfs

import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.activity.ComponentActivity
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.security.MessageDigest

data class ScanResult(
    val files: Int,
    val dirs: Int,
    val size: Long
)

class DirectoryScanner(private val context: ComponentActivity) {

    private val buffer = ByteArray(16 * 1024 * 1024)
    private val visitedDirs = mutableSetOf<String>()

    suspend fun scan(
        treeUri: Uri,
        dry_run: Boolean,
        onProgress: ((files: Int, dirs: Int, size: Long) -> Unit)? = null
    ): ScanResult = withContext(Dispatchers.IO) {

        var totalFiles = 0
        var totalDirs = 0
        var totalSize = 0L

        suspend fun dfs(docId: String) {
            if (!visitedDirs.add(docId)) return

            val filesList = mutableListOf<Triple<String, String, Long>>()
            val dirsList = mutableListOf<Pair<String, String>>()

            val childrenUri =
                DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, docId)

            val projection = arrayOf(
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME
            )

            context.contentResolver.query(childrenUri, projection, null, null, null)
                ?.use { c ->
                    while (c.moveToNext()) {
                        val childDocId = c.getString(0)
                        val mime = c.getString(1)
                        val size = c.getLong(2)
                        val name = c.getString(3)

                        if (DocumentsContract.Document.MIME_TYPE_DIR == mime)
                            dirsList.add(childDocId to name)
                        else
                            filesList.add(Triple(childDocId, name, size))
                    }
                }

            filesList.sortBy { it.second.lowercase() }
            dirsList.sortBy { it.second.lowercase() }

            for ((childDocId, _, size) in filesList) {
                totalFiles++
                totalSize += size

                if (!dry_run) {
                    val fileUri =
                        DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocId)
                    try {
                        val sha256 = computeSHA256(fileUri)
                        Log.v("FSPSender", "File: $fileUri, SHA256: $sha256")
                    } catch (e: Exception) {
                        Log.e("FSPSender", "Error hashing file $fileUri", e)
                    }
                }

                onProgress?.invoke(totalFiles, totalDirs, totalSize)
            }

            for ((dirId, _) in dirsList) {
                totalDirs++
                dfs(dirId)
            }
        }

        dfs(DocumentsContract.getTreeDocumentId(treeUri))
        ScanResult(totalFiles, totalDirs, totalSize)
    }

    private suspend fun computeSHA256(fileUri: Uri): String = withContext(Dispatchers.IO) {
        val digest = MessageDigest.getInstance("SHA-256")
        val stream = context.contentResolver.openInputStream(fileUri)
            ?: throw Exception("Cannot open $fileUri")
        stream.use {
            var read: Int
            while (it.read(buffer).also { read = it } != -1) digest.update(buffer, 0, read)
        }
        digest.digest().joinToString("") { "%02x".format(it) }
    }
}