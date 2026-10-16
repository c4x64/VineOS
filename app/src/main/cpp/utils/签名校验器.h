// ==================================================================
// 模块：签名校验器
// 说明：SHA256withRSA 自包含验证、PEM 解析、APK 块解析。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// ====================================================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
namespace vine { namespace 安全 {
struct 公钥 {
  std::vector<uint8_t> 模数;
  std::vector<uint8_t> 指数;
  bool 有效 = false;
};
struct 签名结果 {
  bool 通过 = false;
  std::string 摘要十六进制;
  std::string 错误描述;
  std::string 算法 = "SHA256withRSA";
};
class 大数工具 {
 public:
  static int 比较(const std::vector<uint8_t>& 甲, const std::vector<uint8_t>& 乙);
  static std::vector<uint8_t> 去前导零(const std::vector<uint8_t>& 值);
  static bool 为零(const std::vector<uint8_t>& 值);
  static std::vector<uint8_t> 左移一位(const std::vector<uint8_t>& 值);
  static std::vector<uint8_t> 减法(const std::vector<uint8_t>& 甲, const std::vector<uint8_t>& 乙);
  static std::vector<uint8_t> 取模(const std::vector<uint8_t>& 值, const std::vector<uint8_t>& 模数);
  static std::vector<uint8_t> 模乘(const std::vector<uint8_t>& 甲, const std::vector<uint8_t>& 乙, const std::vector<uint8_t>& 模数);
  static std::vector<uint8_t> 模幂(const std::vector<uint8_t>& 底, const std::vector<uint8_t>& 指数, const std::vector<uint8_t>& 模数);
  static size_t 位长度(const std::vector<uint8_t>& 值);
  static bool 测试位(const std::vector<uint8_t>& 值, size_t 位);
};
class 签名校验器 {
 public:
  static std::optional<std::vector<uint8_t>> 解码_base64(const std::string& 文本);
  static std::optional<公钥> 解析_pem_公钥(const std::string& 文本);
  static std::optional<std::vector<uint8_t>> 解析_der_长度(const uint8_t* 数据, size_t 长度, size_t& 偏移, size_t& 内容长);
  static bool 校验_pkcs1_填充(const std::vector<uint8_t>& 解密, const std::vector<uint8_t>& 摘要);
  static 签名结果 校验_沙256_rsa(const std::vector<uint8_t>& 消息, const std::vector<uint8_t>& 签名, const 公钥& 钥匙);
  static 签名结果 校验文件_沙256_rsa(const std::string& 文件路径, const std::vector<uint8_t>& 签名, const 公钥& 钥匙);
  static bool 校验摘要匹配(const std::string& 路径, const std::string& 期望十六进制);
  static std::string 取算法名();
  static bool 校验签名长度(const std::vector<uint8_t>& 签名, const 公钥& 钥匙);
  static bool 校验模数长度(const 公钥& 钥匙);
  static size_t 密钥位数(const 公钥& 钥匙);
  static bool 为常用指数(const 公钥& 钥匙);
  static std::string 公钥指纹(const 公钥& 钥匙);
  static bool 校验_pem_头尾(const std::string& 文本);
  static std::string 去_pem_空白(const std::string& 文本);
  static bool 校验_der_结构(const std::vector<uint8_t>& 数据);
  static int 比较签名(const std::vector<uint8_t>& 甲, const std::vector<uint8_t>& 乙);
  static bool 为空签名(const std::vector<uint8_t>& 签名);
  static double 估计校验耗时(size_t 密钥位);
  static bool 校验文件存在(const std::string& 路径);
};
} }
