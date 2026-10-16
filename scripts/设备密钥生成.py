#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""VineOS 按设备签名根密钥与设备密钥生成器。
中文注释与标识；仅标准库 argparse/json/pathlib 与第三方 cryptography 为外部依赖。
用法：
  python3 scripts/设备密钥生成.py 生成根密钥 --输出 根密钥.json
  python3 scripts/设备密钥生成.py 生成设备密钥 --设备标识 设备A --输出目录 设备密钥/
  python3 scripts/设备密钥生成.py 设备指纹 --公钥 设备公钥.hex
所有写出路径必须位于当前工作目录内，脚本拒绝目录外的输出目标。
"""
import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

工作目录 = Path(__file__).resolve().parent.parent


def 断言目录内(路径: Path) -> Path:
    真实 = 路径.resolve()
    if os.path.commonpath([str(真实), str(工作目录)]) != str(工作目录):
        raise SystemExit(f"拒绝目录外路径: {路径}")
    return 真实


def 需cryptography():
    try:
        from cryptography.hazmat.primitives.asymmetric import ed25519
        return ed25519
    except ImportError:
        raise SystemExit("缺少 cryptography 库：pip install cryptography")


def 公钥指纹(公钥字节: bytes) -> str:
    return hashlib.sha256(公钥字节).hexdigest()


def 生成根密钥(输出: Path):
    ed25519 = 需cryptography()
    from cryptography.hazmat.primitives import serialization
    输出 = 断言目录内(输出)
    私钥 = ed25519.Ed25519PrivateKey.generate()
    私钥字节 = 私钥.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())
    公钥字节 = 私钥.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    输出.parent.mkdir(parents=True, exist_ok=True)
    输出.write_text(json.dumps({
        "算法": "Ed25519",
        "根私钥hex": 私钥字节.hex(),
        "根公钥hex": 公钥字节.hex(),
        "根公钥指纹": 公钥指纹(公钥字节),
    }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.chmod(输出, 0o600)
    print(f"根密钥已写入 {输出}（权限 600，请离线保管，勿提交）")


def 生成设备密钥(设备标识: str, 输出目录: Path):
    ed25519 = 需cryptography()
    输出目录 = 断言目录内(输出目录)
    if not 设备标识 or any(c in 设备标识 for c in "/\\.."):
        raise SystemExit("非法设备标识")
    私钥 = ed25519.Ed25519PrivateKey.generate()
    from cryptography.hazmat.primitives import serialization as 序列化
    私钥字节 = 私钥.private_bytes(序列化.Encoding.Raw, 序列化.PrivateFormat.Raw, 序列化.NoEncryption())
    公钥字节 = 私钥.public_key().public_bytes(序列化.Encoding.Raw, 序列化.PublicFormat.Raw)
    输出目录.mkdir(parents=True, exist_ok=True)
    私钥路径 = 输出目录 / f"{设备标识}.设备私钥.hex"
    公钥路径 = 输出目录 / f"{设备标识}.设备公钥.hex"
    私钥路径.write_text(私钥字节.hex() + "\n", encoding="utf-8")
    公钥路径.write_text(公钥字节.hex() + "\n", encoding="utf-8")
    os.chmod(私钥路径, 0o600)
    print(f"设备标识: {设备标识}")
    print(f"设备指纹: {公钥指纹(公钥字节)}")
    print(f"私钥: {私钥路径}（权限 600） 公钥: {公钥路径}")


def 主() -> int:
    解析器 = argparse.ArgumentParser(description=__doc__)
    子命令 = 解析器.add_subparsers(dest="命令", required=True)
    根命令 = 子命令.add_parser("生成根密钥")
    根命令.add_argument("--输出", required=True, type=Path)
    设备命令 = 子命令.add_parser("生成设备密钥")
    设备命令.add_argument("--设备标识", required=True)
    设备命令.add_argument("--输出目录", required=True, type=Path)
    指纹命令 = 子命令.add_parser("设备指纹")
    指纹命令.add_argument("--公钥", required=True, type=Path)
    参数 = 解析器.parse_args()
    if 参数.命令 == "生成根密钥":
        生成根密钥(参数.输出)
    elif 参数.命令 == "生成设备密钥":
        生成设备密钥(参数.设备标识, 参数.输出目录)
    elif 参数.命令 == "设备指纹":
        原始 = bytes.fromhex(断言目录内(参数.公钥).read_text(encoding="utf-8").strip())
        print(公钥指纹(原始))
    return 0


if __name__ == "__main__":
    raise SystemExit(主())
