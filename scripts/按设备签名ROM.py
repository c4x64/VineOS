#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""VineOS 按设备 ROM 许可证签发与校验。
每台设备拥有独立密钥与标识；服务端用根密钥为（设备，ROM）签发许可证。
某设备作恶被吊销时，仅该设备许可证失效，不影响其他设备。
中文注释与标识；外部依赖仅 cryptography。
用法：
  签发: python3 scripts/按设备签名ROM.py 签发 --根密钥 根密钥.json --rom 包.vrom
        --rom标识 vine-rom-7 --设备标识 设备A --设备公钥hex <hex> --输出 许可证.json [--有效天数 365]
  校验: python3 scripts/按设备签名ROM.py 校验 --根公钥hex <hex> --rom 包.vrom --许可证 许可证.json
"""
import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

工作目录 = Path(__file__).resolve().parent.parent
分块 = 1024 * 1024


def 断言目录内(路径: Path) -> Path:
    真实 = 路径.resolve()
    if os.path.commonpath([str(真实), str(工作目录)]) != str(工作目录):
        raise SystemExit(f"拒绝目录外路径: {路径}")
    return 真实


def 文件哈希(路径: Path) -> str:
    摘要 = hashlib.sha256()
    with 路径.open("rb") as 输入:
        while True:
            数据 = 输入.read(分块)
            if not 数据:
                break
            摘要.update(数据)
    return 摘要.hexdigest()


def 规范字节(许可证体: dict) -> bytes:
    return json.dumps(许可证体, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def 载入根私钥(根密钥路径: Path):
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ed25519
    数据 = json.loads(断言目录内(根密钥路径).read_text(encoding="utf-8"))
    return ed25519.Ed25519PrivateKey.from_private_bytes(bytes.fromhex(数据["根私钥hex"]))


def 载入根公钥(公钥hex: str):
    from cryptography.hazmat.primitives.asymmetric import ed25519
    return ed25519.Ed25519PublicKey.from_public_bytes(bytes.fromhex(公钥hex))


def 签发(根密钥: Path, 包: Path, rom标识: str, 设备标识: str, 设备公钥hex: str, 输出: Path, 有效天数: int):
    if any(c in rom标识 for c in "/\\") or any(c in 设备标识 for c in "/\\.."):
        raise SystemExit("非法标识")
    包 = 断言目录内(包)
    输出 = 断言目录内(输出)
    if not 包.is_file():
        raise SystemExit(f"ROM 包不存在: {包}")
    包哈希 = 文件哈希(包)
    签发秒 = int(time.time())
    许可证体 = {
        "版本": 1,
        "rom标识": rom标识,
        "rom包哈希": 包哈希,
        "设备标识": 设备标识,
        "设备公钥hex": 设备公钥hex.lower(),
        "签发时间": 签发秒,
        "过期时间": 签发秒 + 有效天数 * 86400,
    }
    签名 = 载入根私钥(根密钥).sign(规范字节(许可证体))
    输出.parent.mkdir(parents=True, exist_ok=True)
    输出.write_text(json.dumps({"许可证": 许可证体, "根签名hex": 签名.hex()}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"许可证已签发: {输出}")
    print(f"ROM哈希: {包哈希}")


def 校验(根公钥hex: str, 包: Path, 许可证: Path, 设备标识: str = "") -> int:
    from cryptography.exceptions import InvalidSignature
    包 = 断言目录内(包)
    数据 = json.loads(断言目录内(许可证).read_text(encoding="utf-8"))
    许可证体, 签名hex = 数据["许可证"], 数据["根签名hex"]
    try:
        载入根公钥(根公钥hex).verify(bytes.fromhex(签名hex), 规范字节(许可证体))
    except InvalidSignature:
        print("校验失败：根签名无效")
        return 1
    实际哈希 = 文件哈希(包)
    if 实际哈希 != 许可证体["rom包哈希"]:
        print(f"校验失败：ROM包哈希不符（期望 {许可证体['rom包哈希']}，实际 {实际哈希}）")
        return 1
    现在 = int(time.time())
    if 现在 > int(许可证体["过期时间"]):
        print("校验失败：许可证已过期")
        return 1
    if 设备标识 and 设备标识 != 许可证体["设备标识"]:
        print("校验失败：设备标识不符")
        return 1
    print(f"校验通过：{许可证体['rom标识']} 授权给 {许可证体['设备标识']}")
    return 0


def 主() -> int:
    解析器 = argparse.ArgumentParser(description=__doc__)
    子命令 = 解析器.add_subparsers(dest="命令", required=True)
    签发命令 = 子命令.add_parser("签发")
    签发命令.add_argument("--根密钥", required=True, type=Path)
    签发命令.add_argument("--rom", required=True, type=Path)
    签发命令.add_argument("--rom标识", required=True)
    签发命令.add_argument("--设备标识", required=True)
    签发命令.add_argument("--设备公钥hex", required=True)
    签发命令.add_argument("--输出", required=True, type=Path)
    签发命令.add_argument("--有效天数", type=int, default=365)
    校验命令 = 子命令.add_parser("校验")
    校验命令.add_argument("--根公钥hex", required=True)
    校验命令.add_argument("--rom", required=True, type=Path)
    校验命令.add_argument("--许可证", required=True, type=Path)
    校验命令.add_argument("--设备标识", default="")
    参数 = 解析器.parse_args()
    if 参数.命令 == "签发":
        签发(参数.根密钥, 参数.rom, 参数.rom标识, 参数.设备标识, 参数.设备公钥hex, 参数.输出, 参数.有效天数)
        return 0
    return 校验(参数.根公钥hex, 参数.rom, 参数.许可证, 参数.设备标识)


if __name__ == "__main__":
    raise SystemExit(主())
