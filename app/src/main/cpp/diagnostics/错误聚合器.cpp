// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：错误聚合器实现
#include "错误聚合器.h"
#include "../utils/vine_log.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <unistd.h>
namespace vine {
namespace 诊断 {
static long long 当前毫秒错误(){ struct timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL; }
std::string 提取堆栈首行(const std::string& s){ auto p=s.find('\n'); std::string f=(p==std::string::npos)?s:s.substr(0,p); while(!f.empty()&&(f.back()=='\r'||f.back()==' '))f.pop_back(); size_t b=f.find_first_not_of(' '); if(b!=std::string::npos) f=f.substr(b); if(f.size()>256) f=f.substr(0,256); return f; }
std::string 错误记录::转为日志行() const { char b[2048]; snprintf(b,sizeof(b),"序号=%lld 时间=%lld 类别=%s 标签=%s 指纹=%s 进程=%d 线程=%d 摘要=%s",序号,发生毫秒,类别.c_str(),标签.c_str(),指纹.c_str(),进程号,线程号,摘要.c_str()); return b; }
std::string 错误分组::生成摘要() const { char b[1024]; snprintf(b,sizeof(b),"指纹=%s 类别=%s 次数=%lld 代表=%s",指纹.c_str(),类别.c_str(),次数,代表摘要.c_str()); return b; }
double 错误分组::计算频率(long long 窗口毫秒) const { if(时刻表.empty()||窗口毫秒<=0) return 0; long long 现=时刻表.back(); long long 计=0; for(auto t:时刻表) if(现-t<=窗口毫秒) ++计; return (double)计/((double)窗口毫秒/3600000.0); }
错误聚合器& 错误聚合器::获取实例(){ static 错误聚合器 i; return i; }
错误聚合器::错误聚合器(){}
std::string 错误聚合器::截断文本错误(const std::string& 文本, size_t 上限) { if (文本.size() <= 上限) return 文本; return 文本.substr(0, 上限); }
long long 错误聚合器::通用上报(const std::string& 类别, const std::string& 摘要, const std::string& 堆栈) { return 上报错误(类别, 类别, 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::通用查询分组(const std::string& 类别) const { std::lock_guard<std::mutex> g(互斥锁_); std::vector<错误分组> r; r.reserve(分组_.size()); for (auto& kv : 分组_) if (kv.second.类别 == 类别) r.push_back(kv.second); return r; }
long long 错误聚合器::通用统计次数(const std::string& 类别, long long 窗口毫秒) const { std::lock_guard<std::mutex> g(互斥锁_); if (窗口毫秒 < 0) 窗口毫秒 = 0; long long 合计 = 0; long long 现 = 当前毫秒错误(); for (auto& r : 记录_) { if (r.类别 != 类别) continue; if (现 - r.发生毫秒 > 窗口毫秒) continue; ++合计; } return 合计; }
std::string 错误聚合器::规范化堆栈(const std::string& s){ std::string 首=提取堆栈首行(s); std::string r; for(char ch:首){ if((ch>='0'&&ch<='9')) r.push_back('#'); else r.push_back(ch); } return r; }
std::string 错误聚合器::计算指纹(const std::string& 类别,const std::string& 摘要,const std::string& 堆栈首行){ std::string 规=规范化堆栈(堆栈首行); std::string 联=类别+"|"+提取堆栈首行(摘要).substr(0,128)+"|"+规.substr(0,128); unsigned long long 哈希=1469598103934665603ULL; for(char ch:联){ 哈希^=(unsigned char)ch; 哈希*=1099511628211ULL; } char b[32]; snprintf(b,sizeof(b),"%016llx",哈希); return 类别+"_"+b; }
long long 错误聚合器::上报错误(const std::string& 类别,const std::string& 标签,const std::string& 摘要,const std::string& 堆栈){ std::string 切类=截断文本错误(类别,类别上限_硬); if(切类.empty()) 切类="未知"; std::string 切标=截断文本错误(标签,类别上限_硬); std::string 切摘=截断文本错误(摘要,摘要上限_硬); std::string 切栈=截断文本错误(堆栈,堆栈上限_硬); long long 回=0; std::string 行; { std::lock_guard<std::mutex> g(互斥锁_); 错误记录 r; r.序号=++序号_; r.发生毫秒=当前毫秒错误(); r.类别=切类; r.标签=切标; r.摘要=切摘; r.堆栈=切栈; r.指纹=计算指纹(切类,切摘,提取堆栈首行(切栈)); r.进程号=(int)getpid(); r.线程号=0; 记录_.push_back(std::move(r)); const 错误记录& 存=记录_.back(); if(记录_.size()>记录上限_硬) 记录_.pop_front(); auto it=分组_.find(存.指纹); if(it==分组_.end()){ 错误分组 分; 分.指纹=存.指纹; 分.类别=切类; 分.代表摘要=切摘; 分.首次毫秒=存.发生毫秒; 分.末次毫秒=存.发生毫秒; 分.次数=1; 分.时刻表.reserve(8); 分.时刻表.push_back(存.发生毫秒); 分组_[存.指纹]=std::move(分); } else { it->second.末次毫秒=存.发生毫秒; it->second.次数++; it->second.时刻表.push_back(存.发生毫秒); if(it->second.时刻表.size()>时刻表上限_硬){ size_t 超=it->second.时刻表.size()-时刻表上限_硬; it->second.时刻表.erase(it->second.时刻表.begin(), it->second.时刻表.begin()+超); } } 回=存.序号; 行=存.转为日志行(); } VINE_LOGI("错误上报 %s",行.c_str()); return 回; }
// —— 原生崩溃上报/查询/统计 ——
long long 错误聚合器::上报原生崩溃(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("原生崩溃", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询原生崩溃分组() const { return 通用查询分组("原生崩溃"); }
long long 错误聚合器::统计原生崩溃次数(long long 窗口毫秒) const { return 通用统计次数("原生崩溃", 窗口毫秒); }

// —— 应用无响应上报/查询/统计 ——
long long 错误聚合器::上报应用无响应(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("应用无响应", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询应用无响应分组() const { return 通用查询分组("应用无响应"); }
long long 错误聚合器::统计应用无响应次数(long long 窗口毫秒) const { return 通用统计次数("应用无响应", 窗口毫秒); }

// —— 爪哇异常上报/查询/统计 ——
long long 错误聚合器::上报爪哇异常(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("爪哇异常", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询爪哇异常分组() const { return 通用查询分组("爪哇异常"); }
long long 错误聚合器::统计爪哇异常次数(long long 窗口毫秒) const { return 通用统计次数("爪哇异常", 窗口毫秒); }

// —— 原生信号上报/查询/统计 ——
long long 错误聚合器::上报原生信号(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("原生信号", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询原生信号分组() const { return 通用查询分组("原生信号"); }
long long 错误聚合器::统计原生信号次数(long long 窗口毫秒) const { return 通用统计次数("原生信号", 窗口毫秒); }

// —— 内存不足上报/查询/统计 ——
long long 错误聚合器::上报内存不足(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("内存不足", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询内存不足分组() const { return 通用查询分组("内存不足"); }
long long 错误聚合器::统计内存不足次数(long long 窗口毫秒) const { return 通用统计次数("内存不足", 窗口毫秒); }

// —— 磁盘不足上报/查询/统计 ——
long long 错误聚合器::上报磁盘不足(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("磁盘不足", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询磁盘不足分组() const { return 通用查询分组("磁盘不足"); }
long long 错误聚合器::统计磁盘不足次数(long long 窗口毫秒) const { return 通用统计次数("磁盘不足", 窗口毫秒); }

// —— 网络失败上报/查询/统计 ——
long long 错误聚合器::上报网络失败(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("网络失败", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询网络失败分组() const { return 通用查询分组("网络失败"); }
long long 错误聚合器::统计网络失败次数(long long 窗口毫秒) const { return 通用统计次数("网络失败", 窗口毫秒); }

// —— 权限拒绝上报/查询/统计 ——
long long 错误聚合器::上报权限拒绝(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("权限拒绝", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询权限拒绝分组() const { return 通用查询分组("权限拒绝"); }
long long 错误聚合器::统计权限拒绝次数(long long 窗口毫秒) const { return 通用统计次数("权限拒绝", 窗口毫秒); }

// —— 服务死亡上报/查询/统计 ——
long long 错误聚合器::上报服务死亡(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("服务死亡", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询服务死亡分组() const { return 通用查询分组("服务死亡"); }
long long 错误聚合器::统计服务死亡次数(long long 窗口毫秒) const { return 通用统计次数("服务死亡", 窗口毫秒); }

// —— 渲染超时上报/查询/统计 ——
long long 错误聚合器::上报渲染超时(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("渲染超时", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询渲染超时分组() const { return 通用查询分组("渲染超时"); }
long long 错误聚合器::统计渲染超时次数(long long 窗口毫秒) const { return 通用统计次数("渲染超时", 窗口毫秒); }

// —— 快照失败上报/查询/统计 ——
long long 错误聚合器::上报快照失败(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("快照失败", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询快照失败分组() const { return 通用查询分组("快照失败"); }
long long 错误聚合器::统计快照失败次数(long long 窗口毫秒) const { return 通用统计次数("快照失败", 窗口毫秒); }

// —— 启动失败上报/查询/统计 ——
long long 错误聚合器::上报启动失败(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("启动失败", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询启动失败分组() const { return 通用查询分组("启动失败"); }
long long 错误聚合器::统计启动失败次数(long long 窗口毫秒) const { return 通用统计次数("启动失败", 窗口毫秒); }

// —— 存储损坏上报/查询/统计 ——
long long 错误聚合器::上报存储损坏(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("存储损坏", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询存储损坏分组() const { return 通用查询分组("存储损坏"); }
long long 错误聚合器::统计存储损坏次数(long long 窗口毫秒) const { return 通用统计次数("存储损坏", 窗口毫秒); }

// —— 解码失败上报/查询/统计 ——
long long 错误聚合器::上报解码失败(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("解码失败", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询解码失败分组() const { return 通用查询分组("解码失败"); }
long long 错误聚合器::统计解码失败次数(long long 窗口毫秒) const { return 通用统计次数("解码失败", 窗口毫秒); }

// —— 输入丢失上报/查询/统计 ——
long long 错误聚合器::上报输入丢失(const std::string& 摘要,const std::string& 堆栈) { return 通用上报("输入丢失", 摘要, 堆栈); }
std::vector<错误分组> 错误聚合器::查询输入丢失分组() const { return 通用查询分组("输入丢失"); }
long long 错误聚合器::统计输入丢失次数(long long 窗口毫秒) const { return 通用统计次数("输入丢失", 窗口毫秒); }

std::vector<错误分组> 错误聚合器::查询全部分组() const { std::lock_guard<std::mutex> g(互斥锁_); std::vector<错误分组> r; for(auto& kv:分组_) r.push_back(kv.second); return r; }
std::vector<错误分组> 错误聚合器::查询热门分组(size_t n) const { auto v=查询全部分组(); if(n==0) return {}; if(n>1000) n=1000; if(v.size()>n){ std::nth_element(v.begin(),v.begin()+n,v.end(),[](auto&a,auto&b){return a.次数>b.次数;}); v.resize(n); std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.次数>b.次数;}); } else { std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.次数>b.次数;}); } return v; }
std::vector<错误记录> 错误聚合器::查询窗口(long long s,long long e,size_t n) const { if(n==0) return {}; if(n>5000) n=5000; std::lock_guard<std::mutex> g(互斥锁_); std::vector<错误记录> r; r.reserve(n < 记录_.size() ? n : 记录_.size()); for(auto& x:记录_) if(x.发生毫秒>=s&&x.发生毫秒<=e){ r.push_back(x); if(r.size()>=n) break; } return r; }
bool 错误聚合器::落盘(const std::string& p) const { if(p.empty()) return false; std::vector<错误记录> 快照; { std::lock_guard<std::mutex> g(互斥锁_); 快照.reserve(记录_.size()); for(auto& r:记录_) 快照.push_back(r); } FILE* f=fopen(p.c_str(),"w"); if(!f) return false; for(auto& r:快照) fprintf(f,"%s\n",r.转为日志行().c_str()); fclose(f); return true; }
bool 错误聚合器::从文件加载(const std::string& p){ if(p.empty()) return false; FILE* f=fopen(p.c_str(),"r"); if(!f) return false; char b[4096]; size_t 读数=0; while(fgets(b,sizeof(b),f)){ if(读数>=记录上限_硬) break; std::string s(b); if(s.size()>摘要上限_硬) s.resize(摘要上限_硬); if(!s.empty()&&s.back()=='\n') s.pop_back(); if(s.empty()) continue; std::lock_guard<std::mutex> g(互斥锁_); 错误记录 r; r.序号=++序号_; r.发生毫秒=当前毫秒错误(); r.摘要=s; r.类别="历史"; 记录_.push_back(std::move(r)); if(记录_.size()>记录上限_硬) 记录_.pop_front(); ++读数; } fclose(f); return true; }
void 错误聚合器::清空(){ std::lock_guard<std::mutex> g(互斥锁_); 记录_.clear(); 分组_.clear(); }
size_t 错误聚合器::记录总数() const { std::lock_guard<std::mutex> g(互斥锁_); return 记录_.size(); }
size_t 错误聚合器::分组总数() const { std::lock_guard<std::mutex> g(互斥锁_); return 分组_.size(); }
}  // namespace 诊断
}  // namespace vine
