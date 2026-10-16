// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：线程转储器实现
#include "线程转储器.h"
#include "../utils/vine_log.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>
namespace vine {
namespace 诊断 {
namespace {
std::string 读小(const std::string& p){ int f=open(p.c_str(),O_RDONLY|O_CLOEXEC); if(f<0) return {}; std::string r; r.reserve(2048); char b[2048]; while(true){ ssize_t n=read(f,b,sizeof(b)); if(n<0){ if(errno==EINTR) continue; break; } if(n==0) break; r.append(b,(size_t)n); if(r.size()>262144) break; } close(f); return r; }
} // namespace
long long 当前毫秒转储(){ struct timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL; }
std::string 状态字符转描述(char s){ switch(s){ case 'R': return "运行态"; case 'S': return "睡眠态"; case 'D': return "磁盘睡眠"; case 'T': return "停止态"; case 't': return "追踪停止"; case 'Z': return "僵尸态"; case 'X': return "死亡态"; case 'I': return "空闲内核"; default: { char b[16]; snprintf(b,sizeof(b),"未知(%c)",s); return b; } } }
namespace { struct 状态表项 { char 符号; const char* 名称; const char* 描述; }; static constexpr 状态表项 状态表硬[8] = { {'R',"运行态","运行态：内核调度状态 R，详见 proc(5) 手册"}, {'S',"睡眠态","睡眠态：内核调度状态 S，详见 proc(5) 手册"}, {'D',"磁盘睡眠","磁盘睡眠：内核调度状态 D，详见 proc(5) 手册"}, {'T',"停止态","停止态：内核调度状态 T，详见 proc(5) 手册"}, {'t',"追踪停止","追踪停止：内核调度状态 t，详见 proc(5) 手册"}, {'Z',"僵尸态","僵尸态：内核调度状态 Z，详见 proc(5) 手册"}, {'X',"死亡态","死亡态：内核调度状态 X，详见 proc(5) 手册"}, {'I',"空闲内核","空闲内核：内核调度状态 I，详见 proc(5) 手册"} }; }
char 线程转储器::状态表符号(size_t 下标) { return 下标 < 8 ? 状态表硬[下标].符号 : '?'; }
const char* 线程转储器::状态表名称(size_t 下标) { return 下标 < 8 ? 状态表硬[下标].名称 : "未知"; }
size_t 线程转储器::状态表大小() { return 8; }
bool 线程转储器::通用是否(char 实际, char 目标) const { return 实际 == 目标; }
size_t 线程转储器::通用统计数量(char 目标, const 线程转储& 转储) const { size_t 计数 = 0; for (auto& 线程 : 转储.线程列表) if (线程.状态 == 目标) ++计数; return 计数; }
std::string 线程转储器::通用描述(char 目标) const { for (auto& 项 : 状态表硬) if (项.符号 == 目标) return std::string(项.描述); char 缓冲[32]; snprintf(缓冲, sizeof(缓冲), "未知状态(%c)", 目标); return std::string(缓冲); }
std::string 线程信息::转为日志行() const { char b[1024]; snprintf(b,sizeof(b),"tid=%d 名=%s 状态=%c(%s) 用户=%lld 内核=%lld 常驻=%lldKB 等待=%s 主线程=%d",线程号,线程名.c_str(),状态,状态描述.c_str(),用户节拍,内核节拍,常驻千字节,等待通道.c_str(),是否主线程?1:0); return b; }
std::string 线程信息::转为简表() const { char b[256]; snprintf(b,sizeof(b),"%5d %-16s %c",线程号,线程名.c_str(),状态); return b; }
bool 线程信息::是否异常() const { return 状态=='D'||状态=='Z'||状态=='X'; }
std::string 线程转储::转为文本() const { std::string r; char h[128]; snprintf(h,sizeof(h),"转储 时刻=%lld 进程=%lld 线程数=%zu\n",采集时刻毫秒,进程标识,线程列表.size()); r+=h; for(auto& t:线程列表) r+=t.转为日志行()+"\n"; return r; }
std::string 线程转储::转为摘要() const { char b[256]; snprintf(b,sizeof(b),"线程=%zu 运行=%zu 睡眠=%zu",线程列表.size(),运行中数量(),睡眠中数量()); return b; }
size_t 线程转储::运行中数量() const { size_t n=0; for(auto& t:线程列表) if(t.状态=='R') ++n; return n; }
size_t 线程转储::睡眠中数量() const { size_t n=0; for(auto& t:线程列表) if(t.状态=='S') ++n; return n; }
线程转储器& 线程转储器::获取实例(){ static 线程转储器 i; return i; }
线程转储器::线程转储器(){}
// —— 运行态（R）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否运行态(char 状态) const { return 通用是否(状态, 'R'); }
size_t 线程转储器::统计运行态数量(const 线程转储& 转储) const { return 通用统计数量('R', 转储); }
std::string 线程转储器::描述运行态() const { return 通用描述('R'); }

// —— 睡眠态（S）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否睡眠态(char 状态) const { return 通用是否(状态, 'S'); }
size_t 线程转储器::统计睡眠态数量(const 线程转储& 转储) const { return 通用统计数量('S', 转储); }
std::string 线程转储器::描述睡眠态() const { return 通用描述('S'); }

// —— 磁盘睡眠（D）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否磁盘睡眠(char 状态) const { return 通用是否(状态, 'D'); }
size_t 线程转储器::统计磁盘睡眠数量(const 线程转储& 转储) const { return 通用统计数量('D', 转储); }
std::string 线程转储器::描述磁盘睡眠() const { return 通用描述('D'); }

// —— 停止态（T）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否停止态(char 状态) const { return 通用是否(状态, 'T'); }
size_t 线程转储器::统计停止态数量(const 线程转储& 转储) const { return 通用统计数量('T', 转储); }
std::string 线程转储器::描述停止态() const { return 通用描述('T'); }

// —— 追踪停止（t）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否追踪停止(char 状态) const { return 通用是否(状态, 't'); }
size_t 线程转储器::统计追踪停止数量(const 线程转储& 转储) const { return 通用统计数量('t', 转储); }
std::string 线程转储器::描述追踪停止() const { return 通用描述('t'); }

// —— 僵尸态（Z）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否僵尸态(char 状态) const { return 通用是否(状态, 'Z'); }
size_t 线程转储器::统计僵尸态数量(const 线程转储& 转储) const { return 通用统计数量('Z', 转储); }
std::string 线程转储器::描述僵尸态() const { return 通用描述('Z'); }

// —— 死亡态（X）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否死亡态(char 状态) const { return 通用是否(状态, 'X'); }
size_t 线程转储器::统计死亡态数量(const 线程转储& 转储) const { return 通用统计数量('X', 转储); }
std::string 线程转储器::描述死亡态() const { return 通用描述('X'); }

// —— 空闲内核（I）判断三件套 ——
// 说明：状态字符来自 /proc 任务状态字段，单字符高效比较。
bool 线程转储器::是否空闲内核(char 状态) const { return 通用是否(状态, 'I'); }
size_t 线程转储器::统计空闲内核数量(const 线程转储& 转储) const { return 通用统计数量('I', 转储); }
std::string 线程转储器::描述空闲内核() const { return 通用描述('I'); }

线程转储 线程转储器::采集转储() {
  线程转储 转储; 转储.采集时刻毫秒=当前毫秒转储(); 转储.进程标识=(long long)getpid();
  std::vector<int> 编号; 编号.reserve(64); if(!枚举线程号(编号)) return 转储;
  if (编号.size() > 512) 编号.resize(512);
  转储.线程列表.reserve(编号.size());
  for(int 号:编号){ 线程信息 信息; if(读取单线程(号,信息)) { if (信息.线程名.size() > 256) 信息.线程名.resize(256); if (信息.等待通道.size() > 256) 信息.等待通道.resize(256); if (信息.内核栈摘要.size() > 512) 信息.内核栈摘要.resize(512); 转储.线程列表.push_back(std::move(信息)); } }
  return 转储;
}
bool 线程转储器::枚举线程号(std::vector<int>& 输出) {
  输出.clear(); 输出.reserve(64); DIR* 目录=opendir("/proc/self/task"); if(!目录) return false;
  struct dirent* 项; while((项=readdir(目录))!=nullptr){
    if(项->d_name[0]=='.') continue;
    char* 尾=nullptr; long 值=strtol(项->d_name,&尾,10);
    if(尾!=项->d_name && *尾=='\0') 输出.push_back((int)值);
  }
  closedir(目录); return !输出.empty();
}
bool 线程转储器::读取单线程(int 线程号, 线程信息& 输出) {
  输出.线程号=线程号; 输出.是否主线程=(线程号==getpid());
  char 路径[128]; snprintf(路径,sizeof(路径),"/proc/self/task/%d/stat",线程号);
  std::string 内容=读小(路径); if(内容.empty()) return false;
  if(!解析状态行(内容,输出)) return false;
  读取线程名(线程号,输出.线程名); 输出.状态描述=状态字符转描述(输出.状态);
  读取等待通道(线程号,输出.等待通道); 读取内核栈(线程号,输出.内核栈摘要);
  snprintf(路径,sizeof(路径),"/proc/self/task/%d/statm",线程号);
  std::string 内存=读小(路径); if(!内存.empty()){ long 虚=0,常=0; if(sscanf(内存.c_str(),"%ld %ld",&虚,&常)==2){ long 页=sysconf(_SC_PAGESIZE)/1024; if(页<=0)页=4; 输出.虚拟内存千字节=(long long)虚*页; 输出.常驻千字节=(long long)常*页; } }
  return true;
}
bool 线程转储器::解析状态行(const std::string& 内容, 线程信息& 输出) {
  auto 右括号=内容.rfind(')'); if(右括号==std::string::npos) return false;
  const char* 始=内容.c_str()+右括号+1; const char* 终=内容.c_str()+内容.size();
  long long 槽[20]={0}; int 列=0; bool 负=false; bool 在数=false; long long 当前=0;
  char 状态字符='?'; bool 已取状态=false;
  for(const char* 指针=始;指针<终;++指针){ char 字符=*指针; bool 空白=(字符==' '||字符=='\t'); if(!已取状态){ if(空白) continue; 状态字符=字符; 已取状态=true; while(指针+1<终&&*(指针+1)!=' '&&*(指针+1)!='\t') ++指针; 列=0; continue; } if(空白){ if(在数){ if(列<20) 槽[列]=负?-当前:当前; ++列; } 在数=false; 当前=0; 负=false; continue; } if(字符=='-'&&!在数){ 负=true; 在数=true; 当前=0; continue; } if(字符>='0'&&字符<='9'){ 在数=true; 当前=当前*10+(字符-'0'); continue; } if(在数){ if(列<20) 槽[列]=负?-当前:当前; ++列; 在数=false; 当前=0; 负=false; while(指针+1<终&&*(指针+1)!=' '&&*(指针+1)!='\t') ++指针; } }
  if(在数&&列<20){ 槽[列]=负?-当前:当前; ++列; }
  if(!已取状态||列<18) return false;
  输出.状态=状态字符; 输出.用户节拍=槽[11]; 输出.内核节拍=槽[12]; 输出.优先级=槽[15]; 输出.线程数=槽[17];
  return true;
}
bool 线程转储器::读取等待通道(int 线程号, std::string& 输出) { char p[128]; snprintf(p,sizeof(p),"/proc/self/task/%d/wchan",线程号); 输出=读小(p); if(!输出.empty()&&输出.back()=='\n')输出.pop_back(); 输出.erase(std::remove(输出.begin(),输出.end(),'\n'),输出.end()); return true; }
bool 线程转储器::读取内核栈(int 线程号, std::string& 输出) { char p[128]; snprintf(p,sizeof(p),"/proc/self/task/%d/stack",线程号); std::string 全=读小(p); if(全.size()>512) 全=全.substr(0,512); 输出=全; return true; }
bool 线程转储器::读取线程名(int 线程号, std::string& 输出) { char p[128]; snprintf(p,sizeof(p),"/proc/self/task/%d/comm",线程号); 输出=读小(p); if(!输出.empty()&&输出.back()=='\n')输出.pop_back(); if(输出.empty()){ snprintf(p,sizeof(p),"/proc/self/task/%d/cmdline",线程号); 输出=读小(p);} return true; }
std::vector<线程信息> 线程转储器::筛选异常线程(const 线程转储& 转储){ std::vector<线程信息> r; for(auto& t:转储.线程列表) if(t.是否异常()) r.push_back(t); return r; }
std::string 线程转储器::生成可读报告(const 线程转储& 转储){ std::string r="=== 线程转储 ===\n"+转储.转为摘要()+"\n"; for(auto& t:转储.线程列表){ r+="  "+t.转为日志行()+"\n"; if(!t.内核栈摘要.empty()) r+="    栈:"+t.内核栈摘要.substr(0,200)+"\n"; } return r; }
bool 线程转储器::落盘(const 线程转储& 转储, const std::string& 路径){ FILE* f=fopen(路径.c_str(),"w"); if(!f) return false; std::string t=转储.转为文本(); fwrite(t.data(),1,t.size(),f); fclose(f); return true; }
}  // namespace 诊断
}  // namespace vine
