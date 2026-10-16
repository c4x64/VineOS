// ==================================================================
// 模块：路径规范化器
// 说明：词法规范化、符号链接消解、目录穿越防御、绝对路径解析。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// 全部函数为真实实现，禁止留空。
// ==================================================================
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sys/types.h>
namespace vine { namespace 安全 {
// 规范化选项
struct 规范化选项 {
  bool 消解符号链接 = true;
  bool 必须存在 = false;
  bool 拒绝空路径 = true;
  size_t 最大长度 = 4096;
  size_t 最大链接跳数 = 40;
};
// 规范化结果
struct 规范化结果 {
  bool 成功 = false;
  std::string 规范路径;
  std::string 错误描述;
  bool 曾含链接 = false;
  bool 曾含点段 = false;
};
class 路径规范化器 {
 public:
  static 规范化结果 规范化(const std::string& 原始路径, const 规范化选项& 选项 = 规范化选项{});
  static std::string 词法规范化(const std::string& 路径, bool* 含点段 = nullptr);
  static std::optional<std::string> 消解链接(const std::string& 路径, size_t 最大跳数 = 40);
  static bool 是否绝对路径(const std::string& 路径);
  static bool 是否含穿越段(const std::string& 路径);
  static bool 是否在基目录内(const std::string& 候选, const std::string& 基目录);
  static std::string 连接路径(const std::string& 基, const std::string& 相对);
  static std::string 取绝对路径(const std::string& 路径);
  static std::vector<std::string> 切分路径(const std::string& 路径);
  static std::string 合并路径(const std::vector<std::string>& 段, bool 绝对 = true);
  static bool 路径存在(const std::string& 路径);
  static bool 为目录(const std::string& 路径);
  static bool 为符号链接(const std::string& 路径);
  static std::optional<std::string> 读链接目标(const std::string& 路径);
  static bool 校验长度合法(const std::string& 路径, size_t 上限 = 4096);
  static bool 校验字符合法(const std::string& 路径);
  static bool 校验无空字节(const std::string& 路径);
  static std::string 去除尾斜杠(const std::string& 路径);
  static std::string 确保尾斜杠(const std::string& 路径);
  static std::string 取扩展名(const std::string& 路径);
  static std::string 去扩展名(const std::string& 路径);
  static std::string 取基名(const std::string& 路径);
  static std::string 取目录名(const std::string& 路径);
  static bool 同一文件(const std::string& 甲, const std::string& 乙);
  static int 比较路径(const std::string& 甲, const std::string& 乙);
  static bool 有前缀(const std::string& 路径, const std::string& 前缀);
  static bool 有后缀(const std::string& 路径, const std::string& 后缀);
  static std::string 相对化(const std::string& 全路径, const std::string& 基目录);
  static size_t 共同前缀长(const std::string& 甲, const std::string& 乙);
  static bool 为隐藏路径(const std::string& 路径);
  static bool 含通配符(const std::string& 路径);
  static std::string 转义空格(const std::string& 路径);
};
} }
