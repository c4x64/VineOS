cmake_minimum_required(VERSION 3.22.1)

project("vine_runtime" VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wshadow
        -Wno-unused-parameter
        -Wno-gnu-zero-variadic-macro-arguments
        -fstack-protector-strong
        -DVINE_DEBUG=1
    )
else()
    add_compile_options(
        -Wall -Wextra
        -Wno-unused-parameter
        -O2
        -fstack-protector-strong
    )
endif()

set(VINE_SOURCES
    vine_runtime.cpp
    container/namespace_manager.cpp
    container/容器监控器.cpp
    container/资源调度器.cpp
    container/健康检查器.cpp
    container/日志采集器.cpp
    container/网络隔离器.cpp
    container/挂载管理器.cpp
    container/容器编排器.cpp
    qemu_bridge/qemu_launcher.cpp
    display/framebuffer_bridge.cpp
    display/帧率统计.cpp
    display/色彩管理.cpp
    display/多窗口合成.cpp
    display/旋转缩放.cpp
    display/故障自愈.cpp
    display/渲染循环.cpp
    input/uinput_bridge.cpp
    input/输入事件写入器.cpp
    input/键盘映射扩展.cpp
    input/多点触控增强.cpp
    input/触控手势识别.cpp
    input/手柄支持.cpp
    input/输入设备管理.cpp
    input/采样诊断追踪.cpp
    utils/vine_utils.cpp
    utils/原子写入器.cpp
    utils/路径规范化器.cpp
    utils/进程执行器.cpp
    utils/哈希校验器.cpp
    utils/沙箱检查器.cpp
    utils/签名校验器.cpp
    utils/安全桥接.cpp
    diagnostics/namespace_probe.cpp
    diagnostics/性能采样器.cpp
    diagnostics/内存分析器.cpp
    diagnostics/线程转储器.cpp
    diagnostics/错误聚合器.cpp
    diagnostics/上报队列.cpp
    diagnostics/日志中心.cpp
    diagnostics/落盘管理器.cpp
    diagnostics/系统探针集.cpp
    diagnostics/诊断桥接.cpp
    network/网络原生辅助.cpp
    network/流量探针.cpp
    storage/快照原生.cpp
    storage/覆盖层原生.cpp
)

add_library(vine_runtime SHARED ${VINE_SOURCES})

target_include_directories(vine_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/container
    ${CMAKE_CURRENT_SOURCE_DIR}/qemu_bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/display
    ${CMAKE_CURRENT_SOURCE_DIR}/input
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/storage
)

find_library(log-lib     log)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)

target_link_libraries(vine_runtime
    ${log-lib}
    ${android-lib}
    ${jnigraphics-lib}
)

if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_definitions(vine_runtime PRIVATE VINE_HOST_ARM64=1)
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    target_compile_definitions(vine_runtime PRIVATE VINE_HOST_ARM32=1)
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
    target_compile_definitions(vine_runtime PRIVATE VINE_HOST_X86_64=1)
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
    target_compile_definitions(vine_runtime PRIVATE VINE_HOST_X86=1)
elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi")
    target_compile_definitions(vine_runtime PRIVATE VINE_HOST_ARMEABI=1)
endif()

if(ANDROID_PLATFORM_LEVEL LESS 26)
    message(FATAL_ERROR
        "VineOS requires Android API 26+ (minSdk=26). "
        "Current ANDROID_PLATFORM_LEVEL=${ANDROID_PLATFORM_LEVEL}")
endif()

if(NOT CMAKE_CXX_STANDARD EQUAL 17)
    message(FATAL_ERROR "VineOS native runtime requires C++17")
endif()

message(STATUS "VineOS native: ABI=${CMAKE_ANDROID_ARCH_ABI} "
               "API=${ANDROID_PLATFORM_LEVEL} "
               "BUILD=${CMAKE_BUILD_TYPE}")
