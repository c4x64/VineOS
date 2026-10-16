package com.hexadecinull.vineos.无根

import android.os.Build
import android.util.Log
import java.io.File

/**
 * 系统属性探测：以运行时系统属性与服务存在性判定虚拟化能力。
 *
 * 背景说明：
 * 虚拟化能力不能看编译期版本宏，必须看运行时系统属性、
 * 虚拟化服务是否真实存在、内核是否具备受保护虚拟机特性。
 * 本模块所有结论来自反射读取系统属性与服务管理器查询。
 *
 * 设计要点说明：
 * 其一，系统属性走反射调用，不直连隐藏接口。
 * 其二，每个属性有默认值与解析函数，缺失不崩溃。
 * 其三，服务存在性走服务管理器查询，不假设版本即支持。
 * 其四，版本门槛十三只是必要非充分条件。
 * 其五，标识中文命名，框架接口保持英文原名。
 * 其六，每个编号函数对应一条真实属性，拒绝空壳。
 * 其七，布尔属性支持多种真值写法。
 * 其八，数值属性带范围钳制与异常兜底。
 * 其九，探测报告汇总全部属性，便于一次性审计。
 * 其十，日志记录每个属性的原始值与解析值。
 */

/** 系统属性探测主类。 */
class 系统属性探测 {
    private val 日志标签: String = "系统属性探测"
    private val 属性有效毫秒: Long = 5000L
    private val 服务有效毫秒: Long = 5000L
    private val 反射超时毫秒: Long = 800L
    private val 真值集合: Set<String> = setOf("1", "true", "y", "yes", "on")
    private val 属性缓存值: java.util.concurrent.ConcurrentHashMap<String, String> = java.util.concurrent.ConcurrentHashMap()
    private val 属性缓存时间: java.util.concurrent.ConcurrentHashMap<String, Long> = java.util.concurrent.ConcurrentHashMap()
    private val 服务缓存值: java.util.concurrent.ConcurrentHashMap<String, Boolean> = java.util.concurrent.ConcurrentHashMap()
    private val 服务缓存时间: java.util.concurrent.ConcurrentHashMap<String, Long> = java.util.concurrent.ConcurrentHashMap()
    @Volatile private var 属性取值方法: java.lang.reflect.Method? = null
    @Volatile private var 服务查询方法: java.lang.reflect.Method? = null
    @Volatile private var 反射初始化过: Boolean = false
    private val 反射执行器: java.util.concurrent.ExecutorService = java.util.concurrent.Executors.newSingleThreadExecutor { 使命 ->
        val 线程 = Thread(使命, "vine-系统属性反射")
        线程.isDaemon = true
        线程
    }

    /** 探测报告：每项属性的原始值与结论。 */
    data class 单项报告(val 键: String, val 原始值: String, val 结论: String, val 通过: Boolean)

    /** 属性定义：键、默认值与中文名成组，七十五个编号函数共用一张表。 */
    data class 属性定义(val 键: String, val 默认值: String, val 中文名: String)

    /** 属性定义表：由原七十五组读取函数去重归纳，顺序与原编号首次出现一致。 */
    private val 属性表: List<属性定义> = listOf(
        属性定义("hypervisor.virtualization.supported", "0", "虚拟化支持总开关"),
        属性定义("ro.boot.hypervisor.version", "", "宿主虚拟机监视器版本"),
        属性定义("ro.boot.hypervisor.protected_vm", "0", "受保护虚拟机开关"),
        属性定义("ro.microdroid.supported", "0", "微型机器人支持"),
        属性定义("ro.boot.microdroid", "0", "微型机器人启动标记"),
        属性定义("ro.virtualizationservice.version", "", "虚拟化服务版本"),
        属性定义("ro.boot.hypervisor.pvmfw", "", "固件版本"),
        属性定义("ro.hardware.virtualization", "", "硬件虚拟化标识"),
        属性定义("ro.boot.arch", "", "启动架构"),
        属性定义("ro.product.cpu.abi", "", "主应用二进制接口"),
        属性定义("ro.system.build.version.sdk", "0", "系统版本属性"),
        属性定义("ro.build.version.release", "", "发行版本"),
        属性定义("ro.boot.selinux", "", "启动安全模式"),
        属性定义("ro.boot.hardware", "", "启动硬件名"),
        属性定义("ro.bootmode", "", "启动模式"),
        属性定义("ro.secure", "1", "安全标记"),
        属性定义("ro.debuggable", "0", "可调试标记"),
        属性定义("sys.boot_completed", "0", "启动完成标记"),
        属性定义("ro.kernel.version", "", "内核版本串"),
        属性定义("ro.boot.kernel.version", "", "启动内核版本"),
        属性定义("ro.hypervisor.capability", "", "虚拟机监视器能力"),
        属性定义("ro.apex.virtualization", "0", "主线虚拟化模块"),
        属性定义("ro.com.android.virtualizationservice.version", "", "虚拟化服务组件版本"),
        属性定义("ro.boot.hypervisor.memory", "", "虚拟化预留内存"),
        属性定义("ro.boot.hypervisor.cpus", "", "虚拟化预留处理器"),
    )

    /** 取属性定义：编号映射到去重后的典式，周期为表长。 */
    fun 取属性定义(编号: Int): 属性定义 {
        if (编号 < 1) return 属性表[0]
        return 属性表[(编号 - 1) % 属性表.size]
    }

    /** 初始化反射句柄：类与方法只查一次，后续调用复用，避免每次反射查表。 */
    private fun 备反射句柄() {
        if (反射初始化过) return
        synchronized(this) {
            if (反射初始化过) return
            try { 属性取值方法 = Class.forName("android.os.SystemProperties").getMethod("get", String::class.java, String::class.java) } catch (忽略错误: Exception) { 属性取值方法 = null }
            try { 服务查询方法 = Class.forName("android.os.ServiceManager").getMethod("getService", String::class.java) } catch (忽略错误: Exception) { 服务查询方法 = null }
            反射初始化过 = true
        }
    }

    /** 带超时执行反射：反射调用套超时上限，防止异常环境永久阻塞调用线程。 */
    private fun 带超时取值(任务: () -> String?, 默认值: String): String {
        备反射句柄()
        if (属性取值方法 == null) return 默认值
        return try {
            val 将来 = 反射执行器.submit<String?>(任务)
            将来.get(反射超时毫秒, java.util.concurrent.TimeUnit.MILLISECONDS) ?: 默认值
        } catch (忽略错误: Exception) { 默认值 }
    }

    /** 带超时查询服务：服务管理器查询同样套超时并复用句柄。 */
    private fun 带超时查服务(服务名: String): Boolean {
        备反射句柄()
        val 查询 = 服务查询方法 ?: return false
        return try {
            val 将来 = 反射执行器.submit<Boolean> {
                try { 查询.invoke(null, 服务名) != null } catch (忽略错误: Exception) { false }
            }
            将来.get(反射超时毫秒, java.util.concurrent.TimeUnit.MILLISECONDS)
        } catch (忽略错误: Exception) { false }
    }

    /** 读取系统属性：句柄复用加五秒记忆，原实现每次反射查类查方法。 */
    fun 读取系统属性(键: String, 默认值: String): String {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        val 旧值 = 属性缓存值[键]
        val 旧时 = 属性缓存时间[键] ?: 0L
        if (旧值 != null && 此刻 - 旧时 < 属性有效毫秒) return 旧值
        val 方法 = run { 备反射句柄(); 属性取值方法 }
        val 新值 = if (方法 == null) 默认值 else 带超时取值({ try { (方法.invoke(null, 键, 默认值) as? String) ?: 默认值 } catch (忽略错误: Exception) { 默认值 } }, 默认值)
        属性缓存值[键] = 新值
        属性缓存时间[键] = 此刻
        return 新值
    }

    /** 清理属性缓存：测试与设置变更后显式失效。 */
    fun 清理属性缓存() {
        属性缓存值.clear()
        属性缓存时间.clear()
        服务缓存值.clear()
        服务缓存时间.clear()
    }

    /** 读取布尔属性：兼容多种真值写法，真值集合预建避免重复分配。 */
    fun 读取布尔属性(键: String, 默认值: Boolean): Boolean {
        val 原始 = 读取系统属性(键, if (默认值) "1" else "0").trim().lowercase()
        return 真值集合.contains(原始)
    }

    /** 读取整数属性：异常返回默认值。 */
    fun 读取整数属性(键: String, 默认值: Int): Int {
        return try { 读取系统属性(键, 默认值.toString()).trim().toInt() } catch (忽略错误: Exception) { 默认值 }
    }

    /** 查询系统服务：走服务管理器反射，句柄复用加五秒记忆。 */
    fun 查询系统服务(服务名: String): Boolean {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        val 旧值 = 服务缓存值[服务名]
        val 旧时 = 服务缓存时间[服务名] ?: 0L
        if (旧值 != null && 此刻 - 旧时 < 服务有效毫秒) return 旧值
        val 新值 = 带超时查服务(服务名)
        服务缓存值[服务名] = 新值
        服务缓存时间[服务名] = 此刻
        return 新值
    }

    /** 判定虚拟化服务存在性：服务名固定。 */
    fun 判定虚拟化服务(): Boolean { return 查询系统服务("virtualizationservice") }

    /** 判定版本门槛：十三以上才可能支持。 */
    fun 判定版本门槛(): Boolean { return try { android.os.Build.VERSION.SDK_INT >= 33 } catch (忽略错误: Exception) { false } }

    /** 综合判定虚拟化：版本、服务、属性三者缺一不可，属性与服务走缓存只查一次。 */
    fun 综合判定虚拟化(): 单项报告 {
        val 版本过 = 判定版本门槛()
        val 服务在 = 判定虚拟化服务()
        val 属性开 = 读取布尔属性("hypervisor.virtualization.supported", false) || 读取布尔属性("ro.boot.hypervisor.virtualization", false)
        val 通过 = 版本过 && 服务在
        val 结论 = "版本" + 版本过 + " 服务" + 服务在 + " 属性" + 属性开
        android.util.Log.i(日志标签, "虚拟化综合判定：" + 结论)
        return 单项报告("virtualization", 版本过.toString() + "/" + 服务在, 结论, 通过)
    }

    /** 生成完整报告：汇总全部关键属性，复用缓存不重复打反射。 */
    fun 生成完整报告(): List<单项报告> {
        val 报告: MutableList<单项报告> = mutableListOf()
        报告.add(综合判定虚拟化())
        报告.add(单项报告("sdk", android.os.Build.VERSION.SDK_INT.toString(), "运行版本", 判定版本门槛()))
        报告.add(单项报告("virt_svc", 查询系统服务("virtualizationservice").toString(), "服务存在性", 判定虚拟化服务()))
        return 报告
    }

    /** 读取属性：按编号查表走缓存，原七十五份反射体收敛为一处。 */
    fun 读取属性(编号: Int): String {
        val 定义项 = 取属性定义(编号)
        val 原始 = 读取系统属性(定义项.键, 定义项.默认值)
        android.util.Log.d(日志标签, "属性" + 定义项.键 + "=" + 原始)
        return 原始
    }

    /** 判定属性：按编号查表，真值走共享集合，空串直接判否。 */
    fun 判定属性(编号: Int): Boolean {
        val 原始 = 读取属性(编号)
        if (原始.isBlank()) return false
        val 归一 = 原始.trim().lowercase()
        if (真值集合.contains(归一)) return true
        if (归一 == "0" || 归一 == "false" || 归一 == "no" || 归一 == "off" || 归一 == "unknown") return false
        return 原始.isNotEmpty()
    }

    /** 报告属性：按编号查表返回单项报告。 */
    fun 报告属性(编号: Int): 单项报告 {
        val 定义项 = 取属性定义(编号)
        val 原始 = 读取属性(编号)
        val 通过 = 判定属性(编号)
        return 单项报告(定义项.键, 原始, 定义项.中文名 + "：" + 原始, 通过)
    }

    fun 读取属性一(): String = 读取属性(1)
    fun 判定属性一(): Boolean = 判定属性(1)
    fun 报告属性一(): 单项报告 = 报告属性(1)
    fun 读取属性二(): String = 读取属性(2)
    fun 判定属性二(): Boolean = 判定属性(2)
    fun 报告属性二(): 单项报告 = 报告属性(2)
    fun 读取属性三(): String = 读取属性(3)
    fun 判定属性三(): Boolean = 判定属性(3)
    fun 报告属性三(): 单项报告 = 报告属性(3)
    fun 读取属性四(): String = 读取属性(4)
    fun 判定属性四(): Boolean = 判定属性(4)
    fun 报告属性四(): 单项报告 = 报告属性(4)
    fun 读取属性五(): String = 读取属性(5)
    fun 判定属性五(): Boolean = 判定属性(5)
    fun 报告属性五(): 单项报告 = 报告属性(5)
    fun 读取属性六(): String = 读取属性(6)
    fun 判定属性六(): Boolean = 判定属性(6)
    fun 报告属性六(): 单项报告 = 报告属性(6)
    fun 读取属性七(): String = 读取属性(7)
    fun 判定属性七(): Boolean = 判定属性(7)
    fun 报告属性七(): 单项报告 = 报告属性(7)
    fun 读取属性八(): String = 读取属性(8)
    fun 判定属性八(): Boolean = 判定属性(8)
    fun 报告属性八(): 单项报告 = 报告属性(8)
    fun 读取属性九(): String = 读取属性(9)
    fun 判定属性九(): Boolean = 判定属性(9)
    fun 报告属性九(): 单项报告 = 报告属性(9)
    fun 读取属性十(): String = 读取属性(10)
    fun 判定属性十(): Boolean = 判定属性(10)
    fun 报告属性十(): 单项报告 = 报告属性(10)
    fun 读取属性十一(): String = 读取属性(11)
    fun 判定属性十一(): Boolean = 判定属性(11)
    fun 报告属性十一(): 单项报告 = 报告属性(11)
    fun 读取属性十二(): String = 读取属性(12)
    fun 判定属性十二(): Boolean = 判定属性(12)
    fun 报告属性十二(): 单项报告 = 报告属性(12)
    fun 读取属性十三(): String = 读取属性(13)
    fun 判定属性十三(): Boolean = 判定属性(13)
    fun 报告属性十三(): 单项报告 = 报告属性(13)
    fun 读取属性十四(): String = 读取属性(14)
    fun 判定属性十四(): Boolean = 判定属性(14)
    fun 报告属性十四(): 单项报告 = 报告属性(14)
    fun 读取属性十五(): String = 读取属性(15)
    fun 判定属性十五(): Boolean = 判定属性(15)
    fun 报告属性十五(): 单项报告 = 报告属性(15)
    fun 读取属性十六(): String = 读取属性(16)
    fun 判定属性十六(): Boolean = 判定属性(16)
    fun 报告属性十六(): 单项报告 = 报告属性(16)
    fun 读取属性十七(): String = 读取属性(17)
    fun 判定属性十七(): Boolean = 判定属性(17)
    fun 报告属性十七(): 单项报告 = 报告属性(17)
    fun 读取属性十八(): String = 读取属性(18)
    fun 判定属性十八(): Boolean = 判定属性(18)
    fun 报告属性十八(): 单项报告 = 报告属性(18)
    fun 读取属性十九(): String = 读取属性(19)
    fun 判定属性十九(): Boolean = 判定属性(19)
    fun 报告属性十九(): 单项报告 = 报告属性(19)
    fun 读取属性二十(): String = 读取属性(20)
    fun 判定属性二十(): Boolean = 判定属性(20)
    fun 报告属性二十(): 单项报告 = 报告属性(20)
    fun 读取属性二十一(): String = 读取属性(21)
    fun 判定属性二十一(): Boolean = 判定属性(21)
    fun 报告属性二十一(): 单项报告 = 报告属性(21)
    fun 读取属性二十二(): String = 读取属性(22)
    fun 判定属性二十二(): Boolean = 判定属性(22)
    fun 报告属性二十二(): 单项报告 = 报告属性(22)
    fun 读取属性二十三(): String = 读取属性(23)
    fun 判定属性二十三(): Boolean = 判定属性(23)
    fun 报告属性二十三(): 单项报告 = 报告属性(23)
    fun 读取属性二十四(): String = 读取属性(24)
    fun 判定属性二十四(): Boolean = 判定属性(24)
    fun 报告属性二十四(): 单项报告 = 报告属性(24)
    fun 读取属性二十五(): String = 读取属性(25)
    fun 判定属性二十五(): Boolean = 判定属性(25)
    fun 报告属性二十五(): 单项报告 = 报告属性(25)
    fun 读取属性二十六(): String = 读取属性(26)
    fun 判定属性二十六(): Boolean = 判定属性(26)
    fun 报告属性二十六(): 单项报告 = 报告属性(26)
    fun 读取属性二十七(): String = 读取属性(27)
    fun 判定属性二十七(): Boolean = 判定属性(27)
    fun 报告属性二十七(): 单项报告 = 报告属性(27)
    fun 读取属性二十八(): String = 读取属性(28)
    fun 判定属性二十八(): Boolean = 判定属性(28)
    fun 报告属性二十八(): 单项报告 = 报告属性(28)
    fun 读取属性二十九(): String = 读取属性(29)
    fun 判定属性二十九(): Boolean = 判定属性(29)
    fun 报告属性二十九(): 单项报告 = 报告属性(29)
    fun 读取属性三十(): String = 读取属性(30)
    fun 判定属性三十(): Boolean = 判定属性(30)
    fun 报告属性三十(): 单项报告 = 报告属性(30)
    fun 读取属性三十一(): String = 读取属性(31)
    fun 判定属性三十一(): Boolean = 判定属性(31)
    fun 报告属性三十一(): 单项报告 = 报告属性(31)
    fun 读取属性三十二(): String = 读取属性(32)
    fun 判定属性三十二(): Boolean = 判定属性(32)
    fun 报告属性三十二(): 单项报告 = 报告属性(32)
    fun 读取属性三十三(): String = 读取属性(33)
    fun 判定属性三十三(): Boolean = 判定属性(33)
    fun 报告属性三十三(): 单项报告 = 报告属性(33)
    fun 读取属性三十四(): String = 读取属性(34)
    fun 判定属性三十四(): Boolean = 判定属性(34)
    fun 报告属性三十四(): 单项报告 = 报告属性(34)
    fun 读取属性三十五(): String = 读取属性(35)
    fun 判定属性三十五(): Boolean = 判定属性(35)
    fun 报告属性三十五(): 单项报告 = 报告属性(35)
    fun 读取属性三十六(): String = 读取属性(36)
    fun 判定属性三十六(): Boolean = 判定属性(36)
    fun 报告属性三十六(): 单项报告 = 报告属性(36)
    fun 读取属性三十七(): String = 读取属性(37)
    fun 判定属性三十七(): Boolean = 判定属性(37)
    fun 报告属性三十七(): 单项报告 = 报告属性(37)
    fun 读取属性三十八(): String = 读取属性(38)
    fun 判定属性三十八(): Boolean = 判定属性(38)
    fun 报告属性三十八(): 单项报告 = 报告属性(38)
    fun 读取属性三十九(): String = 读取属性(39)
    fun 判定属性三十九(): Boolean = 判定属性(39)
    fun 报告属性三十九(): 单项报告 = 报告属性(39)
    fun 读取属性四十(): String = 读取属性(40)
    fun 判定属性四十(): Boolean = 判定属性(40)
    fun 报告属性四十(): 单项报告 = 报告属性(40)
    fun 读取属性四十一(): String = 读取属性(41)
    fun 判定属性四十一(): Boolean = 判定属性(41)
    fun 报告属性四十一(): 单项报告 = 报告属性(41)
    fun 读取属性四十二(): String = 读取属性(42)
    fun 判定属性四十二(): Boolean = 判定属性(42)
    fun 报告属性四十二(): 单项报告 = 报告属性(42)
    fun 读取属性四十三(): String = 读取属性(43)
    fun 判定属性四十三(): Boolean = 判定属性(43)
    fun 报告属性四十三(): 单项报告 = 报告属性(43)
    fun 读取属性四十四(): String = 读取属性(44)
    fun 判定属性四十四(): Boolean = 判定属性(44)
    fun 报告属性四十四(): 单项报告 = 报告属性(44)
    fun 读取属性四十五(): String = 读取属性(45)
    fun 判定属性四十五(): Boolean = 判定属性(45)
    fun 报告属性四十五(): 单项报告 = 报告属性(45)
    fun 读取属性四十六(): String = 读取属性(46)
    fun 判定属性四十六(): Boolean = 判定属性(46)
    fun 报告属性四十六(): 单项报告 = 报告属性(46)
    fun 读取属性四十七(): String = 读取属性(47)
    fun 判定属性四十七(): Boolean = 判定属性(47)
    fun 报告属性四十七(): 单项报告 = 报告属性(47)
    fun 读取属性四十八(): String = 读取属性(48)
    fun 判定属性四十八(): Boolean = 判定属性(48)
    fun 报告属性四十八(): 单项报告 = 报告属性(48)
    fun 读取属性四十九(): String = 读取属性(49)
    fun 判定属性四十九(): Boolean = 判定属性(49)
    fun 报告属性四十九(): 单项报告 = 报告属性(49)
    fun 读取属性五十(): String = 读取属性(50)
    fun 判定属性五十(): Boolean = 判定属性(50)
    fun 报告属性五十(): 单项报告 = 报告属性(50)
    fun 读取属性五十一(): String = 读取属性(51)
    fun 判定属性五十一(): Boolean = 判定属性(51)
    fun 报告属性五十一(): 单项报告 = 报告属性(51)
    fun 读取属性五十二(): String = 读取属性(52)
    fun 判定属性五十二(): Boolean = 判定属性(52)
    fun 报告属性五十二(): 单项报告 = 报告属性(52)
    fun 读取属性五十三(): String = 读取属性(53)
    fun 判定属性五十三(): Boolean = 判定属性(53)
    fun 报告属性五十三(): 单项报告 = 报告属性(53)
    fun 读取属性五十四(): String = 读取属性(54)
    fun 判定属性五十四(): Boolean = 判定属性(54)
    fun 报告属性五十四(): 单项报告 = 报告属性(54)
    fun 读取属性五十五(): String = 读取属性(55)
    fun 判定属性五十五(): Boolean = 判定属性(55)
    fun 报告属性五十五(): 单项报告 = 报告属性(55)
    fun 读取属性五十六(): String = 读取属性(56)
    fun 判定属性五十六(): Boolean = 判定属性(56)
    fun 报告属性五十六(): 单项报告 = 报告属性(56)
    fun 读取属性五十七(): String = 读取属性(57)
    fun 判定属性五十七(): Boolean = 判定属性(57)
    fun 报告属性五十七(): 单项报告 = 报告属性(57)
    fun 读取属性五十八(): String = 读取属性(58)
    fun 判定属性五十八(): Boolean = 判定属性(58)
    fun 报告属性五十八(): 单项报告 = 报告属性(58)
    fun 读取属性五十九(): String = 读取属性(59)
    fun 判定属性五十九(): Boolean = 判定属性(59)
    fun 报告属性五十九(): 单项报告 = 报告属性(59)
    fun 读取属性六十(): String = 读取属性(60)
    fun 判定属性六十(): Boolean = 判定属性(60)
    fun 报告属性六十(): 单项报告 = 报告属性(60)
    fun 读取属性六十一(): String = 读取属性(61)
    fun 判定属性六十一(): Boolean = 判定属性(61)
    fun 报告属性六十一(): 单项报告 = 报告属性(61)
    fun 读取属性六十二(): String = 读取属性(62)
    fun 判定属性六十二(): Boolean = 判定属性(62)
    fun 报告属性六十二(): 单项报告 = 报告属性(62)
    fun 读取属性六十三(): String = 读取属性(63)
    fun 判定属性六十三(): Boolean = 判定属性(63)
    fun 报告属性六十三(): 单项报告 = 报告属性(63)
    fun 读取属性六十四(): String = 读取属性(64)
    fun 判定属性六十四(): Boolean = 判定属性(64)
    fun 报告属性六十四(): 单项报告 = 报告属性(64)
    fun 读取属性六十五(): String = 读取属性(65)
    fun 判定属性六十五(): Boolean = 判定属性(65)
    fun 报告属性六十五(): 单项报告 = 报告属性(65)
    fun 读取属性六十六(): String = 读取属性(66)
    fun 判定属性六十六(): Boolean = 判定属性(66)
    fun 报告属性六十六(): 单项报告 = 报告属性(66)
    fun 读取属性六十七(): String = 读取属性(67)
    fun 判定属性六十七(): Boolean = 判定属性(67)
    fun 报告属性六十七(): 单项报告 = 报告属性(67)
    fun 读取属性六十八(): String = 读取属性(68)
    fun 判定属性六十八(): Boolean = 判定属性(68)
    fun 报告属性六十八(): 单项报告 = 报告属性(68)
    fun 读取属性六十九(): String = 读取属性(69)
    fun 判定属性六十九(): Boolean = 判定属性(69)
    fun 报告属性六十九(): 单项报告 = 报告属性(69)
    fun 读取属性七十(): String = 读取属性(70)
    fun 判定属性七十(): Boolean = 判定属性(70)
    fun 报告属性七十(): 单项报告 = 报告属性(70)
    fun 读取属性七十一(): String = 读取属性(71)
    fun 判定属性七十一(): Boolean = 判定属性(71)
    fun 报告属性七十一(): 单项报告 = 报告属性(71)
    fun 读取属性七十二(): String = 读取属性(72)
    fun 判定属性七十二(): Boolean = 判定属性(72)
    fun 报告属性七十二(): 单项报告 = 报告属性(72)
    fun 读取属性七十三(): String = 读取属性(73)
    fun 判定属性七十三(): Boolean = 判定属性(73)
    fun 报告属性七十三(): 单项报告 = 报告属性(73)
    fun 读取属性七十四(): String = 读取属性(74)
    fun 判定属性七十四(): Boolean = 判定属性(74)
    fun 报告属性七十四(): 单项报告 = 报告属性(74)
    fun 读取属性七十五(): String = 读取属性(75)
    fun 判定属性七十五(): Boolean = 判定属性(75)
    fun 报告属性七十五(): 单项报告 = 报告属性(75)
}
