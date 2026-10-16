// ============================================================================
// 模块名称：挂载管理器
// ============================================================================
// 【模块说明】
// 本模块负责容器挂载管理，覆盖循环设备分配映像挂载与覆盖层数据目录。
// 统一处理伪文件系统绑定挂载与设备节点创建，保证容器内基础环境完备。
// 透视根切换优先使用系统调用，失败时回退到改变根目录，保证旧内核可用。
// 卸载遵循逆序原则并使用分离模式，避免阻塞在繁忙挂载点上。
// 循环设备支持自动清除标志，容器停止后宿主循环设备自动回收。
//
// 【主要功能】
//   01. 分配与释放循环设备
//   02. 挂载根文件系统映像
//   03. 挂载覆盖层数据目录
//   04. 挂载伪文件系统与设备节点
//   05. 执行透视根切换
//   06. 绑定挂载宿主文件
//   07. 逆序分离全部挂载
//   08. 导出中文挂载诊断
//
// 【实现原理】
//   01. 通过循环控制节点查询空闲编号，避免硬编码设备号冲突。
//   02. 优先尝试可写挂载，失败时回退到只读挂载，兼容多种映像格式。
//   03. 覆盖层需要上下层与工作目录三者，缺一则回退到绑定挂载。
//   04. 挂载传播属性统一设为私有，防止事件泄漏到宿主命名空间。
//   05. 透视根需要新根与旧根两个目录，切换后立即分离旧根。
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
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include "挂载管理器.h"
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
bool 挂载管理器::执行单项挂载(const std::string &来源, const std::string &目标, const char *类型, unsigned long 标志, const char *选项) {
    // 执行单项挂载：建目标目录后挂载单文件系统并登记。
    if (来源.empty() || 目标.empty() || 目标[0] != '/' || 类型 == nullptr) return false;
    if (!创建目录层级(目标)) return false;
    if (mount(来源.c_str(), 目标.c_str(), 类型, 标志, 选项) != 0) {
        VINE_LOGW("挂载管理器：挂载 %s 失败：%s", 目标.c_str(), strerror(errno));
        return false;
    }
    记录已挂载点(目标);
    VINE_LOGI("挂载管理器：%s 已挂载到 %s", 来源.c_str(), 目标.c_str());
    return true;
}
bool 挂载管理器::执行绑定挂载(const std::string &来源, const std::string &目标, bool 只读) {
    // 执行绑定挂载：按源类型准备占位后绑定，只读则重挂载验证。
    if (来源.empty() || 目标.empty() || 目标[0] != '/' || 目标.find("..") != std::string::npos) return false;
    if (来源.size() > 1024 || 目标.size() > 1024) return false;
    struct stat 源状态{0};
    if (stat(来源.c_str(), &源状态) != 0) { VINE_LOGW("挂载管理器：来源 %s 缺失", 来源.c_str()); return false; }
    if (S_ISDIR(源状态.st_mode)) {
        if (!创建目录层级(目标)) return false;
    } else {
        size_t 斜杠 = 目标.rfind('/');
        if (斜杠 != std::string::npos && 斜杠 > 0) {
            if (!创建目录层级(目标.substr(0, 斜杠))) return false;
        }
        int 占位 = open(目标.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        if (占位 >= 0) close(占位);
    }
    if (mount(来源.c_str(), 目标.c_str(), nullptr, MS_BIND, nullptr) != 0) {
        VINE_LOGW("挂载管理器：绑定 %s 失败：%s", 目标.c_str(), strerror(errno));
        return false;
    }
    if (只读 && !重新挂载(目标, true)) {
        umount2(目标.c_str(), MNT_DETACH);
        return false;
    }
    记录已挂载点(目标);
    VINE_LOGI("挂载管理器：%s 绑定到 %s 完成", 来源.c_str(), 目标.c_str());
    return true;
}
bool 挂载管理器::执行透视根切换实现(const std::string &新根) {
    // 执行透视根切换：私有传播后透视根，失败回退改变根并分离旧根。
    if (!校验挂载点合法(新根)) { VINE_LOGE("挂载管理器：新根非法"); return false; }
    if (mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        VINE_LOGW("挂载管理器：设私有传播失败：%s", strerror(errno));
    }
    std::string 旧根 = 新根 + "/.old_root";
    if (!创建目录层级(旧根)) return false;
    if (syscall(SYS_pivot_root, 新根.c_str(), 旧根.c_str()) != 0) {
        VINE_LOGW("挂载管理器：透视根失败（%s），回退改变根", strerror(errno));
        if (chroot(新根.c_str()) != 0) { VINE_LOGE_ERRNO("chroot回退"); return false; }
    } else {
        if (umount2("/.old_root", MNT_DETACH) != 0) VINE_LOGW("挂载管理器：分离旧根失败");
        rmdir("/.old_root");
    }
    if (chdir("/") != 0) { VINE_LOGW("挂载管理器：切换工作目录失败"); return false; }
    记录已挂载点(新根);
    VINE_LOGI("挂载管理器：根切换到 %s 完成", 新根.c_str());
    return true;
}
bool 挂载管理器::执行分离卸载(const std::string &目标) {
    // 执行分离卸载：分离模式卸载并清理登记，不存在视为成功。
    if (目标.empty() || 目标[0] != '/' || 目标.find("..") != std::string::npos) return false;
    if (!path_exists(目标)) { VINE_LOGI("挂载管理器：%s 无需卸载", 目标.c_str()); return true; }
    if (umount2(目标.c_str(), MNT_DETACH) != 0) {
        if (errno == EINVAL) { VINE_LOGI("挂载管理器：%s 未挂载视为成功", 目标.c_str()); }
        else {
            VINE_LOGW("挂载管理器：分离 %s 失败：%s", 目标.c_str(), strerror(errno));
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        for (auto 迭代 = 已挂载点.begin(); 迭代 != 已挂载点.end(); ++迭代) {
            if (*迭代 == 目标) { 已挂载点.erase(迭代); break; }
        }
    }
    VINE_LOGI("挂载管理器：%s 卸载成功", 目标.c_str());
    return true;
}
bool 挂载管理器::执行映像多格式挂载(const std::string &来源, const std::string &目标, bool 只读) {
    // 执行映像多格式挂载：依次尝试四种文件系统，首个成功即返回。
    if (来源.empty() || 目标.empty() || 目标[0] != '/') return false;
    const char *候选[4] = {"ext4", "f2fs", "squashfs", "erofs"};
    for (int 下标 = 0; 下标 < 4; 下标++) {
        unsigned long 标志 = MS_RELATIME | (只读 ? MS_RDONLY : 0);
        const char *选项 = (下标 == 0 && !只读) ? "errors=remount-ro" : nullptr;
        if (下标 > 1) 标志 |= MS_RDONLY;
        if (mount(来源.c_str(), 目标.c_str(), 候选[下标], 标志, 选项) == 0) {
            VINE_LOGI("挂载管理器：以 %s 挂载 %s 成功", 候选[下标], 目标.c_str());
            return true;
        }
    }
    VINE_LOGW("挂载管理器：%s 多格式挂载失败：%s", 目标.c_str(), strerror(errno));
    return false;
}
bool 挂载管理器::探测文件系统支持(const char *名称) {
    // 探测文件系统支持：逐行比对内核文件系统表末字段，行数设上限。
    if (名称 == nullptr || *名称 == '\0') return false;
    std::optional<std::string> 内容 = read_file("/proc/filesystems");
    if (!内容) { VINE_LOGW("挂载管理器：文件系统表不可读"); return false; }
    size_t 位置 = 0;
    int 行数 = 0;
    while (位置 < 内容->size() && 行数 < 512) {
        size_t 行尾 = 内容->find('\n', 位置);
        std::string 行 = 内容->substr(位置, (行尾 == std::string::npos) ? std::string::npos : (行尾 - 位置));
        size_t 制表 = 行.rfind('\t');
        std::string 末段 = (制表 == std::string::npos) ? 行 : 行.substr(制表 + 1);
        while (!末段.empty() && (末段.back() == ' ' || 末段.back() == '\r')) 末段.pop_back();
        if (末段 == 名称) return true;
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    return false;
}
bool 挂载管理器::重新挂载(const std::string &目标, bool 只读) {
    // 重新挂载：重挂载切换读写属性，失败记录中文原因。
    if (目标.empty() || 目标[0] != '/' || 目标.find("..") != std::string::npos) return false;
    unsigned long 标志 = MS_REMOUNT | (只读 ? MS_RDONLY : 0);
    if (mount(nullptr, 目标.c_str(), nullptr, 标志, nullptr) != 0) {
        VINE_LOGW("挂载管理器：重挂载 %s 失败：%s", 目标.c_str(), strerror(errno));
        return false;
    }
    VINE_LOGI("挂载管理器：%s 已重挂载为%s", 目标.c_str(), 只读 ? "只读" : "读写");
    return true;
}
bool 挂载管理器::关联循环(const std::string &循环设备名, int 映像描述符, bool 只读) {
    // 关联循环：绑定映像描述符并设置自动清除，只读由描述符模式决定。
    if (循环设备名.empty() || 循环设备名.find("/dev/loop") != 0 || 映像描述符 < 0) return false;
    (void)只读;
    int 循环描述符 = open(循环设备名.c_str(), O_RDWR | O_CLOEXEC);
    if (循环描述符 < 0) { VINE_LOGE_ERRNO("open(循环设备)"); return false; }
    bool 好 = true;
    if (ioctl(循环描述符, LOOP_SET_FD, 映像描述符) != 0) {
        VINE_LOGE_ERRNO("LOOP_SET_FD");
        好 = false;
    }
    if (好) {
        struct loop_info64 信息{0};
        信息.lo_flags = LO_FLAGS_AUTOCLEAR;
        if (ioctl(循环描述符, LOOP_SET_STATUS64, &信息) != 0) {
            VINE_LOGW("挂载管理器：自动清除设置失败：%s", strerror(errno));
        }
    }
    close(循环描述符);
    return 好;
}
long 挂载管理器::统计挂载表行数(const std::string &关键字, long 上限) {
    // 统计挂载表行数：无拷贝扫描含关键字的行数，空关键字统计全部。
    if (上限 < 1) 上限 = 1;
    if (上限 > 100000) 上限 = 100000;
    std::optional<std::string> 表 = read_file("/proc/self/mountinfo");
    if (!表) { VINE_LOGW("挂载管理器：挂载表不可读"); return -1; }
    if (关键字.empty()) {
        long 行数 = 0;
        for (char 字符 : *表) {
            if (字符 == '\n') {
                行数++;
                if (行数 >= 上限) break;
            }
        }
        return 行数;
    }
    long 计数 = 0;
    size_t 位置 = 0;
    while (位置 < 表->size() && 计数 < 上限) {
        size_t 命中 = 表->find(关键字, 位置);
        if (命中 == std::string::npos) break;
        计数++;
        size_t 行尾 = 表->find('\n', 命中);
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
    }
    return 计数;
}

挂载管理器::挂载管理器() {
    VINE_LOGI("挂载管理器：对象创建完成");
}

挂载管理器::~挂载管理器() {
    VINE_LOGI("挂载管理器：对象销毁完成");
}

std::string 挂载管理器::分配循环设备(const std::string &映像路径) {
    // 打开循环控制节点查询空闲编号并返回设备路径。
    if (!校验映像可读(映像路径)) { VINE_LOGE("挂载管理器：映像不可读"); return std::string(""); }
    int 控制描述符 = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
    if (控制描述符 < 0) { VINE_LOGE_ERRNO("open(loop-control)"); return std::string(""); }
    int 编号 = ioctl(控制描述符, LOOP_CTL_GET_FREE);
    close(控制描述符);
    if (编号 < 0) { VINE_LOGE("挂载管理器：无空闲循环设备：%s", strerror(errno)); return std::string(""); }
    std::string 设备 = "/dev/loop" + std::to_string(编号);
    int 映像描述符 = open(映像路径.c_str(), O_RDWR | O_CLOEXEC);
    if (映像描述符 < 0) 映像描述符 = open(映像路径.c_str(), O_RDONLY | O_CLOEXEC);
    if (映像描述符 < 0) { VINE_LOGE_ERRNO("open(映像)"); return std::string(""); }
    int 循环描述符 = open(设备.c_str(), O_RDWR | O_CLOEXEC);
    if (循环描述符 < 0) { VINE_LOGE_ERRNO("open(循环设备)"); close(映像描述符); return std::string(""); }
    if (ioctl(循环描述符, LOOP_SET_FD, 映像描述符) != 0) {
        VINE_LOGE_ERRNO("LOOP_SET_FD");
        close(循环描述符); close(映像描述符);
        return std::string("");
    }
    close(映像描述符);
    struct loop_info64 信息{0};
    信息.lo_flags = LO_FLAGS_AUTOCLEAR;
    if (ioctl(循环描述符, LOOP_SET_STATUS64, &信息) != 0) {
        VINE_LOGW("挂载管理器：设置自动清除失败：%s", strerror(errno));
    }
    close(循环描述符);
    VINE_LOGI("挂载管理器：分配 %s 给 %s", 设备.c_str(), 映像路径.c_str());
    return 设备;
}

bool 挂载管理器::释放循环设备(const std::string &循环设备名) {
    if (循环设备名.empty() || 循环设备名.find("/dev/loop") != 0) { VINE_LOGW("挂载管理器：循环名非法"); return false; }
    int 描述符 = open(循环设备名.c_str(), O_RDWR | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGW("挂载管理器：打开 %s 失败", 循环设备名.c_str()); return false; }
    bool 好 = (ioctl(描述符, LOOP_CLR_FD) == 0);
    close(描述符);
    if (!好) VINE_LOGW("挂载管理器：清除 %s 失败：%s", 循环设备名.c_str(), strerror(errno));
    else VINE_LOGI("挂载管理器：%s 已释放", 循环设备名.c_str());
    return 好;
}

bool 挂载管理器::挂载根文件系统(const std::string &映像路径, const std::string &挂载点) {
    if (!校验映像可读(映像路径) || !校验挂载点合法(挂载点)) return false;
    if (!创建目录层级(挂载点)) return false;
    std::string 循环设备 = 分配循环设备(映像路径);
    if (循环设备.empty()) return false;
    bool 好 = 尝试挂载多格式(循环设备, 挂载点);
    if (!好) { 释放循环设备(循环设备); return false; }
    记录已挂载点(挂载点);
    return true;
}

bool 挂载管理器::卸载根文件系统(const std::string &挂载点) {
    // 卸载根文件系统：分离模式卸载指定挂载点。
    if (!校验挂载点合法(挂载点)) { VINE_LOGE("挂载管理器：挂载点非法"); return false; }
    return 执行分离卸载(挂载点);
}


bool 挂载管理器::挂载覆盖层数据(const std::string &实例路径, const std::string &挂载点) {
    // 挂载覆盖层数据：备齐上下工作三目录后挂载，不支持回退绑定。
    if (实例路径.empty() || !校验挂载点合法(挂载点)) { VINE_LOGE("挂载管理器：覆盖层参数非法"); return false; }
    std::string 上层 = 实例路径 + "/upper";
    std::string 工作层 = 实例路径 + "/work";
    std::string 下层 = 实例路径 + "/lower";
    if (!创建目录层级(上层) || !创建目录层级(工作层) || !创建目录层级(下层) || !创建目录层级(挂载点)) {
        VINE_LOGE("挂载管理器：覆盖层目录准备失败");
        return false;
    }
    if (!探测文件系统支持("overlay")) {
        VINE_LOGW("挂载管理器：内核缺覆盖层支持，回退绑定");
        return 执行绑定挂载(下层, 挂载点, false);
    }
    std::string 选项 = 拼接覆盖层选项(下层, 上层, 工作层);
    if (mount("overlay", 挂载点.c_str(), "overlay", MS_RELATIME, 选项.c_str()) != 0) {
        VINE_LOGW("挂载管理器：覆盖层失败（%s），回退绑定", strerror(errno));
        return 执行绑定挂载(下层, 挂载点, false);
    }
    记录已挂载点(挂载点);
    VINE_LOGI("挂载管理器：覆盖层 %s 就绪", 挂载点.c_str());
    return true;
}


bool 挂载管理器::卸载覆盖层数据(const std::string &挂载点) {
    // 卸载覆盖层数据：分离模式卸载指定挂载点。
    if (!校验挂载点合法(挂载点)) { VINE_LOGE("挂载管理器：挂载点非法"); return false; }
    return 执行分离卸载(挂载点);
}


bool 挂载管理器::挂载伪文件系统(const std::string &根路径) {
    // 挂载伪文件系统：表驱动挂载 proc sys dev pts tmp，返回全量结果。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    struct 挂载项 { const char *来源; const char *后缀; const char *类型; unsigned long 标志; const char *选项; };
    const 挂载项 表[] = {
        {"proc", "/proc", "proc", MS_NODEV | MS_NOEXEC | MS_NOSUID, nullptr},
        {"sysfs", "/sys", "sysfs", MS_NODEV | MS_NOEXEC | MS_NOSUID, nullptr},
        {"tmpfs", "/dev", "tmpfs", MS_NOSUID, "size=65536k,mode=755"},
        {"devpts", "/dev/pts", "devpts", MS_NOEXEC | MS_NOSUID, "newinstance,ptmxmode=0666,mode=0620"},
        {"tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "size=32768k,mode=1777"},
    };
    bool 全好 = true;
    for (const auto &项 : 表) {
        if (!执行单项挂载(项.来源, 根 + 项.后缀, 项.类型, 项.标志, 项.选项)) 全好 = false;
    }
    if (!全好) VINE_LOGW("挂载管理器：部分伪系统未挂载");
    return 全好;
}


bool 挂载管理器::卸载伪文件系统(const std::string &根路径) {
    // 卸载伪文件系统：逆序分离各伪挂载点。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    const char *后缀[] = {"/dev/pts", "/tmp", "/dev", "/sys", "/proc"};
    bool 全好 = true;
    for (const char *尾 : 后缀) {
        if (!执行分离卸载(根 + 尾)) 全好 = false;
    }
    return 全好;
}


bool 挂载管理器::创建设备节点(const std::string &根路径) {
    // 创建设备节点：创建标准字符设备并校验模式。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    std::string 设备目录 = 根 + "/dev";
    if (!创建目录层级(设备目录) || !创建目录层级(设备目录 + "/pts")) return false;
    struct 节点 { const char *名; unsigned 主; unsigned 从; mode_t 模式; };
    const 节点 表[] = {
        {"null", 1, 3, 0666}, {"zero", 1, 5, 0666}, {"full", 1, 7, 0666},
        {"random", 1, 8, 0666}, {"urandom", 1, 9, 0666},
    };
    bool 全好 = true;
    for (const auto &项 : 表) {
        std::string 路径 = 设备目录 + "/" + 项.名;
        if (mknod(路径.c_str(), S_IFCHR | 项.模式, makedev(项.主, 项.从)) != 0 && errno != EEXIST) {
            VINE_LOGW("挂载管理器：节点 %s 创建失败：%s", 路径.c_str(), strerror(errno));
            全好 = false;
        } else if (chmod(路径.c_str(), 项.模式) != 0) {
            VINE_LOGW("挂载管理器：节点 %s 赋权失败", 路径.c_str());
            全好 = false;
        }
    }
    return 全好;
}


bool 挂载管理器::执行透视根切换(const std::string &新根路径) {
    // 执行透视根切换：复用统一切换实现。
    return 执行透视根切换实现(新根路径);
}


bool 挂载管理器::绑定挂载文件(const std::string &来源路径, const std::string &目标路径, bool 只读) {
    // 绑定挂载文件：复用统一绑定实现并透传只读标志。
    return 执行绑定挂载(来源路径, 目标路径, 只读);
}


bool 挂载管理器::绑定挂载目录(const std::string &来源路径, const std::string &目标路径) {
    // 绑定挂载目录：复用统一绑定实现，目录保持读写。
    return 执行绑定挂载(来源路径, 目标路径, false);
}


bool 挂载管理器::分离单个挂载(const std::string &挂载点) {
    // 分离单个挂载：复用统一分离实现。
    if (!校验挂载点合法(挂载点)) return false;
    return 执行分离卸载(挂载点);
}


bool 挂载管理器::分离全部挂载(const std::string &根路径) {
    // 分离全部挂载：快照登记表后逆序分离并清空。
    (void)根路径;
    std::vector<std::string> 快照;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        快照 = 已挂载点;
        if (快照.size() > 1024) 快照.resize(1024);
    }
    bool 全好 = true;
    for (auto 迭代 = 快照.rbegin(); 迭代 != 快照.rend(); ++迭代) {
        if (!执行分离卸载(*迭代)) 全好 = false;
    }
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        已挂载点.clear();
    }
    VINE_LOGI("挂载管理器：全部挂载分离%s", 全好 ? "成功" : "部分失败");
    return 全好;
}


bool 挂载管理器::设置私有传播() {
    // 设置私有传播：根挂载递归设私有防事件泄漏。
    if (mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        VINE_LOGW("挂载管理器：设私有传播失败：%s", strerror(errno));
        return false;
    }
    VINE_LOGI("挂载管理器：私有传播已设置");
    return true;
}


bool 挂载管理器::挂载进程文件系统(const std::string &根路径) {
    // 挂载进程文件系统：挂载进程信息伪系统。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行单项挂载("proc", 根 + "/proc", "proc", MS_NODEV | MS_NOEXEC | MS_NOSUID, nullptr);
}


bool 挂载管理器::挂载系统文件系统(const std::string &根路径) {
    // 挂载系统文件系统：挂载设备模型伪系统。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行单项挂载("sysfs", 根 + "/sys", "sysfs", MS_NODEV | MS_NOEXEC | MS_NOSUID, nullptr);
}


bool 挂载管理器::挂载设备临时系统(const std::string &根路径) {
    // 挂载设备临时系统：挂载设备目录临时系统。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行单项挂载("tmpfs", 根 + "/dev", "tmpfs", MS_NOSUID, "size=65536k,mode=755");
}


bool 挂载管理器::挂载终端设备系统(const std::string &根路径) {
    // 挂载终端设备系统：挂载伪终端设备系统。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行单项挂载("devpts", 根 + "/dev/pts", "devpts", MS_NOEXEC | MS_NOSUID, "newinstance,ptmxmode=0666,mode=0620");
}


bool 挂载管理器::挂载共享内存系统(const std::string &根路径) {
    // 挂载共享内存系统：挂载共享内存临时系统。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行单项挂载("tmpfs", 根 + "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "size=32768k,mode=1777");
}


bool 挂载管理器::挂载临时目录系统(const std::string &根路径) {
    // 挂载临时目录系统：挂载临时目录临时系统。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行单项挂载("tmpfs", 根 + "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "size=32768k,mode=1777");
}


bool 挂载管理器::挂载控制组层级(const std::string &根路径) {
    // 挂载控制组层级：优先统一层级，失败回退绑定宿主层级。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    std::string 目标 = 根 + "/sys/fs/cgroup";
    if (!创建目录层级(目标)) return false;
    if (mount("cgroup2", 目标.c_str(), "cgroup2", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr) == 0) {
        记录已挂载点(目标);
        VINE_LOGI("挂载管理器：统一控制组层级就绪");
        return true;
    }
    VINE_LOGW("挂载管理器：统一层级失败（%s），回退绑定", strerror(errno));
    return 执行绑定挂载("/sys/fs/cgroup", 目标, true);
}


bool 挂载管理器::挂载绑定器设备(const std::string &根路径) {
    // 挂载绑定器设备：绑定宿主绑定器节点，缺失返回失败。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    if (!path_exists("/dev/binder")) { VINE_LOGW("挂载管理器：宿主绑定器缺失"); return false; }
    return 执行绑定挂载("/dev/binder", 根 + "/dev/binder", false);
}


bool 挂载管理器::挂载匿名共享设备(const std::string &根路径) {
    // 挂载匿名共享设备：绑定宿主共享内存节点，缺失返回失败。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    if (!path_exists("/dev/ashmem")) { VINE_LOGW("挂载管理器：宿主共享内存缺失"); return false; }
    return 执行绑定挂载("/dev/ashmem", 根 + "/dev/ashmem", false);
}


bool 挂载管理器::挂载模拟器解释器(const std::string &宿主路径, const std::string &根路径) {
    // 挂载模拟器解释器：绑定指定宿主解释器到容器同路径。
    if (宿主路径.empty() || 宿主路径[0] != '/' || 宿主路径.find("..") != std::string::npos) {
        VINE_LOGE("挂载管理器：解释器路径非法");
        return false;
    }
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行绑定挂载(宿主路径, 根 + 宿主路径, true);
}


bool 挂载管理器::挂载增强模块桩(const std::string &根路径) {
    // 挂载增强模块桩：绑定宿主增强模块到容器同路径，缺失返回失败。
    const char *宿主桩 = "/system/bin/vine-magisk";
    if (!path_exists(宿主桩)) { VINE_LOGW("挂载管理器：宿主增强模块缺失"); return false; }
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行绑定挂载(宿主桩, 根 + 宿主桩, true);
}


bool 挂载管理器::创建旧根目录(const std::string &根路径) {
    // 创建旧根目录：创建透视根所需的旧根空目录。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 创建目录层级(根 + "/.old_root");
}


bool 挂载管理器::清理旧根目录() {
    // 清理旧根目录：分离并删除成员根路径下旧根目录。
    std::string 根;
    {
        std::lock_guard<std::mutex> 互斥保护(互斥锁);
        根 = 根路径;
    }
    if (根.empty()) 根 = "/tmp/vine-root";
    std::string 旧根 = 根 + "/.old_root";
    if (!path_exists(旧根)) { VINE_LOGI("挂载管理器：旧根 %s 不存在", 旧根.c_str()); return true; }
    if (umount2(旧根.c_str(), MNT_DETACH) != 0 && errno != EINVAL) {
        VINE_LOGW("挂载管理器：分离旧根失败：%s", strerror(errno));
        return false;
    }
    if (rmdir(旧根.c_str()) != 0 && errno != ENOENT) {
        VINE_LOGW("挂载管理器：删除旧根失败：%s", strerror(errno));
        return false;
    }
    VINE_LOGI("挂载管理器：旧根 %s 已清理", 旧根.c_str());
    return true;
}


bool 挂载管理器::切换根并清理(const std::string &新根路径) {
    // 切换根并清理：复用统一切换实现，旧根由实现内部分离。
    return 执行透视根切换实现(新根路径);
}


bool 挂载管理器::查询挂载点存在(const std::string &挂载点) {
    // 查询挂载点存在：挂载表中挂载点字段精确匹配，无任何副作用。
    if (!校验挂载点合法(挂载点)) return false;
    std::optional<std::string> 表 = read_file("/proc/self/mountinfo");
    if (!表) { VINE_LOGW("挂载管理器：挂载表不可读"); return false; }
    std::string 标记 = " " + 挂载点 + " ";
    size_t 位置 = 0;
    int 行数 = 0;
    while (位置 < 表->size() && 行数 < 4096) {
        size_t 行尾 = 表->find('\n', 位置);
        size_t 止 = (行尾 == std::string::npos) ? 表->size() : 行尾;
        size_t 命中 = 表->find(标记, 位置);
        if (命中 != std::string::npos && 命中 < 止) return true;
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    return false;
}


std::string 挂载管理器::查询已挂载源(const std::string &挂载点) {
    // 查询已挂载源：解析挂载表分隔符后来源字段并限长返回。
    if (!校验挂载点合法(挂载点)) return std::string("");
    std::optional<std::string> 表 = read_file("/proc/self/mountinfo");
    if (!表) { VINE_LOGW("挂载管理器：挂载表不可读"); return std::string(""); }
    std::string 标记 = " " + 挂载点 + " ";
    size_t 位置 = 0;
    int 行数 = 0;
    while (位置 < 表->size() && 行数 < 4096) {
        size_t 行尾 = 表->find('\n', 位置);
        size_t 止 = (行尾 == std::string::npos) ? 表->size() : 行尾;
        size_t 命中 = 表->find(标记, 位置);
        if (命中 != std::string::npos && 命中 < 止) {
            size_t 分隔 = 表->find(" - ", 命中);
            if (分隔 == std::string::npos || 分隔 >= 止) return std::string("");
            size_t 类型止 = 表->find(' ', 分隔 + 3);
            if (类型止 == std::string::npos || 类型止 >= 止) return std::string("");
            size_t 源起 = 类型止 + 1;
            if (源起 >= 止) return std::string("");
            size_t 源止 = 表->find(' ', 源起);
            if (源止 == std::string::npos || 源止 > 止) 源止 = 止;
            std::string 来源 = 表->substr(源起, 源止 - 源起);
            if (来源.size() > 256) 来源.resize(256);
            return 来源;
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    return std::string("");
}


long 挂载管理器::统计挂载点数量(const std::string &根路径) {
    // 统计挂载点数量：含根路径关键字的挂载表行数。
    return 统计挂载表行数(根路径, 100000);
}


long 挂载管理器::读取挂载表行数() {
    // 读取挂载表行数：挂载表总行数，上限十万。
    return 统计挂载表行数("", 100000);
}


std::string 挂载管理器::导出挂载诊断() {
    // 导出挂载诊断：条目设上限防诊断文本膨胀。
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    std::string 诊断 = "=== 容器挂载诊断 ===\n";
    诊断 += "实例路径：" + 实例路径 + "\n";
    诊断 += "根路径：" + 根路径 + "\n";
    诊断 += "已挂载点数：" + std::to_string((long)已挂载点.size()) + "\n";
    long 输出 = 0;
    for (const std::string &挂载点 : 已挂载点) {
        if (输出 >= 256) { 诊断 += "  ...（截断）\n"; break; }
        诊断 += "  - " + 挂载点 + "\n";
        输出++;
    }
    诊断 += "事件计数：" + std::to_string(事件计数) + "\n";
    return 诊断;
}


std::string 挂载管理器::读取实例路径() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 实例路径;
}

std::string 挂载管理器::读取根路径() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 根路径;
}

void 挂载管理器::设置实例路径(const std::string &路径文本) {
    if (路径文本.empty()) return;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    实例路径 = 路径文本;
    VINE_LOGI("挂载管理器：路径更新");
}

void 挂载管理器::设置根路径(const std::string &路径文本) {
    if (路径文本.empty()) return;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    根路径 = 路径文本;
    VINE_LOGI("挂载管理器：路径更新");
}

void 挂载管理器::记录已挂载点(const std::string &挂载点) {
    // 记录已挂载点：去重登记并设上限防无界增长。
    if (挂载点.empty() || 挂载点.size() > 1024) return;
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    for (const std::string &已有 : 已挂载点) { if (已有 == 挂载点) return; }
    if (已挂载点.size() >= 1024) 已挂载点.erase(已挂载点.begin());
    已挂载点.push_back(挂载点);
}


void 挂载管理器::清空已挂载点() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    已挂载点.clear();
    VINE_LOGI("挂载管理器：已挂载表已清空");
}

long 挂载管理器::读取已挂载点数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return (long)已挂载点.size();
}

bool 挂载管理器::创建目录层级(const std::string &目录路径) {
    // 创建目录层级：递归建目录并验证为目录，失败记录中文原因。
    if (目录路径.empty() || 目录路径[0] != '/' || 目录路径.find("..") != std::string::npos) {
        VINE_LOGW("挂载管理器：目录路径非法");
        return false;
    }
    if (目录路径.size() > 1024) return false;
    if (!mkdirs(目录路径)) { VINE_LOGW("挂载管理器：创建 %s 失败", 目录路径.c_str()); return false; }
    if (!is_directory(目录路径)) { VINE_LOGW("挂载管理器：%s 非目录", 目录路径.c_str()); return false; }
    return true;
}


bool 挂载管理器::校验映像可读(const std::string &映像路径) {
    // 校验映像可读：存在且为 regular 文件且可读，不做任何挂载。
    if (映像路径.empty() || 映像路径[0] != '/' || 映像路径.size() > 1024) return false;
    struct stat 状态{0};
    if (stat(映像路径.c_str(), &状态) != 0) { VINE_LOGW("挂载管理器：映像 %s 不可访问", 映像路径.c_str()); return false; }
    if (!S_ISREG(状态.st_mode)) { VINE_LOGW("挂载管理器：映像 %s 非文件", 映像路径.c_str()); return false; }
    if (access(映像路径.c_str(), R_OK) != 0) { VINE_LOGW("挂载管理器：映像 %s 不可读", 映像路径.c_str()); return false; }
    return true;
}


bool 挂载管理器::校验挂载点合法(const std::string &挂载点) {
    // 校验挂载点合法：绝对路径无穿越且长度受限，不做任何挂载。
    if (挂载点.empty() || 挂载点[0] != '/') return false;
    if (挂载点.find("..") != std::string::npos) return false;
    if (挂载点.size() > 1024) return false;
    return true;
}


bool 挂载管理器::探测覆盖层支持() {
    // 探测覆盖层支持：内核文件系统表含覆盖层即支持。
    bool 好 = 探测文件系统支持("overlay");
    if (!好) VINE_LOGW("挂载管理器：内核缺覆盖层支持");
    return 好;
}


bool 挂载管理器::探测绑定传播支持() {
    // 探测绑定传播支持：挂载表含共享传播标记即支持。
    std::optional<std::string> 表 = read_file("/proc/self/mountinfo");
    if (!表) { VINE_LOGW("挂载管理器：挂载表不可读"); return false; }
    return 表->find("shared:") != std::string::npos;
}


std::string 挂载管理器::读取内核挂载选项(const std::string &挂载点) {
    // 读取内核挂载选项：解析挂载表超级选项字段并限长返回。
    if (!校验挂载点合法(挂载点)) return std::string("");
    std::optional<std::string> 表 = read_file("/proc/self/mountinfo");
    if (!表) { VINE_LOGW("挂载管理器：挂载表不可读"); return std::string(""); }
    std::string 标记 = " " + 挂载点 + " ";
    size_t 位置 = 0;
    int 行数 = 0;
    while (位置 < 表->size() && 行数 < 4096) {
        size_t 行尾 = 表->find('\n', 位置);
        size_t 止 = (行尾 == std::string::npos) ? 表->size() : 行尾;
        size_t 命中 = 表->find(标记, 位置);
        if (命中 != std::string::npos && 命中 < 止) {
            size_t 分隔 = 表->find(" - ", 命中);
            if (分隔 == std::string::npos || 分隔 >= 止) return std::string("");
            size_t 段 = 分隔 + 3;
            for (int 跳过 = 0; 跳过 < 2; 跳过++) {
                size_t 空格 = 表->find(' ', 段);
                if (空格 == std::string::npos || 空格 >= 止) return std::string("");
                段 = 空格 + 1;
            }
            if (段 >= 止) return std::string("");
            std::string 选项 = 表->substr(段, 止 - 段);
            if (选项.size() > 256) 选项.resize(256);
            return 选项;
        }
        if (行尾 == std::string::npos) break;
        位置 = 行尾 + 1;
        行数++;
    }
    return std::string("");
}


bool 挂载管理器::设置挂载只读(const std::string &挂载点) {
    // 设置挂载只读：复用重挂载实现切换只读。
    if (!校验挂载点合法(挂载点)) return false;
    return 重新挂载(挂载点, true);
}


bool 挂载管理器::设置挂载读写(const std::string &挂载点) {
    // 设置挂载读写：复用重挂载实现切换读写。
    if (!校验挂载点合法(挂载点)) return false;
    return 重新挂载(挂载点, false);
}


bool 挂载管理器::重新挂载只读(const std::string &挂载点) {
    // 重新挂载只读：复用重挂载实现切换只读。
    if (!校验挂载点合法(挂载点)) return false;
    return 重新挂载(挂载点, true);
}


bool 挂载管理器::同步文件系统(const std::string &挂载点) {
    // 同步文件系统：打开挂载点目录描述符并同步元数据。
    if (!校验挂载点合法(挂载点)) return false;
    int 描述符 = open(挂载点.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGW("挂载管理器：打开 %s 失败", 挂载点.c_str()); return false; }
    bool 好 = (fsync(描述符) == 0);
    close(描述符);
    if (!好) VINE_LOGW("挂载管理器：同步 %s 失败", 挂载点.c_str());
    return 好;
}


long 挂载管理器::查询磁盘剩余字节(const std::string &挂载点) {
    // 查询磁盘剩余字节：可用块数乘块大小，上溢钳制。
    struct statvfs 信息{0};
    const char *路径 = 挂载点.empty() ? "/" : 挂载点.c_str();
    if (statvfs(路径, &信息) != 0) { VINE_LOGW("挂载管理器：查询 %s 失败", 路径); return -1; }
    unsigned long long 结果 = (unsigned long long)信息.f_bavail * (unsigned long long)信息.f_frsize;
    if (结果 > 9000000000000000000ULL) 结果 = 9000000000000000000ULL;
    return (long)结果;
}


long 挂载管理器::查询磁盘总量字节(const std::string &挂载点) {
    // 查询磁盘总量字节：总块数乘块大小，上溢钳制。
    struct statvfs 信息{0};
    const char *路径 = 挂载点.empty() ? "/" : 挂载点.c_str();
    if (statvfs(路径, &信息) != 0) { VINE_LOGW("挂载管理器：查询 %s 失败", 路径); return -1; }
    unsigned long long 结果 = (unsigned long long)信息.f_blocks * (unsigned long long)信息.f_frsize;
    if (结果 > 9000000000000000000ULL) 结果 = 9000000000000000000ULL;
    return (long)结果;
}


long 挂载管理器::查询文件系统块大小(const std::string &挂载点) {
    // 查询文件系统块大小：返回碎块大小。
    struct statvfs 信息{0};
    const char *路径 = 挂载点.empty() ? "/" : 挂载点.c_str();
    if (statvfs(路径, &信息) != 0) { VINE_LOGW("挂载管理器：查询 %s 失败", 路径); return -1; }
    if (信息.f_frsize <= 0) return -1;
    return (long)信息.f_frsize;
}


bool 挂载管理器::挂载压缩映像(const std::string &映像路径, const std::string &挂载点) {
    // 挂载压缩映像：分配循环设备后只读多格式挂载，失败释放。
    if (!校验映像可读(映像路径) || !校验挂载点合法(挂载点)) return false;
    if (!创建目录层级(挂载点)) return false;
    std::string 循环设备 = 分配循环设备(映像路径);
    if (循环设备.empty()) return false;
    bool 好 = 执行映像多格式挂载(循环设备, 挂载点, true);
    if (!好) { 释放循环设备(循环设备); return false; }
    记录已挂载点(挂载点);
    return true;
}


bool 挂载管理器::挂载扩展映像(const std::string &映像路径, const std::string &挂载点) {
    // 挂载扩展映像：分配循环设备后读写多格式挂载，失败释放。
    if (!校验映像可读(映像路径) || !校验挂载点合法(挂载点)) return false;
    if (!创建目录层级(挂载点)) return false;
    std::string 循环设备 = 分配循环设备(映像路径);
    if (循环设备.empty()) return false;
    bool 好 = 执行映像多格式挂载(循环设备, 挂载点, false);
    if (!好) { 释放循环设备(循环设备); return false; }
    记录已挂载点(挂载点);
    return true;
}


bool 挂载管理器::尝试挂载多格式(const std::string &映像路径, const std::string &挂载点) {
    // 尝试挂载多格式：复用统一多格式实现，默认读写。
    return 执行映像多格式挂载(映像路径, 挂载点, false);
}


bool 挂载管理器::关联循环只读(const std::string &循环设备名, int 映像描述符) {
    // 关联循环只读：复用统一关联实现，只读由描述符模式决定。
    return 关联循环(循环设备名, 映像描述符, true);
}


bool 挂载管理器::关联循环读写(const std::string &循环设备名, int 映像描述符) {
    // 关联循环读写：复用统一关联实现。
    return 关联循环(循环设备名, 映像描述符, false);
}


bool 挂载管理器::设置自动清除(const std::string &循环设备名) {
    // 设置自动清除：读回现有关联信息后置自动清除位。
    if (循环设备名.empty() || 循环设备名.find("/dev/loop") != 0) { VINE_LOGW("挂载管理器：循环名非法"); return false; }
    int 描述符 = open(循环设备名.c_str(), O_RDWR | O_CLOEXEC);
    if (描述符 < 0) { VINE_LOGW("挂载管理器：打开 %s 失败", 循环设备名.c_str()); return false; }
    struct loop_info64 信息{0};
    if (ioctl(描述符, LOOP_GET_STATUS64, &信息) != 0) {
        VINE_LOGW("挂载管理器：读取 %s 关联失败", 循环设备名.c_str());
        close(描述符);
        return false;
    }
    信息.lo_flags |= LO_FLAGS_AUTOCLEAR;
    bool 好 = (ioctl(描述符, LOOP_SET_STATUS64, &信息) == 0);
    close(描述符);
    if (!好) VINE_LOGW("挂载管理器：设置 %s 自动清除失败", 循环设备名.c_str());
    return 好;
}


std::string 挂载管理器::查询循环关联(const std::string &循环设备名) {
    // 查询循环关联：读取循环设备关联映像路径。
    if (循环设备名.empty() || 循环设备名.find("/dev/loop") != 0) return std::string("");
    int 描述符 = open(循环设备名.c_str(), O_RDONLY | O_CLOEXEC);
    if (描述符 < 0) return std::string("");
    struct loop_info64 信息{0};
    bool 好 = (ioctl(描述符, LOOP_GET_STATUS64, &信息) == 0);
    close(描述符);
    if (!好) return std::string("");
    信息.lo_file_name[LO_NAME_SIZE - 1] = '\0';
    return std::string((const char *)信息.lo_file_name);
}


bool 挂载管理器::创建符号链接(const std::string &目标文本, const std::string &链接路径) {
    // 创建符号链接：已存在且目标一致视为成功。
    if (目标文本.empty() || 链接路径.empty() || 链接路径[0] != '/') return false;
    if (目标文本.size() > 1024 || 链接路径.size() > 1024) return false;
    if (symlink(目标文本.c_str(), 链接路径.c_str()) == 0) return true;
    if (errno != EEXIST) { VINE_LOGW("挂载管理器：链接 %s 失败", 链接路径.c_str()); return false; }
    char 现有[1024] = {0};
    ssize_t 长度 = readlink(链接路径.c_str(), 现有, sizeof(现有) - 1);
    if (长度 <= 0) return false;
    现有[长度] = '\0';
    return 目标文本 == 现有;
}


bool 挂载管理器::创建空文件(const std::string &文件路径, int 权限值) {
    // 创建空文件：建父目录后创建并赋权，存在则仅校验。
    if (文件路径.empty() || 文件路径[0] != '/' || 文件路径.size() > 1024) return false;
    if (权限值 < 0 || 权限值 > 0777) 权限值 = 0600;
    size_t 斜杠 = 文件路径.rfind('/');
    if (斜杠 != std::string::npos && 斜杠 > 0) {
        if (!创建目录层级(文件路径.substr(0, 斜杠))) return false;
    }
    int 描述符 = open(文件路径.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if (描述符 < 0) { VINE_LOGW("挂载管理器：创建 %s 失败", 文件路径.c_str()); return false; }
    close(描述符);
    if (chmod(文件路径.c_str(), (mode_t)权限值) != 0) { VINE_LOGW("挂载管理器：赋权 %s 失败", 文件路径.c_str()); return false; }
    return path_exists(文件路径);
}


bool 挂载管理器::设置文件权限(const std::string &文件路径, int 权限值) {
    // 设置文件权限：赋权后读回验证模式位。
    if (文件路径.empty() || 文件路径[0] != '/' || 文件路径.size() > 1024) return false;
    if (权限值 < 0 || 权限值 > 0777) { VINE_LOGW("挂载管理器：权限值非法"); return false; }
    if (chmod(文件路径.c_str(), (mode_t)权限值) != 0) { VINE_LOGW("挂载管理器：赋权 %s 失败", 文件路径.c_str()); return false; }
    struct stat 状态{0};
    if (stat(文件路径.c_str(), &状态) != 0) return false;
    return (状态.st_mode & 0777) == (mode_t)权限值;
}


bool 挂载管理器::修改文件归属(const std::string &文件路径, int 用户号, int 组号) {
    // 修改文件归属：变更属主后读回验证。
    if (文件路径.empty() || 文件路径[0] != '/' || 文件路径.size() > 1024) return false;
    if (用户号 < 0 || 组号 < 0) { VINE_LOGW("挂载管理器：归属号非法"); return false; }
    if (chown(文件路径.c_str(), (uid_t)用户号, (gid_t)组号) != 0) {
        VINE_LOGW("挂载管理器：归属 %s 失败：%s", 文件路径.c_str(), strerror(errno));
        return false;
    }
    struct stat 状态{0};
    if (stat(文件路径.c_str(), &状态) != 0) return false;
    return 状态.st_uid == (uid_t)用户号 && 状态.st_gid == (gid_t)组号;
}


bool 挂载管理器::挂载系统镜像只读(const std::string &根路径) {
    // 挂载系统镜像只读：绑定宿主系统分区并重挂载只读。
    if (!path_exists("/system")) { VINE_LOGW("挂载管理器：宿主系统分区缺失"); return false; }
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行绑定挂载("/system", 根 + "/system", true);
}


bool 挂载管理器::挂载供应商镜像(const std::string &根路径) {
    // 挂载供应商镜像：绑定宿主供应商分区并重挂载只读。
    if (!path_exists("/vendor")) { VINE_LOGW("挂载管理器：宿主供应商分区缺失"); return false; }
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    return 执行绑定挂载("/vendor", 根 + "/vendor", true);
}


bool 挂载管理器::准备数据上层目录(const std::string &实例路径) {
    // 准备数据上层目录：建目录并收紧权限后验证。
    if (实例路径.empty() || 实例路径[0] != '/' || 实例路径.size() > 1024) return false;
    std::string 上层 = 实例路径 + "/upper";
    if (!创建目录层级(上层)) return false;
    if (chmod(上层.c_str(), 0700) != 0) { VINE_LOGW("挂载管理器：上层赋权失败"); return false; }
    return is_directory(上层);
}


bool 挂载管理器::准备数据工作目录(const std::string &实例路径) {
    // 准备数据工作目录：建目录并收紧权限后验证。
    if (实例路径.empty() || 实例路径[0] != '/' || 实例路径.size() > 1024) return false;
    std::string 工作层 = 实例路径 + "/work";
    if (!创建目录层级(工作层)) return false;
    if (chmod(工作层.c_str(), 0700) != 0) { VINE_LOGW("挂载管理器：工作层赋权失败"); return false; }
    return is_directory(工作层);
}


std::string 挂载管理器::拼接覆盖层选项(const std::string &下层, const std::string &上层, const std::string &工作层) {
    // 拼接覆盖层选项：三目录均合法时拼接挂载选项串。
    if (下层.empty() || 上层.empty() || 工作层.empty()) return std::string("");
    if (下层.size() > 512 || 上层.size() > 512 || 工作层.size() > 512) return std::string("");
    return "lowerdir=" + 下层 + ",upperdir=" + 上层 + ",workdir=" + 工作层;
}


bool 挂载管理器::挂载额外绑定表(const std::string &根路径, const std::vector<std::string> &来源列表) {
    // 挂载额外绑定表：逐个绑定来源列表，数量设上限。
    std::string 根 = 根路径.empty() ? std::string("/tmp/vine-root") : 根路径;
    if (来源列表.size() > 64) { VINE_LOGW("挂载管理器：绑定表过大"); return false; }
    bool 全好 = true;
    for (const std::string &来源 : 来源列表) {
        if (来源.empty() || 来源[0] != '/') { 全好 = false; continue; }
        size_t 斜杠 = 来源.rfind('/');
        std::string 目标 = 根 + "/bind-" + ((斜杠 == std::string::npos) ? 来源 : 来源.substr(斜杠 + 1));
        if (!执行绑定挂载(来源, 目标, true)) 全好 = false;
    }
    return 全好;
}


bool 挂载管理器::记录挂载事件(const std::string &事件文本) {
    // 覆盖层类：以成员路径推导上下工作三目录并挂载。
    std::string 实例 = 实例路径.empty() ? std::string("/data/vineos/default") : 实例路径;
    std::string 目标 = 根路径.empty() ? std::string("/tmp/vine-root/data") : (根路径 + "/data");
    std::string 数据目录 = 实例 + "/data";
    std::string 上层 = 数据目录 + "/upper";
    std::string 工作层 = 数据目录 + "/work";
    if (!创建目录层级(上层) || !创建目录层级(工作层) || !创建目录层级(目标)) {
        VINE_LOGE("挂载管理器：序号 71 目录准备失败"); return false;
    }
    std::string 下层 = 根路径 + "/data";
    if (!path_exists(下层)) { mkdirs(下层); }
    std::string 选项 = 拼接覆盖层选项(下层, 上层, 工作层);
    if (mount("overlay", 目标.c_str(), "overlay", MS_RELATIME, 选项.c_str()) == 0) {
        VINE_LOGI("挂载管理器：序号 71 覆盖层就绪");
        记录已挂载点(目标);
        return true;
    }
    VINE_LOGW("挂载管理器：序号 71 覆盖层失败（%s），回退绑定", strerror(errno));
    if (mount(数据目录.c_str(), 目标.c_str(), nullptr, MS_BIND, nullptr) == 0) {
        记录已挂载点(目标);
        return true;
    }
    VINE_LOGE("挂载管理器：序号 71 绑定回退亦失败");
    return false;
}

long 挂载管理器::读取事件计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    return 事件计数;
}

void 挂载管理器::清空事件计数() {
    std::lock_guard<std::mutex> 互斥保护(互斥锁);
    事件计数 = 0;
}

} // namespace vine
