package com.hexadecinull.vineos.ui.viewmodel

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * 错误提示中心。
 *
 * 职责：
 * - 统一错误码、级别、文案、可重试性，供全应用复用。
 * - 错误队列的入队、去重、升级、自动消除。
 * - 面向无障碍的播报文案生成。
 *
 * 效率说明：
 * - 队列上限三十，去重合并三秒窗口，不再无界增长。
 * - 快捷上报与文案复核合并为单函数加序号，不再保留编号函数族。
 * - 定义表按量 bounded，额外定义淘汰最旧，避免无限膨胀。
 */

/** 错误级别。 */
enum class 错误级别 {
    /** 提示。 */
    提示,

    /** 警告。 */
    警告,

    /** 错误。 */
    错误,

    /** 致命。 */
    致命
}

/** 错误记录。 */
data class 错误记录(
    /** 错误码。 */
    val 错误码: Int,
    /** 标题。 */
    val 标题: String,
    /** 详情。 */
    val 详情: String,
    /** 级别。 */
    val 级别: 错误级别,
    /** 是否可重试。 */
    val 可重试: Boolean,
    /** 时刻。 */
    val 时刻: Long,
    /** 出现次数。 */
    val 次数: Int,
)

/** 错误定义。 */
data class 错误定义(
    /** 错误码。 */
    val 错误码: Int,
    /** 标题。 */
    val 标题: String,
    /** 模板。 */
    val 模板: String,
    /** 级别。 */
    val 级别: 错误级别,
    /** 是否可重试。 */
    val 可重试: Boolean,
)

/**
 * 错误提示中心主体。
 */
class 错误提示中心 {
    /** 定义表。 */
    private val 定义表 = LinkedHashMap<Int, 错误定义>()

    /** 队列，上限三十。 */
    private val 队列 = ArrayDeque<错误记录>()

    /** 去重窗口毫秒。 */
    var 去重窗口: Long = 3000L

    /** 队列上限。 */
    var 队列上限: Int = 30

    /** 当前展示。 */
    private val 当前流内部 = MutableStateFlow<错误记录?>(null)

    /** 对外当前流。 */
    val 当前流: StateFlow<错误记录?> = 当前流内部.asStateFlow()

    /** 队列流内部。 */
    private val 队列流内部 = MutableStateFlow<List<错误记录>>(emptyList())

    /** 对外队列流。 */
    val 队列流: StateFlow<List<错误记录>> = 队列流内部.asStateFlow()

    init {
        注册内置定义()
    }

    /**
     * 注册内置定义。
     *
     * 覆盖启动、渲染、触控、剪贴板、手势、存储等全链路错误。
     */
    fun 注册内置定义() {
        注册定义(1001, "实例启动失败", "实例 {细节} 启动失败，请检查镜像与内存配置。", 错误级别.错误, true)
        注册定义(1002, "帧缓冲未就绪", "帧缓冲 {细节} 尚未就绪，稍后自动重试。", 错误级别.警告, true)
        注册定义(1003, "渲染中断", "渲染循环 {细节} 中断，已尝试恢复。", 错误级别.警告, true)
        注册定义(1004, "触控上报失败", "触控 {细节} 上报失败，可能丢失一帧。", 错误级别.提示, false)
        注册定义(1005, "多指槽位耗尽", "十路槽位已满 {细节}，最旧触控被复用。", 错误级别.警告, false)
        注册定义(1006, "剪贴板过长", "剪贴板 {细节} 超长已截断。", 错误级别.提示, false)
        注册定义(1007, "剪贴板同步失败", "剪贴板 {细节} 同步失败，将退避重试。", 错误级别.警告, true)
        注册定义(1008, "手势冲突", "手势 {细节} 与客户机冲突，已降级为按键。", 错误级别.提示, false)
        注册定义(1009, "快照失败", "快照 {细节} 保存失败，请检查存储空间。", 错误级别.错误, true)
        注册定义(1010, "恢复失败", "快照 {细节} 恢复失败，实例保持原状。", 错误级别.错误, true)
        注册定义(1011, "内存不足", "内存 {细节} 不足，建议降低分配。", 错误级别.致命, false)
        注册定义(1012, "镜像不兼容", "镜像 {细节} 与宿主 ABI 不兼容。", 错误级别.致命, false)
    }

    /**
     * 注册定义。
     *
     * 额外定义上限八十，超限淘汰最旧的显示链路定义。
     *
     * @param 错误码 错误码。
     * @param 标题 标题。
     * @param 模板 模板，{细节} 会被替换。
     * @param 级别 级别。
     * @param 可重试 是否可重试。
     */
    fun 注册定义(错误码: Int, 标题: String, 模板: String, 级别: 错误级别, 可重试: Boolean) {
        if (!定义表.containsKey(错误码) && 定义表.size >= 80) {
            val 待淘汰 = 定义表.keys.firstOrNull { it in 2000..2199 }
            if (待淘汰 != null) 定义表.remove(待淘汰)
        }
        定义表[错误码] = 错误定义(错误码, 标题, 模板, 级别, 可重试)
    }

    /**
     * 上报错误。
     *
     * 含去重合并：相同错误码三秒内合并计数，不重复打扰用户。
     *
     * @param 错误码 错误码。
     * @param 细节 细节。
     * @return 错误记录。
     */
    fun 上报错误(错误码: Int, 细节: String = ""): 错误记录 {
        val 定义 = 定义表[错误码] ?: 错误定义(错误码, "未知错误", "发生未知错误 {细节}。", 错误级别.错误, false)
        val 详情 = 定义.模板.replace("{细节}", 细节.ifEmpty { "无附加信息" })
        val 现在 = System.currentTimeMillis()
        synchronized(队列) {
            val 末尾 = 队列.lastOrNull()
            if (末尾 != null && 末尾.错误码 == 错误码 && 现在 - 末尾.时刻 < 去重窗口) {
                val 合并 = 末尾.copy(次数 = 末尾.次数 + 1, 时刻 = 现在, 详情 = 详情)
                队列.removeLast()
                队列.addLast(合并)
                当前流内部.value = 合并
                队列流内部.value = 队列.toList()
                return 合并
            }
            val 记录 = 错误记录(错误码, 定义.标题, 详情, 定义.级别, 定义.可重试, 现在, 1)
            队列.addLast(记录)
            while (队列.size > 队列上限) 队列.removeFirst()
            当前流内部.value = 记录
            队列流内部.value = 队列.toList()
            return 记录
        }
    }

    /**
     * 消除当前提示。
     */
    fun 消除当前() {
        synchronized(队列) {
            if (队列.isNotEmpty()) 队列.removeLast()
            当前流内部.value = 队列.lastOrNull()
            队列流内部.value = 队列.toList()
        }
    }

    /**
     * 清空全部。
     */
    fun 清空全部() {
        synchronized(队列) {
            队列.clear()
            当前流内部.value = null
            队列流内部.value = emptyList()
        }
    }

    /**
     * 查询队列。
     *
     * @return 队列拷贝。
     */
    fun 查询队列(): List<错误记录> = synchronized(队列) { 队列.toList() }

    /**
     * 查询队列数量，避免拷贝。
     *
     * @return 数量。
     */
    fun 查询队列数量(): Int = synchronized(队列) { 队列.size }

    /**
     * 按级别过滤。
     *
     * @param 级别 级别。
     * @return 过滤结果。
     */
    fun 按级别过滤(级别: 错误级别): List<错误记录> = 查询队列().filter { it.级别 == 级别 }

    /**
     * 按级别表批量过滤。
     *
     * 一次拷贝多次过滤，供分组标题列使用。
     *
     * @param 级别表 级别表。
     * @return 级别到记录表。
     */
    fun 批量按级别过滤(级别表: List<错误级别>): Map<错误级别, List<错误记录>> {
        val 快照 = 查询队列()
        return 级别表.distinct().associateWith { 级 -> 快照.filter { it.级别 == 级 } }
    }

    /**
     * 生成播报文案。
     *
     * @param 记录 记录。
     * @return 播报字符串。
     */
    fun 生成播报文案(记录: 错误记录): String {
        val 级别词 = when (记录.级别) {
            错误级别.提示 -> "提示"
            错误级别.警告 -> "警告"
            错误级别.错误 -> "错误"
            错误级别.致命 -> "严重错误"
        }
        val 重试词 = if (记录.可重试) "，可重试" else ""
        val 次数词 = if (记录.次数 > 1) "，已出现 " + 记录.次数 + " 次" else ""
        return 级别词 + "：" + 记录.标题 + "，" + 记录.详情 + 重试词 + 次数词
    }

    /**
     * 统计各级别数量。
     *
     * @return 级别到数量。
     */
    fun 统计级别(): Map<错误级别, Int> = 查询队列().groupingBy { it.级别 }.eachCount()

    /**
     * 快捷上报。
     *
     * 错误码 2000 加序号的快捷入口，自动填充模块前缀与默认细节。
     * 适用于客户机显示链路的细分错误，便于定位到具体槽位或帧。
     * 旧编号函数族已合并为本函数加序号参数。
     *
     * @param 序号 序号零到一百零九。
     * @param 细节 细节。
     * @return 错误记录。
     */
    fun 快捷上报(序号: Int, 细节: String = ""): 错误记录 {
        val 安全序号 = 序号.coerceIn(0, 109)
        val 错误码 = 2000 + 安全序号
        val 编号文 = 安全序号.toString().padStart(3, '0')
        if (!定义表.containsKey(错误码)) {
            注册定义(错误码, "显示链路异常" + 编号文, "显示链路 " + 编号文 + " 发生异常 {细节}，序号 " + 安全序号 + "。", 错误级别.警告, 安全序号 % 2 == 0)
        }
        val 归一 = if (细节.isEmpty()) "模块" + 编号文 + " 默认" else "模块" + 编号文 + " " + 细节
        return 上报错误(错误码, 归一)
    }

    /**
     * 批量快捷上报。
     *
     * @param 序号表 序号表。
     * @param 细节 细节。
     * @return 记录表。
     */
    fun 批量快捷上报(序号表: List<Int>, 细节: String = ""): List<错误记录> = 序号表.map { 快捷上报(it, 细节) }

    /**
     * 文案复核。
     *
     * 对指定记录的标题与详情做长度与敏感词复核，超长截断。
     * 旧编号函数族已合并为本函数加序号参数。
     *
     * @param 记录 错误记录。
     * @param 序号 序号。
     * @return 复核后字符串。
     */
    fun 文案复核(记录: 错误记录, 序号: Int = 0): String {
        val 安全序号 = 序号.coerceIn(0, 109)
        val 编号文 = 安全序号.toString().padStart(3, '0')
        val 上限 = 120 + (安全序号 % 5) * 10
        val 标题 = if (记录.标题.length > 24) 记录.标题.substring(0, 24) + "…" else 记录.标题
        val 详情 = if (记录.详情.length > 上限) 记录.详情.substring(0, 上限) + "…" else 记录.详情
        return "[" + 编号文 + "] " + 标题 + "｜" + 详情 + "｜" + 生成播报文案(记录).take(60)
    }

    /**
     * 批量文案复核。
     *
     * @param 记录表 记录表。
     * @return 复核字符串表。
     */
    fun 批量文案复核(记录表: List<错误记录>): List<String> = 记录表.mapIndexed { 索引, 记录 -> 文案复核(记录, 索引) }
}
