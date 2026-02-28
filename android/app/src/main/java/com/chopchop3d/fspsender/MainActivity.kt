package com.chopchop3d.fspsender

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.DocumentsContract
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.chopchop3d.fspsender.ui.theme.FSPSenderTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.security.MessageDigest

class MainActivity : ComponentActivity() {

    private val openDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
            uri?.let {
                contentResolver.takePersistableUriPermission(
                    it,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
                selectedUri = it
            }
        }

    private var selectedUri by mutableStateOf<Uri?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            FSPSenderTheme {
                MainScreen(
                    onPickDirectory = { openDirectory.launch(null) },
                    selectedUri = selectedUri,
                    context = this
                )
            }
        }
    }
}

@Composable
fun MainScreen(
    onPickDirectory: () -> Unit,
    selectedUri: Uri?,
    context: ComponentActivity
) {
    var fileCount by remember { mutableStateOf(0) }
    var dirCount by remember { mutableStateOf(0) }
    var totalSize by remember { mutableStateOf(0L) }
    var elapsedTime by remember { mutableStateOf(0L) }
    var statusMessage by remember { mutableStateOf("Idle") }

    val scope = rememberCoroutineScope()

    fun updateUI(files: Int, dirs: Int, size: Long, elapsedMs: Long) {
        fileCount = files
        dirCount = dirs
        totalSize = size
        elapsedTime = elapsedMs
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {

        Button(onClick = { onPickDirectory() }) {
            Text("Select Directory")
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (selectedUri != null) {
            Button(
                onClick = {
                    Log.e("FSPSender", "Scan & SHA256 button clicked, starting coroutine")
                    scope.launch {
                        statusMessage = "Starting scan..."
                        fileCount = 0
                        dirCount = 0
                        totalSize = 0L
                        elapsedTime = 0L

                        val startTime = System.currentTimeMillis()

                        val scanResult = withContext(Dispatchers.IO) {
                            scanAndHashDirectory(context, selectedUri) { f, d, s ->
                                val elapsedMs = System.currentTimeMillis() - startTime
                                scope.launch { updateUI(f, d, s, elapsedMs) }
                            }
                        }

                        val totalElapsed = System.currentTimeMillis() - startTime
                        updateUI(scanResult.files, scanResult.dirs, scanResult.size, totalElapsed)
                        statusMessage = "Scan & SHA256 completed"
                    }
                }
            ) {
                Text("Scan & Compute SHA256")
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text("Status: $statusMessage")
        Spacer(modifier = Modifier.height(8.dp))
        Text("Files: $fileCount")
        Text("Directories: $dirCount")
        Text("Total size: $totalSize bytes")
        Spacer(modifier = Modifier.height(8.dp))
        Text("Elapsed time: ${elapsedTime / 1000}.${(elapsedTime % 1000) / 10} s")
        Text("Mean throughput: ${
            if (elapsedTime > 0) {
                val mb = totalSize.toDouble() / (1024 * 1024)
                val sec = elapsedTime.toDouble() / 1000
                String.format("%.2f MB/s", mb / sec)
            } else {
                "0 MB/s"
            }
        }")
    }
}

data class ScanResult(
    val files: Int,
    val dirs: Int,
    val size: Long
)

suspend fun scanAndHashDirectory(
    context: ComponentActivity,
    treeUri: Uri,
    onProgress: ((files: Int, dirs: Int, size: Long) -> Unit)? = null
): ScanResult = coroutineScope {

    var totalFiles = 0
    var totalDirs = 0
    var totalSize = 0L
    val buffer = ByteArray(16 * 1024 * 1024)
    val visitedDirs = mutableSetOf<String>()

    suspend fun dfs(docId: String) {
        if (!visitedDirs.add(docId)) return

        if ( totalDirs % 10 == 0 ) {
            Log.e("FSPSender", "Starting DFS on document ID: $docId")
        }

        val filesList = mutableListOf<Pair<String, Long>>()
        val dirsList = mutableListOf<String>()

        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, docId)
        val projection = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE
        )

        val cursor = context.contentResolver.query(childrenUri, projection, null, null, null)
        cursor?.use { c ->
            while (c.moveToNext()) {
                val childDocId = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID))
                val mime = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE))
                val size = c.getLong(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_SIZE))

                if (DocumentsContract.Document.MIME_TYPE_DIR == mime) {
                    dirsList.add(childDocId)
                } else {
                    filesList.add(childDocId to size)
                }
            }
        }

        // Sort filesList by name asc
        // Sort dirsList by name asc

        for ((childDocId, size) in filesList) {
            totalFiles++
            totalSize += size

            var triggerSize = false
            var thresholdSize = 1024L * 1024L * 10L
            if ( size >= thresholdSize ) {
                triggerSize = true;
            }
            if ( totalSize % thresholdSize == 0L ) {
                triggerSize = true
            }
            var triggerFile = false
            var thresholdFile = 100
            if (  totalFiles % thresholdFile == 0 ) {
                triggerFile = true
            }

            val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocId)

            try {
                val sha256 = computeSHA256(context, fileUri, buffer)
                if ( triggerFile || triggerSize ) {
                    Log.e("FSPSender", "File: $fileUri, Size: $size, SHA256: $sha256")
                }
            } catch (e: Exception) {
                Log.e("FSPSender", "Error hashing file $fileUri", e)
            }

            if (triggerFile ||  triggerSize ) {
                onProgress?.invoke(totalFiles, totalDirs, totalSize)
            }
        }

        for (dirId in dirsList) {
            totalDirs++
            dfs(dirId)
        }
    }

    dfs(DocumentsContract.getTreeDocumentId(treeUri))

    ScanResult(totalFiles, totalDirs, totalSize)
}

suspend fun computeSHA256(context: ComponentActivity, fileUri: Uri, buffer: ByteArray): String =
    withContext(Dispatchers.IO) {
        val digest = MessageDigest.getInstance("SHA-256")
        val stream = context.contentResolver.openInputStream(fileUri)
            ?: throw Exception("Cannot open stream for: $fileUri")

        stream.use {
            var read: Int
            while (it.read(buffer).also { read = it } != -1) {
                digest.update(buffer, 0, read)

               // Log.e("FSPSender", "Read bytes = $read")
            }
        }

        digest.digest().joinToString("") { "%02x".format(it) }
    }