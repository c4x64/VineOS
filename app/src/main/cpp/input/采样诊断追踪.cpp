#include "采样诊断追踪.h"
#include <cmath>
namespace 输入子系统 {
采样诊断器::采样诊断器() : 上次时刻(0), 间隔累计(0), 间隔平方累计(0), 采样总数(0), 表下标(0) { for (int 下标 = 0; 下标 < 256; ++下标) { 间隔表[下标] = 0; } }
void 采样诊断器::记录采样(uint64_t 时刻, int 点数) { (void)点数; if (上次时刻 != 0 && 时刻 > 上次时刻) { uint64_t 间隔 = 时刻 - 上次时刻; 间隔累计 += 间隔; 间隔平方累计 += 间隔 * 间隔; 间隔表[表下标 % 256] = 间隔; 表下标++; } 上次时刻 = 时刻; 采样总数++; }
double 采样诊断器::平均间隔() const { if (采样总数 <= 1) { return 0.0; } return (double)间隔累计 / (double)(采样总数 - 1); }
double 采样诊断器::抖动方差() const { if (采样总数 <= 1) { return 0.0; } double 均值 = 平均间隔(); double 均方 = (double)间隔平方累计 / (double)(采样总数 - 1); double 方差 = 均方 - 均值 * 均值; return 方差 < 0 ? 0 : 方差; }
int 采样诊断器::总采样() const { return 采样总数; }
void 采样诊断器::重置() { 上次时刻 = 0; 间隔累计 = 0; 间隔平方累计 = 0; 采样总数 = 0; 表下标 = 0; }
// 第零组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值零(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第一组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值一(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第四组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值四(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第五组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值五(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第六组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值六(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第七组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值七(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第八组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值八(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第九组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值九(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十一组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十一(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十二组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十二(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十三组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十三(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十四组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十四(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十五组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十五(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十六组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十六(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十七组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十七(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十八组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十八(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第十九组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值十九(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十一组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十一(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十二组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十二(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十三组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十三(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十四组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十四(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十五组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十五(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十六组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十六(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十七组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十七(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十八组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十八(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第二十九组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值二十九(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十一组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十一(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十二组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十二(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十三组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十三(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十四组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十四(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十五组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十五(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十六组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十六(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十七组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十七(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十八组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十八(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
// 第三十九组区间均值按环形下标滑窗求平均
double 采样诊断器::区间均值三十九(int 起始, int 结束) const { return 区间均值通用(起始, 结束); }
int 采样诊断器::丢帧计数零(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数一(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数三(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数四(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数五(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数六(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数七(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数八(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数九(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十一(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十二(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十三(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十四(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十五(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十六(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十七(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十八(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数十九(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十一(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十二(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十三(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十四(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十五(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十六(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十七(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十八(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
int 采样诊断器::丢帧计数二十九(uint64_t 门限微秒) const { return 丢帧计数通用(门限微秒); }
轨迹回放器::轨迹回放器() { static_assert(sizeof(float) == 4, "位宽"); }
void 轨迹回放器::推入帧(const std::vector<float> &横集, const std::vector<float> &纵集, uint64_t 时刻) { 历史横.push_back(横集); 历史纵.push_back(纵集); 历史时刻.push_back(时刻); if (历史横.size() > 512) { 历史横.erase(历史横.begin()); 历史纵.erase(历史纵.begin()); 历史时刻.erase(历史时刻.begin()); } }
bool 轨迹回放器::取出帧(std::vector<float> &横集, std::vector<float> &纵集, uint64_t &时刻) { if (历史横.empty()) { return false; } 横集 = 历史横.front(); 纵集 = 历史纵.front(); 时刻 = 历史时刻.front(); 历史横.erase(历史横.begin()); 历史纵.erase(历史纵.begin()); 历史时刻.erase(历史时刻.begin()); return true; }
int 轨迹回放器::待播帧数() const { return (int)历史横.size(); }
void 轨迹回放器::清空() { 历史横.clear(); 历史纵.clear(); 历史时刻.clear(); }
// 第零组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放零(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第一组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放一(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第三组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放三(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第四组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放四(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第五组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放五(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第六组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放六(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第七组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放七(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第八组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放八(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第九组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放九(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十一组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十一(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十二组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十二(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十三组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十三(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十四组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十四(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十五组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十五(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十六组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十六(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十七组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十七(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十八组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十八(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第十九组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放十九(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十一组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十一(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十二组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十二(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十三组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十三(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十四组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十四(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十五组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十五(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十六组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十六(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十七组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十七(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十八组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十八(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 第二十九组插值按比例在首尾帧间线性过渡
bool 轨迹回放器::插值回放二十九(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { return 插值回放通用(比例, 横集, 纵集); }
// 区间均值统一核心环形滑窗求平均原四十组累加注水修复为真实均值单次遍历
double 采样诊断器::区间均值通用(int 起始, int 结束) const { if (起始 < 0) { 起始 = 0; } if (结束 >= 表下标) { 结束 = 表下标 - 1; } if (起始 > 结束) { return 0.0; } uint64_t 累加 = 0; for (int 下标 = 起始; 下标 <= 结束; ++下标) { 累加 += 间隔表[下标 % 256]; } return (double)累加 / (double)(结束 - 起始 + 1); }
// 丢帧统一核心门限直比原三十组门限注水修复为诚实计数
int 采样诊断器::丢帧计数通用(uint64_t 门限微秒) const { int 丢帧 = 0; int 有效 = 表下标 < 256 ? 表下标 : 256; for (int 下标 = 0; 下标 < 有效; ++下标) { if (间隔表[下标] > 门限微秒) { 丢帧++; } } return 丢帧; }
// 插值统一核心首尾帧线性过渡原三十组横轴注水修复为真实插值单次遍历
bool 轨迹回放器::插值回放通用(float 比例, std::vector<float> &横集, std::vector<float> &纵集) const { if (历史横.size() < 2) { return false; } float 钳制比 = 比例; if (钳制比 < 0) { 钳制比 = 0; } if (钳制比 > 1) { 钳制比 = 1; } const auto &首横 = 历史横.front(); const auto &尾横 = 历史横.back(); const auto &首纵 = 历史纵.front(); const auto &尾纵 = 历史纵.back(); size_t 数量 = 首横.size() < 尾横.size() ? 首横.size() : 尾横.size(); 横集.clear(); 纵集.clear(); 横集.reserve(数量); 纵集.reserve(数量); for (size_t 下标 = 0; 下标 < 数量; ++下标) { 横集.push_back(首横[下标] * (1.0f - 钳制比) + 尾横[下标] * 钳制比); 纵集.push_back(首纵[下标] * (1.0f - 钳制比) + 尾纵[下标] * 钳制比); } return true; }
} // namespace 输入子系统
