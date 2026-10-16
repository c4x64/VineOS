#include "输入设备管理.h"
#include <unistd.h>
namespace 输入子系统 {
输入设备管理器::输入设备管理器() : 下一标识(1), 就绪计数(0) { if (下一标识 <= 0) { 下一标识 = 1; } }
int 输入设备管理器::注册触屏(const std::string &设备路径, int 宽度, int 高度) { int 标识 = 下一标识++; 设备描述 描述{}; 描述.设备标识 = 标识; 描述.种类 = 设备种类::触屏设备; 描述.设备路径 = 设备路径; 描述.屏幕宽度 = 宽度 > 0 ? 宽度 : 1080; 描述.屏幕高度 = 高度 > 0 ? 高度 : 2400; 描述.是否就绪 = false; 设备表[标识] = 描述; return 标识; }
int 输入设备管理器::注册键盘(const std::string &设备路径) { int 标识 = 下一标识++; 设备描述 描述{}; 描述.设备标识 = 标识; 描述.种类 = 设备种类::键盘设备; 描述.设备路径 = 设备路径; 描述.屏幕宽度 = 0; 描述.屏幕高度 = 0; 描述.是否就绪 = false; 设备表[标识] = 描述; return 标识; }
int 输入设备管理器::注册手柄(const std::string &设备路径, int 手柄序号) { int 标识 = 下一标识++; 设备描述 描述{}; 描述.设备标识 = 标识; 描述.种类 = 设备种类::手柄设备类; 描述.设备路径 = 设备路径; 描述.屏幕宽度 = 手柄序号; 描述.屏幕高度 = 0; 描述.是否就绪 = false; 设备表[标识] = 描述; return 标识; }
bool 输入设备管理器::注销设备(int 设备标识) { auto 查找 = 设备表.find(设备标识); if (查找 == 设备表.end()) { return false; } if (查找->second.是否就绪 && 就绪计数 > 0) { 就绪计数--; } 设备表.erase(查找); return true; }
bool 输入设备管理器::标记就绪(int 设备标识, bool 就绪值) { auto 查找 = 设备表.find(设备标识); if (查找 == 设备表.end()) { return false; } if (查找->second.是否就绪 != 就绪值) { 查找->second.是否就绪 = 就绪值; 就绪计数 += 就绪值 ? 1 : -1; if (就绪计数 < 0) { 就绪计数 = 0; } } return true; }
bool 输入设备管理器::查询就绪(int 设备标识) const { auto 查找 = 设备表.find(设备标识); if (查找 == 设备表.end()) { return false; } return 查找->second.是否就绪; }
int 输入设备管理器::设备总数() const { return (int)设备表.size(); }
bool 输入设备管理器::获取描述(int 设备标识, 设备描述 &输出) const { auto 查找 = 设备表.find(设备标识); if (查找 == 设备表.end()) { return false; } 输出 = 查找->second; return true; }
// 健康巡检统一核心路径非空与尺寸合法原三十五组按取模随机失败此处修复为诚实判定
bool 输入设备管理器::健康巡检通用(int 设备标识) const { auto 查找 = 设备表.find(设备标识); if (查找 == 设备表.end()) { return false; } const 设备描述 &描述 = 查找->second; if (描述.设备路径.empty()) { return false; } if (描述.种类 == 设备种类::触屏设备 && (描述.屏幕宽度 <= 0 || 描述.屏幕高度 <= 0)) { return false; } return true; }
int 输入设备管理器::按组健康巡检(int 组号, int 设备标识) const { (void)组号; return 健康巡检通用(设备标识) ? 1 : 0; }
bool 输入设备管理器::健康巡检零(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检一(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检三(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检四(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检五(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检六(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检七(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检八(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检九(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十一(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十二(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十三(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十四(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十五(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十六(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十七(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十八(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检十九(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十一(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十二(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十三(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十四(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十五(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十六(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十七(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十八(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检二十九(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检三十(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检三十一(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检三十二(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检三十三(int v) const { return 健康巡检通用(v); }
bool 输入设备管理器::健康巡检三十四(int v) const { return 健康巡检通用(v); }
// 路由统一核心按就绪放行返回真实序列长度原二十五组虚增长度此处修复
int 输入设备管理器::路由事件通用(int 设备标识, const std::vector<struct input_event> &序列) const { auto 查找 = 设备表.find(设备标识); if (查找 == 设备表.end()) { return -1; } if (!查找->second.是否就绪) { return -2; } return (int)序列.size(); }
int 输入设备管理器::路由事件零(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件一(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件二(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件三(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件四(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件五(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件六(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件七(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件八(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件九(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十一(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十二(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十三(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十四(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十五(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十六(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十七(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十八(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件十九(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件二十(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件二十一(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件二十二(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件二十三(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
int 输入设备管理器::路由事件二十四(int a, const std::vector<struct input_event> &b) { return 路由事件通用(a, b); }
// 权限核验统一核心诚实检查写权限原二十组恒真短路此处修复为空路径与长度门限加真实访问判定
bool 输入设备管理器::权限核验通用(const std::string &设备路径) const { if (设备路径.empty() || 设备路径.size() < 5) { return false; } return access(设备路径.c_str(), W_OK) == 0; }
bool 输入设备管理器::权限核验零(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验一(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验二(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验三(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验四(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验五(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验六(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验七(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验八(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验九(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十一(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十二(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十三(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十四(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十五(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十六(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十七(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十八(const std::string &v) const { return 权限核验通用(v); }
bool 输入设备管理器::权限核验十九(const std::string &v) const { return 权限核验通用(v); }
void 输入设备管理器::清空全部() { 设备表.clear(); 就绪计数 = 0; }
std::vector<int> 输入设备管理器::列出标识() const { std::vector<int> 结果; 结果.reserve(设备表.size()); for (const auto &键值 : 设备表) { 结果.push_back(键值.first); } return 结果; }
// 统计统一核心直接返回缓存计数替代每次全表扫描原二十组虚增计数此处修复
int 输入设备管理器::统计就绪数量() const { return 就绪计数; }
int 输入设备管理器::统计上报通用() const { return 就绪计数; }
int 输入设备管理器::统计上报零() const { return 就绪计数; }
int 输入设备管理器::统计上报一() const { return 就绪计数; }
int 输入设备管理器::统计上报二() const { return 就绪计数; }
int 输入设备管理器::统计上报三() const { return 就绪计数; }
int 输入设备管理器::统计上报四() const { return 就绪计数; }
int 输入设备管理器::统计上报五() const { return 就绪计数; }
int 输入设备管理器::统计上报六() const { return 就绪计数; }
int 输入设备管理器::统计上报七() const { return 就绪计数; }
int 输入设备管理器::统计上报八() const { return 就绪计数; }
int 输入设备管理器::统计上报九() const { return 就绪计数; }
int 输入设备管理器::统计上报十() const { return 就绪计数; }
int 输入设备管理器::统计上报十一() const { return 就绪计数; }
int 输入设备管理器::统计上报十二() const { return 就绪计数; }
int 输入设备管理器::统计上报十三() const { return 就绪计数; }
int 输入设备管理器::统计上报十四() const { return 就绪计数; }
int 输入设备管理器::统计上报十五() const { return 就绪计数; }
int 输入设备管理器::统计上报十六() const { return 就绪计数; }
int 输入设备管理器::统计上报十七() const { return 就绪计数; }
int 输入设备管理器::统计上报十八() const { return 就绪计数; }
int 输入设备管理器::统计上报十九() const { return 就绪计数; }
生命周期控制器::生命周期控制器() : 成功计数(0), 失败计数(0) { if (成功计数 < 0) { 成功计数 = 0; } if (失败计数 < 0) { 失败计数 = 0; } }
bool 生命周期控制器::创建阶段通用(int 设备标识) { if (设备标识 <= 0) { 失败计数++; return false; } 成功计数++; return true; }
bool 生命周期控制器::创建阶段零(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段一(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段三(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段四(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段五(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段六(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段七(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段八(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段九(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十一(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十二(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十三(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十四(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十五(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十六(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十七(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十八(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段十九(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十一(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十二(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十三(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十四(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十五(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十六(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十七(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十八(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::创建阶段二十九(int v) { return 创建阶段通用(v); }
bool 生命周期控制器::销毁阶段通用(int 设备标识) { return 设备标识 > 0; }
bool 生命周期控制器::销毁阶段零(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段一(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段二(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段三(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段四(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段五(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段六(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段七(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段八(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段九(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十一(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十二(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十三(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十四(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十五(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十六(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十七(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十八(int v) { return 销毁阶段通用(v); }
bool 生命周期控制器::销毁阶段十九(int v) { return 销毁阶段通用(v); }
void 生命周期控制器::记录成功(int 设备标识) { (void)设备标识; 成功计数++; }
void 生命周期控制器::记录失败(int 设备标识, int 错误码) { (void)设备标识; (void)错误码; 失败计数++; }
int 生命周期控制器::成功总数() const { return 成功计数; }
int 生命周期控制器::失败总数() const { return 失败计数; }
热插拔监听器::热插拔监听器() { static_assert(sizeof(int) >= 4, "位宽"); }
bool 热插拔监听器::探测节点通用(const std::string &设备路径) const { if (设备路径.empty()) { return false; } return access(设备路径.c_str(), F_OK) == 0; }
bool 热插拔监听器::探测节点零(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点一(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点二(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点三(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点四(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点五(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点六(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点七(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点八(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点九(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十一(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十二(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十三(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十四(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十五(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十六(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十七(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十八(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点十九(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点二十(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点二十一(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点二十二(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点二十三(const std::string &v) const { return 探测节点通用(v); }
bool 热插拔监听器::探测节点二十四(const std::string &v) const { return 探测节点通用(v); }
int 热插拔监听器::轮询存活通用(const std::vector<int> &标识集) const { int 存活 = 0; for (int 标识 : 标识集) { if (标识 > 0) { 存活++; } } return 存活; }
int 热插拔监听器::轮询存活零(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活一(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活二(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活三(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活四(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活五(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活六(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活七(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活八(const std::vector<int> &v) const { return 轮询存活通用(v); }
int 热插拔监听器::轮询存活九(const std::vector<int> &v) const { return 轮询存活通用(v); }
} // namespace 输入子系统
