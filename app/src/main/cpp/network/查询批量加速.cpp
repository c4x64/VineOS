// 查询批量加速实现：全部真实执行系统调用与子进程，非 stub。
#include "查询批量加速.h"
#include "流量探针.h"
#include <chrono>
#include <sstream>
#include <sys/stat.h>

namespace vine {
namespace 网络 {

int 夹紧查询超时(int 请求秒) {
  if (请求秒 < 2) return 2;
  if (请求秒 > 10) return 10;
  return 请求秒;
}

int 夹紧变更超时(int 请求秒) {
  if (请求秒 < 5) return 5;
  if (请求秒 > 30) return 30;
  return 请求秒;
}

int 批量脚本超时(int 条数, int 单条秒) {
  if (条数 < 1) 条数 = 1;
  if (条数 > 16) 条数 = 16;
  if (单条秒 < 2) 单条秒 = 2;
  if (单条秒 > 10) 单条秒 = 10;
  int 结果 = 8 + 条数 * 单条秒;
  if (结果 < 10) return 10;
  if (结果 > 120) return 120;
  return 结果;
}

long long 查询缓存原生::现在毫秒() {
  return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

查询缓存原生::查询缓存原生() {}

查询缓存条目原生 查询缓存原生::获取(const std::string& 键, long long 生存期毫秒, const std::string& 输出, bool 成功) {
  查询缓存条目原生 返回;
  if (命中取回(键, 返回)) return 返回;
  存入(键, 输出, 成功, 生存期毫秒);
  返回.输出 = 输出;
  返回.成功 = 成功;
  返回.到期毫秒 = 现在毫秒() + 生存期毫秒;
  return 返回;
}

bool 查询缓存原生::命中取回(const std::string& 键, 查询缓存条目原生& 返回) {
  std::lock_guard<std::mutex> 保护(互斥锁);
  auto 位置 = 表.find(键);
  long long 现在 = 现在毫秒();
  if (位置 == 表.end()) {
    未命中++;
    return false;
  }
  if (现在 >= 位置->second.到期毫秒) {
    表.erase(位置);
    未命中++;
    return false;
  }
  命中++;
  返回 = 位置->second;
  return true;
}

void 查询缓存原生::存入(const std::string& 键, const std::string& 输出, bool 成功, long long 生存期毫秒) {
  if (生存期毫秒 < 500) 生存期毫秒 = 500;
  if (生存期毫秒 > 600000) 生存期毫秒 = 600000;
  std::string 截断 = 输出.size() > 16000 ? 输出.substr(0, 16000) : 输出;
  std::lock_guard<std::mutex> 保护(互斥锁);
  查询缓存条目原生 条目;
  条目.输出 = 截断;
  条目.成功 = 成功;
  条目.到期毫秒 = 现在毫秒() + 生存期毫秒;
  表[键] = 条目;
  long long 现在 = 现在毫秒();
  if (现在 - 上次清理 >= 30000) {
    上次清理 = 现在;
    for (auto 起始 = 表.begin(); 起始 != 表.end();) {
      if (现在 >= 起始->second.到期毫秒) 起始 = 表.erase(起始);
      else ++起始;
    }
  }
}

int 查询缓存原生::按前缀失效(const std::string& 前缀) {
  std::lock_guard<std::mutex> 保护(互斥锁);
  std::vector<std::string> 待删;
  for (auto& 对 : 表) {
    if (对.first.compare(0, 前缀.size(), 前缀) == 0) 待删.push_back(对.first);
  }
  for (auto& 键 : 待删) 表.erase(键);
  return (int)待删.size();
}

int 查询缓存原生::清理过期() {
  std::lock_guard<std::mutex> 保护(互斥锁);
  long long 现在 = 现在毫秒();
  std::vector<std::string> 待删;
  for (auto& 对 : 表) {
    if (现在 >= 对.second.到期毫秒) 待删.push_back(对.first);
  }
  for (auto& 键 : 待删) 表.erase(键);
  上次清理 = 现在;
  return (int)待删.size();
}

long long 查询缓存原生::命中数() const {
  std::lock_guard<std::mutex> 保护(互斥锁);
  return 命中;
}

long long 查询缓存原生::未命中数() const {
  std::lock_guard<std::mutex> 保护(互斥锁);
  return 未命中;
}

int 查询缓存原生::条目数() const {
  std::lock_guard<std::mutex> 保护(互斥锁);
  return (int)表.size();
}

std::string 缓存探测二进制(查询缓存原生& 缓存, const std::string& 名字) {
  查询缓存条目原生 命中;
  if (缓存.命中取回("二进制:" + 名字, 命中)) return 命中.输出;
  std::string 真实 = 探测二进制(名字);
  缓存.存入("二进制:" + 名字, 真实, true, 查询生存期原生::二进制路径);
  return 真实;
}

std::vector<std::string> 缓存列出命名空间(查询缓存原生& 缓存) {
  查询缓存条目原生 命中;
  if (缓存.命中取回("命名空间列表", 命中)) {
    std::vector<std::string> 复用;
    std::istringstream 输入(命中.输出);
    std::string 行;
    while (std::getline(输入, 行)) {
      if (!行.empty()) 复用.push_back(行);
    }
    return 复用;
  }
  std::vector<std::string> 名单 = 列出命名空间();
  std::string 拼接;
  for (size_t i = 0; i < 名单.size(); ++i) {
    if (i) 拼接 += '\n';
    拼接 += 名单[i];
  }
  缓存.存入("命名空间列表", 拼接, true, 查询生存期原生::命名空间列表);
  return 名单;
}

std::string 缓存查询规则表(查询缓存原生& 缓存) {
  查询缓存条目原生 命中;
  if (缓存.命中取回("规则表", 命中)) return 命中.输出;
  std::string 路径 = 缓存探测二进制(缓存, "iptables");
  命令结果 结果 = 执行命令({路径, "-L", "-n", "--line-numbers", "-v"}, 夹紧查询超时(10));
  std::string 输出 = 结果.是否成功() ? 结果.标准输出 : ("失败:" + 结果.错误输出);
  if (输出.size() > 12000) 输出 = 输出.substr(0, 12000);
  缓存.存入("规则表", 输出, 结果.是否成功(), 查询生存期原生::防火墙表);
  return 输出;
}

size_t 缓存流量网卡数(查询缓存原生& 缓存) {
  查询缓存条目原生 命中;
  if (缓存.命中取回("流量网卡数", 命中)) {
    size_t 数 = 0;
    try {
      数 = (size_t)std::stoul(命中.输出);
    } catch (...) {
      数 = 0;
    }
    return 数;
  }
  auto 列表 = 读取系统流量();
  缓存.存入("流量网卡数", std::to_string(列表.size()), true, 查询生存期原生::系统流量);
  return 列表.size();
}

命令结果 批量创建命名空间(const std::vector<std::string>& 名称组, int 超时秒) {
  命令结果 空;
  if (名称组.empty() || 名称组.size() > 16) {
    空.错误输出 = "批量条数须在 1..16";
    return 空;
  }
  std::string 路径 = 探测二进制("ip");
  std::string 脚本;
  for (size_t i = 0; i < 名称组.size(); ++i) {
    if (名称组[i].empty() || 名称组[i].size() > 32) {
      空.错误输出 = "非法命名空间名";
      return 空;
    }
    if (i) 脚本 += " ; ";
    脚本 += 路径 + " netns add " + 名称组[i] + " 2>/dev/null; " + 路径 + " netns exec " + 名称组[i] + " " + 路径 + " link set lo up";
  }
  int 放大 = 批量脚本超时((int)名称组.size(), 4);
  int 钳制 = 夹紧变更超时(超时秒 <= 0 ? 放大 : 超时秒);
  if (超时秒 <= 0 || 超时秒 < 放大) 钳制 = 放大;
  if (钳制 > 120) 钳制 = 120;
  命令结果 结果 = 执行脚本(脚本, 钳制);
  auto 回查 = 列出命名空间();
  bool 缺失 = false;
  for (auto& 名 : 名称组) {
    bool 找到 = false;
    for (auto& 现有 : 回查) {
      if (现有 == 名) {
        找到 = true;
        break;
      }
    }
    if (!找到) {
      struct stat 信息{};
      std::string 第一 = "/run/netns/" + 名;
      std::string 第二 = "/var/run/netns/" + 名;
      if (stat(第一.c_str(), &信息) != 0 && stat(第二.c_str(), &信息) != 0) 缺失 = true;
    }
  }
  if (缺失 && 结果.是否成功()) {
    结果.退出码 = -1;
    结果.错误输出 += " 部分命名空间回读缺失";
  }
  return 结果;
}

命令结果 批量创建网卡对(
    const std::vector<std::string>& 宿主组,
    const std::vector<std::string>& 容器组,
    const std::vector<std::string>& 命名组,
    int 超时秒) {
  命令结果 空;
  if (宿主组.empty() || 宿主组.size() > 16 || 容器组.size() != 宿主组.size() || 命名组.size() != 宿主组.size()) {
    空.错误输出 = "批量参数长度不一致或超出 1..16";
    return 空;
  }
  std::string 路径 = 探测二进制("ip");
  std::string 脚本;
  for (size_t i = 0; i < 宿主组.size(); ++i) {
    if (宿主组[i].empty() || 宿主组[i].size() > 15 || 容器组[i].empty() || 容器组[i].size() > 15 || 命名组[i].empty()) {
      空.错误输出 = "非法网卡名或命名空间名";
      return 空;
    }
    if (i) 脚本 += " ; ";
    脚本 += 路径 + " link add " + 宿主组[i] + " type veth peer name " + 容器组[i] +
        " ; " + 路径 + " link set " + 容器组[i] + " netns " + 命名组[i] +
        " ; " + 路径 + " link set " + 宿主组[i] + " up";
  }
  int 放大 = 批量脚本超时((int)宿主组.size(), 6);
  int 钳制 = 超时秒 <= 0 ? 放大 : 超时秒;
  if (钳制 < 放大) 钳制 = 放大;
  if (钳制 < 10) 钳制 = 10;
  if (钳制 > 120) 钳制 = 120;
  命令结果 结果 = 执行脚本(脚本, 钳制);
  for (size_t i = 0; i < 宿主组.size(); ++i) {
    std::string 状态 = 查询网卡状态(宿主组[i]);
    if (状态.rfind("失败:", 0) == 0) {
      结果.退出码 = -1;
      结果.错误输出 += " 回读缺失:" + 宿主组[i];
    }
  }
  return 结果;
}

std::string 诊断自检通用(int 序号) {
  if (序号 < 1) 序号 = 1;
  if (序号 > 96) 序号 = 96;
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#" + (序号 < 10 ? "0" + std::to_string(序号) : std::to_string(序号));
}

std::string 快捷摘要通用(class 流量探针& 探针, int 序号) {
  if (序号 < 1) 序号 = 1;
  if (序号 > 96) 序号 = 96;
  auto 列表 = 探针.采样一次();
  std::string 标识 = 序号 < 10 ? "0" + std::to_string(序号) : std::to_string(序号);
  if (列表.empty()) return "空" + 标识;
  std::ostringstream 输出;
  输出 << 标识 << ":" << 列表.size() << ":" << 列表[0].网卡名;
  return 输出.str();
}

命令结果 放行出站通用(const std::string& 子网) {
  if (子网.empty() || 子网.size() > 64) {
    命令结果 失败;
    失败.错误输出 = "非法子网";
    return 失败;
  }
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 夹紧变更超时(10));
}

命令结果 阻断入站通用(int 端口) {
  if (端口 < 1 || 端口 > 65535) {
    命令结果 失败;
    失败.错误输出 = "非法端口";
    return 失败;
  }
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 夹紧变更超时(10));
}

bool 快捷映射端口公式(int 组号, int& 宿主端口, int& 容器端口) {
  if (组号 < 1 || 组号 > 64) return false;
  宿主端口 = 10000 + 137 * 组号;
  容器端口 = 8000 + 组号;
  return true;
}

bool 批量段起始公式(int 组号, int& 起始宿主, int& 起始容器) {
  if (组号 < 31 || 组号 > 70) return false;
  起始宿主 = 26541 + 211 * (组号 - 31);
  起始容器 = 9217 + 7 * (组号 - 31);
  return true;
}

int 按组取最大传输单元(int 组号) {
  static const int 表[] = {1280, 1300, 1320, 1340, 1360, 1380, 1400, 1420, 1440, 1460, 1480, 1500, 1520, 1540, 1560};
  static const int 长度 = (int)(sizeof(表) / sizeof(表[0]));
  int 索引 = 组号 % 长度;
  if (索引 < 0) 索引 += 长度;
  return 表[索引];
}

std::string 批量自检汇总缓存版(查询缓存原生& 缓存) {
  auto 命名空间 = 缓存列出命名空间(缓存);
  size_t 网卡数 = 缓存流量网卡数(缓存);
  return "netns=" + std::to_string(命名空间.size()) + " 网卡=" + std::to_string(网卡数);
}

}  // namespace 网络
}  // namespace vine
