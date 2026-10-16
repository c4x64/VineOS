package com.hexadecinull.vineos.storage

import com.hexadecinull.vineos.data.models.VMInstance
import com.hexadecinull.vineos.domain.VineVMManager
import java.io.File
import javax.inject.Inject
import javax.inject.Singleton

/**
 * 实例快照与恢复（路线图第三阶段）。
 *
 * 快照为实例数据目录的压缩归档，存放于实例快照目录下，经由
 * 原生运行时创建与恢复，大数据不经过堆内存。
 */
@Singleton
class 快照管理器 @Inject constructor(private val 虚拟机管理: VineVMManager) {

    fun 列出(实例: VMInstance): List<String> {
        val 目录 = File(实例.storagePath, "snapshots")
        if (!目录.isDirectory) return emptyList()
        return 目录.listFiles { 文件 -> 文件.extension == "gz" }
            ?.map { it.nameWithoutExtension.substringBeforeLast(".tar") }
            .orEmpty().sorted()
    }

    fun 创建快照(实例: VMInstance, 名称: String): Boolean {
        val 净名 = 名称.trim().replace(Regex("[^A-Za-z0-9._-]"), "_")
        if (净名.isEmpty()) return false
        return 虚拟机管理.snapshot(实例, 净名)
    }

    fun 恢复快照(实例: VMInstance, 名称: String): Boolean = 虚拟机管理.restoreSnapshot(实例, 名称)

    fun 删除快照(实例: VMInstance, 名称: String): Boolean = File(实例.storagePath, "snapshots/$名称.tar.gz").delete()
}
