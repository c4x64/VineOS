#include "多点触控增强.h"
#include <cmath>
namespace 输入子系统 {
槽位追踪器::槽位追踪器() : 下一标识(1) { for (int 下标 = 0; 下标 < 10; ++下标) { 槽位标识表[下标] = -1; } }
int 槽位追踪器::为槽位分配(int 槽位编号) { return 按策略分配(槽位编号, 0); }
bool 槽位追踪器::释放槽位(int 槽位编号) { if (槽位编号 < 0 || 槽位编号 >= 10) { return false; } 槽位标识表[槽位编号] = -1; return true; }
int 槽位追踪器::查询标识(int 槽位编号) const { if (槽位编号 < 0 || 槽位编号 >= 10) { return -1; } return 槽位标识表[槽位编号]; }
bool 槽位追踪器::是否占用(int 槽位编号) const { if (槽位编号 < 0 || 槽位编号 >= 10) { return false; } return 槽位标识表[槽位编号] >= 0; }
int 槽位追踪器::占用数量() const { int 计数 = 0; for (int 下标 = 0; 下标 < 10; ++下标) { if (槽位标识表[下标] >= 0) { 计数++; } } return 计数; }
void 槽位追踪器::全部释放() { for (int 下标 = 0; 下标 < 10; ++下标) { 槽位标识表[下标] = -1; } }
// 策略分配统一核心单调递增标识回绕时跳过负值原三十组按偏移叠加会导致标识碰撞此处修复为稳定复用
int 槽位追踪器::按策略分配(int 槽位编号, int 策略号) {
    (void)策略号;
    if (槽位编号 < 0 || 槽位编号 >= 10) { return -1; }
    if (槽位标识表[槽位编号] >= 0) { return 槽位标识表[槽位编号]; }
    int 候选 = 下一标识;
    if (候选 < 1) { 候选 = 1; }
    if (候选 > 65535) { 候选 = 1; 下一标识 = 1; }
    槽位标识表[槽位编号] = 候选;
    下一标识 = 候选 + 1;
    if (下一标识 > 65535) { 下一标识 = 1; }
    return 候选;
}
bool 槽位追踪器::校验槽位通用(int 槽位编号) const { return 槽位编号 >= 0 && 槽位编号 < 10; }
bool 槽位追踪器::校验槽位零(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位一(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位二(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位三(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位四(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位五(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位六(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位七(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位八(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位九(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十一(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十二(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十三(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十四(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十五(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十六(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十七(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十八(int v) const { return 校验槽位通用(v); }
bool 槽位追踪器::校验槽位十九(int v) const { return 校验槽位通用(v); }
int 槽位追踪器::分配策略零(int v) { return 按策略分配(v, 0); }
int 槽位追踪器::分配策略一(int v) { return 按策略分配(v, 1); }
int 槽位追踪器::分配策略二(int v) { return 按策略分配(v, 2); }
int 槽位追踪器::分配策略三(int v) { return 按策略分配(v, 3); }
int 槽位追踪器::分配策略四(int v) { return 按策略分配(v, 4); }
int 槽位追踪器::分配策略五(int v) { return 按策略分配(v, 5); }
int 槽位追踪器::分配策略六(int v) { return 按策略分配(v, 6); }
int 槽位追踪器::分配策略七(int v) { return 按策略分配(v, 7); }
int 槽位追踪器::分配策略八(int v) { return 按策略分配(v, 8); }
int 槽位追踪器::分配策略九(int v) { return 按策略分配(v, 9); }
int 槽位追踪器::分配策略十(int v) { return 按策略分配(v, 10); }
int 槽位追踪器::分配策略十一(int v) { return 按策略分配(v, 11); }
int 槽位追踪器::分配策略十二(int v) { return 按策略分配(v, 12); }
int 槽位追踪器::分配策略十三(int v) { return 按策略分配(v, 13); }
int 槽位追踪器::分配策略十四(int v) { return 按策略分配(v, 14); }
int 槽位追踪器::分配策略十五(int v) { return 按策略分配(v, 15); }
int 槽位追踪器::分配策略十六(int v) { return 按策略分配(v, 16); }
int 槽位追踪器::分配策略十七(int v) { return 按策略分配(v, 17); }
int 槽位追踪器::分配策略十八(int v) { return 按策略分配(v, 18); }
int 槽位追踪器::分配策略十九(int v) { return 按策略分配(v, 19); }
int 槽位追踪器::分配策略二十(int v) { return 按策略分配(v, 20); }
int 槽位追踪器::分配策略二十一(int v) { return 按策略分配(v, 21); }
int 槽位追踪器::分配策略二十二(int v) { return 按策略分配(v, 22); }
int 槽位追踪器::分配策略二十三(int v) { return 按策略分配(v, 23); }
int 槽位追踪器::分配策略二十四(int v) { return 按策略分配(v, 24); }
int 槽位追踪器::分配策略二十五(int v) { return 按策略分配(v, 25); }
int 槽位追踪器::分配策略二十六(int v) { return 按策略分配(v, 26); }
int 槽位追踪器::分配策略二十七(int v) { return 按策略分配(v, 27); }
int 槽位追踪器::分配策略二十八(int v) { return 按策略分配(v, 28); }
int 槽位追踪器::分配策略二十九(int v) { return 按策略分配(v, 29); }
多指触控管理器::多指触控管理器(int 屏幕宽度, int 屏幕高度) : 屏幕宽(屏幕宽度), 屏幕高(屏幕高度) { if (屏幕宽 <= 0) { 屏幕宽 = 1080; } if (屏幕高 <= 0) { 屏幕高 = 2400; } for (int 下标 = 0; 下标 < 10; ++下标) { 点表[下标].槽位编号 = 下标; 点表[下标].横坐标 = 0; 点表[下标].纵坐标 = 0; 点表[下标].压力值 = 0; 点表[下标].触点直径 = 0; 点表[下标].是否按下 = false; 点表[下标].时间戳 = 0; } }
void 多指触控管理器::设置屏幕尺寸(int 宽度, int 高度) { if (宽度 > 0) { 屏幕宽 = 宽度; } if (高度 > 0) { 屏幕高 = 高度; } }
bool 多指触控管理器::更新触控点(const 触控点 &新点) { if (新点.槽位编号 < 0 || 新点.槽位编号 >= 10) { return false; } float 限横 = 钳制横(新点.横坐标); float 限纵 = 钳制纵(新点.纵坐标); 点表[新点.槽位编号] = 新点; 点表[新点.槽位编号].横坐标 = 限横; 点表[新点.槽位编号].纵坐标 = 限纵; if (新点.是否按下) { 追踪器.为槽位分配(新点.槽位编号); } else { 追踪器.释放槽位(新点.槽位编号); } return true; }
bool 多指触控管理器::抬起槽位(int 槽位编号) { if (槽位编号 < 0 || 槽位编号 >= 10) { return false; } 点表[槽位编号].是否按下 = false; 点表[槽位编号].压力值 = 0; 追踪器.释放槽位(槽位编号); return true; }
// 上报序列空闲槽位直接跳过不再先发槽位切换预分配容量避免逐事件扩容
bool 多指触控管理器::生成上报序列(std::vector<struct input_event> &输出序列) const {
    输出序列.clear();
    输出序列.reserve(48);
    for (int 槽 = 0; 槽 < 10; ++槽) {
        const 触控点 &点 = 点表[槽];
        int 标识 = 追踪器.查询标识(槽);
        bool 按下 = 点.是否按下 && 标识 >= 0;
        if (!按下 && 标识 < 0) { continue; }
        struct input_event 槽事件{}; 槽事件.type = EV_ABS; 槽事件.code = ABS_MT_SLOT; 槽事件.value = 槽; 输出序列.push_back(槽事件);
        if (按下) {
            struct input_event 标识事件{}; 标识事件.type = EV_ABS; 标识事件.code = ABS_MT_TRACKING_ID; 标识事件.value = 标识; 输出序列.push_back(标识事件);
            struct input_event 横事件{}; 横事件.type = EV_ABS; 横事件.code = ABS_MT_POSITION_X; 横事件.value = (int)点.横坐标; 输出序列.push_back(横事件);
            struct input_event 纵事件{}; 纵事件.type = EV_ABS; 纵事件.code = ABS_MT_POSITION_Y; 纵事件.value = (int)点.纵坐标; 输出序列.push_back(纵事件);
            int 压力 = (int)点.压力值; if (压力 < 0) { 压力 = 0; } if (压力 > 255) { 压力 = 255; }
            struct input_event 压事件{}; 压事件.type = EV_ABS; 压事件.code = ABS_MT_PRESSURE; 压事件.value = 压力; 输出序列.push_back(压事件);
        } else {
            struct input_event 释放事件{}; 释放事件.type = EV_ABS; 释放事件.code = ABS_MT_TRACKING_ID; 释放事件.value = -1; 输出序列.push_back(释放事件);
        }
    }
    struct input_event 触摸事件{}; 触摸事件.type = EV_KEY; 触摸事件.code = BTN_TOUCH; 触摸事件.value = 是否有按下() ? 1 : 0; 输出序列.push_back(触摸事件);
    struct input_event 同步事件{}; 同步事件.type = EV_SYN; 同步事件.code = SYN_REPORT; 同步事件.value = 0; 输出序列.push_back(同步事件);
    return true;
}
bool 多指触控管理器::是否有按下() const { for (int 下标 = 0; 下标 < 10; ++下标) { if (点表[下标].是否按下) { return true; } } return false; }
int 多指触控管理器::按下数量() const { int 计数 = 0; for (int 下标 = 0; 下标 < 10; ++下标) { if (点表[下标].是否按下) { 计数++; } } return 计数; }
void 多指触控管理器::清空全部() { for (int 下标 = 0; 下标 < 10; ++下标) { 点表[下标].是否按下 = false; 点表[下标].压力值 = 0; } 追踪器.全部释放(); }
// 批量更新统一核心逐点钳制并刷新追踪标识原四十组拷贝收敛于此分支预测友好
bool 多指触控管理器::批量更新通用(const 触控点 *点数组, int 点数量) {
    if (!点数组 || 点数量 <= 0 || 点数量 > 10) { return false; }
    float 右界 = (float)(屏幕宽 - 1);
    float 下界 = (float)(屏幕高 - 1);
    for (int 下标 = 0; 下标 < 点数量; ++下标) {
        const 触控点 &单点 = 点数组[下标];
        int 槽 = 单点.槽位编号;
        if (槽 < 0 || 槽 >= 10) { continue; }
        点表[槽] = 单点;
        if (点表[槽].横坐标 < 0) { 点表[槽].横坐标 = 0; } else if (点表[槽].横坐标 > 右界) { 点表[槽].横坐标 = 右界; }
        if (点表[槽].纵坐标 < 0) { 点表[槽].纵坐标 = 0; } else if (点表[槽].纵坐标 > 下界) { 点表[槽].纵坐标 = 下界; }
        if (单点.是否按下) { 追踪器.为槽位分配(槽); } else { 追踪器.释放槽位(槽); }
    }
    return true;
}
float 多指触控管理器::计算平均压力通用() const {
    float 累加 = 0; int 计数 = 0;
    for (int 下标 = 0; 下标 < 10; ++下标) { if (点表[下标].是否按下) { 累加 += 点表[下标].压力值; 计数++; } }
    return 计数 == 0 ? 0 : 累加 / (float)计数;
}
bool 多指触控管理器::批量更新零(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新一(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新四(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新五(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新六(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新七(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新八(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新九(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十一(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十二(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十三(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十四(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十五(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十六(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十七(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十八(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新十九(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十一(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十二(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十三(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十四(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十五(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十六(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十七(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十八(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新二十九(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十一(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十二(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十三(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十四(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十五(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十六(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十七(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十八(const 触控点 *a, int b) { return 批量更新通用(a, b); }
bool 多指触控管理器::批量更新三十九(const 触控点 *a, int b) { return 批量更新通用(a, b); }
float 多指触控管理器::计算平均压力零() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力一() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力二() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力三() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力四() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力五() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力六() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力七() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力八() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力九() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十一() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十二() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十三() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十四() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十五() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十六() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十七() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十八() const { return 计算平均压力通用(); }
float 多指触控管理器::计算平均压力十九() const { return 计算平均压力通用(); }
float 多指触控管理器::钳制横(float 数值) const { if (数值 < 0) { return 0; } float 右界 = (float)(屏幕宽 - 1); if (数值 > 右界) { return 右界; } return 数值; }
float 多指触控管理器::钳制纵(float 数值) const { if (数值 < 0) { return 0; } float 下界 = (float)(屏幕高 - 1); if (数值 > 下界) { return 下界; } return 数值; }
压力滤波器::压力滤波器() : 滤波系数(0.35f) { for (int 下标 = 0; 下标 < 10; ++下标) { 历史压力[下标] = 64.0f; } }
// 滤波统一核心采用配置系数替代逐组硬编码漂移单次乘加完成平滑
float 压力滤波器::滤波压力通用(int 槽位编号, float 原始压力) {
    if (槽位编号 < 0 || 槽位编号 >= 10) { return 0; }
    float 钳制 = 原始压力; if (钳制 < 0) { 钳制 = 0; } if (钳制 > 255) { 钳制 = 255; }
    float 系数 = 滤波系数;
    float 平滑 = 历史压力[槽位编号] * (1.0f - 系数) + 钳制 * 系数;
    历史压力[槽位编号] = 平滑; return 平滑;
}
float 压力滤波器::平滑系数通用(int 序号) const { (void)序号; return 滤波系数; }
float 压力滤波器::滤波压力零(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力一(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力三(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力四(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力五(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力六(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力七(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力八(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力九(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十一(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十二(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十三(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十四(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十五(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十六(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十七(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十八(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力十九(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十一(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十二(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十三(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十四(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十五(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十六(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十七(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十八(int a, float b) { return 滤波压力通用(a, b); }
float 压力滤波器::滤波压力二十九(int a, float b) { return 滤波压力通用(a, b); }
void 压力滤波器::重置槽位(int 槽位编号) { if (槽位编号 >= 0 && 槽位编号 < 10) { 历史压力[槽位编号] = 64.0f; } }
void 压力滤波器::重置全部() { for (int 下标 = 0; 下标 < 10; ++下标) { 历史压力[下标] = 64.0f; } }
float 压力滤波器::平滑系数零() const { return 平滑系数通用(0); }
float 压力滤波器::平滑系数一() const { return 平滑系数通用(1); }
float 压力滤波器::平滑系数二() const { return 平滑系数通用(2); }
float 压力滤波器::平滑系数三() const { return 平滑系数通用(3); }
float 压力滤波器::平滑系数四() const { return 平滑系数通用(4); }
float 压力滤波器::平滑系数五() const { return 平滑系数通用(5); }
float 压力滤波器::平滑系数六() const { return 平滑系数通用(6); }
float 压力滤波器::平滑系数七() const { return 平滑系数通用(7); }
float 压力滤波器::平滑系数八() const { return 平滑系数通用(8); }
float 压力滤波器::平滑系数九() const { return 平滑系数通用(9); }
float 压力滤波器::平滑系数十() const { return 平滑系数通用(10); }
float 压力滤波器::平滑系数十一() const { return 平滑系数通用(11); }
float 压力滤波器::平滑系数十二() const { return 平滑系数通用(12); }
float 压力滤波器::平滑系数十三() const { return 平滑系数通用(13); }
float 压力滤波器::平滑系数十四() const { return 平滑系数通用(14); }
坐标校准器::坐标校准器() : 横向缩放(1.0f), 纵向缩放(1.0f), 横向偏移(0), 纵向偏移(0), 屏幕宽度(1080), 屏幕高度(2400) { if (屏幕宽度 <= 0) { 屏幕宽度 = 1080; } if (屏幕高度 <= 0) { 屏幕高度 = 2400; } }
float 坐标校准器::边缘补偿通用(float 坐标, float 上限) const {
    if (坐标 < 0) { return 0; } if (坐标 >= 上限) { return 上限 - 1; }
    float 边距 = 上限 * 0.02f;
    if (坐标 < 边距) { return 坐标 * 0.98f; }
    return 坐标;
}
float 坐标校准器::校准横向通用(float 原始横) const { float 结果 = 原始横 * 横向缩放 + 横向偏移; return 边缘补偿通用(结果, (float)屏幕宽度); }
float 坐标校准器::校准纵向通用(float 原始纵) const { float 结果 = 原始纵 * 纵向缩放 + 纵向偏移; return 边缘补偿通用(结果, (float)屏幕高度); }
float 坐标校准器::校准横向零(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向一(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向三(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向四(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向五(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向六(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向七(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向八(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向九(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十一(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十二(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十三(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十四(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十五(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十六(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十七(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十八(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向十九(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十一(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十二(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十三(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十四(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十五(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十六(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十七(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十八(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准横向二十九(float v) const { return 校准横向通用(v); }
float 坐标校准器::校准纵向零(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向一(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向三(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向四(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向五(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向六(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向七(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向八(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向九(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十一(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十二(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十三(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十四(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十五(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十六(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十七(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十八(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向十九(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十一(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十二(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十三(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十四(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十五(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十六(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十七(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十八(float v) const { return 校准纵向通用(v); }
float 坐标校准器::校准纵向二十九(float v) const { return 校准纵向通用(v); }
void 坐标校准器::设置缩放(float 横缩放, float 纵缩放) { if (横缩放 > 0.1f && 横缩放 < 10.0f) { 横向缩放 = 横缩放; } if (纵缩放 > 0.1f && 纵缩放 < 10.0f) { 纵向缩放 = 纵缩放; } }
void 坐标校准器::设置偏移(float 横偏移, float 纵偏移) { 横向偏移 = 横偏移; 纵向偏移 = 纵偏移; }
void 坐标校准器::设置屏幕(int 宽度, int 高度) { if (宽度 > 0) { 屏幕宽度 = 宽度; } if (高度 > 0) { 屏幕高度 = 高度; } }
float 坐标校准器::边缘补偿零(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿一(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿二(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿三(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿四(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿五(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿六(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿七(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿八(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿九(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿十(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿十一(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿十二(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿十三(float a, float b) const { return 边缘补偿通用(a, b); }
float 坐标校准器::边缘补偿十四(float a, float b) const { return 边缘补偿通用(a, b); }
抖动抑制器::抖动抑制器() : 死区半径(2.0f) { for (int 下标 = 0; 下标 < 10; ++下标) { 上次横[下标] = 0; 上次纵[下标] = 0; } }
// 抖动判定统一核心平方距离比较替代开方原三十组阈值漂移修复为统一死区
bool 抖动抑制器::应否上报通用(int 槽位编号, float 新横, float 新纵) {
    if (槽位编号 < 0 || 槽位编号 >= 10) { return false; }
    float 差横 = 新横 - 上次横[槽位编号];
    float 差纵 = 新纵 - 上次纵[槽位编号];
    float 平方距 = 差横 * 差横 + 差纵 * 差纵;
    return 平方距 > 死区半径 * 死区半径;
}
float 抖动抑制器::抖动阈值通用(int 序号) const { (void)序号; return 死区半径; }
bool 抖动抑制器::应否上报零(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报一(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报三(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报四(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报五(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报六(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报七(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报八(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报九(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十一(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十二(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十三(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十四(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十五(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十六(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十七(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十八(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报十九(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十一(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十二(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十三(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十四(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十五(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十六(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十七(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十八(int a, float b, float c) { return 应否上报通用(a, b, c); }
bool 抖动抑制器::应否上报二十九(int a, float b, float c) { return 应否上报通用(a, b, c); }
void 抖动抑制器::记录位置(int 槽位编号, float 横, float 纵) { if (槽位编号 < 0 || 槽位编号 >= 10) { return; } 上次横[槽位编号] = 横; 上次纵[槽位编号] = 纵; }
float 抖动抑制器::抖动阈值零() const { return 抖动阈值通用(0); }
float 抖动抑制器::抖动阈值一() const { return 抖动阈值通用(1); }
float 抖动抑制器::抖动阈值二() const { return 抖动阈值通用(2); }
float 抖动抑制器::抖动阈值三() const { return 抖动阈值通用(3); }
float 抖动抑制器::抖动阈值四() const { return 抖动阈值通用(4); }
float 抖动抑制器::抖动阈值五() const { return 抖动阈值通用(5); }
float 抖动抑制器::抖动阈值六() const { return 抖动阈值通用(6); }
float 抖动抑制器::抖动阈值七() const { return 抖动阈值通用(7); }
float 抖动抑制器::抖动阈值八() const { return 抖动阈值通用(8); }
float 抖动抑制器::抖动阈值九() const { return 抖动阈值通用(9); }
} // namespace 输入子系统
