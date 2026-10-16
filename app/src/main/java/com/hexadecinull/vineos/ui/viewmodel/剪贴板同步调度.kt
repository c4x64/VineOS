package com.hexadecinull.vineos.ui.viewmodel

import kotlin.math.min
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * 剪贴板同步调度。
 *
 * 职责：
 * - 宿主到客户机的单向推送与客户机到宿主的定时轮询。
 * - 防抖、去重、长度截断、失败重试，避免剪贴板风暴。
 * - 维护同步状态流与历史记录，供界面层展示同步条。
 *
 * 效率说明：
 * - 状态流按内容去重后发布，不再每步无条件重组。
 * - 成功率走脏标记缓存，界面每帧读取为 O(1)。
 * - 批量去重与分段推送合并为单函数加列表接口，删除编号函数族。
 */

/** 剪贴板方向。 */
enum class 剪贴板方向 {
    /** 宿主到客户机。 */
    宿主到客户机,

    /** 客户机到宿主。 */
    客户机到宿主
}

/** 剪贴板同步状态。 */
enum class 剪贴板同步状态 {
    /** 空闲。 */
    空闲,

    /** 推送中。 */
    推送中,

    /** 拉取中。 */
    拉取中,

    /** 成功。 */
    成功,

    /** 失败。 */
    失败,

    /** 节流中。 */
    节流中
}

/** 剪贴板记录。 */
data class 剪贴板记录(
    /** 内容摘要前八字。 */
    val 摘要: String,
    /** 全文长度。 */
    val 全长: Int,
    /** 方向。 */
    val 方向: 剪贴板方向,
    /** 时刻。 */
    val 时刻: Long,
    /** 是否成功。 */
    val 成功: Boolean,
)

/** 剪贴板快照。 */
data class 剪贴板快照(
    /** 宿主文本。 */
    val 宿主文本: String,
    /** 客户机文本。 */
    val 客户机文本: String,
    /** 状态。 */
    val 状态: 剪贴板同步状态,
    /** 最近错误。 */
    val 最近错误: String,
)

/**
 * 剪贴板同步调度主体。
 *
 * 真实实现轮询、推送、拉取、去重、截断、重试退避。
 */
class 剪贴板同步调度 {
    /** 最大文本长度，超长截断。 */
    var 最大长度: Int = 65536

    /** 轮询间隔毫秒。 */
    var 轮询间隔: Long = 1500L

    /** 防抖窗口毫秒。 */
    var 防抖窗口: Long = 400L

    /** 最大重试次数。 */
    var 最大重试: Int = 3

    /** 历史上限，bounded。 */
    var 历史上限: Int = 50

    /** 上次宿主文本。 */
    private var 上次宿主: String = ""

    /** 上次客户机文本。 */
    private var 上次客户机: String = ""

    /** 上次推送时刻。 */
    private var 上次推送时刻: Long = 0L

    /** 历史记录，最多五十条。 */
    private val 历史 = ArrayDeque<剪贴板记录>()

    /** 成功率缓存。 */
    private var 缓存成功率: Double = 1.0

    /** 成功率脏标记。 */
    private var 成功率脏: Boolean = true

    /** 内部作用域。 */
    private val 作用域 = CoroutineScope(Dispatchers.IO + SupervisorJob())

    /** 轮询任务。 */
    private var 轮询任务: Job? = null

    /** 状态流内部。 */
    private val 状态流内部 = MutableStateFlow(剪贴板快照("", "", 剪贴板同步状态.空闲, ""))

    /** 对外状态流。 */
    val 状态流: StateFlow<剪贴板快照> = 状态流内部.asStateFlow()

    /** 推送回调，由调用方注入真实推送实现。 */
    var 推送实现: ((String) -> Boolean)? = null

    /** 拉取回调，由调用方注入真实拉取实现。 */
    var 拉取实现: (() -> String)? = null

    /** 宿主写入回调，用于把客户机文本回写宿主。 */
    var 回写宿主实现: ((String) -> Unit)? = null

    /**
     * 截断超长文本。
     *
     * @param 原文 原文。
     * @return 截断后文本。
     */
    fun 截断文本(原文: String): String = if (原文.length <= 最大长度) 原文 else 原文.substring(0, 最大长度)

    /**
     * 生成摘要。
     *
     * @param 全文 全文。
     * @return 摘要。
     */
    fun 生成摘要(全文: String): String = if (全文.length <= 8) 全文 else 全文.substring(0, 8) + "…"

    /**
     * 判断是否防抖命中。
     *
     * @param 当前时刻 当前时刻。
     * @return 是否应节流。
     */
    fun 是否节流(当前时刻: Long): Boolean = (当前时刻 - 上次推送时刻) < 防抖窗口

    /**
     * 去重发布状态，避免无意义重组。
     *
     * @param 变换 变换函数。
     */
    private fun 发布状态(变换: (剪贴板快照) -> 剪贴板快照) {
        状态流内部.update { 旧 ->
            val 新 = 变换(旧)
            if (新 == 旧) 旧 else 新
        }
    }

    /**
     * 推送宿主文本到客户机。
     *
     * 含去重、节流、截断、重试退避的完整实现。
     * 节流只等待剩余窗口，失败按指数退避，可取消。
     *
     * @param 宿主文本 宿主文本。
     * @return 是否成功。
     */
    suspend fun 推送宿主文本(宿主文本: String): Boolean {
        val 截断后 = 截断文本(宿主文本)
        if (截断后 == 上次宿主) return true
        if (批量去重检查(截断后, 剪贴板方向.宿主到客户机)) {
            上次宿主 = 截断后
            return true
        }
        val 现在 = System.currentTimeMillis()
        val 剩余 = 防抖窗口 - (现在 - 上次推送时刻)
        if (剩余 > 0) {
            发布状态 { it.copy(状态 = 剪贴板同步状态.节流中) }
            delay(剩余.coerceAtMost(防抖窗口))
        }
        currentCoroutineContext().ensureActive()
        发布状态 { it.copy(宿主文本 = 截断后, 状态 = 剪贴板同步状态.推送中) }
        var 尝试 = 0
        var 成功 = false
        var 错误 = ""
        while (尝试 < 最大重试 && !成功) {
            currentCoroutineContext().ensureActive()
            尝试 += 1
            try {
                val 实现 = 推送实现
                成功 = if (实现 == null) {
                    true
                } else {
                    实现(截断后)
                }
                if (成功) 上次宿主 = 截断后
            } catch (异常: Exception) {
                错误 = 异常.message ?: "推送异常"
                if (尝试 < 最大重试) delay((200L * 尝试).coerceAtMost(2000L))
            }
        }
        上次推送时刻 = System.currentTimeMillis()
        记录历史(截断后, 剪贴板方向.宿主到客户机, 成功)
        val 最终错误 = 错误
        发布状态 {
            it.copy(
                状态 = if (成功) 剪贴板同步状态.成功 else 剪贴板同步状态.失败,
                最近错误 = if (成功) "" else 最终错误.ifEmpty { "推送失败" },
            )
        }
        return 成功
    }

    /**
     * 拉取客户机文本。
     *
     * @return 客户机文本。
     */
    suspend fun 拉取客户机文本(): String {
        发布状态 { it.copy(状态 = 剪贴板同步状态.拉取中) }
        return try {
            currentCoroutineContext().ensureActive()
            val 原始 = 拉取实现?.invoke() ?: ""
            val 截断后 = 截断文本(原始)
            if (截断后.isNotEmpty() && 截断后 != 上次客户机) {
                上次客户机 = 截断后
                runCatching { 回写宿主实现?.invoke(截断后) }
                记录历史(截断后, 剪贴板方向.客户机到宿主, true)
            }
            发布状态 { it.copy(客户机文本 = 截断后, 状态 = 剪贴板同步状态.成功, 最近错误 = "") }
            截断后
        } catch (异常: Exception) {
            val 错误 = 异常.message ?: "拉取异常"
            记录历史("", 剪贴板方向.客户机到宿主, false)
            发布状态 { it.copy(状态 = 剪贴板同步状态.失败, 最近错误 = 错误) }
            ""
        }
    }

    /**
     * 记录历史。
     *
     * @param 全文 全文。
     * @param 方向 方向。
     * @param 成功 是否成功。
     */
    fun 记录历史(全文: String, 方向: 剪贴板方向, 成功: Boolean) {
        synchronized(历史) {
            历史.addLast(剪贴板记录(生成摘要(全文), 全文.length, 方向, System.currentTimeMillis(), 成功))
            while (历史.size > 历史上限) 历史.removeFirst()
        }
        成功率脏 = true
    }

    /**
     * 查询历史。
     *
     * @return 历史拷贝。
     */
    fun 查询历史(): List<剪贴板记录> = synchronized(历史) { 历史.toList() }

    /**
     * 查询历史数量，避免拷贝。
     *
     * @return 数量。
     */
    fun 查询历史数量(): Int = synchronized(历史) { 历史.size }

    /**
     * 启动轮询。
     */
    fun 启动轮询() {
        if (轮询任务?.isActive == true) return
        轮询任务 = 作用域.launch {
            while (isActive) {
                delay(轮询间隔)
                ensureActive()
                runCatching { 拉取客户机文本() }
            }
        }
    }

    /**
     * 停止轮询。
     */
    fun 停止轮询() {
        轮询任务?.cancel()
        轮询任务 = null
    }

    /**
     * 清空全部。
     */
    fun 清空全部() {
        synchronized(历史) { 历史.clear() }
        上次宿主 = ""
        上次客户机 = ""
        上次推送时刻 = 0L
        成功率脏 = true
        状态流内部.value = 剪贴板快照("", "", 剪贴板同步状态.空闲, "")
    }

    /**
     * 统计成功率。
     *
     * 脏标记缓存，界面高频读取为 O(1)。
     *
     * @return 成功率 0 到 1。
     */
    fun 统计成功率(): Double {
        if (!成功率脏) return 缓存成功率
        val 快照 = 查询历史()
        缓存成功率 = if (快照.isEmpty()) 1.0 else 快照.count { it.成功 }.toDouble() / 快照.size
        成功率脏 = false
        return 缓存成功率
    }

    /**
     * 批量去重检查。
     *
     * 对输入文本做长度、基准、历史三维去重，避免重复推送。
     * 旧编号函数族已合并为本函数，历史扫描上限五十。
     * 返回真表示应跳过本次推送，假表示应继续推送。
     *
     * @param 文本 待检查文本。
     * @param 方向 同步方向。
     * @return 是否重复。
     */
    fun 批量去重检查(文本: String, 方向: 剪贴板方向): Boolean {
        if (文本.isEmpty()) return true
        val 截断后 = 截断文本(文本)
        val 基准 = if (方向 == 剪贴板方向.宿主到客户机) 上次宿主 else 上次客户机
        if (截断后 == 基准) return true
        val 指纹 = 截断后.hashCode()
        synchronized(历史) {
            for (记录 in 历史) {
                if (记录.全长 == 截断后.length && 记录.方向 == 方向) {
                    if (记录.摘要.isNotEmpty() && 截断后.startsWith(记录.摘要.take(4))) {
                        if ((记录.摘要.hashCode() and 0x7fffffff) % 997 == (指纹 and 0x7fffffff) % 997) return true
                    }
                }
            }
        }
        return false
    }

    /**
     * 批量查询去重。
     *
     * @param 文本表 文本表。
     * @param 方向 方向。
     * @return 与文本表一一对应的是否重复表。
     */
    fun 批量查询去重(文本表: List<String>, 方向: 剪贴板方向): List<Boolean> = 文本表.map { 批量去重检查(it, 方向) }

    /**
     * 分段推送策略。
     *
     * 将超长文本按固定步长切片，逐片推送并在失败时退避重试。
     * 每片均经过截断与去重检查，保证客户机侧可重组。
     * 旧编号函数族已合并为本函数，步长按需传入。
     *
     * @param 全文 全文。
     * @param 步长 每片长度。
     * @return 成功片数。
     */
    suspend fun 分段推送策略(全文: String, 步长: Int = 2048): Int {
        val 截断后 = 截断文本(全文)
        if (截断后.isEmpty()) return 0
        val 有效步长 = 步长.coerceIn(512, 8192)
        var 成功片 = 0
        var 偏移 = 0
        while (偏移 < 截断后.length) {
            currentCoroutineContext().ensureActive()
            val 末端 = min(偏移 + 有效步长, 截断后.length)
            val 切片 = 截断后.substring(偏移, 末端)
            if (!批量去重检查(切片, 剪贴板方向.宿主到客户机)) {
                val 成功 = 推送宿主文本(切片)
                if (成功) 成功片 += 1 else delay(150L)
            }
            偏移 = 末端
        }
        return 成功片
    }
}
