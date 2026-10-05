#!/usr/bin/env python3
"""从猫猫云加密的 config.yaml 提取明文 Clash/mihomo 配置。

用法:
    python maomaoyun_clash.py
    python maomaoyun_clash.py <加密文件> <明文输出>

默认读取本脚本同目录的 config.yaml，写出 config.decrypted.yaml。
输出文件含节点密码，不要外传或提交到仓库。
"""

from __future__ import annotations

import base64
import re
import sys
from pathlib import Path

from cryptography.hazmat.primitives import padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

KEY = b"4422a60e08c97f30"
IV = b"8c97f304422a60e0"


def extract_clash_config(input_path: Path) -> bytes:
    raw = re.sub(r"\s+", "", input_path.read_text(encoding="utf-8"))
    encrypted = base64.b64decode(raw)
    if not encrypted or len(encrypted) % 16 != 0:
        raise ValueError("加密内容不是合法的 AES 块")

    decryptor = Cipher(algorithms.AES(KEY), modes.CBC(IV)).decryptor()
    padded = decryptor.update(encrypted) + decryptor.finalize()
    unpadder = padding.PKCS7(128).unpadder()
    middle = unpadder.update(padded) + unpadder.finalize()
    middle_text = re.sub(r"\s+", "", middle.decode("utf-8"))
    yaml_bytes = base64.b64decode(middle_text)
    text = yaml_bytes.decode("utf-8")
    if "\nproxies:" not in text and not text.startswith("proxies:"):
        raise ValueError("解密结果里没有 proxies 段")
    return yaml_bytes


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    input_path = Path(sys.argv[1]) if len(sys.argv) > 1 else script_dir / "config.yaml"
    output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else script_dir / "config.decrypted.yaml"
    yaml_bytes = extract_clash_config(input_path)
    output_path.write_bytes(yaml_bytes)
    text = yaml_bytes.decode("utf-8")
    anytls = len(re.findall(r"type:\s*anytls", text))
    print(f"wrote {output_path}")
    print(f"bytes {len(yaml_bytes)}")
    print(f"anytls {anytls}")


if __name__ == "__main__":
    main()
