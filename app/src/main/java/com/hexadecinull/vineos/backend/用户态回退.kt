package com.hexadecinull.vineos.backend

import android.util.Log
import com.hexadecinull.vineos.domain.VineVMManager
import java.io.File
import javax.inject.Inject
import javax.inject.Singleton

/**
 * 非 root 设备用户态回退（路线图第四阶段）。
 *
 * 当命名空间创建被拒绝时，虚拟机管理可回退到在用户态下运行
 * 客户根文件系统，而不使用真实命名空间。本文件负责探测与
 * 命令构造；执行仍由前台服务承载，以保持生命周期一致。
 */
@Singleton
class 用户态回退 @Inject constructor(private val 虚拟机管理: VineVMManager) {

    fun 是否可用(): Boolean = 虚拟机管理.hostSupportsProot() || 内置程序存在()

    fun 构造命令(
        用户态程序: String,
        根文件目录: String,
        实例数据目录: String,
    ): List<String> {
        val 命令 = mutableListOf(
            用户态程序,
            "-r", 根文件目录,
            "-b", "$实例数据目录:/data",
            "-b", "/dev:/dev",
            "-b", "/proc:/proc",
            "-b", "/sys:/sys",
            "-0",
            "/init",
            "androidboot.hardware=vine",
            "androidboot.selinux=permissive",
        )
        Log.i(日志标签, "用户态回退命令: ${命令.joinToString(" ")}")
        return 命令
    }

    private fun 内置程序存在(): Boolean {
        // 随应用原生库目录附带的用户态程序经由应用进程包装执行。
        return runCatching {
            File("/system/bin/proot").exists() || File("/system/xbin/proot").exists()
        }.getOrDefault(false)
    }

    companion object {
        private const val 日志标签 = "用户态回退"
    }
}
