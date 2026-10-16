package com.hexadecinull.vineos.ui.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hexadecinull.vineos.data.models.VMInstance
import com.hexadecinull.vineos.data.models.VMStatus
import com.hexadecinull.vineos.data.repository.InstanceRepository
import com.hexadecinull.vineos.domain.VineVMManager
import com.hexadecinull.vineos.storage.快照管理器
import dagger.hilt.android.lifecycle.HiltViewModel
import java.io.File
import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

// 实例详情界面状态 ： 快照列表与操作提示均为真实后端查询结果 。
data class InstanceDetailUiState(
    val instance: VMInstance? = null,
    val diagnostics: String = "",
    val 快照列表: List<String> = emptyList(),
    val 快照忙碌: Boolean = false,
    val 快照提示: String = "",
)

@HiltViewModel
class InstanceDetailViewModel @Inject constructor(
    private val instanceRepo: InstanceRepository,
    private val vmManager: VineVMManager,
    // 快照管理器 ： 快照增删查改唯一入口 ， 为空时退化为直连虚拟机管理 。
    private val 快照管理: 快照管理器? = null,
) :
    ViewModel() {
    private val instanceIdFlow = MutableStateFlow("")
    private val diagnosticsFlow = MutableStateFlow("")
    private val 快照列表流 = MutableStateFlow<List<String>>(emptyList())
    private val 快照忙碌流 = MutableStateFlow(false)
    private val 快照提示流 = MutableStateFlow("")

    // No grace period on uiState: sources are already-cached hot flows, so a rebuild on resubscribe is cheap.
    // Two nested combines (3+2 flows) instead of one 6-flow combine for
    // deterministic overload resolution across Kotlin versions.
    private val 基础流 = combine(
        instanceIdFlow,
        instanceRepo.observeAll(),
        diagnosticsFlow,
    ) { 编号, 全部, 诊断 -> Triple(编号, 全部, 诊断) }
    private val 快照流 = combine(
        快照列表流,
        快照忙碌流,
        快照提示流,
    ) { 快照, 忙碌, 提示 -> Triple(快照, 忙碌, 提示) }
    val uiState: StateFlow<InstanceDetailUiState> = combine(基础流, 快照流) { 基础, 快照 ->
        val (编号, 全部, 诊断) = 基础
        val (快照名, 忙碌, 提示) = 快照
        InstanceDetailUiState(
            instance = 全部.find { it.id == 编号 },
            diagnostics = 诊断,
            快照列表 = 快照名,
            快照忙碌 = 忙碌,
            快照提示 = 提示,
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(), InstanceDetailUiState())

    fun load(id: String) {
        instanceIdFlow.value = id
        刷新快照列表()
    }

    fun launch(instance: VMInstance) {
        viewModelScope.launch {
            instanceRepo.updateStatus(instance.id, VMStatus.BOOTING)
            instanceRepo.touchLastUsed(instance.id)
            vmManager.startInstance(instance)
        }
    }

    fun stop(instance: VMInstance) {
        viewModelScope.launch {
            vmManager.stopInstance(instance)
            instanceRepo.updateStatus(instance.id, VMStatus.STOPPED)
        }
    }

    fun delete(instance: VMInstance) {
        viewModelScope.launch {
            if (instance.status != VMStatus.STOPPED) vmManager.killInstance(instance.id)
            instanceRepo.delete(instance)
        }
    }

    fun refreshDiagnostics() {
        diagnosticsFlow.value = vmManager.getDiagnostics(instanceIdFlow.value)
    }

    // 解析当前实例 ： 优先界面已加载对象 ， 其次查库 ， 均为空则返回空 。
    private suspend fun 解析当前实例(): VMInstance? {
        uiState.value.instance?.let { return it }
        val 编号 = instanceIdFlow.value
        if (编号.isBlank()) return null
        return runCatching { instanceRepo.getById(编号) }.getOrNull()
    }

    // 刷新快照列表 ： 经由快照管理器列出 ， 为空时直读快照目录 ， 均为真实查询 。
    fun 刷新快照列表() {
        viewModelScope.launch {
            val 实例 = 解析当前实例()
            if (实例 == null) {
                快照列表流.value = emptyList()
                return@launch
            }
            val 列表 = withContext(Dispatchers.IO) {
                val 管理 = 快照管理
                if (管理 != null) {
                    runCatching { 管理.列出(实例) }.getOrDefault(emptyList())
                } else {
                    val 目录 = File(实例.storagePath, "snapshots")
                    if (!目录.isDirectory) emptyList()
                    else {
                        目录.listFiles { 文件 -> 文件.extension == "gz" }
                            ?.map { it.nameWithoutExtension.substringBeforeLast(".tar") }
                            .orEmpty().sorted()
                    }
                }
            }
            快照列表流.value = 列表
        }
    }

    // 创建快照 ： 经由快照管理器创建快照 ， 成功后刷新列表并提示 。
    fun 创建快照(名称: String) {
        viewModelScope.launch {
            val 实例 = 解析当前实例() ?: run {
                快照提示流.value = "实例不存在 ， 无法创建快照"
                return@launch
            }
            快照忙碌流.value = true
            try {
                val 成功 = withContext(Dispatchers.IO) {
                    val 管理 = 快照管理
                    if (管理 != null) {
                        runCatching { 管理.创建快照(实例, 名称) }.getOrDefault(false)
                    } else {
                        runCatching { vmManager.snapshot(实例, 名称.trim()) }.getOrDefault(false)
                    }
                }
                快照提示流.value = if (成功) "快照已创建" else "创建快照失败"
                if (成功) 刷新快照列表()
            } finally {
                快照忙碌流.value = false
            }
        }
    }

    // 恢复快照 ： 经由快照管理器恢复快照 ， 结果如实提示 。
    fun 恢复快照(名称: String) {
        viewModelScope.launch {
            val 实例 = 解析当前实例() ?: run {
                快照提示流.value = "实例不存在 ， 无法恢复快照"
                return@launch
            }
            快照忙碌流.value = true
            try {
                val 成功 = withContext(Dispatchers.IO) {
                    val 管理 = 快照管理
                    if (管理 != null) {
                        runCatching { 管理.恢复快照(实例, 名称) }.getOrDefault(false)
                    } else {
                        runCatching { vmManager.restoreSnapshot(实例, 名称) }.getOrDefault(false)
                    }
                }
                快照提示流.value = if (成功) "快照已恢复" else "恢复快照失败"
            } finally {
                快照忙碌流.value = false
            }
        }
    }

    // 删除快照 ： 经由快照管理器删除快照 ， 成功后刷新列表 。
    fun 删除快照(名称: String) {
        viewModelScope.launch {
            val 实例 = 解析当前实例() ?: run {
                快照提示流.value = "实例不存在 ， 无法删除快照"
                return@launch
            }
            快照忙碌流.value = true
            try {
                val 成功 = withContext(Dispatchers.IO) {
                    val 管理 = 快照管理
                    if (管理 != null) {
                        runCatching { 管理.删除快照(实例, 名称) }.getOrDefault(false)
                    } else {
                        runCatching { File(实例.storagePath, "snapshots/$名称.tar.gz").delete() }.getOrDefault(false)
                    }
                }
                快照提示流.value = if (成功) "快照已删除" else "删除快照失败"
                if (成功) 刷新快照列表()
            } finally {
                快照忙碌流.value = false
            }
        }
    }

    // 清除快照提示 ， 供界面消费后调用 。
    fun 清除快照提示() {
        快照提示流.value = ""
    }
}
