package com.hexadecinull.vineos.安全

import java.io.File
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request

/**
 * 吊销同步器：拉取服务端签名的吊销列表，验签后本地缓存并强制执行。
 * 吊销只针对作恶设备标识，其他设备许可证不受影响。
 */
@Singleton
class 吊销同步器 @Inject constructor(private val 网络: OkHttpClient, private val 校验器: 许可证校验器,) {
    @Volatile private var 缓存: Set<String> = emptySet()

    /** 是否吊销（内存缓存命中即生效，无需等待网络）。 */
    fun 是否吊销(设备标识: String): Boolean = 设备标识 in 缓存

    /** 同步远端吊销列表并验签落盘；失败返回 false 且保留旧缓存。 */
    suspend fun 同步(列表地址: String, 根公钥hex: String, 缓存目录: File): Boolean = withContext(Dispatchers.IO) {
        try {
            val 响应 = 网络.newCall(Request.Builder().url(列表地址).build()).execute()
            val 文本 = 响应.use {
                if (!it.isSuccessful) return@withContext false
                it.body.string()
            }
            val 吊销集 = 校验器.解析吊销列表(文本, 根公钥hex)
            缓存目录.mkdirs()
            File(缓存目录, "吊销列表.json").writeText(文本)
            缓存 = 吊销集
            true
        } catch (e: Exception) {
            false
        }
    }

    /** 启动时从本地缓存恢复，不阻塞网络。 */
    fun 从本地恢复(缓存目录: File, 根公钥hex: String) {
        val 文件 = File(缓存目录, "吊销列表.json")
        if (!文件.isFile) return
        try {
            缓存 = 校验器.解析吊销列表(文件.readText(), 根公钥hex)
        } catch (e: Exception) {
            文件.delete()
        }
    }
}
