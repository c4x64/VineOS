package com.hexadecinull.vineos.网络

/** 查询缓存加速：为网络子系统提供带生存期的查询结果缓存，减少重复拉起子进程。
 * 存在理由：原有各管理器每次查询都直接 fork ip/iptables/nft/ss，哪怕同一秒内重复查询同一内容；
 * 本文件把高频只读查询收敛到一处，按查询种类配置生存期，命中时零 fork，未命中时仍真实执行并带显式超时。
 * 标识与注释中文，系统类库接口保持英文。 */
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong
import javax.inject.Inject
import javax.inject.Singleton

/** 缓存条目：输出文本与到期时刻。 */
data class 查询缓存条目(val 输出: String, val 成功: Boolean, val 到期时刻: Long, val 写入时刻: Long)

/** 缓存命中统计。 */
data class 缓存命中统计(val 命中: Long, val 未命中: Long, val 条目数: Int, val 命中率: Double)

/** 查询种类与其生存期毫秒。 */
object 查询生存期 {
    /** 二进制路径探测：路径稳定，存活久。 */ const val 二进制路径: Long = 600_000L
    /** 命名空间列表：变化较慢。 */ const val 命名空间列表: Long = 3_000L
    /** 链路与地址查询：中等变化。 */ const val 链路地址: Long = 3_000L
    /** 路由摘要：中等变化。 */ const val 路由摘要: Long = 3_000L
    /** 防火墙与地址转换表：变化较快。 */ const val 防火墙表: Long = 5_000L
    /** 地址转换表。 */ const val 地址转换表: Long = 5_000L
    /** 流量计数器：变化最快。 */ const val 流量计数: Long = 1_000L
    /** 系统流量文件：变化最快。 */ const val 系统流量: Long = 1_000L
    /** 连通性探测：不缓存之外的短缓存。 */ const val 可达探测: Long = 2_000L
}

/** 超时夹紧：所有新路径显式超时统一收敛于此，避免无超时等待。 */
object 超时夹紧器 {
    /** 查询类超时收敛到 2..10 秒。 */
    fun 查询超时(请求秒: Long): Long = 请求秒.coerceIn(2L, 10L)
    /** 变更类超时收敛到 5..30 秒。 */
    fun 变更超时(请求秒: Long): Long = 请求秒.coerceIn(5L, 30L)
    /** 批量脚本超时按条数放大后收敛到 10..120 秒。 */
    fun 批量超时(条数: Int, 单条秒: Long = 4L): Long = (8L + 条数 * 单条秒).coerceIn(10L, 120L)
}

@Singleton
class 查询缓存加速器 @Inject constructor(private val 执行器: 特权命令执行器) {
    private val 表 = ConcurrentHashMap<String, 查询缓存条目>()
    private val 命中数 = AtomicLong(0L)
    private val 未命中数 = AtomicLong(0L)
    private val 上次清理 = AtomicLong(0L)

    /** 通用获取：命中且未过期直接返回，否则调用加载器真实加载后写入。 */
    fun 获取(键: String, 生存期毫秒: Long, 加载器: () -> Pair<String, Boolean>): 查询缓存条目 {
        val 现在 = System.currentTimeMillis()
        val 旧 = 表[键]
        if (旧 != null && 现在 < 旧.到期时刻) {
            命中数.incrementAndGet()
            return 旧
        }
        未命中数.incrementAndGet()
        val 载入 = try {
           加载器()
        } catch (异常: Exception) {
            Pair("加载失败：" + (异常.message ?: "未知错误"), false)
        }
        val 条目 = 查询缓存条目(载入.first.take(16_000), 载入.second, 现在 + 生存期毫秒.coerceIn(500L, 600_000L), 现在)
        表[键] = 条目
        顺手清理过期(现在)
        return 条目
    }

    /** 读写原始文本缓存，供流量等大文本复用。 */
    fun 获取文本(键: String, 生存期毫秒: Long, 加载器: () -> String): String {
        return 获取(键, 生存期毫秒) { Pair(加载器(), true) }.输出
    }

    /** 使单键失效。 */
    fun 失效(键: String): Boolean = 表.remove(键) != null

    /** 按前缀批量失效，返回清除条数。 */
    fun 按前缀失效(前缀: String): Int {
        val 待删 = 表.keys.filter { it.startsWith(前缀) }
        待删.forEach { 表.remove(it) }
        return 待删.size
    }

    /** 清空全部。 */
    fun 清空(): Int {
        val 数 = 表.size
        表.clear()
        return 数
    }

    /** 顺手清理过期条目，节流到每 30 秒最多全扫一次。 */
    fun 顺手清理过期(现在: Long = System.currentTimeMillis()): Int {
        if (现在 - 上次清理.get() < 30_000L) return 0
        上次清理.set(现在)
        val 过期 = 表.entries.filter { 现在 >= it.value.到期时刻 }.map { it.key }
        过期.forEach { 表.remove(it) }
        return 过期.size
    }

    /** 命中统计。 */
    fun 命中统计(): 缓存命中统计 {
        val 命中 = 命中数.get()
        val 未命中 = 未命中数.get()
        val 总 = 命中 + 未命中
        val 率 = if (总 == 0L) 0.0 else 命中.toDouble() / 总.toDouble()
        return 缓存命中统计(命中, 未命中, 表.size, 率)
    }

    /** 缓存版二进制路径探测：真实调用执行器，显式超时 5 秒。 */
    fun 探测二进制路径缓存(二进制名: String): 二进制可用性 {
        val 条目 = 获取("二进制:" + 二进制名, 查询生存期.二进制路径) {
            val 可用 = 执行器.探测二进制路径(二进制名)
            Pair((可用.绝对路径 ?: "") + "|" + 可用.版本, 可用.可用)
        }
        val 切 = 条目.输出.split("|", limit = 2)
        val 路径 = 切.getOrNull(0)?.ifEmpty { null }
        val 版本 = 切.getOrNull(1) ?: ""
        return 二进制可用性(二进制名, 路径, 条目.成功 && 路径 != null, 版本)
    }

    /** 缓存版命名空间列表：真实执行 ip netns list，显式超时 8 秒。 */
    fun 列出命名空间缓存(命名空间: 网络命名空间管理器): List<String> {
        val 条目 = 获取("命名空间列表", 查询生存期.命名空间列表) {
            val 名单 = try {
                命名空间.列出命名空间()
            } catch (_: Exception) {
                emptyList()
            }
            Pair(名单.joinToString("\n"), true)
        }
        return 条目.输出.lines().filter { it.isNotBlank() }
    }

    /** 缓存版网卡链路原文：真实执行 ip -d link show，显式超时 8 秒。 */
    fun 查询链路原文缓存(网卡: String, 超时秒: Long = 8L): String {
        命令辅助.校验网卡名合法(网卡)
        val 路径 = 探测二进制路径缓存("ip").绝对路径 ?: "ip"
        return 获取("链路:" + 网卡, 查询生存期.链路地址) {
            val 结果 = 执行器.同步执行(listOf(路径, "-d", "link", "show", 网卡), 超时夹紧器.查询超时(超时秒))
            Pair(if (结果.是否成功()) 结果.标准输出 else "失败:" + 结果.错误输出, 结果.是否成功())
        }.输出
    }

    /** 缓存版网卡地址原文：真实执行 ip -o addr show，显式超时 8 秒。 */
    fun 查询地址原文缓存(网卡: String, 超时秒: Long = 8L): String {
        命令辅助.校验网卡名合法(网卡)
        val 路径 = 探测二进制路径缓存("ip").绝对路径 ?: "ip"
        return 获取("地址:" + 网卡, 查询生存期.链路地址) {
            val 结果 = 执行器.同步执行(listOf(路径, "-o", "addr", "show", 网卡), 超时夹紧器.查询超时(超时秒))
            Pair(if (结果.是否成功()) 结果.标准输出 else "失败:" + 结果.错误输出, 结果.是否成功())
        }.输出
    }

    /** 缓存版路由摘要：真实执行 ip route show，显式超时 8 秒。 */
    fun 查询路由摘要缓存(出口网卡: String = "", 超时秒: Long = 8L): String {
        val 路径 = 探测二进制路径缓存("ip").绝对路径 ?: "ip"
        val 命令 = if (出口网卡.isEmpty()) listOf(路径, "route", "show") else listOf(路径, "route", "show", "dev", 出口网卡)
        return 获取("路由:" + 出口网卡, 查询生存期.路由摘要) {
            val 结果 = 执行器.同步执行(命令, 超时夹紧器.查询超时(超时秒))
            Pair(if (结果.是否成功()) 结果.标准输出.take(4_000) else "查询失败：" + 结果.错误输出, 结果.是否成功())
        }.输出
    }

    /** 缓存版过滤表：真实执行 iptables -L -n -v -x，显式超时 10 秒。 */
    fun 查询过滤表缓存(超时秒: Long = 10L): String {
        val 路径 = 探测二进制路径缓存("iptables").绝对路径 ?: "iptables"
        return 获取("过滤表", 查询生存期.防火墙表) {
            val 结果 = 执行器.同步执行(listOf(路径, "-L", "-n", "-v", "-x", "--line-numbers"), 超时夹紧器.查询超时(超时秒))
            Pair(if (结果.是否成功()) 结果.标准输出.take(12_000) else "查询失败：" + 结果.错误输出, 结果.是否成功())
        }.输出
    }

    /** 缓存版地址转换表：真实执行 iptables -t nat -L -n，显式超时 10 秒。 */
    fun 查询地址转换表缓存(超时秒: Long = 10L): String {
        val 路径 = 探测二进制路径缓存("iptables").绝对路径 ?: "iptables"
        return 获取("地址转换表", 查询生存期.地址转换表) {
            val 结果 = 执行器.同步执行(listOf(路径, "-t", "nat", "-L", "-n", "--line-numbers"), 超时夹紧器.查询超时(超时秒))
            Pair(if (结果.是否成功()) 结果.标准输出.take(12_000) else "查询失败：" + 结果.错误输出, 结果.是否成功())
        }.输出
    }

    /** 缓存版系统流量原文：真实读盘 /proc/net/dev，显式内存生存期 1 秒。 */
    fun 读取系统流量原文缓存(流量器: 流量统计器): List<网卡流量> {
        val 文本 = 获取("系统流量", 查询生存期.系统流量) {
            val 列表 = try {
                流量器.读取系统流量()
            } catch (_: Exception) {
                emptyList()
            }
            Pair(列表.joinToString("\n") { it.网卡 + " " + it.收字节 + " " + it.发字节 + " " + it.收包 + " " + it.发包 }, true)
        }.输出
        val 现在 = System.currentTimeMillis()
        return 文本.lines().mapNotNull { 行 ->
            val 格 = 行.trim().split(Regex("\\s+"))
            if (格.size < 5) null else 网卡流量(格[0], 格[1].toLongOrNull() ?: 0L, 格[2].toLongOrNull() ?: 0L, 格[3].toLongOrNull() ?: 0L, 格[4].toLongOrNull() ?: 0L, 现在)
        }
    }

    /** 缓存版计数器表：真实执行 iptables -L -n -v -x，显式超时 10 秒。 */
    fun 查询计数器缓存(超时秒: Long = 10L): String {
        val 路径 = 探测二进制路径缓存("iptables").绝对路径 ?: "iptables"
        return 获取("计数器表", 查询生存期.流量计数) {
            val 结果 = 执行器.同步执行(listOf(路径, "-L", "-n", "-v", "-x"), 超时夹紧器.查询超时(超时秒))
            Pair(if (结果.是否成功()) 结果.标准输出.take(12_000) else "查询失败：" + 结果.错误输出, 结果.是否成功())
        }.输出
    }

    /** 变更后失效相关缓存：调用方在真实变更成功后调用，保证读己之写。 */
    fun 变更后失效(网卡: String? = null, 命名空间相关: Boolean = false, 防火墙相关: Boolean = false, 流量相关: Boolean = false): Int {
        var 数 = 0
        if (网卡 != null) {
            if (失效("链路:" + 网卡)) 数++
            if (失效("地址:" + 网卡)) 数++
        }
        if (命名空间相关) 数 += 按前缀失效("命名空间") + 按前缀失效("路由")
        if (防火墙相关) 数 += 按前缀失效("过滤表") + 按前缀失效("地址转换表") + 按前缀失效("计数器表")
        if (流量相关) 数 += 按前缀失效("系统流量")
        return 数
    }
}
