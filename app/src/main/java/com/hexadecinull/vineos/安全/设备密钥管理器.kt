package com.hexadecinull.vineos.安全

import java.io.File
import java.security.MessageDigest
import java.security.SecureRandom
import javax.inject.Inject
import javax.inject.Singleton
import org.bouncycastle.crypto.AsymmetricCipherKeyPair
import org.bouncycastle.crypto.generators.Ed25519KeyPairGenerator
import org.bouncycastle.crypto.params.Ed25519KeyGenerationParameters
import org.bouncycastle.crypto.params.Ed25519PrivateKeyParameters
import org.bouncycastle.crypto.params.Ed25519PublicKeyParameters
import org.bouncycastle.crypto.signers.Ed25519Signer

/**
 * 设备密钥管理器：每台设备独立生成 Ed25519 密钥对，设备标识为公钥哈希。
 * 私钥仅存于应用私有目录（权限六零零），用于注册时签名质询，证明私钥持有。
 * 服务端只见公钥；某设备作恶被吊销时不影响其他设备。
 */
@Singleton
class 设备密钥管理器 @Inject constructor() {

    private val 随机源 = SecureRandom()

    /** 取或创建本机设备密钥对，返回（设备标识，公钥hex）。 */
    fun 取或创建(密钥目录: File): Pair<String, String> {
        val 私钥文件 = File(密钥目录, "设备私钥.hex")
        val 公钥文件 = File(密钥目录, "设备公钥.hex")
        if (私钥文件.isFile && 公钥文件.isFile) {
            val 公钥hex = 公钥文件.readText().trim().lowercase()
            return Pair(设备标识(公钥hex), 公钥hex)
        }
        val 生成器 = Ed25519KeyPairGenerator()
        生成器.init(Ed25519KeyGenerationParameters(随机源))
        val 密钥对: AsymmetricCipherKeyPair = 生成器.generateKeyPair()
        val 公钥字节 = (密钥对.public as Ed25519PublicKeyParameters).encoded
        val 私钥字节 = (密钥对.private as Ed25519PrivateKeyParameters).encoded
        密钥目录.mkdirs()
        私钥文件.writeText(私钥字节.toHex())
        公钥文件.writeText(公钥字节.toHex())
        私钥文件.setReadable(false, false)
        私钥文件.setReadable(true, true)
        私钥文件.setWritable(false, false)
        私钥文件.setWritable(true, true)
        val 公钥hex = 公钥字节.toHex()
        return Pair(设备标识(公钥hex), 公钥hex)
    }

    /** 设备标识 = 公钥原始字节的 SHA-256 十六进制。 */
    fun 设备标识(公钥hex: String): String {
        val 摘要 = MessageDigest.getInstance("SHA-256")
        return 摘要.digest(公钥hex.hexToBytes()).toHex()
    }

    /** 用设备私钥签名注册质询，供服务端核验私钥持有。 */
    fun 签名质询(密钥目录: File, 质询: ByteArray): ByteArray {
        val 私钥字节 = File(密钥目录, "设备私钥.hex").readText().trim().hexToBytes()
        val 签名器 = Ed25519Signer()
        签名器.init(true, Ed25519PrivateKeyParameters(私钥字节, 0))
        签名器.update(质询, 0, 质询.size)
        return 签名器.generateSignature()
    }

    private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }

    private fun String.hexToBytes(): ByteArray {
        val 纯 = trim()
        require(纯.length % 2 == 0) { "非法十六进制" }
        return ByteArray(纯.length / 2) { i -> 纯.substring(i * 2, i * 2 + 2).toInt(16).toByte() }
    }
}
