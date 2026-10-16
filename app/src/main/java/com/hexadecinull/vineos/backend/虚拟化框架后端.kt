package com.hexadecinull.vineos.backend

import android.os.Build
import android.util.Log
import com.hexadecinull.vineos.domain.VineVMManager
import javax.inject.Inject
import javax.inject.Singleton

/**
 * 安卓虚拟化框架后端（路线图第四阶段）。
 *
 * 在具备虚拟化服务硬件抽象层的安卓十三及以上主机上，客户机可在
 * 虚拟化框架/微机器人形态下运行，而不使用命名空间。本文件负责
 * 能力探测与负载描述；完整微机器人启动由前台服务在签名负载与
 * 签名文件就绪后发起。
 */
@Singleton
class 虚拟化框架后端 @Inject constructor(private val 虚拟机管理: VineVMManager) {

    data class 虚拟化能力(
        val 是否支持: Boolean,
        val 原因说明: String,
    )

    fun 探测能力(): 虚拟化能力 {
        if (Build.VERSION.SDK_INT < 33) {
            return 虚拟化能力(false, "虚拟化框架需要安卓十三及以上，主版本号=${Build.VERSION.SDK_INT}")
        }
        val 原生支持 = 虚拟机管理.hostSupportsAvf()
        return if (原生支持) {
            虚拟化能力(true, "虚拟化服务已存在")
        } else {
            虚拟化能力(false, "本设备未找到虚拟化服务硬件抽象层")
        }
    }

    fun 负载配置(实例标识: String, 根文件路径: String): Map<String, String> {
        Log.i(日志标签, "虚拟化负载 $实例标识 位于 $根文件路径")
        return mapOf(
            "instanceId" to 实例标识,
            "rootfs" to 根文件路径,
            "os" to "microdroid",
            "hypervisor" to "pKVM",
        )
    }

    companion object {
        private const val 日志标签 = "虚拟化框架后端"
    }
}
