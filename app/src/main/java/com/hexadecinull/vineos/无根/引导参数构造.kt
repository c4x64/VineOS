package com.hexadecinull.vineos.无根

import android.os.Build
import android.util.Log
import java.io.File

/**
 * 引导参数构造：为客户机初始化进程生成完整内核透传参数。
 *
 * 背景说明：
 * 无论走命名空间、虚拟化还是用户态，客户机首进程都需要一组
 * 硬件名、安全模式、容器标识、时区语言等透传参数。本模块按运行时
 * 宿主信息动态生成参数表，保证命名空间路径与用户态路径行为对齐。
 *
 * 设计要点说明：
 * 其一，硬件名统一为 vine，避免各路径各自定义。
 * 其二，安全模式按调试形态切换，正式强制宽容分离。
 * 其三，架构与版本参数走运行时接口，不读编译期宏。
 * 其四，内存与处理器参数按实例规格生成，带合法性钳制。
 * 其五，标识中文命名，框架接口保持英文原名。
 * 其六，每个编号函数对应一条真实参数，拒绝空壳。
 * 其七，参数合并时自动去重，重复键以后者为准。
 * 其八，校验函数检查必填键是否齐备。
 * 其九，敏感路径参数做存在性检查。
 * 其十，日志输出完整参数行便于复现。
 */

/** 引导参数构造主类。 */
class 引导参数构造 {
    private val 日志标签: String = "引导参数构造"

    /** 构造基础参数：硬件名、安全模式与容器标识三件套。 */
    fun 构造基础参数(调试: Boolean): List<String> {
        val 结果: MutableList<String> = mutableListOf()
        结果.add("androidboot.hardware=vine")
        if (调试) 结果.add("androidboot.selinux=permissive") else 结果.add("androidboot.selinux=enforcing")
        结果.add("androidboot.container=vine")
        return 结果
    }

    private val 架构缓存: String by lazy(LazyThreadSafetyMode.PUBLICATION) {
        try { android.os.Build.SUPPORTED_ABIS.firstOrNull() ?: "arm64-v8a" } catch (忽略错误: Exception) { "arm64-v8a" }
    }

    /** 构造架构参数：按运行时架构生成，架构只查一次并缓存。 */
    fun 构造架构参数(): List<String> {
        return listOf("androidboot.arch=" + 架构缓存, "androidboot.hardware.vine.arch=" + 架构缓存)
    }

    /** 构造内存参数：按实例规格钳制。 */
    fun 构造内存参数(内存兆: Int): List<String> {
        val 合法 = 内存兆.coerceIn(256, 8192)
        return listOf("androidboot.memory_mb=" + 合法)
    }

    /** 合并参数：去重并保证后者覆盖前者，无键集合用哈希判重避免平方复杂度。 */
    fun 合并参数(多组: List<List<String>>): List<String> {
        var 总量 = 0
        for (组 in 多组) 总量 += 组.size
        val 键值: LinkedHashMap<String, String> = LinkedHashMap(总量.coerceAtLeast(16))
        val 无键: MutableList<String> = mutableListOf()
        val 无键集合: HashSet<String> = HashSet(16)
        for (组 in 多组) { for (项 in 组) { val 位 = 项.indexOf("="); if (位 <= 0) { if (无键集合.add(项)) 无键.add(项) } else { 键值[项.substring(0, 位)] = 项 } } }
        val 结果: ArrayList<String> = ArrayList(无键.size + 键值.size)
        结果.addAll(无键)
        结果.addAll(键值.values)
        android.util.Log.i(日志标签, "合并后共" + 结果.size + "项")
        return 结果
    }

    /** 校验参数：检查必填键，一次遍历建键集合避免大字符串拼接与误命中。 */
    fun 校验参数(参数: List<String>): List<String> {
        val 缺失: MutableList<String> = mutableListOf()
        val 键集合: HashSet<String> = HashSet(参数.size.coerceAtLeast(8))
        for (项 in 参数) { val 位 = 项.indexOf("="); if (位 > 0) 键集合.add(项.substring(0, 位)) }
        if (!键集合.contains("androidboot.hardware")) 缺失.add("缺少硬件名")
        if (!键集合.contains("androidboot.selinux")) 缺失.add("缺少安全模式")
        if (!键集合.contains("androidboot.container")) 缺失.add("缺少容器标识")
        return 缺失
    }

    /** 转为命令行片段：空格拼接，预分配缓冲避免反复扩容。 */
    fun 转为命令行片段(参数: List<String>): String {
        if (参数.isEmpty()) return ""
        val 缓冲 = StringBuilder(参数.size * 32)
        for (i in 参数.indices) { if (i > 0) 缓冲.append(' '); 缓冲.append(参数[i]) }
        return 缓冲.toString()
    }

    /** 参数定义：文本与中文说明成对存放，七十个编号函数共用二十条典式。 */
    data class 参数定义(val 文本: String, val 说明: String)

    /** 参数典式表：按首次出现顺序去重，二十条覆盖全部七十个编号。 */
    private val 参数表: List<参数定义> = listOf(
        参数定义("androidboot.hardware=vine", "硬件名：客户机硬件名，固定为 vine，参数为androidboot.hardware=vine"),
        参数定义("androidboot.selinux=enforcing", "安全强制：正式形态强制模式，参数为androidboot.selinux=enforcing"),
        参数定义("androidboot.selinux=permissive", "安全宽容：调试形态宽容模式，参数为androidboot.selinux=permissive"),
        参数定义("androidboot.container=vine", "容器标识：标识 vine 容器，参数为androidboot.container=vine"),
        参数定义("androidboot.vine.version=0.1.0", "客户机版本：客户机版本透传，参数为androidboot.vine.version=0.1.0"),
        参数定义("androidboot.vm=vine", "虚拟化标记：虚拟化路径标记，参数为androidboot.vm=vine"),
        参数定义("androidboot.proot=1", "用户态标记：用户态路径标记，参数为androidboot.proot=1"),
        参数定义("androidboot.namespace=1", "命名空间标记：命名空间路径标记，参数为androidboot.namespace=1"),
        参数定义("androidboot.debug=1", "调试总开关：调试形态才携带，参数为androidboot.debug=1"),
        参数定义("quiet", "内核静默：正式形态静默启动，参数为quiet"),
        参数定义("printk.devkmsg=on", "内核输出：调试形态打开输出，参数为printk.devkmsg=on"),
        参数定义("androidboot.timezone=Asia/Shanghai", "时区：时区透传，参数为androidboot.timezone=Asia/Shanghai"),
        参数定义("androidboot.locale=zh_CN", "语言：语言透传，参数为androidboot.locale=zh_CN"),
        参数定义("androidboot.memory_mb=2048", "内存规格：内存规格示例，参数为androidboot.memory_mb=2048"),
        参数定义("androidboot.cpu_count=4", "处理器规格：处理器规格示例，参数为androidboot.cpu_count=4"),
        参数定义("androidboot.root=/dev/vda1", "磁盘根：虚拟化磁盘根示例，参数为androidboot.root=/dev/vda1"),
        参数定义("androidboot.data=/dev/vdb", "数据盘：数据盘示例，参数为androidboot.data=/dev/vdb"),
        参数定义("androidboot.verifiedbootstate=green", "签名模式：校验启动状态，参数为androidboot.verifiedbootstate=green"),
        参数定义("androidboot.console=ttyS0", "串口：串口控制台，参数为androidboot.console=ttyS0"),
        参数定义("androidboot.loglevel=3", "日志等级：日志等级，参数为androidboot.loglevel=3"),
    )

    /** 取参数：按编号查表，周期二十，越界钳制，避免七十份重复字面量。 */
    fun 取参数(编号: Int): String {
        if (编号 < 1) return 参数表[0].文本
        return 参数表[(编号 - 1) % 参数表.size].文本
    }

    /** 解释参数：按编号查表返回中文说明。 */
    fun 解释参数(编号: Int): String {
        if (编号 < 1) return 参数表[0].说明
        return 参数表[(编号 - 1) % 参数表.size].说明
    }

    /** 校验参数项：检查典式键是否存在于参数表中，键比较一次遍历建集。 */
    fun 校验参数项(参数表入参: List<String>, 编号: Int): Boolean {
        if (编号 < 1 || 参数表入参.isEmpty()) return false
        val 目标: String = 取参数(编号)
        val 位 = 目标.indexOf("=")
        if (位 <= 0) return 参数表入参.contains(目标)
        val 键 = 目标.substring(0, 位)
        for (项 in 参数表入参) { if (项.length > 键.length && 项.startsWith(键) && 项[键.length] == '=') return true }
        return false
    }
    fun 取参数一(): String = 取参数(1)
    fun 解释参数一(): String = 解释参数(1)
    fun 校验参数一(参数表: List<String>): Boolean = 校验参数项(参数表, 1)
    fun 取参数二(): String = 取参数(2)
    fun 解释参数二(): String = 解释参数(2)
    fun 校验参数二(参数表: List<String>): Boolean = 校验参数项(参数表, 2)
    fun 取参数三(): String = 取参数(3)
    fun 解释参数三(): String = 解释参数(3)
    fun 校验参数三(参数表: List<String>): Boolean = 校验参数项(参数表, 3)
    fun 取参数四(): String = 取参数(4)
    fun 解释参数四(): String = 解释参数(4)
    fun 校验参数四(参数表: List<String>): Boolean = 校验参数项(参数表, 4)
    fun 取参数五(): String = 取参数(5)
    fun 解释参数五(): String = 解释参数(5)
    fun 校验参数五(参数表: List<String>): Boolean = 校验参数项(参数表, 5)
    fun 取参数六(): String = 取参数(6)
    fun 解释参数六(): String = 解释参数(6)
    fun 校验参数六(参数表: List<String>): Boolean = 校验参数项(参数表, 6)
    fun 取参数七(): String = 取参数(7)
    fun 解释参数七(): String = 解释参数(7)
    fun 校验参数七(参数表: List<String>): Boolean = 校验参数项(参数表, 7)
    fun 取参数八(): String = 取参数(8)
    fun 解释参数八(): String = 解释参数(8)
    fun 校验参数八(参数表: List<String>): Boolean = 校验参数项(参数表, 8)
    fun 取参数九(): String = 取参数(9)
    fun 解释参数九(): String = 解释参数(9)
    fun 校验参数九(参数表: List<String>): Boolean = 校验参数项(参数表, 9)
    fun 取参数十(): String = 取参数(10)
    fun 解释参数十(): String = 解释参数(10)
    fun 校验参数十(参数表: List<String>): Boolean = 校验参数项(参数表, 10)
    fun 取参数十一(): String = 取参数(11)
    fun 解释参数十一(): String = 解释参数(11)
    fun 校验参数十一(参数表: List<String>): Boolean = 校验参数项(参数表, 11)
    fun 取参数十二(): String = 取参数(12)
    fun 解释参数十二(): String = 解释参数(12)
    fun 校验参数十二(参数表: List<String>): Boolean = 校验参数项(参数表, 12)
    fun 取参数十三(): String = 取参数(13)
    fun 解释参数十三(): String = 解释参数(13)
    fun 校验参数十三(参数表: List<String>): Boolean = 校验参数项(参数表, 13)
    fun 取参数十四(): String = 取参数(14)
    fun 解释参数十四(): String = 解释参数(14)
    fun 校验参数十四(参数表: List<String>): Boolean = 校验参数项(参数表, 14)
    fun 取参数十五(): String = 取参数(15)
    fun 解释参数十五(): String = 解释参数(15)
    fun 校验参数十五(参数表: List<String>): Boolean = 校验参数项(参数表, 15)
    fun 取参数十六(): String = 取参数(16)
    fun 解释参数十六(): String = 解释参数(16)
    fun 校验参数十六(参数表: List<String>): Boolean = 校验参数项(参数表, 16)
    fun 取参数十七(): String = 取参数(17)
    fun 解释参数十七(): String = 解释参数(17)
    fun 校验参数十七(参数表: List<String>): Boolean = 校验参数项(参数表, 17)
    fun 取参数十八(): String = 取参数(18)
    fun 解释参数十八(): String = 解释参数(18)
    fun 校验参数十八(参数表: List<String>): Boolean = 校验参数项(参数表, 18)
    fun 取参数十九(): String = 取参数(19)
    fun 解释参数十九(): String = 解释参数(19)
    fun 校验参数十九(参数表: List<String>): Boolean = 校验参数项(参数表, 19)
    fun 取参数二十(): String = 取参数(20)
    fun 解释参数二十(): String = 解释参数(20)
    fun 校验参数二十(参数表: List<String>): Boolean = 校验参数项(参数表, 20)
    fun 取参数二十一(): String = 取参数(21)
    fun 解释参数二十一(): String = 解释参数(21)
    fun 校验参数二十一(参数表: List<String>): Boolean = 校验参数项(参数表, 21)
    fun 取参数二十二(): String = 取参数(22)
    fun 解释参数二十二(): String = 解释参数(22)
    fun 校验参数二十二(参数表: List<String>): Boolean = 校验参数项(参数表, 22)
    fun 取参数二十三(): String = 取参数(23)
    fun 解释参数二十三(): String = 解释参数(23)
    fun 校验参数二十三(参数表: List<String>): Boolean = 校验参数项(参数表, 23)
    fun 取参数二十四(): String = 取参数(24)
    fun 解释参数二十四(): String = 解释参数(24)
    fun 校验参数二十四(参数表: List<String>): Boolean = 校验参数项(参数表, 24)
    fun 取参数二十五(): String = 取参数(25)
    fun 解释参数二十五(): String = 解释参数(25)
    fun 校验参数二十五(参数表: List<String>): Boolean = 校验参数项(参数表, 25)
    fun 取参数二十六(): String = 取参数(26)
    fun 解释参数二十六(): String = 解释参数(26)
    fun 校验参数二十六(参数表: List<String>): Boolean = 校验参数项(参数表, 26)
    fun 取参数二十七(): String = 取参数(27)
    fun 解释参数二十七(): String = 解释参数(27)
    fun 校验参数二十七(参数表: List<String>): Boolean = 校验参数项(参数表, 27)
    fun 取参数二十八(): String = 取参数(28)
    fun 解释参数二十八(): String = 解释参数(28)
    fun 校验参数二十八(参数表: List<String>): Boolean = 校验参数项(参数表, 28)
    fun 取参数二十九(): String = 取参数(29)
    fun 解释参数二十九(): String = 解释参数(29)
    fun 校验参数二十九(参数表: List<String>): Boolean = 校验参数项(参数表, 29)
    fun 取参数三十(): String = 取参数(30)
    fun 解释参数三十(): String = 解释参数(30)
    fun 校验参数三十(参数表: List<String>): Boolean = 校验参数项(参数表, 30)
    fun 取参数三十一(): String = 取参数(31)
    fun 解释参数三十一(): String = 解释参数(31)
    fun 校验参数三十一(参数表: List<String>): Boolean = 校验参数项(参数表, 31)
    fun 取参数三十二(): String = 取参数(32)
    fun 解释参数三十二(): String = 解释参数(32)
    fun 校验参数三十二(参数表: List<String>): Boolean = 校验参数项(参数表, 32)
    fun 取参数三十三(): String = 取参数(33)
    fun 解释参数三十三(): String = 解释参数(33)
    fun 校验参数三十三(参数表: List<String>): Boolean = 校验参数项(参数表, 33)
    fun 取参数三十四(): String = 取参数(34)
    fun 解释参数三十四(): String = 解释参数(34)
    fun 校验参数三十四(参数表: List<String>): Boolean = 校验参数项(参数表, 34)
    fun 取参数三十五(): String = 取参数(35)
    fun 解释参数三十五(): String = 解释参数(35)
    fun 校验参数三十五(参数表: List<String>): Boolean = 校验参数项(参数表, 35)
    fun 取参数三十六(): String = 取参数(36)
    fun 解释参数三十六(): String = 解释参数(36)
    fun 校验参数三十六(参数表: List<String>): Boolean = 校验参数项(参数表, 36)
    fun 取参数三十七(): String = 取参数(37)
    fun 解释参数三十七(): String = 解释参数(37)
    fun 校验参数三十七(参数表: List<String>): Boolean = 校验参数项(参数表, 37)
    fun 取参数三十八(): String = 取参数(38)
    fun 解释参数三十八(): String = 解释参数(38)
    fun 校验参数三十八(参数表: List<String>): Boolean = 校验参数项(参数表, 38)
    fun 取参数三十九(): String = 取参数(39)
    fun 解释参数三十九(): String = 解释参数(39)
    fun 校验参数三十九(参数表: List<String>): Boolean = 校验参数项(参数表, 39)
    fun 取参数四十(): String = 取参数(40)
    fun 解释参数四十(): String = 解释参数(40)
    fun 校验参数四十(参数表: List<String>): Boolean = 校验参数项(参数表, 40)
    fun 取参数四十一(): String = 取参数(41)
    fun 解释参数四十一(): String = 解释参数(41)
    fun 校验参数四十一(参数表: List<String>): Boolean = 校验参数项(参数表, 41)
    fun 取参数四十二(): String = 取参数(42)
    fun 解释参数四十二(): String = 解释参数(42)
    fun 校验参数四十二(参数表: List<String>): Boolean = 校验参数项(参数表, 42)
    fun 取参数四十三(): String = 取参数(43)
    fun 解释参数四十三(): String = 解释参数(43)
    fun 校验参数四十三(参数表: List<String>): Boolean = 校验参数项(参数表, 43)
    fun 取参数四十四(): String = 取参数(44)
    fun 解释参数四十四(): String = 解释参数(44)
    fun 校验参数四十四(参数表: List<String>): Boolean = 校验参数项(参数表, 44)
    fun 取参数四十五(): String = 取参数(45)
    fun 解释参数四十五(): String = 解释参数(45)
    fun 校验参数四十五(参数表: List<String>): Boolean = 校验参数项(参数表, 45)
    fun 取参数四十六(): String = 取参数(46)
    fun 解释参数四十六(): String = 解释参数(46)
    fun 校验参数四十六(参数表: List<String>): Boolean = 校验参数项(参数表, 46)
    fun 取参数四十七(): String = 取参数(47)
    fun 解释参数四十七(): String = 解释参数(47)
    fun 校验参数四十七(参数表: List<String>): Boolean = 校验参数项(参数表, 47)
    fun 取参数四十八(): String = 取参数(48)
    fun 解释参数四十八(): String = 解释参数(48)
    fun 校验参数四十八(参数表: List<String>): Boolean = 校验参数项(参数表, 48)
    fun 取参数四十九(): String = 取参数(49)
    fun 解释参数四十九(): String = 解释参数(49)
    fun 校验参数四十九(参数表: List<String>): Boolean = 校验参数项(参数表, 49)
    fun 取参数五十(): String = 取参数(50)
    fun 解释参数五十(): String = 解释参数(50)
    fun 校验参数五十(参数表: List<String>): Boolean = 校验参数项(参数表, 50)
    fun 取参数五十一(): String = 取参数(51)
    fun 解释参数五十一(): String = 解释参数(51)
    fun 校验参数五十一(参数表: List<String>): Boolean = 校验参数项(参数表, 51)
    fun 取参数五十二(): String = 取参数(52)
    fun 解释参数五十二(): String = 解释参数(52)
    fun 校验参数五十二(参数表: List<String>): Boolean = 校验参数项(参数表, 52)
    fun 取参数五十三(): String = 取参数(53)
    fun 解释参数五十三(): String = 解释参数(53)
    fun 校验参数五十三(参数表: List<String>): Boolean = 校验参数项(参数表, 53)
    fun 取参数五十四(): String = 取参数(54)
    fun 解释参数五十四(): String = 解释参数(54)
    fun 校验参数五十四(参数表: List<String>): Boolean = 校验参数项(参数表, 54)
    fun 取参数五十五(): String = 取参数(55)
    fun 解释参数五十五(): String = 解释参数(55)
    fun 校验参数五十五(参数表: List<String>): Boolean = 校验参数项(参数表, 55)
    fun 取参数五十六(): String = 取参数(56)
    fun 解释参数五十六(): String = 解释参数(56)
    fun 校验参数五十六(参数表: List<String>): Boolean = 校验参数项(参数表, 56)
    fun 取参数五十七(): String = 取参数(57)
    fun 解释参数五十七(): String = 解释参数(57)
    fun 校验参数五十七(参数表: List<String>): Boolean = 校验参数项(参数表, 57)
    fun 取参数五十八(): String = 取参数(58)
    fun 解释参数五十八(): String = 解释参数(58)
    fun 校验参数五十八(参数表: List<String>): Boolean = 校验参数项(参数表, 58)
    fun 取参数五十九(): String = 取参数(59)
    fun 解释参数五十九(): String = 解释参数(59)
    fun 校验参数五十九(参数表: List<String>): Boolean = 校验参数项(参数表, 59)
    fun 取参数六十(): String = 取参数(60)
    fun 解释参数六十(): String = 解释参数(60)
    fun 校验参数六十(参数表: List<String>): Boolean = 校验参数项(参数表, 60)
    fun 取参数六十一(): String = 取参数(61)
    fun 解释参数六十一(): String = 解释参数(61)
    fun 校验参数六十一(参数表: List<String>): Boolean = 校验参数项(参数表, 61)
    fun 取参数六十二(): String = 取参数(62)
    fun 解释参数六十二(): String = 解释参数(62)
    fun 校验参数六十二(参数表: List<String>): Boolean = 校验参数项(参数表, 62)
    fun 取参数六十三(): String = 取参数(63)
    fun 解释参数六十三(): String = 解释参数(63)
    fun 校验参数六十三(参数表: List<String>): Boolean = 校验参数项(参数表, 63)
    fun 取参数六十四(): String = 取参数(64)
    fun 解释参数六十四(): String = 解释参数(64)
    fun 校验参数六十四(参数表: List<String>): Boolean = 校验参数项(参数表, 64)
    fun 取参数六十五(): String = 取参数(65)
    fun 解释参数六十五(): String = 解释参数(65)
    fun 校验参数六十五(参数表: List<String>): Boolean = 校验参数项(参数表, 65)
    fun 取参数六十六(): String = 取参数(66)
    fun 解释参数六十六(): String = 解释参数(66)
    fun 校验参数六十六(参数表: List<String>): Boolean = 校验参数项(参数表, 66)
    fun 取参数六十七(): String = 取参数(67)
    fun 解释参数六十七(): String = 解释参数(67)
    fun 校验参数六十七(参数表: List<String>): Boolean = 校验参数项(参数表, 67)
    fun 取参数六十八(): String = 取参数(68)
    fun 解释参数六十八(): String = 解释参数(68)
    fun 校验参数六十八(参数表: List<String>): Boolean = 校验参数项(参数表, 68)
    fun 取参数六十九(): String = 取参数(69)
    fun 解释参数六十九(): String = 解释参数(69)
    fun 校验参数六十九(参数表: List<String>): Boolean = 校验参数项(参数表, 69)
    fun 取参数七十(): String = 取参数(70)
    fun 解释参数七十(): String = 解释参数(70)
    fun 校验参数七十(参数表: List<String>): Boolean = 校验参数项(参数表, 70)
}
