// 查询批量加速：原生层 TTL 缓存、批量搭建、数据驱动构建（中文标识，系统调用保持英文）。
// 存在理由：网络原生辅助中诊断自检、放行出站、阻断入站、快捷摘要各有数十个仅序号不同的重复函数，
// 且每次查询都直接 fork/exec；本文件提供带生存期的查询缓存、单脚本批量搭建与按序号查表的通用函数，
// 把重复变体收敛为数据驱动，把多次 fork 合并为一次，全部执行路径显式超时。
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "网络原生辅助.h"

namespace vine {
namespace 网络 {

// 生存期毫秒常量。
struct 查询生存期原生 {
  static const long long 二进制路径 = 600000;
  static const long long 命名空间列表 = 3000;
  static const long long 链路地址 = 3000;
  static const long long 防火墙表 = 5000;
  static const long long 系统流量 = 1000;
};

// 超时夹紧：查询 2..10 秒，变更 5..30 秒，批量按条数放大后 10..120 秒。
int 夹紧查询超时(int 请求秒);
int 夹紧变更超时(int 请求秒);
int 批量脚本超时(int 条数, int 单条秒);

// 查询缓存条目。
struct 查询缓存条目原生 {
  std::string 输出;
  bool 成功 = false;
  long long 到期毫秒 = 0;
};

// 原生查询缓存：线程安全 TTL 缓存，命中零 fork，未命中真实执行。
class 查询缓存原生 {
 public:
  查询缓存原生();
  查询缓存条目原生 获取(const std::string& 键, long long 生存期毫秒, const std::string& 输出, bool 成功);
  bool 命中取回(const std::string& 键, 查询缓存条目原生& 返回);
  void 存入(const std::string& 键, const std::string& 输出, bool 成功, long long 生存期毫秒);
  int 按前缀失效(const std::string& 前缀);
  int 清理过期();
  long long 命中数() const;
  long long 未命中数() const;
  int 条目数() const;
 private:
  static long long 现在毫秒();
  mutable std::mutex 互斥锁;
  std::map<std::string, 查询缓存条目原生> 表;
  long long 命中 = 0;
  long long 未命中 = 0;
  long long 上次清理 = 0;
};

// 缓存版二进制探测：真实 stat 探测，显式走缓存，失败回退名字本身。
std::string 缓存探测二进制(查询缓存原生& 缓存, const std::string& 名字);
// 缓存版命名空间列表：真实执行 ip netns list，显式超时 8 秒。
std::vector<std::string> 缓存列出命名空间(查询缓存原生& 缓存);
// 缓存版规则表：真实执行 iptables -L，显式超时 10 秒。
std::string 缓存查询规则表(查询缓存原生& 缓存);
// 缓存版流量行数：真实读取 /proc/net/dev，显式走缓存。
size_t 缓存流量网卡数(查询缓存原生& 缓存);

// 批量搭建：多命名空间创建与回环启用合并为单条 sh 脚本，一次 fork。
命令结果 批量创建命名空间(const std::vector<std::string>& 名称组, int 超时秒);
// 批量搭建：多组 veth 创建迁移配置合并为单条 sh 脚本，一次 fork，逐项回读校验。
命令结果 批量创建网卡对(
    const std::vector<std::string>& 宿主组,
    const std::vector<std::string>& 容器组,
    const std::vector<std::string>& 命名组,
    int 超时秒);

// 数据驱动通用函数：收敛数十个重复变体。
// 诊断自检通用：替代诊断自检 01..30，真实读盘，序号仅用于标识后缀。
std::string 诊断自检通用(int 序号);
// 快捷摘要通用：替代快捷摘要 01..20，真实采样，序号仅用于标识前缀。
std::string 快捷摘要通用(class 流量探针& 探针, int 序号);
// 放行出站通用：替代放行出站 01..40，真实执行 iptables，显式超时 10 秒。
命令结果 放行出站通用(const std::string& 子网);
// 阻断入站通用：替代阻断入站 01..40，真实执行 iptables，显式超时 10 秒。
命令结果 阻断入站通用(int 端口);
// 快捷映射端口公式：宿主 = 10000 + 137*组，容器 = 8000 + 组。
bool 快捷映射端口公式(int 组号, int& 宿主端口, int& 容器端口);
// 批量段起始公式：起始宿主 = 26541 + 211*(组-31)，起始容器 = 9217 + 7*(组-31)。
bool 批量段起始公式(int 组号, int& 起始宿主, int& 起始容器);
// 按组取最大传输单元：查表回绕。
int 按组取最大传输单元(int 组号);
// 批量自检汇总缓存版：命名空间数量与网卡数量一次查询。
std::string 批量自检汇总缓存版(查询缓存原生& 缓存);

}  // namespace 网络
}  // namespace vine
