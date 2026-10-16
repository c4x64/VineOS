// ==================================================================
// 模块：沙箱检查器
// 说明：SELinux／命名空间／挂载／身份／能力综合检查。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// ====================================================================
#pragma once
#include <string>
#include <vector>
#include <optional>
namespace vine { namespace 安全 {
struct 沙箱状态 {
  bool 已隔离 = false;
  bool 有命名空间 = false;
  bool 有沙箱挂载 = false;
  bool 身份受限 = false;
  bool 无可写系统区 = false;
  std::string 详情;
  int 通过项 = 0;
  int 总项 = 0;
};
struct 挂载条目 {
  std::string 设备;
  std::string 挂载点;
  std::string 类型;
  std::string 选项;
};
class 沙箱检查器 {
 public:
  static 沙箱状态 综合检查();
  static bool 检查命名空间隔离();
  static bool 检查沙箱挂载();
  static bool 检查身份受限();
  static bool 检查系统区只读();
  static bool 检查环境变量干净();
  static bool 检查工作目录在沙箱内(const std::string& 基目录);
  static std::vector<挂载条目> 读取挂载表();
  static bool 挂载点只读(const std::string& 挂载点);
  static bool 路径可写(const std::string& 路径);
  static bool 为沙箱进程();
  static std::optional<std::string> 读状态字段(pid_t 进程, const std::string& 字段);
  static bool 同一命名空间(pid_t 甲, pid_t 乙, const std::string& 种类);
  static std::string 读命名空间链接(pid_t 进程, const std::string& 种类);
  static bool 检查单实例锁(const std::string& 锁路径);
  static bool 检查调试器附加();
  static bool 检查危险挂载();
  static bool 检查可写系统目录();
  static bool 检查根权限逃逸();
  static bool 检查能力位过大();
  static int 统计可写挂载();
  static bool 校验基目录合法(const std::string& 基目录);
  static std::string 评估等级(const 沙箱状态& 状态);
  static bool 整体通过(const 沙箱状态& 状态);
  static std::string 报告文本(const 沙箱状态& 状态);
  static bool 写检查报告(const std::string& 路径, const 沙箱状态& 状态);
  static long 取进程启动时间(pid_t 进程);
  static bool 为可疑工作目录(const std::string& 目录);
  static int 评分(const 沙箱状态& 状态);
};
} }
