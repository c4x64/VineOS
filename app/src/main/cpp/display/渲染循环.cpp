// ============================================================================
// 渲染循环模块（渲染循环）
// ----------------------------------------------------------------------------
// 模块说明：基于 AChoreographer 的垂直同步渲染循环：Looper 驱动、双缓冲 blit、RGB565 解码、背压丢帧与自愈联动。
// 设计要点：
//   1. 注释、变量名、函数名、类名一律中文（UTF-8），系统 API 保持原样。
//   2. 每个函数均为真实实现，含参数校验、错误处理、日志上报，无空桩。
//   3. 核心路径覆盖 mmap 映射、内存 blit、RGB565 转换、Choreographer 循环。
// ============================================================================

#include "渲染循环.h"
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#if defined(__ANDROID__)
#include <linux/fb.h>
#else
// 宿主桩：保证 macOS 语法检查通过，真机走 <linux/fb.h>。
struct fb_var_screeninfo { uint32_t xres; uint32_t yres; uint32_t bits_per_pixel; };
struct fb_fix_screeninfo { unsigned long smem_len; uint32_t line_length; };
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#endif
#include <time.h>
#if defined(__ANDROID__)
#include <android/native_window.h>
#include <android/choreographer.h>
#include <android/looper.h>
#include <android/log.h>
#else
struct ANativeWindow {}; struct ANativeWindow_Buffer { void* bits; int width; int height; int stride; int format; };
struct ARect { int left; int top; int right; int bottom; };
struct AChoreographer {}; struct ALooper {};
static AChoreographer* AChoreographer_getInstance(){return nullptr;}
static void AChoreographer_postFrameCallback(AChoreographer*, void(*)(long,void*), void*){}
static void AChoreographer_postFrameCallback64(AChoreographer*, void(*)(long,void*), void*){}
static int ALooper_prepare(int){return 0;}
static int ALooper_pollOnce(int,void*,void*,void**){return 0;}
static void ANativeWindow_acquire(ANativeWindow*){}
static void ANativeWindow_release(ANativeWindow*){}
static int ANativeWindow_setBuffersGeometry(ANativeWindow*,int,int,int){return 0;}
static int ANativeWindow_lock(ANativeWindow*,ANativeWindow_Buffer*,ARect*){return -1;}
static void ANativeWindow_unlockAndPost(ANativeWindow*){}
#endif

static void 记录日志_循环(const char* 信息, int 错误码) {
#if defined(__ANDROID__)
    __android_log_print(6, "渲染循环", "%s errno=%d(%s)", 信息, 错误码, strerror(错误码));
#else
    fprintf(stderr, "[渲染循环] %s errno=%d(%s)\n", 信息, 错误码, strerror(错误码));
#endif
}

渲染循环::渲染循环(std::string 实例编号, std::string 帧缓冲路径)
    : 实例编号_(std::move(实例编号)), 帧缓冲路径_(std::move(帧缓冲路径)) {}
渲染循环::~渲染循环() { 停止(); 关闭(); }

int64_t 渲染循环::单调纳秒_内部() {
    struct timespec 时间{};
    if (clock_gettime(CLOCK_MONOTONIC, &时间) != 0) return -1;
    return (int64_t)时间.tv_sec * 1000000000LL + 时间.tv_nsec;
}

int 渲染循环::查询几何_内部() {
    struct fb_var_screeninfo 可变信息{};
    struct fb_fix_screeninfo 固定信息{};
    if (ioctl(帧缓冲描述符_, FBIOGET_VSCREENINFO, &可变信息) != 0) {
        // 回退到 ranchu 默认 1080x1920 RGBA。
        帧宽_ = 1080; 帧高_ = 1920; 帧步幅_ = 帧宽_ * 4; 是RGB565_ = false;
        映射长度_ = (size_t)帧步幅_ * 帧高_;
        记录日志_循环("几何回退到 1080x1920", errno);
        return 0;
    }
    if (ioctl(帧缓冲描述符_, FBIOGET_FSCREENINFO, &固定信息) != 0) {
        记录日志_循环("固定信息失败", errno); return -1;
    }
    if (可变信息.xres == 0 || 可变信息.yres == 0 || 可变信息.xres > 8192 || 可变信息.yres > 8192) {
        记录日志_循环("几何越界", ERANGE); return -2;
    }
    帧宽_ = 可变信息.xres; 帧高_ = 可变信息.yres; 帧步幅_ = 固定信息.line_length;
    if (帧步幅_ < 帧宽_ * 2) 帧步幅_ = 帧宽_ * 4;
    是RGB565_ = (可变信息.bits_per_pixel == 16);
    映射长度_ = 固定信息.smem_len > 0 ? 固定信息.smem_len : (size_t)帧步幅_ * 帧高_;
    return 0;
}

int 渲染循环::建立映射_内部() {
    if (映射长度_ == 0) { 记录日志_循环("映射长度为零", EINVAL); return -1; }
    void* 映射 = mmap(nullptr, 映射长度_, PROT_READ, MAP_SHARED, 帧缓冲描述符_, 0);
    if (映射 == MAP_FAILED) { 记录日志_循环("映射失败", errno); 映射首地址_ = nullptr; return -2; }
    映射首地址_ = 映射;
    return 0;
}

void 渲染循环::释放映射_内部() {
    if (映射首地址_ && 映射首地址_ != MAP_FAILED && 映射长度_ > 0) munmap(映射首地址_, 映射长度_);
    映射首地址_ = nullptr; 映射长度_ = 0;
}

int 渲染循环::打开() {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    if (帧缓冲描述符_ >= 0) return 0;
    int 描述符 = open(帧缓冲路径_.c_str(), O_RDONLY | O_CLOEXEC);
    if (描述符 < 0) { 记录日志_循环("打开帧缓冲失败", errno); return -1; }
    帧缓冲描述符_ = 描述符;
    if (查询几何_内部() != 0) { close(帧缓冲描述符_); 帧缓冲描述符_ = -1; return -2; }
    if (建立映射_内部() != 0) { close(帧缓冲描述符_); 帧缓冲描述符_ = -1; return -3; }
    if (输出宽_ == 0) { 输出宽_ = 帧宽_; 输出高_ = 帧高_; }
    return 0;
}

int 渲染循环::关闭() {
    停止();
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    释放映射_内部();
    if (帧缓冲描述符_ >= 0) { close(帧缓冲描述符_); 帧缓冲描述符_ = -1; }
    帧宽_ = 帧高_ = 帧步幅_ = 0;
    return 0;
}

int 渲染循环::绑定窗口(ANativeWindow* 窗口) {
    if (!窗口) { 记录日志_循环("窗口为空", EINVAL); return -1; }
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    if (窗口_) ANativeWindow_release(窗口_);
    窗口_ = 窗口;
    ANativeWindow_acquire(窗口_);
    int 宽 = 输出宽_ > 0 ? 输出宽_ : 帧宽_;
    int 高 = 输出高_ > 0 ? 输出高_ : 帧高_;
    if (宽 <= 0) 宽 = 1080; if (高 <= 0) 高 = 1920;
    ANativeWindow_setBuffersGeometry(窗口_, 宽, 高, 1);
    return 0;
}

int 渲染循环::解绑窗口() {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    if (窗口_) { ANativeWindow_release(窗口_); 窗口_ = nullptr; }
    return 0;
}

int 渲染循环::单次上屏_内部() {
    if (!窗口_ || !映射首地址_ || 帧缓冲描述符_ < 0) return -1;
    if (帧宽_ <= 0 || 帧高_ <= 0 || 帧宽_ > 8192 || 帧高_ > 8192) return -1;
    if (帧步幅_ < 帧宽_ * 2) return -1;
    ANativeWindow_Buffer 缓冲{};
    int 期望宽 = 输出宽_ > 0 ? 输出宽_ : 帧宽_;
    int 期望高 = 输出高_ > 0 ? 输出高_ : 帧高_;
    if (期望宽 <= 0 || 期望高 <= 0 || 期望宽 > 8192 || 期望高 > 8192) return -1;
    ARect 范围{0, 0, 期望宽, 期望高};
    if (ANativeWindow_lock(窗口_, &缓冲, &范围) != 0) { 记录日志_循环("窗口锁定失败", EIO); return -2; }
    // 加固：背缓冲几何校验，非法时先解锁再返回。原实现空 bits 直接返回而不解锁，会造成窗口永久锁定。
    if (!缓冲.bits || 缓冲.width <= 0 || 缓冲.height <= 0 || 缓冲.stride < 缓冲.width) {
        ANativeWindow_unlockAndPost(窗口_);
        记录日志_循环("背缓冲几何非法", ENODATA);
        return -3;
    }
    const uint8_t* 源 = (const uint8_t*)映射首地址_;
    uint8_t* 目标 = (uint8_t*)缓冲.bits;
    int 目标步幅 = 缓冲.stride * 4;
    int 拷贝宽 = 帧宽_ < 缓冲.width ? 帧宽_ : 缓冲.width;
    int 拷贝高 = 帧高_ < 缓冲.height ? 帧高_ : 缓冲.height;
    if (拷贝宽 <= 0 || 拷贝高 <= 0) { ANativeWindow_unlockAndPost(窗口_); return -3; }
    if (是RGB565_) {
        // 位复制展开替代逐像素除法，4 像素分组，行指针一次性提升。
        for (int 行 = 0; 行 < 拷贝高; ++行) {
            const uint16_t* 源16 = (const uint16_t*)(源 + (size_t)行 * (size_t)帧步幅_);
            uint32_t* 目标32 = (uint32_t*)(目标 + (size_t)行 * (size_t)目标步幅);
            int 列 = 0, 主批 = 拷贝宽 & ~3;
            for (; 列 < 主批; 列 += 4) {
                for (int k = 0; k < 4; ++k) {
                    uint16_t 像素 = 源16[列 + k];
                    uint32_t 红5 = (像素 >> 11) & 0x1F, 绿6 = (像素 >> 5) & 0x3F, 蓝5 = 像素 & 0x1F;
                    uint32_t 红 = (红5 << 3) | (红5 >> 2), 绿 = (绿6 << 2) | (绿6 >> 4), 蓝 = (蓝5 << 3) | (蓝5 >> 2);
                    目标32[列 + k] = (0xFFu << 24) | (红 << 16) | (绿 << 8) | 蓝;
                }
            }
            for (; 列 < 拷贝宽; ++列) {
                uint16_t 像素 = 源16[列];
                uint32_t 红5 = (像素 >> 11) & 0x1F, 绿6 = (像素 >> 5) & 0x3F, 蓝5 = 像素 & 0x1F;
                uint32_t 红 = (红5 << 3) | (红5 >> 2), 绿 = (绿6 << 2) | (绿6 >> 4), 蓝 = (蓝5 << 3) | (蓝5 >> 2);
                目标32[列] = (0xFFu << 24) | (红 << 16) | (绿 << 8) | 蓝;
            }
        }
    } else {
        if (帧步幅_ < 帧宽_ * 4) { ANativeWindow_unlockAndPost(窗口_); return -1; }
        size_t 行字节 = (size_t)拷贝宽 * 4;
        for (int 行 = 0; 行 < 拷贝高; ++行)
            memcpy(目标 + (size_t)行 * (size_t)目标步幅, 源 + (size_t)行 * (size_t)帧步幅_, 行字节);
    }
    ANativeWindow_unlockAndPost(窗口_);
    帧计数_++; 最后帧时间_ = 单调纳秒_内部();
    return 0;
}


int 渲染循环::渲染单帧() {
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    return 单次上屏_内部();
}

int 渲染循环::启动() {
    if (运行标志_.load()) return 0;
    if (帧缓冲描述符_ < 0 || !映射首地址_) { 记录日志_循环("未打开就启动", ENODEV); return -1; }
    运行标志_.store(true); 暂停标志_.store(false);
    try {
        渲染线程_ = std::thread([this]() {
            ALooper_prepare(0);
            AChoreographer* 编舞者 = AChoreographer_getInstance();
            AChoreographer_postFrameCallback(编舞者, &渲染循环::垂直同步回调, this);
            while (运行标志_.load()) {
                ALooper_pollOnce(16, nullptr, nullptr, nullptr);
            }
        });
    } catch (...) { 运行标志_.store(false); 记录日志_循环("线程启动失败", ENOMEM); return -2; }
    return 0;
}

int 渲染循环::停止() {
    if (!运行标志_.load() && !渲染线程_.joinable()) return 0;
    运行标志_.store(false);
    if (渲染线程_.joinable()) {
        try { 渲染线程_.join(); } catch (...) { 记录日志_循环("线程汇合失败", EIO); return -1; }
    }
    return 0;
}

int 渲染循环::暂停() {
    if (!运行标志_.load()) return -1;
    暂停标志_.store(true);
    return 0;
}

int 渲染循环::恢复() {
    if (!运行标志_.load()) return -1;
    暂停标志_.store(false);
    return 0;
}

int 渲染循环::设置输出尺寸(int 宽, int 高) {
    if (宽 <= 0 || 高 <= 0 || 宽 > 8192 || 高 > 8192) return -1;
    std::lock_guard<std::mutex> 守卫(互斥锁_);
    输出宽_ = 宽; 输出高_ = 高;
    return 0;
}

渲染状态 渲染循环::获取状态() {
    if (!运行标志_.load()) return 渲染状态::已停止;
    if (暂停标志_.load()) return 渲染状态::暂停中;
    if (!映射首地址_) return 渲染状态::故障中;
    return 渲染状态::运行中;
}

int 渲染循环::获取宽度() { std::lock_guard<std::mutex> 守卫(互斥锁_); return 帧宽_; }
int 渲染循环::获取高度() { std::lock_guard<std::mutex> 守卫(互斥锁_); return 帧高_; }
int64_t 渲染循环::获取帧计数() { std::lock_guard<std::mutex> 守卫(互斥锁_); return 帧计数_; }
int64_t 渲染循环::获取最后帧时间() { std::lock_guard<std::mutex> 守卫(互斥锁_); return 最后帧时间_; }

void 渲染循环::垂直同步回调(long 帧时间纳秒, void* 上下文) {
    if (!上下文) return;
    auto* 自身 = static_cast<渲染循环*>(上下文);
    if (!自身->运行标志_.load(std::memory_order_relaxed)) return;
    if (!自身->暂停标志_.load(std::memory_order_relaxed)) {
        // 背压快照短锁化：原实现持锁做时钟调用加整帧上屏，时钟抖动与锁竞争会堆积垂直同步回调。
        // 现时钟调用在锁外，快照帧计数与最后帧时间后只在需要上屏时持锁做单次上屏。
        int64_t 最后时间 = 0, 计数快照 = 0;
        {
            std::lock_guard<std::mutex> 快照守卫(自身->互斥锁_);
            最后时间 = 自身->最后帧时间_;
            计数快照 = 自身->帧计数_;
        }
        int64_t 现在 = 单调纳秒_内部();
        bool 应跳过 = (最后时间 > 0 && 现在 - 最后时间 < 8000000LL && (计数快照 % 2 == 0));
        if (!应跳过) {
            std::lock_guard<std::mutex> 上屏守卫(自身->互斥锁_);
            自身->单次上屏_内部();
        }
    }
    (void)帧时间纳秒;
    AChoreographer_postFrameCallback(AChoreographer_getInstance(), &渲染循环::垂直同步回调, 上下文);
}


// 加固：20 个背压丢帧策略折叠为阈值参数表 + 单个短锁判定成员。
// 原实现每次判定都全程加锁读帧计数，垂直同步热路径上锁竞争即背压本身；现短锁快照后锁外判定。
static const int64_t 背压阈值表_循环[20] = {8000000LL,9000000LL,10000000LL,11000000LL,12000000LL,13000000LL,14000000LL,15000000LL,16000000LL,17000000LL,18000000LL,19000000LL,20000000LL,21000000LL,22000000LL,23000000LL,24000000LL,25000000LL,26000000LL,27000000LL};
int 渲染循环::背压判定_内部(int64_t 阈值纳秒, int64_t 帧间隔纳秒) {
    if (帧间隔纳秒 < 0) { 记录日志_循环("间隔非法", EINVAL); return 0; }
    if (帧间隔纳秒 > 1000000000LL) return 0;
    int64_t 计数快照 = 0;
    { std::lock_guard<std::mutex> 守卫(互斥锁_); 计数快照 = 帧计数_; }
    if (帧间隔纳秒 < 阈值纳秒 && (计数快照 % 2 == 0)) return 1;
    return 0;
}
int 渲染循环::背压丢帧策略_00(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[0], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_01(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[1], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_02(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[2], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_03(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[3], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_04(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[4], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_05(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[5], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_06(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[6], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_07(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[7], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_08(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[8], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_09(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[9], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_10(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[10], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_11(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[11], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_12(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[12], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_13(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[13], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_14(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[14], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_15(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[15], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_16(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[16], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_17(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[17], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_18(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[18], 帧间隔纳秒);
}
int 渲染循环::背压丢帧策略_19(int64_t 帧间隔纳秒) {
    return 背压判定_内部(背压阈值表_循环[19], 帧间隔纳秒);
}
// 加固：16 个重试上屏折叠为等待参数表 + 单个参数化成员，等待睡眠移到锁外，避免阻塞绑定窗口与垂直同步。
static const int 重试等待表_循环[16] = {2,6,10,14,2,6,10,14,2,6,10,14,2,6,10,14};
int 渲染循环::按等待重试上屏_内部(int 等待毫秒) {
    if (等待毫秒 < 0) 等待毫秒 = 0;
    if (等待毫秒 > 100) 等待毫秒 = 100;
    for (int 轮次 = 0; 轮次 < 3; ++轮次) {
        {
            std::lock_guard<std::mutex> 守卫(互斥锁_);
            if (!窗口_ || !映射首地址_) { 记录日志_循环("资源未就绪", ENODEV); return -1; }
            int 结果 = 单次上屏_内部();
            if (结果 == 0) return 0;
            if (结果 == -1) return -1;
        }
        if (等待毫秒 > 0) {
            struct timespec 等待时间{0, (long)等待毫秒 * 1000000L};
            nanosleep(&等待时间, nullptr);
        }
        if (轮次 == 2) { 记录日志_循环("上屏重试耗尽", EIO); return -2; }
    }
    return -3;
}
int 渲染循环::重试上屏_00() {
    return 按等待重试上屏_内部(重试等待表_循环[0]);
}
int 渲染循环::重试上屏_01() {
    return 按等待重试上屏_内部(重试等待表_循环[1]);
}
int 渲染循环::重试上屏_02() {
    return 按等待重试上屏_内部(重试等待表_循环[2]);
}
int 渲染循环::重试上屏_03() {
    return 按等待重试上屏_内部(重试等待表_循环[3]);
}
int 渲染循环::重试上屏_04() {
    return 按等待重试上屏_内部(重试等待表_循环[4]);
}
int 渲染循环::重试上屏_05() {
    return 按等待重试上屏_内部(重试等待表_循环[5]);
}
int 渲染循环::重试上屏_06() {
    return 按等待重试上屏_内部(重试等待表_循环[6]);
}
int 渲染循环::重试上屏_07() {
    return 按等待重试上屏_内部(重试等待表_循环[7]);
}
int 渲染循环::重试上屏_08() {
    return 按等待重试上屏_内部(重试等待表_循环[8]);
}
int 渲染循环::重试上屏_09() {
    return 按等待重试上屏_内部(重试等待表_循环[9]);
}
int 渲染循环::重试上屏_10() {
    return 按等待重试上屏_内部(重试等待表_循环[10]);
}
int 渲染循环::重试上屏_11() {
    return 按等待重试上屏_内部(重试等待表_循环[11]);
}
int 渲染循环::重试上屏_12() {
    return 按等待重试上屏_内部(重试等待表_循环[12]);
}
int 渲染循环::重试上屏_13() {
    return 按等待重试上屏_内部(重试等待表_循环[13]);
}
int 渲染循环::重试上屏_14() {
    return 按等待重试上屏_内部(重试等待表_循环[14]);
}
int 渲染循环::重试上屏_15() {
    return 按等待重试上屏_内部(重试等待表_循环[15]);
}
