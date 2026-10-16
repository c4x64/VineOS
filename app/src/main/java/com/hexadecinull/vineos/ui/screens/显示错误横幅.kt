package com.hexadecinull.vineos.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.hexadecinull.vineos.ui.viewmodel.剪贴板同步调度
import com.hexadecinull.vineos.ui.viewmodel.错误提示中心
import com.hexadecinull.vineos.ui.viewmodel.错误级别
import com.hexadecinull.vineos.ui.viewmodel.错误记录

/**
 * 显示错误横幅。
 *
 * 职责：
 * - 展示错误中心的当前错误与队列。
 * - 展示剪贴板同步状态条。
 * - 提供重试与消除的人因入口。
 *
 * 效率说明：
 * - 分组标题与历史行合并为单函数加序号，不再保留编号函数族。
 * - 队列过滤与统计走 derivedStateOf 记忆加键控，避免每帧全量分组。
 */

/**
 * 错误提示横幅。
 *
 * @param 中心 错误中心。
 * @param 重试 重试回调。
 * @param 修饰符 修饰符。
 */
@Composable
fun 错误提示横幅(中心: 错误提示中心, 重试: () -> Unit, 修饰符: Modifier = Modifier) {
    val 当前 by 中心.当前流.collectAsState()
    val 记录 = 当前 ?: return
    val 背景 by remember(记录.级别) {
        derivedStateOf {
            when (记录.级别) {
                错误级别.提示 -> Color(0xFF37474F)
                错误级别.警告 -> Color(0xFFF57F17)
                错误级别.错误 -> Color(0xFFC62828)
                错误级别.致命 -> Color(0xFF6A1B9A)
            }
        }
    }
    val 播报 by remember(记录) { derivedStateOf { 中心.生成播报文案(记录) } }
    Box(
        modifier = 修饰符
            .fillMaxWidth()
            .background(背景, RoundedCornerShape(12.dp))
            .padding(12.dp),
    ) {
        Column {
            Text(text = 记录.标题, style = MaterialTheme.typography.titleSmall, color = Color.White)
            Text(text = 记录.详情, style = MaterialTheme.typography.bodySmall, color = Color.White)
            Text(
                text = 播报,
                style = MaterialTheme.typography.labelSmall,
                color = Color.White.copy(alpha = 0.8f),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (记录.可重试) {
                    Button(onClick = 重试) { Text(text = "重试") }
                }
                TextButton(onClick = { 中心.消除当前() }) { Text(text = "消除", color = Color.White) }
            }
        }
    }
}

/**
 * 剪贴板同步条。
 *
 * 成功率与历史数量随快照记忆，不再每帧拷贝历史。
 *
 * @param 调度 剪贴板调度。
 * @param 同步 同步回调。
 * @param 修饰符 修饰符。
 */
@Composable
fun 剪贴板同步条(调度: 剪贴板同步调度, 同步: (String) -> Unit, 修饰符: Modifier = Modifier) {
    val 快照 by 调度.状态流.collectAsState()
    val 文案 by remember(快照) {
        derivedStateOf { "剪贴板 状态=" + 快照.状态.name + " 成功率=" + 调度.统计成功率() + " 历史=" + 调度.查询历史数量() }
    }
    Row(
        modifier = 修饰符
            .fillMaxWidth()
            .background(Color.Black.copy(alpha = 0.55f), RoundedCornerShape(10.dp))
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = 文案,
            style = MaterialTheme.typography.labelMedium,
            color = Color.White,
            modifier = Modifier.weight(1f),
        )
        TextButton(onClick = { 同步(快照.宿主文本) }) { Text(text = "同步", color = Color.White) }
    }
}

/**
 * 错误队列列表。
 *
 * 按错误码加时刻键控，避免无键重组错位。
 *
 * @param 中心 错误中心。
 * @param 修饰符 修饰符。
 */
@Composable
fun 错误队列列表(中心: 错误提示中心, 修饰符: Modifier = Modifier) {
    val 队列 by 中心.队列流.collectAsState()
    Column(modifier = 修饰符.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        for (记录 in 队列) {
            key(记录.错误码, 记录.时刻) {
                错误队列行(记录 = 记录, 中心 = 中心)
            }
        }
    }
}

/**
 * 错误队列行。
 *
 * @param 记录 记录。
 * @param 中心 中心。
 * @param 修饰符 修饰符。
 */
@Composable
fun 错误队列行(记录: 错误记录, 中心: 错误提示中心, 修饰符: Modifier = Modifier) {
    val 文案 by remember(记录) { derivedStateOf { 中心.生成播报文案(记录) } }
    Row(
        modifier = 修饰符
            .fillMaxWidth()
            .background(Color.White.copy(alpha = 0.08f), RoundedCornerShape(8.dp))
            .padding(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = 文案,
            style = MaterialTheme.typography.labelSmall,
            color = Color.White,
            modifier = Modifier.weight(1f),
        )
    }
}

/**
 * 错误分组标题。
 *
 * 按错误码区间与级别展示分组标题，便于长队列快速定位。
 * 旧编号函数族已合并为本函数加序号参数。
 *
 * @param 中心 错误中心。
 * @param 级别 目标级别。
 * @param 序号 分组序号。
 * @param 修饰符 修饰符。
 */
@Composable
fun 错误分组标题(中心: 错误提示中心, 级别: 错误级别, 序号: Int = 0, 修饰符: Modifier = Modifier) {
    val 队列 by 中心.队列流.collectAsState()
    val 安全序号 = 序号.coerceIn(0, 1024)
    val 分组数量 by remember(队列, 级别) { derivedStateOf { 队列.count { it.级别 == 级别 } } }
    val 标题 by remember(队列, 级别, 安全序号) {
        derivedStateOf { "分组" + 安全序号.toString().padStart(3, '0') + " 级别=" + 级别.name + " 数量=" + 分组数量 }
    }
    Box(
        modifier = 修饰符
            .fillMaxWidth()
            .background(Color.Black.copy(alpha = 0.45f), RoundedCornerShape(8.dp))
            .padding(horizontal = 10.dp, vertical = 6.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        Text(text = 标题, style = MaterialTheme.typography.labelMedium, color = Color.White)
    }
}

/**
 * 错误分组标题列。
 *
 * 一次收集多次分组，供长队列快速定位。
 *
 * @param 中心 错误中心。
 * @param 级别表 级别表。
 * @param 修饰符 修饰符。
 */
@Composable
fun 错误分组标题列(中心: 错误提示中心, 级别表: List<错误级别>, 修饰符: Modifier = Modifier) {
    Column(modifier = 修饰符.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        for ((索引, 级别) in 级别表.withIndex()) {
            key(级别, 索引) {
                错误分组标题(中心 = 中心, 级别 = 级别, 序号 = 索引)
            }
        }
    }
}

/**
 * 剪贴板历史行。
 *
 * 展示指定序号的历史摘要与方向，供排查同步风暴。
 * 旧编号函数族已合并为本函数加序号参数。
 *
 * @param 调度 调度。
 * @param 序号 序号。
 * @param 修饰符 修饰符。
 */
@Composable
fun 剪贴板历史行(调度: 剪贴板同步调度, 序号: Int, 修饰符: Modifier = Modifier) {
    val 快照 by 调度.状态流.collectAsState()
    val 安全序号 = 序号.coerceIn(0, 1024)
    val 文案 by remember(快照, 安全序号) {
        derivedStateOf {
            val 历史 = 调度.查询历史()
            if (历史.isEmpty()) return@derivedStateOf "史" + 安全序号.toString().padStart(3, '0') + " 空"
            val 记录 = 历史.getOrNull(安全序号 % 历史.size)
            "史" + 安全序号.toString().padStart(3, '0') + " " + (记录?.摘要 ?: "空") + " 长=" + (记录?.全长?.toString() ?: "-") + " 向=" +
                (记录?.方向?.name ?: "-")
        }
    }
    Row(
        modifier = 修饰符
            .fillMaxWidth()
            .background(Color.White.copy(alpha = 0.06f), RoundedCornerShape(6.dp))
            .padding(horizontal = 8.dp, vertical = 4.dp),
    ) {
        Text(
            text = 文案,
            style = MaterialTheme.typography.labelSmall,
            color = Color.White,
        )
    }
}

/**
 * 剪贴板历史列。
 *
 * 按序号表键控展示多行，避免调用方复制行函数。
 *
 * @param 调度 调度。
 * @param 序号表 序号表。
 * @param 修饰符 修饰符。
 */
@Composable
fun 剪贴板历史列(调度: 剪贴板同步调度, 序号表: List<Int>, 修饰符: Modifier = Modifier) {
    Column(modifier = 修饰符.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        for (序号 in 序号表) {
            key(序号) {
                剪贴板历史行(调度 = 调度, 序号 = 序号)
            }
        }
    }
}
