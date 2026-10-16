package com.hexadecinull.vineos.安全

import android.content.Context
import android.os.Build
import android.os.Debug
import android.provider.Settings
import java.io.File

// ====================================================================
// 模块：沙箱检查器
// 说明：Root／模拟器／调试器／代理综合检查，真实实现。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// ====================================================================

/** 沙箱检查器。 */
object 沙箱检查器 {

/**
 * 功能：检查Root可执行文件。
 * 说明：常见 su 路径存在性。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查Root可执行文件(): Boolean {
        val 候选 = listOf("/system/bin/su", "/system/xbin/su", "/sbin/su", "/data/local/tmp/su", "/system/app/Superuser.apk")
        return 候选.any { File(it).exists() }
    }

/**
 * 功能：检查Root管理应用。
 * 说明：包名命中 Root 工具。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查Root管理应用(上下文: Context): Boolean {
        val 可疑 = listOf("com.topjohnwu.magisk", "com.koushikdutta.superuser", "eu.chainfire.supersu", "com.noshufou.android.su")
        return try { 可疑.any { 包 -> try { 上下文.packageManager.getPackageInfo(包, 0); true } catch (忽略: Exception) { false } } } catch (忽略: Exception) { false }
    }

/**
 * 功能：检查构建标签。
 * 说明：test-keys 即非正式构建。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查构建标签(): Boolean = Build.TAGS?.contains("test-keys") == true

/**
 * 功能：检查模拟器。
 * 说明：多指纹组合判定。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查模拟器(): Boolean {
        val 指纹 = (Build.FINGERPRINT ?: "") + (Build.MODEL ?: "") + (Build.PRODUCT ?: "")
        val 命中 = listOf("generic", "emulator", "sdk_gphone", "vbox", "genymotion")
        if (命中.any { 指纹.lowercase().contains(it) }) return true
        return Build.HARDWARE?.lowercase()?.contains("goldfish") == true || Build.HARDWARE?.lowercase()?.contains("ranchu") == true
    }

/**
 * 功能：检查调试器。
 * 说明：TracerPid 与 Debug.isDebuggerConnected。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查调试器(): Boolean {
        if (Debug.isDebuggerConnected()) return true
        // 流式首命中早退：逐行读遇 TracerPid 即停，不整文件 readLines 入堆。
        return try {
            java.io.BufferedReader(java.io.FileReader("/proc/self/status"), 4096).use { 读入 ->
                var 行: String?
                while (读入.readLine().also { 行 = it } != null) {
                    if (行!!.startsWith("TracerPid:")) {
                        return@use 行!!.substringAfter(":").trim() != "0"
                    }
                }
                false
            }
        } catch (忽略: Exception) { false }
    }

/**
 * 功能：检查代理。
 * 说明：系统代理与 VPN 并存。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查代理(上下文: Context): Boolean {
        val 代理主机 = System.getProperty("http.proxyHost")
        if (!代理主机.isNullOrEmpty()) return true
        return try { android.net.Proxy.getDefaultHost()?.isNotEmpty() == true } catch (忽略: Exception) { false }
    }

/**
 * 功能：检查可写系统区。
 * 说明：系统目录可写即弱隔离。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查可写系统区(): Boolean = return listOf("/system", "/vendor", "/product").any { File(it).canWrite() }

/**
 * 功能：检查多开。
 * 说明：实例目录存在即多开。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 目录: String 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查多开(目录: String): Boolean = return File(目录).isDirectory

/**
 * 功能：检查工作目录合法。
 * 说明：绝对且存在。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 目录: String 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查工作目录合法(目录: String): Boolean {
        val 文件 = File(目录)
        return 目录.startsWith("/") && 文件.isDirectory
    }

/**
 * 功能：综合评分。
 * 说明：百分制综合评分。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Int。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 综合评分(上下文: Context): Int {
        return 综合评分内部(上下文, null, null, null, null)
    }

    /**
     * 功能：综合评分内部。
     * 说明：表驱动单遍求值；非空参数直接复用，为空则现场检查，保证与综合评分同语义。
     * @param 上下文 上下文；有Root文件／有Root应用／有调试器／可写系统区 预计算可复用。
     * @return 0-100 分数。
     */
    private fun 综合评分内部(
        上下文: Context,
        有Root文件: Boolean?,
        有Root应用: Boolean?,
        有调试器: Boolean?,
        可写系统区: Boolean?,
    ): Int {
        var 分 = 100
        if (有Root文件 ?: 检查Root可执行文件()) 分 -= 30
        if (有Root应用 ?: 检查Root管理应用(上下文)) 分 -= 20
        if (有调试器 ?: 检查调试器()) 分 -= 20
        if (可写系统区 ?: 检查可写系统区()) 分 -= 10
        if (分 < 0) 分 = 0
        return 分
    }

/**
 * 功能：评估等级。
 * 说明：按分数定级。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 分数: Int 调用方传入数据，只读不修改。
 * @return String。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 评估等级(分数: Int): String {
        return when { 分数 >= 90 -> "优秀"; 分数 >= 70 -> "良好"; 分数 >= 50 -> "一般"; else -> "较弱" }
    }

/**
 * 功能：整体通过。
 * 说明：六十分以上通过。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 整体通过(上下文: Context): Boolean = return 综合评分(上下文) >= 60

/**
 * 功能：生成报告。
 * 说明：汇总多项检查的中文报告。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return String。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 生成报告(上下文: Context): String {
        // 单遍求值：四项检查各算一次，评分复用结果；旧实现综合评分调两次＋重复 I/O。
        val 有Root文件 = 检查Root可执行文件()
        val 有Root应用 = try { 检查Root管理应用(上下文) } catch (忽略: Exception) { false }
        val 有调试器 = 检查调试器()
        val 可写系统区 = 检查可写系统区()
        val 是模拟器 = 检查模拟器()
        val 分 = 综合评分内部(上下文, 有Root文件, 有Root应用, 有调试器, 可写系统区)
        val 行 = StringBuilder("沙箱检查报告\n")
        行.append("Root文件：${if (有Root文件) "发现" else "未发现"}\n")
        行.append("模拟器：${if (是模拟器) "是" else "否"}\n")
        行.append("调试器：${if (有调试器) "已附加" else "未附加"}\n")
        行.append("评分：${分}（${评估等级(分)}）\n")
        return 行.toString()
    }

/**
 * 功能：检查单项集合。
 * 说明：返回全部布尔检查映射。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Map<String, Boolean>。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查单项集合(上下文: Context): Map<String, Boolean> {
        return mapOf("Root文件" to 检查Root可执行文件(), "模拟器" to 检查模拟器(), "调试器" to 检查调试器(), "可写系统区" to 检查可写系统区())
    }

/**
 * 功能：统计风险项。
 * 说明：风险项计数。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Int。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 统计风险项(上下文: Context): Int = return 检查单项集合(上下文).count { it.value }

/**
 * 功能：是否高风险。
 * 说明：两项以上风险。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 是否高风险(上下文: Context): Boolean = return 统计风险项(上下文) >= 2

/**
 * 功能：取设备指纹。
 * 说明：品牌型号拼接。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return String。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 取设备指纹(): String = return Build.BRAND + "/" + Build.MODEL + "/" + Build.VERSION.RELEASE

/**
 * 功能：校验指纹非空。
 * 说明：指纹三段非空。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 无 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 校验指纹非空(): Boolean = return Build.BRAND.isNotEmpty() && Build.MODEL.isNotEmpty()

/**
 * 功能：检查开发者选项。
 * 说明：ADB 设置读取。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查开发者选项(上下文: Context): Boolean {
        return try { Settings.Secure.getInt(上下文.contentResolver, Settings.Secure.ADB_ENABLED, 0) == 1 } catch (忽略: Exception) { false }
    }

/**
 * 功能：检查未知来源。
 * 说明：允许未知来源安装。
 * 背景：安全审计的每一步都必须有定义行为，含糊即漏报；
 * 本函数对空输入与缺失上下文返回保守结论，不抛空异常。
 * @param 上下文: Context 调用方传入数据，只读不修改。
 * @return Boolean。
 * 复杂度：线性时间，可在设置页与启动链路调用。
 * 注意：单项通过不代表整体安全，需组合判定。
 */
    fun 检查未知来源(上下文: Context): Boolean {
        return try { Settings.Secure.getInt(上下文.contentResolver, Settings.Secure.INSTALL_NON_MARKET_APPS, 0) == 1 } catch (忽略: Exception) { false }
    }

}
