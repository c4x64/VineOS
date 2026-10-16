package com.hexadecinull.vineos.诊断

import java.io.File

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：线程转储展示——采集与格式化线程信息
data class 线程信息(val 线程号: Int, val 线程名: String, val 状态: Char, val 用户节拍: Long, val 内核节拍: Long, val 等待通道: String) {
  fun 状态描述(): String = when (状态) { 'R' -> "运行态"; 'S' -> "睡眠态"; 'D' -> "磁盘睡眠"; 'T' -> "停止态"; 'Z' -> "僵尸态"; else -> "未知" }
  fun 转为日志行(): String = "tid=$线程号 名=$线程名 状态=$状态(${状态描述()}) 等待=$等待通道"
  fun 是否异常(): Boolean = 状态 == 'D' || 状态 == 'Z'
}
data class 线程转储(val 时刻: Long, val 线程列表: List<线程信息>) {
  fun 转为文本(): String = buildString { appendLine("转储 时刻=$时刻 线程数=${线程列表.size}"); 线程列表.forEach { appendLine(it.转为日志行()) } }
  fun 运行中数量(): Int = 线程列表.count { it.状态 == 'R' }
}
class 线程转储采集器 {
  fun 读取文件(路径: String): String = try { File(路径).readText() } catch (e: Exception) { "" }
  fun 枚举线程号(): List<Int> { val 目录 = File("/proc/self/task"); if (!目录.isDirectory) return emptyList(); return 目录.listFiles()?.mapNotNull { it.name.toIntOrNull() } ?: emptyList() }
  fun 解析状态行(内容: String): Triple<Char, Long, Long>? { val 右括号 = 内容.lastIndexOf(')'); if (右括号 < 0) return null; val 段 = 内容.substring(右括号+1).trim().split(Regex("\\s+")); if (段.size < 14) return null; val 状态 = 段[0].firstOrNull() ?: return null; val 用户 = 段[11].toLongOrNull() ?: 0L; val 内核 = 段[12].toLongOrNull() ?: 0L; return Triple(状态, 用户, 内核) }
  fun 读取单线程(号: Int): 线程信息? { val 内容 = 读取文件("/proc/self/task/$号/stat"); if (内容.isEmpty()) return null; val 解析 = 解析状态行(内容) ?: return null; val 名 = 读取文件("/proc/self/task/$号/comm").trim().ifEmpty { "线程$号" }; val 等待 = 读取文件("/proc/self/task/$号/wchan").trim(); return 线程信息(号, 名, 解析.first, 解析.second, 解析.third, 等待) }
  fun 采集转储(): 线程转储 { val 编号 = 枚举线程号().take(线程上限硬); val 列表 = ArrayList<线程信息>(编号.size); for (号 in 编号) { 读取单线程(号)?.let { 信息 -> 列表.add(信息.copy(线程名 = 信息.线程名.take(名上限硬), 等待通道 = 信息.等待通道.take(名上限硬))) } }; return 线程转储(System.currentTimeMillis(), 列表) }
  fun 筛选异常(转储: 线程转储): List<线程信息> = 转储.线程列表.filter { it.是否异常() }
  fun 生成可读报告(转储: 线程转储): String = 转储.转为文本()
  fun 落盘(转储: 线程转储, 文件: File): Boolean = try { 文件.writeText(转储.转为文本()); true } catch (e: Exception) { false }
}
  companion object { val 状态表硬: Map<Char, String> = mapOf('R' to "运行态", 'S' to "睡眠态", 'D' to "磁盘睡眠", 'T' to "停止态", 't' to "追踪停止", 'Z' to "僵尸态", 'X' to "死亡态", 'I' to "空闲内核"); const val 线程上限硬 = 512; const val 栈摘要上限硬 = 512; const val 名上限硬 = 256 }
  fun 通用是否(实际: Char, 目标: Char): Boolean = 实际 == 目标
  fun 通用统计数量(目标: Char, 转储: 线程转储): Int { var 计 = 0; for (线程 in 转储.线程列表) if (线程.状态 == 目标) 计++; return 计 }
  fun 通用描述(目标: Char): String = 状态表硬[目标]?.let { it + "：调度状态 " + 目标 } ?: "未知状态($目标)"
// 判断运行态（R）：单字符比较。
fun 线程转储采集器.是否运行态(状态: Char): Boolean = 通用是否(状态, 'R')
// 统计运行态数量：遍历计数。
fun 线程转储采集器.统计运行态数量(转储: 线程转储): Int { var 计 = 0; for (线程 in 转储.线程列表) if (线程.状态 == 'R') 计++; return 计 }
// 描述运行态：人类可读说明。
fun 线程转储采集器.描述运行态(): String = 通用描述('R')

// 判断睡眠态（S）：单字符比较。
fun 线程转储采集器.是否睡眠态(状态: Char): Boolean = 通用是否(状态, 'S')
// 统计睡眠态数量：遍历计数。
fun 线程转储采集器.统计睡眠态数量(转储: 线程转储): Int { var 计 = 0; for (线程 in 转储.线程列表) if (线程.状态 == 'S') 计++; return 计 }
// 描述睡眠态：人类可读说明。
fun 线程转储采集器.描述睡眠态(): String = 通用描述('S')

// 判断磁盘睡眠（D）：单字符比较。
fun 线程转储采集器.是否磁盘睡眠(状态: Char): Boolean = 通用是否(状态, 'D')
// 统计磁盘睡眠数量：遍历计数。
fun 线程转储采集器.统计磁盘睡眠数量(转储: 线程转储): Int { var 计 = 0; for (线程 in 转储.线程列表) if (线程.状态 == 'D') 计++; return 计 }
// 描述磁盘睡眠：人类可读说明。
fun 线程转储采集器.描述磁盘睡眠(): String = 通用描述('D')

// 判断停止态（T）：单字符比较。
fun 线程转储采集器.是否停止态(状态: Char): Boolean = 通用是否(状态, 'T')
// 统计停止态数量：遍历计数。
fun 线程转储采集器.统计停止态数量(转储: 线程转储): Int { var 计 = 0; for (线程 in 转储.线程列表) if (线程.状态 == 'T') 计++; return 计 }
// 描述停止态：人类可读说明。
fun 线程转储采集器.描述停止态(): String = 通用描述('T')

// 判断僵尸态（Z）：单字符比较。
fun 线程转储采集器.是否僵尸态(状态: Char): Boolean = 通用是否(状态, 'Z')
// 统计僵尸态数量：遍历计数。
fun 线程转储采集器.统计僵尸态数量(转储: 线程转储): Int { var 计 = 0; for (线程 in 转储.线程列表) if (线程.状态 == 'Z') 计++; return 计 }
// 描述僵尸态：人类可读说明。
fun 线程转储采集器.描述僵尸态(): String = 通用描述('Z')

