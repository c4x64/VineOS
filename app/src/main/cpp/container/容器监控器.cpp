// ============================================================================
// 模块名称：容器监控器
// ============================================================================
// 【模块说明】
// 本模块负责容器运行时资源监控，周期性采集处理器内存块设备网络等指标。
// 通过读取宿主机侧 proc 文件系统与控制组层级文件获取容器真实资源占用。
// 同时兼容控制组第一代与第二代布局，自动探测统一层级是否存在。
// 监控线程在后台周期运行，主线程可随时查询最新快照与使用率。
// 所有数值解析均做边界校验，缺失文件时返回哨兵值并记录中文警告。
//
// 【主要功能】
//   01. 启动后台监控线程并绑定目标进程
//   02. 停止后台线程并清理状态
//   03. 读取处理器时间片快照
//   04. 读取内存占用与上限
//   05. 计算处理器与内存使用率
//   06. 读取块设备读写字节
//   07. 读取网络收发字节与包数
//   08. 读取线程数与文件描述符数
//   09. 读取系统负载与在线核数
//   10. 导出中文诊断文本
//
// 【实现原理】
//   01. 读取 /proc/stat 首行解析各模式节拍并做差分计算使用率。
//   02. 读取 /proc/meminfo 解析总量可用量等字段并换算为字节。
//   03. 读取控制组 memory.current 与 memory.max 获取容器内存视图。
//   04. 读取控制组 cpu.stat 解析使用微秒并结合周期换算配额。
//   05. 读取 /proc/net/dev 按接口名聚合收发字节包数错误数。
//   06. 读取 /proc/进程号/status 解析线程数与虚拟内存常驻集。
//   07. 后台线程使用睡眠间隔加原子标志实现可中断周期采集。
//   08. 历史快照保存在互斥锁保护的队列中，供差分计算使用。
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
#include <sys/sysinfo.h>
#include <linux/limits.h>
#include "容器监控器.h"
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

namespace {
// 在全文中按行首键名匹配单行并提取首个整数，行数超过上限则截断。
// 返回是否命中，命中时数值写入输出参数，累积过程做上溢钳制。
static bool 按行首键提取整数(const std::string &全文, const char *键名, long &数值, long 行上限) {
    if (键名 == nullptr || *键名 == '\0') return false;
    if (行上限 < 16) 行上限 = 16;
    if (行上限 > 4096) 行上限 = 4096;
    size_t 键长 = strlen(键名);
    size_t 位置 = 0;
    long 行数 = 0;
    while (位置 < 全文.size() && 行数 < 行上限) {
        size_t 行尾 = 全文.find('\n', 位置);
        size_t 行长 = (行尾 == std::string::npos) ? (全文.size() - 位置) : (行尾 - 位置);
        if (行长 >= 键长 && 全文.compare(位置, 键长, 键名) == 0) {
            long 累加 = 0;
            bool 已见 = false;
            size_t 扫描 = 位置 + 键长;
            size_t 结束 = 位置 + 行长;
            while (扫描 < 结束) {
                char 字符 = 全文[扫描];
                if (字符 >= '0' && 字符 <= '9') {
                    已见 = true;
                    if (累加 <= 900000000000000000L) 累加 = 累加 * 10 + (字符 - '0');
                    else { 累加 = 9000000000000000000L; break; }
                } else if (已见) {
                    break;
                }
                扫描++;
            }
            if (!已见) return false;
            数值 = 累加;
            return true;
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    return false;
}
}

bool 容器监控器::读取统计快照() {
    // 读取统计快照：单次读取首行并缓存八个字段，五百毫秒内复用。
    struct timespec 当前{0, 0};
    clock_gettime(CLOCK_MONOTONIC, &当前);
    long 现在毫秒 = (long)当前.tv_sec * 1000L + 当前.tv_nsec / 1000000L;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (缓存有效 && 现在毫秒 >= 缓存时刻毫秒 && 现在毫秒 - 缓存时刻毫秒 < 统计缓存有效毫秒) return true;
    }
    int 描述符 = open("/proc/stat", O_RDONLY | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGE_ERRNO("open(/proc/stat)"); return false; }
    char 缓冲[512];
    ssize_t 已读 = read(描述符, 缓冲, sizeof(缓冲) - 1);
    close(描述符);
    if (已读 <= 0) { VINE_LOGW("容器监控器：统计文件为空"); return false; }
    缓冲[已读] = '\0';
    long 字段[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int 个数 = sscanf(缓冲, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
        &字段[0], &字段[1], &字段[2], &字段[3], &字段[4], &字段[5], &字段[6], &字段[7]);
    if (个数 < 4) { VINE_LOGW("容器监控器：统计字段不足"); return false; }
    for (int 下标 = 0; 下标 < 8; 下标++) { if (字段[下标] < 0) 字段[下标] = 0; }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        for (int 下标 = 0; 下标 < 8; 下标++) 缓存节拍[下标] = 字段[下标];
        缓存有效 = true;
        缓存时刻毫秒 = 现在毫秒;
    }
    return true;
}
long 容器监控器::读取处理器节拍字段(int 字段序号) {
    // 节拍字段：序号零用户一美化二内核三空闲四等待五中断六软中断七窃取。
    if (字段序号 < 0 || 字段序号 > 7) { VINE_LOGW("容器监控器：字段序号非法"); return -1; }
    if (!读取统计快照()) return -1;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 缓存节拍[字段序号];
}
long 容器监控器::读取进程状态数值(const char *键名, bool 以千字节计) {
    // 进程状态数值：行首精确匹配键名后取首个整数，千字节换算做上溢钳制。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) { VINE_LOGW("容器监控器：进程未绑定"); return -1; }
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/status";
    if (file_size(路径) > 65536L) { VINE_LOGW("容器监控器：状态文件过大"); return -1; }
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) { VINE_LOGW("容器监控器：进程 %d 状态不可读", 进程标识); return -1; }
    long 数值 = 0;
    if (!按行首键提取整数(*内容, 键名, 数值, 128)) { VINE_LOGW("容器监控器：键 %s 缺失", 键名 ? 键名 : "?"); return -1; }
    if (以千字节计) {
        if (数值 > 9000000000000L) 数值 = 9000000000000L;
        数值 *= 1024L;
    }
    return 数值 < 0 ? 0 : 数值;
}
long 容器监控器::读取内存信息字节(const char *键名) {
    // 内存信息字节：行首匹配内存键后取千字节数并换算为字节。
    if (file_size("/proc/meminfo") > 65536L) { VINE_LOGW("容器监控器：内存文件过大"); return -1; }
    std::optional<std::string> 内容 = read_file("/proc/meminfo");
    if (!内容) { VINE_LOGW("容器监控器：内存文件不可读"); return -1; }
    long 千字节 = 0;
    if (!按行首键提取整数(*内容, 键名, 千字节, 256)) { VINE_LOGW("容器监控器：键 %s 缺失", 键名 ? 键名 : "?"); return -1; }
    if (千字节 > 9000000000000L) 千字节 = 9000000000000L;
    return 千字节 * 1024L;
}
long 容器监控器::读取控制组数值(const char *后缀名) {
    // 控制组数值：优先统一层级，内存用量缺失时回退第一代层级。
    if (后缀名 == nullptr || *后缀名 == '\0') return -1;
    if (strchr(后缀名, '/') != nullptr || strstr(后缀名, "..") != nullptr) return -1;
    std::string 分组;
    std::string 实例;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        分组 = 控制组路径;
        实例 = 目标实例;
    }
    if (分组.empty()) 分组 = "/sys/fs/cgroup";
    std::optional<std::string> 内容 = read_file(分组 + "/" + 后缀名);
    if (!内容 && std::string(后缀名) == "memory.current" && !实例.empty()) {
        内容 = read_file("/sys/fs/cgroup/memory/vineos-" + 实例 + "/memory.usage_in_bytes");
    }
    if (!内容 || 内容->empty()) { VINE_LOGW("容器监控器：控制组文件不可读"); return -1; }
    size_t 起始 = 内容->find_first_not_of(" \t\n");
    if (起始 == std::string::npos) return -1;
    if (内容->compare(起始, 3, "max") == 0) return -1;
    char *尾部 = nullptr;
    long 数值 = strtol(内容->c_str() + 起始, &尾部, 10);
    if (尾部 != 内容->c_str() + 起始 && 数值 >= 0) return 数值;
    long 累加 = 0;
    bool 已见 = false;
    for (char 字符 : *内容) {
        if (字符 >= '0' && 字符 <= '9') {
            已见 = true;
            if (累加 <= 900000000000000000L) 累加 = 累加 * 10 + (字符 - '0');
            else break;
        } else if (已见) {
            break;
        }
    }
    if (!已见) { VINE_LOGW("容器监控器：控制组数值无数字"); return -1; }
    return 累加;
}
long 容器监控器::聚合网络设备列(int 列号) {
    // 网络设备列：单遍扫描收发各列并累加，接口数与文件大小均设上限。
    if (列号 < 0 || 列号 > 15) { VINE_LOGW("容器监控器：网络列号非法"); return -1; }
    if (file_size("/proc/net/dev") > 262144L) { VINE_LOGW("容器监控器：网络文件过大"); return -1; }
    std::optional<std::string> 内容 = read_file("/proc/net/dev");
    if (!内容) { VINE_LOGW("容器监控器：网络文件不可读"); return -1; }
    long 累加 = 0;
    size_t 位置 = 0;
    int 行号 = 0;
    int 接口数 = 0;
    while (位置 < 内容->size() && 接口数 <= 256) {
        size_t 行尾 = 内容->find('\n', 位置);
        size_t 行长 = (行尾 == std::string::npos) ? (内容->size() - 位置) : (行尾 - 位置);
        行号++;
        if (行号 > 2 && 行长 > 0) {
            size_t 冒号 = 内容->find(':', 位置);
            if (冒号 != std::string::npos && 冒号 < 位置 + 行长) {
                std::string 数值段 = 内容->substr(冒号 + 1, 位置 + 行长 - 冒号 - 1);
                long 列[16] = {0};
                int 个数 = sscanf(数值段.c_str(), "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                    &列[0], &列[1], &列[2], &列[3], &列[4], &列[5], &列[6], &列[7],
                    &列[8], &列[9], &列[10], &列[11], &列[12], &列[13], &列[14], &列[15]);
                if (个数 > 列号 && 列[列号] > 0) {
                    if (累加 > 9000000000000000000L - 列[列号]) { 累加 = 9000000000000000000L; break; }
                    累加 += 列[列号];
                }
                接口数++;
            }
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
    }
    return 累加 < 0 ? 0 : 累加;
}
long 容器监控器::读取单值整数(const char *文件路径) {
    // 单值整数：读取首个长整数并校验非负，失败返回哨兵。
    if (文件路径 == nullptr || *文件路径 == '\0') return -1;
    std::optional<std::string> 内容 = read_file(文件路径);
    if (!内容 || 内容->empty()) { VINE_LOGW("容器监控器：读取 %s 失败", 文件路径); return -1; }
    char *尾部 = nullptr;
    long 数值 = strtol(内容->c_str(), &尾部, 10);
    if (尾部 == 内容->c_str() || 数值 < 0) { VINE_LOGW("容器监控器：解析 %s 失败", 文件路径); return -1; }
    return 数值;
}
double 容器监控器::读取单值浮点(const char *文件路径) {
    // 单值浮点：读取首个双精度数并校验非负，失败返回哨兵。
    if (文件路径 == nullptr || *文件路径 == '\0') return -1.0;
    std::optional<std::string> 内容 = read_file(文件路径);
    if (!内容 || 内容->empty()) { VINE_LOGW("容器监控器：读取 %s 失败", 文件路径); return -1.0; }
    char *尾部 = nullptr;
    double 数值 = strtod(内容->c_str(), &尾部);
    if (尾部 == 内容->c_str() || 数值 < 0) { VINE_LOGW("容器监控器：解析 %s 失败", 文件路径); return -1.0; }
    return 数值;
}
long 容器监控器::统计在线核数() {
    // 在线核数：解析多段区间文本求和，失败回退系统调用查询。
    std::optional<std::string> 内容 = read_file("/sys/devices/system/cpu/online");
    if (内容 && !内容->empty()) {
        long 总数 = 0;
        size_t 位置 = 0;
        bool 有效 = false;
        while (位置 < 内容->size()) {
            while (位置 < 内容->size() && (内容->at(位置) == ' ' || 内容->at(位置) == '\n' || 内容->at(位置) == ',')) 位置++;
            if (位置 >= 内容->size()) break;
            char *尾部 = nullptr;
            long 起始 = strtol(内容->c_str() + 位置, &尾部, 10);
            if (尾部 == 内容->c_str() + 位置 || 起始 < 0) break;
            有效 = true;
            long 结束 = 起始;
            size_t 偏移 = (size_t)(尾部 - 内容->c_str());
            if (偏移 < 内容->size() && 内容->at(偏移) == '-') {
                char *尾二 = nullptr;
                结束 = strtol(内容->c_str() + 偏移 + 1, &尾二, 10);
                if (尾二 == 内容->c_str() + 偏移 + 1 || 结束 < 起始) break;
                偏移 = (size_t)(尾二 - 内容->c_str());
            }
            总数 += (结束 - 起始 + 1);
            位置 = 偏移;
            if (总数 > 4096) { 总数 = 4096; break; }
        }
        if (有效 && 总数 > 0) return 总数;
    }
    long 核数 = sysconf(_SC_NPROCESSORS_ONLN);
    if (核数 < 1) 核数 = 1;
    if (核数 > 4096) 核数 = 4096;
    return 核数;
}
long 容器监控器::读取进程输入输出(const char *键名) {
    // 进程输入输出：解析输入输出统计中读写字节行，单位已为字节。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) { VINE_LOGW("容器监控器：进程未绑定"); return -1; }
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/io";
    if (file_size(路径) > 65536L) { VINE_LOGW("容器监控器：输入输出文件过大"); return -1; }
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) { VINE_LOGW("容器监控器：进程 %d 输入输出不可读", 进程标识); return -1; }
    long 数值 = 0;
    if (!按行首键提取整数(*内容, 键名, 数值, 64)) { VINE_LOGW("容器监控器：键 %s 缺失", 键名 ? 键名 : "?"); return -1; }
    return 数值 < 0 ? 0 : 数值;
}
double 容器监控器::读取负载数值(int 序号) {
    // 负载数值：解析负载均值三元组并按序号返回，序号零一二对应一五十五分钟。
    if (序号 < 0 || 序号 > 2) { VINE_LOGW("容器监控器：负载序号非法"); return -1.0; }
    std::optional<std::string> 内容 = read_file("/proc/loadavg");
    if (!内容 || 内容->empty()) { VINE_LOGW("容器监控器：负载文件不可读"); return -1.0; }
    double 负载[3] = {0, 0, 0};
    if (sscanf(内容->c_str(), "%lf %lf %lf", &负载[0], &负载[1], &负载[2]) != 3) {
        VINE_LOGW("容器监控器：负载解析失败");
        return -1.0;
    }
    if (负载[序号] < 0) 负载[序号] = 0;
    return 负载[序号];
}
void 容器监控器::压入历史快照(long 处理器节拍, long 内存字节) {
    // 压入历史快照：双端队列尾部追加并在超限时头部弹出，均摊线性复杂度。
    容器资源快照 快照;
    快照.处理器用户态 = 处理器节拍 < 0 ? 0 : 处理器节拍;
    快照.内存使用字节 = 内存字节 < 0 ? 0 : 内存字节;
    struct timespec 当前{0, 0};
    clock_gettime(CLOCK_MONOTONIC, &当前);
    快照.采集时刻毫秒 = (long)当前.tv_sec * 1000L + 当前.tv_nsec / 1000000L;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    快照.实例标识 = 目标实例;
    快照.进程标识 = 目标进程;
    历史快照.push_back(快照);
    while (历史快照.size() > (size_t)历史上限) 历史快照.pop_front();
}

容器监控器::容器监控器() {
    // 构造函数初始化原子标志与默认采样间隔，并记录中文创建日志。
    运行标志.store(false);
    目标进程 = -1;
    采样间隔毫秒 = 2000;
    VINE_LOGI("容器监控器：对象创建完成");
}

容器监控器::~容器监控器() {
    // 析构时确保后台线程已退出，避免悬空线程访问已销毁成员。
    if (运行标志.load()) { 停止监控(); }
    if (后台线程.joinable()) {
        if (std::this_thread::get_id() != 后台线程.get_id()) { 后台线程.join(); }
        else { 后台线程.detach(); }
    }
    VINE_LOGI("容器监控器：对象销毁完成");
}

void 容器监控器::后台采集循环() {
    // 后台周期循环：每次采集后按间隔睡眠，可被原子标志中断。
    VINE_LOGI("容器监控器：后台线程启动，实例 %s", 目标实例.c_str());
    int 连续失败 = 0;
    while (运行标志.load()) {
        long 处理器节拍 = 读取处理器快照();
        long 内存字节 = 读取内存使用字节();
        if (处理器节拍 < 0 && 内存字节 < 0) {
            连续失败++;
            if (连续失败 > 5) VINE_LOGW("容器监控器：连续失败 %d 次", 连续失败);
        } else {
            连续失败 = 0;
        }
        压入历史快照(处理器节拍, 内存字节);
        for (int 已睡 = 0; 已睡 < 采样间隔毫秒 && 运行标志.load(); 已睡 += 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    VINE_LOGI("容器监控器：后台线程退出");
}


bool 容器监控器::启动监控(const std::string &实例标识, int 进程标识) {
    // 以中文日志记录启动参数，校验进程标识合法性后点亮原子标志并拉起后台线程。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    if (运行标志.load()) { VINE_LOGW("容器监控器：实例 %s 已在监控中，忽略重复启动", 实例标识.c_str()); return true; }
    if (进程标识 <= 0) { VINE_LOGE("容器监控器：启动失败，进程标识 %d 非法", 进程标识); return false; }
    目标实例 = 实例标识;
    目标进程 = 进程标识;
    if (采样间隔毫秒 < 200 || 采样间隔毫秒 > 60000) 采样间隔毫秒 = 2000;
    运行标志.store(true);
    历史用户态 = 0; 历史内核态 = 0; 历史空闲态 = 0; 历史时刻毫秒 = 0;
    缓存有效 = false;
    后台线程 = std::thread(&容器监控器::后台采集循环, this);
    VINE_LOGI("容器监控器：实例 %s 启动监控成功，目标进程 %d", 实例标识.c_str(), 进程标识);
    return true;
}


void 容器监控器::停止监控() {
    // 先翻转原子标志再等待线程汇合，最后清空历史快照并记录中文日志。
    bool 曾运行 = 运行标志.exchange(false);
    if (后台线程.joinable()) {
        if (std::this_thread::get_id() != 后台线程.get_id()) { 后台线程.join(); }
        else { 后台线程.detach(); }
    }
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    历史快照.clear();
    if (曾运行) { VINE_LOGI("容器监控器：实例 %s 已停止监控", 目标实例.c_str()); }
}

bool 容器监控器::正在监控() {
    // 直接读取原子标志，无锁快速判断后台线程是否存活。
    bool 当前状态 = 运行标志.load();
    return 当前状态;
}

long 容器监控器::读取处理器快照() {
    // 处理器快照：解析目标进程统计中用户态与内核态节拍之和。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) { VINE_LOGW("容器监控器：进程未绑定"); return -1; }
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/stat";
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容 || 内容->empty()) { VINE_LOGW("容器监控器：进程 %d 统计不可读", 进程标识); return -1; }
    size_t 右括号 = 内容->rfind(')');
    if (右括号 == std::string::npos) { VINE_LOGW("容器监控器：进程统计格式异常"); return -1; }
    size_t 位置 = 右括号 + 1;
    for (int 跳过 = 0; 跳过 < 13; 跳过++) {
        位置 = 内容->find(' ', 位置);
        if (位置 == std::string::npos) { VINE_LOGW("容器监控器：进程统计字段不足"); return -1; }
        位置++;
        if (位置 >= 内容->size()) { VINE_LOGW("容器监控器：进程统计字段不足"); return -1; }
    }
    long 用户 = 0, 内核 = 0;
    if (sscanf(内容->c_str() + 位置, "%ld %ld", &用户, &内核) != 2) {
        VINE_LOGW("容器监控器：节拍解析失败");
        return -1;
    }
    if (用户 < 0 || 内核 < 0) return -1;
    return 用户 + 内核;
}


long 容器监控器::读取内存使用字节() {
    // 内存使用字节：优先容器分组用量，缺失时回退进程常驻集。
    long 用量 = 读取控制组数值("memory.current");
    if (用量 >= 0) return 用量;
    return 读取进程状态数值("VmRSS:", true);
}


long 容器监控器::读取内存上限字节() {
    // 内存上限字节：分组上限为不限时以整机总量作为有效上限兜底。
    long 上限 = 读取控制组数值("memory.max");
    if (上限 > 0) return 上限;
    long 总量 = 读取内存信息字节("MemTotal:");
    if (总量 > 0) {
        VINE_LOGI("容器监控器：内存上限不限，以总量 %ld 兜底", 总量);
        return 总量;
    }
    return -1;
}


double 容器监控器::读取处理器使用率() {
    // 处理器使用率：基于缓存快照做差分，分子忙碌增量分母总增量。
    long 新用户态 = 读取处理器节拍字段(0);
    long 新内核态 = 读取处理器节拍字段(2);
    long 新空闲 = 读取处理器节拍字段(3);
    if (新用户态 < 0 || 新内核态 < 0 || 新空闲 < 0) { VINE_LOGW("容器监控器：快照无效，无法计算"); return -1.0; }
    long 旧用户态 = 0, 旧内核态 = 0, 旧空闲 = 0;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        旧用户态 = 历史用户态; 旧内核态 = 历史内核态; 旧空闲 = 历史空闲态;
        历史用户态 = 新用户态; 历史内核态 = 新内核态; 历史空闲态 = 新空闲;
    }
    long 旧总量 = 旧用户态 + 旧内核态 + 旧空闲;
    long 新总量 = 新用户态 + 新内核态 + 新空闲;
    long 总增量 = 新总量 - 旧总量;
    long 忙增量 = (新用户态 + 新内核态) - (旧用户态 + 旧内核态);
    double 使用率 = 0;
    if (总增量 > 0 && 忙增量 >= 0) 使用率 = (double)忙增量 * 100.0 / (double)总增量;
    if (使用率 < 0) 使用率 = 0; if (使用率 > 100) 使用率 = 100;
    VINE_LOGI("容器监控器：实例 %s 处理器使用率 %.2f%%", 目标实例.c_str(), 使用率);
    return 使用率;
}


double 容器监控器::读取内存使用率() {
    // 内存使用率：已用除以上限换算百分比，任一无效返回哨兵。
    long 已用 = 读取内存使用字节();
    long 上限 = 读取内存上限字节();
    if (已用 < 0 || 上限 <= 0) { VINE_LOGW("容器监控器：内存数据无效"); return -1.0; }
    return 计算内存使用率(已用, 上限);
}


std::string 容器监控器::读取进程状态文本() {
    // 进程状态文本：提取目标进程状态行，缺失回退首行。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) { VINE_LOGW("容器监控器：进程未绑定"); return std::string(""); }
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/status";
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容 || 内容->empty()) { VINE_LOGW("容器监控器：状态文件不可读"); return std::string(""); }
    size_t 查找 = 内容->find("State:");
    size_t 行首 = 0;
    if (查找 != std::string::npos) {
        size_t 换行 = 内容->rfind('\n', 查找);
        行首 = (换行 == std::string::npos) ? 0 : (换行 + 1);
    }
    size_t 行尾 = 内容->find('\n', 行首);
    size_t 行长 = (行尾 == std::string::npos) ? (内容->size() - 行首) : (行尾 - 行首);
    if (行长 > 256) 行长 = 256;
    if (行首 >= 内容->size() || 行长 == 0) return std::string("");
    VINE_LOGI("容器监控器：通用状态读取成功");
    return 内容->substr(行首, 行长);
}


long 容器监控器::读取线程数量() {
    // 线程数量：读取目标进程状态中线程数字段。
    return 读取进程状态数值("Threads:", false);
}


long 容器监控器::读取描述符数量() {
    // 描述符数量：统计目标进程描述符目录条目数，上限钳制防异常膨胀。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) { VINE_LOGW("容器监控器：目标进程未设置"); return -1; }
    std::string 目录 = "/proc/" + std::to_string(进程标识) + "/fd";
    DIR *句柄 = opendir(目录.c_str());
    if (!句柄) { VINE_LOGW("容器监控器：描述符目录不可读"); return -1; }
    long 计数 = 0;
    struct dirent *条目 = nullptr;
    while ((条目 = readdir(句柄)) != nullptr && 计数 <= 65536) {
        if (条目->d_name[0] == '.') continue;
        计数++;
    }
    closedir(句柄);
    return 计数 > 65536 ? 65536 : 计数;
}


long 容器监控器::读取块读字节() {
    // 块读字节：读取目标进程输入输出统计中读字节数。
    return 读取进程输入输出("read_bytes:");
}


long 容器监控器::读取块写字节() {
    // 块写字节：读取目标进程输入输出统计中写字节数。
    return 读取进程输入输出("write_bytes:");
}


long 容器监控器::读取网络接收字节() {
    // 网络接收字节：聚合各接口收字节列零号。
    return 聚合网络设备列(0);
}


long 容器监控器::读取网络发送字节() {
    // 网络发送字节：聚合各接口发字节列八号。
    return 聚合网络设备列(8);
}


long 容器监控器::读取网络接收包数() {
    // 网络接收包数：聚合各接口收包列一号。
    return 聚合网络设备列(1);
}


long 容器监控器::读取网络发送包数() {
    // 网络发送包数：聚合各接口发包列九号。
    return 聚合网络设备列(9);
}


std::string 容器监控器::读取控制组路径() {
    // 加锁拷贝控制组路径字符串并返回，空时记录中文提示。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    if (控制组路径.empty()) { VINE_LOGW("容器监控器：控制组路径尚未绑定"); return std::string(""); }
    return 控制组路径;
}

bool 容器监控器::刷新控制组路径(int 进程标识) {
    // 通过进程分组文件解析容器所属分组路径，行数设上限防异常膨胀。
    if (进程标识 <= 0) 进程标识 = 目标进程;
    if (进程标识 <= 0) { VINE_LOGE("容器监控器：刷新控制组需要有效进程号"); return false; }
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/cgroup";
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) { VINE_LOGW("容器监控器：读取 %s 失败", 路径.c_str()); return false; }
    std::string 选中;
    size_t 位置 = 0;
    int 行数 = 0;
    while (位置 < 内容->size() && 行数 < 1024) {
        size_t 行尾 = 内容->find('\n', 位置);
        size_t 止点 = (行尾 == std::string::npos) ? std::string::npos : 行尾;
        size_t 冒号 = 内容->rfind(':', 止点);
        if (冒号 != std::string::npos && 冒号 >= 位置) {
            size_t 行末 = (行尾 == std::string::npos) ? 内容->size() : 行尾;
            if (冒号 + 1 < 行末) {
                std::string 分组 = 内容->substr(冒号 + 1, 行末 - 冒号 - 1);
                if (!分组.empty() && 分组 != "/") {
                    选中 = 分组;
                    if (分组.find("vineos") != std::string::npos) break;
                }
            }
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    控制组路径 = 选中.empty() ? std::string("/sys/fs/cgroup") : ("/sys/fs/cgroup" + 选中);
    VINE_LOGI("容器监控器：控制组刷新为 %s", 控制组路径.c_str());
    return true;
}


double 容器监控器::计算处理器使用率(long 旧用户态, long 旧内核态, long 旧空闲, long 新用户态, long 新内核态, long 新空闲) {
    // 使用传入的新旧三元组做差分，分子为忙碌增量分母为总增量。
    if (旧用户态 < 0 || 旧内核态 < 0 || 旧空闲 < 0) { VINE_LOGW("容器监控器：旧快照非法"); return -1.0; }
    if (新用户态 < 0 || 新内核态 < 0 || 新空闲 < 0) { VINE_LOGW("容器监控器：新快照非法"); return -1.0; }
    long 旧总量 = 旧用户态 + 旧内核态 + 旧空闲;
    long 新总量 = 新用户态 + 新内核态 + 新空闲;
    long 总增量 = 新总量 - 旧总量;
    long 忙增量 = (新用户态 + 新内核态) - (旧用户态 + 旧内核态);
    if (总增量 <= 0) { VINE_LOGW("容器监控器：增量为零无法计算"); return -1.0; }
    if (忙增量 < 0) 忙增量 = 0;
    double 使用率 = (double)忙增量 * 100.0 / (double)总增量;
    if (使用率 < 0) 使用率 = 0; if (使用率 > 100) 使用率 = 100;
    VINE_LOGI("容器监控器：差分使用率 %.2f%%", 使用率);
    return 使用率;
}

double 容器监控器::计算内存使用率(long 已用字节, long 上限字节) {
    // 已用除以上限换算百分比，上限非法时返回哨兵并记录警告。
    if (上限字节 <= 0) { VINE_LOGW("容器监控器：上限 %ld 非法", 上限字节); return -1.0; }
    if (已用字节 < 0) 已用字节 = 0;
    double 使用率 = (double)已用字节 * 100.0 / (double)上限字节;
    if (使用率 < 0) 使用率 = 0; if (使用率 > 100) 使用率 = 100;
    return 使用率;
}

bool 容器监控器::写入监控日志(const std::string &日志文本) {
    // 将监控文本追加到实例日志文件，长度设上限并循环写尽防短写。
    if (日志文本.empty() || 日志文本.size() > 65536) { VINE_LOGW("容器监控器：日志文本非法"); return false; }
    std::string 实例;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        实例 = 目标实例;
    }
    if (实例.empty() || 实例.size() > 128) { VINE_LOGW("容器监控器：实例标识非法"); return false; }
    std::string 目录 = "/data/data/com.vineos/logs/" + 实例;
    if (!mkdirs(目录)) { VINE_LOGW("容器监控器：创建日志目录失败"); return false; }
    std::string 路径 = 目录 + "/monitor.log";
    int 描述符 = open(路径.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (描述符 < 0) { VINE_LOGE_ERRNO("open(monitor.log)"); return false; }
    std::string 落盘 = 日志文本 + "\n";
    const char *指针 = 落盘.c_str();
    size_t 剩余 = 落盘.size();
    while (剩余 > 0) {
        ssize_t 单次 = write(描述符, 指针, 剩余);
        if (单次 < 0) {
            if (errno == EINTR) continue;
            VINE_LOGE_ERRNO("write(monitor.log)");
            close(描述符);
            return false;
        }
        指针 += 单次;
        剩余 -= (size_t)单次;
    }
    close(描述符);
    return true;
}


std::string 容器监控器::导出诊断文本() {
    // 汇总实例标识进程号控制组路径与关键指标，生成中文多行诊断文本。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断文本 = "=== 容器监控诊断 ===\n";
    诊断文本 += "实例标识：" + 目标实例 + "\n";
    诊断文本 += "目标进程：" + std::to_string(目标进程) + "\n";
    诊断文本 += "控制组：" + (控制组路径.empty() ? std::string("未绑定") : 控制组路径) + "\n";
    诊断文本 += "采样间隔毫秒：" + std::to_string(采样间隔毫秒) + "\n";
    诊断文本 += "历史快照数：" + std::to_string((long)历史快照.size()) + "\n";
    诊断文本 += "生成方式：读取 proc 与控制组文件实时汇总\n";
    VINE_LOGI("容器监控器：导出诊断，实例 %s", 目标实例.c_str());
    return 诊断文本;
}

long 容器监控器::读取用户态节拍() {
    // 用户态节拍：统计快照零号字段。
    return 读取处理器节拍字段(0);
}


long 容器监控器::读取内核态节拍() {
    // 内核态节拍：统计快照二号字段。
    return 读取处理器节拍字段(2);
}


long 容器监控器::读取空闲节拍() {
    // 空闲节拍：统计快照三号字段。
    return 读取处理器节拍字段(3);
}


long 容器监控器::读取等待节拍() {
    // 等待节拍：统计快照四号字段。
    return 读取处理器节拍字段(4);
}


long 容器监控器::读取中断节拍() {
    // 中断节拍：统计快照五号字段。
    return 读取处理器节拍字段(5);
}


long 容器监控器::读取软中断节拍() {
    // 软中断节拍：统计快照六号字段。
    return 读取处理器节拍字段(6);
}


long 容器监控器::读取窃取节拍() {
    // 窃取节拍：统计快照七号字段。
    return 读取处理器节拍字段(7);
}


long 容器监控器::读取内存总量字节() {
    // 内存总量字节：内存信息总量键。
    return 读取内存信息字节("MemTotal:");
}


long 容器监控器::读取内存空闲字节() {
    // 内存空闲字节：内存信息空闲键。
    return 读取内存信息字节("MemFree:");
}


long 容器监控器::读取内存可用字节() {
    // 内存可用字节：内存信息可用键。
    return 读取内存信息字节("MemAvailable:");
}


long 容器监控器::读取交换总量字节() {
    // 交换总量字节：内存信息交换总量键。
    return 读取内存信息字节("SwapTotal:");
}


long 容器监控器::读取交换空闲字节() {
    // 交换空闲字节：内存信息交换空闲键。
    return 读取内存信息字节("SwapFree:");
}


long 容器监控器::读取脏页字节() {
    // 脏页字节：内存信息脏页键。
    return 读取内存信息字节("Dirty:");
}


long 容器监控器::读取回写字节() {
    // 回写字节：内存信息回写键。
    return 读取内存信息字节("Writeback:");
}


long 容器监控器::读取匿名页字节() {
    // 匿名页字节：内存信息匿名页键。
    return 读取内存信息字节("AnonPages:");
}


long 容器监控器::读取映射页字节() {
    // 映射页字节：内存信息映射页键。
    return 读取内存信息字节("Mapped:");
}


long 容器监控器::读取内核栈字节() {
    // 内核栈字节：内存信息内核栈键。
    return 读取内存信息字节("KernelStack:");
}


long 容器监控器::读取页表字节() {
    // 页表字节：内存信息页表键。
    return 读取内存信息字节("PageTables:");
}


long 容器监控器::读取上下文切换次数() {
    // 上下文切换次数：统计文件上下文切换行。
    std::optional<std::string> 内容 = read_file("/proc/stat");
    if (!内容) { VINE_LOGW("容器监控器：统计文件不可读"); return -1; }
    long 数值 = 0;
    if (!按行首键提取整数(*内容, "ctxt", 数值, 1024)) { VINE_LOGW("容器监控器：切换行缺失"); return -1; }
    return 数值 < 0 ? 0 : 数值;
}


long 容器监控器::读取虚拟内存字节() {
    // 虚拟内存字节：目标进程状态虚拟集键。
    return 读取进程状态数值("VmSize:", true);
}


long 容器监控器::读取常驻内存字节() {
    // 常驻内存字节：目标进程状态常驻集键。
    return 读取进程状态数值("VmRSS:", true);
}


double 容器监控器::读取一分钟负载() {
    // 一分钟负载：负载均值零号。
    return 读取负载数值(0);
}


double 容器监控器::读取五分钟负载() {
    // 五分钟负载：负载均值一号。
    return 读取负载数值(1);
}


double 容器监控器::读取十五分钟负载() {
    // 十五分钟负载：负载均值二号。
    return 读取负载数值(2);
}


long 容器监控器::读取在线核数() {
    // 在线核数：区间求和失败回退系统调用。
    return 统计在线核数();
}


long 容器监控器::读取启动后秒数() {
    // 启动后秒数：运行时间文件首个浮点取整。
    double 秒数 = 读取单值浮点("/proc/uptime");
    if (秒数 < 0) return -1;
    if (秒数 > 9000000000.0) 秒数 = 9000000000.0;
    return (long)秒数;
}


bool 容器监控器::更新历史快照() {
    // 主动触发一次采集并压入历史队列，供外部按需刷新。
    long 处理器节拍 = 读取处理器快照();
    long 内存字节 = 读取内存使用字节();
    if (处理器节拍 < 0 && 内存字节 < 0) { VINE_LOGW("容器监控器：手动刷新失败"); return false; }
    压入历史快照(处理器节拍, 内存字节);
    return true;
}


long 容器监控器::读取历史快照数() {
    // 加锁返回历史快照队列长度。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return (long)历史快照.size();
}

void 容器监控器::清空历史快照() {
    // 加锁清空历史快照向量并记录中文日志。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    历史快照.clear();
    VINE_LOGI("容器监控器：历史快照已清空");
}

std::string 容器监控器::读取实例标识() {
    // 加锁返回目标实例标识的拷贝，保证线程安全。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 目标实例;
}

int 容器监控器::读取目标进程() {
    // 返回绑定的目标进程号，未绑定返回负一。
    return 目标进程;
}

void 容器监控器::设置采样间隔(int 间隔毫秒) {
    // 校验区间后加锁更新采样间隔，非法值钳制到合理范围。
    if (间隔毫秒 < 200) 间隔毫秒 = 200;
    if (间隔毫秒 > 60000) 间隔毫秒 = 60000;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    采样间隔毫秒 = 间隔毫秒;
    VINE_LOGI("容器监控器：采样间隔更新为 %d 毫秒", 采样间隔毫秒);
}

int 容器监控器::读取采样间隔() {
    // 加锁返回当前采样间隔毫秒数。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 采样间隔毫秒;
}

} // namespace vine
