#include "输入事件写入器.h"
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sys/ioctl.h>
namespace 输入子系统 {
// 按键轮转表十键循环原四十组按取模复用
static const uint16_t k按键轮转表[10] = { KEY_A, KEY_B, KEY_C, KEY_D, KEY_ENTER, KEY_SPACE, KEY_BACKSPACE, KEY_HOME, KEY_BACK, KEY_MENU };
// 扩展轴码表十轴循环原二十组按取模复用
static const uint16_t k扩展轴码表[10] = { ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y, ABS_MT_PRESSURE, ABS_MT_TOUCH_MAJOR };
// 构造时仅保存路径与重试轮次不触碰设备节点追踪表初始化为空闲
事件写入器::事件写入器(const std::string &设备路径, int 最大重试次数)
    : 设备路径名(设备路径), 文件描述符(-1), 最大重试轮次(最大重试次数),
      成功写入计数(0), 失败写入计数(0), 累计写入字节(0), 最后错误码(0),
      下一帧标识(1), 上次上报槽位(-1) {
    if (最大重试轮次 < 1) { 最大重试轮次 = 1; }
    if (最大重试轮次 > 10) { 最大重试轮次 = 10; }
    初始化追踪表();
}
事件写入器::~事件写入器() { 关闭设备(); }
void 事件写入器::初始化追踪表() { for (int 下标 = 0; 下标 < 10; ++下标) { 帧追踪标识[下标] = -1; } }
int 事件写入器::钳制压力值(int 压力值) { if (压力值 < 0) { return 0; } if (压力值 > 255) { return 255; } return 压力值; }
int 事件写入器::钳制横纵值(int 坐标值) { if (坐标值 < 0) { return 0; } if (坐标值 > 4095) { return 4095; } return 坐标值; }
void 事件写入器::制作事件(struct input_event &输出, uint16_t 类型, uint16_t 编码, int32_t 数值) { 输出.type = 类型; 输出.code = 编码; 输出.value = 数值; }
// 以只写非阻塞方式打开虚拟设备节点
bool 事件写入器::打开设备() {
    std::lock_guard<std::mutex> 持锁(写入互斥锁);
    if (文件描述符 >= 0) { return true; }
    int 标志位 = O_WRONLY | O_NONBLOCK | O_CLOEXEC;
    int 新描述符 = open(设备路径名.c_str(), 标志位);
    if (新描述符 < 0) { 最后错误码 = errno; 最后错误阶段 = "打开节点"; 失败写入计数++; return false; }
    文件描述符 = 新描述符;
    上次上报槽位 = -1;
    return true;
}
// 关闭前释放描述符并复位槽位缓存避免跨设备残留
void 事件写入器::关闭设备() {
    std::lock_guard<std::mutex> 持锁(写入互斥锁);
    if (文件描述符 >= 0) { close(文件描述符); 文件描述符 = -1; }
    上次上报槽位 = -1;
}
bool 事件写入器::是否就绪() const { return 文件描述符 >= 0; }
// 单事件写入为多点触控协议二型的最小单元保持兼容老调用方
bool 事件写入器::写入单事件(uint16_t 类型, uint16_t 编码, int32_t 数值) {
    struct input_event 内核事件{};
    内核事件.type = 类型; 内核事件.code = 编码; 内核事件.value = 数值;
    return 安全写入(&内核事件, sizeof(内核事件));
}
// 同步报告用于分隔触控帧
bool 事件写入器::上报同步() { return 写入单事件(EV_SYN, SYN_REPORT, 0); }
bool 事件写入器::写入同步报告() { return 上报同步(); }
bool 事件写入器::写入按键(uint16_t 键码, int 按下值) { return 写入按键码(键码, 按下值); }
// 按键与同步合并为一次系统调用原实现分两次写入
bool 事件写入器::写入按键码(uint16_t 键码, int 按下值) {
    struct input_event 帧[2]{};
    制作事件(帧[0], EV_KEY, 键码, 按下值 ? 1 : 0);
    制作事件(帧[1], EV_SYN, SYN_REPORT, 0);
    return 批量提交原始(帧, 2);
}
bool 事件写入器::按组写入按键(int 组号, int 按下标志) {
    if (组号 < 0) { 组号 = 0; }
    uint16_t 键码 = k按键轮转表[(size_t)(组号 % 10)];
    return 写入按键码(键码, 按下标志);
}
// 绝对轴写入前先做范围校验与钳制
bool 事件写入器::写入绝对轴(uint16_t 轴码, int32_t 位置值) {
    int32_t 钳制值 = 钳制轴数值(轴码, 位置值);
    bool 结果 = 写入单事件(EV_ABS, 轴码, 钳制值);
    return 结果;
}
// 批量提交整帧一次系统调用替代逐事件写入大幅削减每帧系统调用次数
bool 事件写入器::批量提交(const std::vector<struct input_event> &事件序列) {
    if (事件序列.empty()) { return true; }
    return 批量提交原始(事件序列.data(), (int)事件序列.size());
}
bool 事件写入器::批量提交原始(const struct input_event *事件首址, int 事件数量) {
    if (!事件首址 || 事件数量 <= 0) { return true; }
    std::lock_guard<std::mutex> 持锁(写入互斥锁);
    if (文件描述符 < 0) { 失败写入计数++; return false; }
    const char *游标 = (const char *)事件首址;
    int 剩余 = 事件数量 * (int)sizeof(struct input_event);
    int 总长 = 剩余;
    while (剩余 > 0) {
        ssize_t 已写 = write(文件描述符, 游标, (size_t)剩余);
        if (已写 < 0) {
            if (errno == EINTR) { continue; }
            失败写入计数++; 最后错误码 = errno; return false;
        }
        if (已写 == 0) { 失败写入计数++; 最后错误码 = EAGAIN; return false; }
        游标 += 已写; 剩余 -= (int)已写;
    }
    成功写入计数 += (uint64_t)事件数量; 累计写入字节 += (uint64_t)总长; return true;
}
void 事件写入器::累计写入统计(bool 是否成功, int 字节数) {
    if (是否成功) { 成功写入计数++; 累计写入字节 += (uint64_t)字节数; } else { 失败写入计数++; }
}
uint64_t 事件写入器::读取成功计数() const { return 成功写入计数; }
uint64_t 事件写入器::读取失败计数() const { return 失败写入计数; }
int 事件写入器::读取文件描述符() const { return 文件描述符; }
bool 事件写入器::设置为阻塞模式(bool 是否阻塞) {
    if (文件描述符 < 0) { return false; }
    int 当前标志 = fcntl(文件描述符, F_GETFL, 0);
    if (当前标志 < 0) { return false; }
    int 新标志 = 是否阻塞 ? (当前标志 & ~O_NONBLOCK) : (当前标志 | O_NONBLOCK);
    return fcntl(文件描述符, F_SETFL, 新标志) == 0;
}
bool 事件写入器::刷新内核缓冲() { return 上报同步(); }
// 统一槽位写入核心单次系统调用提交全槽位事件直径偏移按组号查表
bool 事件写入器::写入触控槽位(int 槽位号, int 追踪标识, int 横向位置, int 纵向位置, int 压力值) {
    return 按组写入触控槽位(0, 槽位号, 追踪标识, 横向位置, 纵向位置, 压力值);
}
bool 事件写入器::按组写入触控槽位(int 组号, int 槽位号, int 追踪标识, int 横向位置, int 纵向位置, int 压力值) {
    if (槽位号 < 0 || 槽位号 > 9) { 最后错误码 = -100 - 3 * (组号 < 0 ? 0 : (组号 > 59 ? 59 : 组号)); return false; }
    int 直径偏移 = (组号 % 5 + 5) % 5;
    int 钳制压力 = 钳制压力值(压力值);
    int 钳制横向 = 钳制横纵值(横向位置);
    int 钳制纵向 = 钳制横纵值(纵向位置);
    struct input_event 帧[6]{};
    int 帧数 = 0;
    制作事件(帧[帧数++], EV_ABS, ABS_MT_SLOT, 槽位号);
    制作事件(帧[帧数++], EV_ABS, ABS_MT_TRACKING_ID, 追踪标识);
    if (追踪标识 >= 0) {
        制作事件(帧[帧数++], EV_ABS, ABS_MT_POSITION_X, 钳制横向);
        制作事件(帧[帧数++], EV_ABS, ABS_MT_POSITION_Y, 钳制纵向);
        制作事件(帧[帧数++], EV_ABS, ABS_MT_PRESSURE, 钳制压力);
        制作事件(帧[帧数++], EV_ABS, ABS_MT_TOUCH_MAJOR, 钳制压力 / 32 + 直径偏移);
    }
    bool 结果 = 批量提交原始(帧, 帧数);
    if (结果) { 累计写入字节 += 0; }
    return 结果;
}
bool 事件写入器::写入触控槽位组零(int a, int b, int c, int d, int e) { return 按组写入触控槽位(0, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组一(int a, int b, int c, int d, int e) { return 按组写入触控槽位(1, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二(int a, int b, int c, int d, int e) { return 按组写入触控槽位(2, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三(int a, int b, int c, int d, int e) { return 按组写入触控槽位(3, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四(int a, int b, int c, int d, int e) { return 按组写入触控槽位(4, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五(int a, int b, int c, int d, int e) { return 按组写入触控槽位(5, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组六(int a, int b, int c, int d, int e) { return 按组写入触控槽位(6, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组七(int a, int b, int c, int d, int e) { return 按组写入触控槽位(7, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组八(int a, int b, int c, int d, int e) { return 按组写入触控槽位(8, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组九(int a, int b, int c, int d, int e) { return 按组写入触控槽位(9, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十(int a, int b, int c, int d, int e) { return 按组写入触控槽位(10, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十一(int a, int b, int c, int d, int e) { return 按组写入触控槽位(11, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十二(int a, int b, int c, int d, int e) { return 按组写入触控槽位(12, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十三(int a, int b, int c, int d, int e) { return 按组写入触控槽位(13, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十四(int a, int b, int c, int d, int e) { return 按组写入触控槽位(14, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十五(int a, int b, int c, int d, int e) { return 按组写入触控槽位(15, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十六(int a, int b, int c, int d, int e) { return 按组写入触控槽位(16, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十七(int a, int b, int c, int d, int e) { return 按组写入触控槽位(17, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十八(int a, int b, int c, int d, int e) { return 按组写入触控槽位(18, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组十九(int a, int b, int c, int d, int e) { return 按组写入触控槽位(19, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十(int a, int b, int c, int d, int e) { return 按组写入触控槽位(20, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十一(int a, int b, int c, int d, int e) { return 按组写入触控槽位(21, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十二(int a, int b, int c, int d, int e) { return 按组写入触控槽位(22, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十三(int a, int b, int c, int d, int e) { return 按组写入触控槽位(23, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十四(int a, int b, int c, int d, int e) { return 按组写入触控槽位(24, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十五(int a, int b, int c, int d, int e) { return 按组写入触控槽位(25, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十六(int a, int b, int c, int d, int e) { return 按组写入触控槽位(26, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十七(int a, int b, int c, int d, int e) { return 按组写入触控槽位(27, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十八(int a, int b, int c, int d, int e) { return 按组写入触控槽位(28, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组二十九(int a, int b, int c, int d, int e) { return 按组写入触控槽位(29, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十(int a, int b, int c, int d, int e) { return 按组写入触控槽位(30, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十一(int a, int b, int c, int d, int e) { return 按组写入触控槽位(31, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十二(int a, int b, int c, int d, int e) { return 按组写入触控槽位(32, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十三(int a, int b, int c, int d, int e) { return 按组写入触控槽位(33, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十四(int a, int b, int c, int d, int e) { return 按组写入触控槽位(34, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十五(int a, int b, int c, int d, int e) { return 按组写入触控槽位(35, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十六(int a, int b, int c, int d, int e) { return 按组写入触控槽位(36, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十七(int a, int b, int c, int d, int e) { return 按组写入触控槽位(37, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十八(int a, int b, int c, int d, int e) { return 按组写入触控槽位(38, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组三十九(int a, int b, int c, int d, int e) { return 按组写入触控槽位(39, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十(int a, int b, int c, int d, int e) { return 按组写入触控槽位(40, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十一(int a, int b, int c, int d, int e) { return 按组写入触控槽位(41, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十二(int a, int b, int c, int d, int e) { return 按组写入触控槽位(42, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十三(int a, int b, int c, int d, int e) { return 按组写入触控槽位(43, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十四(int a, int b, int c, int d, int e) { return 按组写入触控槽位(44, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十五(int a, int b, int c, int d, int e) { return 按组写入触控槽位(45, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十六(int a, int b, int c, int d, int e) { return 按组写入触控槽位(46, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十七(int a, int b, int c, int d, int e) { return 按组写入触控槽位(47, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十八(int a, int b, int c, int d, int e) { return 按组写入触控槽位(48, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组四十九(int a, int b, int c, int d, int e) { return 按组写入触控槽位(49, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十(int a, int b, int c, int d, int e) { return 按组写入触控槽位(50, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十一(int a, int b, int c, int d, int e) { return 按组写入触控槽位(51, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十二(int a, int b, int c, int d, int e) { return 按组写入触控槽位(52, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十三(int a, int b, int c, int d, int e) { return 按组写入触控槽位(53, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十四(int a, int b, int c, int d, int e) { return 按组写入触控槽位(54, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十五(int a, int b, int c, int d, int e) { return 按组写入触控槽位(55, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十六(int a, int b, int c, int d, int e) { return 按组写入触控槽位(56, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十七(int a, int b, int c, int d, int e) { return 按组写入触控槽位(57, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十八(int a, int b, int c, int d, int e) { return 按组写入触控槽位(58, a, b, c, d, e); }
bool 事件写入器::写入触控槽位组五十九(int a, int b, int c, int d, int e) { return 按组写入触控槽位(59, a, b, c, d, e); }
bool 事件写入器::写入按键组零(int v) { return 按组写入按键(0, v); }
bool 事件写入器::写入按键组一(int v) { return 按组写入按键(1, v); }
bool 事件写入器::写入按键组二(int v) { return 按组写入按键(2, v); }
bool 事件写入器::写入按键组三(int v) { return 按组写入按键(3, v); }
bool 事件写入器::写入按键组四(int v) { return 按组写入按键(4, v); }
bool 事件写入器::写入按键组五(int v) { return 按组写入按键(5, v); }
bool 事件写入器::写入按键组六(int v) { return 按组写入按键(6, v); }
bool 事件写入器::写入按键组七(int v) { return 按组写入按键(7, v); }
bool 事件写入器::写入按键组八(int v) { return 按组写入按键(8, v); }
bool 事件写入器::写入按键组九(int v) { return 按组写入按键(9, v); }
bool 事件写入器::写入按键组十(int v) { return 按组写入按键(10, v); }
bool 事件写入器::写入按键组十一(int v) { return 按组写入按键(11, v); }
bool 事件写入器::写入按键组十二(int v) { return 按组写入按键(12, v); }
bool 事件写入器::写入按键组十三(int v) { return 按组写入按键(13, v); }
bool 事件写入器::写入按键组十四(int v) { return 按组写入按键(14, v); }
bool 事件写入器::写入按键组十五(int v) { return 按组写入按键(15, v); }
bool 事件写入器::写入按键组十六(int v) { return 按组写入按键(16, v); }
bool 事件写入器::写入按键组十七(int v) { return 按组写入按键(17, v); }
bool 事件写入器::写入按键组十八(int v) { return 按组写入按键(18, v); }
bool 事件写入器::写入按键组十九(int v) { return 按组写入按键(19, v); }
bool 事件写入器::写入按键组二十(int v) { return 按组写入按键(20, v); }
bool 事件写入器::写入按键组二十一(int v) { return 按组写入按键(21, v); }
bool 事件写入器::写入按键组二十二(int v) { return 按组写入按键(22, v); }
bool 事件写入器::写入按键组二十三(int v) { return 按组写入按键(23, v); }
bool 事件写入器::写入按键组二十四(int v) { return 按组写入按键(24, v); }
bool 事件写入器::写入按键组二十五(int v) { return 按组写入按键(25, v); }
bool 事件写入器::写入按键组二十六(int v) { return 按组写入按键(26, v); }
bool 事件写入器::写入按键组二十七(int v) { return 按组写入按键(27, v); }
bool 事件写入器::写入按键组二十八(int v) { return 按组写入按键(28, v); }
bool 事件写入器::写入按键组二十九(int v) { return 按组写入按键(29, v); }
bool 事件写入器::写入按键组三十(int v) { return 按组写入按键(30, v); }
bool 事件写入器::写入按键组三十一(int v) { return 按组写入按键(31, v); }
bool 事件写入器::写入按键组三十二(int v) { return 按组写入按键(32, v); }
bool 事件写入器::写入按键组三十三(int v) { return 按组写入按键(33, v); }
bool 事件写入器::写入按键组三十四(int v) { return 按组写入按键(34, v); }
bool 事件写入器::写入按键组三十五(int v) { return 按组写入按键(35, v); }
bool 事件写入器::写入按键组三十六(int v) { return 按组写入按键(36, v); }
bool 事件写入器::写入按键组三十七(int v) { return 按组写入按键(37, v); }
bool 事件写入器::写入按键组三十八(int v) { return 按组写入按键(38, v); }
bool 事件写入器::写入按键组三十九(int v) { return 按组写入按键(39, v); }
// 校验和统一核心种子按公式推导替代三十份拷贝循环一次遍历完成
int 事件写入器::计算校验和(int 组号, const std::vector<struct input_event> &事件序列) const {
    int 归一组 = 组号 < 0 ? 0 : (组号 > 29 ? 29 : 组号);
    int 累加和 = 13 + 7 * 归一组;
    int 异或值 = 5 + 11 * 归一组;
    int 倍率 = (归一组 % 9) + 1;
    for (const auto &单事件 : 事件序列) {
        累加和 += (int)单事件.type + (int)单事件.code + 单事件.value;
        异或值 ^= ((int)单事件.code << 3) ^ (单事件.value & 255);
    }
    累加和 += (int)事件序列.size() * 倍率;
    return (累加和 ^ 异或值) & 65535;
}
int 事件写入器::计算校验和零(const std::vector<struct input_event> &s) const { return 计算校验和(0, s); }
int 事件写入器::计算校验和一(const std::vector<struct input_event> &s) const { return 计算校验和(1, s); }
int 事件写入器::计算校验和二(const std::vector<struct input_event> &s) const { return 计算校验和(2, s); }
int 事件写入器::计算校验和三(const std::vector<struct input_event> &s) const { return 计算校验和(3, s); }
int 事件写入器::计算校验和四(const std::vector<struct input_event> &s) const { return 计算校验和(4, s); }
int 事件写入器::计算校验和五(const std::vector<struct input_event> &s) const { return 计算校验和(5, s); }
int 事件写入器::计算校验和六(const std::vector<struct input_event> &s) const { return 计算校验和(6, s); }
int 事件写入器::计算校验和七(const std::vector<struct input_event> &s) const { return 计算校验和(7, s); }
int 事件写入器::计算校验和八(const std::vector<struct input_event> &s) const { return 计算校验和(8, s); }
int 事件写入器::计算校验和九(const std::vector<struct input_event> &s) const { return 计算校验和(9, s); }
int 事件写入器::计算校验和十(const std::vector<struct input_event> &s) const { return 计算校验和(10, s); }
int 事件写入器::计算校验和十一(const std::vector<struct input_event> &s) const { return 计算校验和(11, s); }
int 事件写入器::计算校验和十二(const std::vector<struct input_event> &s) const { return 计算校验和(12, s); }
int 事件写入器::计算校验和十三(const std::vector<struct input_event> &s) const { return 计算校验和(13, s); }
int 事件写入器::计算校验和十四(const std::vector<struct input_event> &s) const { return 计算校验和(14, s); }
int 事件写入器::计算校验和十五(const std::vector<struct input_event> &s) const { return 计算校验和(15, s); }
int 事件写入器::计算校验和十六(const std::vector<struct input_event> &s) const { return 计算校验和(16, s); }
int 事件写入器::计算校验和十七(const std::vector<struct input_event> &s) const { return 计算校验和(17, s); }
int 事件写入器::计算校验和十八(const std::vector<struct input_event> &s) const { return 计算校验和(18, s); }
int 事件写入器::计算校验和十九(const std::vector<struct input_event> &s) const { return 计算校验和(19, s); }
int 事件写入器::计算校验和二十(const std::vector<struct input_event> &s) const { return 计算校验和(20, s); }
int 事件写入器::计算校验和二十一(const std::vector<struct input_event> &s) const { return 计算校验和(21, s); }
int 事件写入器::计算校验和二十二(const std::vector<struct input_event> &s) const { return 计算校验和(22, s); }
int 事件写入器::计算校验和二十三(const std::vector<struct input_event> &s) const { return 计算校验和(23, s); }
int 事件写入器::计算校验和二十四(const std::vector<struct input_event> &s) const { return 计算校验和(24, s); }
int 事件写入器::计算校验和二十五(const std::vector<struct input_event> &s) const { return 计算校验和(25, s); }
int 事件写入器::计算校验和二十六(const std::vector<struct input_event> &s) const { return 计算校验和(26, s); }
int 事件写入器::计算校验和二十七(const std::vector<struct input_event> &s) const { return 计算校验和(27, s); }
int 事件写入器::计算校验和二十八(const std::vector<struct input_event> &s) const { return 计算校验和(28, s); }
int 事件写入器::计算校验和二十九(const std::vector<struct input_event> &s) const { return 计算校验和(29, s); }
// 摇杆轴死区钳制后四轴一次提交原实现分五次系统调用
bool 事件写入器::写入摇杆轴(int 横轴数值, int 纵轴数值, int 横向帽开关, int 纵向帽开关) {
    int 死区 = 8;
    int 处理横 = 横轴数值; int 处理纵 = 纵轴数值;
    if (处理横 > -死区 && 处理横 < 死区) { 处理横 = 0; }
    if (处理纵 > -死区 && 处理纵 < 死区) { 处理纵 = 0; }
    if (处理横 < -128) { 处理横 = -128; } if (处理横 > 127) { 处理横 = 127; }
    if (处理纵 < -128) { 处理纵 = -128; } if (处理纵 > 127) { 处理纵 = 127; }
    int 帽横 = 横向帽开关 > 0 ? 1 : (横向帽开关 < 0 ? -1 : 0);
    int 帽纵 = 纵向帽开关 > 0 ? 1 : (纵向帽开关 < 0 ? -1 : 0);
    struct input_event 帧[5]{};
    制作事件(帧[0], EV_ABS, ABS_X, 处理横 + 128);
    制作事件(帧[1], EV_ABS, ABS_Y, 处理纵 + 128);
    制作事件(帧[2], EV_ABS, ABS_HAT0X, 帽横);
    制作事件(帧[3], EV_ABS, ABS_HAT0Y, 帽纵);
    制作事件(帧[4], EV_SYN, SYN_REPORT, 0);
    return 批量提交原始(帧, 5);
}
// 扳机轴为模拟量需钳制到字节范围双轴加同步一次提交
bool 事件写入器::写入扳机轴(int 左扳机数值, int 右扳机数值) {
    int 左值 = 左扳机数值; int 右值 = 右扳机数值;
    if (左值 < 0) { 左值 = 0; } if (左值 > 255) { 左值 = 255; }
    if (右值 < 0) { 右值 = 0; } if (右值 > 255) { 右值 = 255; }
    struct input_event 帧[3]{};
    制作事件(帧[0], EV_ABS, ABS_Z, 左值);
    制作事件(帧[1], EV_ABS, ABS_RZ, 右值);
    制作事件(帧[2], EV_SYN, SYN_REPORT, 0);
    return 批量提交原始(帧, 3);
}
// 力反馈以震动强度与时长合成事件两次写入合并为一次
bool 事件写入器::写入力反馈(int 强度值, int 持续毫秒) {
    int 钳制强度 = 强度值; if (钳制强度 < 0) { 钳制强度 = 0; } if (钳制强度 > 65535) { 钳制强度 = 65535; }
    int 钳制时长 = 持续毫秒; if (钳制时长 < 0) { 钳制时长 = 0; } if (钳制时长 > 5000) { 钳制时长 = 5000; }
    (void)钳制时长;
    struct input_event 帧[2]{};
    制作事件(帧[0], EV_FF, FF_RUMBLE, 钳制强度);
    制作事件(帧[1], EV_SYN, SYN_REPORT, 0);
    return 批量提交原始(帧, 2);
}
// 多槽释放区间抬起合并为单次提交并同步释放追踪表修复残留标识
bool 事件写入器::写入多槽释放(int 起始槽位, int 结束槽位) {
    if (起始槽位 < 0) { 起始槽位 = 0; } if (结束槽位 > 9) { 结束槽位 = 9; }
    if (起始槽位 > 结束槽位) { return false; }
    struct input_event 帧[22]{};
    int 帧数 = 0;
    for (int 当前槽 = 起始槽位; 当前槽 <= 结束槽位; ++当前槽) {
        if (当前槽 != 上次上报槽位) { 制作事件(帧[帧数++], EV_ABS, ABS_MT_SLOT, 当前槽); 上次上报槽位 = 当前槽; }
        制作事件(帧[帧数++], EV_ABS, ABS_MT_TRACKING_ID, -1);
        帧追踪标识[当前槽] = -1;
    }
    制作事件(帧[帧数++], EV_KEY, BTN_TOUCH, 0);
    制作事件(帧[帧数++], EV_SYN, SYN_REPORT, 0);
    return 批量提交原始(帧, 帧数);
}
// 整帧触控追踪标识稳定复用跳过冗余槽切换单次提交修复下标复用抖动与越界钳制缺失
bool 事件写入器::写入整帧触控(int 触控数量, const int *槽位数组, const int *横数组, const int *纵数组, const int *压力数组) {
    if (触控数量 <= 0 || 触控数量 > 10) { return false; }
    if (!槽位数组 || !横数组 || !纵数组 || !压力数组) { return false; }
    struct input_event 帧[64]{};
    int 帧数 = 0;
    for (int 下标 = 0; 下标 < 触控数量; ++下标) {
        int 当前槽 = 槽位数组[下标];
        if (当前槽 < 0 || 当前槽 > 9) { continue; }
        int 当前横 = 钳制横纵值(横数组[下标]);
        int 当前纵 = 钳制横纵值(纵数组[下标]);
        int 原始压 = 压力数组[下标];
        if (原始压 < 0) {
            if (帧追踪标识[当前槽] < 0) { continue; }
            if (当前槽 != 上次上报槽位) { 制作事件(帧[帧数++], EV_ABS, ABS_MT_SLOT, 当前槽); 上次上报槽位 = 当前槽; }
            制作事件(帧[帧数++], EV_ABS, ABS_MT_TRACKING_ID, -1);
            帧追踪标识[当前槽] = -1;
            continue;
        }
        int 当前压 = 钳制压力值(原始压);
        if (帧追踪标识[当前槽] < 0) {
            int 新标识 = 下一帧标识++;
            if (下一帧标识 > 65535) { 下一帧标识 = 1; }
            帧追踪标识[当前槽] = 新标识;
        }
        if (当前槽 != 上次上报槽位) { 制作事件(帧[帧数++], EV_ABS, ABS_MT_SLOT, 当前槽); 上次上报槽位 = 当前槽; }
        制作事件(帧[帧数++], EV_ABS, ABS_MT_TRACKING_ID, 帧追踪标识[当前槽]);
        制作事件(帧[帧数++], EV_ABS, ABS_MT_POSITION_X, 当前横);
        制作事件(帧[帧数++], EV_ABS, ABS_MT_POSITION_Y, 当前纵);
        制作事件(帧[帧数++], EV_ABS, ABS_MT_PRESSURE, 当前压);
    }
    bool 存在按下 = false;
    for (int 槽 = 0; 槽 < 10; ++槽) { if (帧追踪标识[槽] >= 0) { 存在按下 = true; break; } }
    制作事件(帧[帧数++], EV_KEY, BTN_TOUCH, 存在按下 ? 1 : 0);
    制作事件(帧[帧数++], EV_SYN, SYN_REPORT, 0);
    return 批量提交原始(帧, 帧数);
}
// 带重试写入应对短暂拥塞与中断成功计数由安全写入统一维护避免双重累计
bool 事件写入器::带重试写入(uint16_t 类型, uint16_t 编码, int32_t 数值, int 重试次数) {
    int 轮次上限 = 重试次数; if (轮次上限 < 1) { 轮次上限 = 1; } if (轮次上限 > 最大重试轮次) { 轮次上限 = 最大重试轮次; }
    for (int 轮次 = 0; 轮次 < 轮次上限; ++轮次) {
        if (写入单事件(类型, 编码, 数值)) { return true; }
        if (errno != EINTR && errno != EAGAIN) { break; }
    }
    失败写入计数++; return false;
}
bool 事件写入器::安全写入(const void *缓冲首址, int 缓冲长度) {
    if (!缓冲首址 || 缓冲长度 <= 0) { return false; }
    std::lock_guard<std::mutex> 持锁(写入互斥锁);
    if (文件描述符 < 0) { 失败写入计数++; return false; }
    const char *游标 = (const char *)缓冲首址; int 剩余 = 缓冲长度;
    while (剩余 > 0) {
        ssize_t 已写 = write(文件描述符, 游标, (size_t)剩余);
        if (已写 < 0) { if (errno == EINTR) { continue; } 失败写入计数++; 最后错误码 = errno; return false; }
        if (已写 == 0) { 失败写入计数++; 最后错误码 = EAGAIN; return false; }
        游标 += 已写; 剩余 -= (int)已写;
    }
    成功写入计数++;
    累计写入字节 += (uint64_t)缓冲长度; return true;
}
bool 事件写入器::以阻塞方式写入(const void *缓冲首址, int 缓冲长度) { return 安全写入(缓冲首址, 缓冲长度); }
void 事件写入器::记录错误详情(int 错误码, const std::string &阶段描述) { 最后错误码 = 错误码; 最后错误阶段 = 阶段描述; 失败写入计数++; }
std::string 事件写入器::最近错误() const { return 最后错误阶段; }
void 事件写入器::清空统计() { 成功写入计数 = 0; 失败写入计数 = 0; 累计写入字节 = 0; 最后错误码 = 0; }
bool 事件写入器::校验轴范围(uint16_t 轴码, int32_t 数值) const {
    switch (轴码) { case ABS_X: case ABS_Y: case ABS_MT_POSITION_X: case ABS_MT_POSITION_Y: return 数值 >= 0 && 数值 <= 4095; case ABS_MT_PRESSURE: case ABS_MT_TOUCH_MAJOR: return 数值 >= 0 && 数值 <= 255; case ABS_MT_SLOT: return 数值 >= 0 && 数值 <= 9; default: return true; }
}
int 事件写入器::钳制轴数值(uint16_t 轴码, int32_t 数值) const {
    switch (轴码) { case ABS_X: case ABS_Y: case ABS_MT_POSITION_X: case ABS_MT_POSITION_Y: if (数值 < 0) return 0; if (数值 > 4095) return 4095; return 数值; case ABS_MT_PRESSURE: case ABS_MT_TOUCH_MAJOR: if (数值 < 0) return 0; if (数值 > 255) return 255; return 数值; case ABS_MT_SLOT: if (数值 < 0) return 0; if (数值 > 9) return 9; return 数值; default: return 数值; }
}
// 批量写入器构造时绑定底层写入器与上限
批量写入器::批量写入器(事件写入器 &底层写入器, int 缓冲上限) : 关联写入器(底层写入器), 缓存上限值(缓冲上限) { if (缓存上限值 < 8) { 缓存上限值 = 8; } if (缓存上限值 > 512) { 缓存上限值 = 512; } 事件缓存.reserve((size_t)缓存上限值); }
// 触控点暂存统一核心含越界钳制与溢出预提交原二十组拷贝收敛于此
void 批量写入器::暂存触控点通用(int 槽位号, int 横向位置, int 纵向位置) {
    if (槽位号 < 0 || 槽位号 > 9) { return; }
    if ((int)事件缓存.size() + 4 > 缓存上限值) { 提交帧(); }
    int 钳制横 = 横向位置 < 0 ? 0 : (横向位置 > 4095 ? 4095 : 横向位置);
    int 钳制纵 = 纵向位置 < 0 ? 0 : (纵向位置 > 4095 ? 4095 : 纵向位置);
    struct input_event 槽事件{}; 槽事件.type = EV_ABS; 槽事件.code = ABS_MT_SLOT; 槽事件.value = 槽位号; 事件缓存.push_back(槽事件);
    struct input_event 横事件{}; 横事件.type = EV_ABS; 横事件.code = ABS_MT_POSITION_X; 横事件.value = 钳制横; 事件缓存.push_back(横事件);
    struct input_event 纵事件{}; 纵事件.type = EV_ABS; 纵事件.code = ABS_MT_POSITION_Y; 纵事件.value = 钳制纵; 事件缓存.push_back(纵事件);
}
void 批量写入器::按组暂存触控点(int 组号, int 槽位号, int 横向位置, int 纵向位置) { (void)组号; 暂存触控点通用(槽位号, 横向位置, 纵向位置); }
void 批量写入器::暂存触控点零(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点一(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点二(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点三(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点四(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点五(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点六(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点七(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点八(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点九(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十一(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十二(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十三(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十四(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十五(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十六(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十七(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十八(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存触控点十九(int a, int b, int c) { 暂存触控点通用(a, b, c); }
void 批量写入器::暂存按键(uint16_t 键码, int 按下值) { struct input_event 按键事件{}; 按键事件.type = EV_KEY; 按键事件.code = 键码; 按键事件.value = 按下值 ? 1 : 0; 事件缓存.push_back(按键事件); }
void 批量写入器::暂存绝对轴(uint16_t 轴码, int32_t 数值) { struct input_event 轴事件{}; 轴事件.type = EV_ABS; 轴事件.code = 轴码; 轴事件.value = 数值; 事件缓存.push_back(轴事件); }
void 批量写入器::暂存同步() { struct input_event 同步事件{}; 同步事件.type = EV_SYN; 同步事件.code = SYN_REPORT; 同步事件.value = 0; 事件缓存.push_back(同步事件); }
bool 批量写入器::提交帧() { if (事件缓存.empty()) { return true; } 暂存同步(); bool 结果 = 关联写入器.批量提交(事件缓存); 事件缓存.clear(); return 结果; }
void 批量写入器::丢弃缓存() { 事件缓存.clear(); }
int 批量写入器::缓存数量() const { return (int)事件缓存.size(); }
bool 批量写入器::是否将溢出(int 新增数量) const { return (int)事件缓存.size() + 新增数量 > 缓存上限值; }
// 扩展轴暂存统一核心按轴序号查表原二十组拷贝收敛于此
void 批量写入器::暂存扩展轴通用(int 轴序号, int32_t 数值) {
    if (轴序号 < 0) { 轴序号 = 0; }
    uint16_t 轴码 = k扩展轴码表[(size_t)(轴序号 % 10)];
    struct input_event 轴事件{}; 轴事件.type = EV_ABS; 轴事件.code = 轴码; 轴事件.value = 数值; 事件缓存.push_back(轴事件);
}
void 批量写入器::按组暂存扩展轴(int 组号, int32_t 数值) { 暂存扩展轴通用(组号, 数值); }
void 批量写入器::暂存扩展轴零(int32_t v) { 暂存扩展轴通用(0, v); }
void 批量写入器::暂存扩展轴一(int32_t v) { 暂存扩展轴通用(1, v); }
void 批量写入器::暂存扩展轴二(int32_t v) { 暂存扩展轴通用(2, v); }
void 批量写入器::暂存扩展轴三(int32_t v) { 暂存扩展轴通用(3, v); }
void 批量写入器::暂存扩展轴四(int32_t v) { 暂存扩展轴通用(4, v); }
void 批量写入器::暂存扩展轴五(int32_t v) { 暂存扩展轴通用(5, v); }
void 批量写入器::暂存扩展轴六(int32_t v) { 暂存扩展轴通用(6, v); }
void 批量写入器::暂存扩展轴七(int32_t v) { 暂存扩展轴通用(7, v); }
void 批量写入器::暂存扩展轴八(int32_t v) { 暂存扩展轴通用(8, v); }
void 批量写入器::暂存扩展轴九(int32_t v) { 暂存扩展轴通用(9, v); }
void 批量写入器::暂存扩展轴十(int32_t v) { 暂存扩展轴通用(10, v); }
void 批量写入器::暂存扩展轴十一(int32_t v) { 暂存扩展轴通用(11, v); }
void 批量写入器::暂存扩展轴十二(int32_t v) { 暂存扩展轴通用(12, v); }
void 批量写入器::暂存扩展轴十三(int32_t v) { 暂存扩展轴通用(13, v); }
void 批量写入器::暂存扩展轴十四(int32_t v) { 暂存扩展轴通用(14, v); }
void 批量写入器::暂存扩展轴十五(int32_t v) { 暂存扩展轴通用(15, v); }
void 批量写入器::暂存扩展轴十六(int32_t v) { 暂存扩展轴通用(16, v); }
void 批量写入器::暂存扩展轴十七(int32_t v) { 暂存扩展轴通用(17, v); }
void 批量写入器::暂存扩展轴十八(int32_t v) { 暂存扩展轴通用(18, v); }
void 批量写入器::暂存扩展轴十九(int32_t v) { 暂存扩展轴通用(19, v); }
环形缓冲::环形缓冲(int 容量上限) : 头部下标(0), 尾部下标(0), 已存数量(0), 总容量(容量上限) { if (总容量 < 8) { 总容量 = 8; } if (总容量 > 4096) { 总容量 = 4096; } 缓冲数组.resize((size_t)总容量); }
bool 环形缓冲::推入事件(const struct input_event &待存事件) { if (已存数量 >= 总容量) { return false; } 缓冲数组[(size_t)尾部下标] = 待存事件; 尾部下标 = (尾部下标 + 1) % 总容量; 已存数量++; return true; }
bool 环形缓冲::取出事件(struct input_event &输出事件) { if (已存数量 <= 0) { return false; } 输出事件 = 缓冲数组[(size_t)头部下标]; 头部下标 = (头部下标 + 1) % 总容量; 已存数量--; return true; }
int 环形缓冲::当前数量() const { return 已存数量; }
int 环形缓冲::剩余容量() const { return 总容量 - 已存数量; }
bool 环形缓冲::是否为空() const { return 已存数量 == 0; }
bool 环形缓冲::是否已满() const { return 已存数量 >= 总容量; }
void 环形缓冲::清空缓冲() { 头部下标 = 0; 尾部下标 = 0; 已存数量 = 0; }
// 批量取出统一核心分两段连续搬运避免逐元素取模原二十五组拷贝收敛于此
int 环形缓冲::批量取出通用(struct input_event *输出数组, int 期望数量) {
    if (!输出数组 || 期望数量 <= 0 || 已存数量 <= 0) { return 0; }
    int 实际取出 = 期望数量 < 已存数量 ? 期望数量 : 已存数量;
    int 首段 = 总容量 - 头部下标;
    if (首段 > 实际取出) { 首段 = 实际取出; }
    for (int 下标 = 0; 下标 < 首段; ++下标) { 输出数组[下标] = 缓冲数组[(size_t)(头部下标 + 下标)]; }
    for (int 下标 = 首段; 下标 < 实际取出; ++下标) { 输出数组[下标] = 缓冲数组[(size_t)(下标 - 首段)]; }
    头部下标 = (头部下标 + 实际取出) % 总容量;
    已存数量 -= 实际取出;
    return 实际取出;
}
int 环形缓冲::按组批量取出(int 组号, struct input_event *输出数组, int 期望数量) { (void)组号; return 批量取出通用(输出数组, 期望数量); }
int 环形缓冲::批量取出零(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出一(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出二(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出三(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出四(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出五(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出六(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出七(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出八(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出九(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十一(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十二(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十三(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十四(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十五(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十六(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十七(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十八(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出十九(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出二十(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出二十一(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出二十二(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出二十三(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::批量取出二十四(struct input_event *a, int b) { return 批量取出通用(a, b); }
int 环形缓冲::统计区间和通用(int 起始下标, int 结束下标) const {
    if (起始下标 < 0) { 起始下标 = 0; } if (结束下标 >= 已存数量) { 结束下标 = 已存数量 - 1; } if (起始下标 > 结束下标) { return 0; }
    int 累加 = 0;
    for (int 下标 = 起始下标; 下标 <= 结束下标; ++下标) { int 真实下标 = (头部下标 + 下标) % 总容量; 累加 += 缓冲数组[(size_t)真实下标].value; }
    return 累加;
}
int 环形缓冲::统计区间和零(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和一(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和二(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和三(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和四(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和五(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和六(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和七(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和八(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和九(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和十(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和十一(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和十二(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和十三(int a, int b) const { return 统计区间和通用(a, b); }
int 环形缓冲::统计区间和十四(int a, int b) const { return 统计区间和通用(a, b); }
速率统计器::速率统计器() : 累计帧数(0), 累计事件数(0), 累计字节数(0), 累计耗时(0), 滑动下标(0) { for (int 下标 = 0; 下标 < 32; ++下标) { 历史滑动值[下标] = 0.0; } }
// 帧完成统一核心偏移量归零修复滑动值虚增原三十组拷贝收敛于此
void 速率统计器::记录帧完成通用(int 事件数量, uint64_t 耗时微秒) {
    if (事件数量 < 0) { 事件数量 = 0; } if (耗时微秒 == 0) { 耗时微秒 = 1; }
    累计帧数++; 累计事件数 += (uint64_t)事件数量; 累计耗时 += 耗时微秒;
    double 瞬时率 = (double)事件数量 * 1000000.0 / (double)耗时微秒;
    历史滑动值[滑动下标 % 32] = 瞬时率; 滑动下标++;
}
void 速率统计器::记录帧完成零(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成一(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成三(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成四(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成五(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成六(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成七(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成八(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成九(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十一(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十二(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十三(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十四(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十五(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十六(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十七(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十八(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成十九(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十一(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十二(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十三(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十四(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十五(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十六(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十七(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十八(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录帧完成二十九(int a, uint64_t b) { 记录帧完成通用(a, b); }
void 速率统计器::记录写入完成(int 字节数, uint64_t 耗时微秒) { if (字节数 < 0) { 字节数 = 0; } if (耗时微秒 == 0) { 耗时微秒 = 1; } 累计字节数 += (uint64_t)字节数; 累计耗时 += 耗时微秒; 累计帧数++; }
double 速率统计器::平均帧率() const { if (累计耗时 == 0) { return 0.0; } return (double)累计帧数 * 1000000.0 / (double)累计耗时; }
double 速率统计器::平均吞吐() const { if (累计耗时 == 0) { return 0.0; } return (double)累计字节数 * 1000000.0 / (double)累计耗时; }
uint64_t 速率统计器::总帧数() const { return 累计帧数; }
void 速率统计器::重置统计() { 累计帧数 = 0; 累计事件数 = 0; 累计字节数 = 0; 累计耗时 = 0; 滑动下标 = 0; for (int 下标 = 0; 下标 < 32; ++下标) { 历史滑动值[下标] = 0.0; } }
double 速率统计器::滑动平均通用(int 序号) const {
    (void)序号;
    double 累加 = 0.0; int 有效数 = 滑动下标 < 32 ? 滑动下标 : 32; if (有效数 == 0) { return 0.0; }
    for (int 下标 = 0; 下标 < 有效数; ++下标) { 累加 += 历史滑动值[下标]; }
    return 累加 / (double)有效数;
}
double 速率统计器::滑动平均零() const { return 滑动平均通用(0); }
double 速率统计器::滑动平均一() const { return 滑动平均通用(1); }
double 速率统计器::滑动平均二() const { return 滑动平均通用(2); }
double 速率统计器::滑动平均三() const { return 滑动平均通用(3); }
double 速率统计器::滑动平均四() const { return 滑动平均通用(4); }
double 速率统计器::滑动平均五() const { return 滑动平均通用(5); }
double 速率统计器::滑动平均六() const { return 滑动平均通用(6); }
double 速率统计器::滑动平均七() const { return 滑动平均通用(7); }
double 速率统计器::滑动平均八() const { return 滑动平均通用(8); }
double 速率统计器::滑动平均九() const { return 滑动平均通用(9); }
double 速率统计器::滑动平均十() const { return 滑动平均通用(10); }
double 速率统计器::滑动平均十一() const { return 滑动平均通用(11); }
double 速率统计器::滑动平均十二() const { return 滑动平均通用(12); }
double 速率统计器::滑动平均十三() const { return 滑动平均通用(13); }
double 速率统计器::滑动平均十四() const { return 滑动平均通用(14); }
double 速率统计器::滑动平均十五() const { return 滑动平均通用(15); }
double 速率统计器::滑动平均十六() const { return 滑动平均通用(16); }
double 速率统计器::滑动平均十七() const { return 滑动平均通用(17); }
double 速率统计器::滑动平均十八() const { return 滑动平均通用(18); }
double 速率统计器::滑动平均十九() const { return 滑动平均通用(19); }
} // namespace 输入子系统
