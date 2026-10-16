// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：诊断桥接——统一 JNI 出口声明
#pragma once
#include <jni.h>
#include <string>
namespace vine { namespace 诊断 {
std::string 生成诊断摘要();
std::string 生成性能摘要();
std::string 生成内存摘要();
std::string 生成线程摘要();
std::string 生成错误摘要();
} }
