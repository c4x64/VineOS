// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：上报队列实现
#include "上报队列.h"
#include "../utils/vine_log.h"
#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
namespace vine {
namespace 诊断 {
static long long 现毫秒报(){ struct timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL; }
std::string 上报条目::转为日志行() const { char b[1024]; snprintf(b,sizeof(b),"序号=%lld 类别=%s 优先级=%d 重试=%d 负载=%zuB 目标=%s",序号,类别.c_str(),优先级,重试次数,负载.size(),目标地址.c_str()); return b; }
size_t 上报条目::负载字节() const { return 负载.size(); }
bool 上报条目::是否到期(long long 现) const { return 现>=下次重试毫秒; }
std::string 上报统计::生成摘要() const { char b[256]; snprintf(b,sizeof(b),"入队=%lld 成功=%lld 失败=%lld 丢弃=%lld 等待=%lld",入队总数,成功总数,失败总数,丢弃总数,当前等待); return b; }
上报队列& 上报队列::获取实例(){ static 上报队列 i; return i; }
上报队列::上报队列(){}
std::string 上报队列::截断字段(const std::string& 文本, size_t 上限) { if (文本.size() <= 上限) return 文本; return 文本.substr(0, 上限); }
std::string 上报队列::通用提取(报表字段 字段, const 上报条目& 条目) const {
  char 缓冲[256];
  switch (字段) {
    case 报表字段::标识: snprintf(缓冲, sizeof(缓冲), "%lld", 条目.序号); return std::string(缓冲);
    case 报表字段::发生时间: snprintf(缓冲, sizeof(缓冲), "%lld", 条目.入队毫秒); return std::string(缓冲);
    case 报表字段::上报时间: snprintf(缓冲, sizeof(缓冲), "%lld", 条目.入队毫秒); return std::string(缓冲);
    case 报表字段::重试次数: snprintf(缓冲, sizeof(缓冲), "%d", 条目.重试次数); return std::string(缓冲);
    case 报表字段::负载大小: snprintf(缓冲, sizeof(缓冲), "%zu", 条目.负载.size()); return std::string(缓冲);
    case 报表字段::优先级: snprintf(缓冲, sizeof(缓冲), "%d", 条目.优先级); return std::string(缓冲);
    case 报表字段::目标地址: return 条目.目标地址;
    case 报表字段::压缩标记: return 条目.类别 + "_compressed";
    case 报表字段::加密标记: return 条目.类别 + "_encrypted";
    case 报表字段::分片序号: return 条目.类别 + "_shard_seq";
    case 报表字段::分片总数: return 条目.类别 + "_shard_total";
    case 报表字段::校验和: return 条目.校验和;
  }
  return {};
}
bool 上报队列::通用校验(报表字段 字段, const std::string& 值) const {
  (void)字段;
  if (值.empty()) return false;
  if (值.size() > 字段值上限_硬) return false;
  return true;
}
// —— 上报标识（report_id）提取与校验 ——
std::string 上报队列::提取上报标识(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::标识, 条目); }
bool 上报队列::校验上报标识(const std::string& 值) const { return 通用校验(报表字段::标识, 值); }

// —— 发生时间（occur_time）提取与校验 ——
std::string 上报队列::提取发生时间(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::发生时间, 条目); }
bool 上报队列::校验发生时间(const std::string& 值) const { return 通用校验(报表字段::发生时间, 值); }

// —— 上报时间（upload_time）提取与校验 ——
std::string 上报队列::提取上报时间(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::上报时间, 条目); }
bool 上报队列::校验上报时间(const std::string& 值) const { return 通用校验(报表字段::上报时间, 值); }

// —— 重试次数（retry_count）提取与校验 ——
std::string 上报队列::提取重试次数(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::重试次数, 条目); }
bool 上报队列::校验重试次数(const std::string& 值) const { return 通用校验(报表字段::重试次数, 值); }

// —— 负载大小（payload_size）提取与校验 ——
std::string 上报队列::提取负载大小(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::负载大小, 条目); }
bool 上报队列::校验负载大小(const std::string& 值) const { return 通用校验(报表字段::负载大小, 值); }

// —— 优先级（priority）提取与校验 ——
std::string 上报队列::提取优先级(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::优先级, 条目); }
bool 上报队列::校验优先级(const std::string& 值) const { return 通用校验(报表字段::优先级, 值); }

// —— 目标地址（endpoint）提取与校验 ——
std::string 上报队列::提取目标地址(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::目标地址, 条目); }
bool 上报队列::校验目标地址(const std::string& 值) const { return 通用校验(报表字段::目标地址, 值); }

// —— 压缩标记（compressed）提取与校验 ——
std::string 上报队列::提取压缩标记(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::压缩标记, 条目); }
bool 上报队列::校验压缩标记(const std::string& 值) const { return 通用校验(报表字段::压缩标记, 值); }

// —— 加密标记（encrypted）提取与校验 ——
std::string 上报队列::提取加密标记(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::加密标记, 条目); }
bool 上报队列::校验加密标记(const std::string& 值) const { return 通用校验(报表字段::加密标记, 值); }

// —— 分片序号（shard_seq）提取与校验 ——
std::string 上报队列::提取分片序号(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::分片序号, 条目); }
bool 上报队列::校验分片序号(const std::string& 值) const { return 通用校验(报表字段::分片序号, 值); }

// —— 分片总数（shard_total）提取与校验 ——
std::string 上报队列::提取分片总数(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::分片总数, 条目); }
bool 上报队列::校验分片总数(const std::string& 值) const { return 通用校验(报表字段::分片总数, 值); }

// —— 校验和（checksum）提取与校验 ——
std::string 上报队列::提取校验和(const 上报条目& 条目) const { std::lock_guard<std::mutex> 守卫(互斥锁_); return 通用提取(报表字段::校验和, 条目); }
bool 上报队列::校验校验和(const std::string& 值) const { return 通用校验(报表字段::校验和, 值); }

bool 上报队列::初始化(const 队列配置& 配置){ std::lock_guard<std::mutex> g(互斥锁_); 配置_=配置; if(配置_.落盘目录.empty()) 配置_.落盘目录="/tmp"; mkdir(配置_.落盘目录.c_str(),0755); return true; }
long long 上报队列::入队(const std::string& 类别,const std::string& 负载,int 优先级,const std::string& 目标){ std::string 切类=截断字段(类别,类别上限_硬); std::string 切载=截断字段(负载,负载上限_硬); std::string 切目=截断字段(目标,目标上限_硬); if(切类.empty()) 切类="默认"; int 切优=优先级; if(切优<0) 切优=0; if(切优>10) 切优=10; std::lock_guard<std::mutex> g(互斥锁_); size_t 上限=配置_.内存上限; if(上限<16) 上限=16; if(上限>10000) 上限=10000; if(等待_.size()>=上限){ 等待_.pop_front(); 统计_.丢弃总数++; } 上报条目 e; e.序号=++序号_; e.入队毫秒=现毫秒报(); e.下次重试毫秒=e.入队毫秒; e.类别=切类; e.负载=切载; e.优先级=切优; e.目标地址=切目; e.校验和=计算校验(切载); 等待_.push_back(std::move(e)); 统计_.入队总数++; 统计_.当前等待=(long long)等待_.size(); return 序号_; }
std::vector<上报条目> 上报队列::取出待发批次(size_t 上限){ if(上限==0) return {}; if(上限>200) 上限=200; std::lock_guard<std::mutex> g(互斥锁_); long long 现=现毫秒报(); std::vector<const 上报条目*> 到期; 到期.reserve(等待_.size()); for(auto& e:等待_) if(e.是否到期(现)) 到期.push_back(&e); if(到期.empty()) return {}; auto 比较=[](const 上报条目* a,const 上报条目* b){ if(a->优先级!=b->优先级) return a->优先级>b->优先级; return a->序号<b->序号; }; std::vector<上报条目> r; r.reserve(上限 < 到期.size() ? 上限 : 到期.size()); if(到期.size()>上限*2){ std::nth_element(到期.begin(),到期.begin()+上限,到期.end(),比较); 到期.resize(上限); std::sort(到期.begin(),到期.end(),比较); } else { std::sort(到期.begin(),到期.end(),比较); } for(size_t i=0;i<到期.size()&&r.size()<上限;++i) r.push_back(*到期[i]); return r; }
void 上报队列::标记成功(long long s){ std::lock_guard<std::mutex> g(互斥锁_); for(auto it=等待_.begin();it!=等待_.end();++it) if(it->序号==s){ 等待_.erase(it); break; } 发送中_.erase(s); 统计_.成功总数++; 统计_.当前等待=(long long)等待_.size(); }
void 上报队列::标记失败(long long s,const std::string& why){ (void)why; std::lock_guard<std::mutex> g(互斥锁_); for(auto& e:等待_) if(e.序号==s){ e.重试次数++; if(e.重试次数>配置_.最大重试){ } else { e.下次重试毫秒=现毫秒报()+计算退避(e.重试次数); } 统计_.失败总数++; break; } }
void 上报队列::标记丢弃(long long s){ std::lock_guard<std::mutex> g(互斥锁_); for(auto it=等待_.begin();it!=等待_.end();++it) if(it->序号==s){ 等待_.erase(it); 统计_.丢弃总数++; break; } 统计_.当前等待=(long long)等待_.size(); }
上报统计 上报队列::查询统计() const { std::lock_guard<std::mutex> g(互斥锁_); return 统计_; }
long long 上报队列::计算退避(int r) const { long long d=配置_.基础退避毫秒; for(int i=1;i<r;++i){ d*=2; if(d>配置_.最大退避毫秒){ d=配置_.最大退避毫秒; break; } } long long 抖动=(d/8==0)?0:(rand()%(d/8)); return d+抖动; }
std::string 上报队列::计算校验(const std::string& s) const { unsigned long long h=1469598103934665603ULL; for(char ch:s){ h^=(unsigned char)ch; h*=1099511628211ULL; } char b[32]; snprintf(b,sizeof(b),"%016llx",h); return b; }
std::string 上报队列::队列文件() const { return 配置_.落盘目录+"/上报队列.dat"; }
bool 上报队列::落盘(){ std::lock_guard<std::mutex> g(互斥锁_); std::string 临=队列文件()+".tmp"; FILE* f=fopen(临.c_str(),"w"); if(!f) return false; for(auto& e:等待_) fprintf(f,"%lld\t%s\t%d\t%s\n",e.序号,e.类别.c_str(),e.优先级,e.负载.c_str()); fclose(f); rename(临.c_str(),队列文件().c_str()); return true; }
bool 上报队列::从落盘恢复(){ std::lock_guard<std::mutex> g(互斥锁_); FILE* f=fopen(队列文件().c_str(),"r"); if(!f) return false; char b[8192]; while(fgets(b,sizeof(b),f)){ 上报条目 e; char 类[128]={0},载[4096]={0}; int 优=0; long long 序=0; if(sscanf(b,"%lld\t%127[^\t]\t%d\t%4095[^\n]",&序,类,&优,载)==4){ e.序号=序; e.类别=类; e.优先级=优; e.负载=载; e.入队毫秒=现毫秒报(); e.下次重试毫秒=e.入队毫秒; if(序>序号_)序号_=序; 等待_.push_back(e); } } fclose(f); return true; }
void 上报队列::设置发送器(std::function<bool(const 上报条目&)> s){ std::lock_guard<std::mutex> g(互斥锁_); 发送器_=s; }
bool 上报队列::尝试发送一批(){ auto 批=取出待发批次(配置_.单批上限); if(批.empty()) return true; bool 全成=true; for(auto& e:批){ bool ok=false; { std::lock_guard<std::mutex> g(互斥锁_); if(发送器_) try{ ok=发送器_(e); }catch(...){ ok=false; } else { ok=true; } } if(ok) 标记成功(e.序号); else { 标记失败(e.序号,"发送失败"); 全成=false; } } 落盘(); return 全成; }
void 上报队列::清空(){ std::lock_guard<std::mutex> g(互斥锁_); 等待_.clear(); 发送中_.clear(); }
size_t 上报队列::等待数量() const { std::lock_guard<std::mutex> g(互斥锁_); return 等待_.size(); }
}  // namespace 诊断
}  // namespace vine
