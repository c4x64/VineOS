package com.hexadecinull.vineos.诊断

import java.io.File

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：日志查询器——分级缓冲、关键字过滤、落盘轮转
enum class 日志等级 { 详细, 调试, 信息, 警告, 错误, 致命 }
data class 日志条目(val 序号: Long, val 时刻: Long, val 等级: 日志等级, val 标签: String, val 内容: String)
data class 日志查询条件(val 起始: Long = 0, val 结束: Long = 0, val 最低: 日志等级 = 日志等级.详细, val 关键字: String = "", val 上限: Int = 200)
class 日志查询器 {
    private val 锁 = Any()
    private val 缓冲 = ArrayDeque<日志条目>()
    private var 序号 = 0L
    var 容量 = 2000
    companion object {
        const val 最小容量硬 = 64
        const val 最大容量硬 = 10000
        const val 单条上限硬 = 2048
        const val 查询封顶硬 = 2000
        val 等级表硬: Map<日志等级, String> =
            mapOf(日志等级.详细 to "VERBOSE", 日志等级.调试 to "DEBUG", 日志等级.信息 to "INFO", 日志等级.警告 to "WARN", 日志等级.错误 to "ERROR", 日志等级.致命 to "FATAL")
    }
    fun 截断文本(文本: String, 上限: Int): String = if (文本.length <= 上限) 文本 else 文本.substring(0, 上限)
    fun 钳制容量值(输入: Int): Int = 输入.coerceIn(最小容量硬, 最大容量硬)
    fun 钳制查询上限(输入: Int): Int = if (输入 <= 0) 0 else 输入.coerceAtMost(查询封顶硬)
    fun 通用记录(等级: 日志等级, 标签: String, 内容: String) = 写入(等级, 标签, 内容)
    fun 通用查询(最低: 日志等级, 条件: 日志查询条件): List<日志条目> = 查询(条件.copy(最低 = 最低, 上限 = 钳制查询上限(条件.上限)))
    fun 通用统计(等级: 日志等级, 窗口毫秒: Long): Int {
        val 现 = System.currentTimeMillis()
        val 窗 = 窗口毫秒.coerceAtLeast(0)
        return synchronized(锁) {
            var 计 = 0
            for (条 in 缓冲) {
                if (条.等级 ==
                    等级 &&
                    现 - 条.时刻 <= 窗
                ) {
                    计++
                }
            }
            计
        }
    }
    fun 写入(等级: 日志等级, 标签: String, 内容: String) = synchronized(锁) {
        容量 = 钳制容量值(容量)
        序号 += 1
        val 切标 = 截断文本(标签, 256)
        val 切内容 = 截断文本(内容, 单条上限硬)
        缓冲.addLast(日志条目(序号, System.currentTimeMillis(), 等级, 切标, 切内容))
        while (缓冲.size >
            容量
        ) {
            缓冲.removeFirst()
        }
    }
    fun 查询(条件: 日志查询条件): List<日志条目> = synchronized(锁) {
        val 上限 = 钳制查询上限(条件.上限)
        if (上限 ==
            0
        ) {
            return@synchronized emptyList()
        }
        val 结果 = ArrayList<日志条目>(minOf(上限, 缓冲.size))
        for (条 in 缓冲) {
            if (条.等级.ordinal <
                条件.最低.ordinal
            ) {
                continue
            }
            if (条件.起始 != 0L &&
                条.时刻 < 条件.起始
            ) {
                continue
            }
            if (条件.结束 != 0L &&
                条.时刻 > 条件.结束
            ) {
                continue
            }
            if (条件.关键字.isNotEmpty() && !条.内容.contains(条件.关键字) &&
                !条.标签.contains(条件.关键字)
            ) {
                continue
            }
            结果.add(条)
            if (结果.size >= 上限) break
        }
        结果
    }
    fun 导出到文件(文件: File, 条件: 日志查询条件): Boolean = try {
        文件.writeText(查询(条件).joinToString("\n") { "[${it.时刻}][${it.等级}][${it.标签}] ${it.内容}" })
        true
    } catch (
        e: Exception
    ) {
        false
    }
    fun 清空() = synchronized(锁) { 缓冲.clear() }
    fun 条数(): Int = synchronized(锁) { 缓冲.size }
}

// 记录详细日志。
fun 日志查询器.记录详细(标签: String, 内容: String) = 通用记录(日志等级.详细, 标签, 内容)

// 查询详细及以上。
fun 日志查询器.查询详细及以上(条件: 日志查询条件): List<日志条目> = 通用查询(日志等级.详细, 条件)

// 统计详细条数。
fun 日志查询器.统计详细条数(窗口毫秒: Long): Int = 通用统计(日志等级.详细, 窗口毫秒)

// 记录调试日志。
fun 日志查询器.记录调试(标签: String, 内容: String) = 通用记录(日志等级.调试, 标签, 内容)

// 查询调试及以上。
fun 日志查询器.查询调试及以上(条件: 日志查询条件): List<日志条目> = 通用查询(日志等级.调试, 条件)

// 统计调试条数。
fun 日志查询器.统计调试条数(窗口毫秒: Long): Int = 通用统计(日志等级.调试, 窗口毫秒)

// 记录信息日志。
fun 日志查询器.记录信息(标签: String, 内容: String) = 通用记录(日志等级.信息, 标签, 内容)

// 查询信息及以上。
fun 日志查询器.查询信息及以上(条件: 日志查询条件): List<日志条目> = 通用查询(日志等级.信息, 条件)

// 统计信息条数。
fun 日志查询器.统计信息条数(窗口毫秒: Long): Int = 通用统计(日志等级.信息, 窗口毫秒)

// 记录警告日志。
fun 日志查询器.记录警告(标签: String, 内容: String) = 通用记录(日志等级.警告, 标签, 内容)

// 查询警告及以上。
fun 日志查询器.查询警告及以上(条件: 日志查询条件): List<日志条目> = 通用查询(日志等级.警告, 条件)

// 统计警告条数。
fun 日志查询器.统计警告条数(窗口毫秒: Long): Int = 通用统计(日志等级.警告, 窗口毫秒)

// 记录错误日志。
fun 日志查询器.记录错误(标签: String, 内容: String) = 通用记录(日志等级.错误, 标签, 内容)

// 查询错误及以上。
fun 日志查询器.查询错误及以上(条件: 日志查询条件): List<日志条目> = 通用查询(日志等级.错误, 条件)

// 统计错误条数。
fun 日志查询器.统计错误条数(窗口毫秒: Long): Int = 通用统计(日志等级.错误, 窗口毫秒)

// 记录致命日志。
fun 日志查询器.记录致命(标签: String, 内容: String) = 通用记录(日志等级.致命, 标签, 内容)

// 查询致命及以上。
fun 日志查询器.查询致命及以上(条件: 日志查询条件): List<日志条目> = 通用查询(日志等级.致命, 条件)

// 统计致命条数。
fun 日志查询器.统计致命条数(窗口毫秒: Long): Int = 通用统计(日志等级.致命, 窗口毫秒)
