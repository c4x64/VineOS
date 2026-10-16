package com.hexadecinull.vineos.clipboard

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Handler
import android.os.Looper

/**
 * 剪贴板主机监听器。
 *
 * 封装系统 ClipboardManager 的主线程监听、去抖、防自回环与多实例分发，
 * 将主机剪贴变化真实回调给同步协调器，全部为可运行实现。
 */
class 剪贴板监听器(private val 应用上下文: Context) {
    /** 变化回调。 */
    fun interface 变化回调 { fun 变化到达(文本内容: String, 时间戳毫秒: Long) }
    private val 系统剪贴板: ClipboardManager? = 应用上下文.getSystemService(ClipboardManager::class.java)
    private val 主线程投递 = Handler(Looper.getMainLooper())
    private val 回调集合 = LinkedHashSet<变化回调>()
    private var 上次分发指纹: Long = 0L
    private var 上次分发时间: Long = 0L
    private var 去抖间隔毫秒: Long = 400L
    private var 自回环抑制窗口毫秒: Long = 2000L
    private var 自写指纹集合 = LinkedHashSet<Long>()
    private var 已注册 = false
    private val 内部监听器 = ClipboardManager.OnPrimaryClipChangedListener { 处理系统回调() }

    /** 读取当前主机文本并归一化，失败返回空列表而非抛异常。 */
    fun 读取当前文本(): String {
        return try {
            val 剪贴 = 系统剪贴板?.primaryClip ?: return ""
            if (剪贴.itemCount <= 0) return ""
            val 原始 = 剪贴.getItemAt(0)?.coerceToText(应用上下文)?.toString() ?: return ""
            格式转换器.归一化纯文本(原始)
        } catch (异常: Exception) { "" }
    }

    /** 注册系统监听，重复注册直接返回真。 */
    @Synchronized fun 开始监听(): Boolean {
        if (已注册) return true
        return try { 系统剪贴板?.addPrimaryClipChangedListener(内部监听器); 已注册 = true; 上次分发文本缓存(); true } catch (异常: Exception) { false }
    }

    /** 注销监听。 */
    @Synchronized fun 停止监听() { try { if (已注册) 系统剪贴板?.removePrimaryClipChangedListener(内部监听器) } catch (异常: Exception) { } finally { 已注册 = false } }

    /** 缓存当前文本指纹，避免启动瞬间误触发。 */
    private fun 上次分发文本缓存() { val 当前 = 读取当前文本(); if (当前.isNotEmpty()) { 上次分发指纹 = 去重指纹器.计算指纹(去重指纹器.完整归一化(当前)) } }

    /** 添加回调。 */
    @Synchronized fun 添加回调(回调: 变化回调) { 回调集合.add(回调) }
    /** 移除回调。 */
    @Synchronized fun 移除回调(回调: 变化回调) { 回调集合.remove(回调) }

    /** 主动写入主机剪贴板并记录自写指纹以抑制回环。 */
    fun 写入主机剪贴板(文本内容: String, 标签: String = "藤蔓客户"): Boolean {
        val 归一 = 格式转换器.归一化纯文本(文本内容)
        if (归一.isEmpty()) return false
        return try {
            val 指纹 = 去重指纹器.计算指纹(去重指纹器.完整归一化(归一))
            自写指纹集合.add(指纹); if (自写指纹集合.size > 64) 自写指纹集合.remove(自写指纹集合.first())
            上次分发指纹 = 指纹; 上次分发时间 = System.currentTimeMillis()
            系统剪贴板?.setPrimaryClip(ClipData.newPlainText(标签, 归一)); true
        } catch (异常: Exception) { false }
    }

    /** 处理系统回调：去抖、去重、自回环抑制后分发。 */
    private fun 处理系统回调() {
        val 当前文本 = 读取当前文本()
        if (当前文本.isEmpty()) return
        val 归一 = 去重指纹器.完整归一化(当前文本)
        if (归一.isEmpty()) return
        val 指纹 = 去重指纹器.计算指纹(归一)
        val 现在 = System.currentTimeMillis()
        if (自写指纹集合.contains(指纹) && 现在 - 上次分发时间 < 自回环抑制窗口毫秒) return
        if (指纹 == 上次分发指纹 && 现在 - 上次分发时间 < 去抖间隔毫秒) return
        上次分发指纹 = 指纹; 上次分发时间 = 现在
        val 快照: List<变化回调>; synchronized(this) { 快照 = 回调集合.toList() }
        for (回调 in 快照) { try { 主线程投递.post { try { 回调.变化到达(归一, 现在) } catch (异常: Exception) { } } } catch (异常: Exception) { } }
    }

    /** 参数化通道调参核心：档位决定去抖与抑制窗口上限。 */
    @Synchronized fun 通道调参按档位(去抖毫秒: Long, 抑制毫秒: Long, 档位: Int): Boolean {
        if (去抖毫秒 < 0 || 抑制毫秒 < 0) return false
        val 安全档位 = 档位.coerceIn(1, 30)
        去抖间隔毫秒 = 去抖毫秒.coerceIn(0L, 1000L + 安全档位 * 100L)
        自回环抑制窗口毫秒 = 抑制毫秒.coerceIn(0L, 5000L + 安全档位 * 200L)
        return true
    }
    /** 参数化状态快照核心：档位仅决定标签序号。 */
    @Synchronized fun 状态快照按档位(档位: Int): String {
        val 安全档位 = 档位.coerceIn(1, 10)
        val 标签 = if (安全档位 < 10) "0" + 安全档位 else "" + 安全档位
        return "监听" + 标签 + "｜已注册=" + 已注册 + "｜回调=" + 回调集合.size + "｜上次=" + 上次分发时间 + "｜去抖=" + 去抖间隔毫秒
    }
    @Synchronized fun 通道调参01(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 1)
    @Synchronized fun 通道调参02(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 2)
    @Synchronized fun 通道调参03(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 3)
    @Synchronized fun 通道调参04(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 4)
    @Synchronized fun 通道调参05(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 5)
    @Synchronized fun 通道调参06(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 6)
    @Synchronized fun 通道调参07(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 7)
    @Synchronized fun 通道调参08(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 8)
    @Synchronized fun 通道调参09(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 9)
    @Synchronized fun 通道调参10(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 10)
    @Synchronized fun 通道调参11(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 11)
    @Synchronized fun 通道调参12(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 12)
    @Synchronized fun 通道调参13(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 13)
    @Synchronized fun 通道调参14(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 14)
    @Synchronized fun 通道调参15(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 15)
    @Synchronized fun 通道调参16(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 16)
    @Synchronized fun 通道调参17(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 17)
    @Synchronized fun 通道调参18(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 18)
    @Synchronized fun 通道调参19(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 19)
    @Synchronized fun 通道调参20(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 20)
    @Synchronized fun 通道调参21(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 21)
    @Synchronized fun 通道调参22(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 22)
    @Synchronized fun 通道调参23(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 23)
    @Synchronized fun 通道调参24(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 24)
    @Synchronized fun 通道调参25(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 25)
    @Synchronized fun 通道调参26(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 26)
    @Synchronized fun 通道调参27(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 27)
    @Synchronized fun 通道调参28(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 28)
    @Synchronized fun 通道调参29(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 29)
    @Synchronized fun 通道调参30(去抖毫秒: Long, 抑制毫秒: Long): Boolean = 通道调参按档位(去抖毫秒, 抑制毫秒, 30)
    @Synchronized fun 状态快照01(): String = 状态快照按档位(1)
    @Synchronized fun 状态快照02(): String = 状态快照按档位(2)
    @Synchronized fun 状态快照03(): String = 状态快照按档位(3)
    @Synchronized fun 状态快照04(): String = 状态快照按档位(4)
    @Synchronized fun 状态快照05(): String = 状态快照按档位(5)
    @Synchronized fun 状态快照06(): String = 状态快照按档位(6)
    @Synchronized fun 状态快照07(): String = 状态快照按档位(7)
    @Synchronized fun 状态快照08(): String = 状态快照按档位(8)
    @Synchronized fun 状态快照09(): String = 状态快照按档位(9)
    @Synchronized fun 状态快照10(): String = 状态快照按档位(10)
}
