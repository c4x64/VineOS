// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：日志中心——多级别环形缓冲、文件轮转、关键字查询
// 数据来源：各模块写入的分级日志
// 线程安全：读写锁保护
#pragma once
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
enum class 日志等级 { 详细=0, 调试=1, 信息=2, 警告=3, 错误=4, 致命=5 };
struct 日志条目 { long long 序号=0; long long 时刻毫秒=0; 日志等级 等级=日志等级::信息; std::string 标签; std::string 内容; int 线程号=0; std::string 转为日志行() const; };
struct 日志查询条件 { long long 起始毫秒=0; long long 结束毫秒=0; 日志等级 最低等级=日志等级::详细; std::string 关键字; std::string 标签过滤; size_t 上限=200; };
class 日志中心 {
 public:
  static 日志中心& 获取实例();
  static std::string 等级转名(日志等级 e); static bool 从名转等级(const std::string& n, 日志等级& o);
  // 写入详细（VERBOSE）日志
  void 记录详细(const std::string& 标签,const std::string& 内容);
  // 查询详细及以上日志
  std::vector<日志条目> 查询详细及以上(const 日志查询条件& 条件) const;
  // 统计详细条数
  size_t 统计详细条数(long long 窗口毫秒) const;
  // 写入调试（DEBUG）日志
  void 记录调试(const std::string& 标签,const std::string& 内容);
  // 查询调试及以上日志
  std::vector<日志条目> 查询调试及以上(const 日志查询条件& 条件) const;
  // 统计调试条数
  size_t 统计调试条数(long long 窗口毫秒) const;
  // 写入信息（INFO）日志
  void 记录信息(const std::string& 标签,const std::string& 内容);
  // 查询信息及以上日志
  std::vector<日志条目> 查询信息及以上(const 日志查询条件& 条件) const;
  // 统计信息条数
  size_t 统计信息条数(long long 窗口毫秒) const;
  // 写入警告（WARN）日志
  void 记录警告(const std::string& 标签,const std::string& 内容);
  // 查询警告及以上日志
  std::vector<日志条目> 查询警告及以上(const 日志查询条件& 条件) const;
  // 统计警告条数
  size_t 统计警告条数(long long 窗口毫秒) const;
  // 写入错误（ERROR）日志
  void 记录错误(const std::string& 标签,const std::string& 内容);
  // 查询错误及以上日志
  std::vector<日志条目> 查询错误及以上(const 日志查询条件& 条件) const;
  // 统计错误条数
  size_t 统计错误条数(long long 窗口毫秒) const;
  // 写入致命（FATAL）日志
  void 记录致命(const std::string& 标签,const std::string& 内容);
  // 查询致命及以上日志
  std::vector<日志条目> 查询致命及以上(const 日志查询条件& 条件) const;
  // 统计致命条数
  size_t 统计致命条数(long long 窗口毫秒) const;
  void 写入(日志等级 等级,const std::string& 标签,const std::string& 内容);
  std::vector<日志条目> 查询(const 日志查询条件& 条件) const;
  bool 落盘轮转(const std::string& 目录,size_t 单文件上限字节);
  bool 导出到文件(const std::string& 路径,const 日志查询条件& 条件) const;
  void 设置容量(size_t n); void 清空(); size_t 条数() const;
 static constexpr size_t 最小容量_硬 = 64;
  static constexpr size_t 最大容量_硬 = 10000;
  static constexpr size_t 单条上限字节_硬 = 2048;
  static constexpr size_t 查询上限封顶_硬 = 2000;
  static constexpr size_t 标签上限字节_硬 = 256;
  void 通用记录(日志等级 等级, const std::string& 标签, const std::string& 内容);
  std::vector<日志条目> 通用查询(日志等级 最低, const 日志查询条件& 条件) const;
  size_t 通用统计(日志等级 等级, long long 窗口毫秒) const;
  static std::string 截断文本(const std::string& 文本, size_t 上限);
  static size_t 钳制容量(size_t 输入);
  static size_t 钳制查询上限(size_t 输入);
  private: 日志中心(); mutable std::mutex 互斥锁_; std::deque<日志条目> 缓冲_; size_t 容量_=2000; long long 序号_=0; };
}  // namespace 诊断
}  // namespace vine
