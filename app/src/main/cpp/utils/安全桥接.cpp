// ==================================================================
// 模块：安全桥接（JNI）
// 说明：将原生哈希／沙箱／原子写能力暴露给 Kotlin 层，真实实现。
// 约定：注释与标识一律中文；标准库／系统调用／JNI 接口保持英文。
// 命名：JNI 静态注册名必须为 ASCII 转义形（非 ASCII 按 UTF-16 码元
//   输出 _0xxxx），否则运行时按转义名查找会找不到中文原名函数。
// 配套 Kotlin：com.hexadecinull.vineos.安全.原生安全桥接。
// ==================================================================
#include <jni.h>

#include <string>
#include <vector>

#include "哈希校验器.h"
#include "沙箱检查器.h"
#include "原子写入器.h"
#include "路径规范化器.h"

// 把 Java 字符串转为 C++ 字符串；失败返回空串。
// 参数：环境 —— JNI 环境；文本 —— Java 字符串。
// 返回：UTF-8 内容，空指针对应空串。
// 性能：按 UTF 字节长度一次性 reserve，避免逐字节重分配。
static std::string 取字符串(JNIEnv* 环境, jstring 文本) {
  if (!环境 || !文本) return std::string();
  jsize 字节数 = 环境->GetStringUTFLength(文本);
  const char* 原始 = 环境->GetStringUTFChars(文本, nullptr);
  if (!原始) return std::string();
  std::string 结果;
  结果.reserve(字节数 > 0 ? (size_t)字节数 : 0);
  结果.assign(原始);
  环境->ReleaseStringUTFChars(文本, 原始);
  return 结果;
}

// 把 C++ 字符串转为 Java 字符串。
static jstring 回字符串(JNIEnv* 环境, const std::string& 文本) {
  if (!环境) return nullptr;
  return 环境->NewStringUTF(文本.c_str());
}

// 原生计算文件 SHA-256，失败返回空串，Kotlin 层回退到纯实现。
extern "C" JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__0539f_0751f_08ba1_07b97_06587_04ef6_054c8_05e0c(JNIEnv* 环境, jclass, jstring 路径) {
  std::string 路径串 = 取字符串(环境, 路径);
  if (路径串.empty()) return 回字符串(环境, "");
  vine::安全::文件哈希结果 结果 = vine::安全::哈希校验器::计算文件沙256(路径串);
  if (!结果.成功) return 回字符串(环境, "");
  return 回字符串(环境, 结果.十六进制);
}

// 原生沙箱综合评分，返回 0-100，失败返回 -1。
extern "C" JNIEXPORT jint JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__0539f_0751f_06c99_07bb1_08bc4_05206(JNIEnv* 环境, jclass) {
  (void)环境;
  vine::安全::沙箱状态 状态 = vine::安全::沙箱检查器::综合检查();
  return (jint)vine::安全::沙箱检查器::评分(状态);
}

// 原生沙箱报告文本。
extern "C" JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__0539f_0751f_06c99_07bb1_062a5_0544a(JNIEnv* 环境, jclass) {
  vine::安全::沙箱状态 状态 = vine::安全::沙箱检查器::综合检查();
  return 回字符串(环境, vine::安全::沙箱检查器::报告文本(状态));
}

// 原生原子写入文本，成功返回写入字节数，失败返回 -1 并写出错误。
extern "C" JNIEXPORT jlong JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__0539f_0751f_0539f_05b50_05199_05165_06587_0672c(JNIEnv* 环境, jclass, jstring 路径, jstring 内容) {
  std::string 路径串 = 取字符串(环境, 路径);
  std::string 内容串 = 取字符串(环境, 内容);
  if (路径串.empty()) return (jlong)-1;
  vine::安全::写入配置 配置;
  配置.写后校验 = true;
  vine::安全::写入结果 结果 = vine::安全::原子写入器::写入文本(路径串, 内容串, 配置);
  if (!结果.成功) return (jlong)-1;
  return (jlong)结果.写入字节;
}

// 原生路径规范化，失败返回空串。
extern "C" JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__0539f_0751f_089c4_08303_05316_08def_05f84(JNIEnv* 环境, jclass, jstring 原始) {
  std::string 原始串 = 取字符串(环境, 原始);
  if (原始串.empty()) return 回字符串(环境, "");
  vine::安全::规范化选项 选项;
  vine::安全::规范化结果 结果 = vine::安全::路径规范化器::规范化(原始串, 选项);
  if (!结果.成功) return 回字符串(环境, "");
  return 回字符串(环境, 结果.规范路径);
}

// 原生 HMAC-SHA256 十六进制，失败返回空串。
extern "C" JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__0539f_0751f_08ba1_07b97HMAC(JNIEnv* 环境, jclass, jstring 密钥, jstring 消息) {
  std::string 密钥串 = 取字符串(环境, 密钥);
  std::string 消息串 = 取字符串(环境, 消息);
  if (密钥串.empty()) return 回字符串(环境, "");
  std::string 十六进制 = vine::安全::哈希校验器::计算字符串_hmac_十六进制(密钥串, 消息串);
  return 回字符串(环境, 十六进制);
}

// 原生版本标识，供 Kotlin 层确认桥接已加载。
extern "C" JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos__05b89_05168__0539f_0751f_05b89_05168_06865_063a5__06865_063a5_07248_0672c(JNIEnv* 环境, jclass) {
  return 回字符串(环境, "安全桥接 1.0");
}
