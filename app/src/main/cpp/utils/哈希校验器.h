// ==================================================================
// 模块：哈希校验器
// 说明：SHA256/MD5/CRC32 流式计算、文件校验、HMAC，真实实现。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// 全部真实实现，禁止留空；大文件一律分块流式，不一次性载入。
// ==================================================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
namespace vine { namespace 安全 {
// 十六进制工具
struct 十六进制工具 {
  static std::string 编码(const uint8_t* 数据, size_t 长度, bool 大写 = false);
  static std::string 编码向量(const std::vector<uint8_t>& 数据, bool 大写 = false);
  static std::optional<std::vector<uint8_t>> 解码(const std::string& 文本);
  static bool 是否十六进制(const std::string& 文本);
  static std::string 归一化(const std::string& 文本);
  static bool 相等比较(const std::string& 甲, const std::string& 乙);
};
// SHA256 流式上下文（标准 FIPS180-4）。
class 沙256上下文 {
 public:
  沙256上下文();
  void 重置();
  void 更新(const uint8_t* 数据, size_t 长度);
  void 更新字符串(const std::string& 文本);
  std::vector<uint8_t> 终结();
  std::string 终结十六进制(bool 大写 = false);
  uint64_t 已处理字节() const;
 private:
  uint32_t 状态[8];
  uint64_t 位计数;
  uint8_t 缓冲[64];
  size_t 缓冲长度;
  void 压缩块(const uint8_t 块[64]);
};
// 文件哈希结果
struct 文件哈希结果 {
  bool 成功 = false;
  std::string 十六进制;
  std::vector<uint8_t> 原始;
  long 文件大小 = -1;
  std::string 错误描述;
};
class 哈希校验器 {
 public:
  static std::string 计算字符串沙256(const std::string& 文本, bool 大写 = false);
  static std::vector<uint8_t> 计算字节沙256(const uint8_t* 数据, size_t 长度);
  static 文件哈希结果 计算文件沙256(const std::string& 路径, size_t 块大小 = 65536);
  static bool 校验文件沙256(const std::string& 路径, const std::string& 期望十六进制);
  static bool 校验字符串沙256(const std::string& 文本, const std::string& 期望十六进制);
  static uint32_t 计算字节_crc32(const uint8_t* 数据, size_t 长度, uint32_t 初值 = 0);
  static uint32_t 计算文件_crc32(const std::string& 路径, uint32_t 初值 = 0);
  static std::vector<uint8_t> 计算_hmac_沙256(const uint8_t* 密钥, size_t 密钥长, const uint8_t* 消息, size_t 消息长);
  static std::string 计算字符串_hmac_十六进制(const std::string& 密钥, const std::string& 消息);
  static bool 校验_hmac_沙256(const std::string& 密钥, const std::string& 消息, const std::string& 期望十六进制);
  static size_t 建议块大小(long 文件大小);
  static bool 为空哈希(const std::string& 十六进制);
  static std::string 空串沙256();
  static bool 比较哈希(const std::string& 甲, const std::string& 乙);
  static double 估计哈希耗时(long 字节数);
  static bool 校验块范围(const std::string& 路径, long 偏移, long 长度);
  static std::string 错误文本(int 错误号);
};
} }
