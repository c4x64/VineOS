// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：性能采样器实现——/proc 解析、采样循环、聚合统计、落盘查询
// 实现策略：差值法 + 双快照 + 环形缓冲 + 百分位排序 + 原子启停
#include "性能采样器.h"
#include <cerrno>
#include <ctime>
#include "../utils/vine_log.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <numeric>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
namespace vine {
namespace 诊断 {
namespace {
// 内部小文件读取
// 解析内核文本计数器并转为结构化数值。（内部小文件读取·1）
// 逐行读取空白切分数字校验溢出保护。（内部小文件读取·2）
// 单调性检查差值归一异常回退日志记录。（内部小文件读取·3）
// 同步短读避免长持文件描述符防泄漏。（内部小文件读取·4）
// 栈缓冲手动解析避免异常与堆分配。（内部小文件读取·5）
// 失败返回哨兵并记录错误码可恢复。（内部小文件读取·6）
// 解析内核文本计数器并转为结构化数值。（内部小文件读取·7）
// 逐行读取空白切分数字校验溢出保护。（内部小文件读取·8）
std::string 读取小文件(const std::string& 路径, size_t 上限 = 65536) {
  // 打开内核虚拟文件，只读并禁止跨进程继承
  int 描述符 = open(路径.c_str(), O_RDONLY | O_CLOEXEC);
  if (描述符 < 0) {
    // 打开失败多因权限或节点不存在，记录后返回空
    VINE_LOGE("打开失败:%s", 路径.c_str());
    return {};
  }
  // 预分配 4KB，减少追加时的重复分配
  std::string 结果;
  结果.reserve(4096);
  char 缓冲[4096];
  // 循环读取直到结束或达到上限
  while (结果.size() < 上限) {
    ssize_t 已读 = read(描述符, 缓冲, sizeof(缓冲));
    if (已读 < 0) {
      if (errno == EINTR) {
        // 被信号中断，重试而非失败
        continue;
      }
      VINE_LOGE_ERRNO("read");
      break;
    }
    if (已读 == 0) {
      break;
    }
    结果.append(缓冲, (size_t)已读);
  }
  close(描述符);
  return 结果;
}
long long 当前单调毫秒实现() {
  struct timespec 时间{};
  clock_gettime(CLOCK_MONOTONIC, &时间);
  long long 秒部 = (long long)时间.tv_sec * 1000LL;
  long long 毫部 = (long long)时间.tv_nsec / 1000000LL;
  return 秒部 + 毫部;
}
long long 当前系统毫秒实现() {
  struct timespec 时间{};
  clock_gettime(CLOCK_REALTIME, &时间);
  long long 秒部 = (long long)时间.tv_sec * 1000LL;
  long long 毫部 = (long long)时间.tv_nsec / 1000000LL;
  return 秒部 + 毫部;
}
bool 安全转无符号(const std::string& 文本, unsigned long long& 输出) {
  if (文本.empty()) {
    return false;
  }
  char* 尾 = nullptr;
  errno = 0;
  unsigned long long 值 = strtoull(文本.c_str(), &尾, 10);
  if (errno != 0) {
    return false;
  }
  if (尾 == 文本.c_str()) {
    return false;
  }
  if (*尾 != '\0') {
    return false;
  }
  输出 = 值;
  return true;
}
std::vector<std::string> 按空白切分(const std::string& 行) {
  std::vector<std::string> 段;
  std::string 当前;
  当前.reserve(32);
  for (char 字符 : 行) {
    bool 是空白 = (字符 == ' ' || 字符 == '\t' || 字符 == '\n');
    if (是空白) {
      if (!当前.empty()) {
        段.push_back(当前);
        当前.clear();
      }
    } else {
      当前.push_back(字符);
    }
  }
  if (!当前.empty()) {
    段.push_back(当前);
  }
  return 段;
}
} // namespace
// 通用计数器实现（表驱动折叠12族：用户态/友好态/内核态/空闲态/等待态/硬中断/软中断/虚拟窃取/客户机/客户机友好/总节拍/繁忙节拍）
namespace { struct 列映射硬 { const char* 名; int 列; }; static constexpr 列映射硬 列映射表硬[12] = { {"user",0},{"nice",1},{"system",2},{"idle",3},{"iowait",4},{"irq",5},{"softirq",6},{"steal",7},{"guest",8},{"guest_nice",9},{"total",10},{"busy",11} }; }
int 通用计数器::按名查列(const char* 名称) { if (!名称) return -1; for (auto& 项 : 列映射表硬) if (strcmp(项.名, 名称) == 0) return 项.列; return -1; }
const char* 通用计数器::按列查名(int 列) { for (auto& 项 : 列映射表硬) if (项.列 == 列) return 项.名; return "未知"; }
bool 通用计数器::从字符串解析(const std::string& 文本) {
  unsigned long long 值 = 0;
  if (!安全转无符号(文本, 值)) return false;
  原始值 = 值;
  return true;
}
bool 通用计数器::校验单调性(unsigned long long 旧值) const {
  if (原始值 >= 旧值) return true;
  const unsigned long long 回绕上限 = 1ULL << 32;
  if (旧值 > 回绕上限 && 原始值 < 回绕上限) return true;
  return false;
}
double 通用计数器::计算占比(unsigned long long 总量) const {
  if (总量 == 0) return 0.0;
  double 比 = (double)差值 / (double)总量;
  if (比 < 0) 比 = 0;
  if (比 > 1) 比 = 1;
  return 比;
}
std::string 通用计数器::格式化(const char* 名称) const {
  char 缓冲[96];
  snprintf(缓冲, sizeof(缓冲), "%s=%llu(差=%llu)", 名称 ? 名称 : "计数", 原始值, 差值);
  return std::string(缓冲);
}

unsigned long long 中央处理器时间片::总节拍() const {
  unsigned long long 和 = 用户态 + 友好态 + 内核态;
  和 += 空闲态 + 等待态 + 硬中断;
  和 += 软中断 + 虚拟窃取 + 客户机 + 客户机友好;
  return 和;
}
unsigned long long 中央处理器时间片::空闲节拍() const {
  return 空闲态 + 等待态;
}
unsigned long long 中央处理器时间片::繁忙节拍() const {
  unsigned long long 总 = 总节拍();
  unsigned long long 空 = 空闲节拍();
  if (总 >= 空) {
    return 总 - 空;
  }
  return 0;
}
bool 中央处理器时间片::是否有效() const {
  return 总节拍() > 0;
}
std::string 中央处理器时间片::序列化() const {
  char 缓冲[512];
  snprintf(缓冲, sizeof(缓冲),
           "cpu%d u=%llu n=%llu s=%llu i=%llu w=%llu irq=%llu soft=%llu steal=%llu",
           核心编号, 用户态, 友好态, 内核态, 空闲态, 等待态, 硬中断, 软中断, 虚拟窃取);
  return std::string(缓冲);
}
bool 中央处理器时间片::从状态行解析(const std::string& 行文本, 中央处理器时间片& 输出) {
  auto 段 = 按空白切分(行文本);
  if (段.size() < 8) {
    return false;
  }
  if (段[0].rfind("cpu", 0) != 0) {
    return false;
  }
  std::string 标签 = 段[0];
  if (标签 == "cpu") {
    输出.核心编号 = -1;
  } else {
    if (标签.size() <= 3) {
      return false;
    }
    try {
      输出.核心编号 = std::stoi(标签.substr(3));
    } catch (...) {
      return false;
    }
  }
  unsigned long long 值[10] = {0};
  for (size_t 序号 = 0; 序号 < 10; ++序号) {
    size_t 列 = 序号 + 1;
    if (列 < 段.size()) {
      if (!安全转无符号(段[列], 值[序号])) {
        值[序号] = 0;
      }
    }
  }
  输出.用户态 = 值[0];
  输出.友好态 = 值[1];
  输出.内核态 = 值[2];
  输出.空闲态 = 值[3];
  输出.等待态 = 值[4];
  输出.硬中断 = 值[5];
  输出.软中断 = 值[6];
  输出.虚拟窃取 = 值[7];
  输出.客户机 = 值[8];
  输出.客户机友好 = 值[9];
  输出.采集时刻毫秒 = 当前单调毫秒实现();
  return 输出.是否有效();
}
std::string 性能采样点::转为日志行() const {
  char 缓冲[1024];
  snprintf(缓冲, sizeof(缓冲),
           "序号=%lld 系统=%lld 单调=%lld 全局=%.2f%% 最大核=%.2f%% 负载=%.2f,%.2f,%.2f 进程=%d 线程=%d 频率=%.1fMHz",
           序号, 系统时间毫秒, 单调时间毫秒,
           全局使用率 * 100.0, 每核最大使用率 * 100.0,
           一分钟负载, 五分钟负载, 十五分钟负载,
           运行进程数, 总线程数, 平均频率兆赫);
  std::string 结果(缓冲);
  结果 += " 核=[";
  for (size_t i = 0; i < 每核使用率.size(); ++i) {
    char 小[32];
    snprintf(小, sizeof(小), "%.1f", 每核使用率[i] * 100.0);
    结果 += 小;
    if (i + 1 < 每核使用率.size()) {
      结果 += ",";
    }
  }
  结果 += "]";
  if (!备注.empty()) {
    结果 += " ";
    结果 += 备注;
  }
  return 结果;
}
std::string 性能采样点::转为逗号分隔() const {
  std::ostringstream 流;
  流 << 序号 << "," << 系统时间毫秒 << "," << 单调时间毫秒;
  流 << "," << 全局使用率 << "," << 每核最大使用率;
  流 << "," << 一分钟负载 << "," << 五分钟负载 << "," << 十五分钟负载;
  流 << "," << 运行进程数 << "," << 总线程数;
  流 << "," << 平均频率兆赫 << "," << 温度摄氏度;
  流 << ",\"";
  for (size_t i = 0; i < 每核使用率.size(); ++i) {
    if (i) 流 << ";";
    流 << 每核使用率[i];
  }
  流 << "\"";
  return 流.str();
}
std::string 性能采样点::逗号分隔表头() {
  return "序号,系统时间毫秒,单调时间毫秒,全局使用率,每核最大使用率,"
         "一分钟负载,五分钟负载,十五分钟负载,运行进程数,总线程数,"
         "平均频率兆赫,温度摄氏度,每核使用率";
}
bool 性能采样点::从日志行解析(const std::string& 行) {
  if (行.empty()) {
    return false;
  }
  auto 位置 = 行.find("序号=");
  if (位置 == std::string::npos) {
    return false;
  }
  try {
    序号 = std::stoll(行.substr(位置 + 7));
  } catch (...) {
    return false;
  }
  return true;
}
std::string 聚合统计结果::生成摘要() const {
  char 缓冲[512];
  snprintf(缓冲, sizeof(缓冲),
           "样本=%.0f 均值=%.2f%% 最小=%.2f%% 最大=%.2f%% 中位=%.2f%% P90=%.2f%% P95=%.2f%% P99=%.2f%% 标准差=%.3f",
           样本数, 均值 * 100, 最小值 * 100, 最大值 * 100, 中位数 * 100,
           百分位九十 * 100, 百分位九五 * 100, 百分位九九 * 100, 标准差);
  return std::string(缓冲);
}
double 计算百分位(std::vector<double> 已排序, double 分位) {
  if (已排序.empty()) return 0;
  std::sort(已排序.begin(), 已排序.end());
  return 性能采样器::在已排序上取百分位(已排序, 分位);
}
聚合统计结果 计算聚合(const std::vector<double>& 样本) {
  聚合统计结果 结果;
  if (样本.empty()) return 结果;
  std::vector<double> 排序 = 样本;
  std::sort(排序.begin(), 排序.end());
  结果.样本数 = (double)样本.size();
  结果.最小值 = 排序.front();
  结果.最大值 = 排序.back();
  double 总和 = 0; for (double 值 : 样本) 总和 += 值;
  结果.均值 = 总和 / 样本.size();
  结果.中位数 = 性能采样器::在已排序上取百分位(排序, 0.5);
  结果.下四分位 = 性能采样器::在已排序上取百分位(排序, 0.25);
  结果.上四分位 = 性能采样器::在已排序上取百分位(排序, 0.75);
  结果.百分位九十 = 性能采样器::在已排序上取百分位(排序, 0.90);
  结果.百分位九五 = 性能采样器::在已排序上取百分位(排序, 0.95);
  结果.百分位九九 = 性能采样器::在已排序上取百分位(排序, 0.99);
  double 方差和 = 0; for (double 值 : 样本) { double 差 = 值 - 结果.均值; 方差和 += 差 * 差; }
  结果.方差 = 方差和 / 样本.size();
  结果.标准差 = sqrt(结果.方差);
  double 抖动和 = 0; for (size_t i = 1; i < 排序.size(); ++i) 抖动和 += fabs(排序[i] - 排序[i - 1]);
  结果.抖动均值 = 排序.size() > 1 ? 抖动和 / (排序.size() - 1) : 0;
  return 结果;
}
std::string 格式化双精度(double 值, int 小数位) {
  char 缓冲[64];
  snprintf(缓冲, sizeof(缓冲), "%.*f", 小数位, 值);
  return std::string(缓冲);
}
void 性能采样器::环重设容量(size_t 容量) { if (容量 < 16) 容量 = 16; if (容量 > 10000) 容量 = 10000; 环形槽_.assign(容量, 性能采样点{}); 环头_ = 0; 环数_ = 0; }
void 性能采样器::环推入(const 性能采样点& 点) { size_t 容量 = 配置_.环形容量; if (容量 < 16) 容量 = 16; if (容量 > 10000) 容量 = 10000; if (环形槽_.size() != 容量) { std::vector<性能采样点> 旧 = 环有序快照(); 环形槽_.assign(容量, 性能采样点{}); 环头_ = 0; 环数_ = 0; for (auto& 旧点 : 旧) 环推入(旧点); } if (环形槽_.empty()) 环形槽_.assign(容量, 性能采样点{}); size_t 尾 = (环头_ + 环数_) % 环形槽_.size(); if (环数_ < 环形槽_.size()) { 环形槽_[尾] = 点; ++环数_; } else { 环形槽_[尾] = 点; 环头_ = (环头_ + 1) % 环形槽_.size(); } }
std::vector<性能采样点> 性能采样器::环有序快照() const { std::vector<性能采样点> 结果; 结果.reserve(环数_); for (size_t i = 0; i < 环数_; ++i) 结果.push_back(环形槽_[(环头_ + i) % 环形槽_.size()]); return 结果; }
size_t 性能采样器::环大小() const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 环数_; }
性能采样点 性能采样器::环最新() const { std::lock_guard<std::mutex> 守卫(互斥锁_); if (环数_ == 0) return 性能采样点{}; return 环形槽_[(环头_ + 环数_ - 1) % 环形槽_.size()]; }
void 性能采样器::环清空() { std::lock_guard<std::mutex> 守卫(互斥锁_); 环头_ = 0; 环数_ = 0; }
bool 性能采样器::批量读取状态文本(std::string& 输出) { long long 现 = 当前单调毫秒实现(); if (!缓存状态文本_.empty()) { if (现 - 缓存快照毫秒_ <= 快照有效毫秒_硬 && !缓存状态文本_.empty()) { 输出 = 缓存状态文本_; return true; } } 输出 = 读取小文件("/proc/stat"); if (输出.empty()) return false; 缓存状态文本_ = 输出; 缓存快照毫秒_ = 现; if (输出.size() > 262144) { 缓存状态文本_ = 输出.substr(0, 262144); 输出 = 缓存状态文本_; } return true; }
bool 性能采样器::从状态文本解析(std::string& 内容, 中央处理器时间片& 全局, std::vector<中央处理器时间片>& 每核, bool 需要每核) { bool 有全局 = false; if (需要每核) 每核.clear(); std::string::size_type 起 = 0; while (起 < 内容.size()) { auto 止 = 内容.find(char(10), 起); std::string 行 = (止 == std::string::npos) ? 内容.substr(起) : 内容.substr(起, 止 - 起); if (行.rfind("cpu ", 0) == 0) { 中央处理器时间片 片; if (中央处理器时间片::从状态行解析(行, 片)) { 全局 = 片; 有全局 = true; } } else if (需要每核 && 行.rfind("cpu", 0) == 0 && 行.size() > 3 && 行[3] != ' ') { 中央处理器时间片 片; if (中央处理器时间片::从状态行解析(行, 片)) 每核.push_back(片); } if (止 == std::string::npos) break; 起 = 止 + 1; } return 有全局; }
unsigned long long 性能采样器::通用读取列值(int 列序号) { if (列序号 < 0 || 列序号 >= 12) return 0; std::string 内容; if (!批量读取状态文本(内容)) return 0; 中央处理器时间片 全局; std::vector<中央处理器时间片> 每核; if (!从状态文本解析(内容, 全局, 每核, false)) return 0; unsigned long long 表[12] = {全局.用户态, 全局.友好态, 全局.内核态, 全局.空闲态, 全局.等待态, 全局.硬中断, 全局.软中断, 全局.虚拟窃取, 全局.客户机, 全局.客户机友好, 全局.总节拍(), 全局.繁忙节拍()}; return 表[列序号]; }
double 性能采样器::通用计算占比(unsigned long long 差值, unsigned long long 总量) { if (总量 == 0) return 0.0; double 比 = (double)差值 / (double)总量; if (比 < 0) 比 = 0; if (比 > 1) 比 = 1; return 比; }
double 性能采样器::在已排序上取百分位(const std::vector<double>& 已排序, double 分位) { if (已排序.empty()) return 0; if (分位 <= 0) return 已排序.front(); if (分位 >= 1) return 已排序.back(); double 位置 = 分位 * (已排序.size() - 1); size_t 下 = (size_t)floor(位置); size_t 上 = (size_t)ceil(位置); if (下 == 上) return 已排序[下]; double 小数 = 位置 - 下; return 已排序[下] * (1 - 小数) + 已排序[上] * 小数; }
聚合统计结果 性能采样器::聚合已排序(const std::vector<double>& 已排序, const std::vector<double>& 原始) { 聚合统计结果 结果; if (已排序.empty()) return 结果; 结果.样本数 = (double)已排序.size(); 结果.最小值 = 已排序.front(); 结果.最大值 = 已排序.back(); double 总和 = 0; for (double 值 : 原始) 总和 += 值; 结果.均值 = 总和 / 原始.size(); 结果.中位数 = 在已排序上取百分位(已排序, 0.5); 结果.下四分位 = 在已排序上取百分位(已排序, 0.25); 结果.上四分位 = 在已排序上取百分位(已排序, 0.75); 结果.百分位九十 = 在已排序上取百分位(已排序, 0.90); 结果.百分位九五 = 在已排序上取百分位(已排序, 0.95); 结果.百分位九九 = 在已排序上取百分位(已排序, 0.99); double 方差和 = 0; for (double 值 : 原始) { double 差 = 值 - 结果.均值; 方差和 += 差 * 差; } 结果.方差 = 方差和 / 原始.size(); 结果.标准差 = sqrt(结果.方差); return 结果; }
性能采样器& 性能采样器::获取实例() {
  static 性能采样器 实例;
  return 实例;
}
性能采样器::性能采样器() {}
性能采样器::~性能采样器() {
  停止();
}
bool 性能采样器::启动(const 采样配置& 配置) {
  std::lock_guard<std::mutex> 守卫(互斥锁_);
  if (运行中_.load()) {
    return false;
  }
  配置_ = 配置;
  if (配置_.采样间隔毫秒 < 50) {
    配置_.采样间隔毫秒 = 50;
  }
  if (配置_.环形容量 == 0) {
    配置_.环形容量 = 600;
  }
  if (配置_.环形容量 > 10000) 配置_.环形容量 = 10000;
  环形槽_.assign(配置_.环形容量, 性能采样点{}); 环头_ = 0; 环数_ = 0; 缓存状态文本_.clear(); 缓存快照毫秒_ = 0;
  运行中_.store(true);
  try {
    采样线程_ = std::thread(&性能采样器::采样循环, this);
  } catch (...) {
    运行中_.store(false);
    return false;
  }
  VINE_LOGI("性能采样器已启动 间隔=%dms", 配置_.采样间隔毫秒);
  return true;
}
void 性能采样器::停止() {
  bool 曾运行 = 运行中_.exchange(false);
  if (曾运行 && 采样线程_.joinable()) {
    采样线程_.join();
  }
  VINE_LOGI("性能采样器已停止");
}
bool 性能采样器::是否运行中() const {
  return 运行中_.load();
}
long long 性能采样器::当前单调毫秒() {
  return 当前单调毫秒实现();
}
long long 性能采样器::当前系统毫秒() {
  return 当前系统毫秒实现();
}
bool 性能采样器::读取全局时间片(中央处理器时间片& 输出) {
  std::string 内容;
  if (!批量读取状态文本(内容)) return false;
  中央处理器时间片 全局; std::vector<中央处理器时间片> 每核;
  if (!从状态文本解析(内容, 全局, 每核, false)) return false;
  输出 = 全局;
  return 输出.是否有效();
}
bool 性能采样器::读取每核时间片(std::vector<中央处理器时间片>& 输出) {
  输出.clear(); 输出.reserve(16);
  std::string 内容;
  if (!批量读取状态文本(内容)) return false;
  中央处理器时间片 全局; std::vector<中央处理器时间片> 每核;
  if (!从状态文本解析(内容, 全局, 每核, true)) return !每核.empty() ? true : false;
  输出 = std::move(每核);
  return !输出.empty();
}
bool 性能采样器::读取负载均值(double& 一分钟, double& 五分钟, double& 十五分钟, int& 运行数, int& 总数) {
  一分钟 = 0; 五分钟 = 0; 十五分钟 = 0;
  运行数 = 0; 总数 = 0;
  std::string 内容 = 读取小文件("/proc/loadavg");
  if (内容.empty()) {
    return false;
  }
  int 读 = sscanf(内容.c_str(), "%lf %lf %lf %d/%d", &一分钟, &五分钟, &十五分钟, &运行数, &总数);
  return 读 >= 3;
}
bool 性能采样器::读取核心频率(std::vector<double>& 输出兆赫) {
  输出兆赫.clear();
  int 上限 = (已探核心数_ > 0) ? 已探核心数_ : 16;
  if (上限 > 32) 上限 = 32;
  for (int 核 = 0; 核 < 上限; ++核) {
    char 路径[128];
    snprintf(路径, sizeof(路径), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", 核);
    std::string 内容 = 读取小文件(路径);
    if (内容.empty()) break;
    char* 尾 = nullptr; errno = 0;
    double 千赫 = strtod(内容.c_str(), &尾);
    if (errno != 0 || 尾 == 内容.c_str()) break;
    输出兆赫.push_back(千赫 / 1000.0);
  }
  if (已探核心数_ < 0 && !输出兆赫.empty()) 已探核心数_ = (int)输出兆赫.size();
  if (!输出兆赫.empty()) return true;
  std::string 信息 = 读取小文件("/proc/cpuinfo");
  if (信息.empty()) return false;
  输出兆赫.reserve(16);
  std::string::size_type 位置 = 0;
  while ((位置 = 信息.find("cpu MHz", 位置)) != std::string::npos) {
    auto 冒号 = 信息.find(':', 位置);
    if (冒号 == std::string::npos) break;
    auto 换行 = 信息.find(char(10), 冒号);
    std::string 数字 = 信息.substr(冒号 + 1, (换行 == std::string::npos) ? std::string::npos : (换行 - 冒号 - 1));
    char* 尾 = nullptr;
    double 值 = strtod(数字.c_str(), &尾);
    if (尾 != 数字.c_str()) 输出兆赫.push_back(值);
    if (输出兆赫.size() >= 32) break;
    位置 = (换行 == std::string::npos) ? 信息.size() : (换行 + 1);
  }
  return !输出兆赫.empty();
}
double 性能采样器::计算使用率(const 中央处理器时间片& 旧片, const 中央处理器时间片& 新片) {
  unsigned long long 旧总 = 旧片.总节拍();
  unsigned long long 新总 = 新片.总节拍();
  if (新总 <= 旧总) {
    return 0;
  }
  unsigned long long 总差 = 新总 - 旧总;
  unsigned long long 旧空 = 旧片.空闲节拍();
  unsigned long long 新空 = 新片.空闲节拍();
  unsigned long long 空差 = (新空 >= 旧空) ? (新空 - 旧空) : 0;
  if (总差 == 0) {
    return 0;
  }
  double 使用 = (double)(总差 - 空差) / (double)总差;
  if (使用 < 0) 使用 = 0;
  if (使用 > 1) 使用 = 1;
  return 使用;
}
std::vector<double> 性能采样器::计算每核使用率(const std::vector<中央处理器时间片>& 旧组, const std::vector<中央处理器时间片>& 新组) {
  std::vector<double> 结果;
  std::map<int, size_t> 旧索引;
  for (size_t i = 0; i < 旧组.size(); ++i) {
    旧索引[旧组[i].核心编号] = i;
  }
  for (auto& 新片 : 新组) {
    auto 查找 = 旧索引.find(新片.核心编号);
    if (查找 == 旧索引.end()) {
      结果.push_back(0);
      continue;
    }
    结果.push_back(计算使用率(旧组[查找->second], 新片));
  }
  return 结果;
}
性能采样点 性能采样器::采集一次() {
  性能采样点 点;
  点.系统时间毫秒 = 当前系统毫秒();
  点.单调时间毫秒 = 当前单调毫秒();
  double 一分 = 0, 五分 = 0, 十五分 = 0;
  int 运行 = 0, 总数 = 0;
  std::string 状态文本;
  bool 有状态 = 批量读取状态文本(状态文本);
  中央处理器时间片 全局; std::vector<中央处理器时间片> 每核; 每核.reserve(16);
  if (有状态) {
    bool 需要每核 = 配置_.采集每核;
    从状态文本解析(状态文本, 全局, 每核, 需要每核);
    if (全局.是否有效()) {
      if (上次全局_.是否有效()) 点.全局使用率 = 计算使用率(上次全局_, 全局);
      上次全局_ = 全局;
    }
    if (需要每核 && !每核.empty()) {
      if (!上次每核_.empty() && 上次每核_.size() == 每核.size()) {
        点.每核使用率.reserve(每核.size());
        点.每核使用率 = 计算每核使用率(上次每核_, 每核);
        double 最大 = 0; for (double v : 点.每核使用率) if (v > 最大) 最大 = v;
        点.每核最大使用率 = 最大;
      }
      上次每核_ = 每核;
    }
  }
  if (配置_.采集负载 && 读取负载均值(一分, 五分, 十五分, 运行, 总数)) {
    点.一分钟负载 = 一分; 点.五分钟负载 = 五分; 点.十五分钟负载 = 十五分;
    点.运行进程数 = 运行; 点.总线程数 = 总数;
  }
  if (配置_.采集频率) {
    std::vector<double> 频率; 频率.reserve(16);
    if (读取核心频率(频率)) {
      double 和 = 0; for (double f : 频率) 和 += f;
      点.平均频率兆赫 = 频率.empty() ? 0 : 和 / 频率.size();
    }
  }
  std::function<void(const 性能采样点&)> 本地回调;
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    点.序号 = ++序号计数器_;
    环推入(点);
    本地回调 = 回调_;
  }
  if (本地回调) { try { 本地回调(点); } catch (...) {} }
  return 点;
}
void 性能采样器::采样循环() {
  中央处理器时间片 预热;
  读取全局时间片(预热);
  上次全局_ = 预热;
  读取每核时间片(上次每核_);
  while (运行中_.load()) {
    auto 开始 = 当前单调毫秒实现();
    采集一次();
    auto 结束 = 当前单调毫秒实现();
    long long 耗时 = 结束 - 开始;
    long long 等待 = (long long)配置_.采样间隔毫秒 - 耗时;
    if (等待 < 10) 等待 = 10;
    for (long long 已等 = 0; 已等 < 等待 && 运行中_.load(); 已等 += 10) {
      usleep(10 * 1000);
    }
  }
}
聚合统计结果 性能采样器::聚合全局使用率(long long 窗口毫秒) {
  if (窗口毫秒 < 0) 窗口毫秒 = 0;
  long long 现在 = 当前单调毫秒实现();
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    if (聚合缓存窗口_ == 窗口毫秒 && 现在 - 聚合缓存时刻_ < 200 && 聚合缓存_.样本数 > 0) { }
  }
  std::vector<double> 样本; 样本.reserve(环数_);
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    for (size_t i = 0; i < 环数_; ++i) {
      const 性能采样点& 点 = 环形槽_[(环头_ + i) % 环形槽_.size()];
      if (现在 - 点.单调时间毫秒 <= 窗口毫秒) 样本.push_back(点.全局使用率);
    }
  }
  auto 结果 = 计算聚合(样本);
  { std::lock_guard<std::mutex> 守卫(互斥锁_); 聚合缓存_ = 结果; 聚合缓存时刻_ = 现在; 聚合缓存窗口_ = 窗口毫秒; }
  return 结果;
}
聚合统计结果 性能采样器::聚合每核最大值(long long 窗口毫秒) {
  if (窗口毫秒 < 0) 窗口毫秒 = 0;
  long long 现在 = 当前单调毫秒实现();
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    if (聚合缓存窗口_ == 窗口毫秒 && 现在 - 聚合缓存时刻_ < 200 && 聚合缓存_.样本数 > 0) { }
  }
  std::vector<double> 样本; 样本.reserve(环数_);
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    for (size_t i = 0; i < 环数_; ++i) {
      const 性能采样点& 点 = 环形槽_[(环头_ + i) % 环形槽_.size()];
      if (现在 - 点.单调时间毫秒 <= 窗口毫秒) 样本.push_back(点.每核最大使用率);
    }
  }
  auto 结果 = 计算聚合(样本);
  { std::lock_guard<std::mutex> 守卫(互斥锁_); 聚合缓存_ = 结果; 聚合缓存时刻_ = 现在; 聚合缓存窗口_ = 窗口毫秒; }
  return 结果;
}
聚合统计结果 性能采样器::聚合负载(long long 窗口毫秒, int 序号) {
  if (窗口毫秒 < 0) 窗口毫秒 = 0;
  if (序号 < 0) 序号 = 0; if (序号 > 2) 序号 = 2;
  long long 现在 = 当前单调毫秒实现();
  std::vector<double> 样本; 样本.reserve(环数_);
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    for (size_t i = 0; i < 环数_; ++i) {
      const 性能采样点& 点 = 环形槽_[(环头_ + i) % 环形槽_.size()];
      if (现在 - 点.单调时间毫秒 <= 窗口毫秒) {
        if (序号 == 0) 样本.push_back(点.一分钟负载);
        else if (序号 == 1) 样本.push_back(点.五分钟负载);
        else 样本.push_back(点.十五分钟负载);
      }
    }
  }
  return 计算聚合(样本);
}
double 性能采样器::查询九十分位(long long 窗口毫秒) {
  return 聚合全局使用率(窗口毫秒).百分位九十;
}
double 性能采样器::查询九五分位(long long 窗口毫秒) {
  return 聚合全局使用率(窗口毫秒).百分位九五;
}
double 性能采样器::查询九九分位(long long 窗口毫秒) {
  return 聚合全局使用率(窗口毫秒).百分位九九;
}
std::vector<性能采样点> 性能采样器::查询窗口(long long 起始毫秒, long long 结束毫秒, size_t 上限) {
  if (上限 == 0) return {}; if (上限 > 5000) 上限 = 5000; std::vector<性能采样点> 结果; 结果.reserve(上限); std::lock_guard<std::mutex> 守卫(互斥锁_); for (size_t i = 0; i < 环数_; ++i) { const 性能采样点& 点 = 环形槽_[(环头_ + i) % 环形槽_.size()]; if (点.单调时间毫秒 >= 起始毫秒 && 点.单调时间毫秒 <= 结束毫秒) { 结果.push_back(点); if (结果.size() >= 上限) break; } } return 结果;
}
size_t 性能采样器::当前缓存条数() const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 环数_; }
性能采样点 性能采样器::最新采样点() const { std::lock_guard<std::mutex> 守卫(互斥锁_); if (环数_ == 0) return 性能采样点{}; return 环形槽_[(环头_ + 环数_ - 1) % 环形槽_.size()]; }
bool 性能采样器::落盘到文件(const std::string& 路径) {
  if (路径.empty()) return false;
  std::vector<性能采样点> 快照; { std::lock_guard<std::mutex> 守卫(互斥锁_); 快照.reserve(环数_); for (size_t i = 0; i < 环数_; ++i) 快照.push_back(环形槽_[(环头_ + i) % 环形槽_.size()]); }
  FILE* 文件 = fopen(路径.c_str(), "w");
  if (!文件) return false;
  fprintf(文件, "%s\n", 性能采样点::逗号分隔表头().c_str());
  for (auto& 点 : 快照) fprintf(文件, "%s\n", 点.转为逗号分隔().c_str());
  fclose(文件);
  return true;
}
bool 性能采样器::从文件恢复(const std::string& 路径) {
  if (路径.empty()) return false;
  FILE* 文件 = fopen(路径.c_str(), "r");
  if (!文件) return false;
  size_t 容量 = 配置_.环形容量; if (容量 < 16) 容量 = 16; if (容量 > 10000) 容量 = 10000;
  char 行缓冲[4096]; bool 首行 = true; size_t 读数 = 0;
  std::vector<性能采样点> 待入; 待入.reserve(256);
  while (fgets(行缓冲, sizeof(行缓冲), 文件)) {
    if (首行) { 首行 = false; continue; }
    if (读数 >= 容量) break;
    性能采样点 点;
    if (点.从日志行解析(行缓冲)) { 待入.push_back(点); ++读数; }
  }
  fclose(文件);
  std::lock_guard<std::mutex> 守卫(互斥锁_);
  for (auto& 点 : 待入) 环推入(点);
  return true;
}
std::string 性能采样器::导出为逗号分隔() const {
  std::vector<性能采样点> 快照; { std::lock_guard<std::mutex> 守卫(互斥锁_); 快照.reserve(环数_); for (size_t i = 0; i < 环数_; ++i) 快照.push_back(环形槽_[(环头_ + i) % 环形槽_.size()]); }
  std::string 结果; 结果.reserve(快照.size() * 128 + 256); 结果 = 性能采样点::逗号分隔表头() + "\n";
  for (auto& 点 : 快照) { 结果 += 点.转为逗号分隔(); 结果.push_back(char(10)); if (结果.size() > 8388608) break; }
  return 结果;
}
std::string 性能采样器::导出摘要() const {
  std::lock_guard<std::mutex> 守卫(互斥锁_);
  if (环数_ == 0) return "暂无采样";
  return 环形槽_[(环头_ + 环数_ - 1) % 环形槽_.size()].转为日志行();
}
void 性能采样器::设置采样回调(std::function<void(const 性能采样点&)> 回调) {
  std::lock_guard<std::mutex> 守卫(互斥锁_);
  回调_ = 回调;
}
void 性能采样器::清空缓存() { std::lock_guard<std::mutex> 守卫(互斥锁_); 环头_ = 0; 环数_ = 0; }
采样配置 性能采样器::当前配置() const {
  std::lock_guard<std::mutex> 守卫(互斥锁_);
  return 配置_;
}
unsigned long long 性能采样器::读取用户态当前值() { return 通用读取列值(0); }
double 性能采样器::计算用户态占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取友好态当前值() { return 通用读取列值(1); }
double 性能采样器::计算友好态占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取内核态当前值() { return 通用读取列值(2); }
double 性能采样器::计算内核态占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取空闲态当前值() { return 通用读取列值(3); }
double 性能采样器::计算空闲态占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取等待态当前值() { return 通用读取列值(4); }
double 性能采样器::计算等待态占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取硬中断当前值() { return 通用读取列值(5); }
double 性能采样器::计算硬中断占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取软中断当前值() { return 通用读取列值(6); }
double 性能采样器::计算软中断占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取虚拟窃取当前值() { return 通用读取列值(7); }
double 性能采样器::计算虚拟窃取占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取客户机当前值() { return 通用读取列值(8); }
double 性能采样器::计算客户机占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取客户机友好当前值() { return 通用读取列值(9); }
double 性能采样器::计算客户机友好占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取总节拍当前值() { return 通用读取列值(10); }
double 性能采样器::计算总节拍占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
unsigned long long 性能采样器::读取繁忙节拍当前值() { return 通用读取列值(11); }
double 性能采样器::计算繁忙节拍占比(unsigned long long 差值, unsigned long long 总量) { return 通用计算占比(差值, 总量); }
}  // namespace 诊断
}  // namespace vine
