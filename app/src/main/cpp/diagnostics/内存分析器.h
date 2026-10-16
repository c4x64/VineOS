// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：内存分析器——内存快照、smaps 聚合、泄漏趋势判断
// 数据来源：/proc/meminfo、/proc/self/smaps、/proc/self/maps、/proc/self/status
// 线程安全：互斥保护，可并发调用
// 落盘格式：结构化文本 + 校验和
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
// 内存快照：一次采集的完整视图
struct 内存快照 {
  long long 采集时刻毫秒=0; long long 进程标识=0;
  long long 内存总量千字节=0; // MemTotal
  long long 内存空闲千字节=0; // MemFree
  long long 内存可用千字节=0; // MemAvailable
  long long 缓冲千字节=0; // Buffers
  long long 缓存千字节=0; // Cached
  long long 交换缓存千字节=0; // SwapCached
  long long 活跃内存千字节=0; // Active
  long long 非活跃内存千字节=0; // Inactive
  long long 活跃匿名千字节=0; // Active(anon)
  long long 非活跃匿名千字节=0; // Inactive(anon)
  long long 活跃文件千字节=0; // Active(file)
  long long 非活跃文件千字节=0; // Inactive(file)
  long long 不可回收千字节=0; // Unevictable
  long long 加锁内存千字节=0; // Mlocked
  long long 交换总量千字节=0; // SwapTotal
  long long 交换空闲千字节=0; // SwapFree
  long long 脏页千字节=0; // Dirty
  long long 写回千字节=0; // Writeback
  long long 匿名页千字节=0; // AnonPages
  long long 映射文件千字节=0; // Mapped
  long long 共享内存千字节=0; // Shmem
  long long 内核slab千字节=0; // Slab
  long long 可回收slab千字节=0; // SReclaimable
  long long 不可回收slab千字节=0; // SUnreclaim
  long long 内核栈千字节=0; // KernelStack
  long long 页表千字节=0; // PageTables
  long long 提交上限千字节=0; // CommitLimit
  long long 已提交千字节=0; // Committed_AS
  long long 巨页总数千字节=0; // HugePages_Total
  long long 巨页空闲千字节=0; // HugePages_Free
  long long 进程常驻千字节=0; long long 进程虚拟千字节=0; long long 进程比例集千字节=0; long long 进程唯一集千字节=0;
  long long 交换已用千字节=0; double 内存使用率=0; double 交换使用率=0;
  std::string 转为日志行() const; std::string 转为逗号分隔() const; static std::string 逗号分隔表头();
  bool 是否有效() const; double 计算压力分数() const; };
struct 映射段信息 { std::string 起始地址; std::string 结束地址; std::string 权限; long long 大小千字节=0; long long 常驻千字节=0; long long 比例集千字节=0; long long 独占千字节=0; std::string 路径; std::string 转为日志行() const; };
struct 内存趋势点 { long long 时刻毫秒=0; long long 常驻千字节=0; long long 比例集千字节=0; long long 可用千字节=0; };
struct 泄漏判定 { bool 疑似泄漏=false; double 每小时增长千字节=0; double 置信度=0; std::string 原因; std::string 生成报告() const; };
class 内存分析器 {
 public:
  static 内存分析器& 获取实例();
  struct 内存字段描述 { const char* 前缀; const char* 键; long long 内存快照::* 成员; };
  static const 内存字段描述* 内存字段表(size_t& 数量);
  static const 内存字段描述* 按前缀查找(const std::string& 行);
  bool 通用解析行(const std::string& 行, const char* 前缀, long long& 输出千字节);
  bool 通用校验合理(long long 值) const;
  void 通用累加到快照(内存快照& 快照, long long 内存快照::* 成员, long long 值);
  // 解析内存总量（MemTotal）行
  bool 解析内存总量行(const std::string& 行, long long& 输出千字节);
  // 校验内存总量合理性
  bool 校验内存总量合理(long long 值) const;
  // 聚合内存总量到快照
  void 累加内存总量到快照(内存快照& 快照, long long 值);
  // 解析内存空闲（MemFree）行
  bool 解析内存空闲行(const std::string& 行, long long& 输出千字节);
  // 校验内存空闲合理性
  bool 校验内存空闲合理(long long 值) const;
  // 聚合内存空闲到快照
  void 累加内存空闲到快照(内存快照& 快照, long long 值);
  // 解析内存可用（MemAvailable）行
  bool 解析内存可用行(const std::string& 行, long long& 输出千字节);
  // 校验内存可用合理性
  bool 校验内存可用合理(long long 值) const;
  // 聚合内存可用到快照
  void 累加内存可用到快照(内存快照& 快照, long long 值);
  // 解析缓冲（Buffers）行
  bool 解析缓冲行(const std::string& 行, long long& 输出千字节);
  // 校验缓冲合理性
  bool 校验缓冲合理(long long 值) const;
  // 聚合缓冲到快照
  void 累加缓冲到快照(内存快照& 快照, long long 值);
  // 解析缓存（Cached）行
  bool 解析缓存行(const std::string& 行, long long& 输出千字节);
  // 校验缓存合理性
  bool 校验缓存合理(long long 值) const;
  // 聚合缓存到快照
  void 累加缓存到快照(内存快照& 快照, long long 值);
  // 解析交换缓存（SwapCached）行
  bool 解析交换缓存行(const std::string& 行, long long& 输出千字节);
  // 校验交换缓存合理性
  bool 校验交换缓存合理(long long 值) const;
  // 聚合交换缓存到快照
  void 累加交换缓存到快照(内存快照& 快照, long long 值);
  // 解析活跃内存（Active）行
  bool 解析活跃内存行(const std::string& 行, long long& 输出千字节);
  // 校验活跃内存合理性
  bool 校验活跃内存合理(long long 值) const;
  // 聚合活跃内存到快照
  void 累加活跃内存到快照(内存快照& 快照, long long 值);
  // 解析非活跃内存（Inactive）行
  bool 解析非活跃内存行(const std::string& 行, long long& 输出千字节);
  // 校验非活跃内存合理性
  bool 校验非活跃内存合理(long long 值) const;
  // 聚合非活跃内存到快照
  void 累加非活跃内存到快照(内存快照& 快照, long long 值);
  // 解析活跃匿名（Active(anon)）行
  bool 解析活跃匿名行(const std::string& 行, long long& 输出千字节);
  // 校验活跃匿名合理性
  bool 校验活跃匿名合理(long long 值) const;
  // 聚合活跃匿名到快照
  void 累加活跃匿名到快照(内存快照& 快照, long long 值);
  // 解析非活跃匿名（Inactive(anon)）行
  bool 解析非活跃匿名行(const std::string& 行, long long& 输出千字节);
  // 校验非活跃匿名合理性
  bool 校验非活跃匿名合理(long long 值) const;
  // 聚合非活跃匿名到快照
  void 累加非活跃匿名到快照(内存快照& 快照, long long 值);
  // 解析活跃文件（Active(file)）行
  bool 解析活跃文件行(const std::string& 行, long long& 输出千字节);
  // 校验活跃文件合理性
  bool 校验活跃文件合理(long long 值) const;
  // 聚合活跃文件到快照
  void 累加活跃文件到快照(内存快照& 快照, long long 值);
  // 解析非活跃文件（Inactive(file)）行
  bool 解析非活跃文件行(const std::string& 行, long long& 输出千字节);
  // 校验非活跃文件合理性
  bool 校验非活跃文件合理(long long 值) const;
  // 聚合非活跃文件到快照
  void 累加非活跃文件到快照(内存快照& 快照, long long 值);
  // 解析不可回收（Unevictable）行
  bool 解析不可回收行(const std::string& 行, long long& 输出千字节);
  // 校验不可回收合理性
  bool 校验不可回收合理(long long 值) const;
  // 聚合不可回收到快照
  void 累加不可回收到快照(内存快照& 快照, long long 值);
  // 解析加锁内存（Mlocked）行
  bool 解析加锁内存行(const std::string& 行, long long& 输出千字节);
  // 校验加锁内存合理性
  bool 校验加锁内存合理(long long 值) const;
  // 聚合加锁内存到快照
  void 累加加锁内存到快照(内存快照& 快照, long long 值);
  // 解析交换总量（SwapTotal）行
  bool 解析交换总量行(const std::string& 行, long long& 输出千字节);
  // 校验交换总量合理性
  bool 校验交换总量合理(long long 值) const;
  // 聚合交换总量到快照
  void 累加交换总量到快照(内存快照& 快照, long long 值);
  // 解析交换空闲（SwapFree）行
  bool 解析交换空闲行(const std::string& 行, long long& 输出千字节);
  // 校验交换空闲合理性
  bool 校验交换空闲合理(long long 值) const;
  // 聚合交换空闲到快照
  void 累加交换空闲到快照(内存快照& 快照, long long 值);
  // 解析脏页（Dirty）行
  bool 解析脏页行(const std::string& 行, long long& 输出千字节);
  // 校验脏页合理性
  bool 校验脏页合理(long long 值) const;
  // 聚合脏页到快照
  void 累加脏页到快照(内存快照& 快照, long long 值);
  // 解析写回（Writeback）行
  bool 解析写回行(const std::string& 行, long long& 输出千字节);
  // 校验写回合理性
  bool 校验写回合理(long long 值) const;
  // 聚合写回到快照
  void 累加写回到快照(内存快照& 快照, long long 值);
  // 解析匿名页（AnonPages）行
  bool 解析匿名页行(const std::string& 行, long long& 输出千字节);
  // 校验匿名页合理性
  bool 校验匿名页合理(long long 值) const;
  // 聚合匿名页到快照
  void 累加匿名页到快照(内存快照& 快照, long long 值);
  // 解析映射文件（Mapped）行
  bool 解析映射文件行(const std::string& 行, long long& 输出千字节);
  // 校验映射文件合理性
  bool 校验映射文件合理(long long 值) const;
  // 聚合映射文件到快照
  void 累加映射文件到快照(内存快照& 快照, long long 值);
  // 解析共享内存（Shmem）行
  bool 解析共享内存行(const std::string& 行, long long& 输出千字节);
  // 校验共享内存合理性
  bool 校验共享内存合理(long long 值) const;
  // 聚合共享内存到快照
  void 累加共享内存到快照(内存快照& 快照, long long 值);
  // 解析内核slab（Slab）行
  bool 解析内核slab行(const std::string& 行, long long& 输出千字节);
  // 校验内核slab合理性
  bool 校验内核slab合理(long long 值) const;
  // 聚合内核slab到快照
  void 累加内核slab到快照(内存快照& 快照, long long 值);
  // 解析可回收slab（SReclaimable）行
  bool 解析可回收slab行(const std::string& 行, long long& 输出千字节);
  // 校验可回收slab合理性
  bool 校验可回收slab合理(long long 值) const;
  // 聚合可回收slab到快照
  void 累加可回收slab到快照(内存快照& 快照, long long 值);
  // 解析不可回收slab（SUnreclaim）行
  bool 解析不可回收slab行(const std::string& 行, long long& 输出千字节);
  // 校验不可回收slab合理性
  bool 校验不可回收slab合理(long long 值) const;
  // 聚合不可回收slab到快照
  void 累加不可回收slab到快照(内存快照& 快照, long long 值);
  // 解析内核栈（KernelStack）行
  bool 解析内核栈行(const std::string& 行, long long& 输出千字节);
  // 校验内核栈合理性
  bool 校验内核栈合理(long long 值) const;
  // 聚合内核栈到快照
  void 累加内核栈到快照(内存快照& 快照, long long 值);
  // 解析页表（PageTables）行
  bool 解析页表行(const std::string& 行, long long& 输出千字节);
  // 校验页表合理性
  bool 校验页表合理(long long 值) const;
  // 聚合页表到快照
  void 累加页表到快照(内存快照& 快照, long long 值);
  // 解析提交上限（CommitLimit）行
  bool 解析提交上限行(const std::string& 行, long long& 输出千字节);
  // 校验提交上限合理性
  bool 校验提交上限合理(long long 值) const;
  // 聚合提交上限到快照
  void 累加提交上限到快照(内存快照& 快照, long long 值);
  // 解析已提交（Committed_AS）行
  bool 解析已提交行(const std::string& 行, long long& 输出千字节);
  // 校验已提交合理性
  bool 校验已提交合理(long long 值) const;
  // 聚合已提交到快照
  void 累加已提交到快照(内存快照& 快照, long long 值);
  // 解析巨页总数（HugePages_Total）行
  bool 解析巨页总数行(const std::string& 行, long long& 输出千字节);
  // 校验巨页总数合理性
  bool 校验巨页总数合理(long long 值) const;
  // 聚合巨页总数到快照
  void 累加巨页总数到快照(内存快照& 快照, long long 值);
  // 解析巨页空闲（HugePages_Free）行
  bool 解析巨页空闲行(const std::string& 行, long long& 输出千字节);
  // 校验巨页空闲合理性
  bool 校验巨页空闲合理(long long 值) const;
  // 聚合巨页空闲到快照
  void 累加巨页空闲到快照(内存快照& 快照, long long 值);
  内存快照 采集内存快照();
  bool 读取内存信息(内存快照& 输出);
  bool 读取进程状态(内存快照& 输出);
  bool 读取比例集(内存快照& 输出);
  bool 解析映射段(std::vector<映射段信息>& 输出, size_t 上限);
  std::map<std::string,long long> 按路径聚合常驻();
  std::vector<映射段信息> 查询最大映射(size_t 前几名);
  void 记录趋势点(const 内存快照& 快照);
  泄漏判定 判定泄漏趋势();
  std::vector<内存趋势点> 查询趋势(long long 起始毫秒,long long 结束毫秒);
  std::string 生成内存报告();
  bool 落盘快照(const std::string& 路径);
  bool 统计文件描述符(int& 已用, int& 上限);
  void 清空趋势(); size_t 趋势点数() const;
 private: 内存分析器(); 内存分析器(const 内存分析器&)=delete; 内存分析器& operator=(const 内存分析器&)=delete;
  mutable std::mutex 互斥锁_; std::deque<内存趋势点> 趋势_; static constexpr size_t 趋势上限_硬 = 1024; static constexpr size_t 映射上限_硬 = 2000; std::map<std::string,long long> 上次聚合_; };
long long 解析千字节数值(const std::string& 行);
bool 从状态行提取(const std::string& 行,const std::string& 前缀,long long& 输出);
}  // namespace 诊断
}  // namespace vine
