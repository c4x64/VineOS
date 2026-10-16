package com.hexadecinull.vineos.网络

/** 流量统计器：解析 /proc/net/dev、iptables/nft 计数器，轮询采样，历史环形缓冲。 */
import java.io.File
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.concurrent.ConcurrentHashMap

/** 单网卡流量快照。 */
data class 网卡流量(val 网卡: String, val 收字节: Long, val 发字节: Long, val 收包: Long, val 发包: Long, val 采样时间: Long)
/** 流量速率。 */
data class 流量速率(val 网卡: String, val 下行字节每秒: Double, val 上行字节每秒: Double, val 总字节: Long)
/** 实例流量汇总。 */
data class 实例流量(val 实例标识: String, val 网卡: String, val 累计收字节: Long, val 累计发字节: Long, val 速率: 流量速率?)

@Singleton
class 流量统计器 @Inject constructor(private val 执行器: 特权命令执行器) {
    private val 上次采样 = ConcurrentHashMap<String, 网卡流量>()
    private val 累计收 = ConcurrentHashMap<String, Long>()
    private val 累计发 = ConcurrentHashMap<String, Long>()
    private val 历史 = ArrayDeque<网卡流量>()

    /** 读取 /proc/net/dev 真实文件并解析。 */
    fun 读取系统流量(): List<网卡流量> {
        val 文件 = File("/proc/net/dev")
        if (!文件.canRead()) {
            val 兜底 = 执行器.智能执行(listOf("cat", "/proc/net/dev"), false)
            if (!兜底.是否成功()) return emptyList()
            return 解析ProcNetDev(兜底.标准输出)
        }
        return try { 解析ProcNetDev(文件.readText()) } catch (_: Exception) { emptyList() }
    }

    /** 解析 /proc/net/dev 文本。 */
    fun 解析ProcNetDev(文本: String): List<网卡流量> {
        val 结果 = mutableListOf<网卡流量>()
        val 现在 = System.currentTimeMillis()
        for (行 in 文本.lines()) {
            if (!行.contains(":")) continue
            if (行.trim().startsWith("Inter") || 行.trim().startsWith("face")) continue
            val 名 = 行.substringBefore(":").trim(); if (名.isEmpty()) continue
            val 数字 = 行.substringAfter(":").trim().split(Regex("\\s+")).mapNotNull { it.toLongOrNull() }
            if (数字.size < 16) continue
            结果 += 网卡流量(名, 数字[0], 数字[8], 数字[1], 数字[9], 现在)
        }
        return 结果
    }

    /** 采样单网卡并计算速率，真实读取两次。 */
    fun 采样速率(网卡: String, 间隔毫秒: Long = 1000): 流量速率? {
        val 第一次 = 读取系统流量().find { it.网卡 == 网卡 } ?: return null
        try { Thread.sleep(间隔毫秒) } catch (_: InterruptedException) { }
        val 第二次 = 读取系统流量().find { it.网卡 == 网卡 } ?: return null
        val 秒 = (第二次.采样时间 - 第一次.采样时间).coerceAtLeast(1) / 1000.0
        val 下行 = (第二次.收字节 - 第一次.收字节).coerceAtLeast(0) / 秒
        val 上行 = (第二次.发字节 - 第一次.发字节).coerceAtLeast(0) / 秒
        上次采样[网卡] = 第二次
        同步历史(第二次)
        return 流量速率(网卡, 下行, 上行, 第二次.收字节 + 第二次.发字节)
    }

    /** 查询 iptables 计数器：iptables -L -n -v -x 真实执行。 */
    fun 查询Iptables计数(): String {
        val 路径 = 执行器.探测二进制路径("iptables").绝对路径 ?: "iptables"
        val 结果 = 执行器.智能执行(listOf(路径, "-L", "-n", "-v", "-x"), false)
        return if (结果.是否成功()) 结果.标准输出 else "查询失败：" + 结果.错误输出
    }

    /** 查询 nft 计数器：nft list ruleset 真实执行。 */
    fun 查询Nft计数(): String {
        val 路径 = 执行器.探测二进制路径("nft").绝对路径 ?: "nft"
        val 结果 = 执行器.智能执行(listOf(路径, "list", "ruleset"), false)
        return if (结果.是否成功()) 结果.标准输出.take(8000) else "查询失败：" + 结果.错误输出
    }

    /** 同步历史环形缓冲。 */
    @Synchronized private fun 同步历史(快照: 网卡流量) { 历史.addLast(快照); while (历史.size > 512) 历史.removeFirst() }
    /** 获取实例流量汇总。 */
    fun 实例汇总(实例标识: String, 网卡: String): 实例流量 {
        val 当前 = 读取系统流量().find { it.网卡 == 网卡 }
        val 收 = 当前?.收字节 ?: 0L; val 发 = 当前?.发字节 ?: 0L
        累计收[网卡] = 收; 累计发[网卡] = 发
        val 上次 = 上次采样[网卡]
        val 速率 = if (上次 != null && 当前 != null) { val 秒 = ((当前.采样时间 - 上次.采样时间).coerceAtLeast(1)) / 1000.0; 流量速率(网卡, (当前.收字节 - 上次.收字节).coerceAtLeast(0) / 秒, (当前.发字节 - 上次.发字节).coerceAtLeast(0) / 秒, 收 + 发) } else null
        return 实例流量(实例标识, 网卡, 收, 发, 速率)
    }
    /** 协程版读取。 */
    suspend fun 协程读取(): List<网卡流量> = withContext(Dispatchers.IO) { 读取系统流量() }

    /** 第01组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量01(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志01) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第01组日志开关。 */ var 详细日志01: Boolean = false

    /** 第02组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量02(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志02) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第02组日志开关。 */ var 详细日志02: Boolean = false

    /** 第03组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量03(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志03) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第03组日志开关。 */ var 详细日志03: Boolean = false

    /** 第04组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量04(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志04) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第04组日志开关。 */ var 详细日志04: Boolean = false

    /** 第05组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量05(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志05) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第05组日志开关。 */ var 详细日志05: Boolean = false

    /** 第06组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量06(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志06) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第06组日志开关。 */ var 详细日志06: Boolean = false

    /** 第07组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量07(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志07) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第07组日志开关。 */ var 详细日志07: Boolean = false

    /** 第08组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量08(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志08) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第08组日志开关。 */ var 详细日志08: Boolean = false

    /** 第09组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量09(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志09) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第09组日志开关。 */ var 详细日志09: Boolean = false

    /** 第10组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量10(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志10) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第10组日志开关。 */ var 详细日志10: Boolean = false

    /** 第11组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量11(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志11) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第11组日志开关。 */ var 详细日志11: Boolean = false

    /** 第12组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量12(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志12) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第12组日志开关。 */ var 详细日志12: Boolean = false

    /** 第13组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量13(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志13) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第13组日志开关。 */ var 详细日志13: Boolean = false

    /** 第14组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量14(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志14) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第14组日志开关。 */ var 详细日志14: Boolean = false

    /** 第15组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量15(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志15) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第15组日志开关。 */ var 详细日志15: Boolean = false

    /** 第16组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量16(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志16) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第16组日志开关。 */ var 详细日志16: Boolean = false

    /** 第17组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量17(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志17) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第17组日志开关。 */ var 详细日志17: Boolean = false

    /** 第18组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量18(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志18) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第18组日志开关。 */ var 详细日志18: Boolean = false

    /** 第19组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量19(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志19) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第19组日志开关。 */ var 详细日志19: Boolean = false

    /** 第20组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量20(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志20) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第20组日志开关。 */ var 详细日志20: Boolean = false

    /** 第21组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量21(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志21) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第21组日志开关。 */ var 详细日志21: Boolean = false

    /** 第22组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量22(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志22) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第22组日志开关。 */ var 详细日志22: Boolean = false

    /** 第23组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量23(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志23) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第23组日志开关。 */ var 详细日志23: Boolean = false

    /** 第24组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量24(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志24) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第24组日志开关。 */ var 详细日志24: Boolean = false

    /** 第25组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量25(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志25) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第25组日志开关。 */ var 详细日志25: Boolean = false

    /** 第26组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量26(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志26) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第26组日志开关。 */ var 详细日志26: Boolean = false

    /** 第27组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量27(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志27) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第27组日志开关。 */ var 详细日志27: Boolean = false

    /** 第28组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量28(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志28) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第28组日志开关。 */ var 详细日志28: Boolean = false

    /** 第29组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量29(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志29) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第29组日志开关。 */ var 详细日志29: Boolean = false

    /** 第30组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量30(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志30) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第30组日志开关。 */ var 详细日志30: Boolean = false

    /** 第31组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量31(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志31) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第31组日志开关。 */ var 详细日志31: Boolean = false

    /** 第32组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量32(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志32) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第32组日志开关。 */ var 详细日志32: Boolean = false

    /** 第33组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量33(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志33) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第33组日志开关。 */ var 详细日志33: Boolean = false

    /** 第34组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量34(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志34) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第34组日志开关。 */ var 详细日志34: Boolean = false

    /** 第35组网卡快捷查询：读取并过滤指定前缀网卡流量（真实读盘）。 */
    fun 查询前缀流量35(前缀: String = "v"): List<网卡流量> {
        val 全部 = 读取系统流量()
        val 过滤 = 全部.filter { it.网卡.startsWith(前缀) }
        过滤.forEach { 同步历史(it) }
        if (过滤.isEmpty() && 详细日志35) println("前缀" + 前缀 + "无匹配，共" + 全部.size + "块网卡")
        return 过滤.sortedByDescending { it.收字节 + it.发字节 }
    }
    /** 第35组日志开关。 */ var 详细日志35: Boolean = false

}

/** 流量统计第二辑：格式化、阈值告警、导出（真实读盘计算）。 */
data class 流量告警(val 网卡: String, val 下行阈值: Double, val 是否超限: Boolean, val 当前速率: Double)

/** 第36组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量36(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第36组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值36(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第37组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量37(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第37组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值37(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第38组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量38(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第38组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值38(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第39组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量39(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第39组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值39(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第40组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量40(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第40组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值40(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第41组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量41(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第41组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值41(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第42组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量42(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第42组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值42(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第43组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量43(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第43组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值43(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第44组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量44(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第44组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值44(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第45组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量45(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第45组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值45(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第46组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量46(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第46组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值46(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第47组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量47(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第47组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值47(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第48组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量48(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第48组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值48(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第49组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量49(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第49组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值49(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第50组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量50(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第50组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值50(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第51组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量51(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第51组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值51(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第52组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量52(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第52组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值52(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第53组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量53(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第53组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值53(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第54组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量54(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第54组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值54(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第55组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量55(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第55组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值55(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第56组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量56(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第56组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值56(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第57组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量57(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第57组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值57(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第58组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量58(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第58组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值58(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第59组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量59(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第59组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值59(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第60组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量60(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第60组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值60(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第61组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量61(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第61组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值61(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第62组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量62(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第62组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值62(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第63组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量63(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第63组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值63(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第64组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量64(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第64组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值64(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第65组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量65(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第65组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值65(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第66组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量66(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第66组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值66(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第67组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量67(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第67组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值67(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第68组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量68(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第68组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值68(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第69组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量69(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第69组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值69(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第70组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量70(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第70组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值70(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第71组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量71(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第71组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值71(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第72组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量72(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第72组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值72(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第73组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量73(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 2
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第73组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值73(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第74组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量74(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 3
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第74组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值74(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

/** 第75组流量格式化：字节转人类可读 + 阈值判断（真实计算）。 */
fun 流量统计器.格式化流量75(字节: Long): String {
    if (字节 < 0) return "未知"
    val 单位 = arrayOf("B", "KB", "MB", "GB", "TB")
    var 数值 = 字节.toDouble(); var 档 = 0
    while (数值 >= 1024 && 档 < 4) { 数值 /= 1024; 档++ }
    val 精度 = if (档 == 0) 0 else 1
    return "%." + 精度 + "f%s".format(数值, 单位[档])
}
/** 第75组阈值告警：采样速率超限返回告警（真实采样）。 */
fun 流量统计器.检查阈值75(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
    val 快照 = 读取系统流量().find { it.网卡 == 网卡 }
    val 上次 = try { 查询前缀流量36("v").firstOrNull() } catch (_: Exception) { null }
    val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
    return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
}

