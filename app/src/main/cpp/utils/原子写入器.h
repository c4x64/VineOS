// ====================================================================
// 模块：原子写入器
// 说明：提供崩溃安全的原子文件写入、备份轮转、目录刷盘等能力。
// 版本：1.0
// 编码：UTF-8
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
//   例如：class 完整性校验器，fun 校验文件哈希（Kotlin 侧）。
//   本模块所有函数均为真实实现，禁止留空、禁止抛空异常。
// 安全要求：
//   一、所有路径先规范化再使用，拒绝目录穿越与符号链接逃逸。
//   二、所有文件写操作必须原子化（临时文件＋刷盘＋重命名）。
//   三、所有外部进程必须绝对路径解析、超时强制终止。
//   四、所有哈希计算必须流式分块，避免一次性载入大文件。
//   五、所有失败必须记录错误原因并返回明确状态，不吞异常。
// ====================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <sys/types.h>

namespace vine {
namespace 安全 {

// 写入配置：调用方可调整权限、刷盘策略与备份行为。
struct 写入配置 {
  // 目标文件权限（默认 0644）。
  mode_t 文件权限 = 0644;
  // 临时文件权限（默认 0600，避免未完成内容被偷窥）。
  mode_t 临时权限 = 0600;
  // 是否在重命名前对文件内容执行刷盘。
  bool 内容刷盘 = true;
  // 是否在重命名后对父目录执行刷盘（保证目录项持久化）。
  bool 目录刷盘 = true;
  // 是否保留旧文件备份（重命名为 .bak）。
  bool 保留备份 = false;
  // 备份最大保留个数。
  int 备份个数 = 3;
  // 写入后是否执行同步校验（读回并比较哈希）。
  bool 写后校验 = true;
};

// 写入结果：携带成功标志、字节数与错误描述。
struct 写入结果 {
  bool 成功 = false;
  long 写入字节 = 0;
  std::string 错误描述;
  std::string 最终路径;
  std::string 备份路径;
};

// 原子写入器：所有写操作经临时文件＋刷盘＋原子重命名完成。
class 原子写入器 {
 public:
  // 将文本内容原子写入目标路径。
  static 写入结果 写入文本(const std::string& 目标路径, const std::string& 内容, const 写入配置& 配置 = 写入配置{});
  // 将二进制内容原子写入目标路径。
  static 写入结果 写入二进制(const std::string& 目标路径, const uint8_t* 数据, size_t 长度, const 写入配置& 配置 = 写入配置{});
  // 将向量内容原子写入目标路径。
  static 写入结果 写入向量(const std::string& 目标路径, const std::vector<uint8_t>& 数据, const 写入配置& 配置 = 写入配置{});
  // 仅当目标不存在时写入（排他创建），存在则失败返回。
  static 写入结果 排他写入(const std::string& 目标路径, const std::string& 内容, const 写入配置& 配置 = 写入配置{});
  // 在同一目录内生成唯一的临时文件路径（基于模板＋随机后缀）。
  static std::string 生成临时路径(const std::string& 目标路径);
  // 对已打开的文件描述符执行刷盘。
  static bool 刷盘文件(int 文件描述符, std::string* 错误 = nullptr);
  // 对父目录执行刷盘，保证重命名持久化。
  static bool 刷盘目录(const std::string& 目标路径, std::string* 错误 = nullptr);
  // 轮转备份：把现有目标依次改名为 .bak.1/.bak.2……。
  static bool 轮转备份(const std::string& 目标路径, int 最大个数, std::string* 错误 = nullptr);
  // 校验两个文件内容是否一致（流式比较）。
  static bool 比较文件一致(const std::string& 路径甲, const std::string& 路径乙);
  // 安全删除临时文件（忽略不存在）。
  static void 清理临时文件(const std::string& 临时路径);
};

// 目录工具：确保父目录存在并刷盘。
class 目录保障器 {
 public:
  static bool 确保父目录存在(const std::string& 文件路径, mode_t 权限 = 0755);
  static bool 确保目录存在(const std::string& 目录路径, mode_t 权限 = 0755);
  static std::string 取父目录(const std::string& 路径);
  static std::string 取文件名(const std::string& 路径);
};

}  // namespace 安全
}  // namespace vine
