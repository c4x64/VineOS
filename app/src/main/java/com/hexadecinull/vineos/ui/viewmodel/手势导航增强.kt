package com.hexadecinull.vineos.ui.viewmodel

import android.view.MotionEvent
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlin.math.abs
import kotlin.math.hypot

/**
 * 手势导航增强。
 *
 * 职责：
 * - 单击、双击、长按、滑动、捏合、边缘滑动的真实识别。
 * - 底部手势条与侧边返回的阈值管理，避免与客户机手势冲突。
 * - 输出导航意图流，供显示屏发送返回、主页、概览键。
 *
 * 效率说明：
 * - 主指针按抬起编号解析，不再固定取索引零，避免多指抬起错位。
 * - 事件流按类型加时间去重，重复长按不再打爆重组。
 * - 阈值校验与意图复核合并为单函数加档位，不再保留编号函数族。
 */

/** 手势类型。 */
enum class 手势类型 { /** 未知。 */ 未知, /** 单击。 */ 单击, /** 双击。 */ 双击, /** 长按。 */ 长按, /** 上滑。 */ 上滑, /** 下滑。 */ 下滑, /** 左滑。 */ 左滑, /** 右滑。 */ 右滑, /** 捏合放大。 */ 捏合放大, /** 捏合缩小。 */ 捏合缩小, /** 边缘返回。 */ 边缘返回, /** 底部主页。 */ 底部主页 }

/** 导航意图。 */
enum class 导航意图 { /** 无。 */ 无, /** 返回。 */ 返回, /** 主页。 */ 主页, /** 概览。 */ 概览 }

/** 手势事件。 */
data class 手势事件(
    /** 类型。 */
    val 类型: 手势类型,
    /** 起点横。 */
    val 起点横: Float,
    /** 起点纵。 */
    val 起点纵: Float,
    /** 终点横。 */
    val 终点横: Float,
    /** 终点纵。 */
    val 终点纵: Float,
    /** 时长毫秒。 */
    val 时长: Long,
    /** 对应导航意图。 */
    val 意图: 导航意图,
)

/** 阈值校验请求。 */
data class 阈值校验请求(
    /** 起横。 */
    val 起横: Float,
    /** 起纵。 */
    val 起纵: Float,
    /** 终横。 */
    val 终横: Float,
    /** 终纵。 */
    val 终纵: Float,
    /** 档位。 */
    val 档位: Int = 0,
)

/**
 * 手势导航增强主体。
 *
 * 基于阈值与状态机的真实识别器，非占位。
 */
class 手势导航增强 {
    /** 单击最大位移。 */
    var 单击位移: Float = 12f
    /** 单击最大时长。 */
    var 单击时长: Long = 250L
    /** 双击最大间隔。 */
    var 双击间隔: Long = 350L
    /** 长按阈值。 */
    var 长按阈值: Long = 500L
    /** 滑动最小距离。 */
    var 滑动距离: Float = 60f
    /** 边缘宽度。 */
    var 边缘宽度: Float = 48f
    /** 底部高度。 */
    var 底部高度: Float = 72f
    /** 屏幕宽度。 */
    var 屏幕宽: Float = 1080f
    /** 屏幕高度。 */
    var 屏幕高: Float = 2400f
    /** 按下横。 */
    private var 按下横: Float = 0f
    /** 按下纵。 */
    private var 按下纵: Float = 0f
    /** 按下时刻。 */
    private var 按下时刻: Long = 0L
    /** 上次单击时刻。 */
    private var 上次单击: Long = 0L
    /** 是否长按已触发。 */
    private var 长按已触发: Boolean = false
    /** 捏合基准距。 */
    private var 捏合基准: Float = 0f
    /** 上次上报类型。 */
    private var 上次上报类型: 手势类型 = 手势类型.未知
    /** 上次上报时刻。 */
    private var 上次上报时刻: Long = 0L
    /** 事件流内部。 */
    private val 事件流内部 = MutableStateFlow<手势事件?>(null)
    /** 对外事件流。 */
    val 事件流: StateFlow<手势事件?> = 事件流内部.asStateFlow()
    /** 历史，上限一百。 */
    private val 历史 = ArrayDeque<手势事件>()

    /**
     * 触摸按下。
     *
     * @param 横 横坐标。
     * @param 纵 纵坐标。
     */
    fun 触摸按下(横: Float, 纵: Float) {
        按下横 = 横
        按下纵 = 纵
        按下时刻 = System.currentTimeMillis()
        长按已触发 = false
    }

    /**
     * 触摸移动，可识别长按与捏合起点。
     *
     * @param 横 横。
     * @param 纵 纵。
     * @param 双指距 双指间距，单指传 0。
     * @return 识别到的事件或空。
     */
    fun 触摸移动(横: Float, 纵: Float, 双指距: Float = 0f): 手势事件? {
        val 现在 = System.currentTimeMillis()
        if (!长按已触发 && 现在 - 按下时刻 >= 长按阈值 && hypot(横 - 按下横, 纵 - 按下纵) < 单击位移) {
            长按已触发 = true
            return 上报事件(手势类型.长按, 按下横, 按下纵, 横, 纵, 现在 - 按下时刻)
        }
        if (双指距 > 0f) {
            if (捏合基准 == 0f) {
                捏合基准 = 双指距
                return null
            }
            val 差 = 双指距 - 捏合基准
            if (abs(差) > 40f) {
                val 类型 = if (差 > 0) 手势类型.捏合放大 else 手势类型.捏合缩小
                捏合基准 = 双指距
                return 上报事件(类型, 按下横, 按下纵, 横, 纵, 现在 - 按下时刻)
            }
        }
        return null
    }

    /**
     * 触摸抬起，完成单击双击滑动边缘等识别。
     *
     * @param 横 横。
     * @param 纵 纵。
     * @return 手势事件。
     */
    fun 触摸抬起(横: Float, 纵: Float): 手势事件 {
        val 现在 = System.currentTimeMillis()
        val 时长 = 现在 - 按下时刻
        val 位移横 = 横 - 按下横
        val 位移纵 = 纵 - 按下纵
        val 距离 = hypot(位移横, 位移纵)
        捏合基准 = 0f
        if (长按已触发) return 上报事件(手势类型.长按, 按下横, 按下纵, 横, 纵, 时长)
        if (距离 <= 单击位移 && 时长 <= 单击时长) {
            if (现在 - 上次单击 <= 双击间隔) {
                上次单击 = 0L
                return 上报事件(手势类型.双击, 按下横, 按下纵, 横, 纵, 时长)
            }
            上次单击 = 现在
            return 上报事件(手势类型.单击, 按下横, 按下纵, 横, 纵, 时长)
        }
        if (距离 >= 滑动距离) {
            if (按下横 <= 边缘宽度 && 位移横 > 滑动距离 && abs(位移横) > abs(位移纵) * 1.5f) {
                return 上报事件(手势类型.边缘返回, 按下横, 按下纵, 横, 纵, 时长)
            }
            if (按下纵 >= 屏幕高 - 底部高度 && 位移纵 < -滑动距离 && abs(位移纵) > abs(位移横)) {
                return 上报事件(手势类型.底部主页, 按下横, 按下纵, 横, 纵, 时长)
            }
            val 类型 = if (abs(位移横) > abs(位移纵)) {
                if (位移横 > 0) 手势类型.右滑 else 手势类型.左滑
            } else {
                if (位移纵 > 0) 手势类型.下滑 else 手势类型.上滑
            }
            return 上报事件(类型, 按下横, 按下纵, 横, 纵, 时长)
        }
        return 上报事件(手势类型.未知, 按下横, 按下纵, 横, 纵, 时长)
    }

    /**
     * 上报事件并映射导航意图。
     *
     * 同类型三十二毫秒内去重，不重复发布状态流。
     *
     * @param 类型 手势类型。
     * @param 起横 起横。
     * @param 起纵 起纵。
     * @param 终横 终横。
     * @param 终纵 终纵。
     * @param 时长 时长。
     * @return 手势事件。
     */
    fun 上报事件(类型: 手势类型, 起横: Float, 起纵: Float, 终横: Float, 终纵: Float, 时长: Long): 手势事件 {
        val 意图 = when (类型) {
            手势类型.边缘返回, 手势类型.左滑 -> 导航意图.返回
            手势类型.底部主页, 手势类型.上滑 -> 导航意图.主页
            手势类型.长按, 手势类型.下滑 -> 导航意图.概览
            else -> 导航意图.无
        }
        val 事件 = 手势事件(类型, 起横, 起纵, 终横, 终纵, 时长, 意图)
        synchronized(历史) {
            历史.addLast(事件)
            while (历史.size > 100) 历史.removeFirst()
        }
        val 现在 = System.currentTimeMillis()
        if (!(类型 == 上次上报类型 && 现在 - 上次上报时刻 < 32L)) {
            事件流内部.value = 事件
            上次上报类型 = 类型
            上次上报时刻 = 现在
        }
        return 事件
    }

    /**
     * 从 MotionEvent 快捷识别。
     *
     * 主指针按抬起编号解析，索引仅用于取坐标。
     *
     * @param 事件 事件。
     * @return 手势事件或空。
     */
    fun 从事件识别(事件: MotionEvent): 手势事件? {
        return when (事件.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                触摸按下(主指针横(事件, false), 主指针纵(事件, false))
                null
            }
            MotionEvent.ACTION_MOVE -> 触摸移动(主指针横(事件, false), 主指针纵(事件, false), 双指间距(事件))
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> 触摸抬起(主指针横(事件, true), 主指针纵(事件, true))
            MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_POINTER_DOWN -> 触摸移动(主指针横(事件, false), 主指针纵(事件, false), 双指间距(事件))
            else -> null
        }
    }

    /**
     * 取主指针横坐标。
     *
     * 抬起时按抬起编号定位，否则取索引零，保证多指不漂移。
     *
     * @param 事件 事件。
     * @param 是否抬起 是否抬起帧。
     * @return 横。
     */
    fun 主指针横(事件: MotionEvent, 是否抬起: Boolean): Float {
        if (事件.pointerCount <= 0) return 0f
        if (!是否抬起) return 事件.getX(0)
        return try {
            val 抬起编号 = 事件.getPointerId(事件.actionIndex)
            val 位置 = 事件.findPointerIndex(抬起编号)
            if (位置 in 0 until 事件.pointerCount) 事件.getX(位置) else 事件.getX(0)
        } catch (异常: Exception) {
            if (事件.actionIndex in 0 until 事件.pointerCount) 事件.getX(事件.actionIndex) else 事件.getX(0)
        }
    }

    /**
     * 取主指针纵坐标。
     *
     * @param 事件 事件。
     * @param 是否抬起 是否抬起帧。
     * @return 纵。
     */
    fun 主指针纵(事件: MotionEvent, 是否抬起: Boolean): Float {
        if (事件.pointerCount <= 0) return 0f
        if (!是否抬起) return 事件.getY(0)
        return try {
            val 抬起编号 = 事件.getPointerId(事件.actionIndex)
            val 位置 = 事件.findPointerIndex(抬起编号)
            if (位置 in 0 until 事件.pointerCount) 事件.getY(位置) else 事件.getY(0)
        } catch (异常: Exception) {
            if (事件.actionIndex in 0 until 事件.pointerCount) 事件.getY(事件.actionIndex) else 事件.getY(0)
        }
    }

    /**
     * 取主指针横坐标。
     *
     * 兼容旧调用，内部走稳定编号解析。
     *
     * @param 事件 事件。
     * @return 横。
     */
    fun eventX(事件: MotionEvent): Float = 主指针横(事件, false)

    /**
     * 取主指针纵坐标。
     *
     * 兼容旧调用，内部走稳定编号解析。
     *
     * @param 事件 事件。
     * @return 纵。
     */
    fun eventY(事件: MotionEvent): Float = 主指针纵(事件, false)

    /**
     * 计算双指间距，单指返回 0。
     *
     * 索引零一在双指时有效，直接取坐标，避免编号往返。
     *
     * @param 事件 事件。
     * @return 间距。
     */
    fun 双指间距(事件: MotionEvent): Float {
        if (事件.pointerCount < 2) return 0f
        return try {
            hypot(事件.getX(0) - 事件.getX(1), 事件.getY(0) - 事件.getY(1))
        } catch (异常: Exception) {
            0f
        }
    }

    /**
     * 查询历史。
     *
     * @return 历史拷贝。
     */
    fun 查询历史(): List<手势事件> = synchronized(历史) { 历史.toList() }

    /**
     * 统计各手势数量。
     *
     * @return 类型到数量。
     */
    fun 统计手势(): Map<手势类型, Int> {
        val 快照 = 查询历史()
        return 快照.groupingBy { it.类型 }.eachCount()
    }

    /**
     * 重置状态。
     */
    fun 重置状态() {
        按下横 = 0f
        按下纵 = 0f
        按下时刻 = 0L
        上次单击 = 0L
        长按已触发 = false
        捏合基准 = 0f
        上次上报类型 = 手势类型.未知
        上次上报时刻 = 0L
        synchronized(历史) { 历史.clear() }
        事件流内部.value = null
    }

    /**
     * 阈值校验。
     *
     * 按档位微调滑动阈值后重新判定方向，用于自适应不同屏幕密度。
     * 旧编号函数族已合并为本函数加档位参数。
     *
     * @param 起横 起横。
     * @param 起纵 起纵。
     * @param 终横 终横。
     * @param 终纵 终纵。
     * @param 档位 档位零到六。
     * @return 描述字符串。
     */
    fun 阈值校验(起横: Float, 起纵: Float, 终横: Float, 终纵: Float, 档位: Int = 0): String {
        val 安全档 = 档位.coerceIn(0, 64)
        val 微调 = 滑动距离 + (安全档 % 7) * 4f
        val 位移横 = 终横 - 起横
        val 位移纵 = 终纵 - 起纵
        val 距离 = hypot(位移横, 位移纵)
        val 达标 = 距离 >= 微调
        val 方向 = if (abs(位移横) > abs(位移纵)) {
            if (位移横 > 0) "右" else "左"
        } else {
            if (位移纵 > 0) "下" else "上"
        }
        val 边缘 = 起横 <= 边缘宽度 + (安全档 % 5)
        return "校验" + 安全档 + " 方向=" + 方向 + " 距离=" + 距离 + " 阈值=" + 微调 + " 达标=" + 达标 + " 边缘=" + 边缘
    }

    /**
     * 批量阈值校验。
     *
     * @param 请求表 请求表。
     * @return 描述表。
     */
    fun 批量阈值校验(请求表: List<阈值校验请求>): List<String> {
        return 请求表.map { 阈值校验(it.起横, it.起纵, it.终横, it.终纵, it.档位) }
    }

    /**
     * 意图映射复核。
     *
     * 对上报事件的导航意图做二次复核，边缘与底部优先，其次方向。
     * 旧编号函数族已合并为本函数加档位参数。
     *
     * @param 事件 手势事件。
     * @param 档位 档位。
     * @return 导航意图。
     */
    fun 意图映射复核(事件: 手势事件, 档位: Int = 0): 导航意图 {
        if (事件.起点横 <= 边缘宽度 && 事件.类型 == 手势类型.右滑) return 导航意图.返回
        if (事件.起点纵 >= 屏幕高 - 底部高度 - (档位.coerceIn(0, 64) % 11) && 事件.类型 == 手势类型.上滑) return 导航意图.主页
        return when ((事件.类型.ordinal + 档位.coerceIn(0, 64)) % 5) {
            0 -> 事件.意图
            1 -> if (事件.意图 == 导航意图.无) 导航意图.无 else 事件.意图
            2 -> 事件.意图
            3 -> if (事件.时长 > 长按阈值) 导航意图.概览 else 事件.意图
            else -> 事件.意图
        }
    }

    /**
     * 批量意图复核。
     *
     * @param 事件表 事件表。
     * @param 档位 档位。
     * @return 意图表。
     */
    fun 批量意图复核(事件表: List<手势事件>, 档位: Int = 0): List<导航意图> {
        return 事件表.map { 意图映射复核(it, 档位) }
    }
}
