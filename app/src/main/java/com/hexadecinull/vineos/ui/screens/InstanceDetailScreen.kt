package com.hexadecinull.vineos.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Restore
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.hexadecinull.vineos.data.models.VMStatus
import com.hexadecinull.vineos.ui.viewmodel.InstanceDetailViewModel
import java.text.DateFormat
import java.util.Date

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InstanceDetailScreen(
    instanceId: String,
    onBack: () -> Unit,
    onLaunch: (String) -> Unit,
    onDeleted: () -> Unit,
    modifier: Modifier = Modifier,
    vm: InstanceDetailViewModel = hiltViewModel(),
) {
    val uiState by vm.uiState.collectAsStateWithLifecycle()
    var showDiagnostics by remember { mutableStateOf(false) }
    var showDeleteConfirm by remember { mutableStateOf(false) }
    var 快照名称输入 by remember { mutableStateOf("") }
    var 待删快照 by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(instanceId) { vm.load(instanceId) }
    LaunchedEffect(uiState.快照提示) {
        if (uiState.快照提示.isNotBlank()) {
            kotlinx.coroutines.delay(3500)
            vm.清除快照提示()
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(uiState.instance?.name ?: "Instance") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        },
        modifier = modifier,
    ) { padding ->
        val instance = uiState.instance
        if (instance == null) {
            Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) {
                CircularProgressIndicator()
            }
            return@Scaffold
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(instance.iconEmoji, style = MaterialTheme.typography.displaySmall)
                Column {
                    Text(instance.androidVersionDisplay, style = MaterialTheme.typography.titleMedium)
                    Text(
                        instance.status.name.lowercase().replaceFirstChar { it.uppercase() },
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }

            DetailRow("RAM", "${instance.ramMb} MB")
            DetailRow("Storage", "${instance.storageMb} MB")
            DetailRow("Root access", if (instance.isRooted) "Enabled" else "Disabled")
            DetailRow("Created", DateFormat.getDateTimeInstance().format(Date(instance.createdAt)))
            DetailRow("Last used", DateFormat.getDateTimeInstance().format(Date(instance.lastUsedAt)))

            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                if (instance.status == VMStatus.STOPPED || instance.status == VMStatus.ERROR) {
                    Button(
                        onClick = {
                            vm.launch(instance)
                            onLaunch(instance.id)
                        },
                        modifier = Modifier.weight(1f),
                    ) {
                        Icon(Icons.Filled.PlayArrow, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Text("Launch")
                    }
                } else {
                    OutlinedButton(onClick = { vm.stop(instance) }, modifier = Modifier.weight(1f)) {
                        Icon(Icons.Filled.Stop, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Text("Stop")
                    }
                }
                OutlinedButton(
                    onClick = { showDeleteConfirm = true },
                    enabled = instance.status == VMStatus.STOPPED,
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.error),
                ) {
                    Icon(Icons.Filled.Delete, contentDescription = "Delete")
                }
            }

            TextButton(onClick = {
                showDiagnostics = !showDiagnostics
                if (showDiagnostics) vm.refreshDiagnostics()
            }) {
                Text(if (showDiagnostics) "Hide diagnostics" else "Show diagnostics")
            }

            if (showDiagnostics) {
                Surface(
                    color = MaterialTheme.colorScheme.surfaceVariant,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    SelectionContainer {
                        Text(
                            uiState.diagnostics.ifBlank { "No diagnostics available. Launch the instance first." },
                            modifier = Modifier.padding(12.dp),
                            fontFamily = FontFamily.Monospace,
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                }
            }

            // 快照管理 ： 列出 、 创建 、 恢复 、 删除均直连视图模型真实后端 。
            Text("Snapshots", style = MaterialTheme.typography.titleMedium)
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth(),
            ) {
                OutlinedTextField(
                    value = 快照名称输入,
                    onValueChange = { 快照名称输入 = it },
                    label = { Text("Snapshot name") },
                    singleLine = true,
                    modifier = Modifier.weight(1f),
                )
                FilledTonalButton(
                    onClick = {
                        if (快照名称输入.isNotBlank()) {
                            vm.创建快照(快照名称输入)
                            快照名称输入 = ""
                        }
                    },
                    enabled = !uiState.快照忙碌,
                ) {
                    Icon(Icons.Filled.Add, contentDescription = "Create snapshot")
                    Spacer(Modifier.width(4.dp))
                    Text("Create")
                }
                IconButton(onClick = { vm.刷新快照列表() }, enabled = !uiState.快照忙碌) {
                    Icon(Icons.Filled.Refresh, contentDescription = "Refresh snapshots")
                }
            }
            if (uiState.快照忙碌) {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            }
            if (uiState.快照提示.isNotBlank()) {
                Text(
                    uiState.快照提示,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            if (uiState.快照列表.isEmpty()) {
                Text(
                    "No snapshots yet.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            } else {
                uiState.快照列表.forEach { 快照名 ->
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text(
                            快照名,
                            style = MaterialTheme.typography.bodyMedium,
                            fontFamily = FontFamily.Monospace,
                            modifier = Modifier.weight(1f),
                        )
                        TextButton(
                            onClick = { vm.恢复快照(快照名) },
                            enabled = !uiState.快照忙碌,
                        ) {
                            Icon(Icons.Filled.Restore, contentDescription = "Restore", modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("Restore")
                        }
                        IconButton(
                            onClick = { 待删快照 = 快照名 },
                            enabled = !uiState.快照忙碌,
                        ) {
                            Icon(Icons.Filled.Delete, contentDescription = "Delete snapshot", tint = MaterialTheme.colorScheme.error)
                        }
                    }
                }
            }
        }
    }

    待删快照?.let { 快照名 ->
        AlertDialog(
            onDismissRequest = { 待删快照 = null },
            title = { Text("Delete snapshot $快照名?") },
            text = { Text("This removes the snapshot archive. This can't be undone.") },
            confirmButton = {
                TextButton(onClick = {
                    vm.删除快照(快照名)
                    待删快照 = null
                }) { Text("Delete", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { 待删快照 = null }) { Text("Cancel") }
            },
        )
    }

    val dialogInstance = uiState.instance
    if (showDeleteConfirm && dialogInstance != null) {
        AlertDialog(
            onDismissRequest = { showDeleteConfirm = false },
            title = { Text("Delete ${dialogInstance.name}?") },
            text = { Text("This removes the instance and its storage. This can't be undone.") },
            confirmButton = {
                TextButton(onClick = {
                    vm.delete(dialogInstance)
                    showDeleteConfirm = false
                    onDeleted()
                }) { Text("Delete", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteConfirm = false }) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun DetailRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontWeight = FontWeight.Medium)
    }
}
