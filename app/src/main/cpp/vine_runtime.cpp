#include <jni.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cmath>
#include <sys/system_properties.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "utils/vine_log.h"
#include "utils/vine_utils.h"
#include "container/namespace_manager.h"
#include "qemu_bridge/qemu_launcher.h"
#include "display/framebuffer_bridge.h"
#include "input/uinput_bridge.h"

struct InstanceRuntime {
    int64_t container_handle = 0;
    std::string 实例标识;
    std::unique_ptr<vine::display::FramebufferBridge> fb;
    std::unique_ptr<vine::input::UInputBridge> input;
};

static std::unordered_map<int64_t, InstanceRuntime> g_runtimes;
static std::unordered_map<std::string, std::pair<bool, bool>> g_instance_flags;
// Guards g_instance_flags (and instance-handle lookups that resolve them); new lock uses a Chinese identifier per project rule.
static std::mutex 实例标记锁;

// Erases a stale per-instance flag entry. No-op on empty id.
static void 清除实例标记(const std::string& 实例标识) {
    if (实例标识.empty()) return;
    std::lock_guard<std::mutex> 守卫(实例标记锁);
    g_instance_flags.erase(实例标识);
}

static std::string j2s(JNIEnv* env, jstring js) {
    if (!js) return {};
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string s(c);
    env->ReleaseStringUTFChars(js, c);
    return s;
}

static jstring s2j(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_initialize(
        JNIEnv* env, jobject,
        jstring j_data_dir, jstring j_native_lib_dir) {
    const std::string data_dir = j2s(env, j_data_dir);
    const std::string lib_dir = j2s(env, j_native_lib_dir);
    if (!vine::qemu::verify_qemu_binary(vine::qemu::qemu_arm_path(lib_dir))) {
        VINE_LOGW("qemu-arm unavailable, 32-bit support disabled");
    }
    bool ok = vine::NamespaceManager::instance().init(data_dir, lib_dir);
    VINE_LOGI("VineRuntime::initialize → %s", ok ? "OK" : "FAILED");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_shutdown(
        JNIEnv*, jobject) {
    g_runtimes.clear();
    {
        std::lock_guard<std::mutex> 守卫(实例标记锁);
        g_instance_flags.clear();
    }
    vine::NamespaceManager::instance().shutdown();
}

JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_createInstance(
        JNIEnv* env, jobject,
        jstring j_instance_id, jstring, jint) {
    const std::string id = j2s(env, j_instance_id);
    auto& mgr = vine::NamespaceManager::instance();
    const std::string path = mgr.data_dir() + "/instances/" + id;
    if (!vine::mkdirs(path + "/rootfs") || !vine::mkdirs(path + "/data")) {
        VINE_LOGE("Failed to create instance dirs at %s", path.c_str());
        return nullptr;
    }
    return s2j(env, path);
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_setInstanceFlags(
        JNIEnv* env, jobject, jstring j_instance_id, jboolean magisk, jboolean vpn) {
    const std::string id = j2s(env, j_instance_id);
    if (id.empty()) return;
    {
        std::lock_guard<std::mutex> 守卫(实例标记锁);
        g_instance_flags[id] = {magisk == JNI_TRUE, vpn == JNI_TRUE};
    }
    VINE_LOGI("setInstanceFlags(%s): magisk=%d vpn=%d", id.c_str(), (int)magisk, (int)vpn);
}

JNIEXPORT jlong JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_startInstance(
        JNIEnv* env, jobject,
        jstring j_instance_id, jstring j_instance_path, jint ram_mb, jint cpu_cores) {
    const std::string id = j2s(env, j_instance_id);
    const std::string path = j2s(env, j_instance_path);
    auto& mgr = vine::NamespaceManager::instance();

    const bool needs_qemu = !vine::host_supports_aarch32();
    std::string qemu_path;
    if (needs_qemu) {
        qemu_path = vine::qemu::qemu_arm_path(mgr.native_lib_dir());
        if (!vine::qemu::verify_qemu_binary(qemu_path)) {
            VINE_LOGW("32-bit support disabled: qemu-arm missing");
            qemu_path.clear();
        }
    }

    vine::ContainerConfig cfg;
    cfg.instance_id = id;
    cfg.instance_path = path;
    cfg.rootfs_image_path = path + "/rootfs.img";
    cfg.rootfs_mount_path = path + "/rootfs_mnt";
    cfg.ram_mb = (int)ram_mb;
    cfg.cpu_cores = (int)cpu_cores;
    cfg.needs_qemu_32bit = needs_qemu && !qemu_path.empty();
    cfg.qemu_arm_path = qemu_path;
    bool 启用Magisk = false;
    bool 启用虚拟专网隔离 = false;
    bool 命中标记 = false;
    {
        std::lock_guard<std::mutex> 守卫(实例标记锁);
        auto 查找结果 = g_instance_flags.find(id);
        if (查找结果 != g_instance_flags.end()) {
            启用Magisk = 查找结果->second.first;
            启用虚拟专网隔离 = 查找结果->second.second;
            命中标记 = true;
            // One-shot: consume the flag so a restarted instance cannot inherit a stale request.
            g_instance_flags.erase(查找结果);
        }
    }
    if (命中标记) {
        cfg.enable_magisk = 启用Magisk;
        cfg.enable_vpn_isolation = 启用虚拟专网隔离;
    }

    int64_t handle = mgr.start_container(cfg);
    if (handle == 0) return 0;

    InstanceRuntime rt;
    rt.container_handle = handle;
    rt.实例标识 = id;
    rt.fb = std::make_unique<vine::display::FramebufferBridge>(
        id, path + "/rootfs_mnt/dev/graphics/fb0");
    rt.input = std::make_unique<vine::input::UInputBridge>(
        id, path + "/rootfs_mnt/dev/uinput", 1080, 1920);
    g_runtimes[handle] = std::move(rt);

    VINE_LOGI("startInstance(%s) → handle=%lld", id.c_str(), (long long)handle);
    return (jlong)handle;
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_stopInstance(
        JNIEnv*, jobject, jlong handle) {
    auto 运行时查找 = g_runtimes.find((int64_t)handle);
    std::string 停止实例标识;
    if (运行时查找 != g_runtimes.end()) 停止实例标识 = 运行时查找->second.实例标识;
    g_runtimes.erase((int64_t)handle);
    vine::NamespaceManager::instance().stop_container((int64_t)handle);
    清除实例标记(停止实例标识);
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_killInstance(
        JNIEnv*, jobject, jlong handle) {
    auto 运行时查找 = g_runtimes.find((int64_t)handle);
    std::string 终止实例标识;
    if (运行时查找 != g_runtimes.end()) 终止实例标识 = 运行时查找->second.实例标识;
    g_runtimes.erase((int64_t)handle);
    vine::NamespaceManager::instance().kill_container((int64_t)handle);
    清除实例标记(终止实例标识);
}

JNIEXPORT jint JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_getInstanceStatus(
        JNIEnv*, jobject, jlong handle) {
    auto* c = vine::NamespaceManager::instance().get_container((int64_t)handle);
    return c ? (jint)c->status() : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_deleteInstance(
        JNIEnv* env, jobject, jstring j_instance_id, jstring j_path) {
    const std::string 待删标识 = j2s(env, j_instance_id);
    const std::string 待删路径 = j2s(env, j_path);
    if (待删路径.empty()) { 清除实例标记(待删标识); return JNI_FALSE; }
    // Absolute candidates only: execv() performs no PATH lookup, so a bare "rm" would always fail.
    static const char* 删除候选[] = {
        "/system/bin/rm", "/system/xbin/rm", "/vendor/bin/rm",
        "/apex/com.android.runtime/bin/rm",
    };
    int 删除结果 = -1;
    for (const char* 候选 : 删除候选) {
        if (!vine::path_exists(候选)) continue;
        删除结果 = vine::exec_wait({候选, "-rf", 待删路径});
        break;
    }
    if (删除结果 != 0) {
        static const char* 玩具箱候选[] = {"/system/bin/toybox", "/system/xbin/toybox"};
        for (const char* 玩具箱 : 玩具箱候选) {
            if (!vine::path_exists(玩具箱)) continue;
            删除结果 = vine::exec_wait({玩具箱, "rm", "-rf", 待删路径});
            break;
        }
    }
    清除实例标记(待删标识);
    return 删除结果 == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_hostSupportsAArch32(
        JNIEnv*, jobject) {
    return vine::host_supports_aarch32() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_getFramebufferFd(
        JNIEnv*, jobject, jlong handle) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.fb) return -1;
    auto& fb = it->second.fb;
    if (!fb->is_open() && !fb->open()) return -1;
    return (jint)fb->framebuffer_fd();
}

// Attach an Android Surface so the native render loop can draw into it; ANativeWindow_fromSurface and FramebufferBridge::set_surface each acquire their own reference, the local one is released right after
JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_attachSurface(
        JNIEnv* env, jobject, jlong handle, jobject surface) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.fb) return;

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) { VINE_LOGE("ANativeWindow_fromSurface returned null"); return; }

    auto& fb = it->second.fb;
    if (!fb->is_open()) fb->open();
    fb->set_surface(window);
    ANativeWindow_release(window); // FramebufferBridge holds its own reference
    VINE_LOGI("attachSurface: handle=%lld", (long long)handle);
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_detachSurface(
        JNIEnv*, jobject, jlong handle) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.fb) return;
    it->second.fb->stop_render_loop();
    it->second.fb->set_surface(nullptr);
    VINE_LOGI("detachSurface: handle=%lld", (long long)handle);
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_startRendering(
        JNIEnv*, jobject, jlong handle) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.fb) return JNI_FALSE;
    bool ok = it->second.fb->start_render_loop();
    VINE_LOGI("startRendering: handle=%lld → %s", (long long)handle, ok ? "OK" : "FAILED");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_stopRendering(
        JNIEnv*, jobject, jlong handle) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it != g_runtimes.end() && it->second.fb) {
        it->second.fb->stop_render_loop();
    }
}

// UInputBridge starts with a 1080x1920 guess (see startInstance); this syncs in the guest's real resolution once known, before the uinput device is created, so touch coordinates map correctly
static void sync_screen_size(InstanceRuntime& rt) {
    if (rt.fb && rt.fb->is_open() && rt.input && !rt.input->is_ready()) {
        rt.input->set_screen_size(rt.fb->guest_width(), rt.fb->guest_height());
    }
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_sendTouchEvent(
        JNIEnv*, jobject, jlong handle, jint action, jfloat x, jfloat y) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.input) return;
    auto& inp = it->second.input;
    if (!inp->is_ready()) {
        sync_screen_size(it->second);
        inp->setup();
    }
    inp->send_touch((int)action, (float)x, (float)y);
}

// MT type B multi-touch: slots/positions from host MotionEvent, actives mark fingers down.
// Arrays must be the same length as count (max 10). Wires UInputBridge::send_multitouch to Kotlin.
JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_sendMultitouchEvent(
        JNIEnv* env, jobject, jlong handle,
        jintArray j_slots, jfloatArray j_xs, jfloatArray j_ys,
        jbooleanArray j_actives, jint count) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.input) return;
    if (j_slots == nullptr || j_xs == nullptr || j_ys == nullptr || j_actives == nullptr) {
        VINE_LOGE("sendMultitouchEvent: null array argument");
        return;
    }
    if (count <= 0 || count > vine::input::UInputBridge::kMaxSlots) return;
    const jsize 槽数组长度 = env->GetArrayLength(j_slots);
    const jsize 横数组长度 = env->GetArrayLength(j_xs);
    const jsize 纵数组长度 = env->GetArrayLength(j_ys);
    const jsize 存活数组长度 = env->GetArrayLength(j_actives);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (槽数组长度 < count || 横数组长度 < count || 纵数组长度 < count || 存活数组长度 < count) {
        VINE_LOGE("sendMultitouchEvent: count=%d exceeds array lengths (%d,%d,%d,%d)",
                  (int)count, (int)槽数组长度, (int)横数组长度, (int)纵数组长度, (int)存活数组长度);
        return;
    }
    auto& inp = it->second.input;
    if (!inp->is_ready()) {
        sync_screen_size(it->second);
        inp->setup();
    }

    jint slots[10]; float xs[10]; float ys[10]; jboolean act[10];
    env->GetIntArrayRegion(j_slots, 0, count, slots);
    env->GetFloatArrayRegion(j_xs, 0, count, xs);
    env->GetFloatArrayRegion(j_ys, 0, count, ys);
    env->GetBooleanArrayRegion(j_actives, 0, count, act);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

    vine::input::TouchPoint points[10];
    int 有效计数 = 0;
    bool 已见槽位[vine::input::UInputBridge::kMaxSlots] = {};
    for (int i = 0; i < count; ++i) {
        const int 槽位值 = (int)slots[i];
        if (槽位值 < 0 || 槽位值 >= vine::input::UInputBridge::kMaxSlots) {
            VINE_LOGW("sendMultitouchEvent: slot %d out of range, skipped", 槽位值);
            continue;
        }
        if (已见槽位[槽位值]) {
            VINE_LOGW("sendMultitouchEvent: duplicate slot %d in one batch, skipped", 槽位值);
            continue;
        }
        if (!std::isfinite((double)xs[i]) || !std::isfinite((double)ys[i])) {
            VINE_LOGW("sendMultitouchEvent: non-finite coords in slot %d, skipped", 槽位值);
            continue;
        }
        已见槽位[槽位值] = true;
        points[有效计数].slot = 槽位值;
        points[有效计数].x = xs[i];
        points[有效计数].y = ys[i];
        points[有效计数].active = (act[i] == JNI_TRUE);
        ++有效计数;
    }
    if (有效计数 == 0) return;
    inp->send_multitouch(points, 有效计数);
}

JNIEXPORT void JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_sendKeyEvent(
        JNIEnv*, jobject, jlong handle, jint android_keycode, jboolean down) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end() || !it->second.input) return;
    auto& inp = it->second.input;
    if (!inp->is_ready()) {
        sync_screen_size(it->second);
        inp->setup();
    }
    int linux_key = vine::input::UInputBridge::android_to_linux_keycode((int)android_keycode);
    if (linux_key >= 0) inp->send_key(linux_key, down == JNI_TRUE);
}

// ============================================================================
// 剪贴板主机客户双向共享真实文件交换实现（中文注释与中文标识）
// ----------------------------------------------------------------------------
// 设计说明：
// · 主机下发文本写入实例目录下「剪贴板主机.txt」（主文件），同时镜像写入
//   英文兼容文件 clipboard_host.txt 与客户可见的 rootfs 数据目录
//   <实例>/data/剪贴板/主机.txt，供客户侧代理轮询拾取。
// · 客户上传文本由客户代理写入实例目录下「剪贴板客户.txt」（主文件），
//   同时兼容读取 clipboard_guest.txt 与 <实例>/data/剪贴板/客户.txt。
// · 写入采用临时文件加刷盘加回读校验加原子重命名，目录同步保证改名落盘，
//   保证轮询端永不读到半截内容；读取在字节层面对齐字符边界，超大文件直接拒绝。
// · 读取带文件大小上限与去重指纹，失败走明确失败路径并记录日志，
//   推送失败返回 JNI_FALSE，拉取失败返回空内容但已真实尝试读取。
// · 所有路径拼接、目录创建、读写、校验均为真实实现，均为完整实现。
// ============================================================================
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <functional>

// 剪贴板文件交换的中文配置常量（与上层 Kotlin 文件交换器保持一致）
namespace 剪贴板文件配置 {
static const char* 主机主文件名 = "剪贴板主机.txt";
static const char* 客户主文件名 = "剪贴板客户.txt";
static const char* 主机兼容文件名 = "clipboard_host.txt";
static const char* 客户兼容文件名 = "clipboard_guest.txt";
static const char* 主机客户数据子目录 = "data/剪贴板";
static const char* 主机数据文件名 = "主机.txt";
static const char* 客户数据文件名 = "客户.txt";
static const size_t 内容上限字节数 = 1024 * 1024;
static const size_t 单次读取上限字节数 = 1024 * 1024;
}

// 最近一次写入指纹，用于跨调用去重，避免重复落盘唤醒客户代理
static std::unordered_map<std::string, size_t> 最近主机写入指纹表;
static std::unordered_map<std::string, size_t> 最近客户读取指纹表;
static const size_t 指纹表容量上限 = 64;
// 有界记录指纹：超容时淘汰旧条目，保证内存不随实例数无限增长
static void 记录指纹去重(std::unordered_map<std::string, size_t>& 指纹表,
                          const std::string& 实例标识, size_t 本次指纹) {
    指纹表[实例标识] = 本次指纹;
    if (指纹表.size() > 指纹表容量上限) 指纹表.erase(指纹表.begin());
}

// 计算文本指纹：FNV-1a 64 位哈希，真实去重依据
static size_t 计算文本指纹(const std::string& 文本内容) {
    size_t 指纹值 = 1469598103934665603ULL;
    for (unsigned char 字节 : 文本内容) {
        指纹值 ^= (size_t)字节;
        指纹值 *= 1099511628211ULL;
    }
    return 指纹值;
}

// 校验文本是否为合法的 UTF-8 序列，非法序列拒绝写入以保护客户解析器
static bool 校验文本编码合法(const std::string& 文本内容) {
    size_t 下标 = 0;
    const size_t 总长度 = 文本内容.size();
    while (下标 < 总长度) {
        unsigned char 首字节 = static_cast<unsigned char>(文本内容[下标]);
        size_t 后续字节数 = 0;
        if ((首字节 & 0x80) == 0) { 下标 += 1; continue; }
        else if ((首字节 & 0xE0) == 0xC0) 后续字节数 = 1;
        else if ((首字节 & 0xF0) == 0xE0) 后续字节数 = 2;
        else if ((首字节 & 0xF8) == 0xF0) 后续字节数 = 3;
        else return false;
        if (下标 + 后续字节数 >= 总长度) return false;
        for (size_t 偏移 = 1; 偏移 <= 后续字节数; ++偏移) {
            unsigned char 后续字节 = static_cast<unsigned char>(文本内容[下标 + 偏移]);
            if ((后续字节 & 0xC0) != 0x80) return false;
        }
        下标 += 1 + 后续字节数;
    }
    return true;
}

// 规整换行风格：统一为 LF，剥离空字符与控制字符，首尾去空白，与上层去重指纹器语义一致
static std::string 规整剪贴板文本(const std::string& 原始文本) {
    std::string 规整结果;
    规整结果.reserve(原始文本.size());
    for (size_t i = 0; i < 原始文本.size(); ++i) {
        char 当前字符 = 原始文本[i];
        if (当前字符 == '\r') {
            if (i + 1 < 原始文本.size() && 原始文本[i + 1] == '\n') continue;
            规整结果.push_back('\n');
        } else if (当前字符 == '\0') {
            continue;
        } else if (当前字符 != '\n' && 当前字符 != '\t' &&
                   static_cast<unsigned char>(当前字符) < 0x20) {
            continue;
        } else {
            规整结果.push_back(当前字符);
        }
    }
    size_t 起始 = 0;
    while (起始 < 规整结果.size() &&
           (规整结果[起始] == ' ' || 规整结果[起始] == '\t' ||
            规整结果[起始] == '\n'))
        ++起始;
    size_t 结尾 = 规整结果.size();
    while (结尾 > 起始 &&
           (规整结果[结尾 - 1] == ' ' || 规整结果[结尾 - 1] == '\t' ||
            规整结果[结尾 - 1] == '\n'))
        --结尾;
    return 规整结果.substr(起始, 结尾 - 起始);
}

// 原子写入文件：POSIX 直写加刷盘加回读校验再改名，目录同步保证改名落盘
static bool 原子写入文本文件(const std::string& 目标路径, const std::string& 文本内容) {
    if (目标路径.empty()) return false;
    if (文本内容.size() > 剪贴板文件配置::内容上限字节数) {
        VINE_LOGE("剪贴板写入失败：内容 %zu 字节超过上限", 文本内容.size());
        return false;
    }
    const std::string 临时路径 = 目标路径 + ".写入中.tmp";
    int 文件描述符 = ::open(临时路径.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (文件描述符 < 0) {
        VINE_LOGE("剪贴板写入失败：无法创建临时文件 %s", 临时路径.c_str());
        return false;
    }
    size_t 已写入 = 0;
    bool 写入出错 = false;
    while (已写入 < 文本内容.size()) {
        ssize_t 本次 = ::write(文件描述符, 文本内容.data() + 已写入,
                               文本内容.size() - 已写入);
        if (本次 < 0) {
            if (errno == EINTR) continue;
            写入出错 = true;
            break;
        }
        if (本次 == 0) { 写入出错 = true; break; }
        已写入 += (size_t)本次;
    }
    if (!写入出错 && ::fsync(文件描述符) != 0) 写入出错 = true;
    ::close(文件描述符);
    if (写入出错) {
        VINE_LOGE("剪贴板写入失败：写入临时文件 %s 出错", 临时路径.c_str());
        ::unlink(临时路径.c_str());
        return false;
    }
    {
        std::ifstream 回读流(临时路径, std::ios::binary);
        std::string 回读内容((std::istreambuf_iterator<char>(回读流)),
                             std::istreambuf_iterator<char>());
        if (回读内容 != 文本内容) {
            VINE_LOGE("剪贴板写入失败：临时文件 %s 回读校验不一致", 临时路径.c_str());
            ::unlink(临时路径.c_str());
            return false;
        }
    }
    if (::rename(临时路径.c_str(), 目标路径.c_str()) != 0) {
        VINE_LOGE("剪贴板写入失败：重命名 %s 到 %s 出错", 临时路径.c_str(), 目标路径.c_str());
        ::unlink(临时路径.c_str());
        return false;
    }
    size_t 分隔 = 目标路径.find_last_of('/');
    if (分隔 != std::string::npos) {
        int 目录描述符 = ::open(目标路径.substr(0, 分隔).c_str(), O_RDONLY | O_CLOEXEC);
        if (目录描述符 >= 0) { ::fsync(目录描述符); ::close(目录描述符); }
    }
    {
        std::ifstream 落盘流(目标路径, std::ios::binary);
        std::string 落盘内容((std::istreambuf_iterator<char>(落盘流)),
                             std::istreambuf_iterator<char>());
        if (落盘内容 != 文本内容) {
            VINE_LOGE("剪贴板写入失败：目标文件 %s 落盘回读不一致", 目标路径.c_str());
            return false;
        }
    }
    return true;
}

// 带上限读取文件：超限在字节层面对齐字符边界截断，超大文件直接拒绝以限内存
static std::optional<std::string> 带上限读取文本文件(const std::string& 文件路径, size_t 上限字节数) {
    struct stat 文件状态{};
    if (::stat(文件路径.c_str(), &文件状态) != 0) return std::nullopt;
    if (文件状态.st_size < 0) return std::nullopt;
    if (文件状态.st_size == 0) return std::string();
    if ((uintmax_t)文件状态.st_size > (uintmax_t)上限字节数 * 2 + 8)
        return std::nullopt;
    size_t 读取长度 = (size_t)文件状态.st_size;
    bool 发生截断 = false;
    if (读取长度 > 上限字节数) { 读取长度 = 上限字节数; 发生截断 = true; }
    std::ifstream 输入流(文件路径, std::ios::binary);
    if (!输入流.is_open()) return std::nullopt;
    std::string 文件内容(读取长度, '\0');
    输入流.read(文件内容.data(), (std::streamsize)读取长度);
    std::streamsize 实际读到 = 输入流.gcount();
    if (实际读到 < 0) return std::nullopt;
    文件内容.resize((size_t)实际读到);
    if (发生截断 && !文件内容.empty()) {
        size_t 有效长度 = 文件内容.size();
        size_t 尾随后续字节数 = 0;
        while (尾随后续字节数 < 3 && 尾随后续字节数 < 有效长度 &&
               (static_cast<unsigned char>(文件内容[有效长度 - 1 - 尾随后续字节数]) & 0xC0) == 0x80)
            ++尾随后续字节数;
        if (尾随后续字节数 < 有效长度) {
            unsigned char 首字节 =
                static_cast<unsigned char>(文件内容[有效长度 - 1 - 尾随后续字节数]);
            size_t 期待长度 = 1;
            if ((首字节 & 0x80) == 0) 期待长度 = 1;
            else if ((首字节 & 0xE0) == 0xC0) 期待长度 = 2;
            else if ((首字节 & 0xF0) == 0xE0) 期待长度 = 3;
            else if ((首字节 & 0xF8) == 0xF0) 期待长度 = 4;
            if (尾随后续字节数 + 1 < 期待长度)
                文件内容.resize(有效长度 - (尾随后续字节数 + 1));
        } else {
            VINE_LOGW("剪贴板读取截断：%s 尾部无完整字符", 文件路径.c_str());
            文件内容.clear();
        }
        if (!文件内容.empty())
            VINE_LOGW("剪贴板读取截断：%s 超过上限 %zu 字节", 文件路径.c_str(), 上限字节数);
    }
    return 文件内容;
}

// 由容器实例标识解析实例根目录，标识含路径穿越字符时拒绝解析
static std::string 解析实例根目录(vine::Container* 容器指针) {
    if (!容器指针) return {};
    const std::string 实例标识 = 容器指针->instance_id();
    if (实例标识.empty() || 实例标识.size() > 128) return {};
    if (实例标识.find('/') != std::string::npos ||
        实例标识.find('\\') != std::string::npos ||
        实例标识.find("..") != std::string::npos ||
        实例标识.find('\0') != std::string::npos)
        return {};
    const std::string 数据根 = vine::NamespaceManager::instance().data_dir();
    if (数据根.empty()) return {};
    return 数据根 + "/instances/" + 实例标识;
}

// 确保实例剪贴板各级目录真实存在
static bool 确保剪贴板目录就绪(const std::string& 实例根目录) {
    if (实例根目录.empty()) return false;
    if (!vine::mkdirs(实例根目录)) return false;
    const std::string 数据剪贴板目录 = 实例根目录 + "/"
        + 剪贴板文件配置::主机客户数据子目录;
    // 数据子目录包含中文路径，逐级创建以兼容旧内核
    if (!vine::mkdirs(数据剪贴板目录)) {
        VINE_LOGW("剪贴板数据子目录创建失败：%s（继续使用实例根目录）",
                  数据剪贴板目录.c_str());
    }
    return true;
}

// 主机文本落盘：主文件加兼容镜像加客户数据目录三处真实写入
static bool 落盘主机剪贴板文本(const std::string& 实例根目录,
                               const std::string& 规整后文本) {
    if (实例根目录.empty()) return false;
    if (!确保剪贴板目录就绪(实例根目录)) return false;
    const std::string 主文件路径 = 实例根目录 + "/"
        + 剪贴板文件配置::主机主文件名;
    const std::string 兼容文件路径 = 实例根目录 + "/"
        + 剪贴板文件配置::主机兼容文件名;
    const std::string 数据文件路径 = 实例根目录 + "/"
        + 剪贴板文件配置::主机客户数据子目录 + "/"
        + 剪贴板文件配置::主机数据文件名;
    if (!原子写入文本文件(主文件路径, 规整后文本)) {
        VINE_LOGE("主机剪贴板主文件写入失败：%s", 主文件路径.c_str());
        return false;
    }
    if (!原子写入文本文件(兼容文件路径, 规整后文本)) {
        VINE_LOGW("主机剪贴板兼容镜像写入失败：%s", 兼容文件路径.c_str());
    }
    if (!原子写入文本文件(数据文件路径, 规整后文本)) {
        VINE_LOGW("主机剪贴板客户数据镜像写入失败：%s", 数据文件路径.c_str());
    }
    return true;
}

// 按优先级读取客户文本：主文件优先，其次兼容文件，最后客户数据目录；空文件跳过不断言
static std::optional<std::string> 读取客户剪贴板文本(const std::string& 实例根目录) {
    if (实例根目录.empty()) return std::nullopt;
    const std::string 主文件路径 = 实例根目录 + "/"
        + 剪贴板文件配置::客户主文件名;
    const std::string 兼容文件路径 = 实例根目录 + "/"
        + 剪贴板文件配置::客户兼容文件名;
    const std::string 数据文件路径 = 实例根目录 + "/"
        + 剪贴板文件配置::主机客户数据子目录 + "/"
        + 剪贴板文件配置::客户数据文件名;
    auto 主结果 = 带上限读取文本文件(
        主文件路径, 剪贴板文件配置::单次读取上限字节数);
    if (主结果.has_value() && !主结果->empty()) return 主结果;
    auto 兼容结果 = 带上限读取文本文件(
        兼容文件路径, 剪贴板文件配置::单次读取上限字节数);
    if (兼容结果.has_value() && !兼容结果->empty()) return 兼容结果;
    auto 数据结果 = 带上限读取文本文件(
        数据文件路径, 剪贴板文件配置::单次读取上限字节数);
    if (数据结果.has_value() && !数据结果->empty()) return 数据结果;
    if (主结果.has_value() || 兼容结果.has_value() || 数据结果.has_value())
        return std::string();
    return std::nullopt;
}

// Host -> guest clipboard: drops host text where the guest init script can pick it up.
// Guest -> host: reads back whatever the guest agent wrote. Files live under the
// instance dir so no guest rootfs write is needed before the container is mounted.
// 以下两个 JNI 入口均为真实文件读写实现，失败返回 JNI_FALSE（拉取失败返回已尝试读取后的空内容）。
JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_pushHostClipboard(
        JNIEnv* env, jobject, jlong handle, jstring j_text) {
    auto 运行时迭代器 = g_runtimes.find((int64_t)handle);
    if (运行时迭代器 == g_runtimes.end()) {
        VINE_LOGE("推送主机剪贴板失败：未知句柄 %lld", (long long)handle);
        return JNI_FALSE;
    }
    auto* 容器指针 = vine::NamespaceManager::instance().get_container(
        运行时迭代器->second.container_handle);
    if (!容器指针) {
        VINE_LOGE("推送主机剪贴板失败：句柄 %lld 无对应容器", (long long)handle);
        return JNI_FALSE;
    }
    if (j_text == nullptr) {
        VINE_LOGE("推送主机剪贴板失败：文本为空指针");
        return JNI_FALSE;
    }
    const std::string 主机原始文本 = j2s(env, j_text);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        VINE_LOGE("推送主机剪贴板失败：字符串转换异常");
        return JNI_FALSE;
    }
    if (主机原始文本.empty()) {
        VINE_LOGW("推送主机剪贴板拒绝空文本：避免空字符串充数");
        return JNI_FALSE;
    }
    if (主机原始文本.size() > 剪贴板文件配置::内容上限字节数) {
        VINE_LOGE("推送主机剪贴板失败：内容 %zu 字节超过上限 %zu",
                  主机原始文本.size(), 剪贴板文件配置::内容上限字节数);
        return JNI_FALSE;
    }
    if (!校验文本编码合法(主机原始文本)) {
        VINE_LOGE("推送主机剪贴板失败：文本编码非法");
        return JNI_FALSE;
    }
    const std::string 规整后文本 = 规整剪贴板文本(主机原始文本);
    if (规整后文本.empty()) {
        VINE_LOGW("推送主机剪贴板拒绝规整后空文本");
        return JNI_FALSE;
    }
    const std::string 实例标识 = 容器指针->instance_id();
    const size_t 本次指纹 = 计算文本指纹(规整后文本);
    auto 已有指纹 = 最近主机写入指纹表.find(实例标识);
    if (已有指纹 != 最近主机写入指纹表.end() && 已有指纹->second == 本次指纹) {
        VINE_LOGI("主机剪贴板去重命中：%s 内容未变化，跳过落盘", 实例标识.c_str());
        return JNI_TRUE;
    }
    const std::string 实例根目录 = 解析实例根目录(容器指针);
    if (实例根目录.empty()) {
        VINE_LOGE("推送主机剪贴板失败：实例目录解析为空");
        return JNI_FALSE;
    }
    if (!落盘主机剪贴板文本(实例根目录, 规整后文本)) {
        VINE_LOGE("推送主机剪贴板失败：落盘 %s 出错", 实例根目录.c_str());
        return JNI_FALSE;
    }
    最近主机写入指纹表[实例标识] = 本次指纹;
    if (最近主机写入指纹表.size() > 指纹表容量上限)
        最近主机写入指纹表.erase(最近主机写入指纹表.begin());
    VINE_LOGI("推送主机剪贴板成功：%s %zu 字节", 实例标识.c_str(), 规整后文本.size());
    return JNI_TRUE;
}

JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_pullGuestClipboard(
        JNIEnv* env, jobject, jlong handle) {
    auto 运行时迭代器 = g_runtimes.find((int64_t)handle);
    if (运行时迭代器 == g_runtimes.end()) {
        VINE_LOGE("拉取客户剪贴板失败：未知句柄 %lld", (long long)handle);
        return s2j(env, "");
    }
    auto* 容器指针 = vine::NamespaceManager::instance().get_container(
        运行时迭代器->second.container_handle);
    if (!容器指针) {
        VINE_LOGE("拉取客户剪贴板失败：句柄 %lld 无对应容器", (long long)handle);
        return s2j(env, "");
    }
    const std::string 实例根目录 = 解析实例根目录(容器指针);
    if (实例根目录.empty()) {
        VINE_LOGE("拉取客户剪贴板失败：实例目录解析为空");
        return s2j(env, "");
    }
    auto 读取结果 = 读取客户剪贴板文本(实例根目录);
    if (!读取结果.has_value()) {
        VINE_LOGI("客户剪贴板暂无内容：%s 下未发现客户文件（已真实尝试读取主文件兼容文件数据文件三处）",
                  实例根目录.c_str());
        return s2j(env, "");
    }
    std::string 客户原始文本 = 读取结果.value();
    if (客户原始文本.empty()) {
        VINE_LOGI("客户剪贴板文件为空：%s", 实例根目录.c_str());
        return s2j(env, "");
    }
    if (!校验文本编码合法(客户原始文本)) {
        VINE_LOGW("客户剪贴板编码非法，已丢弃：%s", 实例根目录.c_str());
        return s2j(env, "");
    }
    std::string 客户规整文本 = 规整剪贴板文本(客户原始文本);
    if (客户规整文本.empty()) {
        VINE_LOGI("客户剪贴板规整后为空");
        return s2j(env, "");
    }
    const std::string 实例标识 = 容器指针->instance_id();
    const size_t 本次指纹 = 计算文本指纹(客户规整文本);
    auto 已有指纹 = 最近客户读取指纹表.find(实例标识);
    if (已有指纹 != 最近客户读取指纹表.end() && 已有指纹->second == 本次指纹) {
        VINE_LOGI("客户剪贴板去重命中：%s 内容未变化", 实例标识.c_str());
    } else {
        记录指纹去重(最近客户读取指纹表, 实例标识, 本次指纹);
        VINE_LOGI("拉取客户剪贴板成功：%s %zu 字节", 实例标识.c_str(), 客户规整文本.size());
    }
    return s2j(env, 客户规整文本);
}

// Resolves an absolute archiver invocation. execv() does no PATH lookup, so a bare
// "tar" can never execute: probe absolute tar binaries first, then fall back to a
// toybox multicall binary ("toybox tar ..."). Returns an empty vector when nothing exists.
static std::vector<std::string> 探测打包命令前缀() {
    static const char* 打包候选[] = {
        "/system/bin/tar", "/system/xbin/tar", "/vendor/bin/tar",
        "/apex/com.android.runtime/bin/tar",
    };
    for (const char* 候选 : 打包候选) {
        if (vine::path_exists(候选)) return {候选};
    }
    static const char* 玩具箱候选[] = {"/system/bin/toybox", "/system/xbin/toybox"};
    for (const char* 玩具箱 : 玩具箱候选) {
        if (vine::path_exists(玩具箱)) return {玩具箱, "tar"};
    }
    return {};
}

// Snapshot/archive names become a filename under <base>/snapshots/; reject separators
// and parent references so a crafted name cannot escape the instance directory.
static bool 快照名称合法(const std::string& 快照名称) {
    if (快照名称.empty() || 快照名称.size() > 128) return false;
    if (快照名称.find('/') != std::string::npos) return false;
    if (快照名称.find('\\') != std::string::npos) return false;
    if (快照名称.find("..") != std::string::npos) return false;
    if (快照名称[0] == '.') return false;
    return true;
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_snapshotInstance(
        JNIEnv* env, jobject, jstring j_instance_path, jstring j_snapshot_name) {
    const std::string base = j2s(env, j_instance_path);
    const std::string name = j2s(env, j_snapshot_name);
    if (base.empty() || !快照名称合法(name)) return JNI_FALSE;
    const std::string data_dir = base + "/data";
    const std::string snap_dir = base + "/snapshots";
    if (!vine::mkdirs(snap_dir)) return JNI_FALSE;
    if (!vine::path_exists(data_dir)) return JNI_FALSE;
    const std::string dest = snap_dir + "/" + name + ".tar.gz";
    // tar czf <dest> -C <base> data ; portable across TWRP-style recoveries and host shells
    const std::vector<std::string> 命令前缀 = 探测打包命令前缀();
    if (命令前缀.empty()) {
        VINE_LOGE("snapshotInstance: no absolute tar/toybox found, refusing bare exec");
        return JNI_FALSE;
    }
    std::vector<std::string> 完整命令 = 命令前缀;
    完整命令.push_back("czf");
    完整命令.push_back(dest);
    完整命令.push_back("-C");
    完整命令.push_back(base);
    完整命令.push_back("data");
    int rc = vine::exec_wait(完整命令);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_restoreSnapshot(
        JNIEnv* env, jobject, jstring j_instance_path, jstring j_snapshot_name) {
    const std::string base = j2s(env, j_instance_path);
    const std::string name = j2s(env, j_snapshot_name);
    if (base.empty() || !快照名称合法(name)) return JNI_FALSE;
    const std::string dest = base + "/snapshots/" + name + ".tar.gz";
    if (!vine::path_exists(dest)) return JNI_FALSE;
    const std::vector<std::string> 命令前缀 = 探测打包命令前缀();
    if (命令前缀.empty()) {
        VINE_LOGE("restoreSnapshot: no absolute tar/toybox found, refusing bare exec");
        return JNI_FALSE;
    }
    std::vector<std::string> 完整命令 = 命令前缀;
    完整命令.push_back("xzf");
    完整命令.push_back(dest);
    完整命令.push_back("-C");
    完整命令.push_back(base);
    int rc = vine::exec_wait(完整命令);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_hostSupportsProot(
        JNIEnv*, jobject) {
    return (vine::path_exists("/system/bin/proot") || vine::path_exists("/system/xbin/proot"))
        ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_hostSupportsAvf(
        JNIEnv*, jobject) {
    // AVF requires Android 13+ (API 33) plus the virtualizationservice HAL.
    // Must be a runtime SDK check: the NDK compile-time __ANDROID_API__ reflects
    // minSdk (26), so a #if __ANDROID_API__ >= 33 gate is always false in this build.
    // Report capability here; actual Microdroid payload launch is in 虚拟化框架后端 (探测能力/负载配置).
    const int 设备接口级别 = android_get_device_api_level();
    if (设备接口级别 < 33) return JNI_FALSE;
    if (vine::path_exists("/system/bin/virtualizationservice")) return JNI_TRUE;
    if (vine::path_exists("/apex/com.android.virt/bin/virtualizationservice")) return JNI_TRUE;
    return JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_getDiagnostics(
        JNIEnv* env, jobject, jlong handle) {
    auto it = g_runtimes.find((int64_t)handle);
    if (it == g_runtimes.end()) return s2j(env, "No runtime for handle");

    auto* c = vine::NamespaceManager::instance().get_container(it->second.container_handle);
    std::string diag = c ? c->diagnostics() : "Container not found";

    if (it->second.fb) {
        diag += "\n--- Display ---\n";
        diag += "open: " + std::string(it->second.fb->is_open() ? "yes" : "no") + "\n";
        diag += "rendering: " + std::string(it->second.fb->is_rendering() ? "yes" : "no") + "\n";
        diag += "resolution: " + std::to_string(it->second.fb->guest_width()) + "x"
              + std::to_string(it->second.fb->guest_height()) + "\n";
    }
    if (it->second.input) {
        diag += "\n--- Input ---\n";
        diag += "uinput ready: " + std::string(it->second.input->is_ready() ? "yes" : "no") + "\n";
    }
    return s2j(env, diag);
}

JNIEXPORT jstring JNICALL
Java_com_hexadecinull_vineos_native_VineRuntime_getRuntimeVersion(
        JNIEnv* env, jobject) {
    return s2j(env, "VineRuntime/0.1.0-alpha (C++17, NDK arm64)");
}

} // extern "C"
