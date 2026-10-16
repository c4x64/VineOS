// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：错误聚合器——指纹分组、去重计数、时间窗统计、落盘查询
// 数据来源：各子系统上报的错误记录
// 线程安全：互斥保护
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
struct 错误记录 { long long 序号=0; long long 发生毫秒=0; std::string 类别; std::string 标签; std::string 摘要; std::string 堆栈; std::string 指纹; int 进程号=0; int 线程号=0; std::string 转为日志行() const; };
struct 错误分组 { std::string 指纹; std::string 类别; std::string 代表摘要; long long 首次毫秒=0; long long 末次毫秒=0; long long 次数=0; std::vector<long long> 时刻表; std::string 生成摘要() const; double 计算频率(long long 窗口毫秒) const; };
class 错误聚合器 {
 public:
  static 错误聚合器& 获取实例();
  std::string 计算指纹(const std::string& 类别,const std::string& 摘要,const std::string& 堆栈首行);
  // 上报原生崩溃类错误
  long long 上报原生崩溃(const std::string& 摘要,const std::string& 堆栈);
  // 查询原生崩溃分组
  std::vector<错误分组> 查询原生崩溃分组() const;
  // 统计原生崩溃次数
  long long 统计原生崩溃次数(long long 窗口毫秒) const;
  // 上报应用无响应类错误
  long long 上报应用无响应(const std::string& 摘要,const std::string& 堆栈);
  // 查询应用无响应分组
  std::vector<错误分组> 查询应用无响应分组() const;
  // 统计应用无响应次数
  long long 统计应用无响应次数(long long 窗口毫秒) const;
  // 上报爪哇异常类错误
  long long 上报爪哇异常(const std::string& 摘要,const std::string& 堆栈);
  // 查询爪哇异常分组
  std::vector<错误分组> 查询爪哇异常分组() const;
  // 统计爪哇异常次数
  long long 统计爪哇异常次数(long long 窗口毫秒) const;
  // 上报原生信号类错误
  long long 上报原生信号(const std::string& 摘要,const std::string& 堆栈);
  // 查询原生信号分组
  std::vector<错误分组> 查询原生信号分组() const;
  // 统计原生信号次数
  long long 统计原生信号次数(long long 窗口毫秒) const;
  // 上报内存不足类错误
  long long 上报内存不足(const std::string& 摘要,const std::string& 堆栈);
  // 查询内存不足分组
  std::vector<错误分组> 查询内存不足分组() const;
  // 统计内存不足次数
  long long 统计内存不足次数(long long 窗口毫秒) const;
  // 上报磁盘不足类错误
  long long 上报磁盘不足(const std::string& 摘要,const std::string& 堆栈);
  // 查询磁盘不足分组
  std::vector<错误分组> 查询磁盘不足分组() const;
  // 统计磁盘不足次数
  long long 统计磁盘不足次数(long long 窗口毫秒) const;
  // 上报网络失败类错误
  long long 上报网络失败(const std::string& 摘要,const std::string& 堆栈);
  // 查询网络失败分组
  std::vector<错误分组> 查询网络失败分组() const;
  // 统计网络失败次数
  long long 统计网络失败次数(long long 窗口毫秒) const;
  // 上报权限拒绝类错误
  long long 上报权限拒绝(const std::string& 摘要,const std::string& 堆栈);
  // 查询权限拒绝分组
  std::vector<错误分组> 查询权限拒绝分组() const;
  // 统计权限拒绝次数
  long long 统计权限拒绝次数(long long 窗口毫秒) const;
  // 上报服务死亡类错误
  long long 上报服务死亡(const std::string& 摘要,const std::string& 堆栈);
  // 查询服务死亡分组
  std::vector<错误分组> 查询服务死亡分组() const;
  // 统计服务死亡次数
  long long 统计服务死亡次数(long long 窗口毫秒) const;
  // 上报渲染超时类错误
  long long 上报渲染超时(const std::string& 摘要,const std::string& 堆栈);
  // 查询渲染超时分组
  std::vector<错误分组> 查询渲染超时分组() const;
  // 统计渲染超时次数
  long long 统计渲染超时次数(long long 窗口毫秒) const;
  // 上报快照失败类错误
  long long 上报快照失败(const std::string& 摘要,const std::string& 堆栈);
  // 查询快照失败分组
  std::vector<错误分组> 查询快照失败分组() const;
  // 统计快照失败次数
  long long 统计快照失败次数(long long 窗口毫秒) const;
  // 上报启动失败类错误
  long long 上报启动失败(const std::string& 摘要,const std::string& 堆栈);
  // 查询启动失败分组
  std::vector<错误分组> 查询启动失败分组() const;
  // 统计启动失败次数
  long long 统计启动失败次数(long long 窗口毫秒) const;
  // 上报存储损坏类错误
  long long 上报存储损坏(const std::string& 摘要,const std::string& 堆栈);
  // 查询存储损坏分组
  std::vector<错误分组> 查询存储损坏分组() const;
  // 统计存储损坏次数
  long long 统计存储损坏次数(long long 窗口毫秒) const;
  // 上报解码失败类错误
  long long 上报解码失败(const std::string& 摘要,const std::string& 堆栈);
  // 查询解码失败分组
  std::vector<错误分组> 查询解码失败分组() const;
  // 统计解码失败次数
  long long 统计解码失败次数(long long 窗口毫秒) const;
  // 上报输入丢失类错误
  long long 上报输入丢失(const std::string& 摘要,const std::string& 堆栈);
  // 查询输入丢失分组
  std::vector<错误分组> 查询输入丢失分组() const;
  // 统计输入丢失次数
  long long 统计输入丢失次数(long long 窗口毫秒) const;
  long long 上报错误(const std::string& 类别,const std::string& 标签,const std::string& 摘要,const std::string& 堆栈);
  std::vector<错误分组> 查询全部分组() const;
  std::vector<错误分组> 查询热门分组(size_t 前几名) const;
  std::vector<错误记录> 查询窗口(long long 起始,long long 结束,size_t 上限) const;
  bool 落盘(const std::string& 路径) const; bool 从文件加载(const std::string& 路径);
  void 清空(); size_t 记录总数() const; size_t 分组总数() const;
  static constexpr size_t 记录上限_硬 = 5000;
  static constexpr size_t 时刻表上限_硬 = 512;
  static constexpr size_t 摘要上限_硬 = 1024;
  static constexpr size_t 堆栈上限_硬 = 8192;
  static constexpr size_t 类别上限_硬 = 64;
  long long 通用上报(const std::string& 类别, const std::string& 摘要, const std::string& 堆栈);
  std::vector<错误分组> 通用查询分组(const std::string& 类别) const;
  long long 通用统计次数(const std::string& 类别, long long 窗口毫秒) const;
  static std::string 截断文本错误(const std::string& 文本, size_t 上限);
  private: 错误聚合器(); mutable std::mutex 互斥锁_; long long 序号_=0; std::deque<错误记录> 记录_; std::map<std::string,错误分组> 分组_; std::string 规范化堆栈(const std::string& 堆栈); };
std::string 提取堆栈首行(const std::string& 堆栈);
}  // namespace 诊断
}  // namespace vine
