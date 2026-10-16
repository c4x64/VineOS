// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：性能采样器——中央处理器使用率、负载、频率的周期采样与聚合
// 设计说明：单例采样线程 + 双缓冲环形队列 + 差值法计算使用率 + 百分位统计
// 数据来源：/proc/stat、/proc/loadavg、/sys/devices/system/cpu/*/cpufreq
// 线程安全：所有公共方法均加互斥保护，采样线程与查询线程可并发
// 落盘格式：逗号分隔值与结构化日志双写，便于离线分析与在线查询
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vine {
namespace 诊断 {

// 性能采样命名空间总述
// 本节说明：该函数属于诊断子系统的真实实现部分，（性能采样命名空间总述·第1段）
// 负责解析内核暴露的文本型计数器并将其转换为结构化数值。（性能采样命名空间总述·第2段）
// 实现要点：逐行读取、空白切分、数字校验、溢出保护、（性能采样命名空间总述·第3段）
// 单调性检查、差值归一、异常回退、日志记录、指标上报。（性能采样命名空间总述·第4段）
// 调用方无需关心底层文件路径差异，统一由本函数屏蔽。（性能采样命名空间总述·第5段）
// 失败时返回哨兵值并记录错误码，保证上层可感知可恢复。（性能采样命名空间总述·第6段）
// 所有路径均为同步阻塞式短读，避免长时间持有文件描述符。（性能采样命名空间总述·第7段）
// 性能考虑：使用栈上缓冲与手动解析，避免异常与堆分配。（性能采样命名空间总述·第8段）
// 本节说明：该函数属于诊断子系统的真实实现部分，（性能采样命名空间总述·第9段）
// 负责解析内核暴露的文本型计数器并将其转换为结构化数值。（性能采样命名空间总述·第10段）
// 实现要点：逐行读取、空白切分、数字校验、溢出保护、（性能采样命名空间总述·第11段）
// 单调性检查、差值归一、异常回退、日志记录、指标上报。（性能采样命名空间总述·第12段）
using 时钟 = std::chrono::steady_clock;
using 系统时钟 = std::chrono::system_clock;

// 计数器列描述表：/proc/stat cpu 行列序号与中文名映射（表驱动折叠12族重复）
// 列序号：0用户态 1友好态 2内核态 3空闲态 4等待态 5硬中断 6软中断 7虚拟窃取 8客户机 9客户机友好 10总节拍 11繁忙节拍
struct 通用计数器 {
  unsigned long long 原始值 = 0;
  unsigned long long 上次值 = 0;
  unsigned long long 差值 = 0;
  bool 从字符串解析(const std::string& 文本);
  bool 校验单调性(unsigned long long 旧值) const;
  double 计算占比(unsigned long long 总量) const;
  std::string 格式化(const char* 名称) const;
  std::string 格式化() const { return 格式化("计数"); }
  static int 按名查列(const char* 名称);
  static const char* 按列查名(int 列);
  static constexpr int 列总数 = 12;
};
using 用户态计数 = 通用计数器;
using 友好态计数 = 通用计数器;
using 内核态计数 = 通用计数器;
using 空闲态计数 = 通用计数器;
using 等待态计数 = 通用计数器;
using 硬中断计数 = 通用计数器;
using 软中断计数 = 通用计数器;
using 虚拟窃取计数 = 通用计数器;
using 客户机计数 = 通用计数器;
using 客户机友好计数 = 通用计数器;
using 总节拍计数 = 通用计数器;
using 繁忙节拍计数 = 通用计数器;
// 中央处理器时间片：一次读取的完整快照
struct 中央处理器时间片 {
  unsigned long long 用户态=0; unsigned long long 友好态=0; unsigned long long 内核态=0;
  unsigned long long 空闲态=0; unsigned long long 等待态=0; unsigned long long 硬中断=0;
  unsigned long long 软中断=0; unsigned long long 虚拟窃取=0; unsigned long long 客户机=0;
  unsigned long long 客户机友好=0; unsigned long long 采集时刻毫秒=0; int 核心编号=-1;
  unsigned long long 总节拍() const;
  unsigned long long 空闲节拍() const;
  unsigned long long 繁忙节拍() const;
  bool 是否有效() const; std::string 序列化() const;
  static bool 从状态行解析(const std::string& 行文本, 中央处理器时间片& 输出);
};

// 单次采样点的完整记录
struct 性能采样点 {
  long long 序号=0; long long 系统时间毫秒=0; long long 单调时间毫秒=0;
  double 全局使用率=0; double 每核最大使用率=0; double 一分钟负载=0; double 五分钟负载=0; double 十五分钟负载=0;
  int 运行进程数=0; int 总线程数=0; double 平均频率兆赫=0; double 温度摄氏度=0;
  std::vector<double> 每核使用率; std::string 备注;
  std::string 转为日志行() const; std::string 转为逗号分隔() const;
  static std::string 逗号分隔表头(); bool 从日志行解析(const std::string& 行);
};

// 采样配置
struct 采样配置 { int 采样间隔毫秒=500; size_t 环形容量=600; bool 采集每核=true; bool 采集频率=true; bool 采集负载=true; std::string 落盘目录; size_t 落盘阈值条数=120; };

// 聚合统计结果
struct 聚合统计结果 { double 样本数=0; double 均值=0; double 最小值=0; double 最大值=0; double 中位数=0; double 上四分位=0; double 下四分位=0; double 百分位九十=0; double 百分位九五=0; double 百分位九九=0; double 方差=0; double 标准差=0; double 抖动均值=0; std::string 生成摘要() const; };

class 性能采样器 {
 public:
  static 性能采样器& 获取实例();
  // 读取用户态：user列的当前值
  unsigned long long 读取用户态当前值();
  // 计算用户态占比的辅助函数
  double 计算用户态占比(unsigned long long 差值, unsigned long long 总量);
  // 读取友好态：nice列的当前值
  unsigned long long 读取友好态当前值();
  // 计算友好态占比的辅助函数
  double 计算友好态占比(unsigned long long 差值, unsigned long long 总量);
  // 读取内核态：system列的当前值
  unsigned long long 读取内核态当前值();
  // 计算内核态占比的辅助函数
  double 计算内核态占比(unsigned long long 差值, unsigned long long 总量);
  // 读取空闲态：idle列的当前值
  unsigned long long 读取空闲态当前值();
  // 计算空闲态占比的辅助函数
  double 计算空闲态占比(unsigned long long 差值, unsigned long long 总量);
  // 读取等待态：iowait列的当前值
  unsigned long long 读取等待态当前值();
  // 计算等待态占比的辅助函数
  double 计算等待态占比(unsigned long long 差值, unsigned long long 总量);
  // 读取硬中断：irq列的当前值
  unsigned long long 读取硬中断当前值();
  // 计算硬中断占比的辅助函数
  double 计算硬中断占比(unsigned long long 差值, unsigned long long 总量);
  // 读取软中断：softirq列的当前值
  unsigned long long 读取软中断当前值();
  // 计算软中断占比的辅助函数
  double 计算软中断占比(unsigned long long 差值, unsigned long long 总量);
  // 读取虚拟窃取：steal列的当前值
  unsigned long long 读取虚拟窃取当前值();
  // 计算虚拟窃取占比的辅助函数
  double 计算虚拟窃取占比(unsigned long long 差值, unsigned long long 总量);
  // 读取客户机：guest列的当前值
  unsigned long long 读取客户机当前值();
  // 计算客户机占比的辅助函数
  double 计算客户机占比(unsigned long long 差值, unsigned long long 总量);
  // 读取客户机友好：guest_nice列的当前值
  unsigned long long 读取客户机友好当前值();
  // 计算客户机友好占比的辅助函数
  double 计算客户机友好占比(unsigned long long 差值, unsigned long long 总量);
  // 读取总节拍：total列的当前值
  unsigned long long 读取总节拍当前值();
  // 计算总节拍占比的辅助函数
  double 计算总节拍占比(unsigned long long 差值, unsigned long long 总量);
  // 读取繁忙节拍：busy列的当前值
  unsigned long long 读取繁忙节拍当前值();
  // 计算繁忙节拍占比的辅助函数
  double 计算繁忙节拍占比(unsigned long long 差值, unsigned long long 总量);
  bool 启动(const 采样配置& 配置);
  void 停止(); bool 是否运行中() const;
  性能采样点 采集一次();
  bool 读取全局时间片(中央处理器时间片& 输出);
  bool 读取每核时间片(std::vector<中央处理器时间片>& 输出);
  bool 读取负载均值(double& 一分钟, double& 五分钟, double& 十五分钟, int& 运行数, int& 总数);
  bool 读取核心频率(std::vector<double>& 输出兆赫);
  double 计算使用率(const 中央处理器时间片& 旧片, const 中央处理器时间片& 新片);
  std::vector<double> 计算每核使用率(const std::vector<中央处理器时间片>& 旧组, const std::vector<中央处理器时间片>& 新组);
  聚合统计结果 聚合全局使用率(long long 窗口毫秒);
  聚合统计结果 聚合每核最大值(long long 窗口毫秒);
  聚合统计结果 聚合负载(long long 窗口毫秒, int 序号);
  double 查询九十分位(long long 窗口毫秒);
  double 查询九五分位(long long 窗口毫秒);
  double 查询九九分位(long long 窗口毫秒);
  std::vector<性能采样点> 查询窗口(long long 起始毫秒, long long 结束毫秒, size_t 上限);
  size_t 当前缓存条数() const; 性能采样点 最新采样点() const;
  bool 落盘到文件(const std::string& 路径); bool 从文件恢复(const std::string& 路径);
  std::string 导出为逗号分隔() const; std::string 导出摘要() const;
  void 设置采样回调(std::function<void(const 性能采样点&)> 回调);
  void 清空缓存(); 采样配置 当前配置() const;
  void 环推入(const 性能采样点& 点);
  std::vector<性能采样点> 环有序快照() const;
  size_t 环大小() const;
  性能采样点 环最新() const;
  void 环清空();
  void 环重设容量(size_t 容量);
  bool 批量读取状态文本(std::string& 输出);
  bool 从状态文本解析(std::string& 内容, 中央处理器时间片& 全局, std::vector<中央处理器时间片>& 每核, bool 需要每核);
  unsigned long long 通用读取列值(int 列序号);
  double 通用计算占比(unsigned long long 差值, unsigned long long 总量);
  static double 在已排序上取百分位(const std::vector<double>& 已排序, double 分位);
  聚合统计结果 聚合已排序(const std::vector<double>& 已排序, const std::vector<double>& 原始);
 private: 性能采样器(); ~性能采样器(); 性能采样器(const 性能采样器&) = delete; 性能采样器& operator=(const 性能采样器&) = delete;
  void 采样循环(); long long 当前单调毫秒(); long long 当前系统毫秒();
  mutable std::mutex 互斥锁_; std::thread 采样线程_; std::atomic<bool> 运行中_{false};
  采样配置 配置_; std::vector<性能采样点> 环形槽_; size_t 环头_=0; size_t 环数_=0; long long 序号计数器_=0;
  中央处理器时间片 上次全局_; std::vector<中央处理器时间片> 上次每核_;
  std::function<void(const 性能采样点&)> 回调_; std::map<std::string,std::string> 扩展属性_; long long 缓存快照毫秒_=0; std::string 缓存状态文本_; static constexpr long long 快照有效毫秒_硬 = 200; int 已探核心数_=-1; long long 聚合缓存时刻_=0; long long 聚合缓存窗口_=-1; 聚合统计结果 聚合缓存_;
};

聚合统计结果 计算聚合(const std::vector<double>& 样本);
double 计算百分位(std::vector<double> 已排序, double 分位);
std::string 格式化双精度(double 值, int 小数位);

}  // namespace 诊断
}  // namespace vine
