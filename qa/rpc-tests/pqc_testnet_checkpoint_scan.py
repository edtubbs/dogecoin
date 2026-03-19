#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
from datetime import datetime, timezone
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


DEFAULT_TESTNET_CHECKPOINT_HEIGHT = 5900000


def write_log_file(path: str, fields: Dict[str, str]) -> None:
    with open(path, "w", encoding="utf-8") as log_file:
        for key in sorted(fields.keys()):
            log_file.write(f"{key}: {fields[key]}\n")


def default_output_log_path(txid: str) -> str:
    return os.path.abspath(f"core-e2e-validation-{txid}.log")


def utc_now_z() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def pick_field(cli_value: Optional[str], log_values: Dict[str, str], *keys: str) -> Optional[str]:
    if cli_value:
        return cli_value
    for key in keys:
        value = log_values.get(key)
        if value:
            return value
    return None


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
    tag = b"FLC1"
    return (bytes([0x6A, 0x24]) + tag + commitment).hex()


def compute_commitment_hex(pubkey_hex: str, signature_hex: str) -> str:
    pubkey = normalize_hex(pubkey_hex)
    signature = normalize_hex(signature_hex)
    return hashlib.sha256(pubkey + signature).hexdigest()


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
    raise RuntimeError("timed out waiting for RPC startup")


def wait_for_sync(rpc: AuthServiceProxy, min_height: int, timeout: int) -> int:
    deadline = time.time() + timeout
    height = -1
    while time.time() < deadline:
        height = rpc.getblockcount()
        if height >= min_height:
            return height
        time.sleep(2.0)
    raise RuntimeError(f"timed out waiting for sync to height {min_height} (current: {height})")


def did_reach_checkpoint(current_height: int, checkpoint_height: int) -> bool:
    return current_height >= 0 and current_height >= checkpoint_height


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run a node with checkpoints and validate PQC commitment transaction from Core end-to-end inputs."
    )
    parser.add_argument("--srcdir", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "src")))
    parser.add_argument("--dogecoind", help="Path to dogecoind binary (defaults to --srcdir/dogecoind)")
    parser.add_argument("--log-file", help="Optional key/value log with tx and PQC fields")
    parser.add_argument("--txid", help="Transaction ID to scan on-chain")
    parser.add_argument("--txis", help="Alias for --txid from libdogecoin E2E scripts")
    parser.add_argument("--height", help="Block height containing txid")
    parser.add_argument("--commitment-type", help="PQC commitment type/tag from libdogecoin E2E scripts (FLC1)")
    parser.add_argument("--tag", help="Alias for --commitment-type")
    parser.add_argument("--pubkey-hex", help="PQC public key hex")
    parser.add_argument("--pubkey", help="Alias for --pubkey-hex")
    parser.add_argument("--signature-hex", help="PQC signature hex")
    parser.add_argument("--signature", help="Alias for --signature-hex")
    parser.add_argument("--wallet-address", help="Optional wallet/testnet address expected in tx outputs")
    parser.add_argument("--dogecoin-testnet-wallet-address", help="Alias for --wallet-address from libdogecoin E2E scripts")
    parser.add_argument("--checkpoint-height", type=int, default=DEFAULT_TESTNET_CHECKPOINT_HEIGHT, help="Checkpoint height to verify")
    parser.add_argument("--sync-timeout", type=int, default=900, help="Seconds to wait for node startup/sync")
    parser.add_argument("--network", choices=["testnet", "regtest"], default="testnet")
    parser.add_argument("--datadir", help="Optional datadir for node (created if missing)")
    parser.add_argument("--rpc-port", type=int)
    parser.add_argument("--p2p-port", type=int)
    parser.add_argument("--addnode", action="append", default=[], help="Optional addnode peers (repeatable)")
    parser.add_argument("--output-log", help="Write end-to-end run log to this file path")
    parser.add_argument("--nocleanup", action="store_true", help="Do not clean temporary datadir on exit")
    args = parser.parse_args()

    log_values: Dict[str, str] = {}
    if args.log_file:
        log_values = parse_log_file(args.log_file)

    commitment_type_raw = pick_field(args.commitment_type or args.tag, log_values, "commitment_type", "tag")
    algo = infer_algo(commitment_type_raw or "")
    if algo is None:
        raise ValueError("missing/invalid commitment type/tag (use FLC1 via --commitment-type/--tag or provide commitment_type/tag in --log-file)")

    txid = pick_field(args.txid or args.txis, log_values, "txid", "txis")
    height_raw = pick_field(args.height, log_values, "height")
    pubkey_hex = pick_field(args.pubkey_hex or args.pubkey, log_values, "pubkey_hex", "pubkey")
    signature_hex = pick_field(args.signature_hex or args.signature, log_values, "signature_hex", "signature")
    if not txid:
        raise ValueError("missing txid (use --txid or provide txid in --log-file)")
    if not height_raw:
        raise ValueError("missing height (use --height or provide height in --log-file)")
    if not pubkey_hex:
        raise ValueError("missing pubkey_hex/pubkey (use --pubkey-hex or provide pubkey in --log-file)")
    if not signature_hex:
        raise ValueError("missing signature_hex/signature (use --signature-hex or provide signature in --log-file)")
    try:
        tx_height = int(height_raw)
    except ValueError as exc:
        raise ValueError(f"invalid height in log: {height_raw}") from exc

    normalized_pubkey_hex = normalize_hex(pubkey_hex).hex()
    normalized_signature_hex = normalize_hex(signature_hex).hex()
    commitment_hex = compute_commitment_hex(normalized_pubkey_hex, normalized_signature_hex)
    expected_script_hex = compute_pqc_script(algo, normalized_pubkey_hex, normalized_signature_hex)
    logged_script_hex = log_values.get("script_pub_key_hex") or log_values.get("script_pub_key")
    if logged_script_hex and normalize_hex(logged_script_hex).hex() != expected_script_hex:
        raise ValueError("log script_pub_key_hex does not match computed PQC script")

    wallet_address = pick_field(
        args.wallet_address or args.dogecoin_testnet_wallet_address,
        log_values,
        "wallet_address",
        "dogecoin_testnet_wallet_address",
        "dogecoin_wallet_address",
        "address",
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
    default_rpc_port = 18383 if args.network == "testnet" else 18332
    default_p2p_port = 22556 if args.network == "testnet" else 18444
    rpc_port = args.rpc_port if args.rpc_port is not None else default_rpc_port
    p2p_port = args.p2p_port if args.p2p_port is not None else default_p2p_port
    rpc_url = f"http://{rpc_user}:{rpc_pass}@127.0.0.1:{rpc_port}"

    command = [
        dogecoind,
        f"-{args.network}",
        f"-checkpoints={1 if args.network == 'testnet' else 0}",
        "-txindex=1",
        f"-datadir={datadir}",
        "-server",
        "-listen=0",
        "-discover=0",
        f"-dnsseed={1 if args.network == 'testnet' else 0}",
        "-upnp=0",
        f"-rpcuser={rpc_user}",
        f"-rpcpassword={rpc_pass}",
        f"-rpcport={rpc_port}",
        f"-port={p2p_port}",
    ]
    for peer in args.addnode:
        command.append(f"-addnode={peer}")

    process = subprocess.Popen(command)
    rpc = None
    checkpoint_hash = ""
    current_height = -1
    wait_height = max(tx_height, args.checkpoint_height)
    match_on_chain_script = False
    match_on_chain_address = wallet_address is None
    block_hash = ""
    output_log_path = args.output_log or default_output_log_path(txid)
    try:
        try:
            rpc = wait_for_rpc(rpc_url, args.sync_timeout)
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

            match_on_chain_script = tx_contains_script(tx, expected_script_hex)
            if not match_on_chain_script:
                raise RuntimeError("computed PQC script not found in tx outputs")
            print("match_on_chain_script: true")

            if wallet_address:
                match_on_chain_address = tx_contains_address(tx, wallet_address)
                if not match_on_chain_address:
                    raise RuntimeError(f"wallet address {wallet_address} not present in tx outputs")
                print("match_on_chain_address: true")
        except Exception as exc:
            write_log_file(
                output_log_path,
                    {
                        "block_hash": block_hash,
                        "checkpoint_hash": checkpoint_hash,
                        "checkpoint_height": str(args.checkpoint_height),
                        "checkpoint_sync_reached": "true" if did_reach_checkpoint(current_height, args.checkpoint_height) else "false",
                        "checkpoint_sync_target_height": str(args.checkpoint_height),
                        "checkpoints_enabled": "true" if args.network == "testnet" else "false",
                        "commitment_hex": commitment_hex,
                        "commitment_type": "FLC1",
                        "date_utc": utc_now_z(),
                        "height": str(tx_height),
                        "match": "false",
                        "match_on_chain_address": "true" if match_on_chain_address else "false",
                        "match_on_chain_script": "true" if match_on_chain_script else "false",
                        "core_tx_validation": "false",
                        "network": args.network,
                        "notes": str(exc),
                        "pubkey_hex": normalized_pubkey_hex,
                        "recomputed_commitment_hex": commitment_hex,
                        "script_pub_key_hex": expected_script_hex,
                        "signature_hex": normalized_signature_hex,
                        "sync_target_height": str(wait_height),
                        "synced_height": str(current_height),
                        "txid": txid,
                    },
                )
            print(f"output_log: {output_log_path}")
            raise
        write_log_file(
            output_log_path,
                {
                    "block_hash": block_hash,
                    "checkpoint_hash": checkpoint_hash,
                    "checkpoint_height": str(args.checkpoint_height),
                    "checkpoint_sync_reached": "true" if did_reach_checkpoint(current_height, args.checkpoint_height) else "false",
                    "checkpoint_sync_target_height": str(args.checkpoint_height),
                    "checkpoints_enabled": "true" if args.network == "testnet" else "false",
                    "commitment_hex": commitment_hex,
                    "commitment_type": "FLC1",
                    "date_utc": utc_now_z(),
                    "height": str(tx_height),
                    "match": "true",
                    "match_on_chain_address": "true" if match_on_chain_address else "false",
                    "match_on_chain_script": "true" if match_on_chain_script else "false",
                    "core_tx_validation": "true",
                    "network": args.network,
                    "notes": "core e2e checkpoint scan successful",
                    "pubkey_hex": normalized_pubkey_hex,
                    "recomputed_commitment_hex": commitment_hex,
                    "script_pub_key_hex": expected_script_hex,
                    "signature_hex": normalized_signature_hex,
                    "sync_target_height": str(wait_height),
                    "synced_height": str(current_height),
                    "txid": txid,
                    "wallet_address": wallet_address or "",
                },
            )
        print(f"output_log: {output_log_path}")
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
