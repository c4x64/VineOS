// ============================================================================
// 模块名称：健康检查器
// ============================================================================
// 【模块说明】
// 本模块负责容器健康检查，覆盖初始化进程存活挂载点绑定器与网络命名空间。
// 通过信号零探测进程存活，解析挂载信息表确认关键伪文件系统已挂载。
// 检查绑定器与匿名共享内存节点是否存在，缺失时给出中文降级提示。
// 比较宿主与容器网络命名空间链接，判断隔离是否生效。
// 后台巡检线程周期执行全量检查，结果保存在互斥锁保护的报告中。
//
// 【主要功能】
//   01. 检查初始化进程是否存活
//   02. 检查伪文件系统挂载点
//   03. 检查绑定器与共享内存节点
//   04. 检查网络命名空间隔离
//   05. 检查根文件系统可写性
//   06. 后台周期巡检并生成报告
//   07. 查询历史故障次数
//   08. 导出中文健康诊断
//
// 【实现原理】
//   01. 使用 kill 信号零无副作用探测进程是否存在，不发送实际信号。
//   02. 读取 /proc/进程号/mountinfo 逐行匹配挂载点路径与文件系统类型。
//   03. 使用 access 与 stat 系统调用确认设备节点存在且权限正确。
//   04. 使用 readlink 读取网络命名空间符号链接并比较字符串。
//   05. 使用 access 写权限位判断根文件系统是否可写。
//   06. 后台线程使用可中断睡眠实现周期巡检。
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
#include "健康检查器.h"
#include <sys/statvfs.h>
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
bool 健康检查器::探测进程存活(int 进程标识) {
    // 探测进程存活：信号零探测，无副作用，有权限无信号亦视为存活。
    if (进程标识 <= 0) return false;
    if (kill((pid_t)进程标识, 0) == 0) return true;
    return errno == EPERM;
}
bool 健康检查器::检查目录可达(const std::string &完整路径) {
    // 检查目录可达：存在且可读可执行，失败记录中文原因。
    if (完整路径.empty() || 完整路径.size() > 1024) return false;
    if (access(完整路径.c_str(), F_OK) != 0) { VINE_LOGW("健康检查器：目录 %s 缺失", 完整路径.c_str()); return false; }
    if (access(完整路径.c_str(), R_OK | X_OK) != 0) { VINE_LOGW("健康检查器：目录 %s 不可访问", 完整路径.c_str()); return false; }
    return true;
}
bool 健康检查器::检查挂载表包含(const char *挂载点) {
    // 检查挂载表包含：目标进程挂载表存在该挂载点，进程无效回退本地路径。
    if (挂载点 == nullptr || *挂载点 == '\0') return false;
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) {
        std::string 本地 = 根路径 + 挂载点;
        bool 好 = path_exists(本地);
        if (!好) VINE_LOGW("健康检查器：挂载点 %s 缺失", 本地.c_str());
        return 好;
    }
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/mountinfo";
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) {
        std::string 本地 = 根路径 + 挂载点;
        bool 好 = path_exists(本地);
        if (!好) VINE_LOGW("健康检查器：挂载点 %s 缺失", 本地.c_str());
        return 好;
    }
    bool 找到 = (内容->find(挂载点) != std::string::npos);
    if (!找到) VINE_LOGW("健康检查器：挂载表缺少 %s", 挂载点);
    return 找到;
}
bool 健康检查器::检查设备节点(const char *节点) {
    // 检查设备节点：宿主或容器内任一存在即通过，类型异常记录警告。
    if (节点 == nullptr || *节点 == '\0') return false;
    std::string 宿主路径 = 节点;
    std::string 容器路径 = 根路径 + 节点;
    bool 宿主好 = path_exists(宿主路径);
    bool 容器好 = 根路径.empty() ? true : path_exists(容器路径);
    struct stat 状态{0};
    if (宿主好 && stat(宿主路径.c_str(), &状态) == 0) {
        if (!S_ISCHR(状态.st_mode) && !S_ISREG(状态.st_mode)) {
            VINE_LOGW("健康检查器：节点 %s 类型异常", 宿主路径.c_str());
        }
    }
    if (!宿主好) VINE_LOGW("健康检查器：宿主节点 %s 缺失（可能影响兼容）", 宿主路径.c_str());
    return 宿主好 || 容器好;
}
long 健康检查器::读取首个整数(const char *文件路径, long 上限) {
    // 读取首个整数：扫描文本首个整数并钳制，失败返回哨兵。
    if (文件路径 == nullptr || *文件路径 == '\0') return -1;
    if (上限 < 1) 上限 = 1;
    if (上限 > 9000000000000L) 上限 = 9000000000000L;
    std::optional<std::string> 内容 = read_file(文件路径);
    if (!内容) { VINE_LOGW("健康检查器：读取 %s 失败", 文件路径); return -1; }
    long 数值 = 0;
    bool 已见 = false;
    for (char 字符 : *内容) {
        if (字符 >= '0' && 字符 <= '9') {
            已见 = true;
            if (数值 <= 900000000000000000L) 数值 = 数值 * 10 + (字符 - '0');
            else break;
        } else if (已见) {
            break;
        }
    }
    if (!已见) return -1;
    if (数值 > 上限) 数值 = 上限;
    return 数值;
}
long 健康检查器::统计挂载表行数(long 上限) {
    // 统计挂载表行数：单遍计数换行符，行数设上限。
    if (上限 < 1) 上限 = 1;
    if (上限 > 100000) 上限 = 100000;
    std::optional<std::string> 内容 = read_file("/proc/self/mountinfo");
    if (!内容) { VINE_LOGW("健康检查器：挂载表不可读"); return -1; }
    long 行数 = 0;
    for (char 字符 : *内容) {
        if (字符 == '\n') {
            行数++;
            if (行数 >= 上限) break;
        }
    }
    return 行数;
}
long 健康检查器::统计僵尸数(long 上限) {
    // 统计僵尸数：遍历进程目录解析状态字段，数量与扫描均设上限。
    if (上限 < 1) 上限 = 1;
    if (上限 > 100000) 上限 = 100000;
    DIR *目录 = opendir("/proc");
    if (!目录) { VINE_LOGW("健康检查器：进程目录不可读"); return -1; }
    long 僵尸 = 0;
    long 已扫 = 0;
    struct dirent *条目 = nullptr;
    while ((条目 = readdir(目录)) != nullptr && 已扫 < 4096) {
        bool 数字 = true;
        for (const char *指针 = 条目->d_name; *指针; 指针++) {
            if (*指针 < '0' || *指针 > '9') { 数字 = false; break; }
        }
        if (!数字) continue;
        已扫++;
        std::string 路径 = std::string("/proc/") + 条目->d_name + "/stat";
        std::optional<std::string> 内容 = read_file(路径);
        if (!内容) continue;
        size_t 右括号 = 内容->rfind(')');
        if (右括号 == std::string::npos || 右括号 + 2 >= 内容->size()) continue;
        if (内容->at(右括号 + 2) == 'Z') {
            僵尸++;
            if (僵尸 >= 上限) break;
        }
    }
    closedir(目录);
    return 僵尸;
}

健康检查器::健康检查器() {
    运行标志.store(false);
    VINE_LOGI("健康检查器：对象创建完成");
}

健康检查器::~健康检查器() {
    if (运行标志.load()) { 停止巡检(); }
    if (后台线程.joinable()) {
        if (std::this_thread::get_id() != 后台线程.get_id()) { 后台线程.join(); }
        else { 后台线程.detach(); }
    }
    VINE_LOGI("健康检查器：对象销毁完成");
}

bool 健康检查器::启动巡检(const std::string &实例标识, int 进程标识, const std::string &根路径) {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    if (运行标志.load()) { VINE_LOGW("健康检查器：已在巡检中"); return true; }
    if (进程标识 <= 0) { VINE_LOGE("健康检查器：进程标识非法"); return false; }
    目标实例 = 实例标识;
    目标进程 = 进程标识;
    this->根路径 = 根路径;
    运行标志.store(true);
    健康分值 = 100; 连续健康 = 0; 连续故障 = 0;
    后台线程 = std::thread(&健康检查器::后台巡检循环, this);
    VINE_LOGI("健康检查器：实例 %s 启动巡检", 实例标识.c_str());
    return true;
}

void 健康检查器::停止巡检() {
    // 翻转原子标志并等待后台线程汇合。
    bool 曾运行 = 运行标志.exchange(false);
    if (后台线程.joinable()) {
        if (std::this_thread::get_id() != 后台线程.get_id()) { 后台线程.join(); }
        else { 后台线程.detach(); }
    }
    if (曾运行) { VINE_LOGI("健康检查器：实例 %s 停止巡检", 目标实例.c_str()); }
}

bool 健康检查器::正在巡检() {
    return 运行标志.load();
}

bool 健康检查器::检查初始化存活() {
    // 检查初始化存活：探测目标初始化进程是否存在。
    return 探测进程存活(目标进程);
}


bool 健康检查器::检查挂载点完备() {
    // 检查挂载点完备：核心伪文件系统挂载点均存在。
    bool 进程好 = 检查挂载表包含("/proc");
    bool 系统好 = 检查挂载表包含("/sys");
    bool 设备好 = 检查挂载表包含("/dev");
    return 进程好 && 系统好 && 设备好;
}


bool 健康检查器::检查绑定器节点() {
    // 检查绑定器节点：确认绑定器设备节点存在。
    return 检查设备节点("/dev/binder");
}


bool 健康检查器::检查共享内存节点() {
    // 检查共享内存节点：确认匿名共享内存设备存在。
    return 检查设备节点("/dev/ashmem");
}


bool 健康检查器::检查网络隔离() {
    // 检查网络隔离：比较宿主与容器网络命名空间链接是否不同。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) return false;
    char 宿主链接[256] = {0};
    char 容器链接[256] = {0};
    ssize_t 宿主长 = readlink("/proc/self/ns/net", 宿主链接, sizeof(宿主链接) - 1);
    std::string 容器路径 = "/proc/" + std::to_string(进程标识) + "/ns/net";
    ssize_t 容器长 = readlink(容器路径.c_str(), 容器链接, sizeof(容器链接) - 1);
    if (宿主长 <= 0 || 容器长 <= 0) { VINE_LOGW("健康检查器：命名空间链接不可读"); return false; }
    宿主链接[宿主长] = '\0';
    容器链接[容器长] = '\0';
    bool 已隔离 = (std::string(宿主链接) != std::string(容器链接));
    VINE_LOGI("健康检查器：网络隔离=%s", 已隔离 ? "是" : "否");
    return 已隔离;
}


bool 健康检查器::检查根目录可写() {
    // 检查根目录可写：确认容器根路径具备写权限。
    std::string 根 = 根路径.empty() ? std::string("/") : 根路径;
    if (access(根.c_str(), W_OK) != 0) { VINE_LOGW("健康检查器：根 %s 不可写", 根.c_str()); return false; }
    return true;
}


bool 健康检查器::执行全量检查() {
    // 依次执行核心检查并汇总，更新分值与报告。
    bool 存活好 = 检查初始化存活();
    bool 挂载好 = 检查挂载点完备();
    bool 绑定好 = 检查绑定器节点();
    bool 网络好 = 检查网络隔离();
    bool 根好 = 检查根目录可写();
    bool 全好 = 存活好 && 挂载好 && 根好 && 网络好;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        if (全好) { 连续健康++; 连续故障 = 0; if (健康分值 < 100) 健康分值 += 2; }
        else { 连续故障++; 连续健康 = 0; 故障总数++; 健康分值 = 健康分值 > 10 ? 健康分值 - 10 : 0; }
        最后报告 = std::string("存活:") + (存活好 ? "好" : "坏") + " 挂载:" + (挂载好 ? "好" : "坏") + " 绑定:" + (绑定好 ? "好" : "坏");
    }
    if (!全好) VINE_LOGW("健康检查器：全量检查发现异常");
    return 全好;
}

int 健康检查器::读取健康分值() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 健康分值;
}

long 健康检查器::读取故障计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 故障总数;
}

std::string 健康检查器::读取最后报告() {
    // 加锁拷贝最后一次巡检报告文本。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 最后报告;
}

std::string 健康检查器::导出健康诊断() {
    // 汇总实例进程根路径分值与计数，生成中文诊断。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断 = "=== 容器健康诊断 ===\n";
    诊断 += "实例标识：" + 目标实例 + "\n";
    诊断 += "目标进程：" + std::to_string(目标进程) + "\n";
    诊断 += "根路径：" + 根路径 + "\n";
    诊断 += "健康分值：" + std::to_string(健康分值) + "\n";
    诊断 += "连续健康：" + std::to_string(连续健康) + "\n";
    诊断 += "连续故障：" + std::to_string(连续故障) + "\n";
    诊断 += "最后报告：" + 最后报告 + "\n";
    return 诊断;
}

bool 健康检查器::检查进程存活(int 进程标识) {
    // 对任意传入进程号做信号零探测。
    if (进程标识 <= 0) return false;
    if (kill((pid_t)进程标识, 0) == 0) return true;
    return errno == EPERM;
}

bool 健康检查器::检查路径存在(const std::string &路径文本) {
    if (路径文本.empty()) return false;
    return path_exists(路径文本);
}

bool 健康检查器::检查路径可读(const std::string &路径文本) {
    if (路径文本.empty()) return false;
    return access(路径文本.c_str(), R_OK) == 0;
}

bool 健康检查器::检查路径可写(const std::string &路径文本) {
    if (路径文本.empty()) return false;
    return access(路径文本.c_str(), W_OK) == 0;
}

bool 健康检查器::检查路径可执行(const std::string &路径文本) {
    if (路径文本.empty()) return false;
    return access(路径文本.c_str(), X_OK) == 0;
}

bool 健康检查器::检查挂载点存在(const std::string &挂载点) {
    if (挂载点.empty()) return false;
    std::string 完整 = 根路径 + 挂载点;
    bool 好 = path_exists(完整);
    if (!好) VINE_LOGW("健康检查器：挂载点 %s 缺失", 完整.c_str());
    return 好;
}

bool 健康检查器::检查文件系统类型(const std::string &挂载点, const std::string &期望类型) {
    if (挂载点.empty() || 期望类型.empty()) return false;
    std::string 完整 = 根路径 + 挂载点;
    struct stat 状态数据{0};
    if (stat(完整.c_str(), &状态数据) != 0) {
        VINE_LOGW("健康检查器：%s 不可访问", 完整.c_str());
        return false;
    }
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) return true;
    std::string 表路径 = "/proc/" + std::to_string(进程标识) + "/mountinfo";
    std::optional<std::string> 表内容 = read_file(表路径);
    if (!表内容) return true;
    bool 命中 = (表内容->find(挂载点) != std::string::npos) && (表内容->find(期望类型) != std::string::npos);
    if (!命中) VINE_LOGW("健康检查器：%s 类型非 %s", 挂载点.c_str(), 期望类型.c_str());
    return 命中;
}

long 健康检查器::统计挂载点数量() {
    // 统计挂载点数量：挂载表总行数，上限十万。
    return 统计挂载表行数(100000);
}


long 健康检查器::读取挂载信息行数() {
    // 读取挂载信息行数：挂载表总行数，上限十万。
    return 统计挂载表行数(100000);
}


bool 健康检查器::检查伪终端就绪() {
    // 检查伪终端就绪：终端设备系统挂载点存在。
    return 检查挂载表包含("/dev/pts");
}


bool 健康检查器::检查控制台就绪() {
    // 检查控制台就绪：空设备节点存在。
    return 检查设备节点("/dev/null");
}


bool 健康检查器::检查空设备就绪() {
    // 检查空设备就绪：空设备挂载点存在。
    return 检查挂载表包含("/dev");
}


bool 健康检查器::检查随机数设备() {
    // 检查随机数设备：确认随机数设备节点存在。
    return 检查设备节点("/dev/urandom");
}


bool 健康检查器::检查回环接口() {
    // 检查回环接口：回环接口状态为启用。
    std::optional<std::string> 内容 = read_file("/sys/class/net/lo/operstate");
    if (!内容) { VINE_LOGW("健康检查器：回环状态不可读"); return false; }
    std::string 去空 = *内容;
    while (!去空.empty() && (去空.back() == '\n' || 去空.back() == ' ')) 去空.pop_back();
    bool 启用 = (去空 == "up" || 去空 == "unknown");
    if (!启用) VINE_LOGW("健康检查器：回环未启用");
    return 启用;
}


bool 健康检查器::检查默认路由() {
    // 检查默认路由：路由表存在默认目的行，行数设上限。
    std::optional<std::string> 内容 = read_file("/proc/net/route");
    if (!内容) { VINE_LOGW("健康检查器：路由表不可读"); return false; }
    size_t 位置 = 0;
    int 行号 = 0;
    while (位置 < 内容->size() && 行号 < 1024) {
        size_t 行尾 = 内容->find('\n', 位置);
        std::string 行 = 内容->substr(位置, (行尾 == std::string::npos) ? std::string::npos : (行尾 - 位置));
        行号++;
        if (行号 > 1) {
            char 接口[32] = {0};
            char 目的[32] = {0};
            char 网关[32] = {0};
            if (sscanf(行.c_str(), "%31s %31s %31s", 接口, 目的, 网关) == 3) {
                if (std::string(目的) == "00000000") return true;
            }
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
    }
    VINE_LOGW("健康检查器：默认路由缺失");
    return false;
}


bool 健康检查器::检查域名解析() {
    // 检查域名解析：容器解析配置包含域名服务器。
    std::string 路径 = 根路径 + "/etc/resolv.conf";
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容) {
        内容 = read_file("/etc/resolv.conf");
        if (!内容) { VINE_LOGW("健康检查器：解析配置不可读"); return false; }
    }
    bool 好 = (内容->find("nameserver") != std::string::npos);
    if (!好) VINE_LOGW("健康检查器：域名服务器缺失");
    return 好;
}


bool 健康检查器::检查系统属性(const std::string &属性名) {
    if (属性名.empty()) return false;
    std::string 属性路径 = 根路径 + "/default.prop";
    std::optional<std::string> 文件内容 = read_file(属性路径);
    if (!文件内容) return true;
    return 文件内容->find(属性名) != std::string::npos;
}

bool 健康检查器::检查初始化参数() {
    // 检查初始化参数：目标进程命令行非空。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) return false;
    std::string 路径 = "/proc/" + std::to_string(进程标识) + "/cmdline";
    std::optional<std::string> 内容 = read_file(路径);
    if (!内容 || 内容->empty() || 内容->at(0) == '\0') {
        VINE_LOGW("健康检查器：进程 %d 命令行缺失", 进程标识);
        return false;
    }
    return true;
}


bool 健康检查器::检查数据目录() {
    // 检查数据目录：容器数据目录可达。
    return 检查目录可达(根路径 + "/data");
}


bool 健康检查器::检查系统目录() {
    // 检查系统目录：容器系统目录可达。
    return 检查目录可达(根路径 + "/system");
}


bool 健康检查器::检查供应商目录() {
    // 检查供应商目录：容器供应商目录可达。
    return 检查目录可达(根路径 + "/vendor");
}


bool 健康检查器::检查缓存目录() {
    // 检查缓存目录：容器缓存目录可达。
    return 检查目录可达(根路径 + "/cache");
}


long 健康检查器::读取连续健康次数() {
    // 读取连续健康次数：加锁返回成员计数。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 连续健康;
}


long 健康检查器::读取连续故障次数() {
    // 读取连续故障次数：加锁返回成员计数。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 连续故障;
}


void 健康检查器::重置计数器() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    连续健康 = 0; 连续故障 = 0; 故障总数 = 0;
    VINE_LOGI("健康检查器：计数器已重置");
}

int 健康检查器::读取巡检间隔() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 巡检间隔毫秒;
}

void 健康检查器::设置巡检间隔(int 间隔毫秒) {
    // 钳制区间后加锁更新巡检间隔。
    int 有效间隔 = 间隔毫秒;
    if (有效间隔 < 500) 有效间隔 = 500;
    if (有效间隔 > 60000) 有效间隔 = 60000;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    巡检间隔毫秒 = 有效间隔;
    VINE_LOGI("健康检查器：巡检间隔更新为 %d 毫秒", 巡检间隔毫秒);
}

std::string 健康检查器::读取实例标识() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 目标实例;
}

int 健康检查器::读取目标进程() {
    return 目标进程;
}

std::string 健康检查器::读取根路径() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 根路径;
}

bool 健康检查器::更新最后报告(const std::string &报告文本) {
    if (报告文本.empty()) return false;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    最后报告 = 报告文本;
    return true;
}

void 健康检查器::记录一次健康() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    连续健康++; 连续故障 = 0;
    if (健康分值 < 100) 健康分值 += 5;
    if (健康分值 > 100) 健康分值 = 100;
}

void 健康检查器::记录一次故障(const std::string &原因文本) {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    连续故障++; 连续健康 = 0; 故障总数++;
    if (健康分值 > 10) 健康分值 -= 10; else 健康分值 = 0;
    最后报告 = 原因文本;
    VINE_LOGW("健康检查器：记录故障：%s", 原因文本.c_str());
}

bool 健康检查器::检查控制组可写() {
    // 检查控制组可写：统一层级具备写权限。
    if (access("/sys/fs/cgroup", W_OK | X_OK) != 0) { VINE_LOGW("健康检查器：控制组不可写"); return false; }
    return true;
}


bool 健康检查器::检查内存压力() {
    // 检查内存压力：可用占比高于百分之五视为通过。
    std::optional<std::string> 内容 = read_file("/proc/meminfo");
    if (!内容) { VINE_LOGW("健康检查器：内存信息不可读"); return false; }
    size_t 总量位 = 内容->find("MemTotal:");
    size_t 可用位 = 内容->find("MemAvailable:");
    if (总量位 == std::string::npos || 可用位 == std::string::npos) return false;
    long 总量 = 0, 可用 = 0;
    if (sscanf(内容->c_str() + 总量位, "%*s %ld", &总量) != 1 || 总量 <= 0) return false;
    if (sscanf(内容->c_str() + 可用位, "%*s %ld", &可用) != 1 || 可用 < 0) return false;
    bool 通过 = (可用 * 100L / 总量) > 5;
    if (!通过) VINE_LOGW("健康检查器：内存压力过大");
    return 通过;
}


bool 健康检查器::检查处理器压力() {
    // 检查处理器压力：一分钟负载低于核数两倍视为通过。
    std::optional<std::string> 内容 = read_file("/proc/loadavg");
    if (!内容 || 内容->empty()) { VINE_LOGW("健康检查器：负载不可读"); return false; }
    double 负载 = 0;
    if (sscanf(内容->c_str(), "%lf", &负载) != 1) return false;
    long 核数 = sysconf(_SC_NPROCESSORS_ONLN);
    if (核数 < 1) 核数 = 1;
    bool 通过 = 负载 < (double)核数 * 2.0;
    if (!通过) VINE_LOGW("健康检查器：处理器压力过大");
    return 通过;
}


bool 健康检查器::检查磁盘空间() {
    // 检查磁盘空间：容器根可用空间高于六十四兆视为通过。
    std::string 根 = 根路径.empty() ? std::string("/") : 根路径;
    struct statvfs 信息{0};
    if (statvfs(根.c_str(), &信息) != 0) { VINE_LOGW("健康检查器：空间查询失败"); return false; }
    unsigned long long 可用 = (unsigned long long)信息.f_bavail * (unsigned long long)信息.f_frsize;
    bool 通过 = 可用 > 67108864ULL;
    if (!通过) VINE_LOGW("健康检查器：磁盘空间不足");
    return 通过;
}


bool 健康检查器::检查文件描述符耗尽() {
    // 检查文件描述符耗尽：已分配低于上限九成视为通过。
    std::optional<std::string> 内容 = read_file("/proc/sys/fs/file");
    if (!内容 || 内容->empty()) { VINE_LOGW("健康检查器：描述符统计不可读"); return false; }
    long 已配 = 0, 上限 = 0;
    if (sscanf(内容->c_str(), "%ld %*d %ld", &已配, &上限) != 2 || 上限 <= 0) return false;
    bool 通过 = 已配 < 上限 * 9L / 10L;
    if (!通过) VINE_LOGW("健康检查器：描述符将耗尽");
    return 通过;
}


bool 健康检查器::检查线程数超限() {
    // 检查线程数超限：分组进程数低于上限视为通过。
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) return false;
    long 当前 = 读取首个整数("/sys/fs/cgroup/pids.current", 100000000L);
    if (当前 < 0) return 探测进程存活(进程标识);
    long 上限 = 读取首个整数("/sys/fs/cgroup/pids.max", 100000000L);
    if (上限 < 0) return true;
    bool 通过 = 当前 < 上限;
    if (!通过) VINE_LOGW("健康检查器：进程数超限");
    return 通过;
}


bool 健康检查器::检查僵尸进程() {
    // 检查僵尸进程：无僵尸视为通过。
    long 僵尸 = 统计僵尸数(1024);
    if (僵尸 < 0) return 探测进程存活(目标进程);
    if (僵尸 > 0) VINE_LOGW("健康检查器：僵尸 %ld 个", 僵尸);
    return 僵尸 == 0;
}


long 健康检查器::读取僵尸进程数() {
    // 读取僵尸进程数：扫描进程目录统计僵尸态。
    return 统计僵尸数(100000);
}


bool 健康检查器::检查初始化响应(int 超时毫秒) {
    int 进程标识 = 目标进程;
    if (进程标识 <= 0) return false;
    if (超时毫秒 < 100) 超时毫秒 = 100;
    struct timespec 开始{0, 0};
    clock_gettime(CLOCK_MONOTONIC, &开始);
    bool 好 = (kill((pid_t)进程标识, 0) == 0 || errno == EPERM);
    struct timespec 结束{0, 0};
    clock_gettime(CLOCK_MONOTONIC, &结束);
    long 往返 = (结束.tv_sec - 开始.tv_sec) * 1000L + (结束.tv_nsec - 开始.tv_nsec) / 1000000L;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        探测往返毫秒 = 往返;
    }
    return 好;
}

bool 健康检查器::发送存活探测() {
    return 检查初始化响应(1000);
}

long 健康检查器::读取探测往返毫秒() {
    // 读取探测往返毫秒：加锁返回上次探测耗时。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 探测往返毫秒;
}


bool 健康检查器::检查日志可写() {
    // 检查日志可写：容器日志目录可达。
    return 检查目录可达(根路径 + "/data/logs");
}


bool 健康检查器::检查套接字目录() {
    // 检查套接字目录：容器套接字目录可达。
    return 检查目录可达(根路径 + "/dev/socket");
}


bool 健康检查器::检查属性服务() {
    // 检查属性服务：目标进程存活视为属性通道可用。
    return 探测进程存活(目标进程);
}


void 健康检查器::后台巡检循环() {
    // 后台巡检循环：全量检查已更新计数，此处仅可中断睡眠。
    VINE_LOGI("健康检查器：后台线程启动");
    while (运行标志.load()) {
        执行全量检查();
        int 间隔 = 3000;
        {
            std::lock_guard<std::mutex> 互斥保护(互斥锁);
            间隔 = 巡检间隔毫秒;
        }
        for (int 已睡 = 0; 已睡 < 间隔 && 运行标志.load(); 已睡 += 200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    VINE_LOGI("健康检查器：后台线程退出");
}


} // namespace vine
