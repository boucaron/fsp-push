package com.chopchop3d.fspsender

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.DocumentsContract
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.chopchop3d.fspsender.ui.theme.FSPSenderTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

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
    var readTime by remember { mutableStateOf(0L) } // cumulative read time
    var isScanning by remember { mutableStateOf(false) }
    var statusMessage by remember { mutableStateOf("Idle") }

    val scope = rememberCoroutineScope()

    fun updateUI(files: Int, dirs: Int, size: Long, startTime: Long) {
        fileCount = files
        dirCount = dirs
        totalSize = size
        elapsedTime = System.currentTimeMillis() - startTime
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
                    scope.launch {
                        isScanning = true
                        fileCount = 0
                        dirCount = 0
                        totalSize = 0L
                        elapsedTime = 0L
                        readTime = 0L
                        statusMessage = "Starting scan..."

                        val startTime = System.currentTimeMillis()

                        val scanResult = withContext(Dispatchers.IO) {
                            scanAndReadDirectory(context, selectedUri) { f, d, s, readMs ->
                                scope.launch {
                                    updateUI(f, d, s, startTime)
                                    readTime = readMs
                                }
                            }
                        }

                        // Final update
                        updateUI(scanResult.files, scanResult.dirs, scanResult.size, startTime)
                        statusMessage = "Scan & read completed"
                        isScanning = false
                    }
                }
            ) {
                Text("Scan & Read")
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
        Text("Read time: ${readTime / 1000}.${(readTime % 1000) / 10} s")
    }
}

data class ScanResult(
    val files: Int,
    val dirs: Int,
    val size: Long
)

/**
 * DFS scan with inline file read and live read-time measurement
 */
suspend fun scanAndReadDirectory(
    context: ComponentActivity,
    treeUri: Uri,
    onProgress: ((files: Int, dirs: Int, size: Long, readTimeMs: Long) -> Unit)? = null
): ScanResult = coroutineScope {

    var totalFiles = 0
    var totalDirs = 0
    var totalSize = 0L
    var totalReadTime = 0L
    val buffer = ByteArray(16 * 1024 * 1024) // 16MB
    val visitedDirs = mutableSetOf<String>()
    var thresholdFiles = 100
    var thresholdSize = 1024L * 1024L * 10L // 10MB

    suspend fun dfs(docId: String) {
        if (!visitedDirs.add(docId)) return

        val filesList = mutableListOf<Pair<String, Long>>() // documentId, size
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
                val childDocId =
                    c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID))
                val mime = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE))
                val size = c.getLong(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_SIZE))
                if (DocumentsContract.Document.MIME_TYPE_DIR == mime) dirsList.add(childDocId)
                else filesList.add(childDocId to size)
            }
        }

        // Process files and read them
        for ((childDocId, size) in filesList) {
            totalFiles++
            totalSize += size

            // Read the file and measure time
            val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocId)
            val readMs = measureReadFile(context, fileUri, buffer)
            totalReadTime += readMs

            if (totalFiles % thresholdFiles == 0 || totalSize % thresholdSize == 0L) {
                withContext(Dispatchers.Main) {
                    onProgress?.invoke(totalFiles, totalDirs, totalSize, totalReadTime)
                }
            }
        }

        // Recurse into directories
        for (dirId in dirsList) {
            totalDirs++
            dfs(dirId)
        }
    }

    dfs(DocumentsContract.getTreeDocumentId(treeUri))

    ScanResult(totalFiles, totalDirs, totalSize)
}

/**
 * Reads an entire file using the given buffer and returns time spent in ms
 */
suspend fun measureReadFile(context: ComponentActivity, fileUri: Uri, buffer: ByteArray): Long =
    withContext(Dispatchers.IO) {
        var totalRead = 0L
        val startTime = System.currentTimeMillis()
        context.contentResolver.openInputStream(fileUri)?.use { stream ->
            var read: Int
            while (stream.read(buffer).also { read = it } != -1) {
                totalRead += read
            }
        }
        System.currentTimeMillis() - startTime
    }