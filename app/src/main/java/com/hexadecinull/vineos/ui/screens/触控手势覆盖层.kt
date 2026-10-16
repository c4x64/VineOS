package com.hexadecinull.vineos.ui.screens

import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.hexadecinull.vineos.ui.viewmodel.多指触控分发器
import com.hexadecinull.vineos.ui.viewmodel.手势导航增强
import com.hexadecinull.vineos.ui.viewmodel.显示状态机
import com.hexadecinull.vineos.ui.viewmodel.显示状态

/**
 * 触控手势覆盖层。
 *
 * 职责：
 * - 承载 SurfaceView 并按 getPointerId 稳定上报多指。
 * - 绘制触控圆点、手势轨迹、状态条。
 * - 与中文视图模型协作，不直接持有原生句柄。
 *
 * 效率说明：
 * - 快照收集后走 derivedStateOf 提炼，避免整列无意义重组。
 * - 调试条合并为单函数加列表列，每帧只查一次全表。
 * - 触控抬起按编号定位加越界保护，不再直写动作索引。
 */

/**
 * 稳定多指表面。
 *
 * 槽位一律取自 getPointerId，索引仅用于取坐标。
 *
 * @param 分发器 分发器。
 * @param 手势 手势识别器。
 * @param 表面就绪 表面就绪回调。
 * @param 表面销毁 表面销毁回调。
 * @param 多指上报 多指上报回调。
 * @param 单指上报 单指上报回调。
 * @param 修饰符 修饰符。
 */
@Composable
fun 稳定多指表面(
    分发器: 多指触控分发器,
    手势: 手势导航增强,
    表面就绪: (android.view.Surface) -> Unit,
    表面销毁: () -> Unit,
    多指上报: (IntArray, FloatArray, FloatArray, BooleanArray) -> Unit,
    单指上报: (Int, Float, Float) -> Unit,
    修饰符: Modifier = Modifier,
) {
    val 最新分发器 by rememberUpdatedState(分发器)
    val 最新手势 by rememberUpdatedState(手势)
    val 最新表面就绪 by rememberUpdatedState(表面就绪)
    val 最新表面销毁 by rememberUpdatedState(表面销毁)
    val 最新多指上报 by rememberUpdatedState(多指上报)
    val 最新单指上报 by rememberUpdatedState(单指上报)
    AndroidView(
        factory = { 上下文 ->
            SurfaceView(上下文).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(持有者: SurfaceHolder) { 最新表面就绪(持有者.surface) }
                    override fun surfaceChanged(持有者: SurfaceHolder, 格式: Int, 宽: Int, 高: Int) {}
                    override fun surfaceDestroyed(持有者: SurfaceHolder) { 最新表面销毁() }
                })
                setOnTouchListener { _, 事件 ->
                    if (事件.pointerCount > 1) {
                        val 数量 = minOf(事件.pointerCount, 10)
                        val 编号集 = IntArray(数量) { 索引 ->事件.getPointerId(索引) }
                        val 横集 = FloatArray(数量) { 索引 ->事件.getX(索引) }
                        val 纵集 = FloatArray(数量) { 索引 ->事件.getY(索引) }
                        val 存活集 = BooleanArray(数量) { true }
                        when (事件.actionMasked) {
                            MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_UP -> {
                                val 抬起位 = 最新分发器.定位抬起位置(事件, 编号集, 数量)
                                if (抬起位 in 0 until 数量)存活集[抬起位] = false
                            }
                            MotionEvent.ACTION_CANCEL ->存活集.fill(false)
                            else -> {}
                        }
                        最新分发器.分发多指事件(事件.actionMasked, 事件.actionIndex, 数量, 编号集, 横集, 纵集, 存活集)
                        最新多指上报(编号集, 横集, 纵集, 存活集)
                    } else {
                        when (事件.actionMasked) {
                            MotionEvent.ACTION_DOWN -> {
                                最新手势.触摸按下(事件.x, 事件.y)
                                最新单指上报(0, 事件.x, 事件.y)
                            }
                            MotionEvent.ACTION_MOVE -> {
                                最新手势.触摸移动(事件.x, 事件.y)
                                最新单指上报(1, 事件.x, 事件.y)
                            }
                            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                                最新手势.触摸抬起(事件.x, 事件.y)
                                最新单指上报(2, 事件.x, 事件.y)
                            }
                        }
                    }
                    true
                }
            }
        },
        modifier = 修饰符,
    )
}

/**
 * 触控圆点覆盖。
 *
 * 快照提炼为存活点表后绘制，空快照直接返回。
 *
 * @param 分发器 分发器。
 * @param 修饰符 修饰符。
 */
@Composable
fun 触控圆点覆盖(分发器: 多指触控分发器, 修饰符: Modifier = Modifier) {
    val 快照 by 分发器.快照流.collectAsState(initial = null)
    val 点表 by remember(快照) {
        derivedStateOf {
            val 当前 = 快照 ?: return@derivedStateOf emptyList<Triple<Float, Float, Boolean>>()
            List(当前.有效数量) { 索引 ->
                Triple(当前.横坐标数组[索引], 当前.纵坐标数组[索引], 当前.存活数组[索引])
            }
        }
    }
    Box(modifier = 修饰符.fillMaxSize()) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            for ((横, 纵, 存活) in 点表) {
                drawCircle(
                    color = if (存活) Color(0xFF4FC3F7) else Color(0xFFFF8A65),
                    radius = 42f,
                    center = Offset(横, 纵),
                )
            }
        }
    }
}

/**
 * 显示状态横幅。
 *
 * 状态与性能合并为一行文案记忆，避免文本拼接每帧重算。
 *
 * @param 状态机 状态机。
 * @param 修饰符 修饰符。
 */
@Composable
fun 显示状态横幅(状态机: 显示状态机, 修饰符: Modifier = Modifier) {
    val 状态 by 状态机.状态流.collectAsState()
    val 性能 by 状态机.性能流.collectAsState()
    val 文案 by remember(状态, 性能) {
        derivedStateOf { "状态=" + 状态.name + " 帧=" + 性能.总帧数 + " 掉帧=" + 性能.掉帧数 + " 首帧=" + 性能.首帧耗时 + "ms" }
    }
    val 是否启动中 by remember(状态) { derivedStateOf { 状态 == 显示状态.启动中 } }
    Column(
        modifier = 修饰符
            .fillMaxWidth()
            .background(Color.Black.copy(alpha = 0.55f))
            .padding(horizontal = 12.dp, vertical = 8.dp),
    ) {
        Text(
            text = 文案,
            style = MaterialTheme.typography.labelMedium,
            color = Color.White,
        )
        if (是否启动中) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
        }
    }
}

/**
 * 调试触控条。
 *
 * 展示指定槽位的占用、坐标、速度，供开发者排查映射。
 * 按 getPointerId 取编号，保证抬指后不漂移。
 * 旧编号函数族已合并为本函数加序号参数。
 *
 * @param 分发器 分发器。
 * @param 槽位 槽位序号。
 * @param 序号 调试序号。
 * @param 修饰符 修饰符。
 */
@Composable
fun 调试触控条(分发器: 多指触控分发器, 槽位: Int, 序号: Int = 0, 修饰符: Modifier = Modifier) {
    val 快照 by 分发器.快照流.collectAsState(initial = null)
    val 详情 by remember(快照, 槽位) { derivedStateOf { 分发器.查询槽位状态(槽位) } }
    val 编号文 = 详情?.指针编号?.toString() ?: "空"
    val 横文 = 详情?.横坐标?.toString() ?: "-"
    val 纵文 = 详情?.纵坐标?.toString() ?: "-"
    val 是否按下 = 详情?.是否按下 == true
    val 文案 by remember(槽位, 序号, 编号文, 横文, 纵文) {
        derivedStateOf { "条" + 序号.toString().padStart(3, '0') + " 槽=" + 槽位 + " 编号=" + 编号文 + " 横=" + 横文 + " 纵=" + 纵文 }
    }
    Row(
        modifier = 修饰符
            .fillMaxWidth()
            .background(Color.Black.copy(alpha = 0.35f))
            .padding(horizontal = 10.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            modifier = Modifier
                .size(14.dp)
                .background(if (是否按下) Color.Green else Color.Gray, CircleShape),
        )
        Text(
            text = 文案,
            style = MaterialTheme.typography.labelSmall,
            color = Color.White,
            modifier = Modifier.padding(start = 8.dp),
        )
    }
}

/**
 * 调试触控条列。
 *
 * 每帧只查一次全表，按槽位键控，避免整列无键重组。
 *
 * @param 分发器 分发器。
 * @param 槽位表 槽位表。
 * @param 修饰符 修饰符。
 */
@Composable
fun 调试触控条列(分发器: 多指触控分发器, 槽位表: List<Int>, 修饰符: Modifier = Modifier) {
    val 快照 by 分发器.快照流.collectAsState(initial = null)
    val 全表 by remember(快照) { derivedStateOf { 分发器.查询全部槽位状态() } }
    Column(modifier = 修饰符.fillMaxWidth()) {
        for ((表索引, 槽位) in 槽位表.withIndex()) {
            key(槽位) {
                val 详情 = 全表.getOrNull(槽位)
                val 是否按下 = 详情?.是否按下 == true
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(Color.Black.copy(alpha = 0.35f))
                        .padding(horizontal = 10.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Box(
                        modifier = Modifier
                            .size(14.dp)
                            .background(if (是否按下) Color.Green else Color.Gray, CircleShape),
                    )
                    Text(
                        text = "条" + 表索引.toString().padStart(3, '0') + " 槽=" + 槽位 + " 编号=" + (详情?.指针编号?.toString() ?: "空"),
                        style = MaterialTheme.typography.labelSmall,
                        color = Color.White,
                        modifier = Modifier.padding(start = 8.dp),
                    )
                }
            }
        }
    }
}
