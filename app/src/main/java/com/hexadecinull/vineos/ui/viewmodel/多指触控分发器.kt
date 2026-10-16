package com.hexadecinull.vineos.ui.viewmodel

import android.view.MotionEvent
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

/**
 * 多指触控分发器。
 *
 * 职责：
 * - 按 getPointerId 稳定映射槽位，不使用指针索引，避免抬指后索引压缩错乱。
 * - 维护编号到槽位、槽位到编号的双向映射，支持最多十路并发触控。
 * - 对坐标做钳制、去抖、速度估计，向虚拟机发送类型 B 多点协议。
 * - 收集触控状态快照，供界面层显示调试圆点与轨迹。
 *
 * 槽位协议说明：
 * - 槽位编号取自 MotionEvent.getPointerId，保证手势内稳定。
 * - 存活标志为假表示该编号在本帧抬起，接收方应发送抬起后回收槽位。
 * - 坐标钳制到显示区域，避免负数或越界导致客户机光标漂移。
 *
 * 效率说明：
 * - 槽位分配 O(1)：布尔占用表加空闲栈，不再每帧全表扫描。
 * - 快照流节流加去重：最小发布间隔加坐标量化指纹，避免每帧打爆重组。
 * - 调试查询走批量列表接口，不再保留编号函数族。
 */

/** 触控点信息：记录单指的完整生命周期。 */
data class 触控点信息(
    /** 稳定指针编号，来自 getPointerId。 */
    val 指针编号: Int,
    /** 分配的槽位，与指针编号一一对应。 */
    val 槽位序号: Int,
    /** 当前横坐标，已钳制。 */
    val 横坐标: Float,
    /** 当前纵坐标，已钳制。 */
    val 纵坐标: Float,
    /** 是否仍按下。 */
    val 是否按下: Boolean,
    /** 按下时间戳毫秒。 */
    val 按下时刻: Long,
    /** 最近移动速度像素每秒。 */
    val 移动速度: Float,
)

/** 槽位快照：一次分发后的完整结果。 */
data class 槽位快照(
    /** 槽位数组，内容为槽位序号。 */
    val 槽位数组: IntArray,
    /** 横坐标数组。 */
    val 横坐标数组: FloatArray,
    /** 纵坐标数组。 */
    val 纵坐标数组: FloatArray,
    /** 存活标志数组。 */
    val 存活数组: BooleanArray,
    /** 有效数量。 */
    val 有效数量: Int,
    /** 分发时刻。 */
    val 分发时刻: Long,
    /** 稳定指针编号数组，与槽位数组一一对应。 */
    val 编号数组: IntArray = IntArray(0),
)

/** 分发结果：含快照与是否需要上报。 */
data class 分发结果(
    /** 本次快照。 */
    val 快照: 槽位快照,
    /** 是否发生抬起回收。 */
    val 含抬起: Boolean,
    /** 是否发生新按入。 */
    val 含按入: Boolean,
    /** 是否坐标越界被钳制。 */
    val 曾钳制: Boolean,
)

/**
 * 多指触控分发器主体。
 *
 * 线程安全说明：所有公开方法均可在主线程调用，内部用同步锁保护映射表。
 * 性能说明：映射表容量固定为十，不做堆外分配，快照复用数组拷贝。
 */
class 多指触控分发器 {
    /** 最多支持的并发触控路数。 */
    val 最多槽位数: Int = 10
    /** 显示区域宽度，用于钳制。 */
    var 显示宽度: Float = 1920f
    /** 显示区域高度，用于钳制。 */
    var 显示高度: Float = 1080f
    /** 去抖阈值，小于该距离的移动视为静止。 */
    var 去抖阈值: Float = 1.5f
    /** 快照最小发布间隔毫秒，节流重组风暴。 */
    var 最小发布间隔: Long = 32L
    /** 内部锁对象。 */
    private val 内部锁 = Any()
    /** 指针编号到槽位的映射。 */
    private val 编号到槽位 = LinkedHashMap<Int, Int>(10)
    /** 槽位到指针编号的反向映射。 */
    private val 槽位到编号 = LinkedHashMap<Int, Int>(10)
    /** 槽位占用表，O(1) 分配。 */
    private val 槽位占用表 = BooleanArray(10)
    /** 空闲槽栈，栈顶优先分配。 */
    private val 空闲槽栈 = ArrayDeque<Int>()
    /** 触控点详情表。 */
    private val 触控详情 = LinkedHashMap<Int, 触控点信息>(10)
    /** 上一帧横坐标缓存。 */
    private val 上帧横坐标 = FloatArray(10)
    /** 上一帧纵坐标缓存。 */
    private val 上帧纵坐标 = FloatArray(10)
    /** 上帧时刻缓存。 */
    private var 上帧时刻: Long = 0L
    /** 上次快照发布时刻。 */
    private var 上次发布时刻: Long = 0L
    /** 上次发布指纹，去重用。 */
    private var 上次发布指纹: Long = 0L
    /** 累计分发帧数。 */
    var 累计帧数: Long = 0L
        private set
    /** 累计按入次数。 */
    var 累计按入: Long = 0L
        private set
    /** 累计抬起次数。 */
    var 累计抬起: Long = 0L
        private set
    /** 累计钳制次数。 */
    var 累计钳制: Long = 0L
        private set
    /** 快照状态流，供界面订阅。 */
    private val 快照流内部 = MutableStateFlow<槽位快照?>(null)
    /** 对外只读快照流。 */
    val 快照流: StateFlow<槽位快照?> = 快照流内部.asStateFlow()
    /** 最近一次分发结果缓存。 */
    var 最近结果: 分发结果? = null
        private set

    init {
        重填空闲栈()
    }

    /**
     * 重填空闲栈。
     */
    private fun 重填空闲栈() {
        空闲槽栈.clear()
        for (槽 in 9 downTo 0) 空闲槽栈.addLast(槽)
    }

    /**
     * 从 MotionEvent 分发多指事件。
     *
     * 按 getPointerId 稳定映射槽位，绝不使用指针索引作为槽位。
     * 抬起标记按抬起编号定位，不直接信任动作索引越界。
     *
     * @param 事件 触控事件。
     * @param 区域宽 显示宽。
     * @param 区域高 显示高。
     * @return 分发结果。
     */
    fun 从事件分发(事件: MotionEvent, 区域宽: Float, 区域高: Float): 分发结果 {
        显示宽度 = 区域宽
        显示高度 = 区域高
        val 上限 = minOf(事件.pointerCount, 最多槽位数)
        val 编号集 = IntArray(上限) { 索引 -> 事件.getPointerId(索引) }
        val 横集 = FloatArray(上限) { 索引 -> 事件.getX(索引) }
        val 纵集 = FloatArray(上限) { 索引 -> 事件.getY(索引) }
        val 存活集 = BooleanArray(上限) { true }
        when (事件.actionMasked) {
            MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_UP -> {
                val 抬起位 = 定位抬起位置(事件, 编号集, 上限)
                if (抬起位 in 0 until 上限) 存活集[抬起位] = false
            }
            MotionEvent.ACTION_CANCEL -> 存活集.fill(false)
            MotionEvent.ACTION_POINTER_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_DOWN -> {
            }
        }
        return 分发多指事件(事件.actionMasked, 事件.actionIndex, 上限, 编号集, 横集, 纵集, 存活集)
    }

    /**
     * 按抬起编号定位其在本次数组中的位置。
     *
     * 动作索引在截断后可能越界，必须按编号回查。
     *
     * @param 事件 触控事件。
     * @param 编号集 本次编号集。
     * @param 上限 有效数。
     * @return 位置或负一。
     */
    fun 定位抬起位置(事件: MotionEvent, 编号集: IntArray, 上限: Int): Int {
        return try {
            val 动作索引 = 事件.actionIndex
            if (动作索引 < 0 || 动作索引 >= 事件.pointerCount) return -1
            val 抬起编号 = 事件.getPointerId(动作索引)
            for (i in 0 until 上限) if (编号集[i] == 抬起编号) return i
            if (动作索引 in 0 until 上限) 动作索引 else -1
        } catch (异常: Exception) {
            val 动作索引 = 事件.actionIndex
            if (动作索引 in 0 until 上限) 动作索引 else -1
        }
    }

    /**
     * 分发多指事件核心入口。
     *
     * @param 动作 动作掩码。
     * @param 动作索引 动作索引。
     * @param 数量 指针数量。
     * @param 编号集 稳定指针编号集。
     * @param 横集 横坐标集。
     * @param 纵集 纵坐标集。
     * @param 存活集 存活标志集。
     * @return 分发结果。
     */
    fun 分发多指事件(动作: Int, 动作索引: Int, 数量: Int, 编号集: IntArray, 横集: FloatArray, 纵集: FloatArray, 存活集: BooleanArray): 分发结果 {
        synchronized(内部锁) {
            val 有效数 = minOf(数量, 编号集.size, 横集.size, 纵集.size, 存活集.size, 最多槽位数)
            val 槽位数组 = IntArray(有效数)
            val 编号拷贝 = IntArray(有效数)
            val 横坐标数组 = FloatArray(有效数)
            val 纵坐标数组 = FloatArray(有效数)
            val 存活数组 = BooleanArray(有效数)
            var 含抬起 = false
            var 含按入 = false
            var 曾钳制 = false
            val 当前时刻 = System.currentTimeMillis()
            val 时间差 = if (上帧时刻 == 0L) 16L else max(1L, 当前时刻 - 上帧时刻)
            val 秒差 = 时间差 / 1000f
            for (索引 in 0 until 有效数) {
                val 编号 = 编号集[索引]
                val 原始横 = 横集[索引]
                val 原始纵 = 纵集[索引]
                val 存活 = 存活集[索引]
                val 钳制后横 = 钳制横坐标(原始横)
                val 钳制后纵 = 钳制纵坐标(原始纵)
                if (钳制后横 != 原始横 || 钳制后纵 != 原始纵) 曾钳制 = true
                val 槽位 = 为编号分配槽位(编号)
                val 安全槽 = 槽位.coerceIn(0, 最多槽位数 - 1)
                槽位数组[索引] = 安全槽
                编号拷贝[索引] = 编号
                横坐标数组[索引] = 去抖过滤(安全槽, 钳制后横, 上帧横坐标[安全槽])
                纵坐标数组[索引] = 去抖过滤(安全槽, 钳制后纵, 上帧纵坐标[安全槽])
                存活数组[索引] = 存活
                val 历史 = 触控详情[编号]
                val 位移 = if (历史 == null) 0f else hypot(钳制后横 - 历史.横坐标, 钳制后纵 - 历史.纵坐标)
                val 速度 = if (历史 == null) 0f else 位移 / 秒差
                if (历史 == null && 存活) {
                    含按入 = true
                    累计按入 += 1
                }
                if (!存活) {
                    含抬起 = true
                    累计抬起 += 1
                }
                if (存活) {
                    触控详情[编号] = 触控点信息(编号, 安全槽, 钳制后横, 钳制后纵, true, 历史?.按下时刻 ?: 当前时刻, 速度)
                } else {
                    触控详情[编号] = 触控点信息(编号, 安全槽, 钳制后横, 钳制后纵, false, 历史?.按下时刻 ?: 当前时刻, 0f)
                }
                上帧横坐标[安全槽] = 钳制后横
                上帧纵坐标[安全槽] = 钳制后纵
            }
            if (曾钳制) 累计钳制 += 1
            累计帧数 += 1
            上帧时刻 = 当前时刻
            val 快照 = 槽位快照(槽位数组, 横坐标数组, 纵坐标数组, 存活数组, 有效数, 当前时刻, 编号拷贝)
            回收抬起槽位(编号集, 存活集, 有效数)
            val 结果 = 分发结果(快照, 含抬起, 含按入, 曾钳制)
            最近结果 = 结果
            条件发布快照(快照, 当前时刻, 含抬起 || 含按入)
            return 结果
        }
    }

    /**
     * 节流发布快照，避免每帧重组。
     *
     * 有按入抬起立即发布，否则按最小间隔加指纹去重。
     *
     * @param 快照 快照。
     * @param 时刻 时刻。
     * @param 强制 是否强制发布。
     */
    private fun 条件发布快照(快照: 槽位快照, 时刻: Long, 强制: Boolean) {
        if (强制) {
            快照流内部.value = 快照
            上次发布时刻 = 时刻
            上次发布指纹 = 快照指纹(快照)
            return
        }
        if (时刻 - 上次发布时刻 < 最小发布间隔) return
        val 指纹 = 快照指纹(快照)
        if (指纹 == 上次发布指纹) return
        上次发布指纹 = 指纹
        上次发布时刻 = 时刻
        快照流内部.value = 快照
    }

    /**
     * 计算快照指纹用于去重。
     *
     * 坐标量化到像素，避免浮点抖动击穿去重。
     *
     * @param 快照 快照。
     * @return 指纹。
     */
    fun 快照指纹(快照: 槽位快照): Long {
        var 累 = 快照.有效数量.toLong() * 1000003L
        for (i in 0 until 快照.有效数量) {
            val 量化横 = (快照.横坐标数组[i] + 0.5f).toInt()
            val 量化纵 = (快照.纵坐标数组[i] + 0.5f).toInt()
            val 存活位 = if (快照.存活数组[i]) 1L else 0L
            累 = 累 * 31L + (量化横.toLong() * 73856093L xor 量化纵.toLong() * 19349663L xor 存活位 * 83492791L)
        }
        return 累
    }

    /**
     * 为编号分配槽位。
     *
     * 已存在则复用，不存在则从空闲栈弹出，满十则复用最旧。
     * 全程 O(1)，不再每帧拷贝全表。
     *
     * @param 编号 稳定指针编号。
     * @return 槽位序号。
     */
    fun 为编号分配槽位(编号: Int): Int {
        编号到槽位[编号]?.let { return it }
        val 空闲 = 空闲槽栈.removeLastOrNull()
        if (空闲 != null) {
            编号到槽位[编号] = 空闲
            槽位到编号[空闲] = 编号
            槽位占用表[空闲.coerceIn(0, 最多槽位数 - 1)] = true
            return 空闲
        }
        val 最旧 = 编号到槽位.keys.firstOrNull() ?: return 0
        val 复用槽 = 编号到槽位[最旧] ?: 0
        编号到槽位.remove(最旧)
        触控详情.remove(最旧)
        编号到槽位[编号] = 复用槽
        槽位到编号[复用槽] = 编号
        return 复用槽
    }

    /**
     * 回收抬起槽位。
     *
     * @param 编号集 编号集。
     * @param 存活集 存活集。
     * @param 有效数 有效数。
     */
    fun 回收抬起槽位(编号集: IntArray, 存活集: BooleanArray, 有效数: Int) {
        val 上限 = minOf(有效数, 编号集.size, 存活集.size)
        for (索引 in 0 until 上限) {
            if (!存活集[索引]) 回收槽位(编号集[索引])
        }
    }

    /**
     * 回收槽位。
     *
     * @param 编号 指针编号。
     */
    fun 回收槽位(编号: Int) {
        val 槽位 = 编号到槽位.remove(编号) ?: return
        if (槽位到编号[槽位] == 编号) 槽位到编号.remove(槽位)
        if (槽位 in 0 until 最多槽位数 && !槽位到编号.containsKey(槽位)) {
            槽位占用表[槽位] = false
            if (!空闲槽栈.contains(槽位)) 空闲槽栈.addLast(槽位)
        }
        触控详情.remove(编号)
    }

    /**
     * 重置全部映射与统计。
     */
    fun 重置全部() {
        synchronized(内部锁) {
            编号到槽位.clear()
            槽位到编号.clear()
            触控详情.clear()
            槽位占用表.fill(false)
            重填空闲栈()
            上帧横坐标.fill(0f)
            上帧纵坐标.fill(0f)
            上帧时刻 = 0L
            上次发布时刻 = 0L
            上次发布指纹 = 0L
            累计帧数 = 0L
            累计按入 = 0L
            累计抬起 = 0L
            累计钳制 = 0L
            最近结果 = null
            快照流内部.value = null
        }
    }

    /**
     * 钳制横坐标到显示区。
     *
     * @param 原始 原始横坐标。
     * @return 钳制后横坐标。
     */
    fun 钳制横坐标(原始: Float): Float = 原始.coerceIn(0f, 显示宽度)

    /**
     * 钳制纵坐标到显示区。
     *
     * @param 原始 原始纵坐标。
     * @return 钳制后纵坐标。
     */
    fun 钳制纵坐标(原始: Float): Float = 原始.coerceIn(0f, 显示高度)

    /**
     * 去抖过滤微小抖动。
     *
     * @param 槽位 槽位。
     * @param 当前 当前坐标。
     * @param 上帧 上帧坐标。
     * @return 过滤后坐标。
     */
    fun 去抖过滤(槽位: Int, 当前: Float, 上帧: Float): Float {
        return if (abs(当前 - 上帧) < 去抖阈值 && 上帧 != 0f) 上帧 else 当前
    }

    /**
     * 查询存活触控数。
     *
     * @return 存活数。
     */
    fun 查询存活数(): Int = synchronized(内部锁) { 触控详情.values.count { it.是否按下 } }

    /**
     * 查询槽位占用表。
     *
     * @return 槽位到编号的拷贝。
     */
    fun 查询槽位占用(): Map<Int, Int> = synchronized(内部锁) { 槽位到编号.toMap() }

    /**
     * 查询触控详情快照。
     *
     * @return 详情列表。
     */
    fun 查询触控详情(): List<触控点信息> = synchronized(内部锁) { 触控详情.values.toList() }

    /**
     * 计算两指间距。
     *
     * @param 甲 第一指。
     * @param 乙 第二指。
     * @return 间距像素。
     */
    fun 计算两指间距(甲: 触控点信息, 乙: 触控点信息): Float = hypot(甲.横坐标 - 乙.横坐标, 甲.纵坐标 - 乙.纵坐标)

    /**
     * 判断是否为静止帧。
     *
     * @param 结果 分发结果。
     * @return 是否静止。
     */
    fun 是否静止帧(结果: 分发结果): Boolean = !结果.含按入 && !结果.含抬起

    /**
     * 查询槽位状态。
     *
     * 按槽位序号直接查占用与坐标，供调试层绘制。
     * 若槽位空闲则返回空，结果含钳制后坐标与速度估计。
     * 旧编号函数族已合并为本函数加批量列表接口。
     *
     * @param 槽位序号 目标槽位，取值 0 到 9。
     * @return 触控点信息或空。
     */
    fun 查询槽位状态(槽位序号: Int): 触控点信息? {
        synchronized(内部锁) {
            if (槽位序号 !in 0 until 最多槽位数) return null
            val 编号 = 槽位到编号[槽位序号] ?: return null
            val 详情 = 触控详情[编号] ?: return null
            val 校准横 = 钳制横坐标(详情.横坐标)
            val 校准纵 = 钳制纵坐标(详情.纵坐标)
            if (校准横 == 详情.横坐标 && 校准纵 == 详情.纵坐标) return 详情
            return 详情.copy(横坐标 = 校准横, 纵坐标 = 校准纵)
        }
    }

    /**
     * 查询全部槽位状态。
     *
     * 一次加锁返回十槽全表，调试列每帧只查一次。
     *
     * @return 十槽状态表，空闲为 null。
     */
    fun 查询全部槽位状态(): List<触控点信息?> {
        synchronized(内部锁) {
            return List(最多槽位数) { 槽 ->
                val 编号 = 槽位到编号[槽] ?: return@List null
                触控详情[编号]
            }
        }
    }

    /**
     * 批量查询槽位状态。
     *
     * @param 序号表 槽位序号表。
     * @return 与序号表一一对应的状态表。
     */
    fun 批量查询槽位状态(序号表: List<Int>): List<触控点信息?> {
        synchronized(内部锁) {
            return 序号表.map { 槽 ->
                if (槽 !in 0 until 最多槽位数) null
                else {
                    val 编号 = 槽位到编号[槽] ?: return@map null
                    触控详情[编号]
                }
            }
        }
    }

    /**
     * 统计槽位流量。
     *
     * 累计帧数与按入抬起数的加权快照，用于性能面板展示。
     * 旧编号函数族已合并为本函数，不再按序号复制。
     *
     * @return 流量描述字符串。
     */
    fun 统计槽位流量(): String {
        synchronized(内部锁) {
            var 存活 = 0
            for (详情 in 触控详情.values) if (详情.是否按下) 存活 += 1
            return "槽 存活=" + 存活 + " 占用=" + 槽位到编号.size + " 帧=" + 累计帧数 + " 按入=" + 累计按入
        }
    }

    /**
     * 按槽位表批量统计流量行。
     *
     * @param 序号表 槽位序号表。
     * @return 每槽一行描述。
     */
    fun 批量统计槽位流量(序号表: List<Int>): List<String> {
        val 全表 = 查询全部槽位状态()
        return 序号表.map { 槽 ->
            val 详情 = 全表.getOrNull(槽)
            "槽" + 槽 + " 编号=" + (详情?.指针编号?.toString() ?: "空") + " 按下=" + (详情?.是否按下 ?: false)
        }
    }
}
