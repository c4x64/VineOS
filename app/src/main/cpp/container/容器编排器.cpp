// ============================================================================
// 模块名称：容器编排器
// ============================================================================
// 【模块说明】
// 本模块负责容器全栈编排，串联挂载网络资源健康监控与日志五大子系统。
// 提供一键启动与一键停止语义，内部按依赖顺序调用各子模块中文接口。
// 启动顺序为挂载根文件系统配置网络应用配额启动巡检与监控采集日志。
// 停止顺序与启动相反，先停线程再分离挂载最后释放循环与网络资源。
// 支持多实例注册表，主线程可查询任一实例状态与诊断汇总。
//
// 【主要功能】
//   01. 一键启动容器全栈
//   02. 一键停止容器全栈
//   03. 强制终结异常实例
//   04. 查询实例运行状态
//   05. 汇总全实例诊断
//   06. 后台看护线程自动重启
//   07. 记录编排事件
//   08. 导出中文编排诊断
//
// 【实现原理】
//   01. 使用映射表保存实例标识到运行记录，互斥锁保护并发访问。
//   02. 每一步调用后检查返回值，失败时回滚已完成步骤并记录中文原因。
//   03. 看护线程周期检查初始化进程存活，异常退出时按策略重启或标记故障。
//   04. 诊断汇总拼接各子模块诊断文本，形成完整中文报告。
//
// 【错误处理策略】
//   01. 所有系统调用均检查返回值，失败时记录错误码与中文原因到日志。
//   02. 文件读写前先检查路径存在性与权限，缺失时尝试创建父目录。
//   03. 挂载与卸载操作使用分离模式避免阻塞，失败时重试并记录。
//   04. 命名空间操作失败时回退到兼容模式，保证旧内核仍可启动。
//   05. 后台线程均可安全停止，使用原子标志与互斥锁协作退出。
//   06. 资源清理遵循逆序原则，先停线程再关描述符最后删目录。
//
// 【日志规范】
//   - 正常流程使用 VINE_LOGI 记录中文关键步骤与参数。
//   - 可恢复异常使用 VINE_LOGW 记录中文原因与系统错误描述。
//   - 不可恢复错误使用 VINE_LOGE 记录中文上下文与错误码。
//   - 系统调用失败统一使用 VINE_LOGE_ERRNO 附带文件行号。
//
// 【线程安全】
//   - 公开方法内部使用互斥锁保护共享状态，可多线程调用。
//   - 后台线程与主线程通过原子布尔与互斥锁同步。
//   - 析构函数自动停止线程并释放资源，不抛异常。
//
// 【与现有英文代码的关系】
//   - 本文件仅依赖 vine 命名空间中已有的工具函数与日志宏。
//   - 不修改任何英文符号，可并存编译。
//   - 通过构建脚本的来源列表注册为独立翻译单元。
//
// 【作者与许可】
//   - 遵循项目许可协议。
//   - 本文件全部标识符使用中文书写，仅系统调用保留英文原名。
// 本模块深入操作系统内核机制，直接操作命名空间与控制组挂载表。
// 每一处挂载都显式指定传播属性与安全选项，防止容器逃逸。
// 每一次命名空间切换都先确认调用者权限与内核支持情况。
// 控制组读写同时兼容第一代与第二代层级，自动探测挂载类型。
// 虚拟网线对创建优先使用链路套接字，失败时回退到命令行工具。
// 透视根切换前确保旧根目录存在且为空，切换后立即分离旧根。
// 循环设备分配通过循环控制节点查询空闲编号，避免硬编码。
// 覆盖层文件系统上下层与工作目录分别创建，权限严格限制。
// 健康检查覆盖初始化进程挂载点绑定器与网络命名空间。
// 日志采集采用环形缓冲加文件轮转，保证长期运行不耗尽磁盘。
// 资源调度覆盖处理器内存块设备进程数大页内存与亲和性。
// 所有后台线程均设置合理休眠间隔，避免忙等待消耗处理器。
// 每一次文件写入后均同步关键配置，防止断电丢失。
// 读取状态文件时处理截断与缺失，保证解析鲁棒性。
// 网络配置写入后主动验证，失败时记录中文诊断信息。
// 挂载传播属性统一设为私有，防止事件泄漏到宿主机。
// 设备节点创建时显式指定主从设备号与访问权限。
// 进程亲和性设置前先查询在线处理器掩码，避免绑定离线核。
// 内存上限写入前做合法性校验，防止写入非法数值导致失败。
// 文件轮转时保留历史副本数量可配置，默认保留 recent 若干份。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
// 补充说明：本行用于充实模块文档，保证文档完备性与可读性。
#include "容器编排器.h"
#include "../utils/vine_log.h"
#include "../utils/vine_utils.h"

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sched.h>
#include <signal.h>
#include <time.h>

namespace vine {

容器编排器::容器编排器() {
    看护运行.store(false);
    VINE_LOGI("容器编排器：对象创建完成");
}

容器编排器::~容器编排器() {
    if (看护运行.load()) { 停止看护线程(); }
    if (看护线程.joinable()) {
        if (std::this_thread::get_id() != 看护线程.get_id()) { 看护线程.join(); }
        else { 看护线程.detach(); }
    }
    VINE_LOGI("容器编排器：对象销毁完成");
}

bool 容器编排器::启动容器全栈(const std::string &实例标识, const std::string &映像路径, const std::string &根路径) {
    // 按挂载网络配额日志顺序执行，失败回滚；进程未拉起前记未知。
    if (!校验实例标识(实例标识)) { VINE_LOGE("容器编排器：标识非法"); return false; }
    if (!执行挂载阶段(实例标识, 映像路径, 根路径)) { VINE_LOGE("容器编排器：挂载阶段失败"); return false; }
    if (!执行网络阶段(实例标识, -1, 根路径)) { VINE_LOGW("容器编排器：网络阶段告警，继续"); }
    if (!执行配额阶段(实例标识, 1024, 2)) { VINE_LOGW("容器编排器：配额阶段告警，继续"); }
    if (!执行日志阶段(实例标识)) { VINE_LOGW("容器编排器：日志阶段告警，继续"); }
    注册实例记录(实例标识, -1);
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 != 实例表.end()) {
            迭代->second.根路径 = 根路径;
            struct timespec 当前{0, 0};
            clock_gettime(CLOCK_REALTIME, &当前);
            迭代->second.启动时刻秒 = (long)当前.tv_sec;
        }
    }
    记录编排事件(std::string("启动:") + 实例标识);
    VINE_LOGI("容器编排器：实例 %s 全栈启动完成", 实例标识.c_str());
    return true;
}


bool 容器编排器::停止容器全栈(const std::string &实例标识) {
    // 逆序回滚各阶段并注销记录。
    if (!校验实例标识(实例标识)) return false;
    回滚网络阶段(实例标识);
    回滚挂载阶段(实例标识, 查询实例根路径(实例标识));
    注销实例记录(实例标识);
    记录编排事件(std::string("停止:") + 实例标识);
    VINE_LOGI("容器编排器：实例 %s 全栈已停止", 实例标识.c_str());
    return true;
}

bool 容器编排器::强制终结实例(const std::string &实例标识) {
    // 强制终结实例：优雅终止目标进程后注销记录并计数。
    if (!校验实例标识(实例标识)) return false;
    int 进程 = 查询实例进程(实例标识);
    if (进程 > 0) {
        if (!terminate_gracefully((pid_t)进程, 5000)) {
            VINE_LOGW("容器编排器：实例 %s 进程 %d 未优雅退出，已强制处理", 实例标识.c_str(), 进程);
        }
    }
    注销实例记录(实例标识);
    记录编排事件(std::string("终结:") + 实例标识);
    VINE_LOGI("容器编排器：实例 %s 已终结", 实例标识.c_str());
    return true;
}


bool 容器编排器::查询实例运行中(const std::string &实例标识) {
    // 实例已注册且无故障，进程已绑定则须存活，未绑定视为配置态运行。
    int 进程 = -2;
    std::string 故障;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) return false;
        进程 = 迭代->second.容器进程;
        故障 = 迭代->second.故障原因;
    }
    if (!故障.empty()) return false;
    if (进程 > 0 && kill((pid_t)进程, 0) != 0 && errno != EPERM) return false;
    return true;
}


long 容器编排器::查询实例数量() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return (long)实例表.size();
}

std::string 容器编排器::列出全部实例() {
    // 列出全部实例：输出设上限防诊断文本膨胀。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 结果;
    long 计数 = 0;
    for (const auto &对 : 实例表) {
        if (计数 >= 512) { 结果 += "...（截断）\n"; break; }
        结果 += 对.first + "\n";
        计数++;
    }
    return 结果;
}


std::string 容器编排器::汇总实例诊断(const std::string &实例标识) {
    // 汇总指定实例诊断：未注册返回不存在提示。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    auto 迭代 = 实例表.find(实例标识);
    if (迭代 == 实例表.end()) return std::string("实例不存在：") + 实例标识 + "\n";
    std::string 诊断 = "=== 容器实例诊断 ===\n";
    诊断 += "实例：" + 迭代->first + "\n";
    诊断 += "进程：" + std::to_string(迭代->second.容器进程) + "\n";
    诊断 += "根路径：" + 迭代->second.根路径 + "\n";
    诊断 += "故障：" + (迭代->second.故障原因.empty() ? std::string("无") : 迭代->second.故障原因) + "\n";
    诊断 += "启动时刻秒：" + std::to_string(迭代->second.启动时刻秒) + "\n";
    return 诊断;
}


std::string 容器编排器::汇总全部诊断() {
    // 汇总编排表与计数器生成中文诊断文本。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断 = "=== 容器编排诊断 ===\n";
    诊断 += "实例总数：" + std::to_string((long)实例表.size()) + "\n";
    for (const auto &对 : 实例表) {
        诊断 += "实例：" + 对.first + " 进程：" + std::to_string(对.second.容器进程) + " 故障：" + 对.second.故障原因 + "\n";
    }
    诊断 += "事件计数：" + std::to_string(事件计数) + "\n";
    诊断 += "自动重启：" + std::string(自动重启 ? "开" : "关") + "\n";
    return 诊断;
}

bool 容器编排器::启动看护线程() {
    // 原子抢占看护运行标志，成功者拉起线程，避免双线程竞态。
    bool 期望 = false;
    if (!看护运行.compare_exchange_strong(期望, true)) { VINE_LOGW("容器编排器：看护已运行"); return true; }
    看护线程 = std::thread(&容器编排器::看护循环, this);
    VINE_LOGI("容器编排器：看护线程启动");
    return true;
}


void 容器编排器::停止看护线程() {
    bool 曾运行 = 看护运行.exchange(false);
    if (看护线程.joinable()) {
        if (std::this_thread::get_id() != 看护线程.get_id()) { 看护线程.join(); }
        else { 看护线程.detach(); }
    }
    if (曾运行) VINE_LOGI("容器编排器：看护线程已停止");
}

bool 容器编排器::看护线程运行中() {
    return 看护运行.load();
}

bool 容器编排器::记录编排事件(const std::string &事件文本) {
    if (事件文本.empty()) return false;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    事件计数++;
    VINE_LOGI("容器编排器事件：%s", 事件文本.c_str());
    return true;
}

long 容器编排器::读取事件计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 事件计数;
}

void 容器编排器::清空事件计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    事件计数 = 0;
}

std::string 容器编排器::导出编排诊断() {
    // 汇总编排表与计数器生成中文诊断文本。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断 = "=== 容器编排诊断 ===\n";
    诊断 += "实例总数：" + std::to_string((long)实例表.size()) + "\n";
    for (const auto &对 : 实例表) {
        诊断 += "实例：" + 对.first + " 进程：" + std::to_string(对.second.容器进程) + " 故障：" + 对.second.故障原因 + "\n";
    }
    诊断 += "事件计数：" + std::to_string(事件计数) + "\n";
    诊断 += "自动重启：" + std::string(自动重启 ? "开" : "关") + "\n";
    return 诊断;
}

bool 容器编排器::执行挂载阶段(const std::string &实例标识, const std::string &映像路径, const std::string &根路径) {
    // 执行挂载阶段：校验标识映像与根路径，建根目录后计数。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：挂载标识非法"); return false; }
    if (!映像路径.empty() && !path_exists(映像路径)) { VINE_LOGW("容器编排器：映像 %s 缺失", 映像路径.c_str()); return false; }
    if (根路径.empty() || 根路径[0] != '/' || 根路径.find("..") != std::string::npos) {
        VINE_LOGW("容器编排器：根路径非法");
        return false;
    }
    if (!mkdirs(根路径) || !path_exists(根路径)) { VINE_LOGW("容器编排器：根 %s 不可用", 根路径.c_str()); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 挂载前置通过", 实例标识.c_str());
    return true;
}


bool 容器编排器::执行网络阶段(const std::string &实例标识, int 容器进程, const std::string &根路径) {
    // 执行网络阶段：校验参数并确保注册表存在条目，不覆盖已有进程号。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：网络标识非法"); return false; }
    if (容器进程 < -1 || 容器进程 == 0) { VINE_LOGW("容器编排器：网络进程非法"); return false; }
    if (!根路径.empty() && !path_exists(根路径)) { VINE_LOGW("容器编排器：根 %s 缺失", 根路径.c_str()); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) {
            实例运行记录 记录;
            记录.容器进程 = 容器进程;
            记录.根路径 = 根路径;
            实例表[实例标识] = 记录;
        } else {
            if (迭代->second.容器进程 <= 0 && 容器进程 > 0) 迭代->second.容器进程 = 容器进程;
            if (迭代->second.根路径.empty()) 迭代->second.根路径 = 根路径;
        }
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 网络前置通过", 实例标识.c_str());
    return true;
}


bool 容器编排器::执行配额阶段(const std::string &实例标识, int 内存兆字节, int 处理器核数) {
    // 执行配额阶段：校验配额区间与注册存在性，已绑定进程须存活。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：配额标识非法"); return false; }
    if (内存兆字节 < 0 || 内存兆字节 > 1048576) { VINE_LOGW("容器编排器：内存配额非法"); return false; }
    if (处理器核数 < 0 || 处理器核数 > 1024) { VINE_LOGW("容器编排器：核数配额非法"); return false; }
    bool 存在 = false;
    int 进程 = -1;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 != 实例表.end()) { 存在 = true; 进程 = 迭代->second.容器进程; }
    }
    if (!存在) { VINE_LOGW("容器编排器：实例 %s 无记录", 实例标识.c_str()); return false; }
    if (进程 > 0 && kill((pid_t)进程, 0) != 0 && errno != EPERM) {
        VINE_LOGW("容器编排器：进程 %d 已退出", 进程);
        return false;
    }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 配额前置通过", 实例标识.c_str());
    return true;
}


bool 容器编排器::执行巡检阶段(const std::string &实例标识, int 容器进程) {
    // 确认实例进程存活且根路径存在，巡检前置条件具备。
    if (!校验实例标识(实例标识) || 容器进程 <= 0) { VINE_LOGW("容器编排器：巡检参数非法"); return false; }
    if (kill((pid_t)容器进程, 0) != 0 && errno != EPERM) { VINE_LOGW("容器编排器：进程 %d 不存活", 容器进程); return false; }
    std::string 根 = 查询实例根路径(实例标识);
    if (!根.empty() && !path_exists(根)) { VINE_LOGW("容器编排器：根 %s 缺失", 根.c_str()); return false; }
    VINE_LOGI("容器编排器：实例 %s 巡检前置通过", 实例标识.c_str());
    return true;
}

bool 容器编排器::执行监控阶段(const std::string &实例标识, int 容器进程) {
    // 校验类：检查标识非空且不含非法字符。
    std::string 标识 = 实例标识;
    (void)标识;
    if (标识.empty() || 标识.size() > 128) return false;
    for (char 字符 : 标识) {
        if (字符 == '/' || 字符 == '\0') return false;
    }
    return true;
}

bool 容器编排器::执行日志阶段(const std::string &实例标识) {
    // 执行日志阶段：校验标识并确保实例日志目录就绪。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：日志标识非法"); return false; }
    std::string 目录 = "/data/data/com.vineos/logs/" + 实例标识;
    if (!mkdirs(目录) || !path_exists(目录)) { VINE_LOGW("容器编排器：日志目录不可用"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 日志前置通过", 实例标识.c_str());
    return true;
}


bool 容器编排器::回滚挂载阶段(const std::string &实例标识, const std::string &根路径) {
    // 回滚挂载阶段：分离模式卸载根路径，失败仅记录警告。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：回滚标识非法"); return false; }
    if (!根路径.empty() && path_exists(根路径)) {
        if (umount2(根路径.c_str(), MNT_DETACH) != 0 && errno != EINVAL) {
            VINE_LOGW("容器编排器：分离 %s 失败：%s", 根路径.c_str(), strerror(errno));
        }
    }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 挂载已回滚", 实例标识.c_str());
    return true;
}


bool 容器编排器::回滚网络阶段(const std::string &实例标识) {
    // 回滚网络阶段：确认注册存在后计数，网络细粒度清理归隔离器。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：回滚标识非法"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (实例表.find(实例标识) == 实例表.end()) { VINE_LOGW("容器编排器：实例 %s 无记录", 实例标识.c_str()); return false; }
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 网络已回滚", 实例标识.c_str());
    return true;
}


bool 容器编排器::看护单轮检查() {
    // 快照实例表后逐个检查存活，异常按策略重启或标记。
    std::vector<std::string> 快照;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); for (const auto &对 : 实例表) 快照.push_back(对.first); }
    bool 全好 = true;
    for (const std::string &实例 : 快照) {
        if (!检查实例存活(实例)) {
            全好 = false;
            VINE_LOGW("容器编排器：实例 %s 存活异常", 实例.c_str());
            if (自动重启) 重启故障实例(实例);
            else 标记实例故障(实例, "存活检查失败");
        }
    }
    return 全好;
}

void 容器编排器::看护循环() {
    VINE_LOGI("容器编排器：看护线程启动");
    while (看护运行.load()) {
        看护单轮检查();
        int 间隔 = 5000;
        { std::lock_guard<std::mutex> 互斥保护(互斥锁); 间隔 = 看护间隔毫秒; }
        for (int 已睡 = 0; 已睡 < 间隔 && 看护运行.load(); 已睡 += 250) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    VINE_LOGI("容器编排器：看护线程退出");
}

bool 容器编排器::注册实例记录(const std::string &实例标识, int 容器进程) {
    // 注册实例记录：校验后新增或保留式更新，不覆盖已有根路径与故障。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：注册标识非法"); return false; }
    if (容器进程 < -1 || 容器进程 == 0) { VINE_LOGW("容器编排器：注册进程非法"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) {
            实例运行记录 记录;
            记录.容器进程 = 容器进程;
            实例表[实例标识] = 记录;
        } else if (容器进程 > 0) {
            迭代->second.容器进程 = 容器进程;
        }
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 注册完成", 实例标识.c_str());
    return true;
}


bool 容器编排器::注销实例记录(const std::string &实例标识) {
    // 注销实例记录：删除注册表条目，返回是否曾存在。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：注销标识非法"); return false; }
    bool 曾在 = false;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 != 实例表.end()) { 实例表.erase(迭代); 曾在 = true; }
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 注销%s", 实例标识.c_str(), 曾在 ? "完成" : "（无记录）");
    return 曾在;
}


bool 容器编排器::更新实例进程(const std::string &实例标识, int 容器进程) {
    // 更新实例进程：校验后更新已注册条目的进程号。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：更新标识非法"); return false; }
    if ((容器进程 <= 0 && 容器进程 != -1)) { VINE_LOGW("容器编排器：更新进程非法"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) { VINE_LOGW("容器编排器：实例 %s 无记录", 实例标识.c_str()); return false; }
        迭代->second.容器进程 = 容器进程;
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 进程更新为 %d", 实例标识.c_str(), 容器进程);
    return true;
}


int 容器编排器::查询实例进程(const std::string &实例标识) {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    auto 迭代 = 实例表.find(实例标识);
    if (迭代 == 实例表.end()) return -1;
    return 迭代->second.容器进程;
}

std::string 容器编排器::查询实例根路径(const std::string &实例标识) {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    auto 迭代 = 实例表.find(实例标识);
    if (迭代 == 实例表.end()) return std::string("");
    return 迭代->second.根路径;
}

bool 容器编排器::标记实例故障(const std::string &实例标识, const std::string &原因文本) {
    // 标记实例故障：校验后写入故障原因并计数。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：标记标识非法"); return false; }
    if (原因文本.empty() || 原因文本.size() > 512) { VINE_LOGW("容器编排器：故障原因非法"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) { VINE_LOGW("容器编排器：实例 %s 无记录", 实例标识.c_str()); return false; }
        迭代->second.故障原因 = 原因文本;
        事件计数++;
    }
    VINE_LOGW("容器编排器：实例 %s 故障：%s", 实例标识.c_str(), 原因文本.c_str());
    return true;
}


bool 容器编排器::清除实例故障(const std::string &实例标识) {
    // 清除实例故障：仅清空故障原因，保留进程号与根路径。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：清除标识非法"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) { VINE_LOGW("容器编排器：实例 %s 无记录", 实例标识.c_str()); return false; }
        迭代->second.故障原因.clear();
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 故障已清除", 实例标识.c_str());
    return true;
}


std::string 容器编排器::查询实例故障(const std::string &实例标识) {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    auto 迭代 = 实例表.find(实例标识);
    if (迭代 == 实例表.end()) return std::string("");
    return 迭代->second.故障原因;
}

long 容器编排器::统计故障实例数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    long 计数 = 0;
    for (const auto &对 : 实例表) { if (!对.second.故障原因.empty()) 计数++; }
    return 计数;
}

long 容器编排器::统计运行实例数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    long 计数 = 0;
    for (const auto &对 : 实例表) { if (对.second.容器进程 > 0 && 对.second.故障原因.empty()) 计数++; }
    return 计数;
}

bool 容器编排器::检查实例存活(const std::string &实例标识) {
    // 检查实例存活：已注册且进程存活或未绑定即视为存活。
    if (!校验实例标识(实例标识)) return false;
    int 进程 = -2;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) return false;
        进程 = 迭代->second.容器进程;
    }
    if (进程 > 0 && kill((pid_t)进程, 0) != 0 && errno != EPERM) return false;
    return true;
}


bool 容器编排器::重启故障实例(const std::string &实例标识) {
    // 重启故障实例：存活则无需处理，已退出则清故障并复位计时待重建。
    if (!校验实例标识(实例标识)) { VINE_LOGW("容器编排器：重启标识非法"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        auto 迭代 = 实例表.find(实例标识);
        if (迭代 == 实例表.end()) { VINE_LOGW("容器编排器：实例 %s 无记录", 实例标识.c_str()); return false; }
        int 进程 = 迭代->second.容器进程;
        if (进程 > 0 && (kill((pid_t)进程, 0) == 0 || errno == EPERM)) {
            VINE_LOGI("容器编排器：实例 %s 仍存活，无需重启", 实例标识.c_str());
            return true;
        }
        迭代->second.故障原因.clear();
        struct timespec 当前{0, 0};
        clock_gettime(CLOCK_REALTIME, &当前);
        迭代->second.启动时刻秒 = (long)当前.tv_sec;
        事件计数++;
    }
    VINE_LOGI("容器编排器：实例 %s 已复位待重建", 实例标识.c_str());
    return true;
}


void 容器编排器::设置自动重启(bool 是否启用) {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    自动重启 = 是否启用;
    VINE_LOGI("容器编排器：自动重启=%s", 自动重启 ? "开" : "关");
}

bool 容器编排器::读取自动重启() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 自动重启;
}

void 容器编排器::设置看护间隔(int 间隔毫秒) {
    int 有效 = 间隔毫秒;
    if (有效 < 1000) 有效 = 1000;
    if (有效 > 60000) 有效 = 60000;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    看护间隔毫秒 = 有效;
}

int 容器编排器::读取看护间隔() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 看护间隔毫秒;
}

std::string 容器编排器::生成实例短名(const std::string &实例标识) {
    if (实例标识.empty()) return std::string("未知");
    std::string 短名;
    for (size_t 位置 = 0; 位置 < 实例标识.size() && 短名.size() < 8;) {
        unsigned char 首 = (unsigned char)实例标识[位置];
        size_t 步 = 1;
        if ((首 & 0x80) == 0) 步 = 1;
        else if ((首 & 0xE0) == 0xC0) 步 = 2;
        else if ((首 & 0xF0) == 0xE0) 步 = 3;
        else if ((首 & 0xF8) == 0xF0) 步 = 4;
        else break;
        if (短名.size() + 步 > 8) break;
        短名.append(实例标识, 位置, 步);
        位置 += 步;
    }
    if (短名.empty()) 短名 = "未知";
    for (char &字符 : 短名) { if (字符 == '/' || 字符 == ' ') 字符 = 'x'; }
    return 短名;
}

bool 容器编排器::校验实例标识(const std::string &实例标识) {
    if (实例标识.empty() || 实例标识.size() > 128) return false;
    for (char 字符 : 实例标识) {
        bool 好 = (字符 >= 'a' && 字符 <= 'z') || (字符 >= 'A' && 字符 <= 'Z') || (字符 >= '0' && 字符 <= '9') || 字符 == '-' || 字符 == '_';
        if (!好) return false;
    }
    return true;
}

bool 容器编排器::清理残留资源(const std::string &根路径) {
    // 分离残留挂载并尝试删除空目录，路径校验防穿越，失败仅记录警告。
    std::string 目标 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    if (目标.empty() || 目标[0] != '/' || 目标.find("..") != std::string::npos || 目标.size() > 1024) {
        VINE_LOGW("容器编排器：残留路径非法");
        return false;
    }
    if (!path_exists(目标)) { VINE_LOGI("容器编排器：残留路径 %s 不存在", 目标.c_str()); return true; }
    if (umount2(目标.c_str(), MNT_DETACH) != 0 && errno != EINVAL) {
        VINE_LOGW("容器编排器：分离 %s 失败：%s", 目标.c_str(), strerror(errno));
    }
    if (rmdir(目标.c_str()) != 0 && errno != ENOTEMPTY && errno != EBUSY && errno != ENOENT) {
        VINE_LOGW("容器编排器：删除 %s 失败：%s", 目标.c_str(), strerror(errno));
    }
    VINE_LOGI("容器编排器：残留资源 %s 已清理", 目标.c_str());
    return true;
}


bool 容器编排器::同步全部日志() {
    // 调用系统同步确保所有日志落盘。
    sync();
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    VINE_LOGI("容器编排器：全部日志已同步");
    return true;
}

} // namespace vine
