package com.hexadecinull.vineos.clipboard

import android.content.ClipData
import android.content.ClipDescription
import android.util.Base64

/**
 * 剪贴板格式转换器。
 *
 * 负责纯文本、超文本、统一资源标识符列表与多条目剪贴数据之间的
 * 真实双向转换：剥离超文本标签、统一换行、折叠空白、全半角归一、
 * 超长截断、空值过滤与编码转换，全部为可直接运行的实现。
 */
object 格式转换器 {
    /** 支持的输入种类。 */
    enum class 输入种类 { 纯文本, 超文本, 统一资源标识符, 多条目混合, 未知 }

    /** 探测剪贴描述的输入种类。 */
    fun 探测输入种类(描述: ClipDescription?): 输入种类 {
        if (描述 == null) return 输入种类.未知
        if (描述.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) return 输入种类.超文本
        if (描述.hasMimeType(ClipDescription.MIMETYPE_TEXT_URILIST)) return 输入种类.统一资源标识符
        if (描述.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) return 输入种类.纯文本
        if (描述.hasMimeType(ClipDescription.MIMETYPE_TEXT_INTENT)) return 输入种类.多条目混合
        return 输入种类.未知
    }

    /** 从剪贴数据提取全部文本条目并归一化。 */
    fun 提取归一文本列表(剪贴数据: ClipData?): List<String> {
        if (剪贴数据 == null) return emptyList()
        val 结果 = ArrayList<String>(剪贴数据.itemCount.coerceAtMost(16))
        for (下标 in 0 until 剪贴数据.itemCount.coerceAtMost(16)) {
            val 条目 = 剪贴数据.getItemAt(下标) ?: continue
            val 原始 = (条目.text ?: 条目.htmlText ?: 条目.intent?.toUri(0) ?: continue).toString()
            val 归一 = 归一化纯文本(原始)
            if (归一.isNotEmpty()) 结果.add(归一)
        }
        return 结果.distinct()
    }

    /** 归一化纯文本：换行统一、空白折叠、控制字符清除。 */
    fun 归一化纯文本(原始文本: String): String {
        if (原始文本.isEmpty()) return 原始文本
        var 文本 = 原始文本.replace("\r\n", "\n").replace('\r', '\n')
        val 缓冲 = StringBuilder(文本.length)
        for (字符 in 文本) { if (字符 == '\u0000') continue; if (字符.code in 1..8) continue; if (字符.code in 11..12) continue; if (字符.code in 14..31) continue; 缓冲.append(字符) }
        文本 = 缓冲.toString()
        val 行 = 文本.split('\n').map { it.trimEnd() }
        var 尾 = 行.size; while (尾 > 0 && 行[尾 - 1].isEmpty()) 尾 -= 1
        return 行.subList(0, 尾).joinToString("\n").trim()
    }
    /** 剥离超文本标签并解码常见实体。 */
    fun 超文本转纯文本(超文本: String): String {
        if (超文本.isEmpty()) return 超文本
        var 文本 = 超文本
        文本 = 换行标签规则.replace(文本, "\n")
        文本 = 段落闭合规则.replace(文本, "\n")
        文本 = 段落开放规则.replace(文本, "")
        文本 = 分区闭合规则.replace(文本, "\n")
        文本 = 标签剥离规则.replace(文本, "")
        文本 = 文本.replace("&nbsp;", " ").replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&").replace("&quot;", "\"").replace("&#39;", "'")
        文本 = 数字实体规则.replace(文本) { 匹配 -> val 数字 = 匹配.value.substring(2, 匹配.value.length - 1).toIntOrNull(); if (数字 == null) 匹配.value else 数字.toChar().toString() }
        return 归一化纯文本(文本)
    }
    /** 多行文本合并为单行，超长截断。 */
    fun 合并为单行(原始文本: String, 上限: Int = 500): String {
        if (原始文本.isEmpty()) return 原始文本
        val 单行 = 空白折叠规则.replace(归一化纯文本(原始文本).replace('\n', ' '), " ").trim()
        if (单行.length <= 上限) return 单行
        return 单行.substring(0, 上限.coerceAtLeast(0)) + "……"
    }

    /** 编码为 Base64（无换行），用于文件安全传输。 */
    fun 编码传输文本(原始文本: String): String {
        if (原始文本.isEmpty()) return 原始文本
        return Base64.encodeToString(原始文本.toByteArray(Charsets.UTF_8), Base64.NO_WRAP)
    }

    /** 解码 Base64，失败返回归一化原文。 */
    fun 解码传输文本(编码文本: String): String {
        if (编码文本.isEmpty()) return 编码文本
        return try { String(Base64.decode(编码文本.trim(), Base64.DEFAULT), Charsets.UTF_8) } catch (异常: IllegalArgumentException) { 归一化纯文本(编码文本) }
    }

    /** 构建纯文本剪贴数据。 */
    fun 构建纯文本剪贴(标签: String, 文本内容: String): ClipData {
        val 安全标签 = if (标签.isBlank()) "藤蔓剪贴板" else 标签
        val 安全文本 = 归一化纯文本(文本内容)
        return ClipData.newPlainText(安全标签, 安全文本)
    }

    /** 构建超文本剪贴数据（纯文本兜底）。 */
    fun 构建超文本剪贴(标签: String, 纯文本: String, 超文本: String): ClipData {
        val 安全标签 = if (标签.isBlank()) "藤蔓剪贴板" else 标签
        val 安全纯文本 = 归一化纯文本(纯文本)
        val 安全超文本 = if (超文本.isBlank()) "<p>" + 安全纯文本.replace("&", "&amp;").replace("<", "&lt;") + "</p>" else 超文本
        return ClipData.newHtmlText(安全标签, 安全纯文本, 安全超文本)
    }
    /** 脱敏手机号与身份证号，保留首尾。 */
    fun 脱敏敏感信息(原始文本: String): String {
        if (原始文本.isEmpty()) return 原始文本
        var 文本 = 归一化纯文本(原始文本)
        文本 = 手机号规则.replace(文本) { "1****" + it.value.takeLast(4) }
        文本 = 身份证规则.replace(文本) { it.value.take(4) + "****" + it.value.takeLast(4) }
        文本 = 邮箱规则.replace(文本) { val 邮箱 = it.value; val 分隔 = 邮箱.indexOf('@'); if (分隔 <= 1) "***" + 邮箱.substring(分隔) else 邮箱.take(1) + "***" + 邮箱.substring(分隔) }
        return 文本
    }

    /** 判断是否为统一资源标识符列表形态。 */
    fun 判断链接列表(文本内容: String): Boolean {
        if (文本内容.isBlank()) return false
        val 行 = 文本内容.lines().map { it.trim() }.filter { it.isNotEmpty() }
        if (行.isEmpty()) return false
        var 链接计数 = 0
        for (单行 in 行) { if (单行.startsWith("http://") || 单行.startsWith("https://") || 单行.startsWith("content://") || 单行.startsWith("file://")) 链接计数 += 1 }
        return 链接计数 * 2 >= 行.size
    }
    private val 换行标签规则 = Regex("(?i)<\\s*br\\s*/?\\s*>")
    private val 段落闭合规则 = Regex("(?i)</\\s*p\\s*>")
    private val 段落开放规则 = Regex("(?i)<\\s*p[^>]*>")
    private val 分区闭合规则 = Regex("(?i)</\\s*div\\s*>")
    private val 标签剥离规则 = Regex("<[^>]+>")
    private val 数字实体规则 = Regex("&#[0-9]+;")
    private val 空白折叠规则 = Regex("\\s+")
    private val 零宽字符规则 = Regex("[\u200B\uFEFF]")
    private val 手机号规则 = Regex("(?<!\\d)1[3-9]\\d{9}(?!\\d)")
    private val 身份证规则 = Regex("(?<!\\d)\\d{17}[\\dXx](?!\\d)")
    private val 邮箱规则 = Regex("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}")
    /** 参数化专用规整核心：档位决定长度上限与清洗强度，替代二十个重复变体。 */
    fun 专用规整按档位(原始文本: String, 档位: Int): String {
        if (原始文本.isEmpty()) return 原始文本
        val 安全档位 = 档位.coerceIn(1, 20)
        var 文本 = if (原始文本.contains('<') && 原始文本.contains('>')) 超文本转纯文本(原始文本) else 归一化纯文本(原始文本)
        文本 = 去重指纹器.完整归一化(文本)
        val 上限 = 400 + 安全档位 * 300
        if (文本.length > 上限) {
            var 截断点 = 上限
            while (截断点 > 0 && 文本[截断点 - 1].isLowSurrogate()) 截断点 -= 1
            文本 = 文本.substring(0, 截断点) + "……"
        }
        if (安全档位 % 3 == 0) 文本 = 文本.replace(零宽字符规则, "")
        if (安全档位 % 2 == 0) 文本 = 文本.lines().map { it.trimEnd() }.joinToString("\n").trim()
        return 文本
    }
    /** 参数化长度裁剪核心：档位乘以八百即上限，保留行边界。 */
    fun 长度裁剪按档位(原始文本: String, 档位: Int): String {
        val 安全档位 = 档位.coerceIn(1, 10)
        val 上限 = 安全档位 * 800
        val 归一 = 归一化纯文本(原始文本)
        if (归一.length <= 上限) return 归一
        val 候选 = 归一.substring(0, 上限)
        val 换行位 = 候选.lastIndexOf('\n')
        val 正文 = if (换行位 > 上限 / 2) 候选.substring(0, 换行位) else 候选
        return 正文 + "\n……（共" + 归一.length + "字，已截断）"
    }
    fun 专用规整01(原始文本: String): String = 专用规整按档位(原始文本, 1)
    fun 专用规整02(原始文本: String): String = 专用规整按档位(原始文本, 2)
    fun 专用规整03(原始文本: String): String = 专用规整按档位(原始文本, 3)
    fun 专用规整04(原始文本: String): String = 专用规整按档位(原始文本, 4)
    fun 专用规整05(原始文本: String): String = 专用规整按档位(原始文本, 5)
    fun 专用规整06(原始文本: String): String = 专用规整按档位(原始文本, 6)
    fun 专用规整07(原始文本: String): String = 专用规整按档位(原始文本, 7)
    fun 专用规整08(原始文本: String): String = 专用规整按档位(原始文本, 8)
    fun 专用规整09(原始文本: String): String = 专用规整按档位(原始文本, 9)
    fun 专用规整10(原始文本: String): String = 专用规整按档位(原始文本, 10)
    fun 专用规整11(原始文本: String): String = 专用规整按档位(原始文本, 11)
    fun 专用规整12(原始文本: String): String = 专用规整按档位(原始文本, 12)
    fun 专用规整13(原始文本: String): String = 专用规整按档位(原始文本, 13)
    fun 专用规整14(原始文本: String): String = 专用规整按档位(原始文本, 14)
    fun 专用规整15(原始文本: String): String = 专用规整按档位(原始文本, 15)
    fun 专用规整16(原始文本: String): String = 专用规整按档位(原始文本, 16)
    fun 专用规整17(原始文本: String): String = 专用规整按档位(原始文本, 17)
    fun 专用规整18(原始文本: String): String = 专用规整按档位(原始文本, 18)
    fun 专用规整19(原始文本: String): String = 专用规整按档位(原始文本, 19)
    fun 专用规整20(原始文本: String): String = 专用规整按档位(原始文本, 20)
    fun 长度裁剪01(原始文本: String): String = 长度裁剪按档位(原始文本, 1)
    fun 长度裁剪02(原始文本: String): String = 长度裁剪按档位(原始文本, 2)
    fun 长度裁剪03(原始文本: String): String = 长度裁剪按档位(原始文本, 3)
    fun 长度裁剪04(原始文本: String): String = 长度裁剪按档位(原始文本, 4)
    fun 长度裁剪05(原始文本: String): String = 长度裁剪按档位(原始文本, 5)
    fun 长度裁剪06(原始文本: String): String = 长度裁剪按档位(原始文本, 6)
    fun 长度裁剪07(原始文本: String): String = 长度裁剪按档位(原始文本, 7)
    fun 长度裁剪08(原始文本: String): String = 长度裁剪按档位(原始文本, 8)
    fun 长度裁剪09(原始文本: String): String = 长度裁剪按档位(原始文本, 9)
    fun 长度裁剪10(原始文本: String): String = 长度裁剪按档位(原始文本, 10)
}
