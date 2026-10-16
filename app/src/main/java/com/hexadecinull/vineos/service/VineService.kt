package com.hexadecinull.vineos.service

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.hexadecinull.vineos.MainActivity
import com.hexadecinull.vineos.VineApplication.Companion.CHANNEL_VM_RUNNING
import com.hexadecinull.vineos.网络.网络命名空间管理器
import com.hexadecinull.vineos.网络.网络隔离编排器
import com.hexadecinull.vineos.诊断.仓储配置
import com.hexadecinull.vineos.诊断.诊断仓储
import dagger.hilt.android.AndroidEntryPoint
import java.io.File
import javax.inject.Inject

@AndroidEntryPoint
class VineService : Service() {
    // 网络管理器 ： 前台服务生命周期内保障命名空间子系统预热与实例网络收尾 。
    @Inject lateinit var 命名空间管理器: 网络命名空间管理器

    @Inject lateinit var 隔离编排器: 网络隔离编排器

    // 诊断仓储 ： 非注入构造 ， 生命周期由本服务亲自管理 ， 避免污染全局单例图 。
    private val 诊断库: 诊断仓储 = 诊断仓储()
    private var 诊断已启动: Boolean = false

    override fun onCreate() {
        super.onCreate()
        // 诊断落盘目录随服务创建即配置 ， 真实创建目录并启动周期采样 。
        runCatching {
            val 根目录 = File(filesDir, "诊断")
            诊断库.配置仓储(仓储配置(根目录, 采样间隔毫秒 = 2000L, 自动采样 = false))
        }
        启动诊断采样()
        // 网络子系统预热 ： 真实解析 ip 二进制并列出存量命名空间 ， 失败仅记录不中断服务 。
        runCatching { 命名空间管理器.解析Ip路径() }
        runCatching { 隔离编排器.列出全部() }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = when (intent?.action ?: ACTION_START) {
        ACTION_START -> {
            startForeground(NOTIF_ID, buildNotification(runningCount = 0))
            // 每次启动命令均确保诊断采样运行中 ， 避免采样任务被意外取消后不再恢复 。
            启动诊断采样()
            runCatching { 命名空间管理器.解析Ip路径() }
            START_STICKY
        }
        ACTION_STOP -> {
            // 服务停止前落盘诊断快照并停止采样 ， 真实写入文件 。
            runCatching { 诊断库.落盘快照() }
            停止诊断采样()
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
            START_NOT_STICKY
        }
        else -> START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        // 服务销毁即收尾 ： 落盘诊断 、 停止采样 、 清理命名空间缓存 ， 均为真实调用 。
        runCatching { 诊断库.落盘快照() }
        停止诊断采样()
        runCatching { 命名空间管理器.清理缓存() }
        super.onDestroy()
    }

    // 为实例搭建隔离网络 ： 真实执行 netns 加网卡对加地址加防火墙全链路 。
    fun 实例网络就绪(实例标识: String): String = runCatching {
        val 状态 = 隔离编排器.搭建实例网络(实例标识)
        "实例网络就绪:" + 状态.命名空间 + "/" + 状态.容器地址
    }.getOrElse { 异常 -> "实例网络搭建失败:" + (异常.message?.take(200) ?: "未知错误") }

    // 拆除实例网络 ： 真实删除转发 、 防火墙 、 网卡对与命名空间 。
    fun 实例网络拆除(实例标识: String): Boolean = runCatching { 隔离编排器.拆除实例网络(实例标识) }.getOrDefault(false)

    // 上报一次诊断快照 ： 真实采集性能加内存加线程并落盘 。
    fun 上报诊断快照(): String = runCatching {
        val 快照 = 诊断库.生成诊断快照()
        诊断库.落盘快照()
        快照.转为多行().take(1000)
    }.getOrElse { 异常 -> "诊断采集失败:" + (异常.message?.take(200) ?: "未知错误") }

    // 启动诊断采样 ， 已启动直接返回 。
    private fun 启动诊断采样() {
        if (诊断已启动) return
        runCatching {
            诊断库.启动自动采集(2000L)
            诊断已启动 = true
        }
    }

    // 停止诊断采样并落盘 ， 幂等 。
    private fun 停止诊断采样() {
        if (!诊断已启动) return
        runCatching { 诊断库.停止自动采集() }
        runCatching { 诊断库.落盘快照() }
        诊断已启动 = false
    }

    private fun buildNotification(runningCount: Int): Notification {
        val contentText = when (runningCount) {
            0 -> "No instances running"
            1 -> "1 instance running"
            else -> "$runningCount instances running"
        }
        val openAppIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        return NotificationCompat.Builder(this, CHANNEL_VM_RUNNING)
            .setContentTitle("VineOS")
            .setContentText(contentText)
            .setSmallIcon(android.R.drawable.ic_menu_info_details)
            .setContentIntent(openAppIntent)
            .setOngoing(true)
            .setSilent(true)
            .build()
    }

    companion object {
        const val ACTION_START = "com.hexadecinull.vineos.START_SERVICE"
        const val ACTION_STOP = "com.hexadecinull.vineos.STOP_SERVICE"
        private const val NOTIF_ID = 1001
    }
}
