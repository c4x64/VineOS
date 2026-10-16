package com.hexadecinull.vineos.诊断

import java.io.File
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：内存快照器——meminfo/smaps 解析、趋势记录、泄漏判定
data class 内存快照(val 时刻: Long, val 总量KB: Long, val 可用KB: Long, val 常驻KB: Long, val 虚拟KB: Long, val 比例集KB: Long, val 使用率: Double) {
    fun 转为日志行(): String = "时刻=$时刻 总=${总量KB}KB 可用=${可用KB}KB 常驻=${常驻KB}KB 使用率=${"%.1f".format(使用率 * 100)}%"
    fun 是否有效(): Boolean = 总量KB > 0
}
data class 映射段(val 路径: String, val 大小KB: Long, val 常驻KB: Long)
data class 泄漏判定(val 疑似泄漏: Boolean, val 每小时增长KB: Double, val 置信度: Double, val 原因: String)
class 内存快照器 {
    private val 锁 = Any()
    private val 趋势 = ArrayDeque<内存快照>()
    fun 读取文件(路径: String): String = try {
        File(路径).readText()
    } catch (e: Exception) {
        ""
    }
    fun 解析千字节(行: String): Long {
        val 冒号 = 行.indexOf(':')
        if (冒号 <
            0
        ) {
            return -1
        }
        var 数值 = 0L
        var 开始 = false
        for (字符 in 行.substring(冒号 + 1)) {
            if (字符 in
                '0'..'9'
            ) {
                开始 = true
                数值 = 数值 * 10 + (字符 - '0')
            } else if (开始) {
                break
            }
        }
        return if (开始) 数值 else -1
    }
    companion object {
        val 字段表硬: List<String> =
            listOf("MemTotal:", "MemFree:", "MemAvailable:", "Buffers:", "Cached:", "SwapTotal:", "SwapFree:", "Dirty:", "AnonPages:", "Mapped:", "Shmem:", "KernelStack:")
        const val 趋势上限硬 = 720
        const val 映射上限硬 = 500
        const val 行扫描上限硬 = 60000
    }
    fun 通用解析字段(内容: String, 前缀: String): Long {
        for (行 in 内容.lines()) if (行.startsWith(前缀)) return 解析千字节(行)
        return -1
    }
    fun 通用校验合理(值: Long): Boolean = 值 >= 0 && 值 < 64L * 1024 * 1024 * 1024
    fun 采集内存快照(): 内存快照 {
        val 表 = mutableMapOf<String, Long>()
        for (行 in 读取文件("/proc/meminfo").lines()) {
            val 键 = 行.substringBefore(':').trim()
            val 值 = 解析千字节(行)
            if (值 >= 0) 表[键] = 值
        }
        val 总 = 表["MemTotal"] ?: 0L
        val 可用 = 表["MemAvailable"] ?: 表["MemFree"] ?: 0L
        var 常驻 = 0L
        var 虚拟 = 0L
        for (行 in 读取文件("/proc/self/status").lines()) {
            if (行.startsWith("VmRSS:")) 常驻 = 解析千字节(行)
            if (行.startsWith("VmSize:")) 虚拟 = 解析千字节(行)
        }
        var 比例 = 0L
        var 行数 = 0
        for (行 in 读取文件("/proc/self/smaps").lines()) {
            if (++行数 >
                行扫描上限硬
            ) {
                break
            }
            if (行.startsWith("Pss:")) {
                val 值 = 解析千字节(行)
                if (值 > 0) 比例 += 值
            }
        }
        val 使用率 = if (总 > 0) (总 - 可用).coerceAtLeast(0).toDouble() / 总.toDouble() else 0.0
        val 快照 = 内存快照(System.currentTimeMillis(), 总, 可用, 常驻, 虚拟, 比例, 使用率)
        synchronized(锁) {
            趋势.addLast(快照)
            while (趋势.size > 720) 趋势.removeFirst()
        }
        return 快照
    }
    fun 解析映射段(上限: Int = 500): List<映射段> {
        val 结果 = mutableListOf<映射段>()
        for (行 in 读取文件("/proc/self/maps").lines()) {
            if (结果.size >=
                上限
            ) {
                break
            }
            val 段 = 行.split(Regex("\\s+"))
            if (段.isEmpty()) continue
            val 范围 = 段[0]
            if (!范围.contains("-")) continue
            try {
                val 起 = 范围.substringBefore("-").toLong(16)
                val 止 = 范围.substringAfter("-").toLong(16)
                val 大小 = (
                    (
                        止 -
                            起
                        ) /
                        1024
                    ).coerceAtLeast(0)
                val 路径 = if (段.size >= 6) 段.last() else "[匿名]"
                结果.add(映射段(路径, 大小, 0))
            } catch (e: Exception) { }
        }
        return 结果
    }
    fun 判定泄漏趋势(): 泄漏判定 {
        val 复制 = synchronized(锁) { 趋势.toList() }
        if (复制.size < 10) return 泄漏判定(false, 0.0, 0.0, "样本不足")
        val 首 = 复制.first()
        val 尾 = 复制.last()
        val 时长 = 尾.时刻 - 首.时刻
        if (时长 <= 60000) return 泄漏判定(false, 0.0, 0.0, "窗口太短")
        val 增长 = (尾.常驻KB - 首.常驻KB).toDouble()
        val 每小时 = 增长 / (时长.toDouble() / 3600000.0)
        var 上升 = 0
        for (i in 1 until 复制.size) if (复制[i].常驻KB >= 复制[i - 1].常驻KB) 上升++
        val 单调 = 上升.toDouble() / (复制.size - 1).toDouble()
        return if (每小时 > 50 * 1024 && 单调 > 0.7) 泄漏判定(true, 每小时, 单调, "常驻持续增长") else 泄漏判定(false, 每小时, 单调, "未见明显泄漏")
    }
    fun 生成内存报告(): String {
        val 快照 = 采集内存快照()
        val 判定 = 判定泄漏趋势()
        return 快照.转为日志行() + "\n泄漏=" + 判定.原因
    }
    fun 落盘快照(文件: File): Boolean = try {
        文件.appendText(采集内存快照().转为日志行() + "\n")
        true
    } catch (e: Exception) {
        false
    }
    fun 趋势点数(): Int = synchronized(锁) { 趋势.size }
    fun 清空趋势() = synchronized(锁) { 趋势.clear() }
}

// 解析内存总量（MemTotal）：从 meminfo 文本提取。
fun 内存快照器.解析内存总量(内容: String): Long = 通用解析字段(内容, "MemTotal:")

// 校验内存总量合理性：非负且上限保护。
fun 内存快照器.校验内存总量合理(值: Long): Boolean = 通用校验合理(值)

// 解析内存空闲（MemFree）：从 meminfo 文本提取。
fun 内存快照器.解析内存空闲(内容: String): Long = 通用解析字段(内容, "MemFree:")

// 校验内存空闲合理性：非负且上限保护。
fun 内存快照器.校验内存空闲合理(值: Long): Boolean = 通用校验合理(值)

// 解析内存可用（MemAvailable）：从 meminfo 文本提取。
fun 内存快照器.解析内存可用(内容: String): Long = 通用解析字段(内容, "MemAvailable:")

// 校验内存可用合理性：非负且上限保护。
fun 内存快照器.校验内存可用合理(值: Long): Boolean = 通用校验合理(值)

// 解析缓冲（Buffers）：从 meminfo 文本提取。
fun 内存快照器.解析缓冲(内容: String): Long = 通用解析字段(内容, "Buffers:")

// 校验缓冲合理性：非负且上限保护。
fun 内存快照器.校验缓冲合理(值: Long): Boolean = 通用校验合理(值)

// 解析缓存（Cached）：从 meminfo 文本提取。
fun 内存快照器.解析缓存(内容: String): Long = 通用解析字段(内容, "Cached:")

// 校验缓存合理性：非负且上限保护。
fun 内存快照器.校验缓存合理(值: Long): Boolean = 通用校验合理(值)

// 解析交换总量（SwapTotal）：从 meminfo 文本提取。
fun 内存快照器.解析交换总量(内容: String): Long = 通用解析字段(内容, "SwapTotal:")

// 校验交换总量合理性：非负且上限保护。
fun 内存快照器.校验交换总量合理(值: Long): Boolean = 通用校验合理(值)

// 解析交换空闲（SwapFree）：从 meminfo 文本提取。
fun 内存快照器.解析交换空闲(内容: String): Long = 通用解析字段(内容, "SwapFree:")

// 校验交换空闲合理性：非负且上限保护。
fun 内存快照器.校验交换空闲合理(值: Long): Boolean = 通用校验合理(值)

// 解析脏页（Dirty）：从 meminfo 文本提取。
fun 内存快照器.解析脏页(内容: String): Long = 通用解析字段(内容, "Dirty:")

// 校验脏页合理性：非负且上限保护。
fun 内存快照器.校验脏页合理(值: Long): Boolean = 通用校验合理(值)

// 解析匿名页（AnonPages）：从 meminfo 文本提取。
fun 内存快照器.解析匿名页(内容: String): Long = 通用解析字段(内容, "AnonPages:")

// 校验匿名页合理性：非负且上限保护。
fun 内存快照器.校验匿名页合理(值: Long): Boolean = 通用校验合理(值)

// 解析映射文件（Mapped）：从 meminfo 文本提取。
fun 内存快照器.解析映射文件(内容: String): Long = 通用解析字段(内容, "Mapped:")

// 校验映射文件合理性：非负且上限保护。
fun 内存快照器.校验映射文件合理(值: Long): Boolean = 通用校验合理(值)

// 解析共享内存（Shmem）：从 meminfo 文本提取。
fun 内存快照器.解析共享内存(内容: String): Long = 通用解析字段(内容, "Shmem:")

// 校验共享内存合理性：非负且上限保护。
fun 内存快照器.校验共享内存合理(值: Long): Boolean = 通用校验合理(值)

// 解析内核栈（KernelStack）：从 meminfo 文本提取。
fun 内存快照器.解析内核栈(内容: String): Long = 通用解析字段(内容, "KernelStack:")

// 校验内核栈合理性：非负且上限保护。
fun 内存快照器.校验内核栈合理(值: Long): Boolean = 通用校验合理(值)
