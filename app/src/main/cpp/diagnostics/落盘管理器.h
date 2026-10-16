// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：落盘管理器——分段文件、大小轮转、校验和、时间窗查询
// 数据来源：采样器与日志中心的批量写入请求
// 线程安全：互斥保护
#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace vine {
namespace 诊断 {
struct 落盘分段 { int 序号=0; std::string 路径; long long 起始毫秒=0; long long 结束毫秒=0; long long 字节数=0; long long 条数=0; std::string 校验和; std::string 转为日志行() const; };
struct 落盘配置 { std::string 根目录; std::string 前缀="诊断"; size_t 单文件上限字节=1048576; int 保留分段数=10; bool 写校验=true; };
class 落盘管理器 {
 public:
  static 落盘管理器& 获取实例();
  bool 初始化(const 落盘配置& 配置);
  // 读取文件序号（file_seq）
  std::string 读取文件序号(const 落盘分段& 分段) const;
  // 校验文件序号是否异常
  bool 校验文件序号正常(const 落盘分段& 分段) const;
  // 读取文件大小（file_size）
  std::string 读取文件大小(const 落盘分段& 分段) const;
  // 校验文件大小是否异常
  bool 校验文件大小正常(const 落盘分段& 分段) const;
  // 读取记录条数（record_count）
  std::string 读取记录条数(const 落盘分段& 分段) const;
  // 校验记录条数是否异常
  bool 校验记录条数正常(const 落盘分段& 分段) const;
  // 读取起始时间（begin_time）
  std::string 读取起始时间(const 落盘分段& 分段) const;
  // 校验起始时间是否异常
  bool 校验起始时间正常(const 落盘分段& 分段) const;
  // 读取结束时间（end_time）
  std::string 读取结束时间(const 落盘分段& 分段) const;
  // 校验结束时间是否异常
  bool 校验结束时间正常(const 落盘分段& 分段) const;
  // 读取轮转原因（rotate_reason）
  std::string 读取轮转原因(const 落盘分段& 分段) const;
  // 校验轮转原因是否异常
  bool 校验轮转原因正常(const 落盘分段& 分段) const;
  // 读取校验结果（verify_result）
  std::string 读取校验结果(const 落盘分段& 分段) const;
  // 校验校验结果是否异常
  bool 校验校验结果正常(const 落盘分段& 分段) const;
  // 读取压缩比例（compress_ratio）
  std::string 读取压缩比例(const 落盘分段& 分段) const;
  // 校验压缩比例是否异常
  bool 校验压缩比例正常(const 落盘分段& 分段) const;
  bool 追加行(const std::string& 行);
  bool 追加多行(const std::vector<std::string>& 行组);
  std::vector<落盘分段> 列出分段() const;
  std::vector<std::string> 查询行(long long 起始,long long 结束,size_t 上限) const;
  bool 轮转(const std::string& 原因);
  void 清理旧分段();
  std::string 生成清单() const;
  size_t 当前分段字节() const; long long 当前分段条数() const;
  enum class 分段字段 { 序号=0, 大小=1, 条数=2, 起始=3, 结束=4, 轮转原因=5, 校验结果=6, 压缩比例=7 };
  std::string 通用读取字段(分段字段 字段, const 落盘分段& 分段) const;
  bool 通用校验正常(const 落盘分段& 分段) const;
  static constexpr size_t 单文件下限_硬 = 65536;
  static constexpr size_t 单文件上限_硬 = 67108864;
  static constexpr int 保留下限_硬 = 1;
  static constexpr int 保留上限_硬 = 100;
  static constexpr size_t 查询上限封顶_硬 = 5000;
  static constexpr size_t 批量缓冲字节_硬 = 65536;
  static constexpr size_t 批量缓冲行数_硬 = 128;
  private: 落盘管理器(); std::string 当前路径() const; std::string 分段路径(int 序号) const; void 扫描现有分段(); std::string 计算校验(const std::string& 文件) const; bool 内部落盘缓冲(bool 强制);
  mutable std::mutex 互斥锁_; 落盘配置 配置_; int 下一序号_=0; long long 当前字节_=0; long long 当前条数_=0; long long 当前起始_=0; std::string 待写缓冲_; size_t 缓冲行数_=0; };
}  // namespace 诊断
}  // namespace vine
