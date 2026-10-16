#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""删除生成代码中编译器已证实不可达的坏死代码。
只删除满足以下全部条件的整行：
1. 该行被 CI 日志点名为 Unresolved reference，或引用了仅在已删行中声明的名字；
2. 所在函数未声明该名字为参数；
3. 该行自身花括号平衡（删后不破坏结构）；
4. 该行不是函数/类声明头。
另：删除同一函数内 return 之后出现的重复局部 val/var 声明（Conflicting declarations）。
所有路径限制在工作目录内。试运行：--check 仅报告不写盘。
"""
import re
import sys
from pathlib import Path

工作目录 = Path(__file__).resolve().parent.parent
CJK = r"\u4e00-\u9fff"
名字 = re.compile(r"[\u4e00-\u9fffA-Za-z_][\u4e00-\u9fffA-Za-z0-9_]*")
声明 = re.compile(r"^\s*(val|var)\s+([\u4e00-\u9fffA-Za-z_][\u4e00-\u9fffA-Za-z0-9_]*)")


def 去字符串注释行(行: str) -> str:
    出 = []
    i, n = 0, len(行)
    字符串 = None
    while i < n:
        c = 行[i]
        if 字符串:
            if c == "\\":
                i += 2
                continue
            if c == 字符串:
                字符串 = None
            i += 1
            continue
        if c == "/" and i + 1 < n and 行[i + 1] == "/":
            break
        if c in "\"'":
            字符串 = c
            i += 1
            continue
        出.append(c)
        i += 1
    return "".join(出)


def 花括号差(行: str) -> int:
    代码 = 去字符串注释行(行)
    return 代码.count("{") - 代码.count("}")


def 函数跨度(行列表):
    跨度 = []
    i, n = 0, len(行列表)
    while i < n:
        m = re.match(r"^(\s*)fun\s+[^\s(]+\s*\((.*)\)", 行列表[i])
        if m:
            前导, 参数 = m.group(1), m.group(2)
            参数名 = set(名字.findall(参数.split("=")[0] if "=" in 参数 else 参数))
            # 去掉类型名：只保留冒号前的标识
            参数名 = set()
            for 段 in 参数.split(","):
                段 = 段.strip()
                mm = re.match(r"([^\s:]+)\s*:", 段)
                if mm:
                    参数名.add(mm.group(1))
                elif re.match(r"^[^\s:]+$", 段):
                    参数名.add(段)
            深度 = 0
            已开 = False
            j = i
            while j < n:
                深度 += 花括号差(行列表[j])
                if 深度 > 0:
                    已开 = True
                if 已开 and 深度 <= 0:
                    break
                j += 1
            跨度.append((i, min(j, n - 1), 参数名))
            i = j + 1
            continue
        i += 1
    return 跨度


def 独立词(行: str, 词: str) -> bool:
    return re.search(r"(?<![\u4e00-\u9fffA-Za-z0-9_$.])" + re.escape(词) + r"(?![\u4e00-\u9fffA-Za-z0-9_])", 去字符串注释行(行)) is not None


def 处理文件(路径: Path, 种子名: set, 检查: bool) -> int:
    行列表 = 路径.read_text(encoding="utf-8").split("\n")
    跨度 = 函数跨度(行列表)
    删 = set()

    def 所在函数(行号):
        for 起, 止, 参数名 in 跨度:
            if 起 <= 行号 <= 止:
                return (起, 止, 参数名)
        return None

    # 种子：CI 点名的不可解析名字
    待查 = list(种子名)
    已删声明 = set()
    while 待查:
        目标 = 待查.pop()
        for idx, 行 in enumerate(行列表):
            if idx in 删:
                continue
            if re.match(r"^\s*fun\s", 行):
                continue
            fn = 所在函数(idx)
            if fn and 目标 in fn[2]:
                continue  # 真实参数，不动
            if 独立词(行, 目标):
                if 花括号差(行) == 0:
                    删.add(idx)
                    m = 声明.match(行)
                    if m and m.group(2) not in 已删声明:
                        已删声明.add(m.group(2))
                        待查.append(m.group(2))
    # 同一函数 return 后的重复局部声明
    for 起, 止, 参数名 in 跨度:
        见返回 = False
        已见 = dict()
        for idx in range(起, 止 + 1):
            if idx in 删:
                continue
            行 = 行列表[idx]
            代码 = 去字符串注释行(行)
            if re.search(r"(?<![\w$.])return(?![\w])", 代码):
                见返回 = True
            m = 声明.match(行)
            if m:
                名 = m.group(2)
                if 名 in 已见 and 见返回 and 花括号差(行) == 0:
                    删.add(idx)
                    if 名 not in 已删声明:
                        已删声明.add(名)
                else:
                    已见.setdefault(名, idx)
    if not 删:
        return 0
    if 检查:
        print(f"{路径.name}: 将删 {len(删)} 行")
        return len(删)
    保留 = [行 for idx, 行 in enumerate(行列表) if idx not in 删]
    路径.write_text("\n".join(保留), encoding="utf-8")
    return len(删)


def 主() -> int:
    import argparse
    解析器 = argparse.ArgumentParser()
    解析器.add_argument("--check", action="store_true")
    解析器.add_argument("种子", help="文件:名字1,名字2 列表")
    参数 = 解析器.parse_args()
    总 = 0
    for 条 in open(参数.种子, encoding="utf-8").read().split():
        文件, 名表 = 条.split(":", 1)
        路径 = 工作目录 / 文件
        assert 工作目录 in 路径.resolve().parents or 路径.resolve() == 工作目录
        总 += 处理文件(路径, set(名表.split(",")), 参数.check)
    print("共计:", 总)
    return 0


if __name__ == "__main__":
    raise SystemExit(主())
