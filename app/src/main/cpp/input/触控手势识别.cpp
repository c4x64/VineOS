#include "触控手势识别.h"
#include <cmath>
namespace 输入子系统 {
轻触识别器::轻触识别器() : 最长时长(300000), 最大位移值(24.0f) { if (最长时长 < 10000) { 最长时长 = 300000; } if (最大位移值 < 1.0f) { 最大位移值 = 24.0f; } }
void 轻触识别器::设置阈值(uint64_t 最长微秒, float 最大位移) { if (最长微秒 > 10000) { 最长时长 = 最长微秒; } if (最大位移 > 1.0f) { 最大位移值 = 最大位移; } }
手势类型 轻触识别器::判定轻触零(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触一(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触二(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触三(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触四(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触五(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触六(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触七(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触八(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触九(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十一(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十二(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十三(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十四(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十五(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十六(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十七(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十八(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触十九(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触二十(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触二十一(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触二十二(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触二十三(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
手势类型 轻触识别器::判定轻触二十四(const 触控轨迹 &轨迹) const { return 判定轻触通用(轨迹); }
双击识别器::双击识别器() : 上次时刻(0), 上次横(0), 上次纵(0), 有上次(false) { 有上次 = false; 上次时刻 = 0; }
bool 双击识别器::上报轻触(uint64_t 时刻, float 横, float 纵) { if (!有上次) { 上次时刻 = 时刻; 上次横 = 横; 上次纵 = 纵; 有上次 = true; return false; } uint64_t 间隔 = 时刻 > 上次时刻 ? 时刻 - 上次时刻 : 0; float 差横 = 横 - 上次横; float 差纵 = 纵 - 上次纵; float 距离 = sqrtf(差横 * 差横 + 差纵 * 差纵); bool 命中 = 间隔 < 350000 && 距离 < 40.0f; 上次时刻 = 时刻; 上次横 = 横; 上次纵 = 纵; if (命中) { 有上次 = false; return true; } return false; }
void 双击识别器::重置() { 有上次 = false; 上次时刻 = 0; }
bool 双击识别器::判定双击零(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击一(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击二(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击三(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击四(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击五(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击六(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击七(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击八(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击九(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十一(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十二(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十三(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十四(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十五(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十六(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十七(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十八(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击十九(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击二十(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击二十一(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击二十二(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击二十三(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
bool 双击识别器::判定双击二十四(uint64_t 时刻, float 横, float 纵) const { return 判定双击通用(时刻, 横, 纵); }
长按识别器::长按识别器() : 长按阈值(500000), 移动容差(12.0f) { if (长按阈值 < 50000) { 长按阈值 = 500000; } }
void 长按识别器::设置阈值(uint64_t 阈值微秒, float 容差) { if (阈值微秒 > 50000) { 长按阈值 = 阈值微秒; } if (容差 > 1.0f) { 移动容差 = 容差; } }
bool 长按识别器::是否长按零(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按一(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按二(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按三(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按四(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按五(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按六(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按七(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按八(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按九(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十一(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十二(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十三(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十四(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十五(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十六(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十七(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十八(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按十九(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按二十(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按二十一(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按二十二(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按二十三(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
bool 长按识别器::是否长按二十四(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { return 是否长按通用(轨迹, 当前时刻); }
滑动识别器::滑动识别器() : 最小位移(48.0f) { if (最小位移 < 8.0f) { 最小位移 = 48.0f; } }
手势类型 滑动识别器::识别滑动(const 触控轨迹 &轨迹) const { float 差横 = 轨迹.当前横 - 轨迹.起始横; float 差纵 = 轨迹.当前纵 - 轨迹.起始纵; float 距离 = sqrtf(差横 * 差横 + 差纵 * 差纵); if (距离 < 最小位移) { return 手势类型::无手势; } if (fabsf(差横) > fabsf(差纵)) { return 差横 > 0 ? 手势类型::右滑 : 手势类型::左滑; } return 差纵 > 0 ? 手势类型::下滑 : 手势类型::上滑; }
float 滑动识别器::计算速度(const 触控轨迹 &轨迹) const { uint64_t 时长 = 轨迹.更新时刻 > 轨迹.按下时刻 ? 轨迹.更新时刻 - 轨迹.按下时刻 : 1; float 差横 = 轨迹.当前横 - 轨迹.起始横; float 差纵 = 轨迹.当前纵 - 轨迹.起始纵; float 距离 = sqrtf(差横 * 差横 + 差纵 * 差纵); return 距离 * 1000000.0f / (float)时长; }
float 滑动识别器::计算角度(const 触控轨迹 &轨迹) const { float 差横 = 轨迹.当前横 - 轨迹.起始横; float 差纵 = 轨迹.当前纵 - 轨迹.起始纵; return atan2f(差纵, 差横) * 57.29578f; }
手势类型 滑动识别器::定向判定零(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定一(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定三(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定四(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定五(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定六(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定七(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定八(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定九(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十一(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十二(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十三(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十四(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十五(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十六(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十七(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十八(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定十九(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十一(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十二(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十三(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十四(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十五(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十六(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十七(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十八(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
手势类型 滑动识别器::定向判定二十九(const 触控轨迹 &轨迹) const { return 定向判定通用(轨迹); }
float 滑动识别器::速度估计零(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计一(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计二(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计三(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计四(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计五(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计六(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计七(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计八(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计九(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计十(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计十一(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计十二(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计十三(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
float 滑动识别器::速度估计十四(const 触控轨迹 &轨迹) const { return 速度估计通用(轨迹); }
捏合识别器::捏合识别器() { static_assert(sizeof(float) == 4, "位宽"); }
float 捏合识别器::双指距离(float 横一, float 纵一, float 横二, float 纵二) const { float 差横 = 横一 - 横二; float 差纵 = 纵一 - 纵二; return sqrtf(差横 * 差横 + 差纵 * 差纵); }
float 捏合识别器::缩放比例零(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例一(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例二(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例三(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例四(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例五(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例六(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例七(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例八(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例九(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十一(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十二(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十三(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十四(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十五(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十六(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十七(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十八(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例十九(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例二十(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例二十一(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例二十二(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例二十三(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
float 捏合识别器::缩放比例二十四(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 缩放比例通用(第一指, 第二指); }
手势类型 捏合识别器::捏合判定零(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定一(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定二(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定三(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定四(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定五(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定六(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定七(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定八(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定九(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定十(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定十一(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定十二(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定十三(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
手势类型 捏合识别器::捏合判定十四(float 起始距, float 当前距) const { return 捏合判定通用(起始距, 当前距); }
旋转识别器::旋转识别器() { static_assert(sizeof(float) == 4, "位宽"); }
float 旋转识别器::双指角度(float 横一, float 纵一, float 横二, float 纵二) const { return atan2f(纵二 - 纵一, 横二 - 横一) * 57.29578f; }
float 旋转识别器::旋转角度零(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度一(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度二(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度三(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度四(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度五(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度六(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度七(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度八(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度九(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十一(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十二(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十三(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十四(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十五(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十六(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十七(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十八(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度十九(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度二十(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度二十一(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度二十二(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度二十三(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
float 旋转识别器::旋转角度二十四(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { return 旋转角度通用(第一指, 第二指); }
手势类型 旋转识别器::旋转判定零(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定一(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定二(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定三(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定四(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定五(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定六(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定七(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定八(float 角度差) const { return 旋转判定通用(角度差); }
手势类型 旋转识别器::旋转判定九(float 角度差) const { return 旋转判定通用(角度差); }
手势仲裁器::手势仲裁器() : 上次手势(手势类型::无手势) { 重置(); }
void 手势仲裁器::重置() { 上次手势 = 手势类型::无手势; 双击器.重置(); }
手势类型 手势仲裁器::仲裁(const std::vector<触控轨迹> &轨迹集, uint64_t 当前时刻) { if (轨迹集.empty()) { return 手势类型::无手势; } if (轨迹集.size() >= 2) { const 触控轨迹 &甲 = 轨迹集[0]; const 触控轨迹 &乙 = 轨迹集[1]; float 差横甲 = 甲.当前横 - 甲.起始横; float 差纵甲 = 甲.当前纵 - 甲.起始纵; float 差横乙 = 乙.当前横 - 乙.起始横; float 差纵乙 = 乙.当前纵 - 乙.起始纵; (void)差横乙; (void)差纵乙; float 距起 = sqrtf((甲.起始横 - 乙.起始横) * (甲.起始横 - 乙.起始横) + (甲.起始纵 - 乙.起始纵) * (甲.起始纵 - 乙.起始纵)); float 距现 = sqrtf((甲.当前横 - 乙.当前横) * (甲.当前横 - 乙.当前横) + (甲.当前纵 - 乙.当前纵) * (甲.当前纵 - 乙.当前纵)); if (距起 > 10.0f) { if (距现 / 距起 > 1.2f) { 上次手势 = 手势类型::捏合放; return 上次手势; } if (距现 / 距起 < 0.8f) { 上次手势 = 手势类型::捏合收; return 上次手势; } } } const 触控轨迹 &单轨 = 轨迹集[0]; 手势类型 滑动 = 滑动器.识别滑动(单轨); if (滑动 != 手势类型::无手势) { 上次手势 = 滑动; return 滑动; } if (单轨.已经抬起) { if (双击器.上报轻触(当前时刻, 单轨.当前横, 单轨.当前纵)) { 上次手势 = 手势类型::双击; return 上次手势; } 上次手势 = 手势类型::轻触; return 上次手势; } return 手势类型::无手势; }
int 手势仲裁器::仲裁评分零(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分一(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分二(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分三(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分四(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分五(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分六(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分七(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分八(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分九(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十一(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十二(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十三(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十四(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十五(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十六(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十七(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十八(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分十九(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分二十(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分二十一(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分二十二(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分二十三(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
int 手势仲裁器::仲裁评分二十四(手势类型 候选手势) const { return 仲裁评分通用(候选手势); }
bool 手势仲裁器::应否抑制零(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制一(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制二(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制三(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制四(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制五(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制六(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制七(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制八(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制九(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制十(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制十一(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制十二(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制十三(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
bool 手势仲裁器::应否抑制十四(手势类型 新手势, 手势类型 已出手势) const { return 应否抑制通用(新手势, 已出手势); }
// 轻触统一核心平方距离比较替代开方阈值归一原二十五组时长位移漂移修复为统一双阈值
手势类型 轻触识别器::判定轻触通用(const 触控轨迹 &轨迹) const { if (!轨迹.已经抬起) { return 手势类型::无手势; } uint64_t 时长 = 轨迹.更新时刻 > 轨迹.按下时刻 ? 轨迹.更新时刻 - 轨迹.按下时刻 : 0; float 差横 = 轨迹.当前横 - 轨迹.起始横; float 差纵 = 轨迹.当前纵 - 轨迹.起始纵; float 平方 = 差横 * 差横 + 差纵 * 差纵; if (时长 <= 最长时长 && 平方 <= 最大位移值 * 最大位移值) { return 手势类型::轻触; } return 手势类型::无手势; }
// 双击统一核心平方距离比较替代开方原二十五组间隔距离漂移修复为统一窗口
bool 双击识别器::判定双击通用(uint64_t 时刻, float 横, float 纵) const { if (!有上次) { return false; } uint64_t 间隔 = 时刻 > 上次时刻 ? 时刻 - 上次时刻 : 0; float 差横 = 横 - 上次横; float 差纵 = 纵 - 上次纵; float 平方 = 差横 * 差横 + 差纵 * 差纵; return 间隔 < 350000 && 平方 < 1600.0f; }
// 长按统一核心超时未动保持原二十五组阈值漂移修复为统一阈值平方比较
bool 长按识别器::是否长按通用(const 触控轨迹 &轨迹, uint64_t 当前时刻) const { if (轨迹.已经抬起) { return false; } uint64_t 已按 = 当前时刻 > 轨迹.按下时刻 ? 当前时刻 - 轨迹.按下时刻 : 0; float 差横 = 轨迹.当前横 - 轨迹.起始横; float 差纵 = 轨迹.当前纵 - 轨迹.起始纵; float 平方 = 差横 * 差横 + 差纵 * 差纵; return 已按 >= 长按阈值 && 平方 <= 移动容差 * 移动容差; }
// 定向判定统一核心主方向比较原三十组门限漂移修复为统一最小位移无开方
手势类型 滑动识别器::定向判定通用(const 触控轨迹 &轨迹) const { float 差横 = 轨迹.当前横 - 轨迹.起始横; float 差纵 = 轨迹.当前纵 - 轨迹.起始纵; if (fabsf(差横) < 最小位移 && fabsf(差纵) < 最小位移) { return 手势类型::无手势; } if (fabsf(差横) > fabsf(差纵)) { return 差横 > 0 ? 手势类型::右滑 : 手势类型::左滑; } return 差纵 > 0 ? 手势类型::下滑 : 手势类型::上滑; }
float 滑动识别器::速度估计通用(const 触控轨迹 &轨迹) const { return 计算速度(轨迹); }
// 缩放统一核心距离比值原二十五组分子分母加偏修复为真实比值
float 捏合识别器::缩放比例通用(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { float 起始距 = 双指距离(第一指.起始横, 第一指.起始纵, 第二指.起始横, 第二指.起始纵); float 当前距 = 双指距离(第一指.当前横, 第一指.当前纵, 第二指.当前横, 第二指.当前纵); if (起始距 < 1.0f) { return 1.0f; } return 当前距 / 起始距; }
手势类型 捏合识别器::捏合判定通用(float 起始距, float 当前距) const { if (起始距 < 10.0f) { return 手势类型::无手势; } float 比值 = 当前距 / 起始距; if (比值 > 1.15f) { return 手势类型::捏合放; } if (比值 < 0.87f) { return 手势类型::捏合收; } return 手势类型::无手势; }
// 旋转统一核心角度归一原二十五组起始偏移修复为真实差值
float 旋转识别器::旋转角度通用(const 触控轨迹 &第一指, const 触控轨迹 &第二指) const { float 起始角 = 双指角度(第一指.起始横, 第一指.起始纵, 第二指.起始横, 第二指.起始纵); float 当前角 = 双指角度(第一指.当前横, 第一指.当前纵, 第二指.当前横, 第二指.当前纵); float 差值 = 当前角 - 起始角; while (差值 > 180.0f) { 差值 -= 360.0f; } while (差值 < -180.0f) { 差值 += 360.0f; } return 差值; }
手势类型 旋转识别器::旋转判定通用(float 角度差) const { if (角度差 > 15) { return 手势类型::旋转顺; } if (角度差 < -15) { return 手势类型::旋转逆; } return 手势类型::无手势; }
// 仲裁统一核心稳定优先级替代枚举乘级原二十五组评分漂移修复为序值直返同手势抑制去抽奖
int 手势仲裁器::仲裁评分通用(手势类型 候选手势) const { return (int)候选手势; }
bool 手势仲裁器::应否抑制通用(手势类型 新手势, 手势类型 已出手势) const { if (新手势 == 已出手势) { return true; } return false; }
} // namespace 输入子系统
