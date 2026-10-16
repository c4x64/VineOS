package com.hexadecinull.vineos.ui.viewmodel

import android.view.Surface
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hexadecinull.vineos.data.models.VMStatus
import com.hexadecinull.vineos.data.repository.InstanceRepository
import com.hexadecinull.vineos.domain.VineVMManager
import com.hexadecinull.vineos.native.VineRuntime
import kotlin.math.min
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * 显示视图模型。
 *
 * 中文高质量实现的显示层视图模型，组合多指分发、手势增强、
 * 状态机、剪贴板调度、错误中心，对 VMDisplayScreen 提供单一入口。
 *
 * 效率说明：
 * - 状态收集按值去重，不再无条件驱动状态机。
 * - 多指发送按最小间隔节流加长度校验，避免空帧风暴。
 * - 兼容旧接口合并为单函数加序号加批量列表，删除编号函数族。
 */

/** 兼容分发批次。 */
data class 兼容分发批次(
    /** 槽位。 */
    val 槽位: IntArray,
    /** 横集。 */
    val 横集: FloatArray,
    /** 纵集。 */
    val 纵集: FloatArray,
    /** 存活集。 */
    val 存活集: BooleanArray,
)

/**
 * 显示视图模型主体。
 *
 * @param 虚拟机管理 虚拟机管理器。
 * @param 实例仓库 实例仓库。
 */
class 显示视图模型(private val 虚拟机管理: VineVMManager, private val 实例仓库: InstanceRepository,) : ViewModel() {
    /** 多指分发器。 */
    val 触控分发 = 多指触控分发器()

    /** 手势增强。 */
    val 手势增强 = 手势导航增强()

    /** 状态机。 */
    val 状态机 = 显示状态机()

    /** 剪贴板调度。 */
    val 剪贴板调度 = 剪贴板同步调度()

    /** 错误中心。 */
    val 错误中心 = 错误提示中心()

    /** 实例编号。 */
    private var 实例编号: String? = null

    /** 实例句柄。 */
    private var 实例句柄: Long = 0L

    /** 状态任务。 */
    private var 状态任务: Job? = null

    /** 上次虚拟机状态，去重用。 */
    private var 上次虚拟机状态: VMStatus? = null

    /** 上次多指发送时刻，节流用。 */
    private var 上次发送时刻: Long = 0L

    /** 多指最小发送间隔毫秒。 */
    var 最小发送间隔: Long = 8L

    /** 是否启动中。 */
    private val 启动中流内部 = MutableStateFlow(false)

    /** 对外启动中流。 */
    val 启动中流: StateFlow<Boolean> = 启动中流内部.asStateFlow()

    /**
     * 附加实例并开始收集状态。
     *
     * 状态按值去重后驱动，避免重复首帧与重复报错。
     *
     * @param 编号 实例编号。
     */
    fun 附加实例(编号: String) {
        实例编号 = 编号
        上次虚拟机状态 = null
        状态机.驱动(显示事件.附加)
        状态任务?.cancel()
        状态任务 = viewModelScope.launch {
            runCatching {
                虚拟机管理.statusFlow(编号).collect { 状态 ->
                    if (状态 == 上次虚拟机状态) return@collect
                    上次虚拟机状态 = 状态
                    val 启动中 = 状态 == VMStatus.BOOTING
                    if (启动中流内部.value != 启动中) 启动中流内部.value = 启动中
                    when (状态) {
                        VMStatus.RUNNING -> if (状态机.当前状态 == 显示状态.启动中) 状态机.驱动(显示事件.首帧到达)
                        VMStatus.ERROR -> {
                            状态机.驱动(显示事件.报错, "实例进入错误态")
                            错误中心.上报错误(1001, 编号)
                        }
                        else -> {}
                    }
                }
            }.onFailure { 异常 ->
                错误中心.上报错误(1001, 异常.message ?: 编号)
            }
        }
        剪贴板调度.推送实现 = { 文本 ->
            val 目标 = 实例编号
            if (目标 == null) false else runCatching { 虚拟机管理.pushClipboard(目标, 文本) }.getOrDefault(false)
        }
        剪贴板调度.拉取实现 = {
            val 目标 = 实例编号
            if (目标 == null) "" else runCatching { 虚拟机管理.pullClipboard(目标) }.getOrDefault("")
        }
        剪贴板调度.启动轮询()
    }

    /**
     * 分离实例并释放资源。
     */
    fun 分离实例() {
        状态任务?.cancel()
        状态任务 = null
        实例编号 = null
        实例句柄 = 0L
        上次虚拟机状态 = null
        上次发送时刻 = 0L
        剪贴板调度.停止轮询()
        触控分发.重置全部()
        手势增强.重置状态()
        状态机.驱动(显示事件.释放)
        if (启动中流内部.value) 启动中流内部.value = false
    }

    /**
     * 表面就绪。
     *
     * @param 表面 表面。
     */
    fun 表面就绪(表面: Surface) {
        val 句柄 = 解析句柄() ?: run {
            错误中心.上报错误(1002, "句柄为空")
            return
        }
        runCatching {
            VineRuntime.attachSurface(句柄, 表面)
            VineRuntime.startRendering(句柄)
        }.onFailure { 异常 ->
            状态机.驱动(显示事件.报错, 异常.message ?: "附加表面失败")
            错误中心.上报错误(1003, 异常.message ?: "")
        }
    }

    /**
     * 表面销毁。
     */
    fun 表面销毁() {
        val 句柄 = 解析句柄() ?: return
        runCatching {
            VineRuntime.stopRendering(句柄)
            VineRuntime.detachSurface(句柄)
        }
    }

    /**
     * 分发多指事件。
     *
     * 槽位为稳定指针编号数组，空帧与超频帧直接丢弃。
     *
     * @param 槽位 稳定指针编号数组。
     * @param 横集 横坐标集。
     * @param 纵集 纵坐标集。
     * @param 存活集 存活标志集。
     */
    fun 分发多指事件(槽位: IntArray, 横集: FloatArray, 纵集: FloatArray, 存活集: BooleanArray) {
        val 编号 = 实例编号 ?: return
        if (槽位.isEmpty()) return
        val 有效数 = min(min(槽位.size, 横集.size), min(纵集.size, 存活集.size))
        if (有效数 <= 0) return
        val 现在 = System.currentTimeMillis()
        if (现在 - 上次发送时刻 < 最小发送间隔) return
        上次发送时刻 = 现在
        runCatching {
            触控分发.分发多指事件(0, 0, 有效数, 槽位, 横集, 纵集, 存活集)
            状态机.上报一帧(现在)
            虚拟机管理.sendMultitouch(编号, 槽位, 横集, 纵集, 存活集)
        }.onFailure { 异常 ->
            错误中心.上报错误(1004, 异常.message ?: "")
        }
    }

    /**
     * 分发单指事件。
     *
     * @param 动作 动作 0 按下 1 移动 2 抬起。
     * @param 横 横坐标。
     * @param 纵 纵坐标。
     */
    fun 分发单指事件(动作: Int, 横: Float, 纵: Float) {
        val 编号 = 实例编号 ?: return
        runCatching {
            when (动作) {
                0 -> 手势增强.触摸按下(横, 纵)
                1 -> 手势增强.触摸移动(横, 纵)
                else -> {
                    val 事件 = 手势增强.触摸抬起(横, 纵)
                    if (事件.意图 != 导航意图.无) 处理导航意图(事件.意图)
                }
            }
            虚拟机管理.sendTouch(编号, 动作, 横, 纵)
        }.onFailure { 异常 ->
            错误中心.上报错误(1004, 异常.message ?: "")
        }
    }

    /**
     * 处理导航意图。
     *
     * @param 意图 导航意图。
     */
    fun 处理导航意图(意图: 导航意图) {
        when (意图) {
            导航意图.返回 -> 发送返回键()
            导航意图.主页 -> 发送主页键()
            导航意图.概览 -> 发送概览键()
            导航意图.无 -> {}
        }
    }

    /** 发送返回键。 */
    fun 发送返回键() = 发送按键(android.view.KeyEvent.KEYCODE_BACK)

    /** 发送主页键。 */
    fun 发送主页键() = 发送按键(android.view.KeyEvent.KEYCODE_HOME)

    /** 发送概览键。 */
    fun 发送概览键() = 发送按键(android.view.KeyEvent.KEYCODE_APP_SWITCH)

    /**
     * 发送按键。
     *
     * @param 键码 键码。
     */
    fun 发送按键(键码: Int) {
        val 编号 = 实例编号 ?: return
        runCatching {
            虚拟机管理.sendKey(编号, 键码, true)
            虚拟机管理.sendKey(编号, 键码, false)
        }.onFailure { 异常 ->
            错误中心.上报错误(1004, 异常.message ?: "")
        }
    }

    /**
     * 同步剪贴板到客户机。
     *
     * @param 文本 文本。
     */
    fun 同步剪贴板到客户机(文本: String) {
        if (文本.isEmpty()) return
        viewModelScope.launch {
            runCatching { 剪贴板调度.推送宿主文本(文本) }.onFailure { 错误中心.上报错误(1007, it.message ?: "") }
        }
    }

    /**
     * 重试启动。
     */
    fun 重试启动() {
        if (状态机.是否可重试()) {
            状态机.驱动(显示事件.重试)
            错误中心.消除当前()
        }
    }

    /**
     * 解析句柄。
     *
     * @return 句柄或空。
     */
    private fun 解析句柄(): Long? {
        if (实例句柄 != 0L) return 实例句柄
        val 编号 = 实例编号 ?: return null
        if (虚拟机管理.getFramebufferFd(编号) < 0) return null
        val 句柄 = 虚拟机管理.getHandle(编号) ?: return null
        实例句柄 = 句柄
        return 句柄
    }

    override fun onCleared() {
        super.onCleared()
        分离实例()
        状态机.重置全部()
    }

    /**
     * 兼容旧接口：附加实例。
     *
     * 保留英文旧 ViewModel 的调用习惯，内部委托给中文实现。
     * 旧编号函数族已合并为本函数加序号参数。
     *
     * @param 编号 实例编号。
     * @param 序号 兼容序号。
     */
    fun 兼容附加(编号: String, 序号: Int = 0) {
        附加实例(编号)
    }

    /**
     * 兼容旧接口：分发多指。
     *
     * 旧编号函数族已合并为本函数加序号参数。
     *
     * @param 槽位 槽位。
     * @param 横集 横集。
     * @param 纵集 纵集。
     * @param 存活集 存活集。
     * @param 序号 兼容序号。
     */
    fun 兼容分发(槽位: IntArray, 横集: FloatArray, 纵集: FloatArray, 存活集: BooleanArray, 序号: Int = 0) {
        分发多指事件(槽位, 横集, 纵集, 存活集)
    }

    /**
     * 批量兼容分发。
     *
     * @param 批次 批次表。
     */
    fun 批量兼容分发(批次: List<兼容分发批次>) {
        for (单 in 批次) 分发多指事件(单.槽位, 单.横集, 单.纵集, 单.存活集)
    }
}
