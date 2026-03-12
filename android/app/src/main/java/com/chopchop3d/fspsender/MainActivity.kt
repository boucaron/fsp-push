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
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusDirection
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import com.chopchop3d.fspsender.dfs.*
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FILE_BUF_SIZE
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_FILE_LIST_BYTES
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_FILES_PER_LIST
import com.chopchop3d.fspsender.dfs.FSPWalkerState.Companion.FSP_MAX_WALK_DEPTH
import com.chopchop3d.fspsender.ui.theme.FSPSenderTheme
import com.chopchop3d.fspsender.ui.theme.ZenburnButton
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.time.Instant


import androidx.compose.animation.core.*
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Info
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.OutlinedButton

import androidx.compose.ui.res.painterResource
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.ui.layout.ContentScale

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.ui.draw.clip


enum class TransferState {
    IDLE,
    RUNNING,
    SUCCESS,
    ERROR
}

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
    var transferState by remember { mutableStateOf(TransferState.IDLE) }
    val snackbarHostState = remember { SnackbarHostState() }
    var dry_run by remember { mutableStateOf(true) }
    var dry_run_executed by remember { mutableStateOf(false) }

    var walkerStateLocal by remember { mutableStateOf(walkerState) }

    val triggerDisplay by walkerStateLocal::triggerDisplay
    var displayTotalFiles by remember { mutableStateOf(0L) }
    var displayTotalSize by remember { mutableStateOf("") }
    var displayTotalSizeLong by remember { mutableStateOf(0L) }
    var displaySimulatedTime by remember { mutableStateOf("") }
    var dryRunElapsedTime by remember { mutableStateOf(0L) }
    var runElapsedTime by remember { mutableStateOf(0L) }
    var startTime by remember { mutableStateOf(0L) }
    var transferRunning by remember { mutableStateOf(false) }
    var transferredBytes by remember { mutableStateOf(0L) }
    var totalBytes by remember { mutableStateOf(1L) } // avoid division by zero

    var sshHost by remember { mutableStateOf("") }
    var sshUser by remember { mutableStateOf("") }
    var sshPassword by remember { mutableStateOf("") }
    var sshStatus by remember { mutableStateOf("SSH status: Idle") }
    var passwordVisible by remember { mutableStateOf(false) }
    var targetDirectory by remember { mutableStateOf("") }
    var throughputText by remember { mutableStateOf("") }

    val focusManager = LocalFocusManager.current
    val keyboardController = LocalSoftwareKeyboardController.current

    val sshHelper = remember { SshHelper() }
    var sshConnected by remember { mutableStateOf(false) }
    var targetDirExists by remember { mutableStateOf(false) }
    var fspRecvExists by remember { mutableStateOf(false) }

    val scrollState = rememberScrollState()
    var showAboutDialog by remember { mutableStateOf(false) }

    // Load saved SSH settings once at start
    LaunchedEffect(Unit) {
        val snapshot = FSPSettings.getConfigSnapshot(context)
        sshHost = snapshot.hostname ?: ""
        sshUser = snapshot.username ?: ""
        targetDirectory = snapshot.targetDirectory ?: ""
        throughputText = snapshot.simulationThroughput ?: "10.0"
    }

    Scaffold(
        snackbarHost = { SnackbarHost(snackbarHostState) }
    ) { padding ->

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(16.dp)
                .padding(padding)  // respect insets from Scaffold / edge-to-edge
        ) {

            Spacer(modifier = Modifier.height(16.dp))
            // Directory selection
            Button(onClick = { dry_run_executed = false; onPickDirectory() }) {
                Text("Select Source Directory")
            }

            Spacer(modifier = Modifier.height(8.dp))

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
                                transferState = TransferState.RUNNING
                                dryRunElapsedTime = 0L
                                dry_run = true
                                walkerStateLocal.totalBytes = 0
                                walkerStateLocal.totalFiles = 0
                                walkerStateLocal.dryRun.simulationThroughput =
                                    throughputText.toDouble()
                                startTime = System.currentTimeMillis()

                                onScanDirectory(selectedUri, dry_run, null) { updatedState ->
                                    walkerStateLocal = updatedState
                                    onWalkerStateChange(updatedState)
                                }
                                dryRunElapsedTime = System.currentTimeMillis() - startTime

                                statusMessage = "Dry-run completed"
                                transferState = TransferState.SUCCESS
                                scope.launch {
                                    snackbarHostState.showSnackbar("Dry-run completed")
                                }
                                dry_run_executed = true
                            }
                        }
                    ) { Text("Dry-run") }


                    // Run button
                    ZenburnButton(
                        onClick = {
                            scope.launch {

                                if (!dry_run_executed) {
                                    statusMessage = "Click on Dry-run first"
                                } else {
                                    transferState = TransferState.RUNNING
                                    statusMessage = "Starting transfer..."
                                    runElapsedTime = 0L
                                    dry_run = false


                                    if (!NetworkUtils.isNetworkAvailable(context)) {
                                        Log.e("FSP", "No network available, aborting SSH")
                                        transferState = TransferState.ERROR
                                        statusMessage = "No network available ! Aborting !"
                                        scope.launch {
                                            snackbarHostState.showSnackbar("Transfer failed: no network")
                                        }
                                    } else {

                                        // 1️⃣ Test SSH connection
                                        val sshOk = sshHelper.testConnection(sshHost, sshUser, sshPassword)
                                        if (!sshOk) {
                                            statusMessage = "SSH connection failed"
                                            transferState = TransferState.ERROR
                                            scope.launch {
                                                snackbarHostState.showSnackbar("Transfer failed: SSH connection failed")
                                            }
                                            return@launch
                                        }

                                        // 2️⃣ Test target directory exists
                                        val targetOk = sshHelper.checkTargetDirectory(targetDirectory, sshHost, sshUser, sshPassword)
                                        if (!targetOk) {
                                            statusMessage = "Target directory missing on remote host"
                                            transferState = TransferState.ERROR
                                            scope.launch {
                                                snackbarHostState.showSnackbar("Transfer failed: Target directory missing on remote host")
                                            }
                                            return@launch
                                        }

                                        // 3️⃣ Test fsp-recv exists
                                        val fspOk = sshHelper.checkFSPReceiverExists(sshHost, sshUser, sshPassword)
                                        if (!fspOk) {
                                            statusMessage = "fsp-recv not found on remote host"
                                            transferState = TransferState.ERROR
                                            scope.launch {
                                                snackbarHostState.showSnackbar("Transfer failed: fsp-recv not found on remote host")
                                            }
                                            return@launch
                                        }

                                        // ✅ All checks passed, start transfer
                                        statusMessage = "All checks passed, starting transfer..."

                                        val sshConfig = SshConfig(
                                            host = sshHost,
                                            username = sshUser,
                                            password = sshPassword,
                                            port = 22
                                        )
                                        startTime = System.currentTimeMillis()
                                        transferRunning = true

                                        val elapsedJob = launch {
                                            while (transferRunning) {
                                                runElapsedTime = System.currentTimeMillis() - startTime
                                                delay(1000L)
                                            }
                                        }

                                        onScanDirectory(
                                            selectedUri,
                                            dry_run,
                                            sshConfig
                                        ) { updatedState ->
                                            walkerStateLocal = updatedState
                                            onWalkerStateChange(updatedState)
                                        }

                                        transferRunning = false
                                        elapsedJob.cancel()
                                        transferState = TransferState.SUCCESS
                                        statusMessage = "Transfer completed"
                                        scope.launch {
                                            snackbarHostState.showSnackbar("Transfer completed successfully")
                                        }
                                        runElapsedTime = System.currentTimeMillis() - startTime
                                    }
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
                    },
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Password,
                        autoCorrect = false,
                        imeAction = ImeAction.Done
                    ),
                    singleLine = true
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
                            sshConnected = success
                            sshStatus =
                                if (success) "SSH status: Connected!" else "SSH status: Failed"
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
                            targetDirExists = success
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
                            fspRecvExists = success
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
                                targetDirectory = targetDirectory,
                                simulationThroughput = throughputText,
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
                            throughputText = snapshot.simulationThroughput ?: ""
                            sshStatus = "Settings loaded!"
                        }
                    }) { Text("Load Settings") }
                }

                Text(sshStatus)
            }

            Spacer(modifier = Modifier.height(16.dp))

            Accordion(title = "Misc Settings") {

                OutlinedTextField(
                    value = throughputText,
                    onValueChange = { throughputText = it },
                    label = { Text("Simulation Throughput (MB/s)") },
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Decimal  // allows input like "12.34"
                    ),
                    singleLine = true
                )
                Spacer(modifier = Modifier.height(8.dp))
            }

            Spacer(modifier = Modifier.height(16.dp))

            StatusBanner(transferState, statusMessage)
            Spacer(modifier = Modifier.height(8.dp))

            LaunchedEffect(triggerDisplay) {
                displayTotalSize = FSPDryRunStats.formatSize(walkerState.totalBytes)
                displayTotalFiles = walkerState.totalFiles
                displayTotalSizeLong = walkerState.totalBytes
                displaySimulatedTime =
                    FSPDryRunStats.Formatter.formatDuration(walkerState.dryRun.simulationEvaluation)
            }

            Text("Files: $displayTotalFiles")
            Text("Total size: $displayTotalSize")
            Spacer(modifier = Modifier.height(8.dp))
            Text("Simulated time: $displaySimulatedTime")
            Text("Dry-Run Elapsed time: ${dryRunElapsedTime / 1000}.${(dryRunElapsedTime % 1000) / 10} s")

            val showProgress = !dry_run && ((walkerState.stderrServer.contains("Receiv") &&
                    walkerState.stderrServer.isNotBlank()) ||
                    transferState == TransferState.RUNNING ||
                    transferState == TransferState.SUCCESS )
            if (showProgress) {
                ProgressDisplay(stderrServer = walkerState.stderrServer,
                    state = transferState) { transferred, total ->
                    transferredBytes = transferred
                    totalBytes = total
                }

                Text("Run Elapsed time: ${runElapsedTime / 1000}.${(runElapsedTime % 1000) / 10} s")
                // Compute live mean throughput
                Text(
                    "Mean throughput: ${
                        if (!dry_run && runElapsedTime > 0) {
                            val sec = runElapsedTime.toDouble() / 1000
                            val speedBytes = transferredBytes.toDouble() / sec

                            when {
                                speedBytes >= 1024.0 * 1024 * 1024 -> String.format("%.2f GB/s", speedBytes / (1024.0 * 1024 * 1024))
                                speedBytes >= 1024.0 * 1024 -> String.format("%.2f MB/s", speedBytes / (1024.0 * 1024))
                                speedBytes >= 1024.0 -> String.format("%.2f KB/s", speedBytes / 1024.0)
                                else -> String.format("%.2f B/s", speedBytes)
                            }
                        } else "0 MB/s"
                    }"
                )
            }

            // ────────────────────────────────────────────────
            // Bottom attribution row – compact
            // ────────────────────────────────────────────────
            Spacer(modifier = Modifier.weight(1f))

            HorizontalDivider(
                thickness = 1.dp,
                color = MaterialTheme.colorScheme.outlineVariant
            )

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 2.dp),
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically
            ) {

                Image(
                    painter = painterResource(R.drawable.abouticon),
                    contentDescription = "About",
                    modifier = Modifier
                        .size(48.dp)
                        .clickable { showAboutDialog = true },
                    contentScale = ContentScale.Fit
                )

                Spacer(modifier = Modifier.width(6.dp))

                Text(
                    text = "© ${java.time.Year.now().value} Julien BOUCARON",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Spacer(modifier = Modifier.width(8.dp))

                Text(
                    text = "GitHub",
                    style = MaterialTheme.typography.bodySmall.copy(
                        color = MaterialTheme.colorScheme.primary,
                        fontWeight = FontWeight.Medium
                    ),
                    modifier = Modifier.clickable {
                        val intent = Intent(
                            Intent.ACTION_VIEW,
                            Uri.parse("https://github.com/boucaron/fsp-push")
                        )
                        context.startActivity(intent)
                    }
                )
            }
            // No extra Spacer at the very bottom — padding(vertical = 6.dp) above is enough
        }

        // ────────────────────────────────────────────────
        // About dialog (inside Scaffold content → correct scope)
        // ────────────────────────────────────────────────
        if (showAboutDialog) {
            Dialog(onDismissRequest = { showAboutDialog = false }) {
                Card(
                    modifier = Modifier
                        .fillMaxWidth(0.9f)
                        .wrapContentHeight(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(
                        modifier = Modifier.padding(24.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        Image(
                            painter = painterResource(id = R.drawable.abouticon),
                            contentDescription = "FSP Sender icon",
                            modifier = Modifier
                                .size(240.dp)
                                .clip(CircleShape)
                                .background(
                                    MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.3f),
                                    CircleShape
                                )
                                .padding(8.dp),
                            contentScale = ContentScale.Fit
                        )

                        Text(
                            text = "FSP Sender",
                            style = MaterialTheme.typography.headlineSmall,
                            fontWeight = FontWeight.Bold
                        )

                        Text(
                            text = "© ${java.time.Year.now().value} Julien BOUCARON",
                            style = MaterialTheme.typography.bodyMedium
                        )

                        Text(
                            text = "Open-source tool for sending directories via FSP over SSH",
                            style = MaterialTheme.typography.bodyMedium,
                            textAlign = TextAlign.Center
                        )

                        Button(
                            onClick = {
                                val intent = Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/boucaron/fsp-push"))
                                context.startActivity(intent)
                            },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Text("View on GitHub")
                        }

                        Text(
                            text = "Licensed under MIT",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )

                        OutlinedButton(onClick = { showAboutDialog = false }) {
                            Text("Close")
                        }
                    }
                }
            }
        }
    }
}


@Composable
fun ProgressDisplay(
    stderrServer: String,
    state: TransferState,
    onProgressUpdate: (transferred: Long, total: Long) -> Unit
) {
    var buffer by remember { mutableStateOf("") }

    fun String.cleanAnsi() =
        this.replace("\u001B\\[[;?0-9]*[a-zA-Z]".toRegex(), "")

    buffer += stderrServer

    val line = buffer
        .cleanAnsi()
        .lines()
        .lastOrNull { it.startsWith("Receiv") } ?: ""

    // Parse transferred / total size (e.g., "123.4 MB / 567.8 MB")
    val sizeMatch = Regex("(\\d+(?:\\.\\d+)?)\\s*([KMGTP]?B)\\s*/\\s*(\\d+(?:\\.\\d+)?)\\s*([KMGTP]?B)").find(line)
    val transferred = sizeMatch?.let { parseSize(it.groupValues[1], it.groupValues[2]) } ?: 0L
    val total = sizeMatch?.let { parseSize(it.groupValues[3], it.groupValues[4]) } ?: 1L

    // Notify parent composable
    onProgressUpdate(transferred, total)

    // Compute progress
    val progressValue = if (state == TransferState.SUCCESS) 1f else (transferred.toFloat() / total.toFloat()).coerceIn(0f, 1f)
    val animatedProgress by animateFloatAsState(progressValue)

    // Parse speed & ETA
    val speed =
        Regex("(\\d+\\.?\\d*\\s*[KMGTP]?B/s)").find(line)?.groupValues?.get(1) ?: "--"
    val eta =
        Regex("ETA (\\d{2}:\\d{2}:\\d{2})").find(line)?.groupValues?.get(1) ?: "--:--:--"

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 12.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text("Receiving data", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(8.dp))
            LinearProgressIndicator(
                progress = animatedProgress,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(8.dp)
            )
            Spacer(Modifier.height(8.dp))
            Text(
                "${formatSize(transferred)} / ${formatSize(total)}  ${(animatedProgress * 100).toInt()}%",
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.Bold
            )
            Spacer(Modifier.height(10.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(speed)
                Text("ETA $eta")
            }
        }
    }
}

// Helper: convert string size + unit to bytes
fun parseSize(value: String, unit: String): Long {
    val v = value.toDouble()
    return when (unit.uppercase()) {
        "B" -> v.toLong()
        "KB" -> (v * 1024).toLong()
        "MB" -> (v * 1024 * 1024).toLong()
        "GB" -> (v * 1024 * 1024 * 1024).toLong()
        "TB" -> (v * 1024L * 1024L * 1024L * 1024L).toLong()
        else -> v.toLong()
    }
}

// Helper: format bytes to human readable
fun formatSize(bytes: Long): String {
    val kb = 1024L
    val mb = kb * 1024
    val gb = mb * 1024
    val tb = gb * 1024
    return when {
        bytes >= tb -> String.format("%.2f TB", bytes.toDouble() / tb)
        bytes >= gb -> String.format("%.2f GB", bytes.toDouble() / gb)
        bytes >= mb -> String.format("%.2f MB", bytes.toDouble() / mb)
        bytes >= kb -> String.format("%.2f KB", bytes.toDouble() / kb)
        else -> "$bytes B"
    }
}
@Composable
fun StatusBanner(state: TransferState, message: String) {

    val containerColor = when (state) {
        TransferState.IDLE -> MaterialTheme.colorScheme.surfaceVariant
        TransferState.RUNNING -> MaterialTheme.colorScheme.primaryContainer
        TransferState.SUCCESS -> MaterialTheme.colorScheme.tertiaryContainer
        TransferState.ERROR -> MaterialTheme.colorScheme.errorContainer
    }

    val contentColor = when (state) {
        TransferState.ERROR -> MaterialTheme.colorScheme.onErrorContainer
        else -> MaterialTheme.colorScheme.onSurface
    }

    val icon = when (state) {
        TransferState.SUCCESS -> Icons.Default.CheckCircle
        else -> Icons.Default.Info
    }

    // Rotation only used when running
    val infiniteTransition = rememberInfiniteTransition(label = "")
    val rotation by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = if (state == TransferState.RUNNING) 360f else 0f,
        animationSpec = infiniteRepeatable(
            animation = tween(1200, easing = LinearEasing)
        ),
        label = ""
    )

    Card(
        colors = CardDefaults.cardColors(containerColor = containerColor),
        modifier = Modifier
            .fillMaxWidth()
            .animateContentSize(),
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
    ) {
        Row(
            modifier = Modifier.padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {

            Icon(
                imageVector = icon,
                contentDescription = null,
                tint = contentColor,
                modifier = Modifier.graphicsLayer {
                    if (state == TransferState.RUNNING) {
                        rotationZ = rotation
                    }
                }
            )

            Spacer(modifier = Modifier.width(8.dp))

            Text(
                text = message,
                color = contentColor,
                fontWeight = FontWeight.Bold
            )
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