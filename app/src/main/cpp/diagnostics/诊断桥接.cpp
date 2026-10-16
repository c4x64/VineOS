// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：诊断桥接实现——汇总各子系统并暴露 JNI
#include "诊断桥接.h"
#include "性能采样器.h"
#include "内存分析器.h"
#include "线程转储器.h"
#include "错误聚合器.h"
#include "上报队列.h"
#include "日志中心.h"
#include "落盘管理器.h"
#include "系统探针集.h"
#include "../utils/vine_log.h"
#include <jni.h>
namespace vine {
namespace 诊断 {
std::string 生成性能摘要(){
  auto 点 = 性能采样器::获取实例().最新采样点();
  if (点.序号 == 0) return "暂无性能采样";
  return 点.转为日志行();
}
std::string 生成内存摘要(){
  auto 快照 = 内存分析器::获取实例().采集内存快照();
  return 快照.转为日志行();
}
std::string 生成线程摘要(){
  auto 转储 = 线程转储器::获取实例().采集转储();
  return 转储.转为摘要();
}
std::string 生成错误摘要(){
  auto& 聚合 = 错误聚合器::获取实例();
  char b[128]; snprintf(b,sizeof(b),"记录=%zu 分组=%zu",聚合.记录总数(),聚合.分组总数());
  return b;
}
std::string 生成诊断摘要(){
  std::string r; r.reserve(4096);
  r += "[性能]" + 生成性能摘要() + "\n";
  r += "[内存]" + 生成内存摘要() + "\n";
  r += "[线程]" + 生成线程摘要() + "\n";
  r += "[错误]" + 生成错误摘要() + "\n";
  r += "[系统]" + 系统探针集::获取实例().采集系统快照().转为日志行();
  return r;
}
}  // namespace 诊断
}  // namespace vine
extern "C" {
JNIEXPORT jstring JNICALL Java_com_hexadecinull_vineos_诊断_诊断原生桥_原生获取诊断摘要(JNIEnv* e, jobject){ auto s = vine::诊断::生成诊断摘要(); return e->NewStringUTF(s.c_str()); }
JNIEXPORT jstring JNICALL Java_com_hexadecinull_vineos_诊断_诊断原生桥_原生获取性能摘要(JNIEnv* e, jobject){ auto s = vine::诊断::生成性能摘要(); return e->NewStringUTF(s.c_str()); }
JNIEXPORT jstring JNICALL Java_com_hexadecinull_vineos_诊断_诊断原生桥_原生获取内存摘要(JNIEnv* e, jobject){ auto s = vine::诊断::生成内存摘要(); return e->NewStringUTF(s.c_str()); }
JNIEXPORT jstring JNICALL Java_com_hexadecinull_vineos_诊断_诊断原生桥_原生获取线程摘要(JNIEnv* e, jobject){ auto s = vine::诊断::生成线程摘要(); return e->NewStringUTF(s.c_str()); }
JNIEXPORT jstring JNICALL Java_com_hexadecinull_vineos_诊断_诊断原生桥_原生获取错误摘要(JNIEnv* e, jobject){ auto s = vine::诊断::生成错误摘要(); return e->NewStringUTF(s.c_str()); }
}
