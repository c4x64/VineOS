// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：系统探针集——处理器/内存/网络/磁盘/电池/温度统一采集
// 数据来源：/proc/cpuinfo、/proc/net/dev、/proc/diskstats、sysfs
// 线程安全：无状态静态采集
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
struct 处理器描述 { std::string 架构; int 核心数=0; double 最高频率兆赫=0; std::vector<double> 各核频率; std::string 特性; };
struct 网络计数 { std::string 接口名; unsigned long long 接收字节=0; unsigned long long 发送字节=0; unsigned long long 接收包=0; unsigned long long 发送包=0; };
struct 系统快照 { long long 时刻毫秒=0; double 一分钟负载=0; double 五分钟负载=0; double 十五分钟负载=0; long long 运行时间秒=0; long long 空闲时间秒=0; int 打开文件数=0; int 文件上限=0; std::string 转为日志行() const; };
class 系统探针集 {
 public:
  static 系统探针集& 获取实例();
  // 采集处理器信息（cpuinfo）
  std::string 采集处理器信息();
  // 解析处理器信息文本
  bool 解析处理器信息(const std::string& 文本, std::string& 输出);
  // 采集内存信息（meminfo）
  std::string 采集内存信息();
  // 解析内存信息文本
  bool 解析内存信息(const std::string& 文本, std::string& 输出);
  // 采集平均负载（loadavg）
  std::string 采集平均负载();
  // 解析平均负载文本
  bool 解析平均负载(const std::string& 文本, std::string& 输出);
  // 采集运行时间（uptime）
  std::string 采集运行时间();
  // 解析运行时间文本
  bool 解析运行时间(const std::string& 文本, std::string& 输出);
  // 采集网络设备（net_dev）
  std::string 采集网络设备();
  // 解析网络设备文本
  bool 解析网络设备(const std::string& 文本, std::string& 输出);
  // 采集磁盘统计（diskstats）
  std::string 采集磁盘统计();
  // 解析磁盘统计文本
  bool 解析磁盘统计(const std::string& 文本, std::string& 输出);
  // 采集文件描述符（fd_count）
  std::string 采集文件描述符();
  // 解析文件描述符文本
  bool 解析文件描述符(const std::string& 文本, std::string& 输出);
  // 采集进程状态（proc_stat）
  std::string 采集进程状态();
  // 解析进程状态文本
  bool 解析进程状态(const std::string& 文本, std::string& 输出);
  // 采集电池状态（battery）
  std::string 采集电池状态();
  // 解析电池状态文本
  bool 解析电池状态(const std::string& 文本, std::string& 输出);
  // 采集温度区域（thermal）
  std::string 采集温度区域();
  // 解析温度区域文本
  bool 解析温度区域(const std::string& 文本, std::string& 输出);
  // 采集频率表（freq_table）
  std::string 采集频率表();
  // 解析频率表文本
  bool 解析频率表(const std::string& 文本, std::string& 输出);
  // 采集内存压力（pressure）
  std::string 采集内存压力();
  // 解析内存压力文本
  bool 解析内存压力(const std::string& 文本, std::string& 输出);
  // 采集中断统计（interrupts）
  std::string 采集中断统计();
  // 解析中断统计文本
  bool 解析中断统计(const std::string& 文本, std::string& 输出);
  // 采集上下文切换（ctxt）
  std::string 采集上下文切换();
  // 解析上下文切换文本
  bool 解析上下文切换(const std::string& 文本, std::string& 输出);
  // 采集进程数（processes）
  std::string 采集进程数();
  // 解析进程数文本
  bool 解析进程数(const std::string& 文本, std::string& 输出);
  处理器描述 查询处理器();
  std::vector<网络计数> 查询网络();
  系统快照 采集系统快照();
  std::map<std::string,std::string> 采集全部();
  std::string 生成报告();
  struct 探针描述 { const char* 键; const char* 路径; const char* 空告警; size_t 截断; };
  static const 探针描述* 探针表(size_t& 数量);
  static const 探针描述* 按键查找(const std::string& 键);
  std::string 通用采集(const 探针描述& 描述);
  bool 通用解析(const std::string& 文本, std::string& 输出, size_t 截断);
  std::string 批量读文件(const std::string& 路径, size_t 上限);
  private: 系统探针集(); std::string 读文件(const std::string& 路径); };
}  // namespace 诊断
}  // namespace vine
