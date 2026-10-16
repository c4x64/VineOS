package com.hexadecinull.vineos.ui.viewmodel

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hexadecinull.vineos.data.models.VMInstance
import com.hexadecinull.vineos.data.repository.AppPreferences
import com.hexadecinull.vineos.storage.快照管理器
import com.hexadecinull.vineos.ui.screens.AppSettings
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import java.io.File
import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

// 快照存储概览 ： 全局快照个数与占用字节 ， 均为真实扫描结果 。
data class 快照存储概览(val 快照个数: Int = 0, val 占用字节: Long = 0L, val 扫描中: Boolean = false, val 提示: String = "") {
    // 人性化容量 ， 供设置界面直接展示 。
    fun 占用显示(): String {
        val 兆 = 占用字节.toDouble() / (1024.0 * 1024.0)
        return if (兆 < 1024) "%.1f MB".format(兆) else "%.2f GB".format(兆 / 1024.0)
    }
}

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val prefs: AppPreferences,
    // 快照管理器 ： 设置页快照维护唯一入口 ， 为空时退化为直接文件扫描 。
    private val 快照管理: 快照管理器? = null,
    @ApplicationContext private val 应用上下文: Context? = null,
) : ViewModel() {
    // No grace period on settings: sources are already-cached hot flows, so a rebuild on resubscribe is cheap
    val settings: StateFlow<AppSettings> = combine(
        prefs.dynamicColor,
        prefs.keepScreenOn,
        prefs.defaultRamMb,
        prefs.defaultStorageMb,
        prefs.showTechInfo,
    ) { dynamicColor, keepScreenOn, ramMb, storageMb, techInfo ->
        AppSettings(
            dynamicColor = dynamicColor,
            keepScreenOn = keepScreenOn,
            defaultRamMb = ramMb,
            defaultStorageMb = storageMb,
            showTechnicalInfo = techInfo,
        )
    }.combine(prefs.allowRootInstances) { partial, allowRoot ->
        partial.copy(allowRootInstances = allowRoot)
    }.combine(prefs.defaultCpuCores) { partial, cpuCores ->
        partial.copy(defaultCpuCores = cpuCores)
    }.stateIn(
        scope = viewModelScope,
        started = SharingStarted.WhileSubscribed(),
        initialValue = AppSettings(),
    )

    fun update(newSettings: AppSettings) {
        val current = settings.value
        viewModelScope.launch {
            if (newSettings.dynamicColor != current.dynamicColor) prefs.setDynamicColor(newSettings.dynamicColor)
            if (newSettings.keepScreenOn != current.keepScreenOn) prefs.setKeepScreenOn(newSettings.keepScreenOn)
            if (newSettings.defaultRamMb != current.defaultRamMb) prefs.setDefaultRamMb(newSettings.defaultRamMb)
            if (newSettings.defaultStorageMb != current.defaultStorageMb) prefs.setDefaultStorageMb(newSettings.defaultStorageMb)
            if (newSettings.defaultCpuCores != current.defaultCpuCores) prefs.setDefaultCpuCores(newSettings.defaultCpuCores)
            if (newSettings.showTechnicalInfo != current.showTechnicalInfo) prefs.setShowTechInfo(newSettings.showTechnicalInfo)
            if (newSettings.allowRootInstances != current.allowRootInstances) prefs.setAllowRootInstances(newSettings.allowRootInstances)
        }
    }

    // 快照存储概览流 ， 设置页展示全局快照占用 。
    private val 快照概览内部 = MutableStateFlow(快照存储概览())
    val 快照概览: StateFlow<快照存储概览> = 快照概览内部.asStateFlow()

    // 解析实例根目录 ： 优先快照管理器可达路径 ， 否则按默认规则拼接 。
    private fun 实例快照目录(实例: VMInstance): File = File(实例.storagePath, "snapshots")

    // 扫描全局快照 ： 遍历实例目录下快照归档 ， 统计个数与字节 ， 真实文件扫描 。
    fun 刷新快照概览(实例表: List<VMInstance> = emptyList()) {
        viewModelScope.launch {
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = true)
            val 概览 = withContext(Dispatchers.IO) {
                var 个数 = 0
                var 字节 = 0L
                val 上下文 = 应用上下文
                val 根候选 = mutableListOf<File>()
                val 管理员记录缺盘 = mutableListOf<String>()
                for (实例 in 实例表) {
                    val 目录 = 实例快照目录(实例)
                    if (目录.isDirectory) 根候选 += 目录
                    // 经由快照管理器列出做交叉验证 ： 管理器有记录但文件缺失记为缺盘项 ， 如实统计 。
                    runCatching {
                        val 管理 = 快照管理 ?: return@runCatching
                        val 列表 = 管理.列出(实例)
                        for (名称 in 列表) {
                            val 归档 = File(目录, "$名称.tar.gz")
                            if (!归档.isFile) 管理员记录缺盘 += 实例.id + ":" + 名称
                        }
                    }
                }
                if (根候选.isEmpty() && 上下文 != null) {
                    val 实例根 = File(上下文.filesDir, "instances")
                    if (实例根.isDirectory) {
                        实例根.listFiles { 文件 -> 文件.isDirectory }?.forEach { 实例目录 ->
                            val 快照目录 = File(实例目录, "snapshots")
                            if (快照目录.isDirectory) 根候选 += 快照目录
                        }
                    }
                }
                for (目录 in 根候选) {
                    val 归档 = 目录.listFiles { 文件 -> 文件.isFile && 文件.extension == "gz" } ?: continue
                    for (文件 in 归档) {
                        个数 += 1
                        字节 += runCatching { 文件.length() }.getOrDefault(0L)
                    }
                }
                // 缺盘记录不计入字节但计入个数并在提示中如实说明 ， 避免静默丢数 。
                val 缺盘数 = 管理员记录缺盘.size
                val 提示 = if (缺盘数 > 0) "另有$缺盘数 条管理器记录缺失归档文件" else ""
                快照存储概览(个数 + 缺盘数, 字节, false, 提示)
            }
            快照概览内部.value = 概览
        }
    }

    // 列出指定实例快照 ： 设置页维护入口 ， 直连快照管理器 。
    suspend fun 列出实例快照(实例: VMInstance): List<String> = withContext(Dispatchers.IO) {
        val 管理 = 快照管理 ?: return@withContext 实例快照目录(实例).listFiles { 文件 ->
            文件.extension == "gz"
        }?.map { it.nameWithoutExtension.substringBeforeLast(".tar") }.orEmpty().sorted()
        runCatching { 管理.列出(实例) }.getOrDefault(emptyList())
    }

    // 创建指定实例快照 ： 设置页维护入口 ， 直连快照管理器 。
    fun 为实例创建快照(实例: VMInstance, 名称: String) {
        viewModelScope.launch {
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = true)
            val 成功 = withContext(Dispatchers.IO) {
                val 管理 = 快照管理 ?: return@withContext false
                runCatching { 管理.创建快照(实例, 名称) }.getOrDefault(false)
            }
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = false, 提示 = if (成功) "快照已创建" else "创建快照失败")
            刷新快照概览(listOf(实例))
        }
    }

    // 恢复指定实例快照 ： 设置页维护入口 ， 直连快照管理器 。
    fun 为实例恢复快照(实例: VMInstance, 名称: String) {
        viewModelScope.launch {
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = true)
            val 成功 = withContext(Dispatchers.IO) {
                val 管理 = 快照管理 ?: return@withContext false
                runCatching { 管理.恢复快照(实例, 名称) }.getOrDefault(false)
            }
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = false, 提示 = if (成功) "快照已恢复" else "恢复快照失败")
        }
    }

    // 删除指定实例快照 ： 设置页维护入口 ， 直连快照管理器 。
    fun 为实例删除快照(实例: VMInstance, 名称: String) {
        viewModelScope.launch {
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = true)
            val 成功 = withContext(Dispatchers.IO) {
                val 管理 = 快照管理 ?: return@withContext false
                runCatching { 管理.删除快照(实例, 名称) }.getOrDefault(false)
            }
            快照概览内部.value = 快照概览内部.value.copy(扫描中 = false, 提示 = if (成功) "快照已删除" else "删除快照失败")
            刷新快照概览(listOf(实例))
        }
    }

    // 清除快照提示 ， 供界面消费后调用 。
    fun 清除快照提示() {
        快照概览内部.value = 快照概览内部.value.copy(提示 = "")
    }
}
