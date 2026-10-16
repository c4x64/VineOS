package com.hexadecinull.vineos.安全

import java.io.File
import java.io.FileInputStream
import java.security.MessageDigest
import java.security.Security
import java.util.TreeMap
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import org.bouncycastle.jce.provider.BouncyCastleProvider

/**
 * 许可证校验器：纯逻辑，可在 JVM 单元测试中直接验证。
 * 校验链（任一失败即拒绝）：
 * 1. 许可证根签名有效（根公钥，Ed25519，规范 JSON）。
 * 2. ROM 包哈希与许可证载明一致（流式计算，大文件不入堆）。
 * 3. 未过期；设备标识与本机一致；设备不在吊销列表中。
 * 规范 JSON 与服务端脚本完全一致：键按 Unicode 码点排序，
 * 分隔符为英文逗号与英文冒号且无空格，非 ASCII 字符原文输出。
 */
@Singleton
class 许可证校验器 @Inject constructor() {

    sealed interface 校验结论 {
        data object 通过 : 校验结论
        data class 拒绝(val 原因: String) : 校验结论
    }

    data class 许可证体(
        val 版本: Long,
        val rom标识: String,
        val rom包哈希: String,
        val 设备标识: String,
        val 设备公钥hex: String,
        val 签发时间: Long,
        val 过期时间: Long,
    )

    private val 解析器 = Json { ignoreUnknownKeys = true }

    /** 解析并校验许可证，返回结论。吊销集合传已验签的吊销设备标识集。 */
    fun 校验(
        许可证文本: String,
        rom包: File,
        本地设备标识: String,
        根公钥hex: String,
        吊销设备: Set<String>,
        现在秒: Long = System.currentTimeMillis() / 1000,
    ): 校验结论 {
        val 根 = try {
            解析器.parseToJsonElement(许可证文本).jsonObject
        } catch (e: Exception) {
            return 校验结论.拒绝("许可证不是合法 JSON")
        }
        val 体对象 = 根["许可证"] as? JsonObject ?: return 校验结论.拒绝("许可证缺少主体")
        val 签名hex = (根["根签名hex"] as? JsonPrimitive)?.content.orEmpty()
        if (签名hex.isBlank()) return 校验结论.拒绝("许可证缺少根签名")
        if (!验根签名(体对象, 签名hex, 根公钥hex)) return 校验结论.拒绝("根签名无效")
        val 体 = try {
            解析主体(体对象)
        } catch (e: Exception) {
            return 校验结论.拒绝("许可证主体字段非法")
        }
        if (体.设备标识 in 吊销设备) return 校验结论.拒绝("设备已被吊销，仅该设备受影响")
        if (体.设备标识 != 本地设备标识) return 校验结论.拒绝("设备标识与本机不符")
        if (现在秒 > 体.过期时间) return 校验结论.拒绝("许可证已过期")
        if (!rom包.isFile) return 校验结论.拒绝("ROM 包文件不存在")
        val 实际哈希 = 计算包哈希(rom包)
        if (!实际哈希.equals(体.rom包哈希, ignoreCase = true)) {
            return 校验结论.拒绝("ROM 包哈希不符，可能被篡改")
        }
        return 校验结论.通过
    }

    /** 校验已签名的吊销列表，返回吊销设备标识集（签名无效抛异常）。 */
    fun 解析吊销列表(列表文本: String, 根公钥hex: String): Set<String> {
        val 根 = 解析器.parseToJsonElement(列表文本).jsonObject
        val 列表体 = 根.getValue("列表").jsonObject
        if (!验根签名(列表体, 根.getValue("根签名hex").jsonPrimitive.content, 根公钥hex)) {
            throw IllegalArgumentException("吊销列表根签名无效")
        }
        return 列表体.getValue("吊销设备").jsonArray.map { it.jsonPrimitive.content }.toSet()
    }

    private fun 解析主体(体: JsonObject): 许可证体 = 许可证体(
        版本 = 体.getValue("版本").jsonPrimitive.long,
        rom标识 = 体.getValue("rom标识").jsonPrimitive.content,
        rom包哈希 = 体.getValue("rom包哈希").jsonPrimitive.content,
        设备标识 = 体.getValue("设备标识").jsonPrimitive.content,
        设备公钥hex = 体.getValue("设备公钥hex").jsonPrimitive.content,
        签发时间 = 体.getValue("签发时间").jsonPrimitive.long,
        过期时间 = 体.getValue("过期时间").jsonPrimitive.long,
    )

    private fun 验根签名(体: JsonObject, 签名hex: String, 根公钥hex: String): Boolean {
        return try {
            确保BC()
            val 工厂 = java.security.KeyFactory.getInstance("Ed25519", "BC")
            val 公钥 = 工厂.generatePublic(
                java.security.spec.X509EncodedKeySpec(x509包装(根公钥hex.hexToBytes())),
            )
            val 验证 = java.security.Signature.getInstance("Ed25519", "BC")
            验证.initVerify(公钥)
            验证.update(规范字节(体))
            验证.verify(签名hex.hexToBytes())
        } catch (e: Exception) {
            false
        }
    }

    /** X.509 SubjectPublicKeyInfo 包装 32 字节 Ed25519 裸公钥。 */
    private fun x509包装(裸公钥: ByteArray): ByteArray {
        require(裸公钥.size == 32) { "Ed25519 公钥须为 32 字节" }
        val 前缀 = byteArrayOf(0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00)
        return 前缀 + 裸公钥
    }

    /** 与服务端脚本一致的规范字节：TreeMap 按码点排序，扁平值手动序列化。 */
    internal fun 规范字节(体: JsonObject): ByteArray {
        val 有序 = TreeMap<String, Any?>()
        for ((键, 值) in 体) {
            有序[键] = when (值) {
                is JsonArray -> 值.map { it.jsonPrimitive.content }
                is JsonObject -> throw IllegalArgumentException("许可证主体须为扁平结构")
                is JsonPrimitive -> if (值.isString) 值.content else 值.long
                else -> 值.toString()
            }
        }
        val 构造 = StringBuilder("{")
        有序.entries.forEachIndexed { 下标, (键, 值) ->
            if (下标 > 0) 构造.append(',')
            构造.append('"').append(转义(键)).append('"').append(':')
            when (值) {
                is Number -> 构造.append(值.toLong())
                is List<*> -> {
                    构造.append('[')
                    值.forEachIndexed { i, 项 ->
                        if (i > 0) 构造.append(',')
                        构造.append('"').append(转义(项.toString())).append('"')
                    }
                    构造.append(']')
                }
                else -> 构造.append('"').append(转义(值.toString())).append('"')
            }
        }
        return 构造.append('}').toString().toByteArray(Charsets.UTF_8)
    }

    /** 与 Python ensure_ascii=False 一致：仅转义引号、反斜杠与控制字符。 */
    private fun 转义(原文: String): String {
        val 出 = StringBuilder(原文.length + 8)
        for (字符 in 原文) {
            when (字符) {
                '"' -> 出.append("\\\"")
                '\\' -> 出.append("\\\\")
                '\n' -> 出.append("\\n")
                '\r' -> 出.append("\\r")
                '\t' -> 出.append("\\t")
                '\b' -> 出.append("\\b")
                '\u000C' -> 出.append("\\f")
                else -> if (字符 < ' ') 出.append("\\u%04x".format(字符.code)) else 出.append(字符)
            }
        }
        return 出.toString()
    }

    /** 流式 SHA-256，大 ROM 不入堆。 */
    fun 计算包哈希(包: File): String {
        val 摘要 = MessageDigest.getInstance("SHA-256")
        FileInputStream(包).use { 输入 ->
            val 缓冲 = ByteArray(1024 * 1024)
            while (true) {
                val 读到 = 输入.read(缓冲)
                if (读到 < 0) break
                摘要.update(缓冲, 0, 读到)
            }
        }
        return 摘要.digest().joinToString("") { "%02x".format(it) }
    }

    private fun 确保BC() {
        if (Security.getProvider("BC") == null) {
            Security.addProvider(BouncyCastleProvider())
        }
    }

    private fun String.hexToBytes(): ByteArray {
        val 纯 = trim()
        require(纯.length % 2 == 0 && 纯.all { it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' }) { "非法十六进制" }
        return ByteArray(纯.length / 2) { i -> 纯.substring(i * 2, i * 2 + 2).toInt(16).toByte() }
    }
}
