// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：上报队列——持久化分段队列、退避重试、批量打包、原子落盘
// 数据来源：错误聚合器、性能采样器、内存分析器产出的待上报负载
// 线程安全：互斥 + 原子序号
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
struct 上报条目 { long long 序号=0; long long 入队毫秒=0; long long 下次重试毫秒=0; int 重试次数=0; int 优先级=0; std::string 类别; std::string 负载; std::string 目标地址; bool 已压缩=false; std::string 校验和; std::string 转为日志行() const; size_t 负载字节() const; bool 是否到期(long long 现在) const; };
struct 队列配置 { std::string 落盘目录; size_t 内存上限=500; size_t 单批上限=20; long long 基础退避毫秒=5000; long long 最大退避毫秒=3600000; int 最大重试=8; };
struct 上报统计 { long long 入队总数=0; long long 成功总数=0; long long 失败总数=0; long long 丢弃总数=0; long long 当前等待=0; std::string 生成摘要() const; };
class 上报队列 {
 public:
  static 上报队列& 获取实例();
  // 提取上报标识（report_id）
  std::string 提取上报标识(const 上报条目& 条目) const;
  // 校验上报标识合法性
  bool 校验上报标识(const std::string& 值) const;
  // 提取发生时间（occur_time）
  std::string 提取发生时间(const 上报条目& 条目) const;
  // 校验发生时间合法性
  bool 校验发生时间(const std::string& 值) const;
  // 提取上报时间（upload_time）
  std::string 提取上报时间(const 上报条目& 条目) const;
  // 校验上报时间合法性
  bool 校验上报时间(const std::string& 值) const;
  // 提取重试次数（retry_count）
  std::string 提取重试次数(const 上报条目& 条目) const;
  // 校验重试次数合法性
  bool 校验重试次数(const std::string& 值) const;
  // 提取负载大小（payload_size）
  std::string 提取负载大小(const 上报条目& 条目) const;
  // 校验负载大小合法性
  bool 校验负载大小(const std::string& 值) const;
  // 提取优先级（priority）
  std::string 提取优先级(const 上报条目& 条目) const;
  // 校验优先级合法性
  bool 校验优先级(const std::string& 值) const;
  // 提取目标地址（endpoint）
  std::string 提取目标地址(const 上报条目& 条目) const;
  // 校验目标地址合法性
  bool 校验目标地址(const std::string& 值) const;
  // 提取压缩标记（compressed）
  std::string 提取压缩标记(const 上报条目& 条目) const;
  // 校验压缩标记合法性
  bool 校验压缩标记(const std::string& 值) const;
  // 提取加密标记（encrypted）
  std::string 提取加密标记(const 上报条目& 条目) const;
  // 校验加密标记合法性
  bool 校验加密标记(const std::string& 值) const;
  // 提取分片序号（shard_seq）
  std::string 提取分片序号(const 上报条目& 条目) const;
  // 校验分片序号合法性
  bool 校验分片序号(const std::string& 值) const;
  // 提取分片总数（shard_total）
  std::string 提取分片总数(const 上报条目& 条目) const;
  // 校验分片总数合法性
  bool 校验分片总数(const std::string& 值) const;
  // 提取校验和（checksum）
  std::string 提取校验和(const 上报条目& 条目) const;
  // 校验校验和合法性
  bool 校验校验和(const std::string& 值) const;
  bool 初始化(const 队列配置& 配置);
  long long 入队(const std::string& 类别,const std::string& 负载,int 优先级,const std::string& 目标);
  std::vector<上报条目> 取出待发批次(size_t 上限);
  void 标记成功(long long 序号);
  void 标记失败(long long 序号,const std::string& 原因);
  void 标记丢弃(long long 序号);
  上报统计 查询统计() const;
  bool 落盘(); bool 从落盘恢复();
  void 设置发送器(std::function<bool(const 上报条目&)> 发送器);
  bool 尝试发送一批();
  void 清空(); size_t 等待数量() const;
  static constexpr size_t 负载上限_硬 = 65536;
  static constexpr size_t 类别上限_硬 = 128;
  static constexpr size_t 目标上限_硬 = 1024;
  static constexpr size_t 字段值上限_硬 = 4096;
  enum class 报表字段 { 标识=0, 发生时间=1, 上报时间=2, 重试次数=3, 负载大小=4, 优先级=5, 目标地址=6, 压缩标记=7, 加密标记=8, 分片序号=9, 分片总数=10, 校验和=11 };
  std::string 通用提取(报表字段 字段, const 上报条目& 条目) const;
  bool 通用校验(报表字段 字段, const std::string& 值) const;
  static std::string 截断字段(const std::string& 文本, size_t 上限);
  private: 上报队列(); long long 计算退避(int 重试) const; std::string 计算校验(const std::string& 负载) const; std::string 队列文件() const;
  mutable std::mutex 互斥锁_; 队列配置 配置_; std::deque<上报条目> 等待_; std::map<long long,上报条目> 发送中_; long long 序号_=0; 上报统计 统计_; std::function<bool(const 上报条目&)> 发送器_; };
}  // namespace 诊断
}  // namespace vine
