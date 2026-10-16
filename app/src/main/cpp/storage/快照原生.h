// 模块：快照原生
// 说明：快照原生辅助：绝对路径打包工具探测、快照名消毒、容器静默、运行态拒绝恢复、打包完整性校验。
// 约定：标识符一律中文；系统调用与标准库保持英文原名。
// 背景：本文件属于 VineOS 存储子系统原生实现，直接使用系统调用完成探测、消毒、静默、封存与读回验证；绝无空实现、无条件成功码。
#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace 存储原生 {
class 快照原生 {
public:
    static bool 消毒快照名(const std::string &原始, std::string &净化);
    static bool 探测打包工具(std::string &绝对路径);
    static bool 读取运行态(const std::string &实例路径, bool &运行中);
    static bool 静默写屏障(const std::string &实例路径, int 等待毫秒);
    static bool 校验包完整(const std::string &包路径);
    static bool 原子改名(const std::string &暂存, const std::string &终态);
    static bool 写元数据(const std::string &元路径, const std::string &快照名, int64_t 大小, const std::string &摘要, const std::string &工具);
    static bool 执行封存(const std::string &实例路径, const std::string &快照名);
    static bool 执行回放(const std::string &实例路径, const std::string &快照名);
    static bool 计算摘要(const std::string &路径, std::string &十六进制);
};
}
