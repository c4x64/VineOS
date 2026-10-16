// ==================================================================
// 模块：进程执行器
// 说明：fork/exec 绝对路径解析、PATH 查找、管道捕获、超时终止。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// 全部真实实现，禁止留空。
// ==================================================================
#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <sys/types.h>
namespace vine { namespace 安全 {
struct 执行配置 {
  std::string 工作目录;
  std::map<std::string,std::string> 环境变量;
  bool 合并错误流 = true;
  bool 允许相对路径 = false;
  long 超时毫秒 = 30000;
  size_t 输出上限 = 4194304;
  bool 关闭标准输入 = true;
};
struct 执行结果 {
  bool 启动成功 = false;
  bool 超时 = false;
  bool 被信号终止 = false;
  int 退出码 = -1;
  int 终止信号 = 0;
  std::string 标准输出;
  std::string 标准错误;
  std::string 错误描述;
  long 耗时毫秒 = 0;
  pid_t 进程号 = -1;
};
class 进程执行器 {
 public:
  static bool 是否绝对路径(const std::string& 路径);
  static bool 是否可执行文件(const std::string& 路径);
  static std::optional<std::string> 在路径中查找(const std::string& 命令名, const std::string& 路径表);
  static std::optional<std::string> 解析可执行路径(const std::string& 命令, bool 允许相对);
  static std::vector<std::string> 切分路径表(const std::string& 路径表);
  static std::string 取默认路径表();
  static pid_t 启动进程(const std::vector<std::string>& 参数, const 执行配置& 配置, int* 读端输出);
  static 执行结果 执行并等待(const std::vector<std::string>& 参数, const 执行配置& 配置);
  static 执行结果 执行壳命令(const std::string& 命令行, const 执行配置& 配置);
  static bool 等待进程(pid_t 进程号, long 超时毫秒, int* 状态, bool* 超时);
  static bool 终止进程(pid_t 进程号, int 超时毫秒);
  static bool 进程存活(pid_t 进程号);
  static int 取退出码(int 状态);
  static bool 正常退出(int 状态);
  static bool 被信号杀死(int 状态, int* 信号);
  static std::string 信号名称(int 信号值);
  static bool 发送信号(pid_t 进程号, int 信号值);
  static long 单调毫秒();
  static std::vector<pid_t> 列出子进程(pid_t 父进程);
  static bool 杀进程树(pid_t 根进程, int 超时毫秒);
  static std::optional<std::string> 读进程状态(pid_t 进程号);
  static bool 为僵尸进程(pid_t 进程号);
  static int 收割僵尸(pid_t 进程号);
  static std::string 转义参数(const std::string& 参数);
  static std::string 连接命令行(const std::vector<std::string>& 参数);
  static std::vector<std::string> 解析命令行(const std::string& 命令行);
  static bool 校验参数安全(const std::vector<std::string>& 参数, std::string* 原因);
  static size_t 估计输出内存(size_t 已读, size_t 上限);
};
} }
