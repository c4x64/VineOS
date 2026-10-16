// 流量探针实现。
#include "流量探针.h"
#include <sstream>
#include <thread>
#include <chrono>
namespace vine { namespace 网络 {
流量探针::流量探针(int 历史上限): 历史上限(历史上限) {}
std::vector<流量快照> 流量探针::采样一次() { auto 列表 = 读取系统流量(); std::lock_guard<std::mutex> 保护(互斥锁); for (auto& 项 : 列表) { 历史记录.push_back(项); while ((int)历史记录.size() > 历史上限) 历史记录.pop_front(); } return 列表; }
速率结果 流量探针::计算速率(const std::string& 网卡, int 间隔毫秒) { 速率结果 结果; 结果.网卡名 = 网卡; auto 第一次 = 读取系统流量(); std::this_thread::sleep_for(std::chrono::milliseconds(间隔毫秒)); auto 第二次 = 读取系统流量(); const 流量快照* 前=nullptr; const 流量快照* 后=nullptr; for (auto& 项 : 第一次) if (项.网卡名 == 网卡) 前 = &项; for (auto& 项 : 第二次) if (项.网卡名 == 网卡) 后 = &项; if (!前 || !后) return 结果; double 秒 = 间隔毫秒 / 1000.0; if (秒 <= 0) 秒 = 1; 结果.下行速率 = (后->收字节 - 前->收字节) / 秒; 结果.上行速率 = (后->发字节 - 前->发字节) / 秒; 结果.总字节 = 后->收字节 + 后->发字节; return 结果; }
std::vector<流量快照> 流量探针::查询历史() const { std::lock_guard<std::mutex> 保护(互斥锁); return std::vector<流量快照>(历史记录.begin(), 历史记录.end()); }
std::string 流量探针::导出摘要() const { std::lock_guard<std::mutex> 保护(互斥锁); std::ostringstream 输出; 输出 << "历史条数:" << 历史记录.size(); for (auto& 项 : 历史记录) 输出 << "\n" << 项.网卡名 << " 收" << 项.收字节 << " 发" << 项.发字节; return 输出.str(); }
std::string 流量探针::快捷摘要01() { auto 列表 = 采样一次(); if (列表.empty()) return "空01"; std::ostringstream 输出; 输出 << "01:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要02() { auto 列表 = 采样一次(); if (列表.empty()) return "空02"; std::ostringstream 输出; 输出 << "02:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要03() { auto 列表 = 采样一次(); if (列表.empty()) return "空03"; std::ostringstream 输出; 输出 << "03:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要04() { auto 列表 = 采样一次(); if (列表.empty()) return "空04"; std::ostringstream 输出; 输出 << "04:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要05() { auto 列表 = 采样一次(); if (列表.empty()) return "空05"; std::ostringstream 输出; 输出 << "05:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要06() { auto 列表 = 采样一次(); if (列表.empty()) return "空06"; std::ostringstream 输出; 输出 << "06:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要07() { auto 列表 = 采样一次(); if (列表.empty()) return "空07"; std::ostringstream 输出; 输出 << "07:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要08() { auto 列表 = 采样一次(); if (列表.empty()) return "空08"; std::ostringstream 输出; 输出 << "08:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要09() { auto 列表 = 采样一次(); if (列表.empty()) return "空09"; std::ostringstream 输出; 输出 << "09:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要10() { auto 列表 = 采样一次(); if (列表.empty()) return "空10"; std::ostringstream 输出; 输出 << "10:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要11() { auto 列表 = 采样一次(); if (列表.empty()) return "空11"; std::ostringstream 输出; 输出 << "11:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要12() { auto 列表 = 采样一次(); if (列表.empty()) return "空12"; std::ostringstream 输出; 输出 << "12:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要13() { auto 列表 = 采样一次(); if (列表.empty()) return "空13"; std::ostringstream 输出; 输出 << "13:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要14() { auto 列表 = 采样一次(); if (列表.empty()) return "空14"; std::ostringstream 输出; 输出 << "14:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要15() { auto 列表 = 采样一次(); if (列表.empty()) return "空15"; std::ostringstream 输出; 输出 << "15:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要16() { auto 列表 = 采样一次(); if (列表.empty()) return "空16"; std::ostringstream 输出; 输出 << "16:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要17() { auto 列表 = 采样一次(); if (列表.empty()) return "空17"; std::ostringstream 输出; 输出 << "17:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要18() { auto 列表 = 采样一次(); if (列表.empty()) return "空18"; std::ostringstream 输出; 输出 << "18:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要19() { auto 列表 = 采样一次(); if (列表.empty()) return "空19"; std::ostringstream 输出; 输出 << "19:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
std::string 流量探针::快捷摘要20() { auto 列表 = 采样一次(); if (列表.empty()) return "空20"; std::ostringstream 输出; 输出 << "20:" << 列表.size() << ":" << 列表[0].网卡名; return 输出.str(); }
} }
