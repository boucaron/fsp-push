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
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material3.*
import androidx.compose.runtime.*
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
import com.chopchop3d.fspsender.dfs.*
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FILE_BUF_SIZE
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_FILE_LIST_BYTES
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_FILES_PER_LIST
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_WALK_DEPTH
import com.chopchop3d.fspsender.ui.theme.FSPSenderTheme
import com.chopchop3d.fspsender.ui.theme.ZenburnButton
import kotlinx.coroutines.launch
import java.time.Instant

class MainActivity : ComponentActivity() {

    private var walkerState by mutableStateOf(
        FSPWalkerState().apply {
            fullPath = ""
            relPath = ""
            entries = mutableListOf()
            currentFiles = 0L
            currentBytes = 0L
            flushNeeded = false
            dryRun = FSPDryRunStats()
            currentDepth = 0
            maxDepth = FSP_MAX_WALK_DEPTH
            maxBytes = FSP_MAX_FILE_LIST_BYTES
            maxFiles = FSP_MAX_FILES_PER_LIST
            mode = FSPWalkerMode.DRY_RUN
            fileBuf = ByteArray(FILE_BUF_SIZE)
            totalFiles = 0L
            totalBytes = 0L
            previousTotalBytes = 0L
            lastSpeedTimestamp = Instant.now()
            lastSpeedBytes = 0L
            lastThroughput = 0.0
        }
    )

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
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen(
                        onPickDirectory = { openDirectory.launch(null) },
                        selectedUri = selectedUri,
                        walkerState = walkerState,
                        onWalkerStateChange = { updated -> walkerState = updated },
                        onScanDirectory = { uri, dryRun, sshConfig, onProgress ->
                            val scanner = DirectoryScanner(this, walkerState, sshConfig)
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
    walkerState: FSPWalkerState,
    onWalkerStateChange: (FSPWalkerState) -> Unit,
    onScanDirectory: suspend (Uri, Boolean, SshConfig?, (walkerState: FSPWalkerState) -> Unit) -> Unit,
    context: ComponentActivity
) {
    val scope = rememberCoroutineScope()
    var statusMessage by remember { mutableStateOf("Idle") }
    var dry_run by remember { mutableStateOf(true) }

    var walkerStateLocal by remember { mutableStateOf(walkerState) }

    var triggerDisplay = walkerStateLocal.triggerDisplay
    var displayTotalFiles by remember { mutableStateOf(0L) }
    var displayTotalSize by remember { mutableStateOf("") }
    var displayTotalSizeLong by remember { mutableStateOf(0L) }
    var displaySimulatedTime by remember { mutableStateOf("") }
    var elapsedTime by remember { mutableStateOf(0L) }

    var sshHost by remember { mutableStateOf("192.168.178.32") }
    var sshUser by remember { mutableStateOf("admin") }
    var sshPassword by remember { mutableStateOf("") }
    var sshStatus by remember { mutableStateOf("SSH status: Idle") }
    var passwordVisible by remember { mutableStateOf(false) }
    var targetDirectory by remember { mutableStateOf("tests") }

    val focusManager = LocalFocusManager.current
    val keyboardController = LocalSoftwareKeyboardController.current

    val sshHelper = remember { SshHelper() }
    val scrollState = rememberScrollState()


    // Load saved SSH settings once at start
    LaunchedEffect(Unit) {
        val snapshot = FSPSettings.getConfigSnapshot(context)
        sshHost = snapshot.hostname ?: ""
        sshUser = snapshot.username ?: ""
        targetDirectory = snapshot.targetDirectory ?: ""
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp)
    ) {
        // Directory selection
        Button(onClick = { onPickDirectory() }) {
            Text("Select Source Directory")
        }

        Spacer(modifier = Modifier.height(16.dp))

        if (selectedUri != null) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                // Dry-run button
                ZenburnButton(
                    onClick = {
                        scope.launch {
                            statusMessage = "Starting dry-run..."
                            elapsedTime = 0L
                            dry_run = true
                            walkerStateLocal.totalBytes = 0
                            walkerStateLocal.totalFiles = 0
                            val startTime = System.currentTimeMillis()

                            onScanDirectory(selectedUri, dry_run, null) { updatedState ->
                                walkerStateLocal = updatedState
                                onWalkerStateChange(updatedState)
                                elapsedTime = System.currentTimeMillis() - startTime
                            }

                            statusMessage = "Dry-run completed"
                        }
                    }
                ) { Text("Dry-run") }

                // Run button
                ZenburnButton(
                    onClick = {
                        scope.launch {
                            statusMessage = "Starting transfer..."
                            elapsedTime = 0L
                            dry_run = false


                            if (!NetworkUtils.isNetworkAvailable(context)) {
                                Log.e("FSP", "No network available, aborting SSH")
                                statusMessage = "No network available ! Aborting !"
                            } else {

                                val sshConfig = SshConfig(
                                    host = sshHost,
                                    username = sshUser,
                                    password = sshPassword,
                                    port = 22
                                )
                                val startTime = System.currentTimeMillis()

                                onScanDirectory(selectedUri, dry_run, sshConfig) { updatedState ->
                                    walkerStateLocal = updatedState
                                    onWalkerStateChange(updatedState)
                                    elapsedTime = System.currentTimeMillis() - startTime
                                }

                                statusMessage = "Transfer completed"
                            }
                        }
                    }
                ) { Text("Run") }
            }
        }

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

        Spacer(modifier = Modifier.height(16.dp))

        Accordion(title = "SSH Settings") {
            OutlinedTextField(
                value = sshHost,
                onValueChange = { sshHost = it },
                label = { Text("SSH Host") },
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(modifier = Modifier.height(8.dp))
            OutlinedTextField(
                value = sshUser,
                onValueChange = { sshUser = it },
                label = { Text("SSH Username") },
                modifier = Modifier.fillMaxWidth()
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
                }
            )
            Spacer(modifier = Modifier.height(8.dp))
            ZenburnButton(onClick = {
                scope.launch {
                    if (!NetworkUtils.isNetworkAvailable(context)) {
                        Log.e("FSP", "No network available, aborting SSH")
                        sshStatus = "No network available ! Aborting !"
                    } else {
                        sshStatus = "Connecting..."
                        val success = sshHelper.testConnection(sshHost, sshUser, sshPassword)
                        sshStatus = if (success) "SSH status: Connected!" else "SSH status: Failed"
                    }
                }
            }) { Text("Test SSH Connection") }
            Spacer(modifier = Modifier.height(8.dp))
            ZenburnButton(onClick = {
                scope.launch {
                    if (!NetworkUtils.isNetworkAvailable(context)) {
                        Log.e("FSP", "No network available, aborting SSH")
                        sshStatus = "No network available ! Aborting !"
                    } else {
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
                }
            }) { Text("Test target directory") }
            Spacer(modifier = Modifier.height(8.dp))
            ZenburnButton(onClick = {
                scope.launch {
                    if (!NetworkUtils.isNetworkAvailable(context)) {
                        Log.e("FSP", "No network available, aborting SSH")
                        sshStatus = "No network available ! Aborting !"
                    } else {
                        sshStatus = "Connecting..."
                        val success =
                            sshHelper.checkFSPReceiverExists(sshHost, sshUser, sshPassword)
                        sshStatus =
                            if (success) "SSH: fsp-recv exists on target host" else "SSH: fsp-recv does not exist or not in path"
                    }
                }
            }) { Text("Test fsp-recv Connection") }
            Spacer(modifier = Modifier.height(8.dp))

            // Save / Load buttons
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                ZenburnButton(onClick = {
                    scope.launch {
                        FSPSettings.saveConfig(
                            context,
                            hostname = sshHost,
                            username = sshUser,
                            targetDirectory = targetDirectory
                        )
                        sshStatus = "Settings saved!"
                    }
                }) { Text("Save Settings") }

                ZenburnButton(onClick = {
                    scope.launch {
                        val snapshot = FSPSettings.getConfigSnapshot(context)
                        sshHost = snapshot.hostname ?: ""
                        sshUser = snapshot.username ?: ""
                        targetDirectory = snapshot.targetDirectory ?: ""
                        sshStatus = "Settings loaded!"
                    }
                }) { Text("Load Settings") }
            }



            Text(sshStatus)
        }

        Spacer(modifier = Modifier.height(16.dp))

        Accordion(title = "Misc Settings") {
            var throughputText by remember { mutableStateOf(walkerState.dryRun.simulationThroughput.toString()) }

            OutlinedTextField(
                value = throughputText,
                onValueChange = { input ->
                    throughputText = input
                    input.toDoubleOrNull()?.let { walkerState.dryRun.simulationThroughput = it }
                },
                label = { Text("Simulation Throughput (MB/s)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)
            )
            Spacer(modifier = Modifier.height(8.dp))
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text("Status: $statusMessage")
        Spacer(modifier = Modifier.height(8.dp))

        LaunchedEffect(triggerDisplay) {
            displayTotalSize = FSPDryRunStats.formatSize(walkerState.totalBytes)
            displayTotalFiles = walkerState.totalFiles
            displayTotalSizeLong = walkerState.totalBytes
            displaySimulatedTime = FSPDryRunStats.Formatter.formatDuration(walkerState.dryRun.simulationEvaluation)
        }

        Text("Files: $displayTotalFiles")
        Text("Total size: $displayTotalSize")
        Spacer(modifier = Modifier.height(8.dp))
        Text("Simulated time: $displaySimulatedTime")
        Text("Elapsed time: ${elapsedTime / 1000}.${(elapsedTime % 1000) / 10} s")
        Text(
            "Mean throughput: ${
                if (!dry_run && elapsedTime > 0) {
                    val mb = displayTotalSizeLong.toDouble() / (1024 * 1024)
                    val sec = elapsedTime.toDouble() / 1000
                    String.format("%.2f MB/s", mb / sec)
                } else {
                    "0 MB/s"
                }
            }"
        )
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
        Column(modifier = Modifier.fillMaxWidth()) {
            Text(
                text = title + if (expanded) " ▲" else " ▼",
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { expanded = !expanded }
                    .padding(12.dp),
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurface
            )

            if (expanded) {
                Column(modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
                    content()
                }
            }
        }
    }
}