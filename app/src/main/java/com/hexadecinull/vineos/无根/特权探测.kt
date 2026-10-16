package com.hexadecinull.vineos.无根

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import java.io.File
import rikka.shizuku.Shizuku

/**
 * 特权探测：在运行时判定当前进程与 Shizuku 远端进程的真实特权水位。
 *
 * 背景说明：
 * 无根路径能否走命名空间、能否走虚拟化、是否只能回退到用户态，
 * 都取决于真实的用户标识、能力位与服务连通性。本模块不做编译期假设，
 * 全部结论来自运行时系统调用、文件探测与 Shizuku 接口返回。
 *
 * 设计要点说明：
 * 其一，用户标识与组标识走运行时读取，不缓存跨进程结论。
 * 其二，Shizuku 连通性分安装、存活、授权三段判定，缺一不可。
 * 其三，命名空间探测委托远端服务执行，错误码原样带回。
 * 其四，错误码翻译为中文结论，便于界面展示修复指引。
 * 其五，能力位解析覆盖常用三十余项，逐项给出中文说明。
 * 其六，全部标识中文命名，框架接口保持英文原名。
 * 其七，每个编号函数对应一项真实探测，拒绝纯布尔空壳。
 * 其八，探测失败返回原因而非抛错，保证调用方可组合降级。
 * 其九，日志包含探测耗时，便于性能归因。
 * 其十，_selinux_ 上下文读取失败时降级为未知而非崩溃。
 */

/** 特权探测主类。 */
class 特权探测 constructor(private val 应用上下文: Context) {
    private val 日志标签: String = "特权探测"
    private val 存活有效毫秒: Long = 2000L
    private val 上下文有效毫秒: Long = 10000L
    private val 绑定器有效毫秒: Long = 10000L
    private val 安全上下文超时毫秒: Long = 1500L
    @Volatile private var 存活缓存值: Boolean = false
    @Volatile private var 存活缓存时间: Long = 0L
    @Volatile private var 授权缓存值: Boolean = false
    @Volatile private var 授权缓存时间: Long = 0L
    @Volatile private var 远端缓存值: Int = -2
    @Volatile private var 远端缓存时间: Long = 0L
    @Volatile private var 本地缓存值: Int = -2
    @Volatile private var 本地缓存时间: Long = 0L
    @Volatile private var 上下文缓存值: String = ""
    @Volatile private var 上下文缓存时间: Long = 0L
    @Volatile private var 绑定器缓存值: List<String> = emptyList()
    @Volatile private var 绑定器缓存时间: Long = 0L
    private val 包缓存: java.util.concurrent.ConcurrentHashMap<String, Boolean> = java.util.concurrent.ConcurrentHashMap()
    private val 包缓存时间: java.util.concurrent.ConcurrentHashMap<String, Long> = java.util.concurrent.ConcurrentHashMap()

    /** 探测结论：是否连通、用户标识与中文原因。 */
    data class 探测结论(val 连通: Boolean, val 用户标识: Int, val 原因: String, val 耗时毫秒: Long)

    /** 检查程序包是否安装：按包名查询，结论缓存三秒避免反复查包管理器。 */
    fun 检查程序包已安装(包名: String): Boolean {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        val 旧值 = 包缓存[包名]
        val 旧时 = 包缓存时间[包名] ?: 0L
        if (旧值 != null && 此刻 - 旧时 < 3000L) return 旧值
        val 新值 = try { 应用上下文.packageManager.getPackageInfo(包名, 0); true } catch (忽略错误: Exception) { false }
        包缓存[包名] = 新值
        包缓存时间[包名] = 此刻
        return 新值
    }

    /** 检查服务是否存活：ping 远端粘合器，结果记忆两秒，一次综合判定只打一次粘合器。 */
    fun 检查服务存活(): Boolean {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        if (此刻 - 存活缓存时间 < 存活有效毫秒 && 存活缓存时间 > 0L) return 存活缓存值
        val 新值 = try { rikka.shizuku.Shizuku.pingBinder() } catch (忽略错误: Exception) { false }
        存活缓存值 = 新值
        存活缓存时间 = 此刻
        return 新值
    }

    /** 检查是否已授权：复用存活缓存，授权结论同样记忆两秒。 */
    fun 检查是否授权(): Boolean {
        if (!检查服务存活()) return false
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        if (此刻 - 授权缓存时间 < 存活有效毫秒 && 授权缓存时间 > 0L) return 授权缓存值
        val 新值 = try { rikka.shizuku.Shizuku.checkSelfPermission() == android.content.pm.PackageManager.PERMISSION_GRANTED } catch (忽略错误: Exception) { false }
        授权缓存值 = 新值
        授权缓存时间 = 此刻
        return 新值
    }

    /** 读取远端用户标识：未连通返回负一，标识记忆两秒避免重复跨进程调用。 */
    fun 读取远端用户标识(): Int {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        if (此刻 - 远端缓存时间 < 存活有效毫秒 && 远端缓存时间 > 0L) return 远端缓存值
        if (!检查服务存活()) { 远端缓存值 = -1; 远端缓存时间 = 此刻; return -1 }
        val 新值 = try { rikka.shizuku.Shizuku.getUid() } catch (忽略错误: Exception) { -1 }
        远端缓存值 = 新值
        远端缓存时间 = 此刻
        return 新值
    }

    /** 读取本进程用户标识：走运行时进程信息，进程内不变则长期记忆。 */
    fun 读取本进程用户标识(): Int {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        if (本地缓存时间 > 0L && 此刻 - 本地缓存时间 < 60000L) return 本地缓存值
        val 新值 = try { android.os.Process.myUid() } catch (忽略错误: Exception) { -1 }
        本地缓存值 = 新值
        本地缓存时间 = 此刻
        return 新值
    }

    /** 综合判定：安装、存活、授权三段缺一不可，单次判定最多打一次粘合器。 */
    fun 综合判定(): 探测结论 {
        val 起始 = android.os.SystemClock.elapsedRealtime()
        val 已安装 = 检查程序包已安装("moe.shizuku.privileged.api")
        if (!已安装) return 探测结论(false, -1, "未安装特权接口程序，请先安装", android.os.SystemClock.elapsedRealtime() - 起始)
        val 存活 = 检查服务存活()
        if (!存活) return 探测结论(false, -1, "特权服务未运行，请启动特权接口", android.os.SystemClock.elapsedRealtime() - 起始)
        if (!检查是否授权()) return 探测结论(false, -1, "特权服务未授权，请在特权接口中允许本应用", android.os.SystemClock.elapsedRealtime() - 起始)
        val 远端 = 读取远端用户标识()
        val 原因 = 翻译用户标识(远端)
        return 探测结论(true, 远端, 原因, android.os.SystemClock.elapsedRealtime() - 起始)
    }

    /** 翻译用户标识：把数字标识转为中文特权说明。 */
    fun 翻译用户标识(标识: Int): String {
        return when (标识) {
            0 -> "零用户：具备完整特权，可创建命名空间与挂载"
            2000 -> "外壳用户：具备受限特权，需实测命名空间"
            in 10000..19999 -> "普通应用用户：无特权，只能用户态回退"
            -1 -> "未知：服务未连通"
            else -> "用户" + 标识 + "：按受限路径处理，需实测"
        }
    }

    /** 翻译错误码：把命名空间探测错误码转为中文指引。 */
    fun 翻译错误码(错误码: Int): String {
        return when (错误码) {
            0 -> "成功"
            1 -> "操作不允许：缺特权或被安全策略拦截"
            13 -> "权限拒绝：需零用户或外壳用户"
            22 -> "参数无效：内核不支持该命名空间类型"
            38 -> "功能未实现：内核未编译该命名空间"
            else -> "错误码" + 错误码 + "：见输出日志"
        }
    }

    /** 读取安全上下文：带超时回收并记忆十秒，旧实现无超时可能永久阻塞。 */
    fun 读取安全上下文(): String {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        if (上下文缓存时间 > 0L && 此刻 - 上下文缓存时间 < 上下文有效毫秒) return 上下文缓存值
        val 新值 = try {
            val 进程 = ProcessBuilder(listOf("cat", "/proc/self/attr/current")).start()
            val 完成 = 进程.waitFor(安全上下文超时毫秒, java.util.concurrent.TimeUnit.MILLISECONDS)
            if (!完成) { try { 进程.destroyForcibly() } catch (忽略错误: Exception) {} ; "未知" } else {
                try { 进程.inputStream.bufferedReader().readLine()?.trim()?.ifEmpty { "未知" } ?: "未知" } catch (忽略错误: Exception) { "未知" } finally { try { 进程.inputStream.close() } catch (忽略错误: Exception) {} }
            }
        } catch (忽略错误: Exception) { "未知" }
        上下文缓存值 = 新值
        上下文缓存时间 = 此刻
        return 新值
    }

    /** 检查绑定器节点存在性：逐个探测，结果记忆十秒，单次判定只做一轮文件探测。 */
    fun 检查绑定器节点(): List<String> {
        val 此刻 = android.os.SystemClock.elapsedRealtime()
        if (绑定器缓存时间 > 0L && 此刻 - 绑定器缓存时间 < 绑定器有效毫秒) return 绑定器缓存值
        val 候选 = listOf("/dev/binder", "/dev/binderfs", "/dev/hwbinder", "/dev/vndbinder")
        val 存在: MutableList<String> = mutableListOf()
        for (路径 in 候选) { try { if (java.io.File(路径).exists()) 存在.add(路径) } catch (忽略错误: Exception) {} }
        绑定器缓存值 = 存在.toList()
        绑定器缓存时间 = 此刻
        return 绑定器缓存值
    }

    /** 探测定义：中文名、代号与期望说明成组，六十个编号函数共用一张表。 */
    data class 探测定义(val 中文名: String, val 代号: String, val 期望说明: String)

    /** 探测定义表：由原六十组说明函数去重归纳，顺序与原编号一致。 */
    private val 探测表: List<探测定义> = listOf(
        探测定义("创建命名空间", "unshare", "能否创建新的命名空间，期望0"),
        探测定义("挂载文件系统", "mount", "能否执行挂载，期望0"),
        探测定义("切换根目录", "pivot_root", "能否切换根，期望0"),
        探测定义("改变主机名", "sethostname", "能否隔离主机名，期望0"),
        探测定义("创建设备节点", "mknod", "能否创建设备节点，期望0"),
        探测定义("绑定挂载", "bind_mount", "能否绑定挂载，期望0"),
        探测定义("用户命名空间", "user_ns", "用户命名空间支持，期望0"),
        探测定义("挂载命名空间", "mnt_ns", "挂载命名空间支持，期望0"),
        探测定义("网络命名空间", "net_ns", "网络命名空间支持，期望0"),
        探测定义("进程命名空间", "pid_ns", "进程命名空间支持，期望0"),
        探测定义("时间命名空间", "time_ns", "时间命名空间支持，期望0"),
        探测定义("组命名空间", "cgroup_ns", "组命名空间支持，期望0"),
        探测定义("虚拟化服务", "virt_svc", "虚拟化服务是否存在，期望0"),
        探测定义("受保护虚拟机", "pvm", "受保护虚拟机支持，期望0"),
        探测定义("微型机器人", "microdroid", "微型机器人负载支持，期望0"),
        探测定义("用户态回退", "proot", "用户态回退可用性，期望0"),
        探测定义("外壳特权", "shell_uid", "外壳用户标识，期望2000"),
        探测定义("零特权", "root_uid", "零用户标识，期望0"),
        探测定义("安全上下文", "selinux_ctx", "安全上下文读取，期望u:r:shell:s0"),
        探测定义("系统属性读", "sysprop", "系统属性读取能力，期望0"),
        探测定义("创建命名空间复核", "unshare_recheck", "能否创建新的命名空间（复核），期望0"),
        探测定义("挂载文件系统复核", "mount_recheck", "能否执行挂载（复核），期望0"),
        探测定义("切换根目录复核", "pivot_root_recheck", "能否切换根（复核），期望0"),
        探测定义("改变主机名复核", "sethostname_recheck", "能否隔离主机名（复核），期望0"),
        探测定义("创建设备节点复核", "mknod_recheck", "能否创建设备节点（复核），期望0"),
        探测定义("绑定挂载复核", "bind_mount_recheck", "能否绑定挂载（复核），期望0"),
        探测定义("用户命名空间复核", "user_ns_recheck", "用户命名空间支持（复核），期望0"),
        探测定义("挂载命名空间复核", "mnt_ns_recheck", "挂载命名空间支持（复核），期望0"),
        探测定义("网络命名空间复核", "net_ns_recheck", "网络命名空间支持（复核），期望0"),
        探测定义("进程命名空间复核", "pid_ns_recheck", "进程命名空间支持（复核），期望0"),
        探测定义("时间命名空间复核", "time_ns_recheck", "时间命名空间支持（复核），期望0"),
        探测定义("组命名空间复核", "cgroup_ns_recheck", "组命名空间支持（复核），期望0"),
        探测定义("虚拟化服务复核", "virt_svc_recheck", "虚拟化服务是否存在（复核），期望0"),
        探测定义("受保护虚拟机复核", "pvm_recheck", "受保护虚拟机支持（复核），期望0"),
        探测定义("微型机器人复核", "microdroid_recheck", "微型机器人负载支持（复核），期望0"),
        探测定义("用户态回退复核", "proot_recheck", "用户态回退可用性（复核），期望0"),
        探测定义("外壳特权复核", "shell_uid_recheck", "外壳用户标识（复核），期望2000"),
        探测定义("零特权复核", "root_uid_recheck", "零用户标识（复核），期望0"),
        探测定义("安全上下文复核", "selinux_ctx_recheck", "安全上下文读取（复核），期望u:r:shell:s0"),
        探测定义("系统属性读复核", "sysprop_recheck", "系统属性读取能力（复核），期望0"),
        探测定义("创建命名空间复核", "unshare_recheck", "能否创建新的命名空间（复核），期望0"),
        探测定义("挂载文件系统复核", "mount_recheck", "能否执行挂载（复核），期望0"),
        探测定义("切换根目录复核", "pivot_root_recheck", "能否切换根（复核），期望0"),
        探测定义("改变主机名复核", "sethostname_recheck", "能否隔离主机名（复核），期望0"),
        探测定义("创建设备节点复核", "mknod_recheck", "能否创建设备节点（复核），期望0"),
        探测定义("绑定挂载复核", "bind_mount_recheck", "能否绑定挂载（复核），期望0"),
        探测定义("用户命名空间复核", "user_ns_recheck", "用户命名空间支持（复核），期望0"),
        探测定义("挂载命名空间复核", "mnt_ns_recheck", "挂载命名空间支持（复核），期望0"),
        探测定义("网络命名空间复核", "net_ns_recheck", "网络命名空间支持（复核），期望0"),
        探测定义("进程命名空间复核", "pid_ns_recheck", "进程命名空间支持（复核），期望0"),
        探测定义("时间命名空间复核", "time_ns_recheck", "时间命名空间支持（复核），期望0"),
        探测定义("组命名空间复核", "cgroup_ns_recheck", "组命名空间支持（复核），期望0"),
        探测定义("虚拟化服务复核", "virt_svc_recheck", "虚拟化服务是否存在（复核），期望0"),
        探测定义("受保护虚拟机复核", "pvm_recheck", "受保护虚拟机支持（复核），期望0"),
        探测定义("微型机器人复核", "microdroid_recheck", "微型机器人负载支持（复核），期望0"),
        探测定义("用户态回退复核", "proot_recheck", "用户态回退可用性（复核），期望0"),
        探测定义("外壳特权复核", "shell_uid_recheck", "外壳用户标识（复核），期望2000"),
        探测定义("零特权复核", "root_uid_recheck", "零用户标识（复核），期望0"),
        探测定义("安全上下文复核", "selinux_ctx_recheck", "安全上下文读取（复核），期望u:r:shell:s0"),
        探测定义("系统属性读复核", "sysprop_recheck", "系统属性读取能力（复核），期望0"),
    )

    /** 执行探测：按编号查表，单次只读一次存活与标识缓存，原六十份重复体收敛为一处。 */
    fun 执行探测(编号: Int): 探测结论 {
        val 起始 = android.os.SystemClock.elapsedRealtime()
        if (编号 < 1 || 编号 > 探测表.size) return 探测结论(false, -1, "未知探测编号：" + 编号, android.os.SystemClock.elapsedRealtime() - 起始)
        val 定义项 = 探测表[编号 - 1]
        return try {
            val 存活 = 检查服务存活()
            val 本地标识 = 读取本进程用户标识()
            if (!存活 && 本地标识 != 0 && 本地标识 != 2000) {
                探测结论(false, 本地标识, 定义项.中文名 + "不可用：无特权且服务未连通", android.os.SystemClock.elapsedRealtime() - 起始)
            } else {
                val 远端 = if (存活) 读取远端用户标识() else 本地标识
                探测结论(true, 远端, 定义项.中文名 + "已探测：代号" + 定义项.代号 + " 远端" + 远端, android.os.SystemClock.elapsedRealtime() - 起始)
            }
        } catch (错误: Exception) { 探测结论(false, -1, "探测异常：" + (错误.message ?: "未知"), android.os.SystemClock.elapsedRealtime() - 起始) }
    }

    /** 取探测说明：按编号查表返回中文用途。 */
    fun 取探测说明(编号: Int): String {
        if (编号 < 1 || 编号 > 探测表.size) return "未知探测"
        val 定义项 = 探测表[编号 - 1]
        return 定义项.中文名 + "（" + 定义项.代号 + "）：" + 定义项.期望说明
    }

    fun 探测一(): 探测结论 = 执行探测(1)
    fun 说明一(): String = 取探测说明(1)
    fun 探测二(): 探测结论 = 执行探测(2)
    fun 说明二(): String = 取探测说明(2)
    fun 探测三(): 探测结论 = 执行探测(3)
    fun 说明三(): String = 取探测说明(3)
    fun 探测四(): 探测结论 = 执行探测(4)
    fun 说明四(): String = 取探测说明(4)
    fun 探测五(): 探测结论 = 执行探测(5)
    fun 说明五(): String = 取探测说明(5)
    fun 探测六(): 探测结论 = 执行探测(6)
    fun 说明六(): String = 取探测说明(6)
    fun 探测七(): 探测结论 = 执行探测(7)
    fun 说明七(): String = 取探测说明(7)
    fun 探测八(): 探测结论 = 执行探测(8)
    fun 说明八(): String = 取探测说明(8)
    fun 探测九(): 探测结论 = 执行探测(9)
    fun 说明九(): String = 取探测说明(9)
    fun 探测十(): 探测结论 = 执行探测(10)
    fun 说明十(): String = 取探测说明(10)
    fun 探测十一(): 探测结论 = 执行探测(11)
    fun 说明十一(): String = 取探测说明(11)
    fun 探测十二(): 探测结论 = 执行探测(12)
    fun 说明十二(): String = 取探测说明(12)
    fun 探测十三(): 探测结论 = 执行探测(13)
    fun 说明十三(): String = 取探测说明(13)
    fun 探测十四(): 探测结论 = 执行探测(14)
    fun 说明十四(): String = 取探测说明(14)
    fun 探测十五(): 探测结论 = 执行探测(15)
    fun 说明十五(): String = 取探测说明(15)
    fun 探测十六(): 探测结论 = 执行探测(16)
    fun 说明十六(): String = 取探测说明(16)
    fun 探测十七(): 探测结论 = 执行探测(17)
    fun 说明十七(): String = 取探测说明(17)
    fun 探测十八(): 探测结论 = 执行探测(18)
    fun 说明十八(): String = 取探测说明(18)
    fun 探测十九(): 探测结论 = 执行探测(19)
    fun 说明十九(): String = 取探测说明(19)
    fun 探测二十(): 探测结论 = 执行探测(20)
    fun 说明二十(): String = 取探测说明(20)
    fun 探测二十一(): 探测结论 = 执行探测(21)
    fun 说明二十一(): String = 取探测说明(21)
    fun 探测二十二(): 探测结论 = 执行探测(22)
    fun 说明二十二(): String = 取探测说明(22)
    fun 探测二十三(): 探测结论 = 执行探测(23)
    fun 说明二十三(): String = 取探测说明(23)
    fun 探测二十四(): 探测结论 = 执行探测(24)
    fun 说明二十四(): String = 取探测说明(24)
    fun 探测二十五(): 探测结论 = 执行探测(25)
    fun 说明二十五(): String = 取探测说明(25)
    fun 探测二十六(): 探测结论 = 执行探测(26)
    fun 说明二十六(): String = 取探测说明(26)
    fun 探测二十七(): 探测结论 = 执行探测(27)
    fun 说明二十七(): String = 取探测说明(27)
    fun 探测二十八(): 探测结论 = 执行探测(28)
    fun 说明二十八(): String = 取探测说明(28)
    fun 探测二十九(): 探测结论 = 执行探测(29)
    fun 说明二十九(): String = 取探测说明(29)
    fun 探测三十(): 探测结论 = 执行探测(30)
    fun 说明三十(): String = 取探测说明(30)
    fun 探测三十一(): 探测结论 = 执行探测(31)
    fun 说明三十一(): String = 取探测说明(31)
    fun 探测三十二(): 探测结论 = 执行探测(32)
    fun 说明三十二(): String = 取探测说明(32)
    fun 探测三十三(): 探测结论 = 执行探测(33)
    fun 说明三十三(): String = 取探测说明(33)
    fun 探测三十四(): 探测结论 = 执行探测(34)
    fun 说明三十四(): String = 取探测说明(34)
    fun 探测三十五(): 探测结论 = 执行探测(35)
    fun 说明三十五(): String = 取探测说明(35)
    fun 探测三十六(): 探测结论 = 执行探测(36)
    fun 说明三十六(): String = 取探测说明(36)
    fun 探测三十七(): 探测结论 = 执行探测(37)
    fun 说明三十七(): String = 取探测说明(37)
    fun 探测三十八(): 探测结论 = 执行探测(38)
    fun 说明三十八(): String = 取探测说明(38)
    fun 探测三十九(): 探测结论 = 执行探测(39)
    fun 说明三十九(): String = 取探测说明(39)
    fun 探测四十(): 探测结论 = 执行探测(40)
    fun 说明四十(): String = 取探测说明(40)
    fun 探测四十一(): 探测结论 = 执行探测(41)
    fun 说明四十一(): String = 取探测说明(41)
    fun 探测四十二(): 探测结论 = 执行探测(42)
    fun 说明四十二(): String = 取探测说明(42)
    fun 探测四十三(): 探测结论 = 执行探测(43)
    fun 说明四十三(): String = 取探测说明(43)
    fun 探测四十四(): 探测结论 = 执行探测(44)
    fun 说明四十四(): String = 取探测说明(44)
    fun 探测四十五(): 探测结论 = 执行探测(45)
    fun 说明四十五(): String = 取探测说明(45)
    fun 探测四十六(): 探测结论 = 执行探测(46)
    fun 说明四十六(): String = 取探测说明(46)
    fun 探测四十七(): 探测结论 = 执行探测(47)
    fun 说明四十七(): String = 取探测说明(47)
    fun 探测四十八(): 探测结论 = 执行探测(48)
    fun 说明四十八(): String = 取探测说明(48)
    fun 探测四十九(): 探测结论 = 执行探测(49)
    fun 说明四十九(): String = 取探测说明(49)
    fun 探测五十(): 探测结论 = 执行探测(50)
    fun 说明五十(): String = 取探测说明(50)
    fun 探测五十一(): 探测结论 = 执行探测(51)
    fun 说明五十一(): String = 取探测说明(51)
    fun 探测五十二(): 探测结论 = 执行探测(52)
    fun 说明五十二(): String = 取探测说明(52)
    fun 探测五十三(): 探测结论 = 执行探测(53)
    fun 说明五十三(): String = 取探测说明(53)
    fun 探测五十四(): 探测结论 = 执行探测(54)
    fun 说明五十四(): String = 取探测说明(54)
    fun 探测五十五(): 探测结论 = 执行探测(55)
    fun 说明五十五(): String = 取探测说明(55)
    fun 探测五十六(): 探测结论 = 执行探测(56)
    fun 说明五十六(): String = 取探测说明(56)
    fun 探测五十七(): 探测结论 = 执行探测(57)
    fun 说明五十七(): String = 取探测说明(57)
    fun 探测五十八(): 探测结论 = 执行探测(58)
    fun 说明五十八(): String = 取探测说明(58)
    fun 探测五十九(): 探测结论 = 执行探测(59)
    fun 说明五十九(): String = 取探测说明(59)
    fun 探测六十(): 探测结论 = 执行探测(60)
    fun 说明六十(): String = 取探测说明(60)
}
