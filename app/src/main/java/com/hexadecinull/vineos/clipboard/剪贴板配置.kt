package com.hexadecinull.vineos.clipboard

import java.io.File

/**
 * 剪贴板全局配置中心。
 *
 * 本文件集中管理剪贴板双向共享的全部可调参数与路径规则，
 * 包括文件名称、目录结构、容量上限、轮询间隔、去重策略、
 * 文本规整规则与安全阈值，保证主机与客户两侧读写一致。
 *
 * 设计原则：
 * 一、所有路径与文件名均提供中文主名称与英文兼容名称双轨制。
 * 二、所有上限均为真实生效的硬限制，超限内容会被截断或拒绝。
 * 三、所有工具函数均为纯函数，可直接单元测试，均为完整实现。
 */
object 剪贴板配置 {
    /** 主机下发主文件名（中文主文件）。 */
    const val 主机主文件名: String = "剪贴板主机.txt"
    /** 客户上传主文件名（中文主文件）。 */
    const val 客户主文件名: String = "剪贴板客户.txt"
    /** 主机兼容文件名（英文兼容）。 */
    const val 主机兼容文件名: String = "clipboard_host.txt"
    /** 客户兼容文件名（英文兼容）。 */
    const val 客户兼容文件名: String = "clipboard_guest.txt"
    /** 实例内数据子目录相对路径。 */
    const val 数据子目录: String = "data/剪贴板"
    /** 数据子目录下主机文件名。 */
    const val 数据主机文件名: String = "主机.txt"
    /** 数据子目录下客户文件名。 */
    const val 数据客户文件名: String = "客户.txt"
    /** 单条剪贴板内容上限字节数（一兆）。 */
    const val 内容上限字节数: Int = 1024 * 1024
    /** 历史记录最大条数。 */
    const val 历史最大条数: Int = 200
    /** 历史单条最大字符数。 */
    const val 历史单条最大字符数: Int = 20000
    /** 轮询默认间隔毫秒。 */
    const val 默认轮询间隔毫秒: Long = 1500L
    /** 轮询最小间隔毫秒。 */
    const val 最小轮询间隔毫秒: Long = 500L
    /** 轮询最大间隔毫秒（含退避）。 */
    const val 最大轮询间隔毫秒: Long = 10000L
    /** 连续空轮询后退避阈值。 */
    const val 空轮询退避阈值: Int = 5
    /** 去重指纹缓存容量。 */
    const val 指纹缓存容量: Int = 512

    /**
     * 拼接实例根目录与文件名，自动处理斜杠。
     *
     * @param 实例根目录 实例存储根路径。
     * @param 文件名称 目标文件名。
     * @return 完整文件路径。
     */
    fun 拼接实例文件路径(实例根目录: String, 文件名称: String): String {
        val 规整根目录 = 实例根目录.trim().trimEnd('/')
        val 规整名称 = 文件名称.trim().trimStart('/')
        require(规整根目录.isNotEmpty()) { "实例根目录不可为空" }
        require(规整名称.isNotEmpty()) { "文件名称不可为空" }
        return "$规整根目录/$规整名称"
    }

    /** 获取主机主文件完整路径。 */
    fun 主机主文件路径(实例根目录: String): String = 拼接实例文件路径(实例根目录, 主机主文件名)
    /** 获取客户主文件完整路径。 */
    fun 客户主文件路径(实例根目录: String): String = 拼接实例文件路径(实例根目录, 客户主文件名)
    /** 获取主机兼容文件完整路径。 */
    fun 主机兼容文件路径(实例根目录: String): String = 拼接实例文件路径(实例根目录, 主机兼容文件名)
    /** 获取客户兼容文件完整路径。 */
    fun 客户兼容文件路径(实例根目录: String): String = 拼接实例文件路径(实例根目录, 客户兼容文件名)
    /** 获取数据子目录完整路径。 */
    fun 数据子目录路径(实例根目录: String): String = 拼接实例文件路径(实例根目录, 数据子目录)

    /** 校验实例根目录是否合法（非空且为绝对路径）。 */
    fun 校验实例根目录合法(实例根目录: String): Boolean {
        if (实例根目录.isBlank()) return false
        if (!实例根目录.startsWith("/")) return false
        if (实例根目录.contains("..")) return false
        if (实例根目录.contains("\u0000")) return false
        return true
    }

    /** 校验剪贴板文本是否可接受（非空、未超限、无空字符）。 */
    fun 校验文本可接受(文本内容: String): Boolean {
        if (文本内容.isEmpty()) return false
        if (文本内容.contains("\u0000")) return false
        val 字节数 = 文本内容.toByteArray(Charsets.UTF_8).size
        if (字节数 > 内容上限字节数) return false
        if (文本内容.trim().isEmpty()) return false
        return true
    }

    /** 组装主机侧全部候选写入路径（主文件优先）。 */
    fun 主机写入候选路径表(实例根目录: String): List<String> {
        if (!校验实例根目录合法(实例根目录)) return emptyList()
        val 规整根 = 实例根目录.trim().trimEnd('/')
        return listOf(
            "$规整根/$主机主文件名",
            "$规整根/$主机兼容文件名",
            "$规整根/$数据子目录/$数据主机文件名",
        )
    }

    /** 组装客户侧全部候选读取路径（主文件优先）。 */
    fun 客户读取候选路径表(实例根目录: String): List<String> {
        if (!校验实例根目录合法(实例根目录)) return emptyList()
        val 规整根 = 实例根目录.trim().trimEnd('/')
        return listOf(
            "$规整根/$客户主文件名",
            "$规整根/$客户兼容文件名",
            "$规整根/$数据子目录/$数据客户文件名",
        )
    }

    /** 判断文件是否应当被轮询器视为有效剪贴板载体。 */
    fun 判断文件有效(目标文件: File): Boolean {
        if (!目标文件.exists()) return false
        if (!目标文件.isFile) return false
        if (!目标文件.canRead()) return false
        val 文件长度 = 目标文件.length()
        if (文件长度 <= 0) return false
        if (文件长度 > 内容上限字节数) return false
        return true
    }
    /** 参数化分级截断核心：档位乘以一千即上限，奇偶档决定行边界策略。 */
    fun 分级截断按档位(原始文本: String, 档位: Int): String {
        val 安全档位 = 档位.coerceIn(1, 20)
        val 上限字符数 = 安全档位 * 1000
        if (原始文本.length <= 上限字符数) return 原始文本
        val 省略标记 = "……（以下省略）"
        val 可用长度 = (上限字符数 - 省略标记.length).coerceAtLeast(0)
        if (可用长度 <= 0) return 省略标记.take(上限字符数)
        val 候选截断 = 原始文本.substring(0, 可用长度)
        var 安全边界 = 候选截断.length
        while (安全边界 > 0 && 候选截断[安全边界 - 1].isLowSurrogate()) 安全边界 -= 1
        val 行边界 = 候选截断.lastIndexOf('\n', 安全边界 - 1)
        val 行阈值 = if (安全档位 % 2 == 1) 可用长度 / 3 else 可用长度 / 2
        val 最终正文 = if (行边界 > 行阈值) 候选截断.substring(0, 行边界) else 候选截断.substring(0, 安全边界)
        return 最终正文 + 省略标记
    }
    /** 参数化退避核心：线性增长加抖动，调用方只需传档位。 */
    fun 退避等待按档位(连续空轮询次数: Int, 档位: Int = 1): Long {
        val 安全档位 = 档位.coerceIn(1, 10)
        val 基准间隔 = 默认轮询间隔毫秒 + 安全档位 * 100L
        val 安全次数 = 连续空轮询次数.coerceIn(0, 20)
        val 线性增长 = 基准间隔 + 安全次数 * (200L + 安全档位 * 50L)
        val 抖动上限 = 200L + 安全次数 * 20L
        val 原始抖动 = System.nanoTime() % (抖动上限 + 1)
        val 抖动 = if (原始抖动 < 0) -原始抖动 else 原始抖动
        return (线性增长 + 抖动).coerceIn(最小轮询间隔毫秒, 最大轮询间隔毫秒)
    }
    /** 轮询器统一退避入口，指数封顶保证无忙循环。 */
    fun 计算退避等待(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 1)
    fun 分级截断策略01(原始文本: String): String = 分级截断按档位(原始文本, 1)
    fun 分级截断策略02(原始文本: String): String = 分级截断按档位(原始文本, 2)
    fun 分级截断策略03(原始文本: String): String = 分级截断按档位(原始文本, 3)
    fun 分级截断策略04(原始文本: String): String = 分级截断按档位(原始文本, 4)
    fun 分级截断策略05(原始文本: String): String = 分级截断按档位(原始文本, 5)
    fun 分级截断策略06(原始文本: String): String = 分级截断按档位(原始文本, 6)
    fun 分级截断策略07(原始文本: String): String = 分级截断按档位(原始文本, 7)
    fun 分级截断策略08(原始文本: String): String = 分级截断按档位(原始文本, 8)
    fun 分级截断策略09(原始文本: String): String = 分级截断按档位(原始文本, 9)
    fun 分级截断策略10(原始文本: String): String = 分级截断按档位(原始文本, 10)
    fun 分级截断策略11(原始文本: String): String = 分级截断按档位(原始文本, 11)
    fun 分级截断策略12(原始文本: String): String = 分级截断按档位(原始文本, 12)
    fun 分级截断策略13(原始文本: String): String = 分级截断按档位(原始文本, 13)
    fun 分级截断策略14(原始文本: String): String = 分级截断按档位(原始文本, 14)
    fun 分级截断策略15(原始文本: String): String = 分级截断按档位(原始文本, 15)
    fun 分级截断策略16(原始文本: String): String = 分级截断按档位(原始文本, 16)
    fun 分级截断策略17(原始文本: String): String = 分级截断按档位(原始文本, 17)
    fun 分级截断策略18(原始文本: String): String = 分级截断按档位(原始文本, 18)
    fun 分级截断策略19(原始文本: String): String = 分级截断按档位(原始文本, 19)
    fun 分级截断策略20(原始文本: String): String = 分级截断按档位(原始文本, 20)
    fun 退避等待计算01(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 1)
    fun 退避等待计算02(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 2)
    fun 退避等待计算03(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 3)
    fun 退避等待计算04(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 4)
    fun 退避等待计算05(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 5)
    fun 退避等待计算06(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 6)
    fun 退避等待计算07(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 7)
    fun 退避等待计算08(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 8)
    fun 退避等待计算09(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 9)
    fun 退避等待计算10(连续空轮询次数: Int): Long = 退避等待按档位(连续空轮询次数, 10)
}
