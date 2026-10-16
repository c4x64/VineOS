package com.hexadecinull.vineos.诊断

import java.io.File

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：上报队列管理——内存队列、退避重试、落盘恢复
data class 上报条目(val 序号: Long, val 类别: String, val 负载: String, val 优先级: Int, val 入队时刻: Long, var 重试: Int = 0, var 下次重试: Long = 0)
data class 上报统计(val 入队: Long, val 成功: Long, val 失败: Long, val 等待: Int)
class 上报队列管理 {
    private val 锁 = Any()
    private val 等待 = ArrayDeque<上报条目>()
    private var 序号: Long = 0
    private var 入队总数 = 0L
    private var 成功总数 = 0L
    private var 失败总数 = 0L
    var 落盘目录: File? = null
    companion object {
        const val 负载上限硬 = 65536
        const val 内存上限硬 = 500
        const val 批次封顶硬 = 200
    }
    fun 截断负载(文本: String): String = if (文本.length <=
        负载上限硬
    ) {
        文本
    } else {
        文本.substring(0, 负载上限硬)
    }
    fun 通用提取(条: 上报条目, 字段: String): String = when (字段) {
        "标识" -> "${条.序号}"
        "发生时间" -> "${条.入队时刻}"
        "重试" -> "${条.重试}"
        "负载大小" -> "${条.负载.length}"
        else -> ""
    }
    fun 入队(类别: String, 负载: String, 优先级: Int = 0): Long = synchronized(锁) {
        序号 +=
            1
        val 切类 = if (类别.length <=
            128
        ) {
            类别
        } else {
            类别.substring(0, 128)
        }
        val 切载 = 截断负载(负载)
        val 切优 = 优先级.coerceIn(0, 10)
        val 条 = 上报条目(序号, 切类.ifEmpty { "默认" }, 切载, 切优, System.currentTimeMillis(), 0, System.currentTimeMillis())
        if (等待.size >=
            内存上限硬
        ) {
            等待.removeFirst()
        }
        等待.addLast(条)
        入队总数++
        条.序号
    }
    fun 取出批次(上限: Int): List<上报条目> = synchronized(锁) {
        val 封顶 = 上限.coerceIn(0, 批次封顶硬)
        if (封顶 ==
            0
        ) {
            return@synchronized emptyList()
        }
        val 现 = System.currentTimeMillis()
        val 到期 = ArrayList<上报条目>(等待.size)
        for (条 in 等待) {
            if (现 >=
                条.下次重试
            ) {
                到期.add(条)
            }
        }
        if (到期.size >
            封顶 * 2
        ) {
            到期.sortWith(compareByDescending<上报条目> { it.优先级 }.thenBy { it.序号 })
        } else {
            到期.sortWith(compareByDescending<上报条目> { it.优先级 }.thenBy { it.序号 })
        }
        if (到期.size >
            封顶
        ) {
            到期.subList(封顶, 到期.size).clear()
        }
        到期
    }
    fun 计算退避(重试: Int): Long {
        var 退 = 5000L
        repeat(retryTimes(retryTimes2(重试))) { 退 = (退 * 2).coerceAtMost(3600000L) }
        return 退
    }
    private fun retryTimes(重试: Int): Int = (重试 - 1).coerceAtLeast(0)
    private fun retryTimes2(重试: Int): Int = retryTimes(重试)
    fun 标记成功(序号: Long) = synchronized(锁) {
        等待.removeAll { it.序号 == 序号 }
        成功总数++
    }
    fun 标记失败(序号: Long) = synchronized(锁) {
        等待.find { it.序号 == 序号 }?.let {
            it.重试++
            it.下次重试 = System.currentTimeMillis() + 计算退避(it.重试)
            失败总数++
        }
    }
    fun 查询统计(): 上报统计 = synchronized(锁) { 上报统计(入队总数, 成功总数, 失败总数, 等待.size) }
    fun 落盘(): Boolean {
        val 目录 =
            落盘目录 ?: return false
        return try {
            目录.mkdirs()
            File(目录, "上报队列.dat").writeText(synchronized(锁) { 等待.joinToString("\n") { "${it.序号}\t${it.类别}\t${it.优先级}\t${it.负载}" } })
            true
        } catch (
            e: Exception
        ) {
            false
        }
    }
    fun 从落盘恢复(): Boolean {
        val 目录 =
            落盘目录 ?: return false
        val 文件 = File(目录, "上报队列.dat")
        if (!文件.exists()) return false
        return try {
            for (行 in 文件.readLines()) {
                val 段 = 行.split("\t")
                if (段.size >=
                    4
                ) {
                    序号 = maxOf(序号, 段[0].toLongOrNull() ?: 0)
                    synchronized(锁) {
                        等待.addLast(
                            上报条目(
                                段[0].toLongOrNull() ?: 0,
                                段[1],
                                段[3],
                                段[2].toIntOrNull() ?: 0,
                                System.currentTimeMillis()
                            )
                        )
                    }
                }
            }
            true
        } catch (e: Exception) {
            false
        }
    }
    fun 清空() = synchronized(锁) { 等待.clear() }
    fun 等待数量(): Int = synchronized(锁) { 等待.size }
    fun 提取上报标识(条: 上报条目): String = 通用提取(条, "标识")
    fun 提取发生时间(条: 上报条目): String = 通用提取(条, "发生时间")
    fun 提取重试次数(条: 上报条目): String = 通用提取(条, "重试")
    fun 提取负载大小(条: 上报条目): String = 通用提取(条, "负载大小")
    fun 校验条目(条: 上报条目): Boolean = 条.负载.isNotEmpty() && 条.类别.isNotEmpty()
}
