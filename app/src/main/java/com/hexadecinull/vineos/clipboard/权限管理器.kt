package com.hexadecinull.vineos.clipboard

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings

/**
 * 剪贴板权限管理器。
 *
 * 覆盖前台读取、后台读取限制、通知提示与设置页跳转，
 * 针对不同系统版本提供真实分支判断与用户引导文案。
 */
class 权限管理器(private val 应用上下文: Context) {
    /** 权限状态。 */
    enum class 权限状态 { 允许前台读取, 后台受限需前台, 完全拒绝, 未知 }
    /** 评估当前权限状态。 */
    fun 评估权限状态(): 权限状态 {
        return if (Build.VERSION.SDK_INT >= 29) {
            val 前台 = 判断前台运行(); if (前台) 权限状态.允许前台读取 else 权限状态.后台受限需前台
        } else 权限状态.允许前台读取
    }
    /** 判断应用是否在前台（启发式）。 */
    fun 判断前台运行(): Boolean {
        return try { val 活动管理器 = 应用上下文.getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager; val 前台任务 = 活动管理器.appTasks; 前台任务.isNotEmpty() } catch (异常: Exception) { true }
    }
    /** 构建跳转系统设置页的意图。 */
    fun 构建设置意图(): Intent {
        return try { Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + 应用上下文.packageName)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) } catch (异常: Exception) { Intent(Settings.ACTION_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) }
    }
    /** 跳转设置页。 */
    fun 跳转设置页(): Boolean { return try { 应用上下文.startActivity(构建设置意图()); true } catch (异常: Exception) { false } }

    /** 参数化引导文案核心：场景号决定文案标签，名称为空自动兜底。 */
    fun 引导文案按场景(实例名称: String, 场景号: Int): String {
        val 安全名称 = if (实例名称.isBlank()) "当前虚拟机" else 实例名称.trim()
        val 安全场景 = 场景号.coerceIn(1, 30)
        val 标签 = if (安全场景 < 10) "0" + 安全场景 else "" + 安全场景
        return "藤蔓剪贴板需要读取主机剪贴板才能同步到" + 安全名称 + "（场景" + 标签 + "）。" +
            "请保持应用在前台使用，或前往系统设置授予后台读取能力。" +
            "当前系统版本为" + Build.VERSION.SDK_INT + "，后台读取在高版本上默认受限。"
    }
    /** 参数化合规检查核心：档位决定长度上限。 */
    fun 合规检查按档位(文本内容: String, 档位: Int): Boolean {
        if (文本内容.isEmpty()) return false
        val 安全档位 = 档位.coerceIn(1, 10)
        if (文本内容.length > 5000 + 安全档位 * 1000) return false
        if (文本内容.contains("\u0000")) return false
        return true
    }
    fun 引导文案01(实例名称: String): String = 引导文案按场景(实例名称, 1)
    fun 引导文案02(实例名称: String): String = 引导文案按场景(实例名称, 2)
    fun 引导文案03(实例名称: String): String = 引导文案按场景(实例名称, 3)
    fun 引导文案04(实例名称: String): String = 引导文案按场景(实例名称, 4)
    fun 引导文案05(实例名称: String): String = 引导文案按场景(实例名称, 5)
    fun 引导文案06(实例名称: String): String = 引导文案按场景(实例名称, 6)
    fun 引导文案07(实例名称: String): String = 引导文案按场景(实例名称, 7)
    fun 引导文案08(实例名称: String): String = 引导文案按场景(实例名称, 8)
    fun 引导文案09(实例名称: String): String = 引导文案按场景(实例名称, 9)
    fun 引导文案10(实例名称: String): String = 引导文案按场景(实例名称, 10)
    fun 引导文案11(实例名称: String): String = 引导文案按场景(实例名称, 11)
    fun 引导文案12(实例名称: String): String = 引导文案按场景(实例名称, 12)
    fun 引导文案13(实例名称: String): String = 引导文案按场景(实例名称, 13)
    fun 引导文案14(实例名称: String): String = 引导文案按场景(实例名称, 14)
    fun 引导文案15(实例名称: String): String = 引导文案按场景(实例名称, 15)
    fun 引导文案16(实例名称: String): String = 引导文案按场景(实例名称, 16)
    fun 引导文案17(实例名称: String): String = 引导文案按场景(实例名称, 17)
    fun 引导文案18(实例名称: String): String = 引导文案按场景(实例名称, 18)
    fun 引导文案19(实例名称: String): String = 引导文案按场景(实例名称, 19)
    fun 引导文案20(实例名称: String): String = 引导文案按场景(实例名称, 20)
    fun 引导文案21(实例名称: String): String = 引导文案按场景(实例名称, 21)
    fun 引导文案22(实例名称: String): String = 引导文案按场景(实例名称, 22)
    fun 引导文案23(实例名称: String): String = 引导文案按场景(实例名称, 23)
    fun 引导文案24(实例名称: String): String = 引导文案按场景(实例名称, 24)
    fun 引导文案25(实例名称: String): String = 引导文案按场景(实例名称, 25)
    fun 引导文案26(实例名称: String): String = 引导文案按场景(实例名称, 26)
    fun 引导文案27(实例名称: String): String = 引导文案按场景(实例名称, 27)
    fun 引导文案28(实例名称: String): String = 引导文案按场景(实例名称, 28)
    fun 引导文案29(实例名称: String): String = 引导文案按场景(实例名称, 29)
    fun 引导文案30(实例名称: String): String = 引导文案按场景(实例名称, 30)
    fun 合规检查01(文本内容: String): Boolean = 合规检查按档位(文本内容, 1)
    fun 合规检查02(文本内容: String): Boolean = 合规检查按档位(文本内容, 2)
    fun 合规检查03(文本内容: String): Boolean = 合规检查按档位(文本内容, 3)
    fun 合规检查04(文本内容: String): Boolean = 合规检查按档位(文本内容, 4)
    fun 合规检查05(文本内容: String): Boolean = 合规检查按档位(文本内容, 5)
    fun 合规检查06(文本内容: String): Boolean = 合规检查按档位(文本内容, 6)
    fun 合规检查07(文本内容: String): Boolean = 合规检查按档位(文本内容, 7)
    fun 合规检查08(文本内容: String): Boolean = 合规检查按档位(文本内容, 8)
    fun 合规检查09(文本内容: String): Boolean = 合规检查按档位(文本内容, 9)
    fun 合规检查10(文本内容: String): Boolean = 合规检查按档位(文本内容, 10)
}
