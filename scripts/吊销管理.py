#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""VineOS 设备吊销列表管理。
某设备作恶时，仅将其设备标识加入吊销列表并重新签名分发；
其他设备的许可证不受影响，实现单设备作恶 blast radius 隔离。
用法：
  python3 scripts/吊销管理.py 吊销 --根密钥 根密钥.json --设备标识 作恶设备 --列表 吊销列表.json
  python3 scripts/吊销管理.py 恢复 --根密钥 根密钥.json --设备标识 改过设备 --列表 吊销列表.json
  python3 scripts/吊销管理.py 查询 --列表 吊销列表.json --设备标识 某设备
吊销列表文件为根密钥签名的 JSON，客户端校验签名后强制执行。
"""
import argparse
import json
import os
import time
from pathlib import Path

工作目录 = Path(__file__).resolve().parent.parent


def 断言目录内(路径: Path) -> Path:
    真实 = 路径.resolve()
    if os.path.commonpath([str(真实), str(工作目录)]) != str(工作目录):
        raise SystemExit(f"拒绝目录外路径: {路径}")
    return 真实


def 规范字节(列表体: dict) -> bytes:
    return json.dumps(列表体, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def 载入列表(列表路径: Path) -> dict:
    列表路径 = 断言目录内(列表路径)
    if 列表路径.exists():
        return json.loads(列表路径.read_text(encoding="utf-8"))
    return {"列表": {"版本": 1, "吊销设备": [], "更新时间": 0}, "根签名hex": ""}


def 落盘签名列表(根密钥路径: Path, 列表体: dict, 列表路径: Path):
    from cryptography.hazmat.primitives.asymmetric import ed25519
    根数据 = json.loads(断言目录内(根密钥路径).read_text(encoding="utf-8"))
    私钥 = ed25519.Ed25519PrivateKey.from_private_bytes(bytes.fromhex(根数据["根私钥hex"]))
    列表体["更新时间"] = int(time.time())
    列表体["吊销设备"] = sorted(set(列表体["吊销设备"]))
    签名 = 私钥.sign(规范字节(列表体))
    断言目录内(列表路径).write_text(
        json.dumps({"列表": 列表体, "根签名hex": 签名.hex()}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"吊销列表已更新并签名: {列表路径}（共 {len(列表体['吊销设备'])} 台吊销）")


def 主() -> int:
    解析器 = argparse.ArgumentParser(description=__doc__)
    子命令 = 解析器.add_subparsers(dest="命令", required=True)
    吊销命令 = 子命令.add_parser("吊销")
    吊销命令.add_argument("--根密钥", required=True, type=Path)
    吊销命令.add_argument("--设备标识", required=True)
    吊销命令.add_argument("--列表", required=True, type=Path)
    恢复命令 = 子命令.add_parser("恢复")
    恢复命令.add_argument("--根密钥", required=True, type=Path)
    恢复命令.add_argument("--设备标识", required=True)
    恢复命令.add_argument("--列表", required=True, type=Path)
    查询命令 = 子命令.add_parser("查询")
    查询命令.add_argument("--列表", required=True, type=Path)
    查询命令.add_argument("--设备标识", required=True)
    参数 = 解析器.parse_args()
    if 参数.命令 == "查询":
        数据 = 载入列表(参数.列表)
        在列 = 参数.设备标识 in 数据["列表"]["吊销设备"]
        print(f"{参数.设备标识}: {'已吊销' if 在列 else '正常'}")
        return 0 if not 在列 else 2
    数据 = 载入列表(参数.列表)
    列表体 = 数据["列表"]
    if 参数.命令 == "吊销":
        if 参数.设备标识 not in 列表体["吊销设备"]:
            列表体["吊销设备"].append(参数.设备标识)
            print(f"已吊销设备: {参数.设备标识}（仅该设备受影响）")
        else:
            print(f"设备已在吊销列表中: {参数.设备标识}")
    else:
        列表体["吊销设备"] = [标识 for 标识 in 列表体["吊销设备"] if 标识 != 参数.设备标识]
        print(f"已恢复设备: {参数.设备标识}")
    落盘签名列表(参数.根密钥, 列表体, 参数.列表)
    return 0


if __name__ == "__main__":
    raise SystemExit(主())
