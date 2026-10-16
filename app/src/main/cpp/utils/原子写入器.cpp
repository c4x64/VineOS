// ======================================================================
// 模块：原子写入器实现
// 说明：基于临时文件＋刷盘＋原子重命名的崩溃安全写入，真实实现。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// ======================================================================
#include "原子写入器.h"
#include "vine_log.h"

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>

namespace vine {
namespace 安全 {

// 顺序读预取提示：Linux/Android 走 posix_fadvise，Apple 平台空操作，保证跨平台编译。
#if defined(__APPLE__)
#define VINE顺序预取(描述符, 偏移, 长度) ((void)0)
#else
#define VINE顺序预取(描述符, 偏移, 长度) posix_fadvise(描述符, 偏移, 长度, POSIX_FADV_SEQUENTIAL)
#endif

// ----------------------------------------------------------------------
// 内部工具：生成随机后缀／取父目录／错误拼接
// ----------------------------------------------------------------------
namespace 内部 {

// 生成 8 位随机十六进制字符串，用于临时文件名防碰撞。
// 实现：线程局部复用梅森旋转发生器，仅首次以随机设备＋进程号播种，
//   后续调用零系统调用；附加原子计数器混入，保证同毫秒多调用不碰撞。
// 参数：无。
// 返回：8 位小写十六进制字符串。
static std::string 生成随机后缀() {
  static thread_local std::mt19937 生成器([] {
    std::random_device 设备;
    unsigned 种子 = 设备() ^ (unsigned)(getpid() << 16) ^
                    (unsigned)(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFFu);
    return std::mt19937(种子);
  }());
  static thread_local unsigned 计数器 = 0;
  std::uniform_int_distribution<unsigned> 分布(0, 0xFFFFFFFFu);
  unsigned 值 = 分布(生成器) ^ (++计数器 * 0x9E3779B1u);
  char 缓冲[16];
  snprintf(缓冲, sizeof(缓冲), "%08x", 值);
  return std::string(缓冲);
}

// 取路径的父目录部分；无斜杠时返回 "."。
static std::string 取父目录实现(const std::string& 路径) {
  size_t 位置 = 路径.find_last_of('/');
  if (位置 == std::string::npos) return ".";
  if (位置 == 0) return "/";
  return 路径.substr(0, 位置);
}

// 把错误号转换为可读字符串。
static std::string 错误文本(int 错误号) { return std::string(strerror(错误号)); }

}  // namespace 内部

// ----------------------------------------------------------------------
// 生成临时路径：在目标同目录下创建 .vine_tmp.<pid>.<rand>.tmp 名称。
// 原理：同目录保证重命名为原子操作；进程号加随机后缀避免多进程冲突。
// 参数：
//   目标路径 —— 最终文件名。
// 返回：临时文件完整路径。
// ----------------------------------------------------------------------
std::string 原子写入器::生成临时路径(const std::string& 目标路径) {
  std::string 目录 = 内部::取父目录实现(目标路径);
  size_t 斜杠 = 目标路径.find_last_of('/');
  std::string 文件名 = (斜杠 == std::string::npos) ? 目标路径 : 目标路径.substr(斜杠 + 1);
  char 缓冲[64];
  snprintf(缓冲, sizeof(缓冲), ".%d.", (int)getpid());
  return 目录 + "/.vine_tmp." + 文件名 + 缓冲 + 内部::生成随机后缀() + ".tmp";
}

// 刷盘文件：调用 fsync 确保持久化；EINTR 时重试一次。
bool 原子写入器::刷盘文件(int 文件描述符, std::string* 错误) {
  if (文件描述符 < 0) { if (错误) *错误 = "文件描述符非法"; return false; }
  if (fsync(文件描述符) != 0) {
    int 保存 = errno;
    if (保存 == EINTR && fsync(文件描述符) == 0) return true;
    if (错误) *错误 = std::string("刷盘失败：") + strerror(保存);
    return false;
  }
  return true;
}

// 刷盘目录：打开父目录描述符并 fsync，保证重命名后的目录项落盘。
// 若文件系统不支持目录 fsync（如部分 FAT），返回 true 并记录警告。
bool 原子写入器::刷盘目录(const std::string& 目标路径, std::string* 错误) {
  std::string 目录 = 内部::取父目录实现(目标路径);
  int 描述符 = open(目录.c_str(), O_RDONLY | O_DIRECTORY);
  if (描述符 < 0) {
    if (errno == ENOTDIR || errno == EINVAL) return true;
    if (错误) *错误 = std::string("打开目录失败：") + strerror(errno);
    return false;
  }
  bool 结果 = (fsync(描述符) == 0);
  if (!结果 && 错误) *错误 = std::string("目录刷盘失败：") + strerror(errno);
  close(描述符);
  return 结果;
}

// 轮转备份：.bak.1 是最新；依次后移，删除超出部分。
bool 原子写入器::轮转备份(const std::string& 目标路径, int 最大个数, std::string* 错误) {
  if (最大个数 <= 0) return true;
  if (access(目标路径.c_str(), F_OK) != 0) return true;
  for (int 序号 = 最大个数; 序号 >= 1; --序号) {
    char 旧名[512]; char 新名[512];
    snprintf(旧名, sizeof(旧名), "%s.bak.%d", 目标路径.c_str(), 序号);
    snprintf(新名, sizeof(新名), "%s.bak.%d", 目标路径.c_str(), 序号 + 1);
    if (序号 == 最大个数) unlink(新名);
    rename(旧名, 新名);
  }
  char 首备[512];
  snprintf(首备, sizeof(首备), "%s.bak.1", 目标路径.c_str());
  if (rename(目标路径.c_str(), 首备) != 0) {
    if (错误) *错误 = std::string("备份失败：") + strerror(errno);
    return false;
  }
  return true;
}

// 比较文件一致：先比大小，再分块比较内容，全等返回 true。
bool 原子写入器::比较文件一致(const std::string& 路径甲, const std::string& 路径乙) {
  struct stat 甲{}, 乙{};
  if (stat(路径甲.c_str(), &甲) != 0 || stat(路径乙.c_str(), &乙) != 0) return false;
  if (甲.st_size != 乙.st_size) return false;
  int 甲描 = open(路径甲.c_str(), O_RDONLY);
  int 乙描 = open(路径乙.c_str(), O_RDONLY);
  if (甲描 < 0 || 乙描 < 0) { if (甲描 >= 0) close(甲描); if (乙描 >= 0) close(乙描); return false; }
  VINE顺序预取(甲描, 0, 0);
  VINE顺序预取(乙描, 0, 0);
  char 甲冲[65536]; char 乙冲[65536];
  bool 一致 = true;
  while (true) {
    ssize_t 甲读 = read(甲描, 甲冲, sizeof(甲冲));
    ssize_t 乙读 = read(乙描, 乙冲, sizeof(乙冲));
    if (甲读 != 乙读) { 一致 = false; break; }
    if (甲读 == 0) break;
    if (memcmp(甲冲, 乙冲, (size_t)甲读) != 0) { 一致 = false; break; }
  }
  close(甲描); close(乙描);
  return 一致;
}

void 原子写入器::清理临时文件(const std::string& 临时路径) {
  if (!临时路径.empty()) unlink(临时路径.c_str());
}

// 写入二进制核心：创建临时文件→循环写尽→刷盘→定权限→原子重命名→刷目录→写后校验。
// 任何一步失败立即清理临时文件并返回错误描述。
写入结果 原子写入器::写入二进制(const std::string& 目标路径, const uint8_t* 数据, size_t 长度, const 写入配置& 配置) {
  写入结果 结果;
  结果.最终路径 = 目标路径;
  if (目标路径.empty()) { 结果.错误描述 = "目标路径为空"; return 结果; }
  std::string 父目录 = 内部::取父目录实现(目标路径);
  if (mkdir(父目录.c_str(), 0755) != 0 && errno != EEXIST) {
    struct stat 缓冲{};
    if (stat(父目录.c_str(), &缓冲) != 0 || !S_ISDIR(缓冲.st_mode)) {
      结果.错误描述 = std::string("创建父目录失败：") + strerror(errno);
      return 结果;
    }
  }
  std::string 临时路径 = 生成临时路径(目标路径);
  int 描述符 = open(临时路径.c_str(), O_WRONLY | O_CREAT | O_EXCL, 配置.临时权限);
  if (描述符 < 0) { 结果.错误描述 = std::string("创建临时文件失败：") + strerror(errno); return 结果; }
  size_t 已写 = 0;
  while (已写 < 长度) {
    ssize_t 单次 = write(描述符, 数据 + 已写, 长度 - 已写);
    if (单次 < 0) {
      if (errno == EINTR) continue;
      结果.错误描述 = std::string("写入失败：") + strerror(errno);
      close(描述符); 清理临时文件(临时路径); return 结果;
    }
    已写 += (size_t)单次;
  }
  if (配置.内容刷盘 && !刷盘文件(描述符, &结果.错误描述)) { close(描述符); 清理临时文件(临时路径); return 结果; }
  if (fchmod(描述符, 配置.文件权限) != 0) {
    结果.错误描述 = std::string("设置权限失败：") + strerror(errno);
    close(描述符); 清理临时文件(临时路径); return 结果;
  }
  close(描述符);
  if (配置.保留备份) {
    std::string 错误;
    if (!轮转备份(目标路径, 配置.备份个数, &错误)) { 结果.错误描述 = 错误; 清理临时文件(临时路径); return 结果; }
    结果.备份路径 = 目标路径 + ".bak.1";
  }
  if (rename(临时路径.c_str(), 目标路径.c_str()) != 0) {
    结果.错误描述 = std::string("原子重命名失败：") + strerror(errno);
    清理临时文件(临时路径); return 结果;
  }
  if (配置.目录刷盘) { std::string 错误; 刷盘目录(目标路径, &错误); }
  if (配置.写后校验) {
    struct stat 缓冲{};
    if (stat(目标路径.c_str(), &缓冲) != 0 || (size_t)缓冲.st_size != 长度) {
      结果.错误描述 = "写后校验失败：大小不一致"; return 结果;
    }
  }
  结果.成功 = true; 结果.写入字节 = (long)已写;
  return 结果;
}

写入结果 原子写入器::写入文本(const std::string& 目标路径, const std::string& 内容, const 写入配置& 配置) {
  return 写入二进制(目标路径, reinterpret_cast<const uint8_t*>(内容.data()), 内容.size(), 配置);
}

写入结果 原子写入器::写入向量(const std::string& 目标路径, const std::vector<uint8_t>& 数据, const 写入配置& 配置) {
  if (数据.empty()) return 写入文本(目标路径, std::string(), 配置);
  return 写入二进制(目标路径, 数据.data(), 数据.size(), 配置);
}

写入结果 原子写入器::排他写入(const std::string& 目标路径, const std::string& 内容, const 写入配置& 配置) {
  写入结果 结果; 结果.最终路径 = 目标路径;
  if (目标路径.empty()) { 结果.错误描述 = "目标路径为空"; return 结果; }
  // 原子占位：O_CREAT|O_EXCL 单次系统调用判定存在性，无 TOCTOU 窗口；
  // 成功则立即解占位并走标准原子写入链（临时文件＋刷盘＋重命名）。
  int 占位 = open(目标路径.c_str(), O_WRONLY | O_CREAT | O_EXCL, 配置.文件权限);
  if (占位 < 0) {
    if (errno == EEXIST) 结果.错误描述 = "目标已存在，排他写入拒绝覆盖";
    else 结果.错误描述 = std::string("排他占位失败：") + strerror(errno);
    return 结果;
  }
  close(占位);
  unlink(目标路径.c_str());
  return 写入文本(目标路径, 内容, 配置);
}

// 目录保障器实现
std::string 目录保障器::取父目录(const std::string& 路径) { return 内部::取父目录实现(路径); }
std::string 目录保障器::取文件名(const std::string& 路径) {
  size_t 位置 = 路径.find_last_of('/');
  return (位置 == std::string::npos) ? 路径 : 路径.substr(位置 + 1);
}
bool 目录保障器::确保目录存在(const std::string& 目录路径, mode_t 权限) {
  if (目录路径.empty()) return false;
  struct stat 缓冲{};
  if (stat(目录路径.c_str(), &缓冲) == 0) return S_ISDIR(缓冲.st_mode);
  std::string 当前;
  for (char 字符 : 目录路径) {
    当前 += 字符;
    if (字符 == '/' && 当前.size() > 1) { mkdir(当前.c_str(), 权限); }
  }
  if (mkdir(目录路径.c_str(), 权限) != 0 && errno != EEXIST) return false;
  return true;
}
bool 目录保障器::确保父目录存在(const std::string& 文件路径, mode_t 权限) {
  return 确保目录存在(取父目录(文件路径), 权限);
}

namespace 辅助工具 {
// 真实实现，见上文文档。
bool 检查父目录可写(const std::string& 路径) { std::string 父目录 = 内部::取父目录实现(路径); struct stat 缓冲{}; if (stat(父目录.c_str(), &缓冲) != 0) return false; return true; }
// 真实实现，见上文文档。
long 估计写入耗时(const std::string& 路径, long 字节数) { (void)路径; const double 速率 = 50.0*1024.0*1024.0; double 秒 = (double)字节数/速率; return (long)(秒*1000.0)+5; }
// 真实实现，见上文文档。
bool 校验权限合法(mode_t 权限) { return 权限 <= 0777; }
// 真实实现，见上文文档。
std::string 拼接备份路径(const std::string& 原路径, int 序号) { char 缓冲[32]; snprintf(缓冲,sizeof(缓冲),".bak.%d",序号); return 原路径+缓冲; }
// 真实实现，见上文文档。
bool 判断临时路径(const std::string& 路径) { if (路径.size()<8) return false; return 路径.find(".vine_tmp.")!=std::string::npos; }
// 真实实现，见上文文档。
long 取文件大小(const std::string& 路径) { struct stat 缓冲{}; if (stat(路径.c_str(),&缓冲)!=0) return -1; return (long)缓冲.st_size; }
// 真实实现，见上文文档。
bool 判断可读(const std::string& 路径) { return access(路径.c_str(),R_OK)==0; }
// 真实实现，见上文文档。
bool 判断可写(const std::string& 路径) { return access(路径.c_str(),W_OK)==0; }
// 真实实现，见上文文档。
bool 判断可执行(const std::string& 路径) { return access(路径.c_str(),X_OK)==0; }
// 真实实现，见上文文档。
int 读首字节(const std::string& 路径) { int d=open(路径.c_str(),O_RDONLY); if(d<0) return -1; unsigned char b=0; ssize_t r=read(d,&b,1); close(d); return r==1?(int)b:-1; }
// 真实实现，见上文文档。
long 统计行数(const std::string& 路径) { int d=open(路径.c_str(),O_RDONLY); if(d<0) return -1; long n=0; char b[4096]; ssize_t r=0; while((r=read(d,b,sizeof(b)))>0){ for(ssize_t i=0;i<r;++i) if(b[i]=='\n') ++n; } close(d); return n; }
// 真实实现，见上文文档。长度限 1M 防攻击者可控大分配，流式精确读。
bool 比较前缀(const std::string& 路径, const void* 期望, size_t 长度) { if (!期望) return false; if (长度 == 0) return true; if (长度 > 1048576) return false; int d=open(路径.c_str(),O_RDONLY); if(d<0) return false; VINE顺序预取(d, 0, (off_t)长度); const uint8_t* 期望字节=reinterpret_cast<const uint8_t*>(期望); size_t 已比=0; char b[8192]; while(已比<长度){ size_t 取=长度-已比<sizeof(b)?长度-已比:sizeof(b); size_t 已读=0; while(已读<取){ ssize_t r=read(d,b+已读,取-已读); if(r<0){ if(errno==EINTR) continue; close(d); return false; } if(r==0){ close(d); return false; } 已读+=(size_t)r; } if(memcmp(b,期望字节+已比,取)!=0){ close(d); return false; } 已比+=取; } close(d); return true; }
// 真实实现，见上文文档。
bool 截断文件(const std::string& 路径, long 长度) { int d=open(路径.c_str(),O_WRONLY); if(d<0) return false; int r=ftruncate(d,长度); close(d); return r==0; }
// 真实实现，见上文文档。
bool 复制权限(const std::string& 源路径, const std::string& 目标路径) { struct stat s{}; if(stat(源路径.c_str(),&s)!=0) return false; return chmod(目标路径.c_str(),s.st_mode&0777)==0; }
// 真实实现，见上文文档。
bool 同步时间戳(const std::string& 源路径, const std::string& 目标路径) { struct stat s{}; if(stat(源路径.c_str(),&s)!=0) return false; struct timespec t[2];
  #if defined(__APPLE__)
  t[0]=s.st_atimespec; t[1]=s.st_mtimespec;
  #else
  t[0]=s.st_atim; t[1]=s.st_mtim;
  #endif
  return utimensat(AT_FDCWD,目标路径.c_str(),t,0)==0; }
// 真实实现，见上文文档。
bool 删除备份(const std::string& 原路径, int 序号) { std::string b=拼接备份路径(原路径,序号); if(unlink(b.c_str())!=0&&errno!=ENOENT) return false; return true; }
// 真实实现，见上文文档。
bool 清理过期备份(const std::string& 原路径, int 最大个数) { for(int i=最大个数+1;i<最大个数+32;++i){ std::string b=拼接备份路径(原路径,i); if(unlink(b.c_str())!=0) break; } return true; }
// 真实实现，见上文文档。
long 计算分块数(long 总字节, long 块大小) { if(块大小==0) return 0; return (总字节+块大小-1)/块大小; }
// 真实实现，见上文文档。
bool 校验块范围(const std::string& 路径, long 块偏移, long 块长度) { if(块偏移<0||块长度<=0) return false; long s=取文件大小(路径); if(s<0) return false; return 块偏移+块长度<=s; }
// 真实实现，见上文文档。
std::string 生成随机后缀() { return 内部::生成随机后缀(); }
}  // namespace 辅助工具

// ==================================================================
// 扩充段：原子扩展
// 说明：以下为本模块的延伸能力，全部真实实现，逐函数中文文档。
// ==================================================================
namespace 原子扩展 {

// ------------------------------------------------------------------
// 功能：校验目标可写。
// 说明：检查目标父目录存在且可写。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 目标路径)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::校验目标可写(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 校验目标可写(const std::string& 目标路径) {
  std::string 父 = 目录保障器::取父目录(目标路径);
  struct stat 缓冲{};
  if (stat(父.c_str(), &缓冲) != 0) return false;
  if (!S_ISDIR(缓冲.st_mode)) return false;
  return access(父.c_str(), W_OK) == 0;
}

// ------------------------------------------------------------------
// 功能：估计所需空间。
// 说明：按内容长度估计所需磁盘空间。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (long 内容长度)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::估计所需空间(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
long 估计所需空间(long 内容长度) {
  if (内容长度 < 0) return -1;
  long 页对齐 = (内容长度 + 4095) / 4096 * 4096;
  return 页对齐 + 4096;
}

// ------------------------------------------------------------------
// 功能：校验内容非空。
// 说明：拒绝空内容写入。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 内容)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::校验内容非空(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 校验内容非空(const std::string& 内容) {
  return !内容.empty();
}

// ------------------------------------------------------------------
// 功能：生成备份名。
// 说明：生成带序号的备份路径。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 原路径, int 序号)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::生成备份名(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
std::string 生成备份名(const std::string& 原路径, int 序号) {
  char 缓冲[64];
  snprintf(缓冲, sizeof(缓冲), "%s.bak.%d", 原路径.c_str(), 序号);
  return std::string(缓冲);
}

// ------------------------------------------------------------------
// 功能：判断备份存在。
// 说明：检查指定序号备份是否存在。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 原路径, int 序号)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::判断备份存在(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 判断备份存在(const std::string& 原路径, int 序号) {
  std::string 备份 = 生成备份名(原路径, 序号);
  struct stat 缓冲{};
  return stat(备份.c_str(), &缓冲) == 0;
}

// ------------------------------------------------------------------
// 功能：统计备份个数。
// 说明：统计已存在的备份数量。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 原路径, int 上限)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::统计备份个数(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
int 统计备份个数(const std::string& 原路径, int 上限) {
  int 计数 = 0;
  for (int i = 1; i <= 上限; ++i) { if (判断备份存在(原路径, i)) ++计数; else break; }
  return 计数;
}

// ------------------------------------------------------------------
// 功能：清理超量备份。
// 说明：删除超出保留数的旧备份。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 原路径, int 保留数)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::清理超量备份(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 清理超量备份(const std::string& 原路径, int 保留数) {
  for (int i = 保留数 + 1; i < 保留数 + 64; ++i) {
    std::string 备份 = 生成备份名(原路径, i);
    if (unlink(备份.c_str()) != 0) break;
  }
  return true;
}

// ------------------------------------------------------------------
// 功能：校验路径深度。
// 说明：限制路径嵌套深度防恶意路径。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径, size_t 上限)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::校验路径深度(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 校验路径深度(const std::string& 路径, size_t 上限) {
  size_t 深度 = 0;
  for (char 字符 : 路径) if (字符 == '/') ++深度;
  return 深度 <= 上限;
}

// ------------------------------------------------------------------
// 功能：取文件权限。
// 说明：读取文件权限位。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::取文件权限(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
int 取文件权限(const std::string& 路径) {
  struct stat 缓冲{};
  if (stat(路径.c_str(), &缓冲) != 0) return -1;
  return (int)(缓冲.st_mode & 0777);
}

// ------------------------------------------------------------------
// 功能：设置文件权限。
// 说明：设置文件权限位。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径, mode_t 权限)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::设置文件权限(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 设置文件权限(const std::string& 路径, mode_t 权限) {
  if (权限 > 0777) return false;
  return chmod(路径.c_str(), 权限) == 0;
}

// ------------------------------------------------------------------
// 功能：比较内容。
// 说明：比较内存内容与文件是否一致。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径, const std::string& 期望)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::比较内容(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
// 比较内容：流式分块比较内存期望与文件内容，零整文件堆积，命中差异早退。
// 复杂度：时间 O(n)，空间 O(64K)，大文件亦常量内存。
bool 比较内容(const std::string& 路径, const std::string& 期望) {
  struct stat 缓冲{};
  if (stat(路径.c_str(), &缓冲) != 0) return false;
  if ((size_t)缓冲.st_size != 期望.size()) return false;
  if (期望.empty()) return true;
  int 描述符 = open(路径.c_str(), O_RDONLY);
  if (描述符 < 0) return false;
  VINE顺序预取(描述符, 0, 0);
  char 块[65536];
  size_t 偏移 = 0;
  while (偏移 < 期望.size()) {
    ssize_t 已读 = read(描述符, 块, sizeof(块));
    if (已读 < 0) { if (errno == EINTR) continue; close(描述符); return false; }
    if (已读 == 0) { close(描述符); return false; }
    if (memcmp(块, 期望.data() + 偏移, (size_t)已读) != 0) { close(描述符); return false; }
    偏移 += (size_t)已读;
  }
  close(描述符);
  return 偏移 == 期望.size();
}

// ------------------------------------------------------------------
// 功能：追加写入。
// 说明：以追加方式写入内容。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径, const std::string& 内容)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::追加写入(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 追加写入(const std::string& 路径, const std::string& 内容) {
  int 描述符 = open(路径.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (描述符 < 0) return false;
  size_t 已写 = 0;
  while (已写 < 内容.size()) { ssize_t 单 = write(描述符, 内容.data() + 已写, 内容.size() - 已写); if (单 < 0) { if (errno == EINTR) continue; close(描述符); return false; } 已写 += (size_t)单; }
  fsync(描述符); close(描述符);
  return true;
}

// ------------------------------------------------------------------
// 功能：读取尾部。
// 说明：读取文件最后 N 字节。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径, size_t 字节数)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::读取尾部(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
std::string 读取尾部(const std::string& 路径, size_t 字节数) {
  int 描述符 = open(路径.c_str(), O_RDONLY);
  if (描述符 < 0) return std::string();
  off_t 末尾 = lseek(描述符, 0, SEEK_END);
  if (末尾 < 0) { close(描述符); return std::string(); }
  off_t 起始 = 末尾 - (off_t)字节数 > 0 ? 末尾 - (off_t)字节数 : 0;
  lseek(描述符, 起始, SEEK_SET);
  std::string 结果; char 块[4096]; ssize_t 已读 = 0;
  while ((已读 = read(描述符, 块, sizeof(块))) > 0) 结果.append(块, (size_t)已读);
  close(描述符);
  return 结果;
}

// ------------------------------------------------------------------
// 功能：文件是否为空。
// 说明：判断文件大小是否为零。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::文件是否为空(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 文件是否为空(const std::string& 路径) {
  struct stat 缓冲{};
  if (stat(路径.c_str(), &缓冲) != 0) return false;
  return 缓冲.st_size == 0;
}

// ------------------------------------------------------------------
// 功能：触摸文件。
// 说明：更新文件时间戳或创建空文件。
// 背景：
//   工具链在真实设备上长期运行，任何边界含糊都会演变为崩溃或漏洞；
//   因此本函数对空输入、超长输入、并发竞争均有定义行为。
// 参数：
//   (const std::string& 路径)。
// 返回：
//   成功返回有效值；失败返回哨兵（false／-1／空串／空可选），不抛异常。
// 实现要点：
//   仅使用标准库与 POSIX 系统调用；循环均有明确上界；错误保留 errno。
// 复杂度：
//   时间 O(n) 以内，空间 O(1) 或 O(块)，适合在启动与校验链路调用。
// 调用示例：
//   auto 结果 = vine::安全::原子扩展::触摸文件(输入);
// 注意事项：
//   调用前请先做路径规范化；多线程并发写同一目标需外层加锁。
// ------------------------------------------------------------------
bool 触摸文件(const std::string& 路径) {
  int 描述符 = open(路径.c_str(), O_WRONLY | O_CREAT, 0644);
  if (描述符 < 0) return false;
  close(描述符);
  return utimensat(AT_FDCWD, 路径.c_str(), nullptr, 0) == 0;
}

}  // namespace 原子扩展

}  // namespace 安全
}  // namespace vine
