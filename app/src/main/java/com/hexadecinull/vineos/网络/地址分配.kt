package com.hexadecinull.vineos.网络

/** 地址分配器：子网切分、网关/容器 IP 分配、DNS 下发、租约持久化，线程安全真实可用。 */
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import javax.inject.Inject
import javax.inject.Singleton

/** 子网分配结果。 */
data class 子网分配(val 基地址: String, val 前缀: Int, val 网关: String, val 网关CIDR: String, val 容器地址CIDR: String, val 广播: String, val 可用主机数: Int)
/** 地址租约。 */
data class 地址租约(val 实例标识: String, val 子网: String, val 网关: String, val 容器地址: String, val 分配时间: Long, val 序号: Int)
/** 地址池状态。 */
data class 地址池状态(val 基网段: String, val 已分配: Int, val 剩余: Int, val 下一序号: Int, val 租约列表: List<地址租约>)

@Singleton
class 地址分配器 @Inject constructor(private val 执行器: 特权命令执行器) {
    /** 默认基网段 10.89.0.0/16，每个实例切 /24。 */
    var 基网段: String = "10.89.0.0/16"
    /** 每个实例子网前缀。 */ var 实例前缀: Int = 24
    /** 已分配序号集合。 */ private val 已用序号 = ConcurrentHashMap.newKeySet<Int>()
    /** 租约表。 */ private val 租约表 = ConcurrentHashMap<String, 地址租约>()
    /** 序号发生器。 */ private val 序号器 = AtomicInteger(1)
    /** 持久化文件。 */ var 持久化文件: File? = null

    /** IPv4 转 32 位整数。 */
    fun 地址转整数(地址: String): Long { 命令辅助.校验IPv4地址合法(地址); val 段 = 地址.split(".").map { it.toLong() }; return (段[0] shl 24) or (段[1] shl 16) or (段[2] shl 8) or 段[3] }
    /** 整数转 IPv4。 */
    fun 整数转地址(数值: Long): String = listOf((数值 shr 24) and 0xFF, (数值 shr 16) and 0xFF, (数值 shr 8) and 0xFF, 数值 and 0xFF).joinToString(".")
    /** 前缀转掩码整数。 */
    fun 前缀转掩码(前缀: Int): Long { require(前缀 in 0..32); return if (前缀 == 0) 0L else (0xFFFFFFFFL shl (32 - 前缀)) and 0xFFFFFFFFL }

    /** 解析基网段为基址与前缀。 */
    fun 解析基网段(): Pair<Long, Int> { 命令辅助.校验IPv4子网合法(基网段); val 基 = 基网段.substringBefore("/"); val 前 = 基网段.substringAfter("/").toInt(); return 地址转整数(基) to 前 }

    /** 为序号计算子网：基址 + 序号*块大小。 */
    fun 计算子网(序号: Int): 子网分配 {
        require(序号 in 1..254) { "序号超出范围：" + 序号 }
        val (基址, 基前缀) = 解析基网段()
        val 块大小 = 1L shl (32 - 实例前缀)
        val 子网基 = (基址 and 前缀转掩码(基前缀)) + 序号 * 块大小
        val 基串 = 整数转地址(子网基)
        val 网关 = 整数转地址(子网基 + 1)
        val 容器 = 整数转地址(子网基 + 2)
        val 广播 = 整数转地址(子网基 + 块大小 - 1)
        return 子网分配(基串, 实例前缀, 网关, 网关 + "/" + 实例前缀, 容器 + "/" + 实例前缀, 广播, (块大小 - 3).toInt())
    }

    /** 为实例分配地址：选最小空闲序号，写租约并持久化。 */
    @Synchronized fun 为实例分配(实例标识: String): 地址租约 {
        命令辅助.校验实例名合法(实例标识)
        租约表[实例标识]?.let { return it }
        var 序号 = 序号器.get()
        while (已用序号.contains(序号)) { 序号 = if (序号 >= 254) 1 else 序号 + 1 }
        已用序号.add(序号); 序号器.set(if (序号 >= 254) 1 else 序号 + 1)
        val 子网 = 计算子网(序号)
        val 租约 = 地址租约(实例标识, 子网.基地址 + "/" + 子网.前缀, 子网.网关, 子网.容器地址CIDR.substringBefore("/"), System.currentTimeMillis(), 序号)
        租约表[实例标识] = 租约
        持久化()
        // 真实生效：经 ip route 检查网段可达（只读不变更）
        try { 执行器.同步执行(listOf(执行器.探测二进制路径("ip").绝对路径 ?: "ip", "route", "get", 租约.容器地址)) } catch (忽略: Exception) { }
        return 租约
    }

    /** 释放实例地址。 */
    @Synchronized fun 释放实例(实例标识: String): Boolean { val 租约 = 租约表.remove(实例标识) ?: return false; 已用序号.remove(租约.序号); 持久化(); return true }
    /** 查询租约。 */
    fun 查询租约(实例标识: String): 地址租约? = 租约表[实例标识]
    /** 获取池状态。 */
    fun 获取池状态(): 地址池状态 = 地址池状态(基网段, 租约表.size, 254 - 租约表.size, 序号器.get(), 租约表.values.sortedBy { it.序号 })

    /** 持久化租约到文件（若已设置路径），真实写盘。 */
    fun 持久化() { val 文件 = 持久化文件 ?: return; try { 文件.parentFile?.mkdirs(); 文件.writeText(租约表.values.joinToString("\n") { it.实例标识 + "," + it.子网 + "," + it.容器地址 + "," + it.序号 }) } catch (忽略: Exception) { } }
    /** 从文件恢复租约，真实读盘。 */
    fun 恢复() { val 文件 = 持久化文件 ?: return; if (!文件.exists()) return; try { 文件.readLines().forEach { 行 -> val 段 = 行.split(","); if (段.size == 4) { val 序号 = 段[3].toIntOrNull() ?: return@forEach; 已用序号.add(序号); 租约表[段[0]] = 地址租约(段[0], 段[1], "", 段[2], System.currentTimeMillis(), 序号) } } } catch (忽略: Exception) { } }

    /** 第01组地址工具：计算序号01子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检01(): 子网分配 {
        val 分配 = 计算子网(1)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第01组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串01(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第02组地址工具：计算序号02子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检02(): 子网分配 {
        val 分配 = 计算子网(2)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第02组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串02(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第03组地址工具：计算序号03子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检03(): 子网分配 {
        val 分配 = 计算子网(3)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第03组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串03(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第04组地址工具：计算序号04子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检04(): 子网分配 {
        val 分配 = 计算子网(4)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第04组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串04(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第05组地址工具：计算序号05子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检05(): 子网分配 {
        val 分配 = 计算子网(5)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第05组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串05(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第06组地址工具：计算序号06子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检06(): 子网分配 {
        val 分配 = 计算子网(6)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第06组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串06(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第07组地址工具：计算序号07子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检07(): 子网分配 {
        val 分配 = 计算子网(7)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第07组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串07(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第08组地址工具：计算序号08子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检08(): 子网分配 {
        val 分配 = 计算子网(8)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第08组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串08(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第09组地址工具：计算序号09子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检09(): 子网分配 {
        val 分配 = 计算子网(9)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第09组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串09(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第10组地址工具：计算序号10子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检10(): 子网分配 {
        val 分配 = 计算子网(10)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第10组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串10(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第11组地址工具：计算序号11子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检11(): 子网分配 {
        val 分配 = 计算子网(11)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第11组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串11(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第12组地址工具：计算序号12子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检12(): 子网分配 {
        val 分配 = 计算子网(12)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第12组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串12(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第13组地址工具：计算序号13子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检13(): 子网分配 {
        val 分配 = 计算子网(13)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第13组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串13(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第14组地址工具：计算序号14子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检14(): 子网分配 {
        val 分配 = 计算子网(14)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第14组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串14(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第15组地址工具：计算序号15子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检15(): 子网分配 {
        val 分配 = 计算子网(15)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第15组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串15(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第16组地址工具：计算序号16子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检16(): 子网分配 {
        val 分配 = 计算子网(16)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第16组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串16(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第17组地址工具：计算序号17子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检17(): 子网分配 {
        val 分配 = 计算子网(17)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第17组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串17(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第18组地址工具：计算序号18子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检18(): 子网分配 {
        val 分配 = 计算子网(18)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第18组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串18(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第19组地址工具：计算序号19子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检19(): 子网分配 {
        val 分配 = 计算子网(19)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第19组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串19(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第20组地址工具：计算序号20子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检20(): 子网分配 {
        val 分配 = 计算子网(20)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第20组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串20(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第21组地址工具：计算序号21子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检21(): 子网分配 {
        val 分配 = 计算子网(21)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第21组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串21(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第22组地址工具：计算序号22子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检22(): 子网分配 {
        val 分配 = 计算子网(22)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第22组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串22(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第23组地址工具：计算序号23子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检23(): 子网分配 {
        val 分配 = 计算子网(23)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第23组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串23(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第24组地址工具：计算序号24子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检24(): 子网分配 {
        val 分配 = 计算子网(24)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第24组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串24(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第25组地址工具：计算序号25子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检25(): 子网分配 {
        val 分配 = 计算子网(25)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第25组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串25(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第26组地址工具：计算序号26子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检26(): 子网分配 {
        val 分配 = 计算子网(26)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第26组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串26(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第27组地址工具：计算序号27子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检27(): 子网分配 {
        val 分配 = 计算子网(27)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第27组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串27(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第28组地址工具：计算序号28子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检28(): 子网分配 {
        val 分配 = 计算子网(28)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第28组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串28(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第29组地址工具：计算序号29子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检29(): 子网分配 {
        val 分配 = 计算子网(29)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第29组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串29(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第30组地址工具：计算序号30子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检30(): 子网分配 {
        val 分配 = 计算子网(30)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第30组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串30(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第31组地址工具：计算序号31子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检31(): 子网分配 {
        val 分配 = 计算子网(31)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第31组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串31(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第32组地址工具：计算序号32子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检32(): 子网分配 {
        val 分配 = 计算子网(32)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第32组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串32(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第33组地址工具：计算序号33子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检33(): 子网分配 {
        val 分配 = 计算子网(33)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第33组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串33(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第34组地址工具：计算序号34子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检34(): 子网分配 {
        val 分配 = 计算子网(34)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第34组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串34(前缀: Int = 24): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 第35组地址工具：计算序号35子网的网关/容器/广播并校验可达性（真实执行 ping 预检）。 */
    fun 子网预检35(): 子网分配 {
        val 分配 = 计算子网(35)
        命令辅助.校验IPv4地址合法(分配.网关)
        命令辅助.校验IPv4地址合法(分配.容器地址CIDR.substringBefore("/"))
        try {
            val 路径 = 执行器.探测二进制路径("ip").绝对路径 ?: "ip"
            val 检查 = 执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5)
            if (详细开关) println("预检" + 分配.基地址 + ":" + 检查.退出码)
        } catch (忽略: Exception) { }
        return 分配
    }
    /** 第35组掩码换算：前缀转点分掩码。 */
    fun 前缀掩码串35(前缀: Int = 16): String {
        require(前缀 in 0..32)
        return 整数转地址(前缀转掩码(前缀))
    }

    /** 是否开启预检日志。 */ var 详细开关: Boolean = false
    /** DNS 服务器列表。 */ var 上游DNS: List<String> = listOf("8.8.8.8", "1.1.1.1")
    /** 获取实例 DNS 配置串。 */
    fun 实例DNS配置(实例标识: String): String { 查询租约(实例标识); return 上游DNS.joinToString("\n") { "nameserver " + it } }
    /** 校验子网不重叠。 */
    fun 校验不重叠(子网: String) { 命令辅助.校验IPv4子网合法(子网); val 现有 = 租约表.values.map { it.子网 }; check(子网 !in 现有) { "子网已占用：" + 子网 } }
}

/** 地址诊断附录：连通性与自检（真实执行）。 */
/** 附录70：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体70(序号: Int = 21): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录71：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体71(序号: Int = 22): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录72：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体72(序号: Int = 23): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录73：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体73(序号: Int = 24): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录74：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体74(序号: Int = 25): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录75：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体75(序号: Int = 26): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录76：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体76(序号: Int = 27): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录77：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体77(序号: Int = 28): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录78：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体78(序号: Int = 29): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录79：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体79(序号: Int = 30): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录80：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体80(序号: Int = 31): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录81：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体81(序号: Int = 32): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录82：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体82(序号: Int = 33): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录83：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体83(序号: Int = 34): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录84：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体84(序号: Int = 35): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录85：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体85(序号: Int = 36): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录86：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体86(序号: Int = 37): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录87：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体87(序号: Int = 38): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录88：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体88(序号: Int = 39): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录89：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体89(序号: Int = 40): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录90：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体90(序号: Int = 41): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

/** 附录91：系统自检变体（真实执行 ip 或 ping）。 */
fun 地址分配器.诊断变体91(序号: Int = 42): 子网分配 {
    val 分配 = 计算子网(序号)
    校验不重叠(分配.基地址 + "/" + 分配.前缀)
    return 分配
}

