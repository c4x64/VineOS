package com.hexadecinull.vineos.诊断

import java.io.File

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：错误聚合展示——指纹分组、去重计数、查询展示
data class 错误记录(val 序号: Long, val 时刻: Long, val 类别: String, val 摘要: String, val 堆栈: String, val 指纹: String)
data class 错误分组(val 指纹: String, val 类别: String, val 代表摘要: String, val 次数: Long, val 首次: Long, val 末次: Long)
class 错误聚合仓 {
  private val 锁 = Any()
  private val 记录 = mutableListOf<错误记录>()
  private var 序号: Long = 0
  fun 计算指纹(类别: String, 摘要: String, 首行: String): String { var 哈希 = -3750763034362895579L; for (字符 in (类别+首行+摘要.take(64))) { 哈希 = 哈希 xor 字符.code.toLong(); 哈希 *= 1099511628211L }; return 类别 + "_" + 哈希.toString(16) }
  fun 提取首行(堆栈: String): String = 堆栈.lineSequence().firstOrNull()?.trim()?.take(256) ?: ""
  companion object { val 类别表硬: List<String> = listOf("原生崩溃","应用无响应","爪哇异常","原生信号","内存不足","磁盘不足","网络失败","权限拒绝","服务死亡","渲染超时","快照失败","启动失败"); const val 记录上限硬 = 5000; const val 查询封顶硬 = 5000 }
  fun 通用上报(类别: String, 摘要: String, 堆栈: String): Long = 上报错误(类别, 摘要, 堆栈)
  fun 通用查询分组(类别: String): List<错误分组> = synchronized(锁) { val 表 = 记录.groupBy { it.指纹 }; val 结果 = ArrayList<错误分组>(表.size); for ((指纹, 组) in 表) { if (组.first().类别 != 类别) continue; val 首 = 组.first(); 结果.add(错误分组(指纹, 首.类别, 首.摘要, 组.size.toLong(), 组.minOf { it.时刻 }, 组.maxOf { it.时刻 })) }; 结果 }
  fun 通用统计次数(类别: String): Long = 通用查询分组(类别).sumOf { it.次数 }
  fun 上报错误(类别: String, 摘要: String, 堆栈: String): Long = synchronized(锁) { 序号 += 1; val 切类 = (if (类别.length <= 64)类别 else 类别.substring(0,64)).ifEmpty { "未知" }; val 切摘 = if (摘要.length <= 1024)摘要 else 摘要.substring(0,1024); val 切栈 = if (堆栈.length <= 8192)堆栈 else 堆栈.substring(0,8192); val 记录项 = 错误记录(序号, System.currentTimeMillis(), 切类, 切摘, 切栈, 计算指纹(切类, 切摘, 提取首行(切栈))); 记录.add(记录项); while (记录.size > 记录上限硬) 记录.removeFirst(); 记录项.序号 }
  fun 查询全部分组(): List<错误分组> = synchronized(锁) { val 表 = 记录.groupBy { it.指纹 }; val 结果 = ArrayList<错误分组>(表.size); for ((指纹, 组) in 表) { val 首 = 组.first(); 结果.add(错误分组(指纹, 首.类别, 首.摘要, 组.size.toLong(), 组.minOf { it.时刻 }, 组.maxOf { it.时刻 })) }; 结果 }
  fun 查询热门(前几名: Int): List<错误分组> { val 封顶 = 前几名.coerceIn(0, 1000); if (封顶 == 0) return emptyList(); return 查询全部分组().sortedByDescending { it.次数 }.take(封顶) }
  fun 查询窗口(起始: Long, 结束: Long, 上限: Int = 5000): List<错误记录> = synchronized(锁) { val 封顶 = 上限.coerceIn(0, 查询封顶硬); if (封顶 == 0) return@synchronized emptyList(); val 结果 = ArrayList<错误记录>(minOf(封顶, 记录.size)); for (条 in 记录) if (条.时刻 in 起始..结束) { 结果.add(条); if (结果.size >= 封顶) break }; 结果 }
  fun 落盘(文件: File): Boolean = try { synchronized(锁) { 文件.writeText(记录.joinToString("\n") { "${it.序号} ${it.类别} ${it.摘要}" }) }; true } catch (e: Exception) { false }
  fun 清空() = synchronized(锁) { 记录.clear() }
  fun 记录总数(): Int = synchronized(锁) { 记录.size }
}
// 上报原生崩溃：固定类别封装。
fun 错误聚合仓.上报原生崩溃(摘要: String, 堆栈: String): Long = 通用上报("原生崩溃", 摘要, 堆栈)
// 统计原生崩溃次数：按类别过滤计数。
fun 错误聚合仓.统计原生崩溃次数(): Long = 通用统计次数("原生崩溃")
// 查询原生崩溃分组：按类别过滤。
fun 错误聚合仓.查询原生崩溃分组(): List<错误分组> = 通用查询分组("原生崩溃")

// 上报应用无响应：固定类别封装。
fun 错误聚合仓.上报应用无响应(摘要: String, 堆栈: String): Long = 通用上报("应用无响应", 摘要, 堆栈)
// 统计应用无响应次数：按类别过滤计数。
fun 错误聚合仓.统计应用无响应次数(): Long = 通用统计次数("应用无响应")
// 查询应用无响应分组：按类别过滤。
fun 错误聚合仓.查询应用无响应分组(): List<错误分组> = 通用查询分组("应用无响应")

// 上报爪哇异常：固定类别封装。
fun 错误聚合仓.上报爪哇异常(摘要: String, 堆栈: String): Long = 通用上报("爪哇异常", 摘要, 堆栈)
// 统计爪哇异常次数：按类别过滤计数。
fun 错误聚合仓.统计爪哇异常次数(): Long = 通用统计次数("爪哇异常")
// 查询爪哇异常分组：按类别过滤。
fun 错误聚合仓.查询爪哇异常分组(): List<错误分组> = 通用查询分组("爪哇异常")

// 上报原生信号：固定类别封装。
fun 错误聚合仓.上报原生信号(摘要: String, 堆栈: String): Long = 通用上报("原生信号", 摘要, 堆栈)
// 统计原生信号次数：按类别过滤计数。
fun 错误聚合仓.统计原生信号次数(): Long = 通用统计次数("原生信号")
// 查询原生信号分组：按类别过滤。
fun 错误聚合仓.查询原生信号分组(): List<错误分组> = 通用查询分组("原生信号")

// 上报内存不足：固定类别封装。
fun 错误聚合仓.上报内存不足(摘要: String, 堆栈: String): Long = 通用上报("内存不足", 摘要, 堆栈)
// 统计内存不足次数：按类别过滤计数。
fun 错误聚合仓.统计内存不足次数(): Long = 通用统计次数("内存不足")
// 查询内存不足分组：按类别过滤。
fun 错误聚合仓.查询内存不足分组(): List<错误分组> = 通用查询分组("内存不足")

// 上报磁盘不足：固定类别封装。
fun 错误聚合仓.上报磁盘不足(摘要: String, 堆栈: String): Long = 通用上报("磁盘不足", 摘要, 堆栈)
// 统计磁盘不足次数：按类别过滤计数。
fun 错误聚合仓.统计磁盘不足次数(): Long = 通用统计次数("磁盘不足")
// 查询磁盘不足分组：按类别过滤。
fun 错误聚合仓.查询磁盘不足分组(): List<错误分组> = 通用查询分组("磁盘不足")

// 上报网络失败：固定类别封装。
fun 错误聚合仓.上报网络失败(摘要: String, 堆栈: String): Long = 通用上报("网络失败", 摘要, 堆栈)
// 统计网络失败次数：按类别过滤计数。
fun 错误聚合仓.统计网络失败次数(): Long = 通用统计次数("网络失败")
// 查询网络失败分组：按类别过滤。
fun 错误聚合仓.查询网络失败分组(): List<错误分组> = 通用查询分组("网络失败")

// 上报权限拒绝：固定类别封装。
fun 错误聚合仓.上报权限拒绝(摘要: String, 堆栈: String): Long = 通用上报("权限拒绝", 摘要, 堆栈)
// 统计权限拒绝次数：按类别过滤计数。
fun 错误聚合仓.统计权限拒绝次数(): Long = 通用统计次数("权限拒绝")
// 查询权限拒绝分组：按类别过滤。
fun 错误聚合仓.查询权限拒绝分组(): List<错误分组> = 通用查询分组("权限拒绝")

// 上报服务死亡：固定类别封装。
fun 错误聚合仓.上报服务死亡(摘要: String, 堆栈: String): Long = 通用上报("服务死亡", 摘要, 堆栈)
// 统计服务死亡次数：按类别过滤计数。
fun 错误聚合仓.统计服务死亡次数(): Long = 通用统计次数("服务死亡")
// 查询服务死亡分组：按类别过滤。
fun 错误聚合仓.查询服务死亡分组(): List<错误分组> = 通用查询分组("服务死亡")

// 上报渲染超时：固定类别封装。
fun 错误聚合仓.上报渲染超时(摘要: String, 堆栈: String): Long = 通用上报("渲染超时", 摘要, 堆栈)
// 统计渲染超时次数：按类别过滤计数。
fun 错误聚合仓.统计渲染超时次数(): Long = 通用统计次数("渲染超时")
// 查询渲染超时分组：按类别过滤。
fun 错误聚合仓.查询渲染超时分组(): List<错误分组> = 通用查询分组("渲染超时")

// 上报快照失败：固定类别封装。
fun 错误聚合仓.上报快照失败(摘要: String, 堆栈: String): Long = 通用上报("快照失败", 摘要, 堆栈)
// 统计快照失败次数：按类别过滤计数。
fun 错误聚合仓.统计快照失败次数(): Long = 通用统计次数("快照失败")
// 查询快照失败分组：按类别过滤。
fun 错误聚合仓.查询快照失败分组(): List<错误分组> = 通用查询分组("快照失败")

// 上报启动失败：固定类别封装。
fun 错误聚合仓.上报启动失败(摘要: String, 堆栈: String): Long = 通用上报("启动失败", 摘要, 堆栈)
// 统计启动失败次数：按类别过滤计数。
fun 错误聚合仓.统计启动失败次数(): Long = 通用统计次数("启动失败")
// 查询启动失败分组：按类别过滤。
fun 错误聚合仓.查询启动失败分组(): List<错误分组> = 通用查询分组("启动失败")

