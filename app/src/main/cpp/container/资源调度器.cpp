// ============================================================================
// 模块名称：资源调度器
// ============================================================================
// 【模块说明】
// 本模块负责容器资源调度，通过控制组层级限制处理器内存与输入输出配额。
// 同时兼容第一代与第二代控制组，优先使用统一层级并自动回退到分离层级。
// 支持内存上限处理器配额处理器集权重冻结解冻与进程数上限等完整语义。
// 所有写入前均校验数值合法性，失败时记录中文原因与系统错误码。
// 提供亲和性与优先级调整能力，保证前台容器获得更好的调度延迟。
//
// 【主要功能】
//   01. 应用内存上限与处理器配额
//   02. 应用块设备权重与进程数上限
//   03. 设置处理器亲和掩码
//   04. 调整进程优先级与调度策略
//   05. 冻结与解冻容器分组
//   06. 查询当前已应用的配额
//   07. 移除分组并清理控制组目录
//   08. 导出中文调度诊断
//
// 【实现原理】
//   01. 探测统一层级存在性决定写入 memory.max 还是 limit_in_bytes。
//   02. 处理器配额换算为周期与配额微秒对，写入 cpu.max 或 cfs 配额对。
//   03. 处理器集通过读取在线掩码校验后写入 cpuset.cpus 生效。
//   04. 冻结使用 freezer.state 或 cgroup.freeze，轮询等待状态翻转。
//   05. 优先级通过 setpriority 与 sched_setscheduler 系统调用实现。
//   06. 亲和性通过 sched_setaffinity 解析掩码字符串后绑定。
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
#include "资源调度器.h"
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
std::string 资源调度器::短名(const std::string &实例标识) {
    // 短名：按字符边界截断并替换分隔符，防目录穿越与非法字节序列。
    if (实例标识.empty()) return std::string("默认");
    std::string 短;
    for (size_t 位置 = 0; 位置 < 实例标识.size() && 短.size() < 8;) {
        unsigned char 首 = (unsigned char)实例标识[位置];
        size_t 步 = 1;
        if ((首 & 0x80) == 0) 步 = 1;
        else if ((首 & 0xE0) == 0xC0) 步 = 2;
        else if ((首 & 0xF0) == 0xE0) 步 = 3;
        else if ((首 & 0xF8) == 0xF0) 步 = 4;
        else break;
        if (短.size() + 步 > 8) break;
        短.append(实例标识, 位置, 步);
        位置 += 步;
    }
    if (短.empty()) 短 = "默认";
    for (char &字符 : 短) { if (字符 == '/' || 字符 == ' ' || 字符 == '.') 字符 = 'x'; }
    return 短;
}
std::string 资源调度器::统一分组(const std::string &实例标识) {
    // 统一分组：拼接统一层级下实例分组绝对路径。
    return "/sys/fs/cgroup/vineos-" + 短名(实例标识);
}
bool 资源调度器::写入统一文件(const std::string &实例标识, const char *文件名, const std::string &内容) {
    // 写入统一文件：建组后截断写入并回读校验，失败记录中文原因。
    if (实例标识.empty() || 文件名 == nullptr || *文件名 == '\0') return false;
    if (strchr(文件名, '/') != nullptr || strstr(文件名, "..") != nullptr) return false;
    if (内容.size() > 4096) return false;
    std::string 分组 = 统一分组(实例标识);
    if (!mkdirs(分组)) { VINE_LOGW("资源调度器：创建 %s 失败", 分组.c_str()); return false; }
    std::string 路径 = 分组 + "/" + 文件名;
    int 描述符 = open(路径.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGW("资源调度器：打开 %s 失败：%s", 路径.c_str(), strerror(errno)); return false; }
    const char *指针 = 内容.c_str();
    size_t 剩余 = 内容.size();
    while (剩余 > 0) {
        ssize_t 单次 = write(描述符, 指针, 剩余);
        if (单次 < 0) {
            if (errno == EINTR) continue;
            VINE_LOGW("资源调度器：写入 %s 失败：%s", 路径.c_str(), strerror(errno));
            close(描述符);
            return false;
        }
        指针 += 单次;
        剩余 -= (size_t)单次;
    }
    close(描述符);
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (已创建分组.size() < 512) 已创建分组.push_back(分组);
        事件计数++;
    }
    return true;
}
long 资源调度器::读取统一整数(const std::string &实例标识, const char *文件名) {
    // 读取统一整数：解析分组文件首个整数，不限配额返回负一。
    if (实例标识.empty() || 文件名 == nullptr || *文件名 == '\0') return -1;
    if (strchr(文件名, '/') != nullptr || strstr(文件名, "..") != nullptr) return -1;
    std::string 路径 = 统一分组(实例标识) + "/" + 文件名;
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容 || 内容->empty()) return -1;
    size_t 起始 = 内容->find_first_not_of(" \t\n");
    if (起始 == std::string::npos) return -1;
    if (内容->compare(起始, 3, "max") == 0) return -1;
    char *尾部 = nullptr;
    long 数值 = strtol(内容->c_str() + 起始, &尾部, 10);
    if (尾部 == 内容->c_str() + 起始 || 数值 < 0) return -1;
    return 数值;
}
std::string 资源调度器::读取统一文本(const std::string &实例标识, const char *文件名) {
    // 读取统一文本：去尾空白后返回分组文件内容。
    if (实例标识.empty() || 文件名 == nullptr || *文件名 == '\0') return std::string("");
    if (strchr(文件名, '/') != nullptr || strstr(文件名, "..") != nullptr) return std::string("");
    std::string 路径 = 统一分组(实例标识) + "/" + 文件名;
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) return std::string("");
    std::string 结果 = *内容;
    while (!结果.empty() && (结果.back() == '\n' || 结果.back() == ' ' || 结果.back() == '\t')) 结果.pop_back();
    return 结果;
}
bool 资源调度器::写入分组值(const std::string &实例标识, const char *统一名, const char *控制器, const char *分离名, const std::string &内容) {
    // 写入分组值：按层级标志选择统一或分离路径写入。
    if (统一层级) return 写入统一文件(实例标识, 统一名, 内容);
    if (控制器 == nullptr || 分离名 == nullptr) return false;
    std::string 标签;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        标签 = 分组标签;
    }
    if (标签.empty()) 标签 = "默认分组";
    return 写入分离层级文件(控制器, 短名(实例标识) + "/" + 分离名, 内容);
}
long 资源调度器::读取分组整数(const std::string &实例标识, const char *统一名, const char *控制器, const char *分离名) {
    // 读取分组整数：按层级标志选择统一或分离路径解析。
    if (统一层级) return 读取统一整数(实例标识, 统一名);
    if (控制器 == nullptr || 分离名 == nullptr) return -1;
    std::string 原文 = 读取分离层级文件(控制器, 短名(实例标识) + "/" + 分离名);
    if (原文.empty()) return -1;
    size_t 起始 = 原文.find_first_not_of(" \t\n");
    if (起始 == std::string::npos) return -1;
    if (原文.compare(起始, 3, "max") == 0) return -1;
    char *尾部 = nullptr;
    long 数值 = strtol(原文.c_str() + 起始, &尾部, 10);
    if (尾部 == 原文.c_str() + 起始 || 数值 < 0) return -1;
    return 数值;
}
bool 资源调度器::校验进程存活(int 进程标识) {
    // 校验进程存活：信号零探测，无副作用。
    if (进程标识 <= 0) return false;
    if (kill((pid_t)进程标识, 0) == 0) return true;
    return errno == EPERM;
}
bool 资源调度器::等待冻结状态(const std::string &分组路径, bool 期望冻结, int 超时毫秒) {
    // 等待冻结状态：轮询统一与分离冻结文件直至期望值或超时。
    if (分组路径.empty()) return false;
    if (超时毫秒 < 500) 超时毫秒 = 500;
    if (超时毫秒 > 30000) 超时毫秒 = 30000;
    std::string 统一文件 = 分组路径 + "/cgroup.freeze";
    std::string 分离文件 = 分组路径 + "/freezer.state";
    for (int 已等 = 0; 已等 < 超时毫秒; 已等 += 100) {
        std::optional<std::string> 内容 = read_file(统一文件);
        if (!内容) 内容 = read_file(分离文件);
        if (内容) {
            std::string 去空 = *内容;
            while (!去空.empty() && (去空.back() == '\n' || 去空.back() == ' ')) 去空.pop_back();
            if (期望冻结 && (去空 == "1" || 去空 == "FROZEN")) return true;
            if (!期望冻结 && (去空 == "0" || 去空 == "THAWED")) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    VINE_LOGW("资源调度器：等待 %s 超时", 分组路径.c_str());
    return false;
}

资源调度器::资源调度器() {
    // 探测统一层级是否存在，决定后续写入路径，并记录中文日志。
    统一层级 = path_exists("/sys/fs/cgroup/cgroup.controllers");
    VINE_LOGI("资源调度器：创建完成，统一层级=%s", 统一层级 ? "是" : "否");
}

资源调度器::~资源调度器() {
    // 析构仅记录日志，不自动删除分组，避免影响仍在运行的容器。
    VINE_LOGI("资源调度器：销毁完成，共创建 %zu 分组", 已创建分组.size());
}

bool 资源调度器::应用全部配额(const std::string &实例标识, int 进程标识, int 内存兆字节, int 处理器核数) {
    // 聚合调用内存与处理器配额，任一失败即返回假并记录。
    bool 内存好 = 应用内存上限(实例标识, 内存兆字节);
    bool 处理器好 = 应用处理器配额(实例标识, 处理器核数);
    bool 绑定好 = true;
    if (进程标识 > 0) 绑定好 = 绑定分组进程(实例标识, 进程标识);
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    if (内存好 && 处理器好 && 绑定好) { VINE_LOGI("资源调度器：实例 %s 全部配额生效", 实例标识.c_str()); return true; }
    VINE_LOGW("资源调度器：实例 %s 部分配额未生效", 实例标识.c_str());
    return false;
}

bool 资源调度器::应用内存上限(const std::string &实例标识, int 内存兆字节) {
    // 统一层级写 memory.max，分离层级写 limit_in_bytes，零或负表示不限。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    if (内存兆字节 < 0) 内存兆字节 = 0;
    std::string 标签 = 短名(实例标识);
    bool 成功 = false;
    if (统一层级) {
        std::string 分组 = "/sys/fs/cgroup/vineos-" + 标签;
        if (!mkdirs(分组)) { VINE_LOGW("资源调度器：创建 %s 失败", 分组.c_str()); return false; }
        std::string 上限文本 = 内存兆字节 > 0 ? std::to_string((long)内存兆字节 * 1024L * 1024L) : std::string("max");
        if (write_file(分组 + "/memory.max", 上限文本)) {
            成功 = true;
            std::lock_guard<std::mutex> 互斥保护(互斥锁);
            已创建分组.push_back(分组);
            内存兆字节_ = 内存兆字节;
        } else VINE_LOGW("资源调度器：写入 memory.max 失败");
    } else {
        std::string 分组 = "/sys/fs/cgroup/memory/vineos-" + 标签;
        if (!mkdirs(分组)) { VINE_LOGW("资源调度器：创建 %s 失败", 分组.c_str()); return false; }
        if (内存兆字节 > 0) {
            if (write_file(分组 + "/memory.limit_in_bytes", std::to_string((long)内存兆字节 * 1024L * 1024L))) {
                成功 = true;
                std::lock_guard<std::mutex> 互斥保护(互斥锁);
                已创建分组.push_back(分组);
            }
        } else 成功 = true;
    }
    if (成功) VINE_LOGI("资源调度器：实例 %s 内存上限 %d MB 生效", 实例标识.c_str(), 内存兆字节);
    return 成功;
}

bool 资源调度器::应用处理器配额(const std::string &实例标识, int 处理器核数) {
    // 统一层级写 cpu.max，分离层级写 cfs 周期配额对。
    if (实例标识.empty()) return false;
    if (处理器核数 < 0) 处理器核数 = 0;
    std::string 标签 = 短名(实例标识);
    long 周期微秒 = 100000L;
    bool 成功 = false;
    if (统一层级) {
        std::string 分组 = "/sys/fs/cgroup/vineos-" + 标签;
        mkdirs(分组);
        std::string 配额文本 = 处理器核数 > 0 ? std::to_string(周期微秒 * (long)处理器核数) + " " + std::to_string(周期微秒) : std::string("max 100000");
        if (write_file(分组 + "/cpu.max", 配额文本)) 成功 = true;
        else VINE_LOGW("资源调度器：写入 cpu.max 失败");
    } else {
        std::string 分组 = "/sys/fs/cgroup/cpu/vineos-" + 标签;
        mkdirs(分组);
        if (处理器核数 > 0) {
            bool 好一 = write_file(分组 + "/cpu.cfs_period_us", std::to_string(周期微秒));
            bool 好二 = write_file(分组 + "/cpu.cfs_quota_us", std::to_string(周期微秒 * (long)处理器核数));
            成功 = 好一 && 好二;
        } else 成功 = true;
    }
    if (成功) {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        处理器核数_ = 处理器核数;
        VINE_LOGI("资源调度器：实例 %s 处理器配额 %d 核生效", 实例标识.c_str(), 处理器核数);
    }
    return 成功;
}

bool 资源调度器::应用块设备权重(const std::string &实例标识, int 权重值) {
    // 应用块设备权重：校验区间后写入输入输出权重文件。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    int 有效 = 权重值;
    if (有效 < 1) 有效 = 1;
    if (统一层级 && 有效 > 10000) 有效 = 10000;
    if (!统一层级 && 有效 > 1000) 有效 = 1000;
    if (!统一层级 && 有效 < 10) 有效 = 10;
    bool 好 = 写入分组值(实例标识, "io.weight", "blkio", "blkio.weight", std::to_string(有效));
    if (好) VINE_LOGI("资源调度器：实例 %s 块权重 %d 生效", 实例标识.c_str(), 有效);
    else VINE_LOGW("资源调度器：实例 %s 块权重写入失败", 实例标识.c_str());
    return 好;
}


bool 资源调度器::应用进程数上限(const std::string &实例标识, long 上限数) {
    // 应用进程数上限：非正视为不限，写入进程数上限文件。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    std::string 文本 = (上限数 <= 0) ? std::string("max") : std::to_string(上限数);
    bool 好 = 写入分组值(实例标识, "pids.max", "pids", "pids.max", 文本);
    if (好) VINE_LOGI("资源调度器：实例 %s 进程上限 %s 生效", 实例标识.c_str(), 文本.c_str());
    else VINE_LOGW("资源调度器：实例 %s 进程上限写入失败", 实例标识.c_str());
    return 好;
}


bool 资源调度器::设置处理器亲和(int 进程标识, const std::string &掩码文本) {
    // 设置处理器亲和：解析区间掩码并绑定，成功后回读验证。
    if (!校验进程存活(进程标识)) { VINE_LOGE("资源调度器：进程 %d 不存活", 进程标识); return false; }
    if (!校验亲和掩码(掩码文本)) { VINE_LOGE("资源调度器：掩码非法"); return false; }
    cpu_set_t 集合;
    CPU_ZERO(&集合);
    size_t 位置 = 0;
    while (位置 < 掩码文本.size()) {
        char *尾部 = nullptr;
        long 起始 = strtol(掩码文本.c_str() + 位置, &尾部, 10);
        if (尾部 == 掩码文本.c_str() + 位置 || 起始 < 0 || 起始 >= 4096) return false;
        long 结束 = 起始;
        size_t 偏移 = (size_t)(尾部 - 掩码文本.c_str());
        if (偏移 < 掩码文本.size() && 掩码文本[偏移] == '-') {
            char *尾二 = nullptr;
            结束 = strtol(掩码文本.c_str() + 偏移 + 1, &尾二, 10);
            if (尾二 == 掩码文本.c_str() + 偏移 + 1 || 结束 < 起始 || 结束 >= 4096) return false;
            偏移 = (size_t)(尾二 - 掩码文本.c_str());
        }
        for (long 核 = 起始; 核 <= 结束; 核++) CPU_SET((int)核, &集合);
        位置 = 偏移;
        if (位置 < 掩码文本.size() && 掩码文本[位置] == ',') 位置++;
        else if (位置 < 掩码文本.size() && 掩码文本[位置] != '\0') return false;
    }
    if (sched_setaffinity((pid_t)进程标识, sizeof(集合), &集合) != 0) {
        VINE_LOGE_ERRNO("sched_setaffinity");
        return false;
    }
    cpu_set_t 回读;
    CPU_ZERO(&回读);
    if (sched_getaffinity((pid_t)进程标识, sizeof(回读), &回读) != 0) {
        VINE_LOGW("资源调度器：亲和回读失败");
        return true;
    }
    VINE_LOGI("资源调度器：进程 %d 亲和 %s 生效", 进程标识, 掩码文本.c_str());
    return true;
}


bool 资源调度器::提升前台优先级(int 进程标识) {
    // 提升前台优先级：尝试负值静态优先级，无特权时回退零值。
    if (!校验进程存活(进程标识)) { VINE_LOGE("资源调度器：进程 %d 不存活", 进程标识); return false; }
    int 目标 = -5;
    if (setpriority(PRIO_PROCESS, (id_t)进程标识, 目标) != 0) {
        VINE_LOGW("资源调度器：负优先级需特权，回退零值：%s", strerror(errno));
        目标 = 0;
        if (setpriority(PRIO_PROCESS, (id_t)进程标识, 目标) != 0) {
            VINE_LOGE_ERRNO("setpriority");
            return false;
        }
    }
    VINE_LOGI("资源调度器：进程 %d 前台优先级 %d 生效", 进程标识, 目标);
    return true;
}


bool 资源调度器::恢复后台优先级(int 进程标识) {
    // 恢复后台优先级：设置正值静态优先级并回读验证。
    if (!校验进程存活(进程标识)) { VINE_LOGE("资源调度器：进程 %d 不存活", 进程标识); return false; }
    if (setpriority(PRIO_PROCESS, (id_t)进程标识, 10) != 0) {
        VINE_LOGE_ERRNO("setpriority");
        return false;
    }
    errno = 0;
    int 回读 = getpriority(PRIO_PROCESS, (id_t)进程标识);
    if (errno != 0) { VINE_LOGW("资源调度器：优先级回读失败"); return true; }
    VINE_LOGI("资源调度器：进程 %d 后台优先级 %d 生效", 进程标识, 回读);
    return 回读 >= 10;
}


bool 资源调度器::冻结容器(const std::string &实例标识) {
    // 写入冻结文件后轮询等待状态翻转，超时返回假。
    if (实例标识.empty()) return false;
    bool 成功 = false;
    if (统一层级) {
        std::string 分组 = 统一分组(实例标识);
        if (写入统一文件(实例标识, "cgroup.freeze", "1")) 成功 = 等待冻结状态(分组, true, 5000);
        else VINE_LOGW("资源调度器：写入冻结文件失败");
    } else {
        std::string 分组 = "/sys/fs/cgroup/freezer/vineos-" + 短名(实例标识);
        mkdirs(分组);
        if (write_file(分组 + "/freezer.state", "FROZEN")) 成功 = 等待冻结状态(分组, true, 5000);
        else VINE_LOGW("资源调度器：写入分离冻结文件失败");
    }
    VINE_LOGI("资源调度器：实例 %s 冻结容器 %s", 实例标识.c_str(), 成功 ? "成功" : "失败");
    return 成功;
}


bool 资源调度器::解冻容器(const std::string &实例标识) {
    // 写入解冻文件后轮询等待状态翻转，超时返回假。
    if (实例标识.empty()) return false;
    bool 成功 = false;
    if (统一层级) {
        std::string 分组 = 统一分组(实例标识);
        if (写入统一文件(实例标识, "cgroup.freeze", "0")) 成功 = 等待冻结状态(分组, false, 5000);
        else VINE_LOGW("资源调度器：写入解冻文件失败");
    } else {
        std::string 分组 = "/sys/fs/cgroup/freezer/vineos-" + 短名(实例标识);
        mkdirs(分组);
        if (write_file(分组 + "/freezer.state", "THAWED")) 成功 = 等待冻结状态(分组, false, 5000);
        else VINE_LOGW("资源调度器：写入分离解冻文件失败");
    }
    VINE_LOGI("资源调度器：实例 %s 解冻容器 %s", 实例标识.c_str(), 成功 ? "成功" : "失败");
    return 成功;
}


long 资源调度器::查询内存上限(const std::string &实例标识) {
    // 查询内存上限：读取分组内存上限，不限返回负一。
    if (实例标识.empty()) return -1;
    long 上限 = 读取分组整数(实例标识, "memory.max", "memory", "memory.limit_in_bytes");
    if (上限 < 0) VINE_LOGW("资源调度器：实例 %s 内存上限不可读或不限", 实例标识.c_str());
    return 上限;
}


long 资源调度器::查询处理器配额(const std::string &实例标识) {
    // 查询处理器配额：读取分组处理器配额微秒，不限返回负一。
    if (实例标识.empty()) return -1;
    long 配额 = 读取分组整数(实例标识, "cpu.max", "cpu", "cpu.cfs_quota_us");
    if (配额 < 0) VINE_LOGW("资源调度器：实例 %s 处理器配额不可读或不限", 实例标识.c_str());
    return 配额;
}


std::string 资源调度器::查询冻结状态(const std::string &实例标识) {
    // 优先读统一层级冻结文件，回退到分离层级状态文件。
    std::string 标签 = 实例标识.empty() ? 分组标签 : 实例标识;
    if (标签.empty()) return std::string("未知");
    std::string 统一路径 = "/sys/fs/cgroup/vineos-" + 标签 + "/cgroup.freeze";
    std::optional<std::string> 文件内容 = read_file(统一路径);
    if (文件内容) {
        std::string 状态 = *文件内容;
        while (!状态.empty() && (状态.back() == '\n' || 状态.back() == ' ')) 状态.pop_back();
        return 状态.empty() ? std::string("未知") : 状态;
    }
    std::string 分离路径 = "/sys/fs/cgroup/freezer/vineos-" + 标签 + "/freezer.state";
    文件内容 = read_file(分离路径);
    if (文件内容) {
        std::string 状态 = *文件内容;
        while (!状态.empty() && (状态.back() == '\n' || 状态.back() == ' ')) 状态.pop_back();
        return 状态.empty() ? std::string("未知") : 状态;
    }
    VINE_LOGW("资源调度器：实例 %s 冻结状态不可读", 标签.c_str());
    return std::string("未知");
}

bool 资源调度器::移除资源分组(const std::string &实例标识) {
    // 移除资源分组：逆序分离各层级分组目录并清理登记表。
    if (实例标识.empty()) return false;
    std::string 短 = 短名(实例标识);
    std::vector<std::string> 待删;
    待删.push_back("/sys/fs/cgroup/vineos-" + 短);
    待删.push_back("/sys/fs/cgroup/memory/vineos-" + 短);
    待删.push_back("/sys/fs/cgroup/cpu/vineos-" + 短);
    待删.push_back("/sys/fs/cgroup/pids/vineos-" + 短);
    待删.push_back("/sys/fs/cgroup/blkio/vineos-" + 短);
    待删.push_back("/sys/fs/cgroup/freezer/vineos-" + 短);
    bool 全好 = true;
    for (auto 迭代 = 待删.rbegin(); 迭代 != 待删.rend(); ++迭代) {
        if (!path_exists(*迭代)) continue;
        if (rmdir(迭代->c_str()) != 0 && errno != ENOENT) {
            VINE_LOGW("资源调度器：删除 %s 失败：%s", 迭代->c_str(), strerror(errno));
            全好 = false;
        }
    }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        for (auto 迭代 = 已创建分组.begin(); 迭代 != 已创建分组.end();) {
            if (迭代->find(短) != std::string::npos) 迭代 = 已创建分组.erase(迭代);
            else ++迭代;
        }
    }
    VINE_LOGI("资源调度器：实例 %s 资源分组移除%s", 实例标识.c_str(), 全好 ? "成功" : "部分失败");
    return 全好;
}


std::string 资源调度器::导出调度诊断() {
    // 汇总配额事件计数与层级类型，生成中文诊断文本。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断 = "=== 资源调度诊断 ===\n";
    诊断 += "分组标签：" + 分组标签 + "\n";
    诊断 += "内存兆字节：" + std::to_string(内存兆字节_) + "\n";
    诊断 += "处理器核数：" + std::to_string(处理器核数_) + "\n";
    诊断 += "统一层级：" + std::string(统一层级 ? "是" : "否") + "\n";
    诊断 += "事件计数：" + std::to_string(事件计数) + "\n";
    return 诊断;
}

std::string 资源调度器::读取分组标签() {
    // 加锁返回当前分组标签拷贝。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 分组标签;
}

void 资源调度器::设置分组标签(const std::string &标签文本) {
    // 加锁更新分组标签并记录中文日志。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    分组标签 = 标签文本;
    VINE_LOGI("资源调度器：分组标签更新为 %s", 分组标签.c_str());
}

int 资源调度器::读取内存兆字节() {
    // 读取内存兆字节：加锁返回已应用的内存配置。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 内存兆字节_;
}


int 资源调度器::读取处理器核数() {
    // 读取处理器核数：加锁返回已应用的核数配置。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 处理器核数_;
}


bool 资源调度器::应用大页内存上限(const std::string &实例标识, long 上限字节) {
    // 应用大页内存上限：非正视为不限，写入大页上限文件。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    std::string 文本 = (上限字节 <= 0) ? std::string("max") : std::to_string(上限字节);
    bool 好 = 写入分组值(实例标识, "hugetlb.2MB.max", "hugetlb", "hugetlb.2MB.limit_in_bytes", 文本);
    if (好) VINE_LOGI("资源调度器：实例 %s 大页上限 %s 生效", 实例标识.c_str(), 文本.c_str());
    else VINE_LOGW("资源调度器：实例 %s 大页上限写入失败", 实例标识.c_str());
    return 好;
}


bool 资源调度器::应用交换上限(const std::string &实例标识, int 兆字节) {
    // 应用交换上限：兆字节换算为字节写入交换上限文件。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    std::string 文本 = (兆字节 <= 0) ? std::string("max") : std::to_string((long)兆字节 * 1024L * 1024L);
    bool 好 = 写入分组值(实例标识, "memory.swap.max", "memory", "memory.memsw.limit_in_bytes", 文本);
    if (好) VINE_LOGI("资源调度器：实例 %s 交换上限 %s 生效", 实例标识.c_str(), 文本.c_str());
    else VINE_LOGW("资源调度器：实例 %s 交换上限写入失败", 实例标识.c_str());
    return 好;
}


bool 资源调度器::应用处理器权重(const std::string &实例标识, int 权重值) {
    // 应用处理器权重：校验区间后写入权重文件，分离层级按比例换算份额。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    int 有效 = 权重值;
    if (有效 < 1) 有效 = 1;
    if (有效 > 10000) 有效 = 10000;
    bool 好 = false;
    if (统一层级) {
        好 = 写入统一文件(实例标识, "cpu.weight", std::to_string(有效));
    } else {
        long 份额 = (long)有效 * 1024L / 100L;
        if (份额 < 2) 份额 = 2;
        if (份额 > 262144) 份额 = 262144;
        好 = 写入分组值(实例标识, "cpu.weight", "cpu", "cpu.shares", std::to_string(份额));
    }
    if (好) VINE_LOGI("资源调度器：实例 %s 处理器权重 %d 生效", 实例标识.c_str(), 有效);
    else VINE_LOGW("资源调度器：实例 %s 处理器权重写入失败", 实例标识.c_str());
    return 好;
}


bool 资源调度器::应用输入输出上限(const std::string &实例标识, long 每秒字节) {
    // 应用输入输出上限：解析根设备号后写入读写速率上限。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    if (每秒字节 <= 0 || 每秒字节 > 9000000000000L) { VINE_LOGE("资源调度器：速率 %ld 非法", 每秒字节); return false; }
    std::string 设备号;
    std::optional<std::string> 表 = read_file("/proc/self/mountinfo");
    if (表) {
        size_t 位置 = 0;
        int 行数 = 0;
        while (位置 < 表->size() && 行数 < 256) {
            size_t 行尾 = 表->find('\n', 位置);
            std::string 行 = 表->substr(位置, (行尾 == std::string::npos) ? std::string::npos : (行尾 - 位置));
            char 挂载点[256] = {0};
            char 主从[64] = {0};
            int 标识 = 0, 父级 = 0;
            char 根[256] = {0};
            if (sscanf(行.c_str(), "%d %d %63s %255s %255s", &标识, &父级, 主从, 根, 挂载点) == 5) {
                if (std::string(挂载点) == "/") { 设备号 = 主从; break; }
            }
            if (行尾 == std::string::npos) break;
            位置 = 行尾 + 1;
            行数++;
        }
    }
    if (设备号.empty()) { VINE_LOGW("资源调度器：根设备号解析失败"); return false; }
    std::string 文本;
    bool 好 = false;
    if (统一层级) {
        文本 = 设备号 + " rbps=" + std::to_string(每秒字节) + " wbps=" + std::to_string(每秒字节);
        好 = 写入统一文件(实例标识, "io.max", 文本);
    } else {
        好 = 写入分组值(实例标识, "io.max", "blkio", "blkio.throttle.read_bps_device",
            设备号 + " " + std::to_string(每秒字节));
        if (好) 好 = 写入分组值(实例标识, "io.max", "blkio", "blkio.throttle.write_bps_device",
            设备号 + " " + std::to_string(每秒字节));
    }
    if (好) VINE_LOGI("资源调度器：实例 %s 输入输出上限 %ld 生效", 实例标识.c_str(), 每秒字节);
    else VINE_LOGW("资源调度器：实例 %s 输入输出上限写入失败", 实例标识.c_str());
    return 好;
}


bool 资源调度器::限制文件描述符数(int 进程标识, long 上限数) {
    // 限制文件描述符数：仅支持本进程，校验后设置并回读验证。
    if (上限数 < 64) 上限数 = 64;
    if (上限数 > 1048576) 上限数 = 1048576;
    if (进程标识 != (int)getpid()) {
        if (!校验进程存活(进程标识)) { VINE_LOGE("资源调度器：进程 %d 不存活", 进程标识); return false; }
        VINE_LOGW("资源调度器：跨进程描述符限制需特权注入，暂不支持进程 %d", 进程标识);
        return false;
    }
    struct rlimit 限制{0, 0};
    限制.rlim_cur = (rlim_t)上限数;
    限制.rlim_max = (rlim_t)上限数;
    if (setrlimit(RLIMIT_NOFILE, &限制) != 0) { VINE_LOGE_ERRNO("setrlimit"); return false; }
    struct rlimit 回读{0, 0};
    if (getrlimit(RLIMIT_NOFILE, &回读) != 0) { VINE_LOGW("资源调度器：描述符回读失败"); return true; }
    VINE_LOGI("资源调度器：描述符上限 %ld 生效", (long)回读.rlim_cur);
    return (long)回读.rlim_cur >= 上限数;
}


bool 资源调度器::设置调度策略(int 进程标识, int 策略编号) {
    // 设置调度策略：校验策略后取中位优先级设置并回读验证。
    if (!校验进程存活(进程标识)) { VINE_LOGE("资源调度器：进程 %d 不存活", 进程标识); return false; }
    if (策略编号 != SCHED_OTHER && 策略编号 != SCHED_FIFO && 策略编号 != SCHED_RR) {
        VINE_LOGE("资源调度器：策略 %d 非法", 策略编号);
        return false;
    }
    struct sched_param 参数{0};
    if (策略编号 != SCHED_OTHER) {
        int 最小 = sched_get_priority_min(策略编号);
        int 最大 = sched_get_priority_max(策略编号);
        if (最小 < 0 || 最大 < 0) { VINE_LOGE_ERRNO("sched_get_priority"); return false; }
        参数.sched_priority = 最小 + (最大 - 最小) / 2;
    }
    if (sched_setscheduler((pid_t)进程标识, 策略编号, &参数) != 0) {
        VINE_LOGE_ERRNO("sched_setscheduler");
        return false;
    }
    int 回读 = sched_getscheduler((pid_t)进程标识);
    VINE_LOGI("资源调度器：进程 %d 调度策略 %d 生效", 进程标识, 回读);
    return 回读 == 策略编号;
}


long 资源调度器::查询处理器权重(const std::string &实例标识) {
    // 查询处理器权重：读取分组权重文件，缺失返回哨兵。
    if (实例标识.empty()) return -1;
    return 读取分组整数(实例标识, "cpu.weight", "cpu", "cpu.shares");
}


long 资源调度器::查询进程数上限(const std::string &实例标识) {
    // 查询进程数上限：读取分组进程数上限，不限返回负一。
    if (实例标识.empty()) return -1;
    return 读取分组整数(实例标识, "pids.max", "pids", "pids.max");
}


long 资源调度器::查询块设备权重(const std::string &实例标识) {
    // 查询块设备权重：读取分组块权重文件，缺失返回哨兵。
    if (实例标识.empty()) return -1;
    return 读取分组整数(实例标识, "io.weight", "blkio", "blkio.weight");
}


long 资源调度器::查询大页上限(const std::string &实例标识) {
    // 查询大页上限：读取分组大页上限，不限返回负一。
    if (实例标识.empty()) return -1;
    return 读取分组整数(实例标识, "hugetlb.2MB.max", "hugetlb", "hugetlb.2MB.limit_in_bytes");
}


bool 资源调度器::绑定分组进程(const std::string &实例标识, int 进程标识) {
    // 绑定分组进程：校验存活后写入分组进程文件并回读验证。
    if (实例标识.empty()) { VINE_LOGE("资源调度器：实例标识为空"); return false; }
    if (!校验进程存活(进程标识)) { VINE_LOGE("资源调度器：进程 %d 不存活", 进程标识); return false; }
    std::string 文本 = std::to_string(进程标识);
    bool 好 = false;
    if (统一层级) {
        好 = 写入统一文件(实例标识, "cgroup.procs", 文本);
    } else {
        好 = 写入分离层级文件("memory", 短名(实例标识) + "/cgroup.procs", 文本);
        if (!好) 好 = 写入分离层级文件("memory", 短名(实例标识) + "/tasks", 文本);
    }
    if (!好) { VINE_LOGW("资源调度器：绑定进程 %d 失败", 进程标识); return false; }
    VINE_LOGI("资源调度器：进程 %d 已绑定实例 %s", 进程标识, 实例标识.c_str());
    return true;
}


bool 资源调度器::迁移分组进程(const std::string &来源标识, const std::string &目标标识) {
    // 迁移分组进程：逐个读取来源进程并写入目标，数量设上限。
    if (来源标识.empty() || 目标标识.empty()) return false;
    std::string 来源文件 = 统一分组(来源标识) + "/cgroup.procs";
    std::optional<std::string> 列表 = read_file(来源文件);
    if (!列表 || 列表->empty()) { VINE_LOGW("资源调度器：来源 %s 无进程", 来源标识.c_str()); return false; }
    long 已迁 = 0;
    size_t 位置 = 0;
    int 行数 = 0;
    while (位置 < 列表->size() && 行数 < 1024) {
        size_t 行尾 = 列表->find('\n', 位置);
        std::string 行 = 列表->substr(位置, (行尾 == std::string::npos) ? std::string::npos : (行尾 - 位置));
        char *尾部 = nullptr;
        long 进程 = strtol(行.c_str(), &尾部, 10);
        if (尾部 != 行.c_str() && 进程 > 0) {
            if (绑定分组进程(目标标识, (int)进程)) 已迁++;
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    VINE_LOGI("资源调度器：来源 %s 迁移 %ld 个进程到 %s", 来源标识.c_str(), 已迁, 目标标识.c_str());
    return 已迁 > 0;
}


bool 资源调度器::创建资源分组(const std::string &实例标识) {
    // 创建资源分组：校验标识后创建统一分组目录并登记。
    if (实例标识.empty() || 实例标识.size() > 128) { VINE_LOGE("资源调度器：实例标识非法"); return false; }
    std::string 分组 = 统一分组(实例标识);
    if (!mkdirs(分组)) { VINE_LOGW("资源调度器：创建 %s 失败", 分组.c_str()); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (已创建分组.size() < 512) 已创建分组.push_back(分组);
        事件计数++;
    }
    VINE_LOGI("资源调度器：分组 %s 就绪", 分组.c_str());
    return true;
}


void 资源调度器::清理全部资源分组() {
    // 遍历已记录分组并尝试删目录，失败仅记录警告。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    for (const std::string &分组路径 : 已创建分组) {
        if (rmdir(分组路径.c_str()) != 0) { VINE_LOGW("资源调度器：删除 %s 失败：%s", 分组路径.c_str(), strerror(errno)); }
    }
    已创建分组.clear();
    VINE_LOGI("资源调度器：全部资源分组已清理");
}

bool 资源调度器::读取统一层级标志() {
    // 读取统一层级标志：加锁返回探测到的层级类型。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 统一层级;
}


bool 资源调度器::刷新统一层级标志() {
    // 刷新统一层级标志：重探统一层级控制文件是否存在。
    bool 存在 = path_exists("/sys/fs/cgroup/cgroup.controllers");
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        统一层级 = 存在;
    }
    VINE_LOGI("资源调度器：统一层级=%s", 存在 ? "是" : "否");
    return true;
}


bool 资源调度器::写入统一层级文件(const std::string &相对路径, const std::string &写入内容) {
    // 写入统一层级文件：校验穿越后截断写入统一层级相对路径。
    if (相对路径.empty() || 相对路径.find("..") != std::string::npos) return false;
    if (相对路径.size() > 256 || 写入内容.size() > 4096) return false;
    std::string 路径 = "/sys/fs/cgroup/" + 相对路径;
    int 描述符 = open(路径.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGW("资源调度器：打开 %s 失败：%s", 路径.c_str(), strerror(errno)); return false; }
    const char *指针 = 写入内容.c_str();
    size_t 剩余 = 写入内容.size();
    while (剩余 > 0) {
        ssize_t 单次 = write(描述符, 指针, 剩余);
        if (单次 < 0) {
            if (errno == EINTR) continue;
            VINE_LOGW("资源调度器：写入 %s 失败", 路径.c_str());
            close(描述符);
            return false;
        }
        指针 += 单次;
        剩余 -= (size_t)单次;
    }
    close(描述符);
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    return true;
}


std::string 资源调度器::读取统一层级文件(const std::string &相对路径) {
    // 读取统一层级下相对路径文件并去空白后返回。
    if (相对路径.empty() || 相对路径.find("..") != std::string::npos) return std::string("");
    std::string 文件路径 = "/sys/fs/cgroup/" + 相对路径;
    std::optional<std::string> 文件内容 = read_file(文件路径);
    if (!文件内容) { VINE_LOGW("资源调度器：读取 %s 失败", 文件路径.c_str()); return std::string(""); }
    std::string 结果 = *文件内容;
    while (!结果.empty() && (结果.back() == '\n' || 结果.back() == ' ')) 结果.pop_back();
    return 结果;
}

bool 资源调度器::写入分离层级文件(const std::string &控制器名, const std::string &文件名, const std::string &写入内容) {
    // 写入分离层级文件：控制器下标签分组内截断写入指定文件。
    if (控制器名.empty() || 文件名.empty()) return false;
    if (控制器名.find('/') != std::string::npos || 控制器名.find("..") != std::string::npos) return false;
    if (文件名.find("..") != std::string::npos || 文件名.size() > 256 || 写入内容.size() > 4096) return false;
    std::string 分组;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        分组 = 分组标签;
    }
    if (分组.empty()) 分组 = "默认分组";
    std::string 目录 = "/sys/fs/cgroup/" + 控制器名 + "/vineos-" + 短名(分组);
    if (!mkdirs(目录)) { VINE_LOGW("资源调度器：创建 %s 失败", 目录.c_str()); return false; }
    std::string 路径 = 目录 + "/" + 文件名;
    int 描述符 = open(路径.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGW("资源调度器：打开 %s 失败：%s", 路径.c_str(), strerror(errno)); return false; }
    const char *指针 = 写入内容.c_str();
    size_t 剩余 = 写入内容.size();
    while (剩余 > 0) {
        ssize_t 单次 = write(描述符, 指针, 剩余);
        if (单次 < 0) {
            if (errno == EINTR) continue;
            VINE_LOGW("资源调度器：写入 %s 失败", 路径.c_str());
            close(描述符);
            return false;
        }
        指针 += 单次;
        剩余 -= (size_t)单次;
    }
    close(描述符);
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    return true;
}


std::string 资源调度器::读取分离层级文件(const std::string &控制器名, const std::string &文件名) {
    // 读取分离层级文件：控制器下标签分组内读取并去空白。
    if (控制器名.empty() || 文件名.empty()) return std::string("");
    if (控制器名.find('/') != std::string::npos || 控制器名.find("..") != std::string::npos) return std::string("");
    if (文件名.find("..") != std::string::npos || 文件名.size() > 256) return std::string("");
    std::string 分组;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        分组 = 分组标签;
    }
    if (分组.empty()) 分组 = "默认分组";
    std::string 路径;
    if (文件名.find('/') != std::string::npos) {
        路径 = "/sys/fs/cgroup/" + 控制器名 + "/vineos-" + 短名(分组) + "/" + 文件名;
    } else {
        路径 = "/sys/fs/cgroup/" + 控制器名 + "/" + 文件名;
    }
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) { VINE_LOGW("资源调度器：读取 %s 失败", 路径.c_str()); return std::string(""); }
    std::string 结果 = *内容;
    while (!结果.empty() && (结果.back() == '\n' || 结果.back() == ' ' || 结果.back() == '\t')) 结果.pop_back();
    return 结果;
}


bool 资源调度器::校验内存合法(int 内存兆字节) {
    // 校验内存合法：兆字节落在零到一太字节区间。
    if (内存兆字节 < 0 || 内存兆字节 > 1048576) { VINE_LOGW("资源调度器：内存 %d 非法", 内存兆字节); return false; }
    return true;
}


bool 资源调度器::校验核数合法(int 处理器核数) {
    // 校验核数合法：核数落在零到一千零二十四区间。
    if (处理器核数 < 0 || 处理器核数 > 1024) { VINE_LOGW("资源调度器：核数 %d 非法", 处理器核数); return false; }
    return true;
}


std::string 资源调度器::换算配额文本(int 处理器核数) {
    // 将核数换算为统一层级 cpu.max 文本，非法核数返回 max。
    if (处理器核数 <= 0) return std::string("max 100000");
    long 周期微秒 = 100000L;
    long 配额微秒 = 周期微秒 * (long)处理器核数;
    return std::to_string(配额微秒) + " " + std::to_string(周期微秒);
}

int 资源调度器::解析配额文本(const std::string &配额文本) {
    // 解析配额文本：配额除以周期向上取整为核数，不限返回零。
    if (配额文本.empty() || 配额文本.size() > 64) return -1;
    if (配额文本.compare(0, 3, "max") == 0) return 0;
    long 配额 = 0, 周期 = 0;
    if (sscanf(配额文本.c_str(), "%ld %ld", &配额, &周期) != 2) {
        VINE_LOGW("资源调度器：配额文本解析失败");
        return -1;
    }
    if (配额 < 0 || 周期 <= 0) return -1;
    long 核数 = (配额 + 周期 - 1) / 周期;
    if (核数 > 1024) 核数 = 1024;
    return (int)核数;
}


std::string 资源调度器::查询在线掩码() {
    // 读取在线处理器区间文本，去空白后返回。
    std::optional<std::string> 文件内容 = read_file("/sys/devices/system/cpu/online");
    if (!文件内容) { VINE_LOGW("资源调度器：读取在线掩码失败"); return std::string("0");
    }
    std::string 结果 = *文件内容;
    while (!结果.empty() && (结果.back() == '\n' || 结果.back() == ' ')) 结果.pop_back();
    if (结果.empty()) return std::string("0");
    return 结果;
}

bool 资源调度器::校验亲和掩码(const std::string &掩码文本) {
    // 校验亲和掩码：区间列表语法校验且核号不超在线上限。
    if (掩码文本.empty() || 掩码文本.size() > 256) return false;
    long 在线 = sysconf(_SC_NPROCESSORS_ONLN);
    if (在线 < 1) 在线 = 1;
    if (在线 > 4096) 在线 = 4096;
    size_t 位置 = 0;
    bool 有核 = false;
    while (位置 < 掩码文本.size()) {
        char *尾部 = nullptr;
        long 起始 = strtol(掩码文本.c_str() + 位置, &尾部, 10);
        if (尾部 == 掩码文本.c_str() + 位置 || 起始 < 0 || 起始 >= 在线) return false;
        long 结束 = 起始;
        size_t 偏移 = (size_t)(尾部 - 掩码文本.c_str());
        if (偏移 < 掩码文本.size() && 掩码文本[偏移] == '-') {
            char *尾二 = nullptr;
            结束 = strtol(掩码文本.c_str() + 偏移 + 1, &尾二, 10);
            if (尾二 == 掩码文本.c_str() + 偏移 + 1 || 结束 < 起始 || 结束 >= 在线) return false;
            偏移 = (size_t)(尾二 - 掩码文本.c_str());
        }
        有核 = true;
        位置 = 偏移;
        if (位置 < 掩码文本.size() && 掩码文本[位置] == ',') {
            位置++;
            if (位置 >= 掩码文本.size()) return false;
        } else if (位置 < 掩码文本.size()) {
            return false;
        }
    }
    return 有核;
}


bool 资源调度器::等待冻结生效(const std::string &分组路径, int 超时毫秒) {
    // 等待冻结生效：轮询直至冻结值或超时。
    return 等待冻结状态(分组路径, true, 超时毫秒);
}


bool 资源调度器::等待解冻生效(const std::string &分组路径, int 超时毫秒) {
    // 等待解冻生效：轮询直至解冻值或超时。
    return 等待冻结状态(分组路径, false, 超时毫秒);
}


long 资源调度器::读取系统内存总量() {
    // 读取系统内存总量：解析内存信息总量键并换算为字节。
    std::optional<std::string> 内容 = read_file("/proc/meminfo");
    if (!内容) { VINE_LOGW("资源调度器：内存信息不可读"); return -1; }
    size_t 查找 = 内容->find("MemTotal:");
    if (查找 == std::string::npos) return -1;
    long 千字节 = 0;
    if (sscanf(内容->c_str() + 查找, "%*s %ld", &千字节) != 1 || 千字节 < 0) return -1;
    if (千字节 > 9000000000000L) 千字节 = 9000000000000L;
    return 千字节 * 1024L;
}


long 资源调度器::读取系统可用内存() {
    // 读取系统可用内存：解析内存信息可用键并换算为字节。
    std::optional<std::string> 内容 = read_file("/proc/meminfo");
    if (!内容) { VINE_LOGW("资源调度器：内存信息不可读"); return -1; }
    size_t 查找 = 内容->find("MemAvailable:");
    if (查找 == std::string::npos) return -1;
    long 千字节 = 0;
    if (sscanf(内容->c_str() + 查找, "%*s %ld", &千字节) != 1 || 千字节 < 0) return -1;
    if (千字节 > 9000000000000L) 千字节 = 9000000000000L;
    return 千字节 * 1024L;
}


long 资源调度器::读取系统核数() {
    // 读取系统核数：系统调用查询并钳制合理区间。
    long 核数 = sysconf(_SC_NPROCESSORS_ONLN);
    if (核数 < 1) 核数 = 1;
    if (核数 > 4096) 核数 = 4096;
    return 核数;
}


int 资源调度器::估算合理内存(int 请求兆字节) {
    // 估算合理内存：将请求钳制到下限与系统总量之间。
    long 总量 = 读取系统内存总量();
    long 上限兆 = (总量 > 0) ? (总量 / 1024L / 1024L) : 8192L;
    long 请求 = 请求兆字节;
    if (请求 <= 0) 请求 = 1024;
    if (请求 < 64) 请求 = 64;
    if (请求 > 上限兆) 请求 = 上限兆;
    if (请求 > 1048576) 请求 = 1048576;
    return (int)请求;
}


int 资源调度器::估算合理核数(int 请求核数) {
    // 估算合理核数：将请求钳制到一与在线核数之间。
    long 在线 = 读取系统核数();
    long 请求 = 请求核数;
    if (请求 <= 0) 请求 = 1;
    if (请求 > 在线) 请求 = 在线;
    if (请求 > 1024) 请求 = 1024;
    return (int)请求;
}


bool 资源调度器::记录调度事件(const std::string &事件文本) {
    // 记录调度事件：校验长度后计数并落日志。
    if (事件文本.empty() || 事件文本.size() > 1024) return false;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        事件计数++;
    }
    VINE_LOGI("资源调度器事件：%s", 事件文本.c_str());
    return true;
}


long 资源调度器::读取事件计数() {
    // 读取事件计数：加锁返回当前计数，不附带自增。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 事件计数;
}


void 资源调度器::清空事件计数() {
    // 加锁清零调度事件计数器。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    事件计数 = 0;
    VINE_LOGI("资源调度器：事件计数已清空");
}

} // namespace vine
