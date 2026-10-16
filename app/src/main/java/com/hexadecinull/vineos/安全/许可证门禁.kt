package com.hexadecinull.vineos.安全

import com.hexadecinull.vineos.BuildConfig
import com.hexadecinull.vineos.data.models.VMInstance
import com.hexadecinull.vineos.data.repository.ROMRepository
import java.io.File
import javax.inject.Inject
import javax.inject.Singleton

/**
 * 许可证门禁：实例启动前的按设备授权检查。
 * 未配置根公钥的开发构建显式放行并上报原因；配置了根公钥的
 * 正式构建缺许可证、验签失败、过期、设备不符或被吊销一律拒绝。
 * 吊销只影响作恶设备自身，其他设备不受牵连。
 */
@Singleton
class 许可证门禁 @Inject constructor(
    private val 密钥管理: 设备密钥管理器,
    private val 校验器: 许可证校验器,
    private val 吊销同步: 吊销同步器,
    private val rom仓库: ROMRepository,
) {
    sealed interface 放行结论 {
        data object 放行 : 放行结论
        data class 拒绝(val 原因: String) : 放行结论
    }

    fun 检查实例(实例: VMInstance, 应用文件目录: File): 放行结论 {
        val 根公钥 = BuildConfig.ROM_CA_PUBKEY_HEX.trim()
        if (根公钥.isEmpty()) return 放行结论.放行
        val rom文件 = rom仓库.getROMFile(实例.romId)
            ?: return 放行结论.拒绝("找不到 ROM 包文件，无法校验许可证")
        val 许可证文件 = File(rom文件.parentFile, "${rom文件.nameWithoutExtension}.license.json")
        if (!许可证文件.isFile) return 放行结论.拒绝("缺少本设备许可证，请向服务端申请")
        val (设备标识, _) = try {
            密钥管理.取或创建(File(应用文件目录, "设备密钥"))
        } catch (e: Exception) {
            return 放行结论.拒绝("设备密钥不可用")
        }
        if (吊销同步.是否吊销(设备标识)) return 放行结论.拒绝("本设备已被吊销，仅本设备受影响")
        return when (val 结论 = 校验器.校验(许可证文件.readText(), rom文件, 设备标识, 根公钥, emptySet())) {
            is 许可证校验器.校验结论.通过 -> 放行结论.放行
            is 许可证校验器.校验结论.拒绝 -> 放行结论.拒绝(结论.原因)
        }
    }
}
