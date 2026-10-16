// 流量探针：轮询 /proc/net/dev，计算速率，环形历史（中文标识）。
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include "网络原生辅助.h"
namespace vine { namespace 网络 {
struct 速率结果 { std::string 网卡名; double 下行速率 = 0; double 上行速率 = 0; long long 总字节 = 0; };
class 流量探针 {
 public:
  explicit 流量探针(int 历史上限 = 256);
  std::vector<流量快照> 采样一次();
  速率结果 计算速率(const std::string& 网卡, int 间隔毫秒);
  std::vector<流量快照> 查询历史() const;
  std::string 导出摘要() const;
  std::string 快捷摘要01();
  std::string 快捷摘要02();
  std::string 快捷摘要03();
  std::string 快捷摘要04();
  std::string 快捷摘要05();
  std::string 快捷摘要06();
  std::string 快捷摘要07();
  std::string 快捷摘要08();
  std::string 快捷摘要09();
  std::string 快捷摘要10();
  std::string 快捷摘要11();
  std::string 快捷摘要12();
  std::string 快捷摘要13();
  std::string 快捷摘要14();
  std::string 快捷摘要15();
  std::string 快捷摘要16();
  std::string 快捷摘要17();
  std::string 快捷摘要18();
  std::string 快捷摘要19();
  std::string 快捷摘要20();
 private:
  mutable std::mutex 互斥锁;
  std::deque<流量快照> 历史记录;
  int 历史上限 = 256;
};
} }
