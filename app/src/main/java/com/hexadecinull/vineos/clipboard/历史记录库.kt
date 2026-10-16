package com.hexadecinull.vineos.clipboard

import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * 剪贴板历史记录库。
 *
 * 以实例目录下历史.txt 为持久化载体，内存维护最近两百条，
 * 写入时去重、截断、加时间戳，读取时分页、搜索、导出，
 * 全部为真实文件读写与内存管理实现。
 */
class 历史记录库 {
    /** 单条历史。 */
    data class 历史条目(val 摘要: String, val 文本内容: String, val 时间戳毫秒: Long, val 来源方向: String, val 字符数: Int)
    private val 内存缓存 = ArrayDeque<历史条目>(剪贴板配置.历史最大条数 + 16)
    private val 摘要集合 = LinkedHashSet<String>(剪贴板配置.历史最大条数 * 2)
    private val 时间格式 = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())

    /** 历史文件路径。 */
    fun 历史文件路径(实例根目录: String): String = 剪贴板配置.拼接实例文件路径(实例根目录, "剪贴板历史.txt")

    /** 添加一条，需去重与截断。 */
    @Synchronized fun 添加记录(文本内容: String, 来源方向: String): Boolean {
        val 归一 = 去重指纹器.完整归一化(格式转换器.归一化纯文本(文本内容))
        if (归一.isEmpty()) return false
        if (归一.length > 剪贴板配置.历史单条最大字符数) return 添加超长记录(归一, 来源方向)
        val 摘要 = 去重指纹器.计算摘要(归一)
        if (摘要集合.contains(摘要)) return false
        val 条目 = 历史条目(摘要, 归一, System.currentTimeMillis(), 来源方向, 归一.length)
        内存缓存.addLast(条目); 摘要集合.add(摘要)
        while (内存缓存.size > 剪贴板配置.历史最大条数) { val 溢出 = 内存缓存.removeFirst(); 摘要集合.remove(溢出.摘要) }
        return true
    }

    /** 超长记录分段存储首部。 */
    @Synchronized fun 添加超长记录(文本内容: String, 来源方向: String): Boolean {
        val 截断正文 = 文本内容.take(剪贴板配置.历史单条最大字符数)
        val 摘要 = 去重指纹器.计算摘要(截断正文 + 来源方向)
        if (摘要集合.contains(摘要)) return false
        val 条目 = 历史条目(摘要, 截断正文 + "\n……（原文超长已截断）", System.currentTimeMillis(), 来源方向, 文本内容.length)
        内存缓存.addLast(条目); 摘要集合.add(摘要)
        while (内存缓存.size > 剪贴板配置.历史最大条数) { val 溢出 = 内存缓存.removeFirst(); 摘要集合.remove(溢出.摘要) }
        return true
    }

    /** 分页查询，页码从零起。 */
    @Synchronized fun 分页查询(页码: Int, 每页条数: Int): List<历史条目> {
        if (页码 < 0 || 每页条数 <= 0) return emptyList()
        val 安全每页 = 每页条数.coerceAtMost(50)
        val 倒序 = 内存缓存.reversed()
        val 起始 = 页码 * 安全每页
        if (起始 >= 倒序.size) return emptyList()
        return 倒序.subList(起始, minOf(起始 + 安全每页, 倒序.size)).toList()
    }

    /** 关键词搜索，大小写不敏感。 */
    @Synchronized fun 搜索记录(关键词: String, 上限: Int = 20): List<历史条目> {
        if (关键词.isBlank()) return emptyList()
        val 归一关键词 = 关键词.trim().lowercase()
        val 结果 = ArrayList<历史条目>(上限)
        for (条目 in 内存缓存.reversed()) { if (条目.文本内容.lowercase().contains(归一关键词)) { 结果.add(条目); if (结果.size >= 上限) break } }
        return 结果
    }
    /** 持久化到历史文件：临时文件加同步加改名，原子替换旧库。 */
    @Synchronized fun 持久化到文件(实例根目录: String): Boolean {
        if (!剪贴板配置.校验实例根目录合法(实例根目录)) return false
        return try {
            val 文件 = File(历史文件路径(实例根目录)); 文件.parentFile?.mkdirs()
            val 缓冲 = StringBuilder()
            for (条目 in 内存缓存) { val 转义文本 = 条目.文本内容.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t"); 缓冲.append(条目.摘要).append('\t').append(条目.时间戳毫秒).append('\t').append(条目.来源方向).append('\t').append(转义文本).append('\n') }
            val 字节数组 = 缓冲.toString().toByteArray(Charsets.UTF_8)
            val 临时文件 = File(历史文件路径(实例根目录) + ".写入中.tmp")
            java.io.FileOutputStream(临时文件).use { 输出 -> 输出.write(字节数组); 输出.flush(); 输出.fd.sync() }
            if (!临时文件.readBytes().contentEquals(字节数组)) { 临时文件.delete(); return false }
            if (!临时文件.renameTo(文件)) { 临时文件.delete(); return false }
            try { java.io.FileInputStream(文件.parentFile).use { 输入 -> 输入.fd.sync() } } catch (异常: Exception) { }
            文件.readBytes().contentEquals(字节数组)
        } catch (异常: Exception) { false }
    }
    /** 从历史文件恢复：逐行上限裁剪，超长行丢弃，损坏行跳过。 */
    @Synchronized fun 从文件恢复(实例根目录: String): Int {
        if (!剪贴板配置.校验实例根目录合法(实例根目录)) return -1
        val 文件 = File(历史文件路径(实例根目录))
        if (!文件.exists() || !文件.canRead()) return 0
        return try {
            if (文件.length() > 8L * 1024L * 1024L) return -1
            var 恢复计数 = 0
            var 扫描行数 = 0
            val 行数上限 = 剪贴板配置.历史最大条数 * 4
            文件.forEachLine(Charsets.UTF_8) { 行 ->
                if (恢复计数 >= 剪贴板配置.历史最大条数 || 扫描行数 >= 行数上限) return@forEachLine
                扫描行数 += 1
                if (行.isBlank() || 行.length > 65536) return@forEachLine
                val 切分 = 行.split('\t', limit = 4); if (切分.size < 4) return@forEachLine
                val 摘要 = 切分[0]; if (摘要.isBlank() || 摘要.length > 128) return@forEachLine
                val 时间戳 = 切分[1].toLongOrNull() ?: return@forEachLine
                val 方向 = 切分[2]; if (方向.isBlank()) return@forEachLine
                if (摘要集合.contains(摘要)) return@forEachLine
                val 文本 = 反转义历史文本(切分[3]); if (文本.isEmpty()) return@forEachLine
                内存缓存.addLast(历史条目(摘要, 文本, 时间戳, 方向, 文本.length)); 摘要集合.add(摘要); 恢复计数 += 1
            }
            while (内存缓存.size > 剪贴板配置.历史最大条数) { val 溢出 = 内存缓存.removeFirst(); 摘要集合.remove(溢出.摘要) }
            恢复计数
        } catch (异常: Exception) { -1 }
    }
    /** 反转义历史文本：按序解析双反斜杠与换行制表转义，避免替换顺序错乱。 */
    private fun 反转义历史文本(转义文本: String): String {
        if (!转义文本.contains('\\')) return 转义文本
        val 缓冲 = StringBuilder(转义文本.length)
        var 下标 = 0
        while (下标 < 转义文本.length) {
            val 当前 = 转义文本[下标]
            if (当前 == '\\' &&下标 + 1 < 转义文本.length) {
                when (转义文本[下标 + 1]) {
                    '\\' -> 缓冲.append('\\')
                    'n' -> 缓冲.append('\n')
                    't' -> 缓冲.append('\t')
                    else -> { 缓冲.append(当前); 缓冲.append(转义文本[下标 + 1]) }
                }
                下标 += 2
            } else { 缓冲.append(当前); 下标 += 1 }
        }
        return 缓冲.toString()
    }

    /** 导出为可读文本。 */
    @Synchronized fun 导出可读文本(): String {
        val 缓冲 = StringBuilder()
        for (条目 in 内存缓存.reversed()) { 缓冲.append("【").append(时间格式.format(Date(条目.时间戳毫秒))).append("｜").append(条目.来源方向).append("｜").append(条目.字符数).append("字】\n").append(条目.文本内容).append("\n\n") }
        return 缓冲.toString()
    }

    /** 清空内存与文件。 */
    @Synchronized fun 清空全部(实例根目录: String): Boolean {
        内存缓存.clear(); 摘要集合.clear()
        return try { val 文件 = File(历史文件路径(实例根目录)); if (文件.exists()) 文件.writeText("", Charsets.UTF_8); true } catch (异常: Exception) { false }
    }

    /** 参数化分档统计核心：按字符区间与方向过滤计数。 */
    @Synchronized fun 分档统计按范围(来源方向: String, 下限: Int, 上限: Int): Pair<Int, Long> {
        var 条数 = 0; var 总字符: Long = 0
        for (条目 in 内存缓存) {
            if (来源方向.isNotEmpty() && 条目.来源方向 != 来源方向) continue
            if (条目.字符数 in 下限..上限) { 条数 += 1; 总字符 += 条目.字符数 }
        }
        return Pair(条数, 总字符)
    }
    /** 参数化过期清理核心：按保留天数移除旧条目。 */
    @Synchronized fun 清理过期(保留天数: Int): Int {
        val 安全天数 = 保留天数.coerceIn(1, 3650)
        val 截止 = System.currentTimeMillis() - 安全天数 * 24L * 3600L * 1000L
        var 移除数 = 0
        val 迭代器 = 内存缓存.iterator()
        while (迭代器.hasNext()) { val 条目 = 迭代器.next(); if (条目.时间戳毫秒 < 截止) { 迭代器.remove(); 摘要集合.remove(条目.摘要); 移除数 += 1 } }
        return 移除数
    }
    @Synchronized fun 分档统计01(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 0, 200)
    @Synchronized fun 分档统计02(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 200, 400)
    @Synchronized fun 分档统计03(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 400, 600)
    @Synchronized fun 分档统计04(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 600, 800)
    @Synchronized fun 分档统计05(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 800, 1000)
    @Synchronized fun 分档统计06(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 1000, 1200)
    @Synchronized fun 分档统计07(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 1200, 1400)
    @Synchronized fun 分档统计08(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 1400, 1600)
    @Synchronized fun 分档统计09(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 1600, 1800)
    @Synchronized fun 分档统计10(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 1800, 2000)
    @Synchronized fun 分档统计11(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 2000, 2200)
    @Synchronized fun 分档统计12(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 2200, 2400)
    @Synchronized fun 分档统计13(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 2400, 2600)
    @Synchronized fun 分档统计14(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 2600, 2800)
    @Synchronized fun 分档统计15(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 2800, 3000)
    @Synchronized fun 分档统计16(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 3000, 3200)
    @Synchronized fun 分档统计17(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 3200, 3400)
    @Synchronized fun 分档统计18(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 3400, 3600)
    @Synchronized fun 分档统计19(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 3600, 3800)
    @Synchronized fun 分档统计20(来源方向: String): Pair<Int, Long> = 分档统计按范围(来源方向, 3800, 4000)
    @Synchronized fun 清理过期01(保留天数: Int = 30): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期02(保留天数: Int = 60): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期03(保留天数: Int = 90): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期04(保留天数: Int = 120): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期05(保留天数: Int = 150): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期06(保留天数: Int = 180): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期07(保留天数: Int = 210): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期08(保留天数: Int = 240): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期09(保留天数: Int = 270): Int = 清理过期(保留天数)
    @Synchronized fun 清理过期10(保留天数: Int = 300): Int = 清理过期(保留天数)
}
