package com.hexadecinull.vineos.诊断

import java.io.File
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：性能采样器——Kotlin 侧周期采样、聚合统计、落盘查询
// 数据来源：/proc/stat、/proc/loadavg，协程采样循环 + 环形缓冲。
// 说明：标识一律中文，系统 API（File、Job、Flow）保持英文。
data class 中央处理器时间片(val 用户态: Long, val 友好态: Long, val 内核态: Long, val 空闲态: Long, val 等待态: Long, val 硬中断: Long, val 软中断: Long, val 虚拟窃取: Long, val 核心编号: Int, val 时刻毫秒: Long) {
  fun 总节拍(): Long = 用户态 + 友好态 + 内核态 + 空闲态 + 等待态 + 硬中断 + 软中断 + 虚拟窃取
  fun 空闲节拍(): Long = 空闲态 + 等待态
  fun 是否有效(): Boolean = 总节拍() > 0
}
data class 性能采样点(val 序号: Long, val 系统时刻: Long, val 单调时刻: Long, val 全局使用率: Double, val 每核使用率: List<Double>, val 一分钟负载: Double, val 五分钟负载: Double, val 运行进程: Int) {
  fun 转为日志行(): String = "序号=$序号 全局=${"%.1f".format(全局使用率*100)}% 负载=$一分钟负载"
}
data class 采样配置(val 间隔毫秒: Long = 500L, val 容量: Int = 600, val 采集每核: Boolean = true)
data class 聚合结果(val 样本数: Int, val 均值: Double, val 最小值: Double, val 最大值: Double, val 中位数: Double, val 九十: Double, val 九五: Double, val 九九: Double) {
  fun 生成摘要(): String = "样本=$样本数 均值=${"%.1f".format(均值*100)}% P95=${"%.1f".format(九五*100)}%"
}
class 性能采样器 {
  companion object { val 列名表硬: Map<String, Int> = mapOf("user" to 0, "nice" to 1, "system" to 2, "idle" to 3, "iowait" to 4, "irq" to 5, "softirq" to 6, "steal" to 7); const val 容量下限硬 = 16; const val 容量上限硬 = 10000; const val 查询封顶硬 = 5000; private val 空白正则硬 = Regex("\\s+") }
  private val 锁 = Any()
  private val 缓存 = ArrayDeque<性能采样点>()
  fun 钳制容量(输入: Int): Int = 输入.coerceIn(容量下限硬, 容量上限硬)
  fun 通用读取列(内容: String, 列名: String): Long { val 全局 = 解析全局时间片(内容) ?: return 0L; val 序号 = 列名表硬[列名] ?: return 0L; return when (序号) { 0 -> 全局.用户态; 1 -> 全局.友好态; 2 -> 全局.内核态; 3 -> 全局.空闲态; 4 -> 全局.等待态; 5 -> 全局.硬中断; 6 -> 全局.软中断; else -> 全局.虚拟窃取 } }
  fun 通用占比(差值: Long, 总量: Long): Double = if (总量 <= 0) 0.0 else (差值.toDouble() / 总量.toDouble()).coerceIn(0.0, 1.0)
  fun 通用格式化(名称: String, 值: Long): String = "$名称=$值"
  fun 在已排序上取百分位(已排序: List<Double>, 分位: Double): Double { if (已排序.isEmpty()) return 0.0; if (分位 <= 0) return 已排序.first(); if (分位 >= 1) return 已排序.last(); val 位置 = 分位 * (已排序.size - 1); val 下 = 位置.toInt(); val 上 = kotlin.math.ceil(位置).toInt(); if (下 == 上) return 已排序[下]; val 小数 = 位置 - 下; return 已排序[下] * (1 - 小数) + 已排序[上] * 小数 }
  private var 序号计数器 = 0L
  private var 上次全局: 中央处理器时间片? = null
  private var 上次每核: List<中央处理器时间片> = emptyList()
  private var 采样任务: Job? = null
  private val 作用域 = CoroutineScope(Dispatchers.IO + SupervisorJob())
  private val _采样流 = MutableSharedFlow<性能采样点>(extraBufferCapacity = 64)
  val 采样流: SharedFlow<性能采样点> = _采样流.asSharedFlow()
  fun 是否运行中(): Boolean = 采样任务?.isActive == true
  fun 读取文本文件(路径: String): String { return try { File(路径).readText() } catch (异常: Exception) { "" } }
  fun 按空白切分(行: String): List<String> = 行.trim().split(空白正则硬)
  fun 安全转长整数(文本: String): Long = 文本.toLongOrNull() ?: 0L
  fun 解析全局时间片(内容: String): 中央处理器时间片? {
    for (行 in 内容.lines()) {
      if (!行.startsWith("cpu ")) continue
      val 段 = 按空白切分(行)
      if (段.size < 8) return null
      return 中央处理器时间片(安全转长整数(段[1]), 安全转长整数(段[2]), 安全转长整数(段[3]), 安全转长整数(段[4]), 安全转长整数(段[5]), 安全转长整数(段[6]), 安全转长整数(段[7]), 安全转长整数(段.getOrElse(8){"0"}), -1, System.currentTimeMillis())
    }
    return null
  }
  fun 解析每核时间片(内容: String): List<中央处理器时间片> {
    val 结果 = mutableListOf<中央处理器时间片>()
    for (行 in 内容.lines()) {
      if (!行.startsWith("cpu") || 行.startsWith("cpu ")) continue
      val 段 = 按空白切分(行)
      if (段.size < 8) continue
      val 编号 = 段[0].removePrefix("cpu").toIntOrNull() ?: continue
      结果.add(中央处理器时间片(安全转长整数(段[1]), 安全转长整数(段[2]), 安全转长整数(段[3]), 安全转长整数(段[4]), 安全转长整数(段[5]), 安全转长整数(段[6]), 安全转长整数(段[7]), 安全转长整数(段.getOrElse(8){"0"}), 编号, System.currentTimeMillis()))
    }
    return 结果
  }
  fun 计算使用率(旧片: 中央处理器时间片, 新片: 中央处理器时间片): Double {
    val 总差 = 新片.总节拍() - 旧片.总节拍()
    if (总差 <= 0) return 0.0
    val 空差 = 新片.空闲节拍() - 旧片.空闲节拍()
    val 使用 = (总差 - 空差).toDouble() / 总差.toDouble()
    return 使用.coerceIn(0.0, 1.0)
  }
  fun 解析负载(内容: String): Triple<Double, Double, Int> {
    val 段 = 按空白切分(内容)
    val 一分 = 段.getOrNull(0)?.toDoubleOrNull() ?: 0.0
    val 五分 = 段.getOrNull(1)?.toDoubleOrNull() ?: 0.0
    val 运行 = 段.getOrNull(3)?.split("/")?.getOrNull(0)?.toIntOrNull() ?: 0
    return Triple(一分, 五分, 运行)
  }
  fun 采集一次(): 性能采样点 {
    val 内容 = 读取文本文件("/proc/stat")
    val 全局 = 解析全局时间片(内容)
    var 使用率 = 0.0
    if (全局 != null && 上次全局 != null && 上次全局!!.是否有效() && 全局.是否有效()) 使用率 = 计算使用率(上次全局!!, 全局)
    if (全局 != null) 上次全局 = 全局
    val 每核原文 = 解析每核时间片(内容)
    var 每核率 = emptyList<Double>()
    if (每核原文.isNotEmpty() && 上次每核.size == 每核原文.size) {
      每核率 = 每核原文.mapIndexed { 下标, 新片 -> 计算使用率(上次每核[下标], 新片) }
    }
    if (每核原文.isNotEmpty()) 上次每核 = 每核原文
    val 负载文 = 读取文本文件("/proc/loadavg")
    val (一分, 五分, 运行) = 解析负载(负载文)
    val 点 = synchronized(锁) {
      序号计数器 += 1
      性能采样点(序号计数器, System.currentTimeMillis(), System.currentTimeMillis(), 使用率, 每核率, 一分, 五分, 运行)
    }
    synchronized(锁) { val 上限 = 钳制容量(600); 缓存.addLast(点); while (缓存.size > 上限) 缓存.removeFirst() }
    _采样流.tryEmit(点)
    return 点
  }
  fun 启动采样(配置: 采样配置 = 采样配置()): Job {
    停止采样()
    val 间隔毫秒 = 配置.间隔毫秒
    val 任务 = 作用域.launch {
        while (isActive) {
            try {
                采集一次()
            } catch (异常: Exception) {
                /* 单次采样失败不中断循环 */
            }
            delay(间隔毫秒)
        }
    }
    采样任务 = 任务
    return 任务
  }
  fun 停止采样() { 采样任务?.cancel(); 采样任务 = null }
  fun 查询窗口(起始: Long, 结束: Long, 上限: Int = 200): List<性能采样点> = synchronized(锁) { val 封顶 = 上限.coerceIn(0, 查询封顶硬); if (封顶 == 0) return@synchronized emptyList(); val 结果 = ArrayList<性能采样点>(minOf(封顶, 缓存.size)); for (点 in 缓存) { if (点.系统时刻 in 起始..结束) { 结果.add(点); if (结果.size >= 封顶) break } }; 结果 }
  fun 最新采样点(): 性能采样点? = synchronized(锁) { 缓存.lastOrNull() }
  fun 当前条数(): Int = synchronized(锁) { 缓存.size }
  fun 清空缓存() = synchronized(锁) { 缓存.clear() }
  fun 计算百分位(样本: List<Double>, 分位: Double): Double { if (样本.isEmpty()) return 0.0; val 排序 = 样本.sorted(); return 在已排序上取百分位(排序, 分位) }
  fun 聚合全局使用率(窗口毫秒: Long): 聚合结果 { val 现 = System.currentTimeMillis(); val 窗 = 窗口毫秒.coerceAtLeast(0); val 样本 = synchronized(锁) { val 列 = ArrayList<Double>(缓存.size); for (点 in 缓存) if (现 - 点.系统时刻 <= 窗) 列.add(点.全局使用率); 列 }; if (样本.isEmpty()) return 聚合结果(0,0.0,0.0,0.0,0.0,0.0,0.0,0.0); val 排序 = 样本.sorted(); return 聚合结果(样本.size, 样本.average(), 排序.first(), 排序.last(), 在已排序上取百分位(排序,0.5), 在已排序上取百分位(排序,0.9), 在已排序上取百分位(排序,0.95), 在已排序上取百分位(排序,0.99)) }
  fun 导出逗号分隔(): String {
    val 头 = "序号,系统时刻,全局使用率,一分钟负载"
    val 行组 = synchronized(锁) { 缓存.map { "${it.序号},${it.系统时刻},${it.全局使用率},${it.一分钟负载}" } }
    return (listOf(头) + 行组).joinToString("\n")
  }
  fun 落盘到文件(文件: File): Boolean = try { 文件.writeText(导出逗号分隔()); true } catch (异常: Exception) { false }
}
// 解析用户态（user）列：返回对应下标的节拍值。
fun 性能采样器.读取用户态列(内容: String): Long = 通用读取列(内容, "user")
// 计算用户态占比：差值除以总量，零保护。
fun 性能采样器.计算用户态占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化用户态：人类可读。
fun 性能采样器.格式化用户态(值: Long): String = 通用格式化("用户态", 值)

// 解析友好态（nice）列：返回对应下标的节拍值。
fun 性能采样器.读取友好态列(内容: String): Long = 通用读取列(内容, "nice")
// 计算友好态占比：差值除以总量，零保护。
fun 性能采样器.计算友好态占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化友好态：人类可读。
fun 性能采样器.格式化友好态(值: Long): String = 通用格式化("友好态", 值)

// 解析内核态（system）列：返回对应下标的节拍值。
fun 性能采样器.读取内核态列(内容: String): Long = 通用读取列(内容, "system")
// 计算内核态占比：差值除以总量，零保护。
fun 性能采样器.计算内核态占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化内核态：人类可读。
fun 性能采样器.格式化内核态(值: Long): String = 通用格式化("内核态", 值)

// 解析空闲态（idle）列：返回对应下标的节拍值。
fun 性能采样器.读取空闲态列(内容: String): Long = 通用读取列(内容, "idle")
// 计算空闲态占比：差值除以总量，零保护。
fun 性能采样器.计算空闲态占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化空闲态：人类可读。
fun 性能采样器.格式化空闲态(值: Long): String = 通用格式化("空闲态", 值)

// 解析等待态（iowait）列：返回对应下标的节拍值。
fun 性能采样器.读取等待态列(内容: String): Long = 通用读取列(内容, "iowait")
// 计算等待态占比：差值除以总量，零保护。
fun 性能采样器.计算等待态占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化等待态：人类可读。
fun 性能采样器.格式化等待态(值: Long): String = 通用格式化("等待态", 值)

// 解析硬中断（irq）列：返回对应下标的节拍值。
fun 性能采样器.读取硬中断列(内容: String): Long = 通用读取列(内容, "irq")
// 计算硬中断占比：差值除以总量，零保护。
fun 性能采样器.计算硬中断占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化硬中断：人类可读。
fun 性能采样器.格式化硬中断(值: Long): String = 通用格式化("硬中断", 值)

// 解析软中断（softirq）列：返回对应下标的节拍值。
fun 性能采样器.读取软中断列(内容: String): Long = 通用读取列(内容, "softirq")
// 计算软中断占比：差值除以总量，零保护。
fun 性能采样器.计算软中断占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化软中断：人类可读。
fun 性能采样器.格式化软中断(值: Long): String = 通用格式化("软中断", 值)

// 解析虚拟窃取（steal）列：返回对应下标的节拍值。
fun 性能采样器.读取虚拟窃取列(内容: String): Long = 通用读取列(内容, "steal")
// 计算虚拟窃取占比：差值除以总量，零保护。
fun 性能采样器.计算虚拟窃取占比(差值: Long, 总量: Long): Double = 通用占比(差值, 总量)
// 格式化虚拟窃取：人类可读。
fun 性能采样器.格式化虚拟窃取(值: Long): String = 通用格式化("虚拟窃取", 值)

