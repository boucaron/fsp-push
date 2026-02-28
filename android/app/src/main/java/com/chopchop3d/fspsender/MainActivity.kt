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
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusDirection
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import com.chopchop3d.fspsender.ui.theme.FSPSenderTheme
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.security.MessageDigest
import java.util.Properties

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

@OptIn(ExperimentalMaterial3Api::class, ExperimentalComposeUiApi::class)
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

    // SSH fields
    var sshHost by remember { mutableStateOf("") }
    var sshUser by remember { mutableStateOf("") }
    var sshPassword by remember { mutableStateOf("") }
    var sshStatus by remember { mutableStateOf("SSH status: Idle") }
    var passwordVisible by remember { mutableStateOf(false) }

    val focusManager = LocalFocusManager.current
    val keyboardController = LocalSoftwareKeyboardController.current
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
        // Directory selection
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
        Text(
            "Mean throughput: ${
                if (elapsedTime > 0) {
                    val mb = totalSize.toDouble() / (1024 * 1024)
                    val sec = elapsedTime.toDouble() / 1000
                    String.format("%.2f MB/s", mb / sec)
                } else {
                    "0 MB/s"
                }
            }"
        )

        Spacer(modifier = Modifier.height(16.dp))

        // SSH input fields
        OutlinedTextField(
            value = sshHost,
            onValueChange = { sshHost = it },
            label = { Text("SSH Host") },
            modifier = Modifier.fillMaxWidth(),
            keyboardOptions = KeyboardOptions.Default.copy(imeAction = ImeAction.Next),
            keyboardActions = KeyboardActions(
                onNext = { focusManager.moveFocus(FocusDirection.Down) }
            )
        )
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedTextField(
            value = sshUser,
            onValueChange = { sshUser = it },
            label = { Text("SSH Username") },
            modifier = Modifier.fillMaxWidth(),
            keyboardOptions = KeyboardOptions.Default.copy(imeAction = ImeAction.Next),
            keyboardActions = KeyboardActions(
                onNext = { focusManager.moveFocus(FocusDirection.Down) }
            )
        )
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedTextField(
            value = sshPassword,
            onValueChange = { sshPassword = it },
            label = { Text("SSH Password") },
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = if (passwordVisible) VisualTransformation.None else PasswordVisualTransformation(),
            trailingIcon = {
                Icon(
                    imageVector = Icons.Default.Lock,
                    contentDescription = "Toggle password visibility",
                    modifier = Modifier.clickable { passwordVisible = !passwordVisible }
                )
            },
            keyboardOptions = KeyboardOptions.Default.copy(
                keyboardType = KeyboardType.Password,
                imeAction = ImeAction.Done
            ),
            keyboardActions = KeyboardActions(
                onDone = { keyboardController?.hide() }
            )
        )
        Spacer(modifier = Modifier.height(8.dp))

        Button(onClick = {
            scope.launch {
                sshStatus = "Connecting..."
                val success = testSSHConnection(sshHost, sshUser, sshPassword)
                sshStatus = if (success) "SSH status: Connected!" else "SSH status: Failed"
            }
        }) {
            Text("Test SSH Connection")
        }
        Spacer(modifier = Modifier.height(8.dp))
        Text(sshStatus)
    }
}

// --- SSH + Scan functions (unchanged) ---

suspend fun testSSHConnection(host: String, username: String, password: String): Boolean =
    withContext(Dispatchers.IO) {
        try {
            val jsch = JSch()
            val session: Session = jsch.getSession(username, host, 22)
            session.setPassword(password)
            val config = Properties()
            config["StrictHostKeyChecking"] = "no"
            // Disable compression
            config["compression.s2c"] = "none"
            config["compression.c2s"] = "none"

            session.setConfig(config)
            session.timeout = 5000
            session.connect()
            session.disconnect()
            true
        } catch (e: Exception) {
            Log.e("FSPSender", "SSH connection failed", e)
            false
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
        if (totalDirs % 10 == 0) Log.e("FSPSender", "Starting DFS on document ID: $docId")

        val filesList = mutableListOf<Triple<String, String, Long>>()
        val dirsList = mutableListOf<Pair<String, String>>()

        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, docId)
        val projection = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME
        )

        val cursor = context.contentResolver.query(childrenUri, projection, null, null, null)
        cursor?.use { c ->
            while (c.moveToNext()) {
                val childDocId = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID))
                val mime = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE))
                val size = c.getLong(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_SIZE))
                val name = c.getString(c.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME))

                if (DocumentsContract.Document.MIME_TYPE_DIR == mime) dirsList.add(childDocId to name)
                else filesList.add(Triple(childDocId, name, size))
            }
        }

        filesList.sortBy { it.second.lowercase() }
        dirsList.sortBy { it.second.lowercase() }

        for ((childDocId, _, size) in filesList) {
            totalFiles++
            var triggerSize = false
            val thresholdSize = 1024L * 1024L * 10L
            if (totalSize / thresholdSize < (totalSize + size) / thresholdSize) triggerSize = true
            totalSize += size

            var triggerFile = false
            val thresholdFile = 10
            if (totalFiles % thresholdFile == 0) triggerFile = true

            val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocId)

            try {
                val sha256 = computeSHA256(context, fileUri, buffer)
                if (triggerFile || triggerSize) Log.e("FSPSender", "File: $fileUri, Size: $size, SHA256: $sha256")
            } catch (e: Exception) {
                Log.e("FSPSender", "Error hashing file $fileUri", e)
            }

            if (triggerFile || triggerSize) onProgress?.invoke(totalFiles, totalDirs, totalSize)
        }

        for ((dirId, _) in dirsList) {
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
            while (it.read(buffer).also { read = it } != -1) digest.update(buffer, 0, read)
        }
        digest.digest().joinToString("") { "%02x".format(it) }
    }