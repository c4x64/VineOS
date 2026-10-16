// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：线程转储器——枚举线程、解析状态、采集等待通道与调度信息
// 数据来源：/proc/self/task/*/stat、statm、wchan、stack、comm
// 线程安全：只读采集，无共享可变状态
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
struct 线程信息 {
  int 线程号=0; std::string 线程名; char 状态='?'; std::string 状态描述;
  long long 用户节拍=0; long long 内核节拍=0; long long 虚拟内存千字节=0; long long 常驻千字节=0;
  long long 优先级=0; long long 线程数=0; std::string 等待通道; std::string 内核栈摘要;
  unsigned long long 调度等待毫秒=0; bool 是否主线程=false;
  std::string 转为日志行() const; std::string 转为简表() const; bool 是否异常() const; };
struct 线程转储 {
  long long 采集时刻毫秒=0; long long 进程标识=0; std::vector<线程信息> 线程列表;
  std::string 转为文本() const; std::string 转为摘要() const; size_t 运行中数量() const; size_t 睡眠中数量() const; };
class 线程转储器 {
 public:
  static 线程转储器& 获取实例();
  // 判断运行态（R）：比较状态字符
  bool 是否运行态(char 状态) const;
  // 统计运行态数量
  size_t 统计运行态数量(const 线程转储& 转储) const;
  // 描述运行态含义
  std::string 描述运行态() const;
  // 判断睡眠态（S）：比较状态字符
  bool 是否睡眠态(char 状态) const;
  // 统计睡眠态数量
  size_t 统计睡眠态数量(const 线程转储& 转储) const;
  // 描述睡眠态含义
  std::string 描述睡眠态() const;
  // 判断磁盘睡眠（D）：比较状态字符
  bool 是否磁盘睡眠(char 状态) const;
  // 统计磁盘睡眠数量
  size_t 统计磁盘睡眠数量(const 线程转储& 转储) const;
  // 描述磁盘睡眠含义
  std::string 描述磁盘睡眠() const;
  // 判断停止态（T）：比较状态字符
  bool 是否停止态(char 状态) const;
  // 统计停止态数量
  size_t 统计停止态数量(const 线程转储& 转储) const;
  // 描述停止态含义
  std::string 描述停止态() const;
  // 判断追踪停止（t）：比较状态字符
  bool 是否追踪停止(char 状态) const;
  // 统计追踪停止数量
  size_t 统计追踪停止数量(const 线程转储& 转储) const;
  // 描述追踪停止含义
  std::string 描述追踪停止() const;
  // 判断僵尸态（Z）：比较状态字符
  bool 是否僵尸态(char 状态) const;
  // 统计僵尸态数量
  size_t 统计僵尸态数量(const 线程转储& 转储) const;
  // 描述僵尸态含义
  std::string 描述僵尸态() const;
  // 判断死亡态（X）：比较状态字符
  bool 是否死亡态(char 状态) const;
  // 统计死亡态数量
  size_t 统计死亡态数量(const 线程转储& 转储) const;
  // 描述死亡态含义
  std::string 描述死亡态() const;
  // 判断空闲内核（I）：比较状态字符
  bool 是否空闲内核(char 状态) const;
  // 统计空闲内核数量
  size_t 统计空闲内核数量(const 线程转储& 转储) const;
  // 描述空闲内核含义
  std::string 描述空闲内核() const;
  线程转储 采集转储();
  bool 枚举线程号(std::vector<int>& 输出);
  bool 读取单线程(int 线程号, 线程信息& 输出);
  bool 解析状态行(const std::string& 内容, 线程信息& 输出);
  bool 读取等待通道(int 线程号, std::string& 输出);
  bool 读取内核栈(int 线程号, std::string& 输出);
  bool 读取线程名(int 线程号, std::string& 输出);
  std::vector<线程信息> 筛选异常线程(const 线程转储& 转储);
  std::string 生成可读报告(const 线程转储& 转储);
  bool 落盘(const 线程转储& 转储, const std::string& 路径);
  bool 通用是否(char 实际, char 目标) const;
  size_t 通用统计数量(char 目标, const 线程转储& 转储) const;
  std::string 通用描述(char 目标) const;
  static char 状态表符号(size_t 下标);
  static const char* 状态表名称(size_t 下标);
  static size_t 状态表大小();
 private: 线程转储器(); };
std::string 状态字符转描述(char 状态);
long long 当前毫秒转储();
}  // namespace 诊断
}  // namespace vine
