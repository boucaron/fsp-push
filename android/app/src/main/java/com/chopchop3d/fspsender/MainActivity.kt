package com.chopchop3d.fspsender

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.spring
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusDirection
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import com.chopchop3d.fspsender.dfs.DirectoryScanner
import com.chopchop3d.fspsender.dfs.FSPWalkerMode
import com.chopchop3d.fspsender.dfs.FSPWalkerState
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FILE_BUF_SIZE
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_WALK_DEPTH
import com.chopchop3d.fspsender.protocol.FSPProtocol
import com.chopchop3d.fspsender.ui.theme.FSPSenderTheme
import com.chopchop3d.fspsender.ui.theme.ZenburnButton
import kotlinx.coroutines.launch
import java.time.Instant


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
        val walkerState = FSPWalkerState(
            "",
            "",
            emptyList(),
            0L,
            0L,
            false,
            0,
            FSP_MAX_WALK_DEPTH,
            FSPProtocol.FSP_MAX_FILES_PER_LIST,
            FSPProtocol.FSP_MAX_FILE_LIST_BYTES,
            FSPWalkerMode.DRY_RUN,
            ByteArray(FILE_BUF_SIZE),
            0L,
            0L,
            0L,
            Instant.now(),
            0L,
            0.0)


        setContent {
            FSPSenderTheme {
                // Root Surface ensures dark background
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen(
                        onPickDirectory = { openDirectory.launch(null) },
                        selectedUri = selectedUri,
                        onScanDirectory = { uri, dryRun, onProgress ->
                            val scanner = DirectoryScanner(this, walkerState)
                            scanner.scan(uri, dryRun, onProgress)
                        },
                        context = this
                    )
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalComposeUiApi::class)
@Composable
fun MainScreen(
    onPickDirectory: () -> Unit,
    selectedUri: Uri?,
    onScanDirectory: suspend (Uri, Boolean, (files: Int, dirs: Int, size: Long) -> Unit) -> Unit,
    context: ComponentActivity
) {
    var fileCount by remember { mutableStateOf(0) }
    var dirCount by remember { mutableStateOf(0) }
    var totalSize by remember { mutableStateOf(0L) }
    var elapsedTime by remember { mutableStateOf(0L) }
    var statusMessage by remember { mutableStateOf("Idle") }
    var dry_run by remember { mutableStateOf(true) }

    // SSH fields
    var sshHost by remember { mutableStateOf("") }
    var sshUser by remember { mutableStateOf("") }
    var sshPassword by remember { mutableStateOf("") }
    var sshStatus by remember { mutableStateOf("SSH status: Idle") }
    var passwordVisible by remember { mutableStateOf(false) }

    var targetDirectory by remember { mutableStateOf("") }

    val focusManager = LocalFocusManager.current
    val keyboardController = LocalSoftwareKeyboardController.current
    val scope = rememberCoroutineScope()

    val sshHelper = remember { SshHelper() }

    fun updateUI(files: Int, dirs: Int, size: Long, elapsedMs: Long) {
        fileCount = files
        dirCount = dirs
        totalSize = size
        elapsedTime = elapsedMs
    }

    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp)
    ) {
        // Directory selection
        Button(onClick = { onPickDirectory() }) {
            Text("Select Directory")
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (selectedUri != null) {
            ZenburnButton(
                onClick = {
                    Log.e("FSPSender", "Dry-run, starting coroutine")
                    scope.launch {
                        statusMessage = "Starting dry-run..."
                        fileCount = 0
                        dirCount = 0
                        totalSize = 0L
                        elapsedTime = 0L
                        dry_run = true
                        val startTime = System.currentTimeMillis()

                        onScanDirectory(selectedUri, dry_run) { f, d, s ->
                            val elapsedMs = System.currentTimeMillis() - startTime
                            fileCount = f
                            dirCount = d
                            totalSize = s
                            elapsedTime = elapsedMs
                        }

                        statusMessage = "Dry-run completed"
                    }
                }
            ) {
                Text("Dry-run")
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
                if (!dry_run && elapsedTime > 0) {
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
        Accordion(title = "SSH Settings") {
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
            OutlinedTextField(
                value = targetDirectory,
                onValueChange = { targetDirectory = it },
                label = { Text("Target Directory") },
                modifier = Modifier.fillMaxWidth(),
                keyboardOptions = KeyboardOptions.Default.copy(imeAction = ImeAction.Next),
                keyboardActions = KeyboardActions(
                    onNext = { focusManager.moveFocus(FocusDirection.Down) }
                )
            )
            Spacer(modifier = Modifier.height(8.dp))

            ZenburnButton(onClick = {
                scope.launch {
                    sshStatus = "Connecting..."
                    val success = sshHelper.testConnection(sshHost, sshUser, sshPassword)
                    sshStatus = if (success) "SSH status: Connected!" else "SSH status: Failed"
                }
            }) {
                Text("Test SSH Connection")
            }
            Spacer(modifier = Modifier.height(8.dp))
            ZenburnButton(onClick = {
                scope.launch {
                    sshStatus = "Connecting..."
                    val success = sshHelper.checkTargetDirectory(
                        targetDirectory,
                        sshHost,
                        sshUser,
                        sshPassword
                    )
                    sshStatus =
                        if (success) "SSH: target directory exists" else "SSH: target directory does not exist"
                }
            }) {
                Text("Test target directory")
            }
            Spacer(modifier = Modifier.height(8.dp))
            ZenburnButton(onClick = {
                scope.launch {
                    sshStatus = "Connecting..."
                    val success = sshHelper.checkFSPReceiverExists(sshHost, sshUser, sshPassword)
                    sshStatus =
                        if (success) "SSH: fsp-recv exists on target host" else "SSH: fsp-recv does not exist or not in path"
                }
            }) {
                Text("Test fsp-recv Connection")
            }
            Spacer(modifier = Modifier.height(8.dp))
            Text(sshStatus)
        }


    }
}



@Composable
fun Accordion(
    title: String,
    initiallyExpanded: Boolean = false,
    content: @Composable () -> Unit
) {
    var expanded by remember { mutableStateOf(initiallyExpanded) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .animateContentSize(animationSpec = spring()),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
        ) {
            // Header / clickable title
            Text(
                text = title + if (expanded) " ▲" else " ▼",
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { expanded = !expanded }
                    .padding(12.dp),
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurface
            )

            // Content only visible when expanded
            if (expanded) {
                Column(modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
                    content()
                }
            }
        }
    }
}
