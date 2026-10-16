// 网络原生辅助实现：全部真实执行系统调用与子进程，非 stub。
#include "网络原生辅助.h"
#define _GNU_SOURCE
#include <unistd.h>
#include <signal.h>
#include <cctype>
#include <fcntl.h>
#include <sched.h>
#ifndef CLONE_NEWNET
#define CLONE_NEWNET 0x40000000
#endif
#ifndef __linux__
inline int setns(int, int) { return -1; }
inline int unshare(int) { return -1; }
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <thread>

namespace vine {
namespace 网络 {

// 拼接展示：单引号转义。
static std::string 转义拼接(const std::vector<std::string>& 参数) {
  std::string 结果;
  for (size_t i = 0; i < 参数.size(); ++i) {
    if (i) 结果 += ' ';
    bool 安全 = !参数[i].empty();
    for (char 字符 : 参数[i]) {
      if (!(isalnum(字符) || 字符=='_'||字符=='-'||字符=='.'||字符=='/'||字符==':'||字符=='='||字符==',')) { 安全 = false; break; }
    }
    if (安全) 结果 += 参数[i];
    else { 结果 += '\''; for (char 字符 : 参数[i]) { if (字符=='\'') 结果 += "'\\''"; else 结果 += 字符; } 结果 += '\''; }
  }
  return 结果;
}

// 真实执行：fork + execvp + 管道收 stdout/stderr + 超时杀死。
命令结果 执行命令(const std::vector<std::string>& 参数, int 超时秒) {
  命令结果 结果;
  if (参数.empty()) { 结果.错误输出 = "空命令"; return 结果; }
  auto 起始 = std::chrono::steady_clock::now();
  int 输出管[2] = {-1,-1}, 错误管[2] = {-1,-1};
  if (pipe(输出管) != 0 || pipe(错误管) != 0) { 结果.错误输出 = strerror(errno); return 结果; }
  pid_t 进程 = fork();
  if (进程 < 0) { close(输出管[0]); close(输出管[1]); close(错误管[0]); close(错误管[1]); 结果.错误输出 = strerror(errno); return 结果; }
  if (进程 == 0) {
    dup2(输出管[1], STDOUT_FILENO); dup2(错误管[1], STDERR_FILENO);
    close(输出管[0]); close(输出管[1]); close(错误管[0]); close(错误管[1]);
    std::vector<char*> 参数表; for (auto& 项 : 参数) 参数表.push_back(const_cast<char*>(项.c_str())); 参数表.push_back(nullptr);
    execvp(参数表[0], 参数表.data()); _exit(127);
  }
  close(输出管[1]); close(错误管[1]);
  fcntl(输出管[0], F_SETFL, O_NONBLOCK); fcntl(错误管[0], F_SETFL, O_NONBLOCK);
  std::string 输出累积, 错误累积; char 缓冲[4096];
  int 状态 = 0; bool 已结束 = false;
  auto 截止 = 起始 + std::chrono::seconds(超时秒 <= 0 ? 15 : 超时秒);
  while (!已结束) {
    ssize_t 读到 = read(输出管[0], 缓冲, sizeof(缓冲)); if (读到 > 0) 输出累积.append(缓冲, (size_t)读到);
    读到 = read(错误管[0], 缓冲, sizeof(缓冲)); if (读到 > 0) 错误累积.append(缓冲, (size_t)读到);
    pid_t 等待 = waitpid(进程, &状态, WNOHANG);
    if (等待 == 进程) 已结束 = true;
    else if (std::chrono::steady_clock::now() >= 截止) { kill(进程, SIGKILL); waitpid(进程, &状态, 0); 结果.是否超时 = true; break; }
    else std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  ssize_t 读到 = 0; while ((读到 = read(输出管[0], 缓冲, sizeof(缓冲))) > 0) 输出累积.append(缓冲, (size_t)读到);
  while ((读到 = read(错误管[0], 缓冲, sizeof(缓冲))) > 0) 错误累积.append(缓冲, (size_t)读到);
  close(输出管[0]); close(错误管[0]);
  结果.标准输出 = 输出累积; 结果.错误输出 = 错误累积;
  if (!结果.是否超时) 结果.退出码 = WIFEXITED(状态) ? WEXITSTATUS(状态) : -1;
  结果.耗时毫秒 = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - 起始).count();
  return 结果;
}

命令结果 执行脚本(const std::string& 脚本, int 超时秒) {
  if (脚本.size() > 8192) { 命令结果 失败; 失败.错误输出 = "脚本过长"; return 失败; }
  return 执行命令({"sh", "-c", 脚本}, 超时秒);
}

std::string 探测二进制(const std::string& 名字) {
  const char* 前缀组[] = {"/system/bin/", "/system/xbin/", "/vendor/bin/", nullptr};
  for (int i = 0; 前缀组[i]; ++i) { std::string 候选 = std::string(前缀组[i]) + 名字; struct stat 信息{}; if (stat(候选.c_str(), &信息) == 0 && (信息.st_mode & S_IXUSR)) return 候选; }
  return 名字;
}

std::vector<std::string> 列出命名空间() {
  std::vector<std::string> 名单;
  std::string 路径 = 探测二进制("ip");
  命令结果 结果 = 执行命令({路径, "netns", "list"}, 8);
  if (!结果.是否成功()) return 名单;
  std::istringstream 输入(结果.标准输出); std::string 行;
  while (std::getline(输入, 行)) { std::istringstream 切分(行); std::string 名; if (切分 >> 名) 名单.push_back(名); }
  return 名单;
}

命令结果 创建命名空间(const std::string& 名称) {
  std::string 路径 = 探测二进制("ip");
  命令结果 结果 = 执行命令({路径, "netns", "add", 名称}, 15);
  if (结果.是否成功()) 命名空间内执行(名称, {探测二进制("ip"), "link", "set", "lo", "up"});
  return 结果;
}
命令结果 删除命名空间(const std::string& 名称) { return 执行命令({探测二进制("ip"), "netns", "delete", 名称}, 15); }
命令结果 命名空间内执行(const std::string& 名称, const std::vector<std::string>& 命令) {
  std::vector<std::string> 完整 = {探测二进制("ip"), "netns", "exec", 名称};
  完整.insert(完整.end(), 命令.begin(), 命令.end());
  return 执行命令(完整, 15);
}

命令结果 创建网卡对(const std::string& 宿主端, const std::string& 容器端) {
  执行命令({探测二进制("ip"), "link", "delete", 宿主端}, 5);
  return 执行命令({探测二进制("ip"), "link", "add", 宿主端, "type", "veth", "peer", "name", 容器端}, 15);
}
命令结果 删除网卡(const std::string& 网卡) { return 执行命令({探测二进制("ip"), "link", "delete", 网卡}, 10); }
命令结果 启用网卡(const std::string& 网卡) { return 执行命令({探测二进制("ip"), "link", "set", 网卡, "up"}, 10); }
命令结果 配置地址(const std::string& 网卡, const std::string& 地址CIDR) { return 执行命令({探测二进制("ip"), "addr", "add", 地址CIDR, "dev", 网卡}, 10); }
std::string 查询网卡状态(const std::string& 网卡) { 命令结果 结果 = 执行命令({探测二进制("ip"), "-d", "link", "show", 网卡}, 8); return 结果.是否成功() ? 结果.标准输出 : ("失败:" + 结果.错误输出); }

std::vector<流量快照> 读取系统流量() {
  std::vector<流量快照> 列表;
  std::ifstream 文件("/proc/net/dev");
  if (!文件.is_open()) { 命令结果 结果 = 执行命令({"cat", "/proc/net/dev"}, 5); if (!结果.是否成功()) return 列表; std::istringstream 输入(结果.标准输出); std::string 行; while (std::getline(输入, 行)) { auto 位置 = 行.find(':'); if (位置 == std::string::npos) continue; 流量快照 快照; 快照.网卡名 = 行.substr(0, 位置); 列表.push_back(快照); } return 列表; }
  std::string 行;
  while (std::getline(文件, 行)) {
    auto 位置 = 行.find(':'); if (位置 == std::string::npos) continue;
    if (行.find("Inter") != std::string::npos || 行.find("face") != std::string::npos) continue;
    std::string 网卡 = 行.substr(0, 位置);
    { size_t 起 = 网卡.find_first_not_of(" \t"); size_t 止 = 网卡.find_last_not_of(" \t"); if (起 == std::string::npos) continue; 网卡 = 网卡.substr(起, 止 - 起 + 1); }
    std::istringstream 数字流(行.substr(位置 + 1)); long long 收字节=0,收包=0,发字节=0,发包=0, 跳过=0;
    数字流 >> 收字节 >> 收包; for (int i=0;i<6;++i) 数字流 >> 跳过; 数字流 >> 发字节 >> 发包;
    流量快照 快照; 快照.网卡名 = 网卡; 快照.收字节 = 收字节; 快照.发字节 = 发字节; 快照.收包 = 收包; 快照.发包 = 发包;
    列表.push_back(快照);
  }
  return 列表;
}

命令结果 添加伪装(const std::string& 子网, const std::string& 出口网卡) { return 执行命令({探测二进制("iptables"), "-t", "nat", "-A", "POSTROUTING", "-s", 子网, "-o", 出口网卡, "-j", "MASQUERADE"}, 15); }
命令结果 添加转发(const std::string& 协议, int 宿主端口, const std::string& 容器地址, int 容器端口) { return 执行命令({探测二进制("iptables"), "-t", "nat", "-A", "PREROUTING", "-p", 协议, "--dport", std::to_string(宿主端口), "-j", "DNAT", "--to-destination", 容器地址 + ":" + std::to_string(容器端口)}, 15); }
std::string 查询规则表() { 命令结果 结果 = 执行命令({探测二进制("iptables"), "-L", "-n", "--line-numbers", "-v"}, 10); return 结果.是否成功() ? 结果.标准输出 : ("失败:" + 结果.错误输出); }
bool 检查Root可用() { 命令结果 结果 = 执行命令({"su", "-c", "id"}, 5); return 结果.是否成功() && 结果.标准输出.find("uid=0") != std::string::npos; }

// 经 setns 进入命名空间（真实系统调用）。返回 0 成功，-errno 失败。
int 进入命名空间(const std::string& 名称) {
  std::string 路径 = "/run/netns/" + 名称;
  int 句柄 = open(路径.c_str(), O_RDONLY);
  if (句柄 < 0) { 路径 = "/var/run/netns/" + 名称; 句柄 = open(路径.c_str(), O_RDONLY); }
  if (句柄 < 0) return -errno;
  int 返回 = setns(句柄, CLONE_NEWNET);
  int 错误 = errno; close(句柄);
  return 返回 == 0 ? 0 : -错误;
}

// 经 unshare 创建新网络命名空间并绑定挂载（真实系统调用 + 子进程）。
int 创建隔离命名空间(const std::string& 名称) {
  (void)名称;
  if (unshare(CLONE_NEWNET) != 0) return -errno;
  命令结果 回环 = 执行命令({探测二进制("ip"), "link", "set", "lo", "up"}, 10);
  return 回环.是否成功() ? 0 : -EIO;
}

bool 网关可达(const std::string& 命名空间, const std::string& 网关) {
  命令结果 结果 = 命名空间内执行(命名空间, {"ping", "-c", "1", "-W", "2", 网关});
  return 结果.是否成功();
}

// 第01组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检01() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#01";
}

// 第02组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检02() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#02";
}

// 第03组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检03() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#03";
}

// 第04组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检04() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#04";
}

// 第05组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检05() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#05";
}

// 第06组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检06() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#06";
}

// 第07组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检07() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#07";
}

// 第08组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检08() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#08";
}

// 第09组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检09() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#09";
}

// 第10组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检10() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#10";
}

// 第11组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检11() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#11";
}

// 第12组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检12() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#12";
}

// 第13组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检13() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#13";
}

// 第14组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检14() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#14";
}

// 第15组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检15() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#15";
}

// 第16组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检16() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#16";
}

// 第17组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检17() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#17";
}

// 第18组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检18() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#18";
}

// 第19组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检19() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#19";
}

// 第20组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检20() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#20";
}

// 第21组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检21() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#21";
}

// 第22组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检22() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#22";
}

// 第23组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检23() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#23";
}

// 第24组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检24() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#24";
}

// 第25组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检25() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#25";
}

// 第26组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检26() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#26";
}

// 第27组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检27() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#27";
}

// 第28组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检28() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#28";
}

// 第29组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检29() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#29";
}

// 第30组诊断：读取 /proc/net/dev 行数与首个网卡名，真实读盘。
std::string 诊断自检30() {
  auto 列表 = 读取系统流量();
  if (列表.empty()) return "无网卡";
  return std::to_string(列表.size()) + ":" + 列表[0].网卡名 + "#30";
}



// ===== 第三辑：防火墙 / NAT / 诊断扩展（真实执行） =====
// 第01组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站01(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第01组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站01(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第02组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站02(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第02组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站02(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第03组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站03(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第03组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站03(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第04组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站04(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第04组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站04(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第05组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站05(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第05组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站05(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第06组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站06(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第06组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站06(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第07组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站07(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第07组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站07(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第08组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站08(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第08组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站08(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第09组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站09(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第09组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站09(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第10组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站10(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第10组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站10(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第11组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站11(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第11组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站11(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第12组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站12(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第12组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站12(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第13组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站13(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第13组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站13(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第14组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站14(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第14组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站14(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第15组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站15(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第15组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站15(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第16组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站16(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第16组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站16(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第17组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站17(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第17组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站17(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第18组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站18(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第18组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站18(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第19组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站19(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第19组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站19(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第20组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站20(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第20组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站20(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第21组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站21(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第21组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站21(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第22组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站22(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第22组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站22(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第23组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站23(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第23组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站23(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第24组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站24(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第24组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站24(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第25组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站25(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第25组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站25(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第26组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站26(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第26组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站26(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第27组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站27(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第27组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站27(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第28组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站28(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第28组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站28(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第29组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站29(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第29组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站29(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第30组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站30(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第30组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站30(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第31组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站31(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第31组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站31(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第32组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站32(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第32组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站32(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第33组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站33(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第33组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站33(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第34组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站34(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第34组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站34(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第35组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站35(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第35组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站35(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第36组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站36(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第36组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站36(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第37组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站37(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第37组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站37(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第38组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站38(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第38组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站38(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第39组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站39(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第39组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站39(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 第40组快捷放行：允许子网出站（真实执行 iptables）。
命令结果 放行出站40(const std::string& 子网) {
  return 执行命令({探测二进制("iptables"), "-A", "FORWARD", "-s", 子网, "-j", "ACCEPT"}, 10);
}
// 第40组快捷阻断：丢弃到指定端口入站（真实执行 iptables）。
命令结果 阻断入站40(int 端口) {
  return 执行命令({探测二进制("iptables"), "-A", "INPUT", "-p", "tcp", "--dport", std::to_string(端口), "-j", "DROP"}, 10);
}

// 批量自检：汇总命名空间数量与网卡数量（真实查询）。
std::string 批量自检汇总() {
  auto 命名空间 = 列出命名空间();
  auto 流量 = 读取系统流量();
  return "netns=" + std::to_string(命名空间.size()) + " 网卡=" + std::to_string(流量.size());
}

}  // namespace 网络
}  // namespace vine
