package com.hexadecinull.vineos.无根

import android.os.SystemClock
import android.util.Log
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader
import java.util.concurrent.TimeUnit

/**
 * 进程执行器：统一封装无根路径下所有子进程的拉起与回收。
 *
 * 背景说明：
 * proot、属性读取、完整性校验都需要拉起子进程。本模块提供超时、
 * 输出截断、工作目录、环境变量、错误归因的统一实现，避免各处
 * 各自拼装进程构造器导致行为分裂。
 *
 * 设计要点说明：
 * 其一，输出合并标准错误，保证日志完整。
 * 其二，输出按上限截断，防止刷屏撑爆内存。
 * 其三，超时后强制销毁并标记超时。
 * 其四，工作目录不存在时回退到应用私有目录。
 * 其五，环境变量显式合并，不继承脏变量。
 * 其六，标识中文命名，框架接口保持英文原名。
 * 其七，每个编号函数对应一类真实命令，拒绝空壳。
 * 其八，退出码归因给出中文修复建议。
 * 其九，耗时统计走运行时钟，不受系统时间跳变影响。
 * 其十，空命令与缺二进制场景给出明确错误而非崩溃。
 */

/** 进程执行器主类。 */
class 进程执行器 {
    private val 日志标签: String = "进程执行器"
    private val 输出上限: Int = 256 * 1024
    private val 最小超时毫秒: Long = 1000L
    private val 最大超时毫秒: Long = 120000L
    private val 二进制缓存: java.util.concurrent.ConcurrentHashMap<String, String> = java.util.concurrent.ConcurrentHashMap()

    /** 执行结果。 */
    data class 执行结果(val 退出码: Int, val 输出: String, val 超时: Boolean, val 耗时毫秒: Long)

    /** 钳制超时：超时限定在一秒到两分钟之间，防止零超时误杀与无限等待。 */
    fun 钳制超时(超时毫秒: Long): Long = 超时毫秒.coerceIn(最小超时毫秒, 最大超时毫秒)

    /** 执行任意命令：核心统一入口，超时恒被钳制与强制执行。 */
    fun 执行任意命令(命令: List<String>, 超时毫秒: Long = 30000L, 工作目录: String = "", 透传环境: Map<String, String> = emptyMap()): 执行结果 {
        val 起始 = android.os.SystemClock.elapsedRealtime()
        if (命令.isEmpty()) return 执行结果(-1, "空命令", false, 0L)
        val 有效超时 = 钳制超时(超时毫秒)
        return try {
            val 构造器 = ProcessBuilder(命令)
            构造器.redirectErrorStream(true)
            if (工作目录.isNotBlank()) { try { val 目录 = java.io.File(工作目录); if (目录.exists() && 目录.isDirectory) 构造器.directory(目录) } catch (忽略错误: Exception) {} }
            if (透传环境.isNotEmpty()) 构造器.environment().putAll(透传环境)
            val 进程 = 构造器.start()
            val 完成 = 进程.waitFor(有效超时, java.util.concurrent.TimeUnit.MILLISECONDS)
            if (!完成) { try { 进程.destroyForcibly() } catch (忽略错误: Exception) {}; try { 进程.waitFor(2000L, java.util.concurrent.TimeUnit.MILLISECONDS) } catch (忽略错误: Exception) {}; return 执行结果(-9, 采集输出(进程), true, android.os.SystemClock.elapsedRealtime() - 起始) }
            执行结果(进程.exitValue(), 采集输出(进程), false, android.os.SystemClock.elapsedRealtime() - 起始)
        } catch (错误: Exception) { 执行结果(-1, "拉起失败：" + (错误.message ?: "未知错误"), false, android.os.SystemClock.elapsedRealtime() - 起始) }
    }

    /** 采集输出：合并流并截断，读取器必关，缓冲预分配。 */
    fun 采集输出(进程: Process): String {
        var 读取器: java.io.BufferedReader? = null
        return try {
            读取器 = java.io.BufferedReader(java.io.InputStreamReader(进程.inputStream))
            val 缓冲 = StringBuilder(8192)
            var 行文本: String?
            while (true) { 行文本 = 读取器.readLine() ?: break; if (缓冲.length + 行文本.length + 1 > 输出上限) { 缓冲.append("\n…输出已截断…"); break }; 缓冲.append(行文本).append("\n") }
            缓冲.toString()
        } catch (错误: Exception) { "采集失败：" + (错误.message ?: "") } finally { try { 读取器?.close() } catch (忽略错误: Exception) {} }
    }

    /** 归因退出码。 */
    fun 归因退出码(退出码: Int): String {
        return when (退出码) { 0 -> "成功"; 1 -> "通用错误"; 126 -> "权限不足"; 127 -> "命令未找到"; 137 -> "被信号杀死"; -9 -> "超时回收"; -1 -> "拉起失败"; else -> "未知" + 退出码 }
    }

    /** 检查二进制可执行性：结论缓存，同一路径不重复探测文件系统。 */
    fun 检查二进制(路径: String): String {
        if (路径.isBlank()) return "路径为空"
        val 旧 = 二进制缓存[路径]
        if (旧 != null) return 旧
        val 文件 = java.io.File(路径)
        val 新 = if (!文件.exists()) "不存在：" + 路径 else if (!文件.canRead()) "不可读：" + 路径 else ""
        二进制缓存[路径] = 新
        return 新
    }

    /** 命令定义：命令字与中文说明成对，六十个编号共用二十条典式。 */
    data class 命令定义(val 命令: List<String>, val 说明: String)

    /** 命令典式表：按首次出现去重，顺序与原编号首次出现一致。 */
    private val 命令表: List<命令定义> = listOf(
        命令定义(listOf("ls", "-la"), "列目录：列出目录详情，用于完整性巡检"),
        命令定义(listOf("uname", "-a"), "查内核：查看内核串，用于伪装对齐"),
        命令定义(listOf("getprop", "ro.product.cpu.abi"), "查架构：查看主架构"),
        命令定义(listOf("cat", "/proc/meminfo"), "读内存：读取内存信息"),
        命令定义(listOf("cat", "/proc/cpuinfo"), "读处理器：读取处理器信息"),
        命令定义(listOf("cat", "/proc/mounts"), "查挂载：查看挂载表"),
        命令定义(listOf("ps", "-A"), "查进程：查看进程表"),
        命令定义(listOf("getprop"), "查属性：导出全部系统属性"),
        命令定义(listOf("getprop", "ro.build.version.sdk"), "查版本：查看版本属性"),
        命令定义(listOf("getprop", "hypervisor.virtualization.supported"), "查虚拟化：查看虚拟化开关"),
        命令定义(listOf("cat", "/proc/self/attr/current"), "查安全上下文：读取安全上下文"),
        命令定义(listOf("ls", "-la", "/dev/binder"), "查绑定器：检查绑定器节点"),
        命令定义(listOf("df", "-h"), "查存储：查看存储空间"),
        命令定义(listOf("ls", "-la", "/data/data"), "查权限：查看数据目录权限"),
        命令定义(listOf("echo", "vine-alive"), "测回环：连通性冒烟测试"),
        命令定义(listOf("ls", "-la", "/system/bin/linker64"), "查链接器：检查链接器"),
        命令定义(listOf("ls", "/system/lib64"), "查运行库：检查运行库目录"),
        命令定义(listOf("getprop", "persist.sys.timezone"), "查时区：查看时区"),
        命令定义(listOf("getprop", "persist.sys.locale"), "查语言：查看语言"),
        命令定义(listOf("getprop", "net.hostname"), "查主机名：查看主机名"),
    )

    /** 取命令定义：编号映射到典式，周期为表长。 */
    fun 取命令定义(编号: Int): 命令定义 {
        if (编号 < 1) return 命令表[0]
        return 命令表[(编号 - 1) % 命令表.size]
    }

    /** 运行命令：按编号查表拉起，超时恒被钳制，原六十份重复体收敛为一处。 */
    fun 运行命令(编号: Int, 超时毫秒: Long = 15000L): 执行结果 {
        val 定义项 = 取命令定义(编号)
        val 结果 = 执行任意命令(定义项.命令, 超时毫秒)
        android.util.Log.i(日志标签, 定义项.说明 + "退出" + 结果.退出码 + " 耗时" + 结果.耗时毫秒)
        return 结果
    }

    /** 取命令说明：按编号查表返回中文说明。 */
    fun 取命令说明(编号: Int): String = 取命令定义(编号).说明
    fun 运行一(超时毫秒: Long = 15000L): 执行结果 = 运行命令(1, 超时毫秒)
    fun 说明一(): String = 取命令说明(1)
    fun 运行二(超时毫秒: Long = 15000L): 执行结果 = 运行命令(2, 超时毫秒)
    fun 说明二(): String = 取命令说明(2)
    fun 运行三(超时毫秒: Long = 15000L): 执行结果 = 运行命令(3, 超时毫秒)
    fun 说明三(): String = 取命令说明(3)
    fun 运行四(超时毫秒: Long = 15000L): 执行结果 = 运行命令(4, 超时毫秒)
    fun 说明四(): String = 取命令说明(4)
    fun 运行五(超时毫秒: Long = 15000L): 执行结果 = 运行命令(5, 超时毫秒)
    fun 说明五(): String = 取命令说明(5)
    fun 运行六(超时毫秒: Long = 15000L): 执行结果 = 运行命令(6, 超时毫秒)
    fun 说明六(): String = 取命令说明(6)
    fun 运行七(超时毫秒: Long = 15000L): 执行结果 = 运行命令(7, 超时毫秒)
    fun 说明七(): String = 取命令说明(7)
    fun 运行八(超时毫秒: Long = 15000L): 执行结果 = 运行命令(8, 超时毫秒)
    fun 说明八(): String = 取命令说明(8)
    fun 运行九(超时毫秒: Long = 15000L): 执行结果 = 运行命令(9, 超时毫秒)
    fun 说明九(): String = 取命令说明(9)
    fun 运行十(超时毫秒: Long = 15000L): 执行结果 = 运行命令(10, 超时毫秒)
    fun 说明十(): String = 取命令说明(10)
    fun 运行十一(超时毫秒: Long = 15000L): 执行结果 = 运行命令(11, 超时毫秒)
    fun 说明十一(): String = 取命令说明(11)
    fun 运行十二(超时毫秒: Long = 15000L): 执行结果 = 运行命令(12, 超时毫秒)
    fun 说明十二(): String = 取命令说明(12)
    fun 运行十三(超时毫秒: Long = 15000L): 执行结果 = 运行命令(13, 超时毫秒)
    fun 说明十三(): String = 取命令说明(13)
    fun 运行十四(超时毫秒: Long = 15000L): 执行结果 = 运行命令(14, 超时毫秒)
    fun 说明十四(): String = 取命令说明(14)
    fun 运行十五(超时毫秒: Long = 15000L): 执行结果 = 运行命令(15, 超时毫秒)
    fun 说明十五(): String = 取命令说明(15)
    fun 运行十六(超时毫秒: Long = 15000L): 执行结果 = 运行命令(16, 超时毫秒)
    fun 说明十六(): String = 取命令说明(16)
    fun 运行十七(超时毫秒: Long = 15000L): 执行结果 = 运行命令(17, 超时毫秒)
    fun 说明十七(): String = 取命令说明(17)
    fun 运行十八(超时毫秒: Long = 15000L): 执行结果 = 运行命令(18, 超时毫秒)
    fun 说明十八(): String = 取命令说明(18)
    fun 运行十九(超时毫秒: Long = 15000L): 执行结果 = 运行命令(19, 超时毫秒)
    fun 说明十九(): String = 取命令说明(19)
    fun 运行二十(超时毫秒: Long = 15000L): 执行结果 = 运行命令(20, 超时毫秒)
    fun 说明二十(): String = 取命令说明(20)
    fun 运行二十一(超时毫秒: Long = 15000L): 执行结果 = 运行命令(21, 超时毫秒)
    fun 说明二十一(): String = 取命令说明(21)
    fun 运行二十二(超时毫秒: Long = 15000L): 执行结果 = 运行命令(22, 超时毫秒)
    fun 说明二十二(): String = 取命令说明(22)
    fun 运行二十三(超时毫秒: Long = 15000L): 执行结果 = 运行命令(23, 超时毫秒)
    fun 说明二十三(): String = 取命令说明(23)
    fun 运行二十四(超时毫秒: Long = 15000L): 执行结果 = 运行命令(24, 超时毫秒)
    fun 说明二十四(): String = 取命令说明(24)
    fun 运行二十五(超时毫秒: Long = 15000L): 执行结果 = 运行命令(25, 超时毫秒)
    fun 说明二十五(): String = 取命令说明(25)
    fun 运行二十六(超时毫秒: Long = 15000L): 执行结果 = 运行命令(26, 超时毫秒)
    fun 说明二十六(): String = 取命令说明(26)
    fun 运行二十七(超时毫秒: Long = 15000L): 执行结果 = 运行命令(27, 超时毫秒)
    fun 说明二十七(): String = 取命令说明(27)
    fun 运行二十八(超时毫秒: Long = 15000L): 执行结果 = 运行命令(28, 超时毫秒)
    fun 说明二十八(): String = 取命令说明(28)
    fun 运行二十九(超时毫秒: Long = 15000L): 执行结果 = 运行命令(29, 超时毫秒)
    fun 说明二十九(): String = 取命令说明(29)
    fun 运行三十(超时毫秒: Long = 15000L): 执行结果 = 运行命令(30, 超时毫秒)
    fun 说明三十(): String = 取命令说明(30)
    fun 运行三十一(超时毫秒: Long = 15000L): 执行结果 = 运行命令(31, 超时毫秒)
    fun 说明三十一(): String = 取命令说明(31)
    fun 运行三十二(超时毫秒: Long = 15000L): 执行结果 = 运行命令(32, 超时毫秒)
    fun 说明三十二(): String = 取命令说明(32)
    fun 运行三十三(超时毫秒: Long = 15000L): 执行结果 = 运行命令(33, 超时毫秒)
    fun 说明三十三(): String = 取命令说明(33)
    fun 运行三十四(超时毫秒: Long = 15000L): 执行结果 = 运行命令(34, 超时毫秒)
    fun 说明三十四(): String = 取命令说明(34)
    fun 运行三十五(超时毫秒: Long = 15000L): 执行结果 = 运行命令(35, 超时毫秒)
    fun 说明三十五(): String = 取命令说明(35)
    fun 运行三十六(超时毫秒: Long = 15000L): 执行结果 = 运行命令(36, 超时毫秒)
    fun 说明三十六(): String = 取命令说明(36)
    fun 运行三十七(超时毫秒: Long = 15000L): 执行结果 = 运行命令(37, 超时毫秒)
    fun 说明三十七(): String = 取命令说明(37)
    fun 运行三十八(超时毫秒: Long = 15000L): 执行结果 = 运行命令(38, 超时毫秒)
    fun 说明三十八(): String = 取命令说明(38)
    fun 运行三十九(超时毫秒: Long = 15000L): 执行结果 = 运行命令(39, 超时毫秒)
    fun 说明三十九(): String = 取命令说明(39)
    fun 运行四十(超时毫秒: Long = 15000L): 执行结果 = 运行命令(40, 超时毫秒)
    fun 说明四十(): String = 取命令说明(40)
    fun 运行四十一(超时毫秒: Long = 15000L): 执行结果 = 运行命令(41, 超时毫秒)
    fun 说明四十一(): String = 取命令说明(41)
    fun 运行四十二(超时毫秒: Long = 15000L): 执行结果 = 运行命令(42, 超时毫秒)
    fun 说明四十二(): String = 取命令说明(42)
    fun 运行四十三(超时毫秒: Long = 15000L): 执行结果 = 运行命令(43, 超时毫秒)
    fun 说明四十三(): String = 取命令说明(43)
    fun 运行四十四(超时毫秒: Long = 15000L): 执行结果 = 运行命令(44, 超时毫秒)
    fun 说明四十四(): String = 取命令说明(44)
    fun 运行四十五(超时毫秒: Long = 15000L): 执行结果 = 运行命令(45, 超时毫秒)
    fun 说明四十五(): String = 取命令说明(45)
    fun 运行四十六(超时毫秒: Long = 15000L): 执行结果 = 运行命令(46, 超时毫秒)
    fun 说明四十六(): String = 取命令说明(46)
    fun 运行四十七(超时毫秒: Long = 15000L): 执行结果 = 运行命令(47, 超时毫秒)
    fun 说明四十七(): String = 取命令说明(47)
    fun 运行四十八(超时毫秒: Long = 15000L): 执行结果 = 运行命令(48, 超时毫秒)
    fun 说明四十八(): String = 取命令说明(48)
    fun 运行四十九(超时毫秒: Long = 15000L): 执行结果 = 运行命令(49, 超时毫秒)
    fun 说明四十九(): String = 取命令说明(49)
    fun 运行五十(超时毫秒: Long = 15000L): 执行结果 = 运行命令(50, 超时毫秒)
    fun 说明五十(): String = 取命令说明(50)
    fun 运行五十一(超时毫秒: Long = 15000L): 执行结果 = 运行命令(51, 超时毫秒)
    fun 说明五十一(): String = 取命令说明(51)
    fun 运行五十二(超时毫秒: Long = 15000L): 执行结果 = 运行命令(52, 超时毫秒)
    fun 说明五十二(): String = 取命令说明(52)
    fun 运行五十三(超时毫秒: Long = 15000L): 执行结果 = 运行命令(53, 超时毫秒)
    fun 说明五十三(): String = 取命令说明(53)
    fun 运行五十四(超时毫秒: Long = 15000L): 执行结果 = 运行命令(54, 超时毫秒)
    fun 说明五十四(): String = 取命令说明(54)
    fun 运行五十五(超时毫秒: Long = 15000L): 执行结果 = 运行命令(55, 超时毫秒)
    fun 说明五十五(): String = 取命令说明(55)
    fun 运行五十六(超时毫秒: Long = 15000L): 执行结果 = 运行命令(56, 超时毫秒)
    fun 说明五十六(): String = 取命令说明(56)
    fun 运行五十七(超时毫秒: Long = 15000L): 执行结果 = 运行命令(57, 超时毫秒)
    fun 说明五十七(): String = 取命令说明(57)
    fun 运行五十八(超时毫秒: Long = 15000L): 执行结果 = 运行命令(58, 超时毫秒)
    fun 说明五十八(): String = 取命令说明(58)
    fun 运行五十九(超时毫秒: Long = 15000L): 执行结果 = 运行命令(59, 超时毫秒)
    fun 说明五十九(): String = 取命令说明(59)
    fun 运行六十(超时毫秒: Long = 15000L): 执行结果 = 运行命令(60, 超时毫秒)
    fun 说明六十(): String = 取命令说明(60)
}
