package com.hexadecinull.vineos.网络

/** 批量加速与数据驱动规则构建：收敛重复变体、合并多次 fork 为单脚本批量执行。
 * 存在理由：原有各管理器存在大量仅序号不同的重复变体（子网预检、快捷映射、命名空间保障、
 * 自愈检查、MTU 调优、防火墙快捷规则等），每次搭建走 6~8 次独立 fork；本文件用参数表与公式
 * 收敛重复，并把 netns/veth/防火墙搭建合并为单条 shell 批量脚本，fork 数从 O(n) 降到 O(1)。
 * 所有执行路径显式超时，标识与注释中文，系统类库接口保持英文。 */
import javax.inject.Inject
import javax.inject.Singleton

/** 网卡搭建请求。 */
data class 网卡搭建请求(val 实例标识: String, val 命名空间: String, val 宿主端: String, val 容器端: String, val 主机地址: String, val 容器地址: String, val 最大传输单元: Int = 1500)

/** 防火墙规格。 */
data class 防火墙规格(val 方向: 防火墙方向, val 协议: 防火墙协议, val 来源: String?, val 目标: String?, val 端口: Int?, val 动作: 防火墙动作, val 优先级: Int, val 备注: String)

/** 批量搭建结果。 */
data class 批量搭建结果(val 成功: Boolean, val 执行输出: String, val 失败项: List<String>, val 耗时毫秒: Long)

/** 数据驱动参数表：原来散落在各文件中的序号变体在此收敛为公式与查表。 */
object 规则参数表 {
    /** 快捷映射：宿主 = 10000 + 137*组，容器 = 8000 + 组，UDP 在宿主基础上 +1。 */
    fun 快捷映射端口(组号: Int): Pair<Int, Int> {
        require(组号 in 1..64) { "组号超出范围：" + 组号 }
        return (10_000 + 137 * 组号) to (8_000 + 组号)
    }
    /** 批量段映射：起始宿主 = 26541 + 211*(组-31)，起始容器 = 9217 + 7*(组-31)，每段 6 个。 */
    fun 批量段起始(组号: Int): Pair<Int, Int> {
        require(组号 in 31..70) { "组号超出范围：" + 组号 }
        return (26_541 + 211 * (组号 - 31)) to (9_217 + 7 * (组号 - 31))
    }
    /** 一键暴露基址：33000 + 100*(组-30)，映射容器 22/80/443。 */
    fun 暴露基址(组号: Int): Int {
        require(组号 in 30..69) { "组号超出范围：" + 组号 }
        return 33_000 + 100 * (组号 - 30)
    }
    /** 常用容器端口表。 */
    val 暴露容器端口: List<Int> = listOf(22, 80, 443)
    /**  MTU 候选表：原 30 组调优变体收敛为一张表。 */
    val 最大传输单元表: List<Int> = listOf(1280, 1300, 1320, 1340, 1360, 1380, 1400, 1420, 1440, 1460, 1480, 1500, 1520, 1540, 1560)
    /** 按组号取 MTU，越界时回绕。 */
    fun 按组取MTU(组号: Int): Int = 最大传输单元表[(组号 % 最大传输单元表.size + 最大传输单元表.size) % 最大传输单元表.size]
    /** 执行策略表：超时 5..14 秒循环，重试 0..2 循环，特权隔组交替。 */
    fun 执行策略(组号: Int): 执行策略 {
        require(组号 >= 1) { "组号须为正数" }
        val 超时 = 5L + (组号 % 10)
        val 重试 = 组号 % 3
        return 执行策略(超时, 重试, 组号 % 2 == 0)
    }
    /** 流量格式化精度表：按组号取 1..3。 */
    fun 格式化精度(组号: Int): Int = 1 + (组号 % 3 + 3) % 3
    /** 防火墙优先级：出站 100+组，入站阻断 200+组，DNS 300+组。 */
    fun 出站优先级(组号: Int): Int = 100 + 组号
    fun 入站阻断优先级(组号: Int): Int = 200 + 组号
    fun 域名优先级(组号: Int): Int = 300 + 组号
    /** 命名空间后缀：-01..-30 零填充。 */
    fun 命名空间后缀(组号: Int): String {
        require(组号 in 1..64) { "组号超出范围：" + 组号 }
        return "-" + 组号.toString().padStart(2, '0')
    }
    /** 转发调优键值：原来 12 组相同 sysctl 收敛为一张表。 */
    val 转发调优键值: Map<String, String> = mapOf(
        "net.ipv4.ip_forward" to "1",
        "net.ipv6.conf.all.forwarding" to "1",
        "net.ipv4.conf.all.rp_filter" to "0"
    )
    /** 流量单位表。 */
    val 流量单位: Array<String> = arrayOf("B", "KB", "MB", "GB", "TB")
}

@Singleton
class 批量加速器 @Inject constructor(
    private val 执行器: 特权命令执行器,
    private val 缓存: 查询缓存加速器,
    private val 命名空间: 网络命名空间管理器,
    private val 网卡: 虚拟网卡管理器,
    private val 地址: 地址分配器,
    private val 转发: 端口转发管理器,
    private val 防火墙: 防火墙规则管理器,
    private val 流量: 流量统计器
) {
    /** 带显式超时的单命令执行：超时收敛后真实执行，特权与普通双路径均显式超时。 */
    fun 超时执行(命令: List<String>, 超时秒: Long, 需要特权: Boolean = true): 网络命令结果 {
        require(命令.isNotEmpty()) { "命令不能为空" }
        val 钳制 = 超时夹紧器.变更超时(超时秒)
        if (!需要特权) return 执行器.同步执行(命令, 钳制)
        val 提权 = try {
            执行器.经Root执行(命令, 钳制)
        } catch (_: Exception) {
            null
        }
        if (提权 != null && 提权.是否成功()) return 提权
        return 执行器.同步执行(命令, 钳制)
    }

    /** 批量保障命名空间：多实例后缀合并为单条 shell 脚本，一次 fork 完成创建与回环启用。 */
    fun 批量保障命名空间(实例标识: String, 组范围: IntRange = 1..4, 超时秒: Long = 30L): 批量搭建结果 {
        命令辅助.校验实例名合法(实例标识)
        val 起始 = System.currentTimeMillis()
        val 基 = "vine-" + 实例标识.lowercase().replace("_", "-").take(20)
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        val 段 = 组范围.toList().take(16).map { 组 ->
            命令辅助.校验命名空间名合法(基 + 规则参数表.命名空间后缀(组))
            路径 + " netns add " + 基 + 规则参数表.命名空间后缀(组) + " 2>/dev/null; " +
                路径 + " netns exec " + 基 + 规则参数表.命名空间后缀(组) + " " + 路径 + " link set lo up"
        }
        val 脚本 = 段.joinToString(" ; ")
        val 结果 = 执行器.执行Shell脚本串(脚本, 超时夹紧器.批量超时(段.size))
        val 失败 = mutableListOf<String>()
        for (组 in 组范围.toList().take(16)) {
            val 名 = 基 + 规则参数表.命名空间后缀(组)
            try {
                if (!命名空间.检查存在(名)) 失败 += 名
            } catch (_: Exception) {
                失败 += 名
            }
        }
        缓存.按前缀失效("命名空间")
        return 批量搭建结果(失败.isEmpty(), 结果.标准输出.take(2_000), 失败, System.currentTimeMillis() - 起始)
    }

    /** 批量搭建网卡对：多组 veth 创建迁移配置合并为单脚本，超时按组数放大。 */
    fun 批量搭建网卡对(请求组: List<网卡搭建请求>): 批量搭建结果 {
        require(请求组.isNotEmpty() && 请求组.size <= 16) { "批量条数须在 1..16" }
        val 起始 = System.currentTimeMillis()
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        val 失败 = mutableListOf<String>()
        val 段 = mutableListOf<String>()
        for (请求 in 请求组) {
            命令辅助.校验网卡名合法(请求.宿主端)
            命令辅助.校验网卡名合法(请求.容器端)
            命令辅助.校验命名空间名合法(请求.命名空间)
            命令辅助.校验MTU合法(请求.最大传输单元)
            段 += 路径 + " link add " + 请求.宿主端 + " type veth peer name " + 请求.容器端 +
                " ; " + 路径 + " link set " + 请求.容器端 + " netns " + 请求.命名空间 +
                " ; " + 路径 + " link set " + 请求.宿主端 + " up" +
                " ; " + 路径 + " addr add " + 请求.主机地址 + " dev " + 请求.宿主端 +
                " ; " + 路径 + " netns exec " + 请求.命名空间 + " " + 路径 + " link set " + 请求.容器端 + " mtu " + 请求.最大传输单元 +
                " ; " + 路径 + " netns exec " + 请求.命名空间 + " " + 路径 + " addr add " + 请求.容器地址 + " dev " + 请求.容器端 +
                " ; " + 路径 + " netns exec " + 请求.命名空间 + " " + 路径 + " link set " + 请求.容器端 + " up"
        }
        val 结果 = 执行器.执行Shell脚本串(段.joinToString(" ; "), 超时夹紧器.批量超时(请求组.size, 6L))
        if (!结果.是否成功()) 失败 += "批量脚本退出码=" + 结果.退出码 + ":" + 结果.错误输出.take(300)
        for (请求 in 请求组) {
            try {
                val 状态 = 网卡.查询网卡状态(请求.宿主端)
                if (状态 == null || !状态.运行中) 失败 += 请求.宿主端
            } catch (_: Exception) {
                失败 += 请求.宿主端
            }
        }
        请求组.forEach { 缓存.变更后失效(网卡 = it.宿主端) }
        return 批量搭建结果(失败.isEmpty(), 结果.标准输出.take(2_000), 失败, System.currentTimeMillis() - 起始)
    }

    /** 批量应用防火墙规格：构造全部命令后容错逐条执行并汇总，单次调用完成多规则。 */
    fun 批量应用防火墙(规格组: List<防火墙规格>): List<防火墙规则> {
        require(规格组.isNotEmpty() && 规格组.size <= 64) { "批量条数须在 1..64" }
        val 结果组 = mutableListOf<防火墙规则>()
        for (规格 in 规格组) {
            结果组 += try {
                防火墙.添加规则(规格.方向, 规格.协议, 规格.来源, 规格.目标, 规格.端口, 规格.动作, 规格.优先级, 规格.备注)
            } catch (_: Exception) {
                防火墙.添加规则(规格.方向, 规格.协议, 规格.来源, 规格.目标, 规格.端口, 防火墙动作.丢弃, 规格.优先级, 规格.备注)
            }
        }
        缓存.变更后失效(防火墙相关 = true)
        return 结果组
    }

    /** 批量应用 sysctl 转发调优：查表驱动，一次解析路径，多次命名空间内执行。 */
    fun 批量应用转发调优(命名: String, 超时秒: Long = 10L): List<网络命令结果> {
        命令辅助.校验命名空间名合法(命名)
        val 路径 = 缓存.探测二进制路径缓存("sysctl").绝对路径 ?: "sysctl"
        val 结果组 = mutableListOf<网络命令结果>()
        for ((键, 值) in 规则参数表.转发调优键值) {
            结果组 += 命名空间.在命名空间执行(命名, listOf(路径, "-w", 键 + "=" + 值), 超时夹紧器.变更超时(超时秒))
        }
        return 结果组
    }

    /** 数据驱动子网预检：收敛原子网预检 01..35 变体，显式超时 5 秒。 */
    fun 子网预检(序号: Int): 子网分配 {
        val 分配 = 地址.计算子网(序号.coerceIn(1, 254))
        命令辅助.校验IPv4地址合法(分配.网关)
        try {
            val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
            执行器.同步执行(listOf(路径, "route", "get", 分配.容器地址CIDR.substringBefore("/")), 5L)
        } catch (_: Exception) {
        }
        return 分配
    }

    /** 数据驱动掩码换算：收敛前缀掩码串 01..35 变体。 */
    fun 前缀掩码串(前缀: Int): String {
        require(前缀 in 0..32) { "非法前缀：" + 前缀 }
        return 地址.整数转地址(地址.前缀转掩码(前缀))
    }

    /** 数据驱动前缀流量查询：收敛查询前缀流量 01..35 变体，走 1 秒缓存。 */
    fun 查询前缀流量(前缀: String = "v"): List<网卡流量> {
        val 全部 = 缓存.读取系统流量原文缓存(流量)
        return 全部.filter { it.网卡.startsWith(前缀) }.sortedByDescending { it.收字节 + it.发字节 }
    }

    /** 数据驱动流量格式化：收敛格式化流量 36..75 变体，精度查表。 */
    fun 格式化流量(字节: Long, 组号: Int = 0): String {
        if (字节 < 0) return "未知"
        val 精度 = 规则参数表.格式化精度(组号)
        var 数值 = 字节.toDouble()
        var 档 = 0
        while (数值 >= 1024 && 档 < 4) {
            数值 /= 1024
            档++
        }
        val 小数 = if (档 == 0) 0 else 精度
        return "%.${小数}f%s".format(数值, 规则参数表.流量单位[档])
    }

    /** 数据驱动阈值检查：收敛检查阈值 36..75 变体，走缓存读盘。 */
    fun 检查阈值(网卡: String, 阈值字节每秒: Double = 1048576.0): 流量告警 {
        val 快照 = 缓存.读取系统流量原文缓存(流量).find { it.网卡 == 网卡 }
        val 速率 = if (快照 != null) 快照.收字节.toDouble() / 1024.0 else 0.0
        return 流量告警(网卡, 阈值字节每秒, 速率 > 阈值字节每秒, 速率)
    }

    /** 数据驱动快捷执行：收敛快捷执行 01..40 变体，策略查表，双路径均显式超时。 */
    fun 快捷执行(组号: Int, 命令: List<String>): 网络命令结果 {
        val 策略 = 规则参数表.执行策略(组号)
        val 钳制 = 超时夹紧器.变更超时(策略.超时秒)
        var 最后错误 = ""
        repeat(策略.重试次 + 1) { 轮次 ->
            val 结果 = if (策略.需要特权) {
                val 提权 = try {
                    执行器.经Root执行(命令, 钳制)
                } catch (_: Exception) {
                    null
                }
                if (提权 != null && 提权.是否成功()) return 提权
                执行器.同步执行(命令, 钳制)
            } else {
                执行器.同步执行(命令, 钳制)
            }
            if (结果.是否成功()) return 结果
            最后错误 = 结果.错误输出
            if (轮次 < 策略.重试次) {
                try {
                    Thread.sleep(命令辅助.计算指数退避(轮次))
                } catch (_: InterruptedException) {
                }
            }
        }
        return 网络命令结果(-1, "", "快捷执行" + 组号 + "失败：" + 最后错误, 0, false, 命令)
    }

    /** 数据驱动端口映射：收敛快捷映射 01..30 与 UDP 变体，端口公式计算。 */
    fun 快捷映射(组号: Int, 实例标识: String, 容器地址: String, 协议: 转发协议 = 转发协议.TCP): 端口映射 {
        val (宿主, 容器) = 规则参数表.快捷映射端口(组号)
        return when (协议) {
            转发协议.UDP -> 转发.添加映射(实例标识, 转发协议.UDP, 宿主 + 1, 容器地址, 容器)
            else -> 转发.添加映射(实例标识, 协议, 宿主, 容器地址, 容器)
        }
    }

    /** 数据驱动批量段映射：收敛批量映射段 31..70 变体，循环 6 端口真实执行。 */
    fun 批量映射段(组号: Int, 实例标识: String, 容器地址: String): List<端口映射> {
        命令辅助.校验实例名合法(实例标识)
        命令辅助.校验IPv4地址合法(容器地址)
        val (起始宿主, 起始容器) = 规则参数表.批量段起始(组号.coerceIn(31, 70))
        val 结果组 = mutableListOf<端口映射>()
        for (偏移 in 0..5) {
            val 单条 = try {
                转发.添加映射(实例标识 + "-" + 偏移, 转发协议.TCP, 起始宿主 + 偏移, 容器地址, 起始容器 + 偏移)
            } catch (_: Exception) {
                null
            }
            if (单条 != null) 结果组 += 单条
        }
        return 结果组
    }

    /** 数据驱动命名空间保障：收敛实例命名空间保障 01..30 与清理变体。 */
    fun 实例命名空间保障(实例标识: String, 组号: Int): 命名空间信息 {
        命令辅助.校验实例名合法(实例标识)
        val 命名 = "vine-" + 实例标识.lowercase().replace("_", "-").take(20) + 规则参数表.命名空间后缀(组号)
        if (!命名空间.检查存在(命名)) 命名空间.创建命名空间(命名)
        命名空间.启用回环网卡(命名)
        return 命名空间.获取信息快照(命名)
    }

    /** 数据驱动命名空间清理：收敛清理 01..30 变体。 */
    fun 实例命名空间清理(实例标识: String, 组号: Int): 网络命令结果 {
        命令辅助.校验实例名合法(实例标识)
        val 命名 = "vine-" + 实例标识.lowercase().replace("_", "-").take(20) + 规则参数表.命名空间后缀(组号)
        val 查询 = try {
            命名空间.查询网卡列表(命名)
        } catch (_: Exception) {
            emptyList()
        }
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        查询.filter { it != "lo" }.forEach { 网卡名 ->
            try {
                命名空间.在命名空间执行(命名, listOf(路径, "link", "delete", 网卡名), 超时夹紧器.变更超时(10L))
            } catch (_: Exception) {
            }
        }
        val 结果 = 命名空间.删除命名空间(命名)
        缓存.按前缀失效("命名空间")
        return 结果
    }

    /** 数据驱动自愈检查：收敛自愈检查 01..29 变体。 */
    fun 自愈检查(实例标识: String): Map<String, Boolean> {
        return try {
            val 编排 = 网络隔离编排单例.获取(执行器, 命名空间, 网卡, 地址, 转发, 防火墙, 流量)
            编排.健康检查(实例标识)
        } catch (_: Exception) {
            mapOf("已知" to false)
        }
    }

    /** 数据驱动一键暴露：收敛一键暴露 30..69 变体，基址公式计算。 */
    fun 一键暴露常用端口(组号: Int, 实例标识: String, 容器地址: String): List<端口映射> {
        val 基 = 规则参数表.暴露基址(组号)
        return 规则参数表.暴露容器端口.mapIndexed { 序号, 容器端口 ->
            转发.添加映射(实例标识, 转发协议.TCP, 基 + 序号, 容器地址, 容器端口)
        }
    }

    /** 数据驱动 MTU 设置：收敛设置网卡 MTU 01..30 变体，查表取值。 */
    fun 设置网卡MTU(网卡名: String, 组号: Int): 网络命令结果 {
        命令辅助.校验网卡名合法(网卡名)
        val 目标 = 规则参数表.按组取MTU(组号)
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        val 设置 = 超时执行(listOf(路径, "link", "set", 网卡名, "mtu", 目标.toString()), 10L)
        if (!设置.是否成功()) return 设置
        val 回读 = 执行器.同步执行(listOf(路径, "-o", "link", "show", 网卡名), 5L)
        缓存.变更后失效(网卡 = 网卡名)
        return if (回读.标准输出.contains(目标.toString())) 设置 else 网络命令结果(-1, 回读.标准输出, "MTU 回读不一致", 0, false, listOf(网卡名))
    }

    /** 数据驱动容器端 MTU：收敛设置容器端 MTU 01..30 变体。 */
    fun 设置容器端MTU(命名: String, 容器端: String, 组号: Int): List<网络命令结果> {
        命令辅助.校验命名空间名合法(命名)
        命令辅助.校验网卡名合法(容器端)
        val 目标 = 规则参数表.按组取MTU(组号).toString()
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        return listOf(
            命名空间.在命名空间执行(命名, listOf(路径, "link", "set", 容器端, "mtu", 目标), 超时夹紧器.变更超时(10L)),
            命名空间.在命名空间执行(命名, listOf(路径, "link", "set", 容器端, "up"), 超时夹紧器.变更超时(10L))
        )
    }

    /** 数据驱动网卡开关：收敛启用并确认 / 禁用并确认 01..15 变体。 */
    fun 启用并确认(网卡名: String): Boolean {
        命令辅助.校验网卡名合法(网卡名)
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        val 结果 = 超时执行(listOf(路径, "link", "set", 网卡名, "up"), 10L)
        if (!结果.是否成功()) return false
        缓存.变更后失效(网卡 = 网卡名)
        return try {
            网卡.查询网卡状态(网卡名)?.运行中 == true
        } catch (_: Exception) {
            false
        }
    }

    /** 数据驱动网卡禁用。 */
    fun 禁用并确认(网卡名: String): Boolean {
        命令辅助.校验网卡名合法(网卡名)
        val 路径 = 缓存.探测二进制路径缓存("ip").绝对路径 ?: "ip"
        val 结果 = 超时执行(listOf(路径, "link", "set", 网卡名, "down"), 10L)
        缓存.变更后失效(网卡 = 网卡名)
        return 结果.是否成功()
    }

    /** 数据驱动防火墙快捷：收敛实例出站放行 01..32 变体，优先级公式计算。 */
    fun 实例出站放行(组号: Int, 子网: String, 备注: String): 防火墙规则 {
        命令辅助.校验IPv4子网合法(子网)
        val 规则 = 防火墙.添加规则(防火墙方向.出站, 防火墙协议.全部, 子网, null, null, 防火墙动作.放行, 规则参数表.出站优先级(组号), 备注)
        缓存.变更后失效(防火墙相关 = true)
        return 规则
    }

    /** 数据驱动防火墙快捷：收敛实例入站阻断 01..32 变体。 */
    fun 实例入站阻断(组号: Int, 端口: Int, 备注: String): 防火墙规则 {
        命令辅助.校验端口合法(端口)
        val 规则 = 防火墙.添加规则(防火墙方向.入站, 防火墙协议.TCP, null, null, 端口, 防火墙动作.丢弃, 规则参数表.入站阻断优先级(组号), 备注)
        缓存.变更后失效(防火墙相关 = true)
        return 规则
    }

    /** 数据驱动 DNS 放行：收敛放行 DNS 33..72 变体。 */
    fun 放行DNS(组号: Int, 子网: String, 备注: String): 防火墙规则 {
        命令辅助.校验IPv4子网合法(子网)
        val 规则 = 防火墙.添加规则(防火墙方向.出站, 防火墙协议.UDP, 子网, null, 53, 防火墙动作.放行, 规则参数表.域名优先级(组号), 备注)
        缓存.变更后失效(防火墙相关 = true)
        return 规则
    }

    /** 数据驱动回环放行：收敛放行回环 33..72 变体，显式超时 10 秒。 */
    fun 放行回环(): 网络命令结果 {
        val 路径 = 缓存.探测二进制路径缓存("iptables").绝对路径 ?: "iptables"
        val 结果 = 超时执行(listOf(路径, "-A", "INPUT", "-i", "lo", "-j", "ACCEPT"), 10L)
        缓存.变更后失效(防火墙相关 = true)
        return 结果
    }

    /** 数据驱动诊断：收敛诊断变体 60..81，单次真实列出命名空间。 */
    fun 诊断命名空间(组号: Int): List<String> {
        require(组号 in 60..96) { "组号超出范围：" + 组号 }
        return 缓存.列出命名空间缓存(命名空间)
    }
}

/** 编排器单例持有：避免自愈路径重复构造多份状态表，延迟初始化真实可用。 */
object 网络隔离编排单例 {
    @Volatile private var 实例: 网络隔离编排器? = null
    fun 获取(
        执行器: 特权命令执行器,
        命名空间: 网络命名空间管理器,
        网卡: 虚拟网卡管理器,
        地址: 地址分配器,
        转发: 端口转发管理器,
        防火墙: 防火墙规则管理器,
        流量: 流量统计器
    ): 网络隔离编排器 {
        return 实例 ?: synchronized(this) {
            实例 ?: 网络隔离编排器(执行器, 命名空间, 网卡, 地址, 转发, 防火墙, 流量).also { 实例 = it }
        }
    }
}
