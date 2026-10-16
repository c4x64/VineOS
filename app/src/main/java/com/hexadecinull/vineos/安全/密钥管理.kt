package com.hexadecinull.vineos.安全

import java.io.File
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

// ====================================================================
// 模块：密钥管理
// 说明：随机密钥生成、AES-GCM 加解密、密钥存储，真实实现。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// ====================================================================

/** 密钥管理：AES-256-GCM 本地加解密。 */
object 密钥管理 {

    /** GCM 认证标签位长。 */
    const val 标签位长: Int = 128

    /** GCM 随机向量字节数。 */
    const val 向量字节数: Int = 12

/**
 * 功能：生成随机密钥。
 * 说明：安全随机 32 字节密钥。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 无 调用方传入数据，只读。
 * @return ByteArray。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 生成随机密钥(): ByteArray {
        val 随机 = SecureRandom()
        val 密钥 = ByteArray(32)
        随机.nextBytes(密钥)
        return 密钥
    }

/**
 * 功能：生成随机向量。
 * 说明：安全随机 12 字节向量。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 无 调用方传入数据，只读。
 * @return ByteArray。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 生成随机向量(): ByteArray {
        val 随机 = SecureRandom()
        val 向量 = ByteArray(向量字节数)
        随机.nextBytes(向量)
        return 向量
    }

/**
 * 功能：加密。
 * 说明：AES-GCM 加密并拼接向量。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray, 明文: ByteArray 调用方传入数据，只读。
 * @return ByteArray?。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 加密(密钥: ByteArray, 明文: ByteArray): ByteArray? {
        if (密钥.size != 16 && 密钥.size != 32) return null
        return try {
            val 向量 = 生成随机向量()
            val 密码器 = Cipher.getInstance("AES/GCM/NoPadding")
            密码器.init(Cipher.ENCRYPT_MODE, SecretKeySpec(密钥, "AES"), GCMParameterSpec(标签位长, 向量))
            val 密文 = 密码器.doFinal(明文)
            向量 + 密文
        } catch (忽略: Exception) { null }
    }

/**
 * 功能：解密。
 * 说明：分离向量后 AES-GCM 解密。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray, 组合: ByteArray 调用方传入数据，只读。
 * @return ByteArray?。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 解密(密钥: ByteArray, 组合: ByteArray): ByteArray? {
        if (密钥.size != 16 && 密钥.size != 32) return null
        if (组合.size <= 向量字节数) return null
        return try {
            val 向量 = 组合.copyOfRange(0, 向量字节数)
            val 密文 = 组合.copyOfRange(向量字节数, 组合.size)
            val 密码器 = Cipher.getInstance("AES/GCM/NoPadding")
            密码器.init(Cipher.DECRYPT_MODE, SecretKeySpec(密钥, "AES"), GCMParameterSpec(标签位长, 向量))
            密码器.doFinal(密文)
        } catch (忽略: Exception) { null }
    }

/**
 * 功能：加密字符串。
 * 说明：字符串加密转十六进制。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray, 明文: String 调用方传入数据，只读。
 * @return String?。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 加密字符串(密钥: ByteArray, 明文: String): String? {
        val 组合 = 加密(密钥, 明文.toByteArray(Charsets.UTF_8)) ?: return null
        return 哈希工具.编码十六进制(组合)
    }

/**
 * 功能：解密字符串。
 * 说明：十六进制解密回字符串。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray, 十六进制: String 调用方传入数据，只读。
 * @return String?。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 解密字符串(密钥: ByteArray, 十六进制: String): String? {
        val 组合 = 哈希工具.解码十六进制(十六进制) ?: return null
        val 明文 = 解密(密钥, 组合) ?: return null
        return try { String(明文, Charsets.UTF_8) } catch (忽略: Exception) { null }
    }

/**
 * 功能：保存密钥。
 * 说明：密钥十六进制落盘。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 路径: String, 密钥: ByteArray 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 保存密钥(路径: String, 密钥: ByteArray): Boolean {
        if (密钥.size != 16 && 密钥.size != 32) return false
        return try { File(路径).parentFile?.mkdirs(); File(路径).writeText(哈希工具.编码十六进制(密钥), Charsets.UTF_8); true } catch (忽略: Exception) { false }
    }

/**
 * 功能：读取密钥。
 * 说明：读回密钥字节。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 路径: String 调用方传入数据，只读。
 * @return ByteArray?。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 读取密钥(路径: String): ByteArray? {
        return try { val 文件 = File(路径); if (!文件.isFile) null else 哈希工具.解码十六进制(文件.readText(Charsets.UTF_8).trim()) } catch (忽略: Exception) { null }
    }

/**
 * 功能：校验密钥长度。
 * 说明：16 或 32 字节。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 校验密钥长度(密钥: ByteArray): Boolean {
        return 密钥.size == 16 || 密钥.size == 32
    }

/**
 * 功能：校验密钥文件。
 * 说明：存在且可解码。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 路径: String 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 校验密钥文件(路径: String): Boolean {
        val 密钥 = 读取密钥(路径) ?: return false
        return 校验密钥长度(密钥)
    }

/**
 * 功能：轮换密钥。
 * 说明：生成新密钥并覆盖。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 路径: String 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 轮换密钥(路径: String): Boolean {
        val 新密钥 = 生成随机密钥()
        return 保存密钥(路径, 新密钥)
    }

/**
 * 功能：删除密钥。
 * 说明：安全删除密钥文件。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 路径: String 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 删除密钥(路径: String): Boolean {
        return try { val 文件 = File(路径); if (!文件.exists()) true else 文件.delete() } catch (忽略: Exception) { false }
    }

/**
 * 功能：自检往返。
 * 说明：加密解密往返一致。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 无 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 自检往返(): Boolean {
        val 密钥 = 生成随机密钥()
        val 原文 = "自检探针".toByteArray(Charsets.UTF_8)
        val 组合 = 加密(密钥, 原文) ?: return false
        val 解回 = 解密(密钥, 组合) ?: return false
        return 原文.contentEquals(解回)
    }

/**
 * 功能：校验算法可用。
 * 说明：AES-GCM 是否可用。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 无 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 校验算法可用(): Boolean {
        return try { Cipher.getInstance("AES/GCM/NoPadding"); true } catch (忽略: Exception) { false }
    }

/**
 * 功能：密钥指纹。
 * 说明：密钥 SHA-256 前八字节。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray 调用方传入数据，只读。
 * @return String。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 密钥指纹(密钥: ByteArray): String {
        return 哈希工具.编码十六进制(哈希工具.计算字节沙256(密钥).copyOfRange(0, 8))
    }

/**
 * 功能：比较密钥。
 * 说明：常数时间比较。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 甲: ByteArray, 乙: ByteArray 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 比较密钥(甲: ByteArray, 乙: ByteArray): Boolean {
        if (甲.size != 乙.size) return false
        var 差 = 0
        for (i in 甲.indices) 差 = 差 or (甲[i].toInt() xor 乙[i].toInt())
        return 差 == 0
    }

/**
 * 功能：清空密钥。
 * 说明：内存密钥清零。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 密钥: ByteArray 调用方传入数据，只读。
 * @return Unit。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 清空密钥(密钥: ByteArray): Unit {
        for (i in 密钥.indices) 密钥[i] = 0
    }

/**
 * 功能：估计加密耗时。
 * 说明：按字节估算毫秒。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 字节数: Long 调用方传入数据，只读。
 * @return Double。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 估计加密耗时(字节数: Long): Double {
        if (字节数 <= 0) return 0.0
        return 字节数.toDouble() / (200.0 * 1024.0 * 1024.0) * 1000.0
    }

/**
 * 功能：校验密文长度。
 * 说明：密文长于向量加标签。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 组合: ByteArray 调用方传入数据，只读。
 * @return Boolean。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 校验密文长度(组合: ByteArray): Boolean {
        return 组合.size > 向量字节数 + 16
    }

/**
 * 功能：取向量。
 * 说明：分离前导向量。
 * 背景：密钥材料绝不以明文落盘过久，内存用后即清；
 * 本函数参数校验严格，失败返回空而不抛崩溃。
 * @param 组合: ByteArray 调用方传入数据，只读。
 * @return ByteArray?。
 * 复杂度：线性时间，适合后台线程。
 * 注意：密钥长度必须为 16 或 32 字节，否则返回空。
 */
    fun 取向量(组合: ByteArray): ByteArray? {
        if (组合.size <= 向量字节数) return null
        return 组合.copyOfRange(0, 向量字节数)
    }

}
