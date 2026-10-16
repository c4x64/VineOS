// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：日志中心实现
#include "日志中心.h"
#include "../utils/vine_log.h"
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
namespace vine {
namespace 诊断 {
static long long 现毫秒志(){ struct timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL; }
std::string 日志条目::转为日志行() const { char b[4096]; snprintf(b,sizeof(b),"[%lld][%s][%s] %s",时刻毫秒,日志中心::等级转名(等级).c_str(),标签.c_str(),内容.c_str()); return b; }
std::string 日志中心::截断文本(const std::string& 文本, size_t 上限) { if (文本.size() <= 上限) return 文本; return 文本.substr(0, 上限); }
size_t 日志中心::钳制容量(size_t 输入) { if (输入 < 最小容量_硬) return 最小容量_硬; if (输入 > 最大容量_硬) return 最大容量_硬; return 输入; }
size_t 日志中心::钳制查询上限(size_t 输入) { if (输入 == 0) return 0; if (输入 > 查询上限封顶_硬) return 查询上限封顶_硬; return 输入; }
void 日志中心::通用记录(日志等级 等级, const std::string& 标签, const std::string& 内容) { 写入(等级, 标签, 内容); }
std::vector<日志条目> 日志中心::通用查询(日志等级 最低, const 日志查询条件& 条件) const { 日志查询条件 复制 = 条件; 复制.最低等级 = 最低; 复制.上限 = 钳制查询上限(复制.上限); return 查询(复制); }
size_t 日志中心::通用统计(日志等级 等级, long long 窗口毫秒) const { std::lock_guard<std::mutex> 守卫(互斥锁_); if (窗口毫秒 < 0) 窗口毫秒 = 0; long long 现 = 现毫秒志(); size_t 计 = 0; for (auto& 条 : 缓冲_) { if ((int)条.等级 != (int)等级) continue; if (现 - 条.时刻毫秒 > 窗口毫秒) continue; ++计; } return 计; }
std::string 日志中心::等级转名(日志等级 e){ switch(e){ case 日志等级::详细: return "VERBOSE"; case 日志等级::调试: return "DEBUG"; case 日志等级::信息: return "INFO"; case 日志等级::警告: return "WARN"; case 日志等级::错误: return "ERROR"; case 日志等级::致命: return "FATAL"; } return "INFO"; }
bool 日志中心::从名转等级(const std::string& n, 日志等级& o){ if(n=="VERBOSE"){o=日志等级::详细;return true;} if(n=="DEBUG"){o=日志等级::调试;return true;} if(n=="INFO"){o=日志等级::信息;return true;} if(n=="WARN"){o=日志等级::警告;return true;} if(n=="ERROR"){o=日志等级::错误;return true;} if(n=="FATAL"){o=日志等级::致命;return true;} return false; }
日志中心& 日志中心::获取实例(){ static 日志中心 i; return i; }
日志中心::日志中心(){}
void 日志中心::写入(日志等级 等级,const std::string& 标签,const std::string& 内容){ std::string 切标签=截断文本(标签,标签上限字节_硬); std::string 切内容=截断文本(内容,单条上限字节_硬); std::lock_guard<std::mutex> g(互斥锁_); 容量_=钳制容量(容量_); 日志条目 e; e.序号=++序号_; e.时刻毫秒=现毫秒志(); e.等级=等级; e.标签=切标签; e.内容=切内容; e.线程号=(int)gettid(); 缓冲_.push_back(e); while(缓冲_.size()>容量_)缓冲_.pop_front(); }
std::vector<日志条目> 日志中心::查询(const 日志查询条件& c) const { size_t 上限=钳制查询上限(c.上限); std::lock_guard<std::mutex> g(互斥锁_); std::vector<日志条目> r; if(上限==0) return r; size_t 预留=上限 < 缓冲_.size() ? 上限 : 缓冲_.size(); r.reserve(预留); for(auto& e:缓冲_){ if((int)e.等级<(int)c.最低等级) continue; if(c.起始毫秒!=0&&e.时刻毫秒<c.起始毫秒) continue; if(c.结束毫秒!=0&&e.时刻毫秒>c.结束毫秒) continue; if(!c.标签过滤.empty()&&e.标签.find(c.标签过滤)==std::string::npos) continue; if(!c.关键字.empty()&&e.内容.find(c.关键字)==std::string::npos&&e.标签.find(c.关键字)==std::string::npos) continue; r.push_back(e); if(r.size()>=上限) break; } return r; }
// —— 详细日志写入/查询/统计 ——
void 日志中心::记录详细(const std::string& 标签,const std::string& 内容) { 通用记录(日志等级::详细, 标签, 内容); }
std::vector<日志条目> 日志中心::查询详细及以上(const 日志查询条件& 条件) const { return 通用查询(日志等级::详细, 条件); }
size_t 日志中心::统计详细条数(long long 窗口毫秒) const { return 通用统计(日志等级::详细, 窗口毫秒); }

// —— 调试日志写入/查询/统计 ——
void 日志中心::记录调试(const std::string& 标签,const std::string& 内容) { 通用记录(日志等级::调试, 标签, 内容); }
std::vector<日志条目> 日志中心::查询调试及以上(const 日志查询条件& 条件) const { return 通用查询(日志等级::调试, 条件); }
size_t 日志中心::统计调试条数(long long 窗口毫秒) const { return 通用统计(日志等级::调试, 窗口毫秒); }

// —— 信息日志写入/查询/统计 ——
void 日志中心::记录信息(const std::string& 标签,const std::string& 内容) { 通用记录(日志等级::信息, 标签, 内容); }
std::vector<日志条目> 日志中心::查询信息及以上(const 日志查询条件& 条件) const { return 通用查询(日志等级::信息, 条件); }
size_t 日志中心::统计信息条数(long long 窗口毫秒) const { return 通用统计(日志等级::信息, 窗口毫秒); }

// —— 警告日志写入/查询/统计 ——
void 日志中心::记录警告(const std::string& 标签,const std::string& 内容) { 通用记录(日志等级::警告, 标签, 内容); }
std::vector<日志条目> 日志中心::查询警告及以上(const 日志查询条件& 条件) const { return 通用查询(日志等级::警告, 条件); }
size_t 日志中心::统计警告条数(long long 窗口毫秒) const { return 通用统计(日志等级::警告, 窗口毫秒); }

// —— 错误日志写入/查询/统计 ——
void 日志中心::记录错误(const std::string& 标签,const std::string& 内容) { 通用记录(日志等级::错误, 标签, 内容); }
std::vector<日志条目> 日志中心::查询错误及以上(const 日志查询条件& 条件) const { return 通用查询(日志等级::错误, 条件); }
size_t 日志中心::统计错误条数(long long 窗口毫秒) const { return 通用统计(日志等级::错误, 窗口毫秒); }

// —— 致命日志写入/查询/统计 ——
void 日志中心::记录致命(const std::string& 标签,const std::string& 内容) { 通用记录(日志等级::致命, 标签, 内容); }
std::vector<日志条目> 日志中心::查询致命及以上(const 日志查询条件& 条件) const { return 通用查询(日志等级::致命, 条件); }
size_t 日志中心::统计致命条数(long long 窗口毫秒) const { return 通用统计(日志等级::致命, 窗口毫秒); }

bool 日志中心::落盘轮转(const std::string& 目录,size_t 上限){ if(目录.empty()) return false; size_t 封顶=上限; if(封顶<65536) 封顶=65536; if(封顶>67108864) 封顶=67108864; mkdir(目录.c_str(),0755); std::vector<日志条目> 快照; { std::lock_guard<std::mutex> g(互斥锁_); 快照.reserve(缓冲_.size()); for(auto& e:缓冲_) 快照.push_back(e); } std::string 路=目录+"/日志中心.log"; FILE* f=fopen(路.c_str(),"a"); if(!f) return false; for(auto& e:快照) fprintf(f,"%s\n",e.转为日志行().c_str()); fclose(f); struct stat 信{}; if(stat(路.c_str(),&信)==0&&(size_t)信.st_size>封顶){ for(int 轮=3;轮>=1;--轮){ char 旧[512]; char 新[512]; snprintf(旧,sizeof(旧),"%s/日志中心.%d.log",目录.c_str(),轮); if(轮==1){ snprintf(新,sizeof(新),"%s/日志中心.log",目录.c_str()); } else { snprintf(新,sizeof(新),"%s/日志中心.%d.log",目录.c_str(),轮-1); } rename(新,旧); } } return true; }
bool 日志中心::导出到文件(const std::string& p,const 日志查询条件& c) const { if(p.empty()) return false; 日志查询条件 复制=c; 复制.上限=钳制查询上限(复制.上限); auto v=查询(复制); FILE* f=fopen(p.c_str(),"w"); if(!f) return false; for(auto& e:v) fprintf(f,"%s\n",e.转为日志行().c_str()); fclose(f); return true; }
void 日志中心::设置容量(size_t n){ std::lock_guard<std::mutex> g(互斥锁_); 容量_=钳制容量(n); while(缓冲_.size()>容量_)缓冲_.pop_front(); }
void 日志中心::清空(){ std::lock_guard<std::mutex> g(互斥锁_); 缓冲_.clear(); }
size_t 日志中心::条数() const { std::lock_guard<std::mutex> g(互斥锁_); return 缓冲_.size(); }
}  // namespace 诊断
}  // namespace vine
