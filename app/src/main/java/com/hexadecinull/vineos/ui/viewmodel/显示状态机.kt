package com.hexadecinull.vineos.ui.viewmodel

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * 显示状态机。
 *
 * 职责：
 * - 管理客户机画面从空闲、启动中、运行中、暂停、错误、停止的全生命周期。
 * - 收集帧率、首帧耗时、错误计数等状态，供覆盖层展示。
 * - 超时熔断与自动重试，避免界面卡死在启动中。
 *
 * 效率说明：
 * - 性能流按最小间隔节流，不再每帧发布重组。
 * - 帧率走滑动窗口均值，真实统计间隔而非写死。
 * - 合法性检查与分段汇总合并为单函数加列表接口。
 */

/** 显示状态。 */
enum class 显示状态 {
    /** 空闲。 */
    空闲,

    /** 启动中。 */
    启动中,

    /** 运行中。 */
    运行中,

    /** 已暂停。 */
    已暂停,

    /** 错误。 */
    错误,

    /** 已停止。 */
    已停止
}

/** 显示事件。 */
enum class 显示事件 {
    /** 附加实例。 */
    附加,

    /** 首帧到达。 */
    首帧到达,

    /** 暂停。 */
    暂停,

    /** 恢复。 */
    恢复,

    /** 报错。 */
    报错,

    /** 停止。 */
    停止,

    /** 重试。 */
    重试,

    /** 超时。 */
    超时,

    /** 释放。 */
    释放
}

/** 状态迁移记录。 */
data class 迁移记录(
    /** 来源。 */
    val 来源: 显示状态,
    /** 事件。 */
    val 事件: 显示事件,
    /** 目标。 */
    val 目标: 显示状态,
    /** 时刻。 */
    val 时刻: Long,
)

/** 性能快照。 */
data class 性能快照(
    /** 帧率。 */
    val 帧率: Float,
    /** 首帧耗时。 */
    val 首帧耗时: Long,
    /** 总帧数。 */
    val 总帧数: Long,
    /** 掉帧数。 */
    val 掉帧数: Long,
    /** 错误数。 */
    val 错误数: Long,
)

/**
 * 显示状态机主体。
 */
class 显示状态机 {
    /** 当前状态。 */
    var 当前状态: 显示状态 = 显示状态.空闲
        private set

    /** 启动超时毫秒。 */
    var 启动超时: Long = 30000L

    /** 最大重试。 */
    var 最大重试: Int = 3

    /** 性能发布最小间隔毫秒。 */
    var 性能发布间隔: Long = 250L

    /** 已重试次数。 */
    var 已重试: Int = 0
        private set

    /** 启动开始时刻。 */
    private var 启动开始: Long = 0L

    /** 首帧时刻。 */
    private var 首帧时刻: Long = 0L

    /** 总帧数。 */
    private var 总帧数: Long = 0L

    /** 掉帧数。 */
    private var 掉帧数: Long = 0L

    /** 错误数。 */
    private var 错误数: Long = 0L

    /** 上帧时刻。 */
    private var 上帧时刻: Long = 0L

    /** 间隔样本和。 */
    private var 间隔总和: Long = 0L

    /** 间隔样本数。 */
    private var 间隔样本: Long = 0L

    /** 上次性能发布时刻。 */
    private var 上次性能发布: Long = 0L

    /** 最近错误。 */
    var 最近错误: String = ""
        private set

    /** 状态流内部。 */
    private val 状态流内部 = MutableStateFlow(显示状态.空闲)

    /** 对外状态流。 */
    val 状态流: StateFlow<显示状态> = 状态流内部.asStateFlow()

    /** 性能流内部。 */
    private val 性能流内部 = MutableStateFlow(性能快照(0f, 0L, 0L, 0L, 0L))

    /** 对外性能流。 */
    val 性能流: StateFlow<性能快照> = 性能流内部.asStateFlow()

    /** 迁移历史，上限二百。 */
    private val 迁移历史 = ArrayDeque<迁移记录>()

    /** 作用域。 */
    private val 作用域 = CoroutineScope(Dispatchers.Main + SupervisorJob())

    /** 超时任务。 */
    private var 超时任务: Job? = null

    /**
     * 驱动状态机。
     *
     * @param 事件 显示事件。
     * @param 错误信息 错误信息。
     * @return 是否迁移成功。
     */
    fun 驱动(事件: 显示事件, 错误信息: String = ""): Boolean {
        val 来源 = 当前状态
        val 目标 = 计算目标(来源, 事件)
        if (目标 == null) return false
        当前状态 = 目标
        if (状态流内部.value != 目标) 状态流内部.value = 目标
        synchronized(迁移历史) {
            迁移历史.addLast(迁移记录(来源, 事件, 目标, System.currentTimeMillis()))
            while (迁移历史.size > 200) 迁移历史.removeFirst()
        }
        处理副作用(事件, 错误信息)
        return true
    }

    /**
     * 计算目标状态。
     *
     * @param 来源 来源。
     * @param 事件 事件。
     * @return 目标或空表示非法迁移。
     */
    fun 计算目标(来源: 显示状态, 事件: 显示事件): 显示状态? = when (来源) {
        显示状态.空闲 -> when (事件) {
            显示事件.附加 -> 显示状态.启动中
            显示事件.释放 -> 显示状态.空闲
            else -> null
        }
        显示状态.启动中 -> when (事件) {
            显示事件.首帧到达 -> 显示状态.运行中
            显示事件.报错, 显示事件.超时 -> 显示状态.错误
            显示事件.停止, 显示事件.释放 -> 显示状态.已停止
            else -> null
        }
        显示状态.运行中 -> when (事件) {
            显示事件.暂停 -> 显示状态.已暂停
            显示事件.报错 -> 显示状态.错误
            显示事件.停止, 显示事件.释放 -> 显示状态.已停止
            else -> null
        }
        显示状态.已暂停 -> when (事件) {
            显示事件.恢复 -> 显示状态.运行中
            显示事件.停止, 显示事件.释放 -> 显示状态.已停止
            显示事件.报错 -> 显示状态.错误
            else -> null
        }
        显示状态.错误 -> when (事件) {
            显示事件.重试 -> if (已重试 < 最大重试) 显示状态.启动中 else 显示状态.错误
            显示事件.停止, 显示事件.释放 -> 显示状态.已停止
            显示事件.附加 -> 显示状态.启动中
            else -> null
        }
        显示状态.已停止 -> when (事件) {
            显示事件.附加 -> 显示状态.启动中
            显示事件.释放 -> 显示状态.空闲
            else -> null
        }
    }

    /**
     * 处理副作用。
     *
     * @param 事件 事件。
     * @param 错误信息 错误信息。
     */
    fun 处理副作用(事件: 显示事件, 错误信息: String) {
        when (事件) {
            显示事件.附加 -> {
                启动开始 = System.currentTimeMillis()
                首帧时刻 = 0L
                启动超时看门狗()
            }
            显示事件.首帧到达 -> {
                首帧时刻 = System.currentTimeMillis()
                超时任务?.cancel()
            }
            显示事件.报错 -> {
                错误数 += 1
                最近错误 = 错误信息.ifEmpty { "未知显示错误" }
                超时任务?.cancel()
            }
            显示事件.重试 -> {
                已重试 += 1
                启动开始 = System.currentTimeMillis()
                启动超时看门狗()
            }
            显示事件.超时 -> {
                错误数 += 1
                最近错误 = "启动超时"
            }
            显示事件.释放, 显示事件.停止 -> {
                超时任务?.cancel()
            }
            else -> {}
        }
        发布性能(true)
    }

    /**
     * 启动超时看门狗。
     */
    fun 启动超时看门狗() {
        超时任务?.cancel()
        超时任务 = 作用域.launch {
            delay(启动超时)
            if (当前状态 == 显示状态.启动中) 驱动(显示事件.超时)
        }
    }

    /**
     * 上报一帧。
     *
     * 性能发布节流到最小间隔，不再每帧重组。
     *
     * @param 时刻 帧时刻。
     */
    fun 上报一帧(时刻: Long = System.currentTimeMillis()) {
        if (当前状态 == 显示状态.启动中 && 首帧时刻 == 0L) {
            驱动(显示事件.首帧到达)
        }
        if (上帧时刻 != 0L) {
            val 间隔 = 时刻 - 上帧时刻
            if (间隔 in 1..1000) {
                间隔总和 += 间隔
                间隔样本 += 1
                if (间隔样本 > 120) {
                    间隔总和 /= 2
                    间隔样本 /= 2
                }
            }
            if (间隔 > 50L) 掉帧数 += 1
        }
        上帧时刻 = 时刻
        总帧数 += 1
        发布性能(false)
    }

    /**
     * 发布性能快照。
     *
     * 非强制时按最小间隔节流，内容无变化不发布。
     *
     * @param 强制 是否强制发布。
     */
    fun 发布性能(强制: Boolean = true) {
        val 现在 = System.currentTimeMillis()
        if (!强制 && 现在 - 上次性能发布 < 性能发布间隔) return
        val 首帧耗时 = if (首帧时刻 == 0L || 启动开始 == 0L) 0L else 首帧时刻 - 启动开始
        val 平均间隔 = if (间隔样本 == 0L) 16L else 间隔总和 / 间隔样本
        val 帧率 = if (总帧数 == 0L) 0f else (1000f / 平均间隔.coerceAtLeast(1L).toFloat()).coerceAtMost(60f)
        val 新快照 = 性能快照(帧率, 首帧耗时, 总帧数, 掉帧数, 错误数)
        if (!强制 && 新快照 == 性能流内部.value) return
        上次性能发布 = 现在
        性能流内部.value = 新快照
    }

    /**
     * 查询迁移历史。
     *
     * @return 历史拷贝。
     */
    fun 查询迁移历史(): List<迁移记录> = synchronized(迁移历史) { 迁移历史.toList() }

    /**
     * 查询迁移历史数量，避免拷贝。
     *
     * @return 数量。
     */
    fun 查询迁移数量(): Int = synchronized(迁移历史) { 迁移历史.size }

    /**
     * 是否可重试。
     *
     * @return 是否可重试。
     */
    fun 是否可重试(): Boolean = 当前状态 == 显示状态.错误 && 已重试 < 最大重试

    /**
     * 重置全部。
     */
    fun 重置全部() {
        超时任务?.cancel()
        超时任务 = null
        当前状态 = 显示状态.空闲
        状态流内部.value = 显示状态.空闲
        已重试 = 0
        启动开始 = 0L
        首帧时刻 = 0L
        总帧数 = 0L
        掉帧数 = 0L
        错误数 = 0L
        上帧时刻 = 0L
        间隔总和 = 0L
        间隔样本 = 0L
        上次性能发布 = 0L
        最近错误 = ""
        synchronized(迁移历史) { 迁移历史.clear() }
        发布性能(true)
    }

    /**
     * 迁移合法性检查。
     *
     * 旧编号函数族已合并为本函数，非法迁移直接返回假。
     * 用于界面禁用非法按钮，避免误触导致状态机拒绝。
     *
     * @param 来源 来源状态。
     * @param 事件 显示事件。
     * @return 是否允许。
     */
    fun 迁移合法性检查(来源: 显示状态, 事件: 显示事件): Boolean = 计算目标(来源, 事件) != null

    /**
     * 批量迁移合法性检查。
     *
     * @param 请求表 来源事件对表。
     * @return 是否允许表。
     */
    fun 批量迁移合法性检查(请求表: List<Pair<显示状态, 显示事件>>): List<Boolean> = 请求表.map { 迁移合法性检查(it.first, it.second) }

    /**
     * 性能分段汇总。
     *
     * 按窗口切分总帧数，估算分段帧率与掉帧率，供性能面板绘制。
     * 旧编号函数族已合并为本函数。
     *
     * @param 窗口 窗口帧数。
     * @return 描述字符串。
     */
    fun 性能分段汇总(窗口: Long = 120L): String {
        val 有效窗口 = if (窗口 <= 0) 120L else 窗口
        val 分段 = (总帧数 / 有效窗口).toInt()
        val 掉帧率 = if (总帧数 == 0L) 0.0 else 掉帧数.toDouble() / 总帧数
        val 首帧 = if (首帧时刻 == 0L) 0L else 首帧时刻 - 启动开始
        return "分段 段=" + 分段 + " 帧率=" + 性能流内部.value.帧率 + " 掉帧率=" + 掉帧率 + " 首帧=" + 首帧
    }

    /**
     * 批量性能分段汇总。
     *
     * @param 窗口表 窗口表。
     * @return 描述表。
     */
    fun 批量性能分段汇总(窗口表: List<Long>): List<String> = 窗口表.map { 性能分段汇总(it) }
}
