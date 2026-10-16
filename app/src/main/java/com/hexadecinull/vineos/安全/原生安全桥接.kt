package com.hexadecinull.vineos.安全

// ====================================================================
// 模块：原生安全桥接
// 说明：优先调用原生 JNI 实现，链接失败自动回退到纯 Kotlin 实现。
// 约定：注释与标识一律中文；标准库/系统调用保持英文。
// ====================================================================

/**
 * 原生安全桥接：外部方法声明与回退封装。
 *
 * 背景：原生层性能更高且与沙箱同源，但不同设备可能缺失符号；
 * 因此每个方法都捕获链接异常并回退，保证任何设备都有定义行为。
 */
object 原生安全桥接 {
    /**
     * 原生库是否已加载。
     *
     * 说明：加载 vine_runtime，失败则全部走纯 Kotlin 回退。
     */
    var 原生可用: Boolean = false
        private set

    init {
        原生可用 = try {
            System.loadLibrary("vine_runtime")
            桥接版本().isNotEmpty()
        } catch (忽略: UnsatisfiedLinkError) {
            false
        } catch (忽略: Exception) {
            false
        }
    }

    /**
     * 功能：计算文件哈希。
     * 说明：原生优先，失败回退到哈希工具。
     * @param 路径 文件路径，只读。
     * @return 十六进制哈希，失败返回空串。
     */
    fun 计算文件哈希(路径: String): String {
        if (原生可用) {
            try {
                val 原生 = 原生计算文件哈希(路径)
                if (原生.isNotEmpty()) return 原生
            } catch (忽略: UnsatisfiedLinkError) {
                原生可用 = false
            } catch (忽略: Exception) {
            }
        }
        return try {
            哈希工具.计算文件沙256(路径)
        } catch (忽略: Exception) {
            ""
        }
    }

    /**
     * 功能：沙箱评分。
     * 说明：原生综合检查评分，失败回退 Java 层评分。
     * @param 上下文 应用上下文，只读。
     * @return 0-100 分数。
     */
    fun 沙箱评分(上下文: android.content.Context): Int {
        if (原生可用) {
            try {
                val 分 = 原生沙箱评分()
                if (分 in 0..100) return 分
            } catch (忽略: UnsatisfiedLinkError) {
                原生可用 = false
            } catch (忽略: Exception) {
            }
        }
        return try {
            沙箱检查器.综合评分(上下文)
        } catch (忽略: Exception) {
            0
        }
    }

    /**
     * 功能：原子写入文本。
     * 说明：原生原子写入，失败返回负一。
     * @param 路径 目标路径。
     * @param 内容 文本内容。
     * @return 写入字节数，失败负一。
     */
    fun 原子写入文本(路径: String, 内容: String): Long {
        if (原生可用) {
            try {
                return 原生原子写入文本(路径, 内容)
            } catch (忽略: UnsatisfiedLinkError) {
                原生可用 = false
            } catch (忽略: Exception) {
                return -1
            }
        }
        return -1
    }

    /**
     * 功能：规范化路径。
     * 说明：原生词法加消解，失败回退原文。
     * @param 原始 原始路径。
     * @return 规范路径，失败返回原文。
     */
    fun 规范化路径(原始: String): String {
        if (原生可用) {
            try {
                val 规范 = 原生规范化路径(原始)
                if (规范.isNotEmpty()) return 规范
            } catch (忽略: UnsatisfiedLinkError) {
                原生可用 = false
            } catch (忽略: Exception) {
            }
        }
        return 原始
    }

    /**
     * 功能：计算 HMAC。
     * 说明：原生 HMAC，失败回退哈希工具。
     * @param 密钥 密钥文本。
     * @param 消息 消息文本。
     * @return 十六进制，失败返回空串。
     */
    fun 计算HMAC(密钥: String, 消息: String): String {
        if (原生可用) {
            try {
                val 原生 = 原生计算HMAC(密钥, 消息)
                if (原生.isNotEmpty()) return 原生
            } catch (忽略: UnsatisfiedLinkError) {
                原生可用 = false
            } catch (忽略: Exception) {
            }
        }
        return try {
            哈希工具.计算字符串HMAC(密钥, 消息)
        } catch (忽略: Exception) {
            ""
        }
    }

    // 以下为 JNI 外部声明，由安全桥接.cpp 实现，请勿改名。
    external fun 原生计算文件哈希(路径: String): String
    external fun 原生沙箱评分(): Int
    external fun 原生沙箱报告(): String
    external fun 原生原子写入文本(路径: String, 内容: String): Long
    external fun 原生规范化路径(原始: String): String
    external fun 原生计算HMAC(密钥: String, 消息: String): String
    external fun 桥接版本(): String
}
