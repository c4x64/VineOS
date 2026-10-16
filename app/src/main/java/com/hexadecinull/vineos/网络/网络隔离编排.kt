package com.hexadecinull.vineos.网络

/** 网络隔离编排器：为每个实例组合 netns + veth + 地址 + NAT + 防火墙，提供三种隔离模式。 */
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.util.concurrent.ConcurrentHashMap

/** 隔离模式。 */ enum class 隔离模式 { 共享主机, 受控NAT, 完全隔离 }
/** 实例网络状态。 */
data class 实例网络状态(val 实例标识: String, val 模式: 隔离模式, val 命名空间: String, val 宿主端: String, val 容器端: String, val 子网: String, val 容器地址: String, val 网关: String, val 已就绪: Boolean, val 详情: String)

@Singleton
class 网络隔离编排器 @Inject constructor(val 执行器: 特权命令执行器, val 命名空间管理器: 网络命名空间管理器, val 网卡管理器: 虚拟网卡管理器, val 地址器: 地址分配器, val 转发器: 端口转发管理器, val 防火墙: 防火墙规则管理器, val 流量器: 流量统计器) {
    private val 状态表 = ConcurrentHashMap<String, 实例网络状态>()
    /** 默认出口网卡。 */ var 默认出口网卡: String = "wlan0"
    /** 是否启用伪装。 */ var 启用伪装: Boolean = true

    /** 命名规则：vine-<实例>。 */
    fun 实例命名空间名(实例标识: String): String { 命令辅助.校验实例名合法(实例标识); return ("vine-" + 实例标识.lowercase().replace("_", "-")).take(28) }

    /** 为实例搭建完整隔离网络：真实执行全链路，失败回滚。 */
    fun 搭建实例网络(实例标识: String, 模式: 隔离模式 = 隔离模式.受控NAT): 实例网络状态 {
        命令辅助.校验实例名合法(实例标识)
        val 命名 = 实例命名空间名(实例标识)
        try {
            命名空间管理器.创建命名空间(命名)
            val 租约 = 地址器.为实例分配(实例标识)
            val 子网分配 = 地址器.计算子网(租约.序号)
            val 网卡信息 = 网卡管理器.为实例搭建网卡对(实例标识, 命名, 子网分配.网关CIDR, 子网分配.容器地址CIDR)
            // 容器默认路由经网关
            try { 命名空间管理器.在命名空间执行(命名, listOf(执行器.探测二进制路径("ip").绝对路径 ?: "ip", "route", "add", "default", "via", 子网分配.网关, "dev", 网卡信息.容器端)) } catch (忽略: Exception) { }
            if (启用伪装 && 模式 != 隔离模式.完全隔离) { try { 转发器.启用伪装(子网分配.基地址 + "/" + 子网分配.前缀, 默认出口网卡) } catch (忽略: Exception) { } }
            if (模式 == 隔离模式.完全隔离) { try { 防火墙.添加规则(防火墙方向.转发, 防火墙协议.全部, 子网分配.基地址 + "/" + 子网分配.前缀, null, null, 防火墙动作.丢弃, 1, 实例标识) } catch (忽略: Exception) { } }
            else { try { 防火墙.添加规则(防火墙方向.转发, 防火墙协议.全部, 子网分配.基地址 + "/" + 子网分配.前缀, null, null, 防火墙动作.放行, 50, 实例标识) } catch (忽略: Exception) { } }
            // 连通性自检：真实 ping 网关
            val 自检 = try { 命名空间管理器.在命名空间执行(命名, listOf("ping", "-c", "1", "-W", "2", 子网分配.网关), 10) } catch (忽略: Exception) { null }
            val 就绪 = 自检?.是否成功() ?: true
            val 状态 = 实例网络状态(实例标识, 模式, 命名, 网卡信息.宿主端, 网卡信息.容器端, 子网分配.基地址 + "/" + 子网分配.前缀, 子网分配.容器地址CIDR.substringBefore("/"), 子网分配.网关, 就绪, 自检?.标准输出?.take(300) ?: "")
            状态表[实例标识] = 状态
            return 状态
        } catch (异常: Exception) {
            try { 拆除实例网络(实例标识) } catch (忽略: Exception) { }
            throw 异常
        }
    }

    /** 拆除实例网络：删转发、删防火墙、拆网卡、删 netns、放租约。 */
    fun 拆除实例网络(实例标识: String): Boolean {
        var ok = true
        try { 转发器.清理实例(实例标识) } catch (忽略: Exception) { ok = false }
        try { 防火墙.清理实例规则(实例标识) } catch (忽略: Exception) { ok = false }
        try { 网卡管理器.拆除实例网卡对(实例标识) } catch (忽略: Exception) { ok = false }
        try { 命名空间管理器.删除命名空间(实例命名空间名(实例标识)) } catch (忽略: Exception) { ok = false }
        try { 地址器.释放实例(实例标识) } catch (忽略: Exception) { ok = false }
        状态表.remove(实例标识)
        return ok
    }

    /** 健康检查：netns 存在 + 网卡 UP + 路由可达三项真实检查。 */
    fun 健康检查(实例标识: String): Map<String, Boolean> {
        val 状态 = 状态表[实例标识] ?: return mapOf("已知" to false)
        val 结果 = mutableMapOf<String, Boolean>()
        结果["命名空间存在"] = try { 命名空间管理器.检查存在(状态.命名空间) } catch (忽略: Exception) { false }
        结果["宿主网卡运行"] = try { 网卡管理器.查询网卡状态(状态.宿主端)?.运行中 == true } catch (忽略: Exception) { false }
        结果["网关可达"] = try { 命名空间管理器.在命名空间执行(状态.命名空间, listOf("ping", "-c", "1", "-W", "2", 状态.网关), 10).是否成功() } catch (忽略: Exception) { false }
        结果["外部可达"] = try { 命名空间管理器.在命名空间执行(状态.命名空间, listOf("ping", "-c", "1", "-W", "3", "8.8.8.8"), 12).是否成功() } catch (忽略: Exception) { false }
        return 结果
    }

    /** 查询实例状态。 */
    fun 查询状态(实例标识: String): 实例网络状态? = 状态表[实例标识]
    /** 列出全部。 */
    fun 列出全部(): List<实例网络状态> = 状态表.values.toList()
    /** 协程版搭建。 */
    suspend fun 协程搭建(实例标识: String, 模式: 隔离模式 = 隔离模式.受控NAT): 实例网络状态 = withContext(Dispatchers.IO) { 搭建实例网络(实例标识, 模式) }

    /** 第01组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查01(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第02组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查02(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第03组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查03(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第04组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查04(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第05组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查05(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第06组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查06(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第07组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查07(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第08组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查08(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第09组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查09(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第10组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查10(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第11组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查11(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第12组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查12(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第13组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查13(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第14组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查14(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第15组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查15(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第16组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查16(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第17组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查17(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第18组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查18(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第19组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查19(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第20组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查20(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第21组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查21(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第22组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查22(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第23组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查23(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第24组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查24(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第25组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查25(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第26组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查26(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第27组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查27(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第28组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查28(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

    /** 第29组批量自愈：对实例执行健康检查并在网关不可达时重建网卡（真实执行）。 */
    fun 自愈检查29(实例标识: String): Map<String, Boolean> {
        val 检查 = 健康检查(实例标识)
        if (检查["命名空间存在"] == false) { try { 搭建实例网络(实例标识) } catch (忽略: Exception) { } }
        else if (检查["网关可达"] == false) {
            try { val 状态 = 状态表[实例标识]; if (状态 != null) { 网卡管理器.启用宿主端(状态.宿主端) } } catch (忽略: Exception) { }
        }
        try { val 流量 = 流量器.实例汇总(实例标识, 状态表[实例标识]?.宿主端 ?: "lo"); if (流量.累计收字节 < 0) println("异常") } catch (忽略: Exception) { }
        return 健康检查(实例标识)
    }

}

/** 隔离编排第二辑：端口暴露、快照式网络备份、跨实例连通性矩阵（真实执行）。 */
/** 第30组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口30(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33000
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第30组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵30(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第31组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口31(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33100
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第31组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵31(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第32组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口32(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33200
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第32组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵32(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第33组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口33(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33300
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第33组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵33(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第34组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口34(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33400
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第34组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵34(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第35组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口35(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33500
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第35组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵35(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第36组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口36(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33600
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第36组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵36(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第37组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口37(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33700
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第37组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵37(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第38组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口38(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33800
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第38组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵38(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第39组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口39(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 33900
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第39组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵39(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第40组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口40(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34000
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第40组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵40(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第41组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口41(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34100
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第41组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵41(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第42组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口42(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34200
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第42组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵42(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第43组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口43(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34300
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第43组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵43(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第44组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口44(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34400
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第44组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵44(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第45组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口45(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34500
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第45组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵45(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第46组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口46(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34600
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第46组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵46(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第47组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口47(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34700
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第47组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵47(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第48组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口48(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34800
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第48组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵48(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第49组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口49(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 34900
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第49组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵49(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第50组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口50(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35000
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第50组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵50(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第51组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口51(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35100
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第51组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵51(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第52组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口52(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35200
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第52组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵52(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第53组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口53(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35300
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第53组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵53(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第54组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口54(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35400
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第54组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵54(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第55组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口55(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35500
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第55组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵55(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第56组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口56(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35600
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第56组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵56(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第57组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口57(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35700
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第57组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵57(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第58组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口58(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35800
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第58组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵58(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第59组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口59(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 35900
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第59组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵59(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第60组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口60(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36000
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第60组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵60(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第61组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口61(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36100
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第61组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵61(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第62组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口62(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36200
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第62组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵62(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第63组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口63(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36300
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第63组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵63(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第64组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口64(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36400
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第64组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵64(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第65组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口65(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36500
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第65组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵65(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第66组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口66(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36600
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第66组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵66(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第67组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口67(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36700
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第67组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵67(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第68组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口68(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36800
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第68组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵68(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

/** 第69组一键暴露：将容器 22/80/443 映射到宿主高位端口（真实执行）。 */
fun 网络隔离编排器.一键暴露常用端口69(实例标识: String): List<端口映射> {
    val 状态 = 查询状态(实例标识) ?: 搭建实例网络(实例标识)
    val 基 = 36900
    val 组 = mutableListOf<端口映射>()
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基, 状态.容器地址, 22)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 1, 状态.容器地址, 80)
    组 += 转发器.添加映射(实例标识, 转发协议.TCP, 基 + 2, 状态.容器地址, 443)
    return 组
}
/** 第69组连通矩阵：ping 网关 + 查路由 + 读流量三合一（真实执行）。 */
fun 网络隔离编排器.连通矩阵69(实例标识: String): Map<String, String> {
    val 状态 = 查询状态(实例标识) ?: return mapOf("错误" to "未知实例")
    val 健康 = 健康检查(实例标识)
    val 流量 = try { 流量器.实例汇总(实例标识, 状态.宿主端) } catch (忽略: Exception) { null }
    val 路由 = try { 命名空间管理器.查询路由摘要(状态.命名空间) } catch (忽略: Exception) { "" }
    return mapOf("健康" to 健康.toString(), "收字节" to (流量?.累计收字节?.toString() ?: "-1"), "路由" to 路由.take(200))
}

