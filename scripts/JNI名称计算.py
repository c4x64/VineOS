#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""按 JNI 规范计算静态注册函数名。
规则：包名点号变下划线；ASCII 字母数字保留；下划线变 _1；
其余字符按 UTF-16 码元输出 _0xxxx（小写十六进制）。
"""
import struct


def 转义(文本: str) -> str:
    输出 = []
    for 字符 in 文本:
        编码 = ord(字符)
        if 字符.isascii() and 字符.isalnum():
            输出.append(字符)
        elif 字符 == "_":
            输出.append("_1")
        else:
            原始 = 字符.encode("utf-16-be")
            for 偏移 in range(0, len(原始), 2):
                码元 = struct.unpack(">H", 原始[偏移:偏移 + 2])[0]
                输出.append("_0%04x" % 码元)
    return "".join(输出)


def 函数名(包: str, 类: str, 方法: str) -> str:
    return "Java_" + "_".join([转义(段) for 段 in 包.split(".")] + [转义(类), 转义(方法)])


if __name__ == "__main__":
    目标 = [
        ("com.hexadecinull.vineos.诊断", "诊断原生桥", "原生获取诊断摘要"),
        ("com.hexadecinull.vineos.诊断", "诊断原生桥", "原生获取性能摘要"),
        ("com.hexadecinull.vineos.诊断", "诊断原生桥", "原生获取内存摘要"),
        ("com.hexadecinull.vineos.诊断", "诊断原生桥", "原生获取线程摘要"),
        ("com.hexadecinull.vineos.诊断", "诊断原生桥", "原生获取错误摘要"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "原生计算文件哈希"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "原生沙箱评分"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "原生沙箱报告"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "原生原子写入文本"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "原生规范化路径"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "原生计算HMAC"),
        ("com.hexadecinull.vineos.安全", "原生安全桥接", "桥接版本"),
    ]
    for 包, 类, 方法 in 目标:
        print(方法 + "=" + 函数名(包, 类, 方法))
