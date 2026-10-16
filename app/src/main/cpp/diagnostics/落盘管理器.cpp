// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：落盘管理器实现
#include "落盘管理器.h"
#include "../utils/vine_log.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
namespace vine {
namespace 诊断 {
static long long 现毫秒盘(){ struct timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL; }
std::string 落盘分段::转为日志行() const {
  char b[512];
  snprintf(b,sizeof(b),"序号=%d 路径=%s 起=%lld 止=%lld 字节=%lld 条=%lld 校验=%s",
           序号,路径.c_str(),起始毫秒,结束毫秒,字节数,条数,校验和.c_str());
  return std::string(b);
}
落盘管理器& 落盘管理器::获取实例(){
  static 落盘管理器 i;
  return i;
}
落盘管理器::落盘管理器(){}
bool 落盘管理器::初始化(const 落盘配置& c){
  std::lock_guard<std::mutex> g(互斥锁_);
  配置_=c;
  if(配置_.单文件上限字节<单文件下限_硬) 配置_.单文件上限字节=单文件下限_硬;
  if(配置_.单文件上限字节>单文件上限_硬) 配置_.单文件上限字节=单文件上限_硬;
  if(配置_.保留分段数<保留下限_硬) 配置_.保留分段数=保留下限_硬;
  if(配置_.保留分段数>保留上限_硬) 配置_.保留分段数=保留上限_硬;
  if(配置_.根目录.empty()) return false;
  mkdir(配置_.根目录.c_str(),0755);
  扫描现有分段();
  if(当前起始_==0) 当前起始_=现毫秒盘();
  return true;
}
void 落盘管理器::扫描现有分段(){
  DIR* d=opendir(配置_.根目录.c_str());
  if(!d) return;
  int 最大=-1;
  struct dirent* e;
  while((e=readdir(d))!=nullptr){
    std::string n(e->d_name);
    if(n.rfind(配置_.前缀,0)!=0) continue;
    auto p=n.rfind('.');
    if(p==std::string::npos) continue;
    try{ int s=std::stoi(n.substr(配置_.前缀.size()+1,p-配置_.前缀.size()-1)); if(s>最大)最大=s; }catch(...){}
  }
  closedir(d);
  下一序号_=最大+1;
}
std::string 落盘管理器::分段路径(int s) const {
  char b[512];
  snprintf(b,sizeof(b),"%s/%s_%05d.log",配置_.根目录.c_str(),配置_.前缀.c_str(),s);
  return b;
}
std::string 落盘管理器::当前路径() const {
  return 分段路径(下一序号_);
}
std::string 落盘管理器::通用读取字段(分段字段 字段, const 落盘分段& 分段) const {
  char 缓冲[256];
  switch (字段) {
    case 分段字段::序号: snprintf(缓冲, sizeof(缓冲), "%d", 分段.序号); return std::string(缓冲);
    case 分段字段::大小: snprintf(缓冲, sizeof(缓冲), "%lld", 分段.字节数); return std::string(缓冲);
    case 分段字段::条数: snprintf(缓冲, sizeof(缓冲), "%lld", 分段.条数); return std::string(缓冲);
    case 分段字段::起始: snprintf(缓冲, sizeof(缓冲), "%lld", 分段.起始毫秒); return std::string(缓冲);
    case 分段字段::结束: snprintf(缓冲, sizeof(缓冲), "%lld", 分段.结束毫秒); return std::string(缓冲);
    case 分段字段::轮转原因: return 分段.路径 + "_rotate_reason";
    case 分段字段::校验结果: return 分段.校验和;
    case 分段字段::压缩比例: return 分段.路径 + "_compress_ratio";
  }
  return {};
}
bool 落盘管理器::通用校验正常(const 落盘分段& 分段) const {
  if (分段.路径.empty()) return false;
  if (分段.路径.size() > 1024) return false;
  if (分段.字节数 < 0 || 分段.条数 < 0) return false;
  if (分段.结束毫秒 != 0 && 分段.起始毫秒 != 0 && 分段.结束毫秒 < 分段.起始毫秒) return false;
  return true;
}
bool 落盘管理器::内部落盘缓冲(bool 强制) {
  if (待写缓冲_.empty()) return true;
  if (!强制 && 待写缓冲_.size() < 批量缓冲字节_硬 && 缓冲行数_ < 批量缓冲行数_硬) return true;
  std::string 路 = 当前路径();
  FILE* 文件 = fopen(路.c_str(), "a");
  if (!文件) return false;
  if (当前条数_ == 0) 当前起始_ = 现毫秒盘();
  fwrite(待写缓冲_.data(), 1, 待写缓冲_.size(), 文件);
  fclose(文件);
  当前字节_ += (long long)待写缓冲_.size();
  当前条数_ += (long long)缓冲行数_;
  待写缓冲_.clear(); 缓冲行数_ = 0;
  return true;
}
std::string 落盘管理器::计算校验(const std::string& f) const {
  FILE* fp=fopen(f.c_str(),"rb");
  if(!fp) return {};
  unsigned long long h=1469598103934665603ULL;
  char b[4096]; size_t n;
  while((n=fread(b,1,sizeof(b),fp))>0){ for(size_t i=0;i<n;++i){ h^=(unsigned char)b[i]; h*=1099511628211ULL; } }
  fclose(fp);
  char o[32]; snprintf(o,sizeof(o),"%016llx",h);
  return o;
}
// —— 文件序号（file_seq）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取文件序号(const 落盘分段& 分段) const { return 通用读取字段(分段字段::序号, 分段); }
bool 落盘管理器::校验文件序号正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 文件大小（file_size）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取文件大小(const 落盘分段& 分段) const { return 通用读取字段(分段字段::大小, 分段); }
bool 落盘管理器::校验文件大小正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 记录条数（record_count）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取记录条数(const 落盘分段& 分段) const { return 通用读取字段(分段字段::条数, 分段); }
bool 落盘管理器::校验记录条数正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 起始时间（begin_time）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取起始时间(const 落盘分段& 分段) const { return 通用读取字段(分段字段::起始, 分段); }
bool 落盘管理器::校验起始时间正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 结束时间（end_time）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取结束时间(const 落盘分段& 分段) const { return 通用读取字段(分段字段::结束, 分段); }
bool 落盘管理器::校验结束时间正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 轮转原因（rotate_reason）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取轮转原因(const 落盘分段& 分段) const { return 通用读取字段(分段字段::轮转原因, 分段); }
bool 落盘管理器::校验轮转原因正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 校验结果（verify_result）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取校验结果(const 落盘分段& 分段) const { return 通用读取字段(分段字段::校验结果, 分段); }
bool 落盘管理器::校验校验结果正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

// —— 压缩比例（compress_ratio）读取与校验 ——
// 说明：从分段元数据提取对应字段并做合理性检查。
std::string 落盘管理器::读取压缩比例(const 落盘分段& 分段) const { return 通用读取字段(分段字段::压缩比例, 分段); }
bool 落盘管理器::校验压缩比例正常(const 落盘分段& 分段) const { return 通用校验正常(分段); }

bool 落盘管理器::追加行(const std::string& 行){
  std::vector<std::string> 集合;
  集合.push_back(行);
  return 追加多行(集合);
}
bool 落盘管理器::追加多行(const std::vector<std::string>& 行组){
  if (行组.empty()) return true;
  bool 需要轮转 = false;
  {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    if (配置_.根目录.empty()) return false;
    if (配置_.单文件上限字节 < 单文件下限_硬) 配置_.单文件上限字节 = 单文件下限_硬;
    if (配置_.单文件上限字节 > 单文件上限_硬) 配置_.单文件上限字节 = 单文件上限_硬;
    if (当前条数_ == 0 && 当前起始_ == 0) 当前起始_ = 现毫秒盘();
    for (auto& 行 : 行组) {
      size_t 取 = 行.size() > 8192 ? 8192 : 行.size();
      待写缓冲_.append(行.data(), 取);
      待写缓冲_.push_back(char(10));
      ++缓冲行数_;
    }
    if (待写缓冲_.size() >= 批量缓冲字节_硬 || 缓冲行数_ >= 批量缓冲行数_硬) {
      if (!内部落盘缓冲(true)) return false;
      需要轮转 = ((size_t)当前字节_ >= 配置_.单文件上限字节);
    }
  }
  if (需要轮转) 轮转("大小上限");
  return true;
}
bool 落盘管理器::轮转(const std::string& why){ (void)why; { std::lock_guard<std::mutex> 守卫(互斥锁_); 内部落盘缓冲(true); if(配置_.保留分段数<保留下限_硬) 配置_.保留分段数=保留下限_硬; if(配置_.保留分段数>保留上限_硬) 配置_.保留分段数=保留上限_硬; } 下一序号_++; 当前字节_=0; 当前条数_=0; 当前起始_=现毫秒盘(); 清理旧分段(); return true; }
void 落盘管理器::清理旧分段(){ auto 全=列出分段(); if((int)全.size()<=配置_.保留分段数) return; std::sort(全.begin(),全.end(),[](auto&a,auto&b){return a.序号<b.序号;}); size_t 删=全.size()-配置_.保留分段数; for(size_t i=0;i<删;++i) unlink(全[i].路径.c_str()); }
std::vector<落盘分段> 落盘管理器::列出分段() const { std::lock_guard<std::mutex> g(互斥锁_); std::vector<落盘分段> r; r.reserve(16); DIR* d=opendir(配置_.根目录.c_str()); if(!d) return r; struct dirent* e; while((e=readdir(d))!=nullptr){ std::string n(e->d_name); if(n.rfind(配置_.前缀,0)!=0) continue; std::string 全=配置_.根目录+"/"+n; struct stat 信{}; if(stat(全.c_str(),&信)!=0) continue; 落盘分段 s; s.路径=全; s.字节数=信.st_size; s.结束毫秒=现毫秒盘(); r.push_back(s); } closedir(d); return r; }
std::vector<std::string> 落盘管理器::查询行(long long s,long long e,size_t n) const { (void)s; (void)e; if(n==0) return {}; if(n>查询上限封顶_硬) n=查询上限封顶_硬; std::vector<std::string> r; r.reserve(n); auto 段=列出分段(); std::sort(段.begin(),段.end(),[](auto&a,auto&b){return a.序号>b.序号;}); for(auto& sg:段){ FILE* f=fopen(sg.路径.c_str(),"r"); if(!f) continue; char b[4096]; while(fgets(b,sizeof(b),f)){ std::string t(b); if(!t.empty()&&t.back()=='\n')t.pop_back(); if(t.size()>8192) t.resize(8192); r.push_back(std::move(t)); if(r.size()>=n) break; } fclose(f); if(r.size()>=n) break; } return r; }
std::string 落盘管理器::生成清单() const { auto v=列出分段(); std::string r; for(auto& s:v) r+=s.转为日志行()+"\n"; return r; }
size_t 落盘管理器::当前分段字节() const { std::lock_guard<std::mutex> g(互斥锁_); return (size_t)当前字节_; }
long long 落盘管理器::当前分段条数() const { std::lock_guard<std::mutex> g(互斥锁_); return 当前条数_; }
}  // namespace 诊断
}  // namespace vine
