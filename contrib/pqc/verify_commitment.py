#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import hashlib
import sys
from typing import Dict, Optional


def normalize_hex(value: str) -> bytes:
    value = value.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    if len(value) % 2 != 0:
        raise ValueError("hex value must have even length")
    return bytes.fromhex(value)


def parse_log_file(path: str) -> Dict[str, str]:
    parsed: Dict[str, str] = {}
    with open(path, "r", encoding="utf-8") as log_file:
        for raw_line in log_file:
            line = raw_line.strip()
            if not line or ":" not in line:
                continue
            key, value = line.split(":", 1)
            parsed[key.strip().lower()] = value.strip()
    return parsed


def infer_algo(raw_type: str) -> Optional[str]:
    normalized = raw_type.strip().lower()
    if normalized in ("falcon512", "flc1", "falcon-512"):
        return "falcon512"
    if normalized in ("dilithium2", "dil2", "dilithium-2"):
        return "dilithium2"
    if normalized in ("raccoong44", "rcg4", "raccoong", "raccoon-g-44"):
        return "raccoong44"
    return None


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify libdogecoin-compatible PQC commitment and emit canonical script."
    )
    parser.add_argument("--algo", choices=["falcon512", "dilithium2", "raccoong44"])
    parser.add_argument("--pubkey", help="PQC public key hex")
    parser.add_argument("--signature", help="PQC signature hex")
    parser.add_argument("--log-file", help="Path to E2E validation log template file")
    parser.add_argument("--expected-commitment", help="Expected commitment hex (optional)")
    parser.add_argument("--expected-script", help="Expected scriptPubKey hex (optional)")
    args = parser.parse_args()

    log_values: Dict[str, str] = {}
    if args.log_file:
        log_values = parse_log_file(args.log_file)

    algo = args.algo or infer_algo(log_values.get("commitment_type", "")) or infer_algo(log_values.get("tag", ""))
    pubkey_hex = args.pubkey or log_values.get("pubkey_hex") or log_values.get("pubkey")
    signature_hex = args.signature or log_values.get("signature_hex") or log_values.get("signature")
    expected_commitment_hex = args.expected_commitment or log_values.get("commitment_hex") or log_values.get("commitment")
    expected_script_hex = args.expected_script or log_values.get("script_pub_key_hex") or log_values.get("script_pub_key")

    if algo is None:
        raise ValueError("missing algorithm (provide --algo or commitment_type/tag in --log-file)")
    if not pubkey_hex:
        raise ValueError("missing pubkey (provide --pubkey or pubkey_hex/pubkey in --log-file)")
    if not signature_hex:
        raise ValueError("missing signature (provide --signature or signature_hex/signature in --log-file)")

    pubkey = normalize_hex(pubkey_hex)
    signature = normalize_hex(signature_hex)

    commitment = hashlib.sha256(pubkey + signature).digest()
    tag_map = {
        "falcon512": b"FLC1",
        "dilithium2": b"DIL2",
        "raccoong44": b"RCG4",
    }
    tag = tag_map[algo]
    script = bytes([0x6A, 0x24]) + tag + commitment

    print(f"tag: {tag.decode('ascii')}")
    print(f"commitment: {commitment.hex()}")
    print(f"script_pub_key: {script.hex()}")

    if expected_commitment_hex:
        expected_commitment = normalize_hex(expected_commitment_hex).hex()
        commitment_matches = expected_commitment == commitment.hex()
        print(f"match_commitment: {'true' if commitment_matches else 'false'}")
        if not commitment_matches:
            print("error: computed commitment does not match expected commitment", file=sys.stderr)
            return 1

    if expected_script_hex:
        expected_script = normalize_hex(expected_script_hex).hex()
        script_matches = expected_script == script.hex()
        print(f"match_script: {'true' if script_matches else 'false'}")
        if not script_matches:
            print("error: computed script does not match expected script", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
