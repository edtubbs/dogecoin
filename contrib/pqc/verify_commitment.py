#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import hashlib
import sys


def normalize_hex(value: str) -> bytes:
    value = value.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    if len(value) % 2 != 0:
        raise ValueError("hex value must have even length")
    return bytes.fromhex(value)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify libdogecoin-compatible PQC commitment and emit canonical script."
    )
    parser.add_argument("--algo", choices=["falcon512", "dilithium2"], required=True)
    parser.add_argument("--pubkey", required=True, help="PQC public key hex")
    parser.add_argument("--signature", required=True, help="PQC signature hex")
    args = parser.parse_args()

    pubkey = normalize_hex(args.pubkey)
    signature = normalize_hex(args.signature)

    commitment = hashlib.sha256(pubkey + signature).digest()
    tag = b"FLC1" if args.algo == "falcon512" else b"DIL2"
    script = bytes([0x6A, 0x24]) + tag + commitment

    print(f"tag: {tag.decode('ascii')}")
    print(f"commitment: {commitment.hex()}")
    print(f"script_pub_key: {script.hex()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
