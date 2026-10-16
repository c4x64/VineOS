// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：系统探针集实现
#include "系统探针集.h"
#include "../utils/vine_log.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>
namespace vine {
namespace 诊断 {
static long long 现毫秒针(){ struct timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL; }
std::string 系统快照::转为日志行() const { char b[512]; snprintf(b,sizeof(b),"时刻=%lld 负载=%.2f,%.2f,%.2f 运行=%lld秒 文件=%d/%d",时刻毫秒,一分钟负载,五分钟负载,十五分钟负载,运行时间秒,打开文件数,文件上限); return b; }
系统探针集& 系统探针集::获取实例(){ static 系统探针集 i; return i; }
系统探针集::系统探针集(){}
std::string 系统探针集::读文件(const std::string& p){ int f=open(p.c_str(),O_RDONLY|O_CLOEXEC); if(f<0) return {}; std::string r; char b[4096]; while(true){ ssize_t n=read(f,b,sizeof(b)); if(n<0){ if(errno==EINTR) continue; break; } if(n==0) break; r.append(b,(size_t)n); if(r.size()>262144) break; } close(f); return r; }
namespace { struct 探针表项硬 { const char* 键; const char* 路径; const char* 空告警; size_t 截断; }; static constexpr 探针表项硬 探针表硬[15] = { {"处理器信息","/proc/cpuinfo","处理器信息读取为空",8192}, {"内存信息","/proc/meminfo","内存信息读取为空",8192}, {"平均负载","/proc/loadavg","平均负载读取为空",4096}, {"运行时间","/proc/uptime","运行时间读取为空",4096}, {"网络设备","/proc/net/dev","网络设备读取为空",8192}, {"磁盘统计","/proc/diskstats","磁盘统计读取为空",8192}, {"文件描述符","/proc/sys/fs/file-nr","文件描述符读取为空",4096}, {"进程状态","/proc/stat","进程状态读取为空",8192}, {"电池状态","/sys/class/power_supply/battery/capacity","电池状态读取为空",4096}, {"温度区域","/sys/class/thermal/thermal_zone0/temp","温度区域读取为空",4096}, {"频率表","/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","频率表读取为空",4096}, {"内存压力","/proc/pressure/memory","内存压力读取为空",4096}, {"中断统计","/proc/interrupts","中断统计读取为空",8192}, {"上下文切换","/proc/stat","上下文切换读取为空",8192}, {"进程数","/proc/loadavg","进程数读取为空",4096} }; }
const 系统探针集::探针描述* 系统探针集::探针表(size_t& 数量) { 数量 = 15; return reinterpret_cast<const 探针描述*>(探针表硬); }
const 系统探针集::探针描述* 系统探针集::按键查找(const std::string& 键) { size_t 数量=0; auto 表=探针表(数量); for (size_t i=0;i<数量;++i) if (键==表[i].键) return &表[i]; return nullptr; }
std::string 系统探针集::批量读文件(const std::string& 路径, size_t 上限) { if (路径.empty()) return {}; if (上限 < 4096) 上限 = 4096; if (上限 > 262144) 上限 = 262144; int f=open(路径.c_str(),O_RDONLY|O_CLOEXEC); if(f<0) return {}; std::string r; r.reserve(8192); char b[4096]; while(r.size()<上限){ ssize_t n=read(f,b,sizeof(b)); if(n<0){ if(errno==EINTR) continue; break; } if(n==0) break; r.append(b,(size_t)n); } close(f); return r; }
std::string 系统探针集::通用采集(const 探针描述& 描述) { std::string 原文 = 批量读文件(描述.路径, 262144); if (原文.empty()) { VINE_LOGW("%s", 描述.空告警); return {}; } std::string 输出; if (!通用解析(原文, 输出, 描述.截断)) return 原文.substr(0, 4096); return 输出; }
bool 系统探针集::通用解析(const std::string& 文本, std::string& 输出, size_t 截断) { if (文本.empty()) return false; size_t 末 = 文本.find_last_not_of(" \t\r\n"); if (末 == std::string::npos) return false; size_t 限 = (末 + 1 > 截断) ? 截断 : (末 + 1); 输出 = 文本.substr(0, 限); return !输出.empty(); }
// —— 处理器信息（cpuinfo）采集与解析 ——
// 路径：/proc/cpuinfo；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集处理器信息() { auto 描述=按键查找("处理器信息"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析处理器信息(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("处理器信息"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 内存信息（meminfo）采集与解析 ——
// 路径：/proc/meminfo；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集内存信息() { auto 描述=按键查找("内存信息"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析内存信息(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("内存信息"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 平均负载（loadavg）采集与解析 ——
// 路径：/proc/loadavg；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集平均负载() { auto 描述=按键查找("平均负载"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析平均负载(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("平均负载"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 运行时间（uptime）采集与解析 ——
// 路径：/proc/uptime；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集运行时间() { auto 描述=按键查找("运行时间"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析运行时间(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("运行时间"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 网络设备（net_dev）采集与解析 ——
// 路径：/proc/net/dev；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集网络设备() { auto 描述=按键查找("网络设备"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析网络设备(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("网络设备"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 磁盘统计（diskstats）采集与解析 ——
// 路径：/proc/diskstats；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集磁盘统计() { auto 描述=按键查找("磁盘统计"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析磁盘统计(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("磁盘统计"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 文件描述符（fd_count）采集与解析 ——
// 路径：/proc/sys/fs/file-nr；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集文件描述符() { auto 描述=按键查找("文件描述符"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析文件描述符(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("文件描述符"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 进程状态（proc_stat）采集与解析 ——
// 路径：/proc/stat；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集进程状态() { auto 描述=按键查找("进程状态"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析进程状态(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("进程状态"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 电池状态（battery）采集与解析 ——
// 路径：/sys/class/power_supply/battery/capacity；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集电池状态() { auto 描述=按键查找("电池状态"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析电池状态(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("电池状态"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 温度区域（thermal）采集与解析 ——
// 路径：/sys/class/thermal/thermal_zone0/temp；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集温度区域() { auto 描述=按键查找("温度区域"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析温度区域(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("温度区域"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 频率表（freq_table）采集与解析 ——
// 路径：/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集频率表() { auto 描述=按键查找("频率表"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析频率表(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("频率表"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 内存压力（pressure）采集与解析 ——
// 路径：/proc/pressure/memory；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集内存压力() { auto 描述=按键查找("内存压力"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析内存压力(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("内存压力"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 中断统计（interrupts）采集与解析 ——
// 路径：/proc/interrupts；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集中断统计() { auto 描述=按键查找("中断统计"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析中断统计(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("中断统计"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 上下文切换（ctxt）采集与解析 ——
// 路径：/proc/stat；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集上下文切换() { auto 描述=按键查找("上下文切换"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析上下文切换(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("上下文切换"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

// —— 进程数（processes）采集与解析 ——
// 路径：/proc/loadavg；策略：短读 + 去空白 + 截断保护。
std::string 系统探针集::采集进程数() { auto 描述=按键查找("进程数"); if (!描述) return {}; return 通用采集(*描述); }
bool 系统探针集::解析进程数(const std::string& 文本, std::string& 输出) { auto 描述=按键查找("进程数"); size_t 截断=描述?描述->截断:8192; return 通用解析(文本, 输出, 截断); }

处理器描述 系统探针集::查询处理器(){ 处理器描述 d; std::string c=批量读文件("/proc/cpuinfo",262144); if(c.empty()) return d; d.各核频率.reserve(16); std::istringstream s(c); std::string l; int 核=0; while(std::getline(s,l)){ if(l.rfind("processor",0)==0) ++核; if(l.find("Features")!=std::string::npos&&d.特性.empty()){ d.特性=l.size()>256?l.substr(0,256):l; } } d.核心数=核; d.架构="arm64"; return d; }
std::vector<网络计数> 系统探针集::查询网络(){ std::vector<网络计数> r; r.reserve(8); std::string c=批量读文件("/proc/net/dev",65536); if(c.empty()) return r; std::istringstream s(c); std::string l; int 行=0; while(std::getline(s,l)){ ++行; if(行<=2) continue; auto 冒号=l.find(':'); if(冒号==std::string::npos) continue; 网络计数 n; std::string 名=l.substr(0,冒号); size_t 起=名.find_first_not_of(" \t"); size_t 止=名.find_last_not_of(" \t"); if(起==std::string::npos) continue; 名=名.substr(起,止-起+1); if(名.size()>64) 名.resize(64); n.接口名=名; unsigned long long 段[16]={0}; const char* 始=l.c_str()+冒号+1; int 取=sscanf(始,"%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &段[0],&段[1],&段[2],&段[3],&段[4],&段[5],&段[6],&段[7],&段[8],&段[9],&段[10],&段[11],&段[12],&段[13],&段[14],&段[15]); if(取>=10){ n.接收字节=段[0]; n.接收包=段[1]; n.发送字节=段[8]; n.发送包=段[9]; r.push_back(std::move(n)); } if(r.size()>=32) break; } return r; }
系统快照 系统探针集::采集系统快照(){ 系统快照 s; s.时刻毫秒=现毫秒针(); std::string 负=批量读文件("/proc/loadavg",4096); if(!负.empty()){ int 运行=0,总数=0; if(sscanf(负.c_str(),"%lf %lf %lf %d/%d",&s.一分钟负载,&s.五分钟负载,&s.十五分钟负载,&运行,&总数)>=3){} } std::string 运=批量读文件("/proc/uptime",4096); if(!运.empty()){ double a=0,b=0; if(sscanf(运.c_str(),"%lf %lf",&a,&b)==2){ s.运行时间秒=(long long)a; s.空闲时间秒=(long long)b; }} std::string 描=批量读文件("/proc/sys/fs/file-nr",4096); if(!描.empty()){ int 已用=0,上限=0,垫=0; if(sscanf(描.c_str(),"%d %d %d",&已用,&垫,&上限)>=1){ s.打开文件数=已用; s.文件上限=上限; } } return s; }
std::map<std::string,std::string> 系统探针集::采集全部(){ std::map<std::string,std::string> m; m["快照"]=采集系统快照().转为日志行(); size_t 数量=0; auto 表=探针表(数量); for(size_t i=0;i<数量;++i){ if(m.size()>=16) break; m[表[i].键]=通用采集(表[i]).substr(0,4096); } return m; }
std::string 系统探针集::生成报告(){
  std::string r; r.reserve(4096); r+="=== 系统探针 ===\n";
  r+=采集系统快照().转为日志行()+"\n";
  auto 网=查询网络(); for(auto& n:网){ char b[256]; snprintf(b,sizeof(b),"网卡 %s 收=%llu 发=%llu\n",n.接口名.c_str(),n.接收字节,n.发送字节); r+=b; if(r.size()>16384) break; }
  auto 处=查询处理器(); r+=处.架构+" 核心="+std::to_string(处.核心数)+"\n";
  return r; }
}  // namespace 诊断
}  // namespace vine
