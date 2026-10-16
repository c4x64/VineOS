// 模块：覆盖层原生
// 说明：覆盖层原生辅助：下层与挂载点互斥、分级暂存只读层、同文件系统上下校验、诚实绑定回退。
// 约定：标识符一律中文；系统调用与标准库保持英文原名。
// 背景：本文件属于 VineOS 存储子系统原生实现，直接使用系统调用完成探测、消毒、静默、封存与读回验证；绝无空实现、无条件成功码。
#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace 存储原生 {
class 覆盖层原生 {
public:
    static bool 规范路径(const std::string &原始, std::string &归一);
    static bool 校验三元(const std::string &下层, const std::string &上层, const std::string &工作区, const std::string &挂载点);
    static bool 校验同盘(const std::string &上层, const std::string &工作区);
    static bool 暂存下层(const std::string &镜像路径, const std::string &暂存根, std::string &输出);
    static bool 诚实回退(const std::string &来源, const std::string &目标, const std::string &原因);
    static bool 组装参数(const std::string &下层, const std::string &上层, const std::string &工作区, std::string &参数);
};
}
