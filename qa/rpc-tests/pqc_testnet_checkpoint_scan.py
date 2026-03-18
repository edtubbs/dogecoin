#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import errno
import hashlib
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Dict, Optional

sys.path.append(os.path.join(os.path.dirname(__file__), "test_framework"))
from authproxy import AuthServiceProxy, JSONRPCException  # pylint: disable=import-error


def normalize_hex(value: str) -> bytes:
    normalized = value.strip().lower()
    if normalized.startswith("0x"):
        normalized = normalized[2:]
    if len(normalized) % 2 != 0:
        raise ValueError("hex value must have even length")
    return bytes.fromhex(normalized)


def infer_algo(raw_type: str) -> Optional[str]:
    normalized = raw_type.strip().lower()
    if normalized in ("falcon512", "flc1", "falcon-512"):
        return "falcon512"
    if normalized in ("dilithium2", "dil2", "dilithium-2"):
        return "dilithium2"
    return None


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


def compute_pqc_script(algo: str, pubkey_hex: str, signature_hex: str) -> str:
    pubkey = normalize_hex(pubkey_hex)
    signature = normalize_hex(signature_hex)
    commitment = hashlib.sha256(pubkey + signature).digest()
    tag = b"FLC1" if algo == "falcon512" else b"DIL2"
    return (bytes([0x6A, 0x24]) + tag + commitment).hex()


def tx_contains_script(tx: Dict, script_hex: str) -> bool:
    needle = script_hex.lower()
    for vout in tx.get("vout", []):
        if vout.get("scriptPubKey", {}).get("hex", "").lower() == needle:
            return True
    return False


def tx_contains_address(tx: Dict, address: str) -> bool:
    needle = address.strip()
    for vout in tx.get("vout", []):
        addresses = vout.get("scriptPubKey", {}).get("addresses", [])
        if needle in addresses:
            return True
    return False


def wait_for_rpc(url: str, timeout: int) -> AuthServiceProxy:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            rpc = AuthServiceProxy(url, timeout=30)
            rpc.getblockcount()
            return rpc
        except OSError as exc:
            if exc.errno != errno.ECONNREFUSED:
                raise
        except JSONRPCException as exc:
            if exc.error.get("code") != -28:
                raise
        time.sleep(0.5)
    raise RuntimeError("timed out waiting for testnet RPC startup")


def wait_for_sync(rpc: AuthServiceProxy, min_height: int, timeout: int) -> int:
    deadline = time.time() + timeout
    height = -1
    while time.time() < deadline:
        height = rpc.getblockcount()
        if height >= min_height:
            return height
        time.sleep(2.0)
    raise RuntimeError(f"timed out waiting for sync to height {min_height} (current: {height})")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run a testnet node with checkpoints and validate PQC commitment transaction from log data."
    )
    parser.add_argument("--srcdir", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "src")))
    parser.add_argument("--dogecoind", help="Path to dogecoind binary (defaults to --srcdir/dogecoind)")
    parser.add_argument("--log-file", required=True, help="libdogecoin E2E log with tx and PQC fields")
    parser.add_argument("--checkpoint-height", type=int, default=5900000, help="Checkpoint height to verify")
    parser.add_argument("--sync-timeout", type=int, default=900, help="Seconds to wait for node startup/sync")
    parser.add_argument("--datadir", help="Optional datadir for testnet node (created if missing)")
    parser.add_argument("--rpc-port", type=int, default=18383)
    parser.add_argument("--p2p-port", type=int, default=22556)
    parser.add_argument("--addnode", action="append", default=[], help="Optional addnode peers (repeatable)")
    parser.add_argument("--nocleanup", action="store_true", help="Do not clean temporary datadir on exit")
    args = parser.parse_args()

    log_values = parse_log_file(args.log_file)
    algo = infer_algo(log_values.get("commitment_type", "")) or infer_algo(log_values.get("tag", ""))
    if algo is None:
        raise ValueError("missing commitment_type/tag in log")

    txid = log_values.get("txid")
    height_raw = log_values.get("height")
    pubkey_hex = log_values.get("pubkey_hex") or log_values.get("pubkey")
    signature_hex = log_values.get("signature_hex") or log_values.get("signature")
    if not txid:
        raise ValueError("missing txid in log")
    if not height_raw:
        raise ValueError("missing height in log")
    if not pubkey_hex:
        raise ValueError("missing pubkey_hex/pubkey in log")
    if not signature_hex:
        raise ValueError("missing signature_hex/signature in log")
    try:
        tx_height = int(height_raw)
    except ValueError as exc:
        raise ValueError(f"invalid height in log: {height_raw}") from exc

    expected_script_hex = compute_pqc_script(algo, pubkey_hex, signature_hex)
    logged_script_hex = log_values.get("script_pub_key_hex") or log_values.get("script_pub_key")
    if logged_script_hex and normalize_hex(logged_script_hex).hex() != expected_script_hex:
        raise ValueError("log script_pub_key_hex does not match computed PQC script")

    wallet_address = (
        log_values.get("wallet_address")
        or log_values.get("dogecoin_testnet_wallet_address")
        or log_values.get("address")
    )

    datadir_created = False
    datadir = args.datadir
    if not datadir:
        datadir = tempfile.mkdtemp(prefix="pqc-testnet-checkpoint-")
        datadir_created = True
    os.makedirs(datadir, exist_ok=True)

    rpc_user = f"pqcuser{random.randint(10000, 99999)}"
    rpc_pass = f"pqcpass{random.randint(100000, 999999)}"
    dogecoind = args.dogecoind or os.path.join(args.srcdir, "dogecoind")
    rpc_url = f"http://{rpc_user}:{rpc_pass}@127.0.0.1:{args.rpc_port}"

    command = [
        dogecoind,
        "-testnet",
        "-checkpoints=1",
        "-txindex=1",
        f"-datadir={datadir}",
        "-server",
        "-listen=0",
        "-discover=0",
        "-dnsseed=1",
        "-upnp=0",
        f"-rpcuser={rpc_user}",
        f"-rpcpassword={rpc_pass}",
        f"-rpcport={args.rpc_port}",
        f"-port={args.p2p_port}",
    ]
    for peer in args.addnode:
        command.append(f"-addnode={peer}")

    process = subprocess.Popen(command)
    rpc = None
    try:
        rpc = wait_for_rpc(rpc_url, args.sync_timeout)
        wait_height = max(tx_height, args.checkpoint_height)
        current_height = wait_for_sync(rpc, wait_height, args.sync_timeout)
        print(f"synced_height: {current_height}")

        checkpoint_hash = rpc.getblockhash(args.checkpoint_height)
        print(f"checkpoint_height: {args.checkpoint_height}")
        print(f"checkpoint_hash: {checkpoint_hash}")

        block_hash = rpc.getblockhash(tx_height)
        block = rpc.getblock(block_hash, 2)
        tx = None
        for block_tx in block.get("tx", []):
            if block_tx.get("txid") == txid:
                tx = block_tx
                break
        if tx is None:
            raise RuntimeError(f"txid {txid} not found in block {block_hash} at height {tx_height}")

        if not tx_contains_script(tx, expected_script_hex):
            raise RuntimeError("computed PQC script not found in tx outputs")
        print("match_on_chain_script: true")

        if wallet_address:
            if not tx_contains_address(tx, wallet_address):
                raise RuntimeError(f"wallet address {wallet_address} not present in tx outputs")
            print("match_on_chain_address: true")
    finally:
        if rpc is not None:
            try:
                rpc.stop()
            except Exception:
                pass
        process.wait(timeout=90)
        if datadir_created and not args.nocleanup:
            shutil.rmtree(datadir, ignore_errors=True)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
