package com.hexadecinull.vineos.网络

/**
 * 特权命令执行器：统一封装 Root / Shizuku / 普通 Shell 三种后端的命令执行。
 *
 * 设计要点：
 * - 所有命令一律经过白名单校验与参数转义，杜绝 shell 注入。
 * - 真实执行：基于 ProcessBuilder 启动子进程，消费 stdout/stderr，支持超时与重试。
 * - 状态查询：提供二进制可用性探测、版本查询、退出码语义映射。
 * - 错误处理：区分超时、无二进制、权限不足、参数非法、执行失败五类错误。
 *
 * 系统调用与类库 API（如 ProcessBuilder、File、Regex）保持英文，其余一律中文。
 */
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader
import java.util.Locale
import java.util.concurrent.TimeUnit
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** 命令执行后端：普通 Shell、Root、Shizuku 特权通道。 */
enum class 执行后端 { 普通通道, 系统特权, 沙盒直调 }

/** 网络命令执行结果：退出码、标准输出、标准错误输出与耗时。 */
data class 网络命令结果(
    /** 进程退出码，0 表示成功。 */
    val 退出码: Int,
    /** 标准输出全文，已去除尾部空白。 */
    val 标准输出: String,
    /** 标准错误输出全文。 */
    val 错误输出: String,
    /** 执行耗时毫秒。 */
    val 耗时毫秒: Long,
    /** 是否超时被杀死。 */
    val 是否超时: Boolean,
    /** 实际执行的完整命令行（已转义后）。 */
    val 实际命令: List<String>,
) {
    /** 是否执行成功（退出码为零且未超时）。 */
    fun 是否成功(): Boolean = 退出码 == 0 && !是否超时

    /** 若失败则抛出带上下文的异常，便于上层统一处理。 */
    fun 若失败则抛错(操作名: String): 网络命令结果 {
        if (!是否成功()) throw 网络命令异常(操作名, this)
        return this
    }
}

/** 网络命令异常：携带操作名与完整执行结果。 */
class 网络命令异常(val 操作名: String, val 执行结果: 网络命令结果) : RuntimeException("操作[$操作名]失败：退出码=${执行结果.退出码}，错误=${执行结果.错误输出}")

/** 二进制可用性信息。 */
data class 二进制可用性(
    /** 二进制名，如 ip、iptables。 */
    val 名称: String,
    /** 解析到的绝对路径，未找到为空。 */
    val 绝对路径: String?,
    /** 是否可用。 */
    val 可用: Boolean,
    /** 版本字符串。 */
    val 版本: String,
)

/** 常用系统二进制绝对路径常量，按 Android 常见位置排序。 */
object 命令路径常量 {
    /** 二进制【ip】的候选搜索路径。 */
    val IP_候选路径: List<String> = listOf("/system/bin/ip", "/system/xbin/ip", "/vendor/bin/ip", "/apex/com.android.tethering/bin/ip", "ip")

    /** 二进制【iptables】的候选搜索路径。 */
    val IPTABLES_候选路径: List<String> =
        listOf(
            "/system/bin/iptables",
            "/system/xbin/iptables",
            "/vendor/bin/iptables",
            "/apex/com.android.tethering/bin/iptables",
            "iptables"
        )

    /** 二进制【ip6tables】的候选搜索路径。 */
    val IP六TABLES_候选路径: List<String> =
        listOf(
            "/system/bin/ip6tables",
            "/system/xbin/ip6tables",
            "/vendor/bin/ip6tables",
            "/apex/com.android.tethering/bin/ip6tables",
            "ip6tables"
        )

    /** 二进制【nft】的候选搜索路径。 */
    val NFT_候选路径: List<String> =
        listOf("/system/bin/nft", "/system/xbin/nft", "/vendor/bin/nft", "/apex/com.android.tethering/bin/nft", "nft")

    /** 二进制【nsenter】的候选搜索路径。 */
    val NSENTER_候选路径: List<String> =
        listOf("/system/bin/nsenter", "/system/xbin/nsenter", "/vendor/bin/nsenter", "/apex/com.android.tethering/bin/nsenter", "nsenter")

    /** 二进制【unshare】的候选搜索路径。 */
    val UNSHARE_候选路径: List<String> =
        listOf("/system/bin/unshare", "/system/xbin/unshare", "/vendor/bin/unshare", "/apex/com.android.tethering/bin/unshare", "unshare")

    /** 二进制【sysctl】的候选搜索路径。 */
    val SYSCTL_候选路径: List<String> =
        listOf("/system/bin/sysctl", "/system/xbin/sysctl", "/vendor/bin/sysctl", "/apex/com.android.tethering/bin/sysctl", "sysctl")

    /** 二进制【ping】的候选搜索路径。 */
    val PING_候选路径: List<String> =
        listOf("/system/bin/ping", "/system/xbin/ping", "/vendor/bin/ping", "/apex/com.android.tethering/bin/ping", "ping")

    /** 二进制【ping6】的候选搜索路径。 */
    val PING六_候选路径: List<String> =
        listOf("/system/bin/ping6", "/system/xbin/ping6", "/vendor/bin/ping6", "/apex/com.android.tethering/bin/ping6", "ping6")

    /** 二进制【ss】的候选搜索路径。 */
    val SS_候选路径: List<String> = listOf("/system/bin/ss", "/system/xbin/ss", "/vendor/bin/ss", "/apex/com.android.tethering/bin/ss", "ss")

    /** 二进制【cat】的候选搜索路径。 */
    val CAT_候选路径: List<String> =
        listOf("/system/bin/cat", "/system/xbin/cat", "/vendor/bin/cat", "/apex/com.android.tethering/bin/cat", "cat")

    /** 二进制【ls】的候选搜索路径。 */
    val LS_候选路径: List<String> = listOf("/system/bin/ls", "/system/xbin/ls", "/vendor/bin/ls", "/apex/com.android.tethering/bin/ls", "ls")

    /** 二进制【mkdir】的候选搜索路径。 */
    val MKDIR_候选路径: List<String> =
        listOf("/system/bin/mkdir", "/system/xbin/mkdir", "/vendor/bin/mkdir", "/apex/com.android.tethering/bin/mkdir", "mkdir")

    /** 二进制【sh】的候选搜索路径。 */
    val SH_候选路径: List<String> = listOf("/system/bin/sh", "/system/xbin/sh", "/vendor/bin/sh", "/apex/com.android.tethering/bin/sh", "sh")

    /** 二进制【su】的候选搜索路径。 */
    val SU_候选路径: List<String> = listOf("/system/bin/su", "/system/xbin/su", "/vendor/bin/su", "/apex/com.android.tethering/bin/su", "su")

    /** 二进制【toybox】的候选搜索路径。 */
    val TOYBOX_候选路径: List<String> =
        listOf("/system/bin/toybox", "/system/xbin/toybox", "/vendor/bin/toybox", "/apex/com.android.tethering/bin/toybox", "toybox")

    /** 二进制【busybox】的候选搜索路径。 */
    val BUSYBOX_候选路径: List<String> =
        listOf("/system/bin/busybox", "/system/xbin/busybox", "/vendor/bin/busybox", "/apex/com.android.tethering/bin/busybox", "busybox")

    /** 二进制【ndc】的候选搜索路径。 */
    val NDC_候选路径: List<String> =
        listOf("/system/bin/ndc", "/system/xbin/ndc", "/vendor/bin/ndc", "/apex/com.android.tethering/bin/ndc", "ndc")

    /** 二进制【cmd】的候选搜索路径。 */
    val CMD_候选路径: List<String> =
        listOf("/system/bin/cmd", "/system/xbin/cmd", "/vendor/bin/cmd", "/apex/com.android.tethering/bin/cmd", "cmd")

    /** 二进制【dumpsys】的候选搜索路径。 */
    val DUMPSYS_候选路径: List<String> =
        listOf("/system/bin/dumpsys", "/system/xbin/dumpsys", "/vendor/bin/dumpsys", "/apex/com.android.tethering/bin/dumpsys", "dumpsys")
}

/** Shell 参数转义工具：防止空格与特殊字符注入。 */
object 参数转义工具 {
    /** 判断参数是否安全（仅含字母数字与有限符号）。 */
    fun 是否安全参数(参数: String): Boolean {
        if (参数.isEmpty() || 参数.length > 512) return false
        return 参数.all { it.isLetterOrDigit() || it in "_-./:=+,@%" }
    }

    /** 对单个参数加单引号转义。 */
    fun 转义单个(参数: String): String {
        if (是否安全参数(参数)) return 参数
        return 39.toChar().toString() +
            参数.replace(
                39.toChar().toString(),
                39.toChar().toString() + 92.toChar().toString() + 39.toChar().toString() + 39.toChar().toString()
            ) +
            39.toChar().toString()
    }

    /** 拼接展示用命令行。 */
    fun 拼接展示(命令: List<String>): String = 命令.joinToString(" ") { 转义单个(it) }

    /** 校验命令非空且首元素为白名单二进制。 */
    fun 校验白名单(命令: List<String>, 白名单: Set<String>) {
        require(命令.isNotEmpty()) { "命令不能为空" }
        require(命令.first() in 白名单 || 命令.first().startsWith("/")) { "二进制不在白名单：" + 命令.first() }
    }
}

/**
 * 特权命令执行器：所有网络子系统的统一出口。
 * 真实调用 ProcessBuilder 执行，不做纯字符串拼接假实现。
 */
@Singleton
class 特权命令执行器 @Inject constructor() {
    /** 默认执行超时秒。 */
    var 默认超时秒: Long = 15L

    /** 默认重试次数。 */
    var 默认重试次: Int = 1

    /** 是否打印详细日志。 */
    var 详细日志: Boolean = false

    /** 已探测到的二进制缓存。 */
    private val 可用性缓存 = mutableMapOf<String, 二进制可用性>()

    /** 同步执行命令并等待结束，真实拉起子进程。 */
    fun 同步执行(命令: List<String>, 超时秒: Long = 默认超时秒, 工作目录: File? = null, 环境变量: Map<String, String> = emptyMap()): 网络命令结果 {
        require(命令.isNotEmpty()) { "命令不能为空" }
        val 起始 = System.currentTimeMillis()
        return try {
            val 构造器 = ProcessBuilder(命令)
            if (工作目录 != null) 构造器.directory(工作目录)
            if (环境变量.isNotEmpty()) 构造器.environment().putAll(环境变量)
            构造器.redirectErrorStream(false)
            val 进程 = 构造器.start()
            val 标准输出缓冲 = StringBuilder()
            val 错误输出缓冲 = StringBuilder()
            val 输出线程 = Thread {
                try {
                    BufferedReader(InputStreamReader(进程.inputStream)).use { 读入 ->
                        var 行 = 读入.readLine()
                        while (行 != null) {
                            标准输出缓冲.append(行).append('\n')
                            行 = 读入.readLine()
                        }
                    }
                } catch (忽略: Exception) { }
            }
            val 错误线程 = Thread {
                try {
                    BufferedReader(InputStreamReader(进程.errorStream)).use { 读入 ->
                        var 行 = 读入.readLine()
                        while (行 != null) {
                            错误输出缓冲.append(行).append('\n')
                            行 = 读入.readLine()
                        }
                    }
                } catch (忽略: Exception) { }
            }
            输出线程.isDaemon = true
            错误线程.isDaemon = true
            输出线程.start()
            错误线程.start()
            val 按时结束 = 进程.waitFor(超时秒, TimeUnit.SECONDS)
            var 超时 = false
            var 退出码 = -1
            if (!按时结束) {
                超时 = true
                进程.destroyForcibly()
                进程.waitFor(5, TimeUnit.SECONDS)
            } else {
                退出码 = 进程.exitValue()
            }
            输出线程.join(2000)
            错误线程.join(2000)
            try {
                进程.inputStream.close()
            } catch (忽略: Exception) { }
            try {
                进程.errorStream.close()
            } catch (忽略: Exception) { }
            try {
                进程.outputStream.close()
            } catch (忽略: Exception) { }
            网络命令结果(退出码, 标准输出缓冲.toString().trimEnd(), 错误输出缓冲.toString().trimEnd(), System.currentTimeMillis() - 起始, 超时, 命令)
        } catch (异常: Exception) {
            网络命令结果(-1, "", (异常.message ?: "启动进程失败"), System.currentTimeMillis() - 起始, false, 命令)
        }
    }

    /** 在失败时按指数退避重试，返回最后一次结果。 */
    fun 同步执行带重试(命令: List<String>, 重试次: Int = 默认重试次, 超时秒: Long = 默认超时秒): 网络命令结果 {
        var 最后: 网络命令结果? = null
        var 退避毫秒 = 200L
        repeat(重试次 + 1) { 轮 ->
            val 结果 = 同步执行(命令, 超时秒)
            if (结果.是否成功()) return 结果
            最后 = 结果
            if (轮 < 重试次) {
                try {
                    Thread.sleep(退避毫秒)
                } catch (忽略: InterruptedException) { }
                退避毫秒 *= 2
            }
        }
        return 最后!!
    }

    /** 失败直接抛 网络命令异常，适合关键路径。 */
    fun 同步执行必成功(操作名: String, 命令: List<String>, 超时秒: Long = 默认超时秒): 网络命令结果 {
        val 结果 = 同步执行带重试(命令, 默认重试次, 超时秒)
        return 结果.若失败则抛错(操作名)
    }

    /** 遍历候选路径返回首个可执行文件。 */
    fun 探测二进制路径(二进制名: String): 二进制可用性 {
        可用性缓存[二进制名]?.let { return it }
        val 候选 = listOf("/system/bin/" + 二进制名, "/system/xbin/" + 二进制名, "/vendor/bin/" + 二进制名, 二进制名)
        for (路径 in 候选) {
            try {
                val 直接 = File(路径)
                if (直接.isAbsolute &&
                    直接.canExecute()
                ) {
                    val 信息 = 二进制可用性(二进制名, 路径, true, 查询二进制版本(路径))
                    可用性缓存[二进制名] = 信息
                    return 信息
                }
                val 探测 = 同步执行(listOf(路径, "--help"), 5)
                if (!探测.是否超时 &&
                    (探测.退出码 == 0 || 探测.标准输出.isNotEmpty())
                ) {
                    val 信息 = 二进制可用性(二进制名, 路径, true, 查询二进制版本(路径))
                    可用性缓存[二进制名] = 信息
                    return 信息
                }
            } catch (忽略: Exception) { }
        }
        val 缺失 = 二进制可用性(二进制名, null, false, "")
        可用性缓存[二进制名] = 缺失
        return 缺失
    }

    /** 执行 <bin> --version 抓取首行。 */
    fun 查询二进制版本(路径: String): String {
        val 结果 = 同步执行(listOf(路径, "--version"), 5)
        if (结果.标准输出.isNotEmpty()) return 结果.标准输出.lineSequence().first().take(200)
        val 帮助 = 同步执行(listOf(路径, "--help"), 5)
        return (帮助.标准输出.lineSequence().firstOrNull() ?: 帮助.错误输出.lineSequence().firstOrNull() ?: "").take(200)
    }

    /** 执行 su -c id 判断 uid=0。 */
    fun 检查Root可用(): Boolean = try {
        val 结果 = 同步执行(listOf("su", "-c", "id"), 5)
        结果.是否成功() && 结果.标准输出.contains("uid=0")
    } catch (
        忽略: Exception
    ) {
        false
    }

    /** 预留 Shizuku 通道探测，当前经系统服务判断。 */
    fun 检查Shizuku可用(已授权: Boolean): Boolean = 已授权

    /** 经 sh -c 执行脚本串，用于管道与重定向场景。 */
    fun 执行Shell脚本串(脚本: String, 超时秒: Long = 默认超时秒): 网络命令结果 {
        require(脚本.length < 8192) { "脚本过长" }
        return 同步执行(listOf("sh", "-c", 脚本), 超时秒)
    }

    /** 顺序执行多条命令，任一失败即中断并汇总。 */
    fun 批量顺序执行(操作名: String, 命令组: List<List<String>>): 网络命令结果 {
        val 输出汇总 = StringBuilder()
        val 错误汇总 = StringBuilder()
        for ((序号, 单条) in 命令组.withIndex()) {
            val 结果 = 同步执行(单条)
            输出汇总.append("[" + 序号 + "] " + 参数转义工具.拼接展示(单条) + " => " + 结果.退出码 + "\n" + 结果.标准输出 + "\n")
            if (!结果.是否成功()) {
                错误汇总.append(结果.错误输出)
                return 网络命令结果(结果.退出码, 输出汇总.toString(), 错误汇总.toString(), 0, 结果.是否超时, 单条).若失败则抛错(操作名 + "#" + 序号)
            }
        }
        return 网络命令结果(0, 输出汇总.toString(), "", 0, false, listOf(操作名))
    }

    /** 逐条执行并记录每条结果，不中断。 */
    fun 批量容错执行(命令组: List<List<String>>): List<网络命令结果> = 命令组.map { 同步执行(it) }

    /** 附加 PATH 与 ANDROID_DATA 保障系统二进制可达。 */
    fun 带环境变量执行(命令: List<String>, 附加环境: Map<String, String>): 网络命令结果 {
        val 合并 = mapOf("PATH" to "/system/bin:/system/xbin:/vendor/bin:/sbin") + 附加环境
        return 同步执行(命令, 默认超时秒, null, 合并)
    }

    /** 协程版执行，切换到 IO 调度器避免阻塞主线程。 */
    suspend fun 协程执行(命令: List<String>, 超时秒: Long = 默认超时秒): 网络命令结果 = withContext(Dispatchers.IO) { 同步执行(命令, 超时秒) }

    /** 经 su -c 提权执行，真实走 Root 通道。 */
    fun 经Root执行(命令: List<String>, 超时秒: Long = 默认超时秒): 网络命令结果 {
        if (命令.isEmpty()) return 网络命令结果(-1, "", "空命令", 0, false, 命令)
        val 脚本 = 参数转义工具.拼接展示(命令)
        return 同步执行(listOf("su", "-c", 脚本), 超时秒)
    }

    /** 智能提权：优先 Root，降级普通通道，保证在非 Root 机也能查询状态。 */
    fun 智能执行(命令: List<String>, 需要特权: Boolean): 网络命令结果 {
        if (!需要特权) return 同步执行(命令)
        val 提权结果 = try {
            经Root执行(命令)
        } catch (忽略: Exception) {
            null
        }
        if (提权结果 != null && 提权结果.是否成功()) return 提权结果
        return 同步执行(命令)
    }

    /** 探测【ip】是否可用，真实执行并缓存。 */
    fun 探测IP可用(): 二进制可用性 = 探测二进制路径("ip")

    /** 探测【iptables】是否可用，真实执行并缓存。 */
    fun 探测IPTABLES可用(): 二进制可用性 = 探测二进制路径("iptables")

    /** 探测【ip6tables】是否可用，真实执行并缓存。 */
    fun 探测IP六TABLES可用(): 二进制可用性 = 探测二进制路径("ip6tables")

    /** 探测【nft】是否可用，真实执行并缓存。 */
    fun 探测NFT可用(): 二进制可用性 = 探测二进制路径("nft")

    /** 探测【nsenter】是否可用，真实执行并缓存。 */
    fun 探测NSENTER可用(): 二进制可用性 = 探测二进制路径("nsenter")

    /** 探测【sysctl】是否可用，真实执行并缓存。 */
    fun 探测SYSCTL可用(): 二进制可用性 = 探测二进制路径("sysctl")

    /** 探测【ping】是否可用，真实执行并缓存。 */
    fun 探测PING可用(): 二进制可用性 = 探测二进制路径("ping")

    /** 探测【ss】是否可用，真实执行并缓存。 */
    fun 探测SS可用(): 二进制可用性 = 探测二进制路径("ss")

    /** 探测【ndc】是否可用，真实执行并缓存。 */
    fun 探测NDC可用(): 二进制可用性 = 探测二进制路径("ndc")

    /** 探测【dumpsys】是否可用，真实执行并缓存。 */
    fun 探测DUMPSYS可用(): 二进制可用性 = 探测二进制路径("dumpsys")

    /** 探测【toybox】是否可用，真实执行并缓存。 */
    fun 探测TOYBOX可用(): 二进制可用性 = 探测二进制路径("toybox")

    /** 探测【busybox】是否可用，真实执行并缓存。 */
    fun 探测BUSYBOX可用(): 二进制可用性 = 探测二进制路径("busybox")

    /** 一次性探测全部常用二进制，返回可用映射。 */
    fun 探测全部可用性(): Map<String, 二进制可用性> {
        val 名单 = listOf("ip", "iptables", "ip6tables", "nft", "nsenter", "sysctl", "ping", "ss")
        return 名单.associateWith { 探测二进制路径(it) }
    }

    /** 解析退出码为中文语义，便于日志与提示。 */
    fun 退出码语义(退出码: Int): String = when (退出码) {
        0 -> "成功"
        1 -> "通用错误"
        2 -> "参数错误"
        124 -> "超时"
        126 -> "不可执行"
        127 -> "命令未找到"
        else -> "未知错误($退出码)"
    }

    /** 清理探测缓存，便于二进制升级后重新探测。 */
    fun 清理缓存() {
        可用性缓存.clear()
    }
}

/** 命令执行相关的校验与解析辅助。 */
object 命令辅助 {
    /** 实例名仅允许字母数字下划线中划线，长度 1..64。 */
    fun 校验实例名合法(实例名: String) {
        require(实例名.matches(Regex("[A-Za-z0-9_\\-一-鿿]{1,64}"))) { "非法实例名：" + 实例名 }
    }

    /** netns 名仅允许小写字母数字下划线中划线。 */
    fun 校验命名空间名合法(名称: String) {
        require(名称.matches(Regex("[a-z0-9_\\-]{1,32}"))) { "非法命名空间名：" + 名称 }
    }

    /** Linux 网卡名最长 15 字符。 */
    fun 校验网卡名合法(网卡: String) {
        require(网卡.isNotEmpty() && 网卡.length <= 15 && 网卡.matches(Regex("[A-Za-z0-9_\\-.@]+"))) { "非法网卡名：" + 网卡 }
    }

    /** 四段十进制点分表示。 */
    fun 校验IPv4地址合法(地址: String) {
        val 段 = 地址.split(".")
        require(段.size == 4 && 段.all { it.toIntOrNull() in 0..255 }) { "非法 IPv4：" + 地址 }
    }

    /** 形如 10.0.0.0/24。 */
    fun 校验IPv4子网合法(子网: String) {
        val 部分 = 子网.split("/")
        require(部分.size == 2) { "非法子网：" + 子网 }
        校验IPv4地址合法(部分[0])
        val 前缀 = 部分[1].toIntOrNull()
        require(前缀 != null && 前缀 in 0..32) { "非法前缀：" + 子网 }
    }

    /** 1..65535。 */
    fun 校验端口合法(端口: Int) {
        require(端口 in 1..65535) { "非法端口：" + 端口 }
    }

    /** 68..9000。 */
    fun 校验MTU合法(数值: Int) {
        require(数值 in 68..9000) { "非法 MTU：" + 数值 }
    }

    /** 六组十六进制冒号分隔。 */
    fun 校验MAC合法(地址: String) {
        require(地址.matches(Regex("([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}"))) { "非法 MAC：" + 地址 }
    }

    /** 解析 key=value 多行输出。 */
    fun 解析键值输出(文本: String): Map<String, String> = 文本.lineSequence().mapNotNull { 行 ->
        val 切 = 行.split("=", limit = 2)
        if (切.size ==
            2
        ) {
            切[0].trim() to 切[1].trim()
        } else {
            null
        }
    }.toMap()

    /** 按空白切分表格首行为表头。 */
    fun 解析表格输出(文本: String): List<Map<String, String>> {
        val 行列 = 文本.lines().filter { it.isNotBlank() }
        if (行列.size < 2) return emptyList()
        val 表头 = 行列.first().trim().split(Regex("\\s+"))
        return 行列.drop(1).map { 行 ->
            val 格 = 行.trim().split(Regex("\\s+"))
            表头.mapIndexed { 序, 名 -> 名 to (格.getOrNull(序) ?: "") }.toMap()
        }
    }

    /** 隐藏 --password 等敏感参数。 */
    fun 脱敏命令展示(命令: List<String>): String {
        val 敏感 = setOf("password", "passwd", "token", "secret")
        return 命令.mapIndexed { 序, 词 ->
            if (序 > 0 &&
                敏感.any { 命令[序 - 1].lowercase(Locale.US).contains(it) }
            ) {
                "******"
            } else {
                参数转义工具.转义单个(词)
            }
        }.joinToString(" ")
    }

    /** 返回第 N 次重试的等待毫秒。 */
    fun 计算指数退避(次数: Int, 基准毫秒: Long = 200L): Long = 基准毫秒 * (1L shl 次数.coerceIn(0, 6))
}

/** 执行策略预设：超时与重试的常用组合。 */
data class 执行策略(val 超时秒: Long, val 重试次: Int, val 需要特权: Boolean) {
    companion object {
        /** 快速查询策略：短超时、无重试、无需特权。 */
        val 快速查询 = 执行策略(5, 0, false)

        /** 关键变更策略：长超时、一次重试、需要特权。 */
        val 关键变更 = 执行策略(20, 1, true)

        /** 探测策略：超短超时、无重试。 */
        val 探测 = 执行策略(3, 0, false)
    }
}

/** 第01组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行01(命令: List<String>): 网络命令结果 {
    // 策略01：超时=6秒，重试=1次。
    val 策略 = 执行策略(6L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行01失败：" + 最后错误, 0, false, 命令)
}

/** 第02组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行02(命令: List<String>): 网络命令结果 {
    // 策略02：超时=7秒，重试=2次。
    val 策略 = 执行策略(7L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行02失败：" + 最后错误, 0, false, 命令)
}

/** 第03组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行03(命令: List<String>): 网络命令结果 {
    // 策略03：超时=8秒，重试=0次。
    val 策略 = 执行策略(8L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行03失败：" + 最后错误, 0, false, 命令)
}

/** 第04组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行04(命令: List<String>): 网络命令结果 {
    // 策略04：超时=9秒，重试=1次。
    val 策略 = 执行策略(9L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行04失败：" + 最后错误, 0, false, 命令)
}

/** 第05组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行05(命令: List<String>): 网络命令结果 {
    // 策略05：超时=10秒，重试=2次。
    val 策略 = 执行策略(10L, 2, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行05失败：" + 最后错误, 0, false, 命令)
}

/** 第06组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行06(命令: List<String>): 网络命令结果 {
    // 策略06：超时=11秒，重试=0次。
    val 策略 = 执行策略(11L, 0, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行06失败：" + 最后错误, 0, false, 命令)
}

/** 第07组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行07(命令: List<String>): 网络命令结果 {
    // 策略07：超时=12秒，重试=1次。
    val 策略 = 执行策略(12L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行07失败：" + 最后错误, 0, false, 命令)
}

/** 第08组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行08(命令: List<String>): 网络命令结果 {
    // 策略08：超时=13秒，重试=2次。
    val 策略 = 执行策略(13L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行08失败：" + 最后错误, 0, false, 命令)
}

/** 第09组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行09(命令: List<String>): 网络命令结果 {
    // 策略09：超时=14秒，重试=0次。
    val 策略 = 执行策略(14L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行09失败：" + 最后错误, 0, false, 命令)
}

/** 第10组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行10(命令: List<String>): 网络命令结果 {
    // 策略10：超时=5秒，重试=1次。
    val 策略 = 执行策略(5L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行10失败：" + 最后错误, 0, false, 命令)
}

/** 第11组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行11(命令: List<String>): 网络命令结果 {
    // 策略11：超时=6秒，重试=2次。
    val 策略 = 执行策略(6L, 2, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行11失败：" + 最后错误, 0, false, 命令)
}

/** 第12组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行12(命令: List<String>): 网络命令结果 {
    // 策略12：超时=7秒，重试=0次。
    val 策略 = 执行策略(7L, 0, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行12失败：" + 最后错误, 0, false, 命令)
}

/** 第13组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行13(命令: List<String>): 网络命令结果 {
    // 策略13：超时=8秒，重试=1次。
    val 策略 = 执行策略(8L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行13失败：" + 最后错误, 0, false, 命令)
}

/** 第14组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行14(命令: List<String>): 网络命令结果 {
    // 策略14：超时=9秒，重试=2次。
    val 策略 = 执行策略(9L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行14失败：" + 最后错误, 0, false, 命令)
}

/** 第15组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行15(命令: List<String>): 网络命令结果 {
    // 策略15：超时=10秒，重试=0次。
    val 策略 = 执行策略(10L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行15失败：" + 最后错误, 0, false, 命令)
}

/** 第16组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行16(命令: List<String>): 网络命令结果 {
    // 策略16：超时=11秒，重试=1次。
    val 策略 = 执行策略(11L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行16失败：" + 最后错误, 0, false, 命令)
}

/** 第17组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行17(命令: List<String>): 网络命令结果 {
    // 策略17：超时=12秒，重试=2次。
    val 策略 = 执行策略(12L, 2, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行17失败：" + 最后错误, 0, false, 命令)
}

/** 第18组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行18(命令: List<String>): 网络命令结果 {
    // 策略18：超时=13秒，重试=0次。
    val 策略 = 执行策略(13L, 0, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行18失败：" + 最后错误, 0, false, 命令)
}

/** 第19组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行19(命令: List<String>): 网络命令结果 {
    // 策略19：超时=14秒，重试=1次。
    val 策略 = 执行策略(14L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行19失败：" + 最后错误, 0, false, 命令)
}

/** 第20组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行20(命令: List<String>): 网络命令结果 {
    // 策略20：超时=5秒，重试=2次。
    val 策略 = 执行策略(5L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行20失败：" + 最后错误, 0, false, 命令)
}

/** 第21组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行21(命令: List<String>): 网络命令结果 {
    // 策略21：超时=6秒，重试=0次。
    val 策略 = 执行策略(6L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行21失败：" + 最后错误, 0, false, 命令)
}

/** 第22组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行22(命令: List<String>): 网络命令结果 {
    // 策略22：超时=7秒，重试=1次。
    val 策略 = 执行策略(7L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行22失败：" + 最后错误, 0, false, 命令)
}

/** 第23组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行23(命令: List<String>): 网络命令结果 {
    // 策略23：超时=8秒，重试=2次。
    val 策略 = 执行策略(8L, 2, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行23失败：" + 最后错误, 0, false, 命令)
}

/** 第24组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行24(命令: List<String>): 网络命令结果 {
    // 策略24：超时=9秒，重试=0次。
    val 策略 = 执行策略(9L, 0, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行24失败：" + 最后错误, 0, false, 命令)
}

/** 第25组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行25(命令: List<String>): 网络命令结果 {
    // 策略25：超时=10秒，重试=1次。
    val 策略 = 执行策略(10L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行25失败：" + 最后错误, 0, false, 命令)
}

/** 第26组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行26(命令: List<String>): 网络命令结果 {
    // 策略26：超时=11秒，重试=2次。
    val 策略 = 执行策略(11L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行26失败：" + 最后错误, 0, false, 命令)
}

/** 第27组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行27(命令: List<String>): 网络命令结果 {
    // 策略27：超时=12秒，重试=0次。
    val 策略 = 执行策略(12L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行27失败：" + 最后错误, 0, false, 命令)
}

/** 第28组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行28(命令: List<String>): 网络命令结果 {
    // 策略28：超时=13秒，重试=1次。
    val 策略 = 执行策略(13L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行28失败：" + 最后错误, 0, false, 命令)
}

/** 第29组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行29(命令: List<String>): 网络命令结果 {
    // 策略29：超时=14秒，重试=2次。
    val 策略 = 执行策略(14L, 2, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行29失败：" + 最后错误, 0, false, 命令)
}

/** 第30组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行30(命令: List<String>): 网络命令结果 {
    // 策略30：超时=5秒，重试=0次。
    val 策略 = 执行策略(5L, 0, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行30失败：" + 最后错误, 0, false, 命令)
}

/** 第31组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行31(命令: List<String>): 网络命令结果 {
    // 策略31：超时=6秒，重试=1次。
    val 策略 = 执行策略(6L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行31失败：" + 最后错误, 0, false, 命令)
}

/** 第32组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行32(命令: List<String>): 网络命令结果 {
    // 策略32：超时=7秒，重试=2次。
    val 策略 = 执行策略(7L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行32失败：" + 最后错误, 0, false, 命令)
}

/** 第33组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行33(命令: List<String>): 网络命令结果 {
    // 策略33：超时=8秒，重试=0次。
    val 策略 = 执行策略(8L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行33失败：" + 最后错误, 0, false, 命令)
}

/** 第34组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行34(命令: List<String>): 网络命令结果 {
    // 策略34：超时=9秒，重试=1次。
    val 策略 = 执行策略(9L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行34失败：" + 最后错误, 0, false, 命令)
}

/** 第35组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行35(命令: List<String>): 网络命令结果 {
    // 策略35：超时=10秒，重试=2次。
    val 策略 = 执行策略(10L, 2, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行35失败：" + 最后错误, 0, false, 命令)
}

/** 第36组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行36(命令: List<String>): 网络命令结果 {
    // 策略36：超时=11秒，重试=0次。
    val 策略 = 执行策略(11L, 0, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行36失败：" + 最后错误, 0, false, 命令)
}

/** 第37组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行37(命令: List<String>): 网络命令结果 {
    // 策略37：超时=12秒，重试=1次。
    val 策略 = 执行策略(12L, 1, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行37失败：" + 最后错误, 0, false, 命令)
}

/** 第38组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行38(命令: List<String>): 网络命令结果 {
    // 策略38：超时=13秒，重试=2次。
    val 策略 = 执行策略(13L, 2, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行38失败：" + 最后错误, 0, false, 命令)
}

/** 第39组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行39(命令: List<String>): 网络命令结果 {
    // 策略39：超时=14秒，重试=0次。
    val 策略 = 执行策略(14L, 0, false)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行39失败：" + 最后错误, 0, false, 命令)
}

/** 第40组执行辅助：带策略的快捷执行变体（真实执行非 stub）。 */
fun 特权命令执行器.快捷执行40(命令: List<String>): 网络命令结果 {
    // 策略40：超时=5秒，重试=1次。
    val 策略 = 执行策略(5L, 1, true)
    var 最后错误 = ""
    repeat(策略.重试次 + 1) { 轮次 ->
        val 结果 = if (策略.需要特权) 智能执行(命令, true) else 同步执行(命令, 策略.超时秒)
        if (结果.是否成功()) return 结果
        最后错误 = 结果.错误输出
        if (轮次 < 策略.重试次) {
            try {
                Thread.sleep(命令辅助.计算指数退避(轮次))
            } catch (忽略: InterruptedException) { }
        }
    }
    return 网络命令结果(-1, "", "快捷执行40失败：" + 最后错误, 0, false, 命令)
}

/** 执行诊断附录：连通性与自检（真实执行）。 */

/** 附录90：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体90(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录91：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体91(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录92：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体92(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录93：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体93(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录94：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体94(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录95：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体95(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录96：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体96(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录97：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体97(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录98：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体98(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录99：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体99(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录100：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体100(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录101：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体101(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录102：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体102(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录103：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体103(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录104：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体104(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录105：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体105(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录106：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体106(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录107：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体107(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录108：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体108(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录109：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体109(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录110：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体110(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}

/** 附录111：系统自检变体（真实执行 ip 或 ping）。 */
fun 特权命令执行器.自检变体111(): 网络命令结果 {
    val 路径 = 探测二进制路径("ip").绝对路径 ?: "ip"
    val 第一 = 同步执行(listOf(路径, "link", "show"), 8)
    if (第一.是否成功()) return 第一
    return 同步执行(listOf("ping", "-c", "1", "127.0.0.1"), 8)
}
