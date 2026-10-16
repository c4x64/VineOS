package com.hexadecinull.vineos.安全

import java.io.File
import java.security.Security
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.bouncycastle.crypto.params.Ed25519PrivateKeyParameters
import org.bouncycastle.crypto.signers.Ed25519Signer
import org.bouncycastle.jce.provider.BouncyCastleProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

/** 许可证校验器单元测试：有效放行；篡改、过期、错设备、吊销一律拒绝。 */
class 许可证校验器测试 {

    private val 校验器 = 许可证校验器()
    private lateinit var 根私钥: Ed25519PrivateKeyParameters
    private lateinit var 根公钥hex: String
    private lateinit var rom包: File

    @Before fun 准备() {
        if (Security.getProvider("BC") == null) Security.addProvider(BouncyCastleProvider())
        val 随机 = java.security.SecureRandom()
        val 私钥字节 = ByteArray(32)
        随机.nextBytes(私钥字节)
        根私钥 = Ed25519PrivateKeyParameters(私钥字节, 0)
        根公钥hex = 根私钥.generatePublicKey().encoded.joinToString("") { "%02x".format(it) }
        rom包 = File.createTempFile("rom包", ".vrom")
        rom包.writeBytes(ByteArray(4096) { it.toByte() })
    }

    private fun 签发许可证(设备标识: String = "测试设备", 过期偏移秒: Long = 3600, 包哈希: String? = null,): String {
        val 体 = buildJsonObject {
            put("版本", 1)
            put("rom标识", "vine-rom-test")
            put("rom包哈希", 包哈希 ?: 校验器.计算包哈希(rom包))
            put("设备标识", 设备标识)
            put("设备公钥hex", "ab".repeat(32))
            put("签发时间", 1_700_000_000L)
            put("过期时间", 1_700_000_000L + 过期偏移秒)
        }
        val 消息 = 校验器.规范字节(体)
        val 签名器 = Ed25519Signer()
        签名器.init(true, 根私钥)
        签名器.update(消息, 0, 消息.size)
        val 签名hex = 签名器.generateSignature().joinToString("") { "%02x".format(it) }
        return Json.encodeToString(
            kotlinx.serialization.json.JsonObject.serializer(),
            buildJsonObject {
                put("许可证", 体)
                put("根签名hex", 签名hex)
            },
        )
    }

    @Test fun 有效许可证放行() {
        val 结论 = 校验器.校验(签发许可证(), rom包, "测试设备", 根公钥hex, emptySet(), 现在秒 = 1_700_000_100L)
        assertEquals(许可证校验器.校验结论.通过, 结论)
    }

    @Test fun 篡改ROM包拒绝() {
        val 许可证 = 签发许可证()
        rom包.appendBytes(byteArrayOf(0x58))
        val 结论 = 校验器.校验(许可证, rom包, "测试设备", 根公钥hex, emptySet(), 现在秒 = 1_700_000_100L)
        assertTrue(结论 is 许可证校验器.校验结论.拒绝)
    }

    @Test fun 篡改许可证主体拒绝() {
        val 篡改后 = 签发许可证().replace("vine-rom-test", "vine-rom-evil")
        val 结论 = 校验器.校验(篡改后, rom包, "测试设备", 根公钥hex, emptySet(), 现在秒 = 1_700_000_100L)
        assertTrue(结论 is 许可证校验器.校验结论.拒绝)
    }

    @Test fun 过期许可证拒绝() {
        val 结论 = 校验器.校验(签发许可证(过期偏移秒 = -10), rom包, "测试设备", 根公钥hex, emptySet(), 现在秒 = 1_700_000_100L)
        assertTrue(结论 is 许可证校验器.校验结论.拒绝)
    }

    @Test fun 错设备拒绝() {
        val 结论 = 校验器.校验(签发许可证(), rom包, "别人的设备", 根公钥hex, emptySet(), 现在秒 = 1_700_000_100L)
        assertTrue(结论 is 许可证校验器.校验结论.拒绝)
    }

    @Test fun 被吊销设备拒绝且不影响他人() {
        val 结论甲 = 校验器.校验(签发许可证(设备标识 = "作恶设备"), rom包, "作恶设备", 根公钥hex, setOf("作恶设备"), 现在秒 = 1_700_000_100L)
        assertTrue(结论甲 is 许可证校验器.校验结论.拒绝)
        val 结论乙 = 校验器.校验(签发许可证(设备标识 = "好设备"), rom包, "好设备", 根公钥hex, setOf("作恶设备"), 现在秒 = 1_700_000_100L)
        assertEquals(许可证校验器.校验结论.通过, 结论乙)
    }

    @Test fun 规范字节与服务端一致() {
        val 体 = buildJsonObject {
            put("版本", 1)
            put("rom标识", "x")
            put("rom包哈希", "y")
            put("设备标识", "z")
            put("设备公钥hex", "ab")
            put("签发时间", 2L)
            put("过期时间", 3L)
        }
        val 期望 = "{\"rom包哈希\":\"y\",\"rom标识\":\"x\",\"版本\":1,\"签发时间\":2,\"设备公钥hex\":\"ab\",\"设备标识\":\"z\",\"过期时间\":3}"
        assertEquals(期望, 校验器.规范字节(体).toString(Charsets.UTF_8))
    }

    @Test fun 吊销列表验签通过且篡改拒绝() {
        val 列表体 = buildJsonObject {
            put("版本", 1)
            put("吊销设备", kotlinx.serialization.json.buildJsonArray { add(JsonPrimitive("作恶设备")) })
            put("更新时间", 1_700_000_000L)
        }
        val 消息 = 校验器.规范字节(列表体 as JsonObject)
        val 签名器 = Ed25519Signer()
        签名器.init(true, 根私钥)
        签名器.update(消息, 0, 消息.size)
        val 签名hex = 签名器.generateSignature().joinToString("") { "%02x".format(it) }
        val 文本 = Json.encodeToString(
            JsonObject.serializer(),
            buildJsonObject {
                put("列表", 列表体)
                put("根签名hex", 签名hex)
            },
        )
        assertEquals(setOf("作恶设备"), 校验器.解析吊销列表(文本, 根公钥hex))
        val 篡改 = 文本.replace("作恶设备", "好设备")
        try {
            校验器.解析吊销列表(篡改, 根公钥hex)
            throw AssertionError("篡改的吊销列表应当被拒绝")
        } catch (e: IllegalArgumentException) {
            // 预期
        }
    }
}
