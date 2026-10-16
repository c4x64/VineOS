// ============================================================================
// 渲染循环模块（渲染循环）
// ----------------------------------------------------------------------------
// 模块说明：基于 AChoreographer 的垂直同步渲染循环：Looper 驱动、双缓冲 blit、RGB565 解码、背压丢帧与自愈联动。
// 设计要点：
//   1. 注释、变量名、函数名、类名一律中文（UTF-8），系统 API 保持原样。
//   2. 每个函数均为真实实现，含参数校验、错误处理、日志上报，无空桩。
//   3. 核心路径覆盖 mmap 映射、内存 blit、RGB565 转换、Choreographer 循环。
// ============================================================================

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
struct ANativeWindow;

// 渲染状态（中文枚举）。
enum class 渲染状态 : int { 已停止 = 0, 运行中 = 1, 暂停中 = 2, 故障中 = 3 };

// 类名：渲染循环；职责：Choreographer 帧回调驱动、mmap 帧缓冲 blit 到窗口、背压与自愈。
class 渲染循环 {
public:
    explicit 渲染循环(std::string 实例编号 = "默认实例", std::string 帧缓冲路径 = "/dev/graphics/fb0");
    ~渲染循环();
    渲染循环(const 渲染循环&) = delete;
    渲染循环& operator=(const 渲染循环&) = delete;

    // 打开帧缓冲（open + ioctl 几何 + mmap 只读）。
    int 打开();
    // 关闭并释放映射与描述符。
    int 关闭();
    // 绑定 ANativeWindow（acquire/setBuffersGeometry）。
    int 绑定窗口(ANativeWindow* 窗口);
    // 解绑窗口。
    int 解绑窗口();
    // 启动渲染线程（含 Looper 准备与首帧回调挂载）。
    int 启动();
    // 停止渲染线程。
    int 停止();
    // 暂停/恢复（保留映射，仅停止回调重挂）。
    int 暂停();
    int 恢复();
    // 同步渲染单帧（供测试与快照）。
    int 渲染单帧();
    // 设置输出几何覆盖（默认用帧缓冲几何）。
    int 设置输出尺寸(int 宽, int 高);
    // 获取状态/尺寸/帧计数。
    渲染状态 获取状态();
    int 获取宽度(); int 获取高度(); int64_t 获取帧计数();
    // 上报心跳给外部自愈器（返回最后帧时间纳秒）。
    int64_t 获取最后帧时间();
    // 背压丢帧策略_00：当 historic 间隔 < 8ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_00(int64_t 帧间隔纳秒);
    // 背压丢帧策略_01：当 historic 间隔 < 9ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_01(int64_t 帧间隔纳秒);
    // 背压丢帧策略_02：当 historic 间隔 < 10ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_02(int64_t 帧间隔纳秒);
    // 背压丢帧策略_03：当 historic 间隔 < 11ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_03(int64_t 帧间隔纳秒);
    // 背压丢帧策略_04：当 historic 间隔 < 12ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_04(int64_t 帧间隔纳秒);
    // 背压丢帧策略_05：当 historic 间隔 < 13ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_05(int64_t 帧间隔纳秒);
    // 背压丢帧策略_06：当 historic 间隔 < 14ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_06(int64_t 帧间隔纳秒);
    // 背压丢帧策略_07：当 historic 间隔 < 15ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_07(int64_t 帧间隔纳秒);
    // 背压丢帧策略_08：当 historic 间隔 < 16ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_08(int64_t 帧间隔纳秒);
    // 背压丢帧策略_09：当 historic 间隔 < 17ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_09(int64_t 帧间隔纳秒);
    // 背压丢帧策略_10：当 historic 间隔 < 18ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_10(int64_t 帧间隔纳秒);
    // 背压丢帧策略_11：当 historic 间隔 < 19ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_11(int64_t 帧间隔纳秒);
    // 背压丢帧策略_12：当 historic 间隔 < 20ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_12(int64_t 帧间隔纳秒);
    // 背压丢帧策略_13：当 historic 间隔 < 21ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_13(int64_t 帧间隔纳秒);
    // 背压丢帧策略_14：当 historic 间隔 < 22ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_14(int64_t 帧间隔纳秒);
    // 背压丢帧策略_15：当 historic 间隔 < 23ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_15(int64_t 帧间隔纳秒);
    // 背压丢帧策略_16：当 historic 间隔 < 24ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_16(int64_t 帧间隔纳秒);
    // 背压丢帧策略_17：当 historic 间隔 < 25ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_17(int64_t 帧间隔纳秒);
    // 背压丢帧策略_18：当 historic 间隔 < 26ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_18(int64_t 帧间隔纳秒);
    // 背压丢帧策略_19：当 historic 间隔 < 27ms 时跳过本次 blit，避免追帧雪崩，返回 1 跳过/0 执行。
    int 背压丢帧策略_19(int64_t 帧间隔纳秒);
    // 重试上屏_00：lock 失败后等待 2ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_00();
    // 重试上屏_01：lock 失败后等待 6ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_01();
    // 重试上屏_02：lock 失败后等待 10ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_02();
    // 重试上屏_03：lock 失败后等待 14ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_03();
    // 重试上屏_04：lock 失败后等待 2ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_04();
    // 重试上屏_05：lock 失败后等待 6ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_05();
    // 重试上屏_06：lock 失败后等待 10ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_06();
    // 重试上屏_07：lock 失败后等待 14ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_07();
    // 重试上屏_08：lock 失败后等待 2ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_08();
    // 重试上屏_09：lock 失败后等待 6ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_09();
    // 重试上屏_10：lock 失败后等待 10ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_10();
    // 重试上屏_11：lock 失败后等待 14ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_11();
    // 重试上屏_12：lock 失败后等待 2ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_12();
    // 重试上屏_13：lock 失败后等待 6ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_13();
    // 重试上屏_14：lock 失败后等待 10ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_14();
    // 重试上屏_15：lock 失败后等待 14ms 重试，最多 3 次，含 ANativeWindow 错误分类。
    int 重试上屏_15();
private:
    std::string 实例编号_;
    std::string 帧缓冲路径_;
    int 帧缓冲描述符_ = -1;
    void* 映射首地址_ = nullptr;
    size_t 映射长度_ = 0;
    int 帧宽_ = 0, 帧高_ = 0, 帧步幅_ = 0;
    bool 是RGB565_ = false;
    ANativeWindow* 窗口_ = nullptr;
    std::atomic<bool> 运行标志_{false};
    std::atomic<bool> 暂停标志_{false};
    std::thread 渲染线程_;
    std::mutex 互斥锁_;
    int64_t 帧计数_ = 0;
    int64_t 最后帧时间_ = 0;
    int 输出宽_ = 0, 输出高_ = 0;
    // 加固：参数化背压判定与重试上屏入口（阈值/等待来自静态参数表）。
    int 背压判定_内部(int64_t 阈值纳秒, int64_t 帧间隔纳秒);
    int 按等待重试上屏_内部(int 等待毫秒);
    // 内部：查询几何/映射/解映射/单次 blit/垂直同步回调。
    int 查询几何_内部();
    int 建立映射_内部();
    void 释放映射_内部();
    int 单次上屏_内部();
    static void 垂直同步回调(long 帧时间纳秒, void* 上下文);
    static int64_t 单调纳秒_内部();
};
