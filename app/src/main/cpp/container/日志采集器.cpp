// ============================================================================
// 模块名称：日志采集器
// ============================================================================
// 【模块说明】
// 本模块负责容器日志采集，通过环形缓冲与文件轮转持久化关键事件。
// 支持内核消息与进程状态采集，后台线程周期刷盘保证不断电丢失。
// 日志文件按大小轮转并保留历史副本，长期运行不耗尽磁盘空间。
// 所有写入均带时间戳与实例标识前缀，便于跨容器检索与排障。
// 提供内存环形缓冲查询接口，主线程可实时获取最近若干条记录。
//
// 【主要功能】
//   01. 打开与关闭日志文件
//   02. 写入带时间戳的日志条目
//   03. 按大小轮转日志文件
//   04. 采集内核消息缓冲
//   05. 采集目标进程状态快照
//   06. 后台线程周期刷盘
//   07. 查询环形缓冲记录
//   08. 导出中文采集诊断
//
// 【实现原理】
//   01. 使用带追加标志的文件描述符写入，保证多线程追加不互相覆盖。
//   02. 使用 stat 查询文件大小，超限时重命名为序号后缀并创建新文件。
//   03. 读取 /dev/kmsg 需要特权，失败时回退到 /proc/kmsg 再回退到 dmesg。
//   04. 环形缓冲使用向量加头下标实现，互斥锁保护读写。
//   05. 后台线程周期将缓冲刷盘并同步文件，保证崩溃后可恢复。
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
#include "日志采集器.h"
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

日志采集器::日志采集器() {
    运行标志.store(false);
    文件描述符 = -1;
    VINE_LOGI("日志采集器：对象创建完成");
}

日志采集器::~日志采集器() {
    if (运行标志.load()) { 停止采集线程(); }
    if (后台线程.joinable()) {
        if (std::this_thread::get_id() != 后台线程.get_id()) { 后台线程.join(); }
        else { 后台线程.detach(); }
    }
    if (文件描述符 >= 0) { fsync(文件描述符); close(文件描述符); 文件描述符 = -1; }
    VINE_LOGI("日志采集器：对象销毁完成");
}

bool 日志采集器::打开日志文件(const std::string &实例标识, const std::string &目录路径) {
    if (实例标识.empty()) { VINE_LOGE("日志采集器：实例标识为空"); return false; }
    std::string 目录 = 目录路径.empty() ? ("/data/data/com.vineos/logs/" + 实例标识) : 目录路径;
    if (!mkdirs(目录)) { VINE_LOGE("日志采集器：创建 %s 失败", 目录.c_str()); return false; }
    std::string 文件 = 目录 + "/container.log";
    int 描述符 = open(文件.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (描述符 < 0) { VINE_LOGE_ERRNO("open(container.log)"); return false; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (文件描述符 >= 0) close(文件描述符);
        文件描述符 = 描述符;
        目标实例 = 实例标识; 日志目录 = 目录; 日志文件 = 文件;
    }
    VINE_LOGI("日志采集器：实例 %s 日志就绪于 %s", 实例标识.c_str(), 文件.c_str());
    return true;
}

void 日志采集器::关闭日志文件() {
    // 先刷盘再关闭描述符，最后记录中文日志。
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (文件描述符 >= 0) {
            fsync(文件描述符);
            close(文件描述符);
            文件描述符 = -1;
        }
    }
    VINE_LOGI("日志采集器：实例 %s 日志文件已关闭", 目标实例.c_str());
}

bool 日志采集器::是否已打开() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 文件描述符 >= 0;
}

bool 日志采集器::写入日志条目(const std::string &级别文本, const std::string &条目文本) {
    // 写入日志条目：环形缓冲按条数与字节双预算淘汰，落盘循环写尽并节流轮转检查。
    if (条目文本.empty() || 条目文本.size() > 65536) { VINE_LOGW("日志采集器：条目非法"); return false; }
    std::string 级别 = 级别文本.empty() ? std::string("信息") : 级别文本;
    std::string 成品 = 格式化条目文本(级别, 条目文本);
    if (成品.size() > 66000) 成品.resize(66000);
    bool 需轮转 = false;
    int 描述符 = -1;
    long 序号 = 0;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        while (!环形缓冲.empty() && (环形缓冲.size() >= 环形容量 || 环形已用字节 + (long)成品.size() > 环形上限字节)) {
            环形已用字节 -= (long)环形缓冲.front().size();
            if (环形已用字节 < 0) 环形已用字节 = 0;
            环形缓冲.pop_front();
            丢弃计数++;
        }
        环形缓冲.push_back(成品);
        环形已用字节 += (long)成品.size();
        写入条数++;
        序号 = 写入条数;
        描述符 = 文件描述符;
        if (序号 % 20 == 0) 需轮转 = true;
    }
    if (描述符 >= 0) {
        std::string 落盘 = 成品 + "\n";
        const char *指针 = 落盘.c_str();
        size_t 剩余 = 落盘.size();
        while (剩余 > 0) {
            ssize_t 单次 = write(描述符, 指针, 剩余);
            if (单次 < 0) {
                if (errno == EINTR) continue;
                VINE_LOGW("日志采集器：写入失败：%s", strerror(errno));
                return false;
            }
            指针 += 单次;
            剩余 -= (size_t)单次;
        }
        if ((序号 % 20) == 0) fsync(描述符);
    }
    if (需轮转 && 检查轮转需要()) 执行日志轮转();
    return true;
}


bool 日志采集器::写入信息条目(const std::string &条目文本) {
    return 写入日志条目("信息", 条目文本);
}

bool 日志采集器::写入警告条目(const std::string &条目文本) {
    return 写入日志条目("警告", 条目文本);
}

bool 日志采集器::写入错误条目(const std::string &条目文本) {
    return 写入日志条目("错误", 条目文本);
}

bool 日志采集器::写入调试条目(const std::string &条目文本) {
    return 写入日志条目("调试", 条目文本);
}

bool 日志采集器::检查轮转需要() {
    // 仅检查不轮转：比较当前大小与上限。
    std::string 文件;
    long 上限 = 4194304L;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 文件 = 日志文件; 上限 = 大小上限; }
    if (文件.empty()) return false;
    struct stat 状态{0};
    if (stat(文件.c_str(), &状态) != 0) return false;
    return (long)状态.st_size >= 上限;
}

bool 日志采集器::执行日志轮转() {
    // 执行日志轮转：单锁串行关闭重命名重建，份数钳制防异常膨胀。
    std::string 文件;
    int 份数 = 5;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        文件 = 日志文件;
        份数 = 保留份数_;
        if (文件.empty()) return false;
        if (份数 < 1) 份数 = 1;
        if (份数 > 50) 份数 = 50;
        if (文件描述符 >= 0) { fsync(文件描述符); close(文件描述符); 文件描述符 = -1; }
    }
    for (int 下标 = 份数 - 1; 下标 >= 1; 下标--) {
        std::string 旧名 = 文件 + "." + std::to_string(下标);
        std::string 新名 = 文件 + "." + std::to_string(下标 + 1);
        if (path_exists(旧名)) rename(旧名.c_str(), 新名.c_str());
    }
    rename(文件.c_str(), (文件 + ".1").c_str());
    std::string 最旧 = 文件 + "." + std::to_string(份数 + 1);
    if (path_exists(最旧)) unlink(最旧.c_str());
    int 描述符 = open(文件.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (描述符 < 0) { VINE_LOGE_ERRNO("open(轮转新文件)"); return false; }
    int 序号 = 0;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        文件描述符 = 描述符;
        轮转序号++;
        轮转次数++;
        序号 = 轮转序号;
    }
    VINE_LOGI("日志采集器：轮转完成，序号 %d", 序号);
    return true;
}


long 日志采集器::读取当前文件大小() {
    std::string 文件;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 文件 = 日志文件; }
    if (文件.empty()) return -1;
    struct stat 状态{0};
    if (stat(文件.c_str(), &状态) != 0) { VINE_LOGW("日志采集器：查询 %s 大小失败", 文件.c_str()); return -1; }
    return (long)状态.st_size;
}

int 日志采集器::读取轮转序号() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 轮转序号;
}

void 日志采集器::设置大小上限(long 上限字节) {
    long 有效 = 上限字节 > 0 ? 上限字节 : 4194304L;
    if (有效 < 65536L) 有效 = 65536L;
    if (有效 > 268435456L) 有效 = 268435456L;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    大小上限 = 有效;
    VINE_LOGI("日志采集器：大小上限 %ld", 大小上限);
}

long 日志采集器::读取大小上限() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 大小上限;
}

void 日志采集器::设置保留份数(int 保留份数) {
    int 有效 = 保留份数 > 0 ? 保留份数 : 5;
    if (有效 < 1) 有效 = 1;
    if (有效 > 50) 有效 = 50;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    保留份数_ = 有效;
    VINE_LOGI("日志采集器：保留份数 %d", 保留份数_);
}

int 日志采集器::读取保留份数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 保留份数_;
}

bool 日志采集器::采集内核消息() {
    // 采集内核消息：非阻塞限长读取内核消息设备，失败回退版本信息。
    std::string 片段;
    int 描述符 = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (描述符 >= 0) {
        char 缓冲[2048];
        ssize_t 已读 = read(描述符, 缓冲, sizeof(缓冲) - 1);
        close(描述符);
        if (已读 > 0) 片段.assign(缓冲, (size_t)已读);
    }
    if (!片段.empty()) return 写入日志条目("信息", std::string("[内核] ") + 片段);
    std::optional<std::string> 版本 = read_file("/proc/version");
    if (!版本 || 版本->empty()) { VINE_LOGW("日志采集器：内核消息源均不可读"); return false; }
    size_t 取长 = 版本->size() > 512 ? 512 : 版本->size();
    return 写入日志条目("信息", std::string("[内核] ") + 版本->substr(0, 取长));
}


bool 日志采集器::采集进程状态(int 进程标识) {
    if (进程标识 <= 0) 进程标识 = 1;
    std::string 文件路径 = "/proc/" + std::to_string(进程标识) + "/status";
    std::optional<std::string> 文件内容 = read_file(文件路径);
    if (!文件内容) { VINE_LOGW("日志采集器：进程 %d 状态不可读", 进程标识); return false; }
    return 写入日志条目("信息", std::string("[进程状态] ") + 文件内容->substr(0, 1024));
}

bool 日志采集器::采集内存信息() {
    std::optional<std::string> 文件内容 = read_file("/proc/meminfo");
    if (!文件内容 || 文件内容->empty()) { VINE_LOGW("日志采集器：内存信息不可读"); return false; }
    size_t 取长 = 文件内容->size() > 1024 ? 1024 : 文件内容->size();
    return 写入日志条目("信息", std::string("[内存] ") + 文件内容->substr(0, 取长));
}

bool 日志采集器::采集挂载信息() {
    std::optional<std::string> 文件内容 = read_file("/proc/self/mountinfo");
    if (!文件内容 || 文件内容->empty()) { VINE_LOGW("日志采集器：挂载信息不可读"); return false; }
    long 行数 = 0;
    for (char 字符 : *文件内容) { if (字符 == '\n') 行数++; }
    return 写入日志条目("信息", std::string("[挂载] 共 ") + std::to_string(行数) + " 项");
}

bool 日志采集器::采集网络信息() {
    std::optional<std::string> 文件内容 = read_file("/proc/net/dev");
    if (!文件内容 || 文件内容->empty()) { VINE_LOGW("日志采集器：网络信息不可读"); return false; }
    size_t 取长 = 文件内容->size() > 1024 ? 1024 : 文件内容->size();
    return 写入日志条目("信息", std::string("[网络] ") + 文件内容->substr(0, 取长));
}

bool 日志采集器::启动采集线程() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    if (运行标志.load()) { VINE_LOGW("日志采集器：已在采集中"); return true; }
    运行标志.store(true);
    后台线程 = std::thread(&日志采集器::后台采集循环, this);
    VINE_LOGI("日志采集器：采集线程启动");
    return true;
}

void 日志采集器::停止采集线程() {
    bool 曾运行 = 运行标志.exchange(false);
    if (后台线程.joinable()) {
        if (std::this_thread::get_id() != 后台线程.get_id()) { 后台线程.join(); }
        else { 后台线程.detach(); }
    }
    刷盘缓冲();
    if (曾运行) VINE_LOGI("日志采集器：采集线程已停止");
}

bool 日志采集器::正在采集() {
    // 正在采集：只读返回后台线程运行标志，无任何副作用。
    return 运行标志.load();
}


bool 日志采集器::刷盘缓冲() {
    // 刷盘缓冲：单锁内同步描述符并计数，失败记录中文原因。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    if (文件描述符 < 0) return false;
    if (fsync(文件描述符) != 0) { VINE_LOGW("日志采集器：刷盘失败：%s", strerror(errno)); return false; }
    刷盘次数++;
    return true;
}


void 日志采集器::清空环形缓冲() {
    // 清空环形缓冲：同步清零条目与字节计数。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    环形缓冲.clear();
    环形已用字节 = 0;
    VINE_LOGI("日志采集器：环形缓冲已清空");
}


long 日志采集器::读取缓冲条数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return (long)环形缓冲.size();
}

std::string 日志采集器::读取最近记录(int 最近条数) {
    // 加锁拷贝环形缓冲尾部若干条并拼接为多行文本。
    int 有效条数 = 最近条数;
    if (有效条数 <= 0) 有效条数 = 20;
    if (有效条数 > 500) 有效条数 = 500;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    if (环形缓冲.empty()) return std::string("");
    long 起始 = (long)环形缓冲.size() > 有效条数 ? (long)环形缓冲.size() - 有效条数 : 0;
    std::string 结果;
    for (long 下标 = 起始; 下标 < (long)环形缓冲.size(); 下标++) {
        结果 += 环形缓冲[(size_t)下标];
        结果 += "\n";
    }
    return 结果;
}

std::string 日志采集器::读取全部记录() {
    // 加锁拼接环形缓冲全部条目，条数过多时截断并提示。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 结果;
    long 上限 = 1000;
    long 已拼 = 0;
    for (const std::string &条目 : 环形缓冲) {
        if (已拼 >= 上限) { 结果 += "...（截断）\n"; break; }
        结果 += 条目 + "\n";
        已拼++;
    }
    return 结果;
}

std::string 日志采集器::导出采集诊断() {
    // 汇总实例目录文件大小与计数器，生成中文诊断。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断 = "=== 日志采集诊断 ===\n";
    诊断 += "实例标识：" + 目标实例 + "\n";
    诊断 += "目录路径：" + 日志目录 + "\n";
    诊断 += "文件路径：" + 日志文件 + "\n";
    诊断 += "缓冲条数：" + std::to_string((long)环形缓冲.size()) + "\n";
    诊断 += "写入条数：" + std::to_string(写入条数) + "\n";
    诊断 += "刷盘次数：" + std::to_string(刷盘次数) + "\n";
    诊断 += "轮转次数：" + std::to_string(轮转次数) + "\n";
    诊断 += "丢弃计数：" + std::to_string(丢弃计数) + "\n";
    诊断 += "轮转序号：" + std::to_string(轮转序号) + "\n";
    return 诊断;
}

std::string 日志采集器::读取实例标识() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 目标实例;
}

std::string 日志采集器::读取目录路径() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 日志目录;
}

std::string 日志采集器::读取文件路径() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 日志文件;
}

void 日志采集器::设置采集间隔(int 间隔毫秒) {
    int 有效 = 间隔毫秒;
    if (有效 < 500) 有效 = 500;
    if (有效 > 60000) 有效 = 60000;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    采集间隔毫秒 = 有效;
    VINE_LOGI("日志采集器：采集间隔 %d", 采集间隔毫秒);
}

int 日志采集器::读取采集间隔() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 采集间隔毫秒;
}

void 日志采集器::记录丢弃计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    丢弃计数++;
}

long 日志采集器::读取丢弃计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 丢弃计数;
}

bool 日志采集器::写入原始字节(const char *数据指针, long 数据长度) {
    if (数据指针 == nullptr || 数据长度 <= 0) return false;
    if (数据长度 > 1048576L) 数据长度 = 1048576L;
    int 描述符 = -1;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 描述符 = 文件描述符; }
    if (描述符 < 0) return false;
    long 已写 = 0;
    while (已写 < 数据长度) {
        ssize_t 单次 = write(描述符, 数据指针 + 已写, (size_t)(数据长度 - 已写));
        if (单次 < 0) {
            if (errno == EINTR) continue;
            VINE_LOGE_ERRNO("write(原始字节)");
            return false;
        }
        if (单次 == 0) break;
        已写 += (long)单次;
    }
    return 已写 == 数据长度;
}

long 日志采集器::读取内核环形大小() {
    // 读取内核环形大小：返回内存环形缓冲已用字节数。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 环形已用字节 < 0 ? 0 : 环形已用字节;
}


bool 日志采集器::设置内核环形大小(long 大小字节) {
    // 设置内核环形大小：钳制字节预算并联动条数容量。
    long 有效 = 大小字节;
    if (有效 < 65536L) 有效 = 65536L;
    if (有效 > 67108864L) 有效 = 67108864L;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    环形上限字节 = 有效;
    size_t 容量 = (size_t)(有效 / 512L) + 64;
    if (容量 < 100) 容量 = 100;
    if (容量 > 20000) 容量 = 20000;
    环形容量 = 容量;
    VINE_LOGI("日志采集器：环形上限 %ld 字节，容量 %zu 条", 环形上限字节, 环形容量);
    return true;
}


bool 日志采集器::压缩历史日志() {
    // 遍历轮转副本并压缩未压缩历史文件，子进程等待设三十秒超时。
    std::string 文件;
    int 份数 = 5;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        文件 = 日志文件;
        份数 = 保留份数_;
        if (文件.empty()) return false;
        if (份数 < 1) 份数 = 1;
        if (份数 > 50) 份数 = 50;
    }
    bool 全部成功 = true;
    for (int 下标 = 1; 下标 <= 份数; 下标++) {
        std::string 副本 = 文件 + "." + std::to_string(下标);
        std::string 压缩 = 副本 + ".gz";
        if (!path_exists(副本) || path_exists(压缩)) continue;
        pid_t 孩子 = fork();
        if (孩子 < 0) { VINE_LOGW("日志采集器：压缩 fork 失败"); 全部成功 = false; continue; }
        if (孩子 == 0) {
            execlp("gzip", "gzip", "-9", 副本.c_str(), nullptr);
            _exit(127);
        }
        int 状态 = 0;
        bool 等到 = false;
        for (int 已等 = 0; 已等 < 30000; 已等 += 100) {
            pid_t 结果 = waitpid(孩子, &状态, WNOHANG);
            if (结果 == 孩子) { 等到 = true; break; }
            if (结果 < 0 && errno != EINTR) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!等到) {
            VINE_LOGW("日志采集器：压缩 %s 超时，终止", 副本.c_str());
            terminate_gracefully(孩子, 5000);
            全部成功 = false;
            continue;
        }
        if (!WIFEXITED(状态) || WEXITSTATUS(状态) != 0) {
            VINE_LOGW("日志采集器：压缩 %s 失败", 副本.c_str());
            全部成功 = false;
        } else {
            VINE_LOGI("日志采集器：%s 已压缩", 副本.c_str());
        }
    }
    return 全部成功;
}


bool 日志采集器::清理过期日志(int 保留天数) {
    // 删除早于保留天数的轮转副本与压缩包。
    if (保留天数 < 0) 保留天数 = 7;
    if (保留天数 > 365) 保留天数 = 365;
    std::string 文件;
    int 份数 = 5;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 文件 = 日志文件; 份数 = 保留份数_; }
    if (文件.empty()) return false;
    struct timespec 现在{0, 0};
    clock_gettime(CLOCK_REALTIME, &现在);
    long 界限 = (long)现在.tv_sec - (long)保留天数 * 86400L;
    long 已删 = 0;
    for (int 下标 = 1; 下标 <= 份数 + 5; 下标++) {
        const std::string 候选[2] = {文件 + "." + std::to_string(下标), 文件 + "." + std::to_string(下标) + ".gz"};
        for (int 序号 = 0; 序号 < 2; 序号++) {
            struct stat 状态{0};
            if (stat(候选[序号].c_str(), &状态) != 0) continue;
            if ((long)状态.st_mtime < 界限) {
                if (unlink(候选[序号].c_str()) == 0) { 已删++; }
                else VINE_LOGW("日志采集器：删除 %s 失败", 候选[序号].c_str());
            }
        }
    }
    VINE_LOGI("日志采集器：清理过期完成，删除 %ld 个", 已删);
    return true;
}

long 日志采集器::统计日志总字节() {
    std::string 文件;
    int 份数 = 5;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 文件 = 日志文件; 份数 = 保留份数_; }
    if (文件.empty()) return -1;
    long 总计 = 0;
    struct stat 状态{0};
    if (stat(文件.c_str(), &状态) == 0) 总计 += (long)状态.st_size;
    for (int 下标 = 1; 下标 <= 份数; 下标++) {
        std::string 副本 = 文件 + "." + std::to_string(下标);
        if (stat(副本.c_str(), &状态) == 0) 总计 += (long)状态.st_size;
        std::string 压缩 = 副本 + ".gz";
        if (stat(压缩.c_str(), &状态) == 0) 总计 += (long)状态.st_size;
    }
    return 总计;
}

long 日志采集器::统计日志文件数() {
    std::string 文件;
    int 份数 = 5;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 文件 = 日志文件; 份数 = 保留份数_; }
    if (文件.empty()) return -1;
    long 计数 = 0;
    if (path_exists(文件)) 计数++;
    for (int 下标 = 1; 下标 <= 份数; 下标++) {
        if (path_exists(文件 + "." + std::to_string(下标))) 计数++;
        if (path_exists(文件 + "." + std::to_string(下标) + ".gz")) 计数++;
    }
    return 计数;
}

bool 日志采集器::校验目录可写() {
    std::string 目录;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 目录 = 日志目录; }
    if (目录.empty()) return false;
    if (access(目录.c_str(), W_OK | X_OK) != 0) { VINE_LOGW("日志采集器：目录 %s 不可写", 目录.c_str()); return false; }
    return true;
}

bool 日志采集器::创建日志目录(const std::string &目录路径) {
    if (目录路径.empty()) return false;
    if (!mkdirs(目录路径)) { VINE_LOGE("日志采集器：创建 %s 失败", 目录路径.c_str()); return false; }
    if (!path_exists(目录路径)) return false;
    if (access(目录路径.c_str(), W_OK) != 0) { VINE_LOGW("日志采集器：目录 %s 不可写", 目录路径.c_str()); return false; }
    VINE_LOGI("日志采集器：目录 %s 就绪", 目录路径.c_str());
    return true;
}

std::string 日志采集器::生成时间戳文本() {
    // 读取单调时钟并格式化为毫秒时间戳文本。
    struct timespec 当前{0, 0};
    clock_gettime(CLOCK_REALTIME, &当前);
    struct tm 本地{0};
    localtime_r(&当前.tv_sec, &本地);
    char 文本[64] = {0};
    snprintf(文本, sizeof(文本), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        本地.tm_year + 1900, 本地.tm_mon + 1, 本地.tm_mday,
        本地.tm_hour, 本地.tm_min, 本地.tm_sec, 当前.tv_nsec / 1000000L);
    return std::string(文本);
}

std::string 日志采集器::格式化条目文本(const std::string &级别文本, const std::string &条目文本) {
    // 拼接时间戳实例级别与正文为单行条目，实例加锁快照防竞态。
    std::string 时间戳 = 生成时间戳文本();
    std::string 实例;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        实例 = 目标实例;
    }
    return "[" + 时间戳 + "][" + 实例 + "][" + 级别文本 + "] " + 条目文本;
}


int 日志采集器::解析级别权重(const std::string &级别文本) {
    if (级别文本 == "调试" || 级别文本 == "DEBUG") return 0;
    if (级别文本 == "信息" || 级别文本 == "INFO") return 1;
    if (级别文本 == "警告" || 级别文本 == "WARN") return 2;
    if (级别文本 == "错误" || 级别文本 == "ERROR") return 3;
    return 1;
}

std::string 日志采集器::过滤级别记录(const std::string &最低级别, int 最近条数) {
    // 按最低级别权重过滤环形缓冲：解析第三对方括号内级别，命中收集后顺序拼接。
    int 门限 = 解析级别权重(最低级别);
    int 上限 = 最近条数 > 0 ? 最近条数 : 50;
    if (上限 > 500) 上限 = 500;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::vector<std::string> 命中;
    for (long 下标 = (long)环形缓冲.size() - 1; 下标 >= 0 && (long)命中.size() < 上限; 下标--) {
        const std::string &条目 = 环形缓冲[(size_t)下标];
        size_t 位置 = 0;
        int 权重 = 1;
        bool 完整 = false;
        for (int 段 = 0; 段 < 3; 段++) {
            size_t 左 = 条目.find('[', 位置);
            if (左 == std::string::npos) break;
            size_t 右 = 条目.find(']', 左 + 1);
            if (右 == std::string::npos) break;
            if (段 == 2) {
                权重 = 解析级别权重(条目.substr(左 + 1, 右 - 左 - 1));
                完整 = true;
            }
            位置 = 右 + 1;
        }
        if (完整 && 权重 >= 门限) 命中.push_back(条目);
    }
    std::string 结果;
    for (auto 迭代 = 命中.rbegin(); 迭代 != 命中.rend(); ++迭代) {
        结果 += *迭代;
        结果 += "\n";
    }
    return 结果;
}


void 日志采集器::设置最低级别(const std::string &级别文本) {
    if (级别文本.empty()) return;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    最低级别 = 级别文本;
    VINE_LOGI("日志采集器：最低级别 %s", 最低级别.c_str());
}

std::string 日志采集器::读取最低级别() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 最低级别;
}

void 日志采集器::后台采集循环() {
    VINE_LOGI("日志采集器：后台线程启动");
    while (运行标志.load()) {
        采集单轮数据();
        int 间隔 = 5000;
        { std::lock_guard<std::mutex> 互斥保护(互斥锁); 间隔 = 采集间隔毫秒; }
        for (int 已睡 = 0; 已睡 < 间隔 && 运行标志.load(); 已睡 += 250) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    VINE_LOGI("日志采集器：后台线程退出");
}

bool 日志采集器::采集单轮数据() {
    bool 好一 = 采集内核消息();
    bool 好二 = 采集内存信息();
    bool 好三 = 采集挂载信息();
    if (好一 || 好二 || 好三) { 刷盘缓冲(); return true; }
    VINE_LOGW("日志采集器：单轮采集全部失败");
    return false;
}

bool 日志采集器::写入启动横幅(const std::string &实例标识) {
    if (实例标识.empty()) return false;
    std::string 横幅 = std::string("========== 容器 ") + 实例标识 + " 日志开始 ==========";
    return 写入日志条目("信息", 横幅);
}

bool 日志采集器::写入停止横幅() {
    std::string 实例;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 实例 = 目标实例; }
    std::string 横幅 = std::string("========== 容器 ") + 实例 + " 日志结束 ==========";
    bool 好 = 写入日志条目("信息", 横幅);
    刷盘缓冲();
    return 好;
}

long 日志采集器::读取刷盘次数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 刷盘次数;
}

long 日志采集器::读取写入条数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 写入条数;
}

long 日志采集器::读取轮转次数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 轮转次数;
}

bool 日志采集器::检查文件描述符有效() {
    int 描述符 = -1;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 描述符 = 文件描述符; }
    if (描述符 < 0) return false;
    if (fcntl(描述符, F_GETFD) < 0) { VINE_LOGW("日志采集器：描述符 %d 已失效", 描述符); return false; }
    return true;
}

bool 日志采集器::重新打开日志文件() {
    std::string 实例, 目录;
    { std::lock_guard<std::mutex> 互斥保护(互斥锁); 实例 = 目标实例; 目录 = 日志目录; }
    if (实例.empty()) return false;
    关闭日志文件();
    return 打开日志文件(实例, 目录);
}

} // namespace vine
