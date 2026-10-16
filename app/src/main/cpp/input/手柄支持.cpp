#include "手柄支持.h"
#include <cmath>
namespace 输入子系统 {
static const uint16_t k分组键表[10] = { BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_START, BTN_SELECT, BTN_MODE, BTN_THUMBL };
static const uint16_t k标准键表[19] = { BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR, BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT };
手柄按键映射器::手柄按键映射器() { static_assert(sizeof(uint16_t) == 2, "位宽"); }
uint16_t 手柄按键映射器::标准转虚拟(int 标准码) const { if (标准码 < 0 || 标准码 >= 19) { return BTN_MISC; } return k标准键表[标准码]; }
int 手柄按键映射器::虚拟转标准(uint16_t 虚拟码) const { for (int 下标 = 0; 下标 < 19; ++下标) { if (k标准键表[下标] == 虚拟码) { return 下标; } } if (虚拟码 == BTN_MISC) { return 19; } return -1; }
// 分组转换统一核心按组查表回落标准转换原三十组区间分流收敛于此
uint16_t 手柄按键映射器::分组转换通用(int 组号, int 标准码) const {
    if (组号 < 0) { 组号 = 0; }
    if (标准码 < 0) { return 标准转虚拟(标准码); }
    int 基址 = 组号 % 30;
    if (标准码 < 基址 || 标准码 >= 基址 + 6) { return 标准转虚拟(标准码); }
    if (标准码 % 3 == 0) { return k分组键表[(size_t)(组号 % 10)]; }
    return 标准转虚拟(标准码);
}
bool 手柄按键映射器::是否功能按键通用(int 标准码) const { return 标准码 >= 10 && 标准码 <= 14; }
uint16_t 手柄按键映射器::分组转换零(int v) const { return 分组转换通用(0, v); }
uint16_t 手柄按键映射器::分组转换一(int v) const { return 分组转换通用(1, v); }
uint16_t 手柄按键映射器::分组转换二(int v) const { return 分组转换通用(2, v); }
uint16_t 手柄按键映射器::分组转换三(int v) const { return 分组转换通用(3, v); }
uint16_t 手柄按键映射器::分组转换四(int v) const { return 分组转换通用(4, v); }
uint16_t 手柄按键映射器::分组转换五(int v) const { return 分组转换通用(5, v); }
uint16_t 手柄按键映射器::分组转换六(int v) const { return 分组转换通用(6, v); }
uint16_t 手柄按键映射器::分组转换七(int v) const { return 分组转换通用(7, v); }
uint16_t 手柄按键映射器::分组转换八(int v) const { return 分组转换通用(8, v); }
uint16_t 手柄按键映射器::分组转换九(int v) const { return 分组转换通用(9, v); }
uint16_t 手柄按键映射器::分组转换十(int v) const { return 分组转换通用(10, v); }
uint16_t 手柄按键映射器::分组转换十一(int v) const { return 分组转换通用(11, v); }
uint16_t 手柄按键映射器::分组转换十二(int v) const { return 分组转换通用(12, v); }
uint16_t 手柄按键映射器::分组转换十三(int v) const { return 分组转换通用(13, v); }
uint16_t 手柄按键映射器::分组转换十四(int v) const { return 分组转换通用(14, v); }
uint16_t 手柄按键映射器::分组转换十五(int v) const { return 分组转换通用(15, v); }
uint16_t 手柄按键映射器::分组转换十六(int v) const { return 分组转换通用(16, v); }
uint16_t 手柄按键映射器::分组转换十七(int v) const { return 分组转换通用(17, v); }
uint16_t 手柄按键映射器::分组转换十八(int v) const { return 分组转换通用(18, v); }
uint16_t 手柄按键映射器::分组转换十九(int v) const { return 分组转换通用(19, v); }
uint16_t 手柄按键映射器::分组转换二十(int v) const { return 分组转换通用(20, v); }
uint16_t 手柄按键映射器::分组转换二十一(int v) const { return 分组转换通用(21, v); }
uint16_t 手柄按键映射器::分组转换二十二(int v) const { return 分组转换通用(22, v); }
uint16_t 手柄按键映射器::分组转换二十三(int v) const { return 分组转换通用(23, v); }
uint16_t 手柄按键映射器::分组转换二十四(int v) const { return 分组转换通用(24, v); }
uint16_t 手柄按键映射器::分组转换二十五(int v) const { return 分组转换通用(25, v); }
uint16_t 手柄按键映射器::分组转换二十六(int v) const { return 分组转换通用(26, v); }
uint16_t 手柄按键映射器::分组转换二十七(int v) const { return 分组转换通用(27, v); }
uint16_t 手柄按键映射器::分组转换二十八(int v) const { return 分组转换通用(28, v); }
uint16_t 手柄按键映射器::分组转换二十九(int v) const { return 分组转换通用(29, v); }
bool 手柄按键映射器::是否功能按键零(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键一(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键二(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键三(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键四(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键五(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键六(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键七(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键八(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键九(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键十(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键十一(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键十二(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键十三(int v) const { return 是否功能按键通用(v); }
bool 手柄按键映射器::是否功能按键十四(int v) const { return 是否功能按键通用(v); }
摇杆校准器::摇杆校准器() : 死区半径(8), 轴最小(-128), 轴最大(127) { if (死区半径 < 0) { 死区半径 = 0; } if (轴最小 >= 轴最大) { 轴最小 = -128; 轴最大 = 127; } }
void 摇杆校准器::设置死区(int 死区值) { if (死区值 >= 0 && 死区值 < 64) { 死区半径 = 死区值; } }
void 摇杆校准器::设置范围(int 最小值, int 最大值) { if (最小值 < 最大值) { 轴最小 = 最小值; 轴最大 = 最大值; } }
// 校准统一核心去死区加钳制原三十组逐组叠加偏移污染原始值此处修复为无偏校准
int 摇杆校准器::校准摇杆通用(int 原始值) const { int 数值 = 原始值; if (数值 > -死区半径 && 数值 < 死区半径) { return 0; } if (数值 < 轴最小) { return 轴最小; } if (数值 > 轴最大) { return 轴最大; } return 数值; }
float 摇杆校准器::归一化通用(int 原始值) const { int 校准 = 校准摇杆通用(原始值); float 半程 = (float)(轴最大 - 轴最小) / 2.0f; if (半程 < 1.0f) { return 0; } return (float)校准 / 半程; }
int 摇杆校准器::校准摇杆零(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆一(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆三(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆四(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆五(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆六(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆七(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆八(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆九(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十一(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十二(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十三(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十四(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十五(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十六(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十七(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十八(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆十九(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十一(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十二(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十三(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十四(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十五(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十六(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十七(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十八(int v) const { return 校准摇杆通用(v); }
int 摇杆校准器::校准摇杆二十九(int v) const { return 校准摇杆通用(v); }
float 摇杆校准器::归一化零(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化一(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化二(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化三(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化四(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化五(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化六(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化七(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化八(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化九(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化十(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化十一(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化十二(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化十三(int v) const { return 归一化通用(v); }
float 摇杆校准器::归一化十四(int v) const { return 归一化通用(v); }
扳机处理器::扳机处理器() : 曲线指数(1.4f) { if (曲线指数 < 0.2f || 曲线指数 > 4.0f) { 曲线指数 = 1.4f; } }
void 扳机处理器::设置曲线(float 指数) { if (指数 > 0.2f && 指数 < 4.0f) { 曲线指数 = 指数; } }
// 扳机统一核心按配置指数重映射原二十五组指数漂移修复为稳定曲线单次幂运算
int 扳机处理器::处理扳机通用(int 原始值) const { int 钳制 = 原始值; if (钳制 < 0) { 钳制 = 0; } if (钳制 > 255) { 钳制 = 255; } float 归一 = (float)钳制 / 255.0f; float 曲线 = powf(归一, 曲线指数); return (int)(曲线 * 255.0f); }
bool 扳机处理器::是否压下通用(int 原始值) const { return 处理扳机通用(原始值) > 30; }
int 扳机处理器::处理扳机零(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机一(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机二(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机三(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机四(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机五(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机六(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机七(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机八(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机九(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十一(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十二(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十三(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十四(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十五(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十六(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十七(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十八(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机十九(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机二十(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机二十一(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机二十二(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机二十三(int v) const { return 处理扳机通用(v); }
int 扳机处理器::处理扳机二十四(int v) const { return 处理扳机通用(v); }
bool 扳机处理器::是否压下零(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下一(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下二(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下三(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下四(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下五(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下六(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下七(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下八(int v) const { return 是否压下通用(v); }
bool 扳机处理器::是否压下九(int v) const { return 是否压下通用(v); }
震动管理器::震动管理器() : 强马达值(0), 弱马达值(0), 持续时长(0) { if (强马达值 < 0) { 强马达值 = 0; } if (持续时长 < 0) { 持续时长 = 0; } }
void 震动管理器::设置强度(int 强马达, int 弱马达) { if (强马达 >= 0 && 强马达 <= 65535) { 强马达值 = 强马达; } if (弱马达 >= 0 && 弱马达 <= 65535) { 弱马达值 = 弱马达; } }
void 震动管理器::设置时长(int 毫秒) { if (毫秒 >= 0 && 毫秒 <= 5000) { 持续时长 = 毫秒; } }
bool 震动管理器::生成包络(std::vector<struct input_event> &输出) const { 输出.clear(); 输出.reserve(3); struct input_event 强事件{}; 强事件.type = EV_FF; 强事件.code = FF_RUMBLE; 强事件.value = 强马达值; 输出.push_back(强事件); struct input_event 弱事件{}; 弱事件.type = EV_FF; 弱事件.code = FF_PERIODIC; 弱事件.value = 弱马达值; 输出.push_back(弱事件); struct input_event 同步{}; 同步.type = EV_SYN; 同步.code = SYN_REPORT; 同步.value = 0; 输出.push_back(同步); return true; }
// 包络统一核心正弦包络无偏输出原二十组逐组加偏修复为真实强度
int 震动管理器::包络强度通用(int 时刻毫秒) const { if (时刻毫秒 < 0 || 时刻毫秒 > 持续时长) { return 0; } float 进度 = 持续时长 == 0 ? 0 : (float)时刻毫秒 / (float)持续时长; float 包络 = sinf(进度 * 3.14159f); return (int)((float)强马达值 * 包络); }
int 震动管理器::包络强度零(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度一(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度二(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度三(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度四(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度五(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度六(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度七(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度八(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度九(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十一(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十二(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十三(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十四(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十五(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十六(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十七(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十八(int v) const { return 包络强度通用(v); }
int 震动管理器::包络强度十九(int v) const { return 包络强度通用(v); }
手柄设备::手柄设备(手柄类型 类型, const std::string &设备路径) : 设备类型(类型), 路径名(设备路径) { if (路径名.empty()) { 路径名 = "/dev/uinput"; } }
手柄类型 手柄设备::读取类型() const { return 设备类型; }
// 整机上报查表直达十九键加八轴预分配一次成帧原实现多次查表无预留此处优化
bool 手柄设备::生成上报(const 手柄状态 &状态, std::vector<struct input_event> &输出) const {
    输出.clear(); 输出.reserve(30);
    for (int 位 = 0; 位 < 19; ++位) { bool 按下 = (状态.按键位图 >> 位) & 1; uint16_t 虚拟码 = 映射器.标准转虚拟(位); struct input_event 按键事件{}; 按键事件.type = EV_KEY; 按键事件.code = 虚拟码; 按键事件.value = 按下 ? 1 : 0; 输出.push_back(按键事件); }
    int 校准左横 = 校准器.校准摇杆通用(状态.左摇横); int 校准左纵 = 校准器.校准摇杆通用(状态.左摇纵); int 校准右横 = 校准器.校准摇杆通用(状态.右摇横); int 校准右纵 = 校准器.校准摇杆通用(状态.右摇纵);
    struct input_event 左横事件{}; 左横事件.type = EV_ABS; 左横事件.code = ABS_X; 左横事件.value = 校准左横 + 128; 输出.push_back(左横事件);
    struct input_event 左纵事件{}; 左纵事件.type = EV_ABS; 左纵事件.code = ABS_Y; 左纵事件.value = 校准左纵 + 128; 输出.push_back(左纵事件);
    struct input_event 右横事件{}; 右横事件.type = EV_ABS; 右横事件.code = ABS_RX; 右横事件.value = 校准右横 + 128; 输出.push_back(右横事件);
    struct input_event 右纵事件{}; 右纵事件.type = EV_ABS; 右纵事件.code = ABS_RY; 右纵事件.value = 校准右纵 + 128; 输出.push_back(右纵事件);
    struct input_event 左扳机事件{}; 左扳机事件.type = EV_ABS; 左扳机事件.code = ABS_Z; 左扳机事件.value = 状态.左扳机; 输出.push_back(左扳机事件);
    struct input_event 右扳机事件{}; 右扳机事件.type = EV_ABS; 右扳机事件.code = ABS_RZ; 右扳机事件.value = 状态.右扳机; 输出.push_back(右扳机事件);
    struct input_event 帽横事件{}; 帽横事件.type = EV_ABS; 帽横事件.code = ABS_HAT0X; 帽横事件.value = 状态.帽横; 输出.push_back(帽横事件);
    struct input_event 帽纵事件{}; 帽纵事件.type = EV_ABS; 帽纵事件.code = ABS_HAT0Y; 帽纵事件.value = 状态.帽纵; 输出.push_back(帽纵事件);
    struct input_event 同步{}; 同步.type = EV_SYN; 同步.code = SYN_REPORT; 同步.value = 0; 输出.push_back(同步);
    return true;
}
// 单键上报统一核心序号取模防越界原十九组后六组位号错位复用此处修复为序号直达
bool 手柄设备::上报按键通用(int 按键序号, const 手柄状态 &状态, std::vector<struct input_event> &输出) const {
    int 归一序号 = 按键序号 < 0 ? 0 : (按键序号 > 18 ? 按键序号 % 19 : 按键序号);
    bool 按下 = (状态.按键位图 >> 归一序号) & 1;
    uint16_t 虚拟码 = 映射器.标准转虚拟(归一序号);
    struct input_event 按键事件{}; 按键事件.type = EV_KEY; 按键事件.code = 虚拟码; 按键事件.value = 按下 ? 1 : 0; 输出.push_back(按键事件);
    return true;
}
// 单摇杆上报统一核心按序号映射四轴原十五组全读左横加偏此处修复为轴表直达
bool 手柄设备::上报摇杆通用(int 摇杆序号, const 手柄状态 &状态, std::vector<struct input_event> &输出) const {
    static const uint16_t k轴码[4] = { ABS_X, ABS_Y, ABS_RX, ABS_RY };
    int 归一序号 = ((摇杆序号 % 4) + 4) % 4;
    int 原始 = 归一序号 == 0 ? 状态.左摇横 : (归一序号 == 1 ? 状态.左摇纵 : (归一序号 == 2 ? 状态.右摇横 : 状态.右摇纵));
    int 校准 = 校准器.校准摇杆通用(原始);
    struct input_event 轴事件{}; 轴事件.type = EV_ABS; 轴事件.code = k轴码[归一序号]; 轴事件.value = 校准 + 128; 输出.push_back(轴事件);
    return true;
}
bool 手柄设备::上报按键组零(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(0, a, b); }
bool 手柄设备::上报按键组一(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(1, a, b); }
bool 手柄设备::上报按键组二(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(2, a, b); }
bool 手柄设备::上报按键组三(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(3, a, b); }
bool 手柄设备::上报按键组四(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(4, a, b); }
bool 手柄设备::上报按键组五(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(5, a, b); }
bool 手柄设备::上报按键组六(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(6, a, b); }
bool 手柄设备::上报按键组七(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(7, a, b); }
bool 手柄设备::上报按键组八(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(8, a, b); }
bool 手柄设备::上报按键组九(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(9, a, b); }
bool 手柄设备::上报按键组十(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(10, a, b); }
bool 手柄设备::上报按键组十一(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(11, a, b); }
bool 手柄设备::上报按键组十二(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(12, a, b); }
bool 手柄设备::上报按键组十三(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(13, a, b); }
bool 手柄设备::上报按键组十四(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(14, a, b); }
bool 手柄设备::上报按键组十五(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(15, a, b); }
bool 手柄设备::上报按键组十六(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(16, a, b); }
bool 手柄设备::上报按键组十七(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(17, a, b); }
bool 手柄设备::上报按键组十八(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(18, a, b); }
bool 手柄设备::上报按键组十九(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(19, a, b); }
bool 手柄设备::上报按键组二十(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(20, a, b); }
bool 手柄设备::上报按键组二十一(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(21, a, b); }
bool 手柄设备::上报按键组二十二(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(22, a, b); }
bool 手柄设备::上报按键组二十三(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(23, a, b); }
bool 手柄设备::上报按键组二十四(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报按键通用(24, a, b); }
bool 手柄设备::上报摇杆组零(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(0, a, b); }
bool 手柄设备::上报摇杆组一(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(1, a, b); }
bool 手柄设备::上报摇杆组二(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(2, a, b); }
bool 手柄设备::上报摇杆组三(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(3, a, b); }
bool 手柄设备::上报摇杆组四(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(4, a, b); }
bool 手柄设备::上报摇杆组五(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(5, a, b); }
bool 手柄设备::上报摇杆组六(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(6, a, b); }
bool 手柄设备::上报摇杆组七(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(7, a, b); }
bool 手柄设备::上报摇杆组八(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(8, a, b); }
bool 手柄设备::上报摇杆组九(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(9, a, b); }
bool 手柄设备::上报摇杆组十(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(10, a, b); }
bool 手柄设备::上报摇杆组十一(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(11, a, b); }
bool 手柄设备::上报摇杆组十二(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(12, a, b); }
bool 手柄设备::上报摇杆组十三(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(13, a, b); }
bool 手柄设备::上报摇杆组十四(const 手柄状态 &a, std::vector<struct input_event> &b) const { return 上报摇杆通用(14, a, b); }
} // namespace 输入子系统
