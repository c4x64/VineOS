package com.hexadecinull.vineos.ui.screens

import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Square
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import com.hexadecinull.vineos.ui.viewmodel.VMDisplayViewModel

@Composable
fun VMDisplayScreen(instanceId: String, onBack: () -> Unit, modifier: Modifier = Modifier, vm: VMDisplayViewModel = hiltViewModel()) {
    DisposableEffect(instanceId) {
        vm.attach(instanceId)
        onDispose { vm.detach() }
    }

    Box(modifier = modifier.fillMaxSize().background(Color.Black)) {
        GuestSurface(
            onSurfaceReady = { surface -> vm.onSurfaceReady(surface) },
            onSurfaceDestroyed = { vm.onSurfaceDestroyed() },
            onTouch = { action, x, y -> vm.sendTouch(action, x, y) },
            onMultitouch = { slots, xs, ys, actives -> vm.sendMultitouch(slots, xs, ys, actives) },
            modifier = Modifier.fillMaxSize(),
        )

        // Navigation bar overlay: back, home, recents
        NavBar(
            onBack = { vm.sendBack() },
            onHome = { vm.sendHome() },
            onRecents = { vm.sendRecents() },
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(bottom = 16.dp),
        )

        // Booting overlay
        if (vm.isBooting) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.85f)),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = "Starting instance…",
                    style = MaterialTheme.typography.titleMedium,
                    color = Color.White,
                )
            }
        }
    }
}

@Composable
private fun GuestSurface(
    onSurfaceReady: (android.view.Surface) -> Unit,
    onSurfaceDestroyed: () -> Unit,
    onTouch: (action: Int, x: Float, y: Float) -> Unit,
    onMultitouch: (slots: IntArray, xs: FloatArray, ys: FloatArray, actives: BooleanArray) -> Unit,
    modifier: Modifier = Modifier,
) {
    AndroidView(
        factory = { context ->
            SurfaceView(context).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(holder: SurfaceHolder) {
                        onSurfaceReady(holder.surface)
                    }
                    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}
                    override fun surfaceDestroyed(holder: SurfaceHolder) {
                        onSurfaceDestroyed()
                    }
                })
                setOnTouchListener { _, event ->
                    if (event.pointerCount > 1) {
                        // 多指类型 B：槽位必须按 getPointerId 稳定映射，不能用指针索引。
                        // 指针索引会在手指抬起后发生压缩重排，指针编号则在整个手势内稳定。
                        val n = minOf(event.pointerCount, 10)
                        val 槽位数 = n
                        val 槽位映射 = IntArray(槽位数) { 索引 -> event.getPointerId(索引) }
                        val 横坐标集 = FloatArray(槽位数) { 索引 -> event.getX(索引) }
                        val 纵坐标集 = FloatArray(槽位数) { 索引 -> event.getY(索引) }
                        val 存活标志集 = BooleanArray(槽位数) { true }
                        when (event.actionMasked) {
                            MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_UP -> {
                                // 按抬起编号定位后再标记，避免截断后动作索引越界。
                                val 抬起位 = try {
                                    val 抬起编号 = event.getPointerId(event.actionIndex)
                                    var 命中 = -1
                                    for (i in 0 until 槽位数) if (槽位映射[i] == 抬起编号) { 命中 = i; break }
                                    命中
                                } catch (异常: Exception) { -1 }
                                if (抬起位 in 0 until 槽位数) 存活标志集[抬起位] = false
                                else if (event.actionIndex in 0 until 槽位数) 存活标志集[event.actionIndex] = false
                            }
                            MotionEvent.ACTION_CANCEL ->
                                存活标志集.fill(false)
                            MotionEvent.ACTION_POINTER_DOWN, MotionEvent.ACTION_MOVE -> {
                                // 保持全部存活，由分发器按编号去重与钳制。
                            }
                        }
                        onMultitouch(槽位映射, 横坐标集, 纵坐标集, 存活标志集)
                    } else {
                        when (event.actionMasked) {
                            MotionEvent.ACTION_DOWN -> onTouch(0, event.x, event.y)
                            MotionEvent.ACTION_MOVE -> onTouch(1, event.x, event.y)
                            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> onTouch(2, event.x, event.y)
                        }
                    }
                    true
                }
            }
        },
        modifier = modifier,
    )
}

@Composable
private fun NavBar(onBack: () -> Unit, onHome: () -> Unit, onRecents: () -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .background(Color.Black.copy(alpha = 0.5f), CircleShape)
            .padding(horizontal = 20.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        FloatingActionButton(
            onClick = onBack,
            modifier = Modifier.size(44.dp),
            containerColor = Color.White.copy(alpha = 0.15f),
            contentColor = Color.White,
        ) {
            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
        }
        Box(modifier = Modifier.size(16.dp))
        FloatingActionButton(
            onClick = onHome,
            modifier = Modifier.size(44.dp),
            containerColor = Color.White.copy(alpha = 0.15f),
            contentColor = Color.White,
        ) {
            Icon(Icons.Filled.Home, contentDescription = "Home")
        }
        Box(modifier = Modifier.size(16.dp))
        FloatingActionButton(
            onClick = onRecents,
            modifier = Modifier.size(44.dp),
            containerColor = Color.White.copy(alpha = 0.15f),
            contentColor = Color.White,
        ) {
            Icon(Icons.Filled.Square, contentDescription = "Recents")
        }
    }
}
