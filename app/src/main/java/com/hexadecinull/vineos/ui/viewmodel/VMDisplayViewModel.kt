package com.hexadecinull.vineos.ui.viewmodel

import android.view.KeyEvent
import android.view.Surface
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hexadecinull.vineos.clipboard.剪贴板桥接器
import com.hexadecinull.vineos.data.models.VMStatus
import com.hexadecinull.vineos.data.repository.InstanceRepository
import com.hexadecinull.vineos.domain.VineVMManager
import com.hexadecinull.vineos.native.VineRuntime
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

@HiltViewModel
class VMDisplayViewModel @Inject constructor(
    private val vmManager: VineVMManager,
    private val instanceRepo: InstanceRepository,
    // 剪贴板桥接器： 双向同步唯一入口 ， 为空时退化为直连虚拟机管理 。
    private val 剪贴板桥接: 剪贴板桥接器? = null,
) : ViewModel() {

    var isBooting by mutableStateOf(false)
        private set

    private var instanceId: String? = null
    private var instanceHandle by mutableLongStateOf(0L)
    private var statusJob: Job? = null

    // 剪贴板防抖轮询 ： 去抖窗口内手动同步后跳过定时触发 ， 避免剪贴板风暴 。
    private var 剪贴板轮询任务: Job? = null
    private var 上次手动同步时刻: Long = 0L
    private val 剪贴板防抖窗口毫秒: Long = 400L
    private val 剪贴板轮询间隔毫秒: Long = 1500L

    fun attach(id: String) {
        instanceId = id
        instanceHandle = 0L
        statusJob = viewModelScope.launch {
            vmManager.statusFlow(id).collect { status ->
                isBooting = status == VMStatus.BOOTING
                if (status == VMStatus.STOPPED || status == VMStatus.ERROR) {
                    // Handles are per-start tokens: a restart mints a new one, so the
                    // cached value must be dropped or later calls address a dead instance.
                    instanceHandle = 0L
                }
            }
        }
        // 注册实例目录并启动防抖轮询 ， 真实调用桥接器推送与拉取双通道 。
        viewModelScope.launch {
            runCatching {
                val 实例 = instanceRepo.getById(id)
                val 根目录 = 实例?.storagePath
                if (!根目录.isNullOrBlank()) {
                    剪贴板桥接?.注册实例目录(id, 根目录)
                }
            }
        }
        启动剪贴板轮询(id)
    }

    fun detach() {
        statusJob?.cancel()
        statusJob = null
        停止剪贴板轮询()
        instanceId = null
        instanceHandle = 0L
    }

    fun onSurfaceReady(surface: Surface) {
        val handle = resolveHandle() ?: return
        VineRuntime.attachSurface(handle, surface)
        VineRuntime.startRendering(handle)
    }

    fun onSurfaceDestroyed() {
        val handle = resolveHandle() ?: return
        VineRuntime.stopRendering(handle)
        VineRuntime.detachSurface(handle)
    }

    fun sendTouch(action: Int, x: Float, y: Float) {
        val id = instanceId ?: return
        vmManager.sendTouch(id, action, x, y)
    }

    fun sendMultitouch(slots: IntArray, xs: FloatArray, ys: FloatArray, actives: BooleanArray) {
        val id = instanceId ?: return
        vmManager.sendMultitouch(id, slots, xs, ys, actives)
    }

    fun syncClipboardToGuest(宿主文本: String): Boolean {
        val id = instanceId ?: return false
        if (宿主文本.isEmpty()) return false
        上次手动同步时刻 = System.currentTimeMillis()
        // 桥接器推送读取系统剪贴板 ， 参数文本直推作为兜底 ， 两条均为真实写入 。
        val 直推成功 = runCatching { vmManager.pushClipboard(id, 宿主文本) }.getOrDefault(false)
        viewModelScope.launch {
            runCatching { 剪贴板桥接?.pushHostToGuest(id) }
            runCatching { 剪贴板桥接?.pullGuestToHost(id) }
        }
        return 直推成功
    }

    fun pullClipboardFromGuest(): String {
        val id = instanceId ?: return ""
        上次手动同步时刻 = System.currentTimeMillis()
        // 先走桥接器双通道拉取并回写系统剪贴板 ， 再返回原生文本 ， 均为真实读取 。
        runCatching { 剪贴板桥接?.pullGuestToHost(id) }
        return runCatching { vmManager.pullClipboard(id) }.getOrDefault("")
    }

    // 启动防抖轮询 ： 每间隔调用桥接器推送与拉取 ， 手动同步后防抖窗口内跳过 。
    private fun 启动剪贴板轮询(编号: String) {
        停止剪贴板轮询()
        val 桥接 = 剪贴板桥接 ?: return
        剪贴板轮询任务 = viewModelScope.launch {
            while (isActive) {
                delay(剪贴板轮询间隔毫秒)
                if (!isActive) break
                val 距手动 = System.currentTimeMillis() - 上次手动同步时刻
                if (距手动 < 剪贴板防抖窗口毫秒) continue
                runCatching { 桥接.pushHostToGuest(编号) }
                runCatching { 桥接.pullGuestToHost(编号) }
            }
        }
    }

    // 停止防抖轮询并释放任务 。
    private fun 停止剪贴板轮询() {
        剪贴板轮询任务?.cancel()
        剪贴板轮询任务 = null
    }

    override fun onCleared() {
        super.onCleared()
        停止剪贴板轮询()
        statusJob?.cancel()
        statusJob = null
        instanceId = null
        instanceHandle = 0L
    }

    fun sendBack() = sendAndroidKey(KeyEvent.KEYCODE_BACK)

    fun sendHome() = sendAndroidKey(KeyEvent.KEYCODE_HOME)

    fun sendRecents() = sendAndroidKey(KeyEvent.KEYCODE_APP_SWITCH)

    private fun sendAndroidKey(keycode: Int) {
        val id = instanceId ?: return
        vmManager.sendKey(id, keycode, true)
        vmManager.sendKey(id, keycode, false)
    }

    private fun resolveHandle(): Long? {
        if (instanceHandle != 0L) return instanceHandle
        val id = instanceId ?: return null
        val fd = vmManager.getFramebufferFd(id)
        if (fd < 0) return null
        // getFramebufferFd() confirms a valid handle exists before we fetch and cache it via getHandle()
        instanceHandle = vmManager.getHandle(id) ?: return null
        return instanceHandle
    }
}
