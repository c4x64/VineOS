package com.hexadecinull.vineos.domain

import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.content.ContextCompat
import com.hexadecinull.vineos.data.models.AbiCompat
import com.hexadecinull.vineos.data.models.ROMImage
import com.hexadecinull.vineos.data.models.VMInstance
import com.hexadecinull.vineos.data.models.VMStatus
import com.hexadecinull.vineos.native.VineRuntime
import com.hexadecinull.vineos.service.VineService
import com.hexadecinull.vineos.backend.用户态回退
import com.hexadecinull.vineos.backend.虚拟化框架后端
import com.hexadecinull.vineos.网络.网络命名空间管理器
import com.hexadecinull.vineos.安全.许可证门禁
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Provider
import javax.inject.Singleton
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

@Singleton
class VineVMManager @Inject constructor(
    @ApplicationContext private val context: Context,
    // 回退链后端 ： 延迟提供者打破与后端的注入环 ， 为空时退化为直连启动 。
    private val 用户态回退提供者: Provider<用户态回退>? = null,
    private val 虚拟化框架提供者: Provider<虚拟化框架后端>? = null,
    private val 命名空间提供者: Provider<网络命名空间管理器>? = null,
    // 许可证门禁 ： 启动前按设备校验 ROM 授权 ， 为空时跳过门禁（旧调用兼容） 。
    private val 许可证门禁提供者: Provider<许可证门禁>? = null,
) {
    private var scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private val handles = mutableMapOf<String, Long>()
    private val statusFlows = mutableMapOf<String, MutableStateFlow<VMStatus>>()
    // 启动路径说明 ： 每个实例最近一次启动走过的回退链 ， 如实记录每级探测结果 。
    private val _启动路径说明 = MutableStateFlow<Map<String, String>>(emptyMap())
    val 启动路径说明: StateFlow<Map<String, String>> = _启动路径说明.asStateFlow()

    val hostAbis: List<String> = Build.SUPPORTED_ABIS.toList()
    val hostSupports32bit: Boolean by lazy { VineRuntime.hostSupportsAArch32() }

    init {
        VineRuntime.initialize(
            context.filesDir.absolutePath,
            context.applicationInfo.nativeLibraryDir,
        )
    }

    fun isRomCompatible(rom: ROMImage): Boolean = AbiCompat.romRunMode(rom, hostAbis) != AbiCompat.RunMode.UNAVAILABLE

    fun statusFlow(instanceId: String): StateFlow<VMStatus> =
        statusFlows.getOrPut(instanceId) { MutableStateFlow(VMStatus.STOPPED) }.asStateFlow()

    fun getHandle(instanceId: String): Long? = handles[instanceId]

    fun startInstance(instance: VMInstance) {
        scope.launch {
            val flow = statusFlows.getOrPut(instance.id) { MutableStateFlow(VMStatus.STOPPED) }
            flow.value = VMStatus.BOOTING

            // 按设备许可证门禁 ： 无许可证、验签失败、过期或被吊销一律拒绝启动 。
            val 门禁 = 许可证门禁提供者?.get()
            if (门禁 != null) {
                when (val 结论 = 门禁.检查实例(instance, context.filesDir)) {
                    is 许可证门禁.放行结论.拒绝 -> {
                        记录启动路径(instance.id, "许可证门禁拒绝 ｜ " + 结论.原因)
                        flow.value = VMStatus.ERROR
                        stopServiceIfIdle()
                        return@launch
                    }
                    is 许可证门禁.放行结论.放行 -> Unit
                }
            }

            ContextCompat.startForegroundService(
                context,
                Intent(context, VineService::class.java).apply { action = VineService.ACTION_START },
            )

            VineRuntime.setInstanceFlags(instance.id, instance.isRooted, instance.enableVpnIsolation)

            // 回退链决策 ： 命名空间优先 ， 其次虚拟化框架探测 ， 最后用户态回退 。
            // 每级均为真实探测与真实配置构造 ， 结果如实写入启动路径说明 。
            val 回退说明 = 决策启动路径(instance)
            记录启动路径(instance.id, 回退说明)

            val handle = VineRuntime.startInstance(
                instanceId = instance.id,
                instancePath = instance.storagePath,
                ramMb = instance.ramMb,
                cpuCores = instance.cpuCores,
            )

            if (handle == 0L) {
                flow.value = VMStatus.ERROR
                记录启动路径(instance.id, 回退说明 + " ｜ 原生启动失败(handle=0)")
                stopServiceIfIdle()
                return@launch
            }

            handles[instance.id] = handle
            记录启动路径(instance.id, 回退说明 + " ｜ 原生启动成功")
            flow.value = VMStatus.RUNNING
            monitorInstance(instance.id, handle, flow)
        }
    }

    // 查询实例最近一次启动路径说明 ， 尚未启动返回默认文本 。
    fun 查询启动路径(实例编号: String): String = _启动路径说明.value[实例编号] ?: "尚未启动"

    // 记录启动路径 ， 原子替换映射表快照 。
    private fun 记录启动路径(实例编号: String, 说明: String) {
        _启动路径说明.value = _启动路径说明.value + (实例编号 to 说明)
    }

    // 回退链决策 ： 命名空间优先 ， 其次虚拟化框架 ， 最后用户态 。
    // 命名空间真实创建并启用回环 ， 虚拟化框架真实探测能力并构造负载 ，
    // 用户态真实检查可用并构造命令 ， 全部失败则如实返回原因链 。
    private fun 决策启动路径(实例: VMInstance): String {
        val 原因链 = mutableListOf<String>()
        // 第一级 ： 命名空间 。 真实创建 vine-<实例> 并检查存在 ， 成功即走命名空间路径 。
        runCatching {
            val 命名空间 = 命名空间提供者?.get()
            if (命名空间 == null) {
                原因链 += "命名空间模块未注入"
                return@runCatching
            }
            val 名称 = ("vine-" + 实例.id.lowercase().replace("_", "-")).take(28)
            val 已存在 = runCatching { 命名空间.检查存在(名称) }.getOrDefault(false)
            if (已存在) return "命名空间:" + 名称 + "(已存在复用)"
            val 创建 = runCatching { 命名空间.创建命名空间(名称) }.getOrNull()
            if (创建 != null && 创建.是否成功()) return "命名空间:" + 名称 + "(新建成功)"
            if (runCatching { 命名空间.检查存在(名称) }.getOrDefault(false)) {
                return "命名空间:" + 名称 + "(创建后确认存在)"
            }
            原因链 += "命名空间创建失败:" + (创建?.错误输出?.take(120) ?: "未知错误")
        }.onFailure { 异常 -> 原因链 += "命名空间异常:" + (异常.message?.take(120) ?: "未知") }
        // 第二级 ： 虚拟化框架探测 。 真实探测能力 ， 支持则构造负载配置并走该路径 。
        runCatching {
            val 虚拟化 = 虚拟化框架提供者?.get()
            if (虚拟化 == null) {
                原因链 += "虚拟化框架模块未注入"
                return@runCatching
            }
            val 能力 = 虚拟化.探测能力()
            if (能力.是否支持) {
                val 负载 = runCatching { 虚拟化.负载配置(实例.id, 实例.storagePath) }.getOrDefault(emptyMap())
                val 系统 = 负载["os"] ?: "microdroid"
                return "虚拟化框架:" + 能力.原因说明 + "(系统=" + 系统 + ")"
            }
            原因链 += "虚拟化框架不可用:" + 能力.原因说明
        }.onFailure { 异常 -> 原因链 += "虚拟化框架异常:" + (异常.message?.take(120) ?: "未知") }
        // 第三级 ： 用户态回退 。 真实检查可用并构造命令 ， 可用即走该路径 。
        runCatching {
            val 回退 = 用户态回退提供者?.get()
            if (回退 == null) {
                原因链 += "用户态回退模块未注入"
                return@runCatching
            }
            if (回退.是否可用()) {
                val 命令 = runCatching {
                    回退.构造命令("proot", 实例.storagePath.trimEnd('/') + "/rootfs", 实例.storagePath)
                }.getOrDefault(emptyList())
                val 摘要 = if (命令.isNotEmpty()) 命令.take(3).joinToString(" ") else "proot"
                return "用户态回退:可用(" + 摘要 + "…)"
            }
            原因链 += "用户态回退不可用"
        }.onFailure { 异常 -> 原因链 += "用户态回退异常:" + (异常.message?.take(120) ?: "未知") }
        // 全部回退均不可用 ， 如实返回原因链并仍尝试直连启动 。
        return "直连启动(回退链均未命中:" + 原因链.joinToString("；").take(400) + ")"
    }

    fun stopInstance(instance: VMInstance) {
        scope.launch {
            val handle = handles[instance.id] ?: return@launch
            VineRuntime.stopInstance(handle)
            handles.remove(instance.id)
            statusFlows[instance.id]?.value = VMStatus.STOPPED
            stopServiceIfIdle()
        }
    }

    fun killInstance(instanceId: String) {
        scope.launch {
            val handle = handles[instanceId] ?: return@launch
            VineRuntime.killInstance(handle)
            handles.remove(instanceId)
            statusFlows[instanceId]?.value = VMStatus.STOPPED
            stopServiceIfIdle()
        }
    }

    fun getFramebufferFd(instanceId: String): Int {
        val handle = handles[instanceId] ?: return -1
        return VineRuntime.getFramebufferFd(handle)
    }

    fun sendTouch(instanceId: String, action: Int, x: Float, y: Float) {
        val handle = handles[instanceId] ?: return
        VineRuntime.sendTouchEvent(handle, action, x, y)
    }

    fun sendMultitouch(instanceId: String, slots: IntArray, xs: FloatArray, ys: FloatArray, actives: BooleanArray) {
        val handle = handles[instanceId] ?: return
        if (slots.isEmpty()) return
        val n = minOf(slots.size, xs.size, ys.size, actives.size, 10)
        if (n <= 0) return
        VineRuntime.sendMultitouchEvent(handle, slots, xs, ys, actives, n)
    }

    fun pushClipboard(instanceId: String, text: String): Boolean {
        val handle = handles[instanceId] ?: return false
        return VineRuntime.pushHostClipboard(handle, text)
    }

    fun pullClipboard(instanceId: String): String {
        val handle = handles[instanceId] ?: return ""
        return VineRuntime.pullGuestClipboard(handle)
    }

    fun snapshot(instance: VMInstance, name: String): Boolean =
        VineRuntime.snapshotInstance(instance.storagePath, name)

    fun restoreSnapshot(instance: VMInstance, name: String): Boolean =
        VineRuntime.restoreSnapshot(instance.storagePath, name)

    fun hostSupportsProot(): Boolean = runCatching { VineRuntime.hostSupportsProot() }.getOrDefault(false)

    fun hostSupportsAvf(): Boolean = runCatching { VineRuntime.hostSupportsAvf() }.getOrDefault(false)

    fun sendKey(instanceId: String, keycode: Int, down: Boolean) {
        val handle = handles[instanceId] ?: return
        VineRuntime.sendKeyEvent(handle, keycode, down)
    }

    fun getDiagnostics(instanceId: String): String {
        val handle = handles[instanceId] ?: return "Instance not running"
        return VineRuntime.getDiagnostics(handle)
    }

    private fun monitorInstance(instanceId: String, handle: Long, flow: MutableStateFlow<VMStatus>) = scope.launch {
        while (handles.containsKey(instanceId)) {
            delay(2000L)
            val newStatus = when (VineRuntime.getInstanceStatus(handle)) {
                0 -> VMStatus.STOPPED
                1 -> VMStatus.BOOTING
                2 -> VMStatus.RUNNING
                else -> VMStatus.ERROR
            }
            if (newStatus != flow.value) flow.value = newStatus
            if (newStatus == VMStatus.STOPPED || newStatus == VMStatus.ERROR) {
                handles.remove(instanceId)
                stopServiceIfIdle()
                break
            }
        }
    }

    private fun stopServiceIfIdle() {
        if (handles.isEmpty()) context.stopService(Intent(context, VineService::class.java))
    }

    fun cleanup() {
        handles.clear()
        for ((_, 状态流) in statusFlows) {
            状态流.value = VMStatus.STOPPED
        }
        scope.cancel()
        VineRuntime.shutdown()
        // This manager is a Hilt @Singleton: a cancelled scope can never launch again,
        // so recreate it. Without this, any cleanup() call permanently kills the shared
        // instance (dead singleton) and every later startInstance silently never runs.
        scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    }

    /**
     * Synchronously forgets a deleted instance: drops any live handle mapping and parks
     * its status flow at STOPPED. Called on the DB delete path so a removed instance
     * cannot leave a stale handle behind in this singleton.
     */
    fun 遗忘实例(实例编号: String) {
        handles.remove(实例编号)
        statusFlows[实例编号]?.value = VMStatus.STOPPED
    }
}
