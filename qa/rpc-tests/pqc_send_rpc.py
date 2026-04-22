#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Integration tests for the PQC wallet RPC commands:
  generatepqccommitment
  sendpqccommitment
  sendpqcreveal

These tests require a running dogecoind node compiled with ENABLE_LIBOQS.
If the node binary does not expose the PQC RPCs the test is skipped
gracefully (exit 0 with a SKIP message).

Test scenarios
--------------
1. generatepqccommitment — correct output format and known-vector values.
2. sendpqccommitment     — broadcasts TX_C; returns a 64-hex-char txid.
3. sendpqcreveal         — broadcasts TX_R from TX_C; returns a 64-hex-char txid.
4. TX_R confirmable      — mining a block confirms TX_R.
5. Error cases           — invalid address / algorithm / hex / missing TX_C.
"""

import hashlib
import sys

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    start_nodes,
    connect_nodes_bi,
    assert_equal,
    assert_raises_message,
    JSONRPCException,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _sha256_natural(pubkey_hex: str, sig_hex: str) -> str:
    """SHA256(pubkey||sig) — natural byte order (wire order in scriptPubKey)."""
    return hashlib.sha256(
        bytes.fromhex(pubkey_hex) + bytes.fromhex(sig_hex)
    ).digest().hex()


def _sha256_reversed(pubkey_hex: str, sig_hex: str) -> str:
    """SHA256(pubkey||sig) byte-reversed — matches uint256::GetHex()."""
    raw = hashlib.sha256(
        bytes.fromhex(pubkey_hex) + bytes.fromhex(sig_hex)
    ).digest()
    return raw[::-1].hex()


def _expected_script(algo: str, pubkey_hex: str, sig_hex: str) -> str:
    tag_map = {
        "falcon512": "464c4331", "flc1": "464c4331",
        "dilithium2": "44494c32", "dil2": "44494c32",
        "raccoong44": "52434734", "rcg4": "52434734",
    }
    tag = tag_map[algo.lower()]
    sha = _sha256_natural(pubkey_hex, sig_hex)
    return "6a24" + tag + sha


# ---------------------------------------------------------------------------
# Test class
# ---------------------------------------------------------------------------

class PQCSendRPCTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1

    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir)
        self.is_network_split = False

    # ------------------------------------------------------------------ #
    # Helpers                                                              #
    # ------------------------------------------------------------------ #

    def _check_pqc_available(self):
        """Return False (and print SKIP) if PQC RPCs are not compiled in."""
        try:
            self.nodes[0].generatepqccommitment("falcon512", "aa55", "bb66")
            return True
        except JSONRPCException as exc:
            if "Method not found" in str(exc) or "method not found" in str(exc):
                print("SKIP: PQC RPCs not available (node not built with ENABLE_LIBOQS)")
                return False
            raise

    def _fund_node(self):
        """Mine 101 blocks so the coinbase output is spendable."""
        self.nodes[0].generate(101)

    # ------------------------------------------------------------------ #
    # generatepqccommitment                                                #
    # ------------------------------------------------------------------ #

    def _test_generate_commitment(self):
        node = self.nodes[0]

        # --- known-vector: falcon512 ---
        result = node.generatepqccommitment("falcon512", "aa55", "bb66")
        assert "algorithm" in result, "missing 'algorithm' key"
        assert "commitment" in result, "missing 'commitment' key"
        assert "scriptPubKey" in result, "missing 'scriptPubKey' key"

        assert_equal(result["commitment"], _sha256_reversed("aa55", "bb66"))
        assert_equal(result["scriptPubKey"], _expected_script("falcon512", "aa55", "bb66"))
        assert "FLC1" in result["algorithm"].upper()

        # --- alias flc1 produces the same output ---
        result_alias = node.generatepqccommitment("flc1", "aa55", "bb66")
        assert_equal(result["commitment"],   result_alias["commitment"])
        assert_equal(result["scriptPubKey"], result_alias["scriptPubKey"])

        # --- dilithium2 tag ---
        result_dil = node.generatepqccommitment("dilithium2", "aa55", "bb66")
        assert result_dil["scriptPubKey"].startswith("6a2444494c32"), \
            "dilithium2 script must start with OP_RETURN PUSH36 DIL2"
        assert_equal(result_dil["commitment"], _sha256_reversed("aa55", "bb66"))

        # --- dil2 alias ---
        result_dil2 = node.generatepqccommitment("dil2", "aabb", "ccdd")
        result_dil_full = node.generatepqccommitment("dilithium2", "aabb", "ccdd")
        assert_equal(result_dil2["commitment"],   result_dil_full["commitment"])
        assert_equal(result_dil2["scriptPubKey"], result_dil_full["scriptPubKey"])

        # --- script is always 38 bytes (76 hex chars) ---
        assert_equal(len(result["scriptPubKey"]), 76)

        # --- commitment is always 32 bytes (64 hex chars) ---
        assert_equal(len(result["commitment"]), 64)

        # --- different inputs produce different commitment ---
        result2 = node.generatepqccommitment("falcon512", "aabb", "ccdd")
        assert result["commitment"] != result2["commitment"], \
            "different inputs must give different commitments"

        # --- error: unknown algorithm ---
        try:
            node.generatepqccommitment("unknown_algo", "aa55", "bb66")
            raise AssertionError("expected RPC error for unknown algorithm")
        except JSONRPCException as exc:
            assert "Unknown PQC algorithm" in str(exc) or "unknown" in str(exc).lower(), \
                f"unexpected error message: {exc}"

        # --- error: non-hex pubkey ---
        try:
            node.generatepqccommitment("falcon512", "zzzz", "bb66")
            raise AssertionError("expected RPC error for non-hex pubkey")
        except JSONRPCException as exc:
            assert "hex" in str(exc).lower(), f"unexpected error: {exc}"

        # --- error: non-hex signature ---
        try:
            node.generatepqccommitment("falcon512", "aa55", "zzzz")
            raise AssertionError("expected RPC error for non-hex signature")
        except JSONRPCException as exc:
            assert "hex" in str(exc).lower(), f"unexpected error: {exc}"

        print("  generatepqccommitment: PASS")

    # ------------------------------------------------------------------ #
    # sendpqccommitment                                                    #
    # ------------------------------------------------------------------ #

    def _test_send_commitment(self):
        node = self.nodes[0]
        addr = node.getnewaddress()

        # --- valid send ---
        txc_txid = node.sendpqccommitment(addr, 4.0, "falcon512", "aa55", "bb66")
        assert isinstance(txc_txid, str), "txid must be a string"
        assert_equal(len(txc_txid), 64)
        assert all(c in "0123456789abcdef" for c in txc_txid), \
            "txid must be lowercase hex"

        # TX_C is in the mempool
        mempool = node.getrawmempool()
        assert txc_txid in mempool, "TX_C must be in the mempool after broadcast"

        # TX_C must contain an OP_RETURN output with the PQC commitment script
        raw = node.getrawtransaction(txc_txid, 1)
        expected_spk = _expected_script("falcon512", "aa55", "bb66")
        spk_values = [vout["scriptPubKey"]["hex"] for vout in raw["vout"]]
        assert expected_spk in spk_values, \
            f"TX_C vout must include PQC OP_RETURN script {expected_spk}; got {spk_values}"

        # --- error: invalid address ---
        try:
            node.sendpqccommitment("notanaddress", 4.0, "falcon512", "aa55", "bb66")
            raise AssertionError("expected RPC error for invalid address")
        except JSONRPCException as exc:
            assert "address" in str(exc).lower() or "invalid" in str(exc).lower(), \
                f"unexpected error: {exc}"

        # --- error: unknown algorithm ---
        try:
            node.sendpqccommitment(addr, 4.0, "badalgo", "aa55", "bb66")
            raise AssertionError("expected RPC error for bad algorithm")
        except JSONRPCException as exc:
            assert "algorithm" in str(exc).lower() or "unknown" in str(exc).lower(), \
                f"unexpected error: {exc}"

        # --- error: non-hex pubkey ---
        try:
            node.sendpqccommitment(addr, 4.0, "falcon512", "zzzz", "bb66")
            raise AssertionError("expected RPC error for non-hex pubkey")
        except JSONRPCException as exc:
            assert "hex" in str(exc).lower(), f"unexpected error: {exc}"

        # Mine TX_C so the carrier outputs are confirmed for the reveal test
        node.generate(1)
        txc_info = node.getrawtransaction(txc_txid, 1)
        assert txc_info.get("confirmations", 0) >= 1, "TX_C should be confirmed"

        print("  sendpqccommitment: PASS")
        return txc_txid

    # ------------------------------------------------------------------ #
    # sendpqcreveal                                                         #
    # ------------------------------------------------------------------ #

    def _test_send_reveal(self, txc_txid: str):
        node = self.nodes[0]

        # --- valid reveal ---
        txr_txid = node.sendpqcreveal(txc_txid, "falcon512", "aa55", "bb66")
        assert isinstance(txr_txid, str), "txid must be a string"
        assert_equal(len(txr_txid), 64)
        assert all(c in "0123456789abcdef" for c in txr_txid), \
            "txid must be lowercase hex"

        # TX_R is different from TX_C
        assert txr_txid != txc_txid, "TX_R txid must differ from TX_C txid"

        # TX_R is in the mempool
        mempool = node.getrawmempool()
        assert txr_txid in mempool, "TX_R must be in the mempool after broadcast"

        # TX_R must spend the carrier output(s) from TX_C
        raw_r = node.getrawtransaction(txr_txid, 1)
        spent_txids = [vin["txid"] for vin in raw_r["vin"]]
        assert txc_txid in spent_txids, \
            "TX_R must spend outputs from TX_C"

        # Mine TX_R and verify confirmation
        node.generate(1)
        txr_info = node.getrawtransaction(txr_txid, 1)
        assert txr_info.get("confirmations", 0) >= 1, \
            "TX_R should be confirmed after mining"

        # --- error: TX_C txid not in wallet ---
        fake_txid = "00" * 32
        try:
            node.sendpqcreveal(fake_txid, "falcon512", "aa55", "bb66")
            raise AssertionError("expected RPC error for unknown TX_C txid")
        except JSONRPCException as exc:
            assert "not found" in str(exc).lower() or "wallet" in str(exc).lower(), \
                f"unexpected error: {exc}"

        # --- error: wrong algorithm for existing TX_C ---
        try:
            node.sendpqcreveal(txc_txid, "badalgox", "aa55", "bb66")
            raise AssertionError("expected RPC error for bad algorithm")
        except JSONRPCException as exc:
            assert "algorithm" in str(exc).lower() or "unknown" in str(exc).lower(), \
                f"unexpected error: {exc}"

        # --- error: non-hex pubkey ---
        try:
            node.sendpqcreveal(txc_txid, "falcon512", "zzzz", "bb66")
            raise AssertionError("expected RPC error for non-hex pubkey")
        except JSONRPCException as exc:
            assert "hex" in str(exc).lower(), f"unexpected error: {exc}"

        print("  sendpqcreveal: PASS")

    # ------------------------------------------------------------------ #
    # Full flow: dilithium2 alias                                          #
    # ------------------------------------------------------------------ #

    def _test_dilithium2_alias_flow(self):
        """Quick smoke-test using dil2 alias."""
        node = self.nodes[0]
        addr = node.getnewaddress()

        txc_txid = node.sendpqccommitment(addr, 2.0, "dil2", "aabb", "ccdd")
        assert_equal(len(txc_txid), 64)

        # Verify OP_RETURN script uses DIL2 tag
        raw = node.getrawtransaction(txc_txid, 1)
        spk_values = [vout["scriptPubKey"]["hex"] for vout in raw["vout"]]
        expected_spk = _expected_script("dilithium2", "aabb", "ccdd")
        assert expected_spk in spk_values, \
            f"TX_C must contain DIL2 commitment; got {spk_values}"

        node.generate(1)

        txr_txid = node.sendpqcreveal(txc_txid, "dil2", "aabb", "ccdd")
        assert_equal(len(txr_txid), 64)
        node.generate(1)

        txr_info = node.getrawtransaction(txr_txid, 1)
        assert txr_info.get("confirmations", 0) >= 1

        print("  dilithium2 (dil2) alias flow: PASS")

    # ------------------------------------------------------------------ #
    # Main entry point                                                     #
    # ------------------------------------------------------------------ #

    def run_test(self):
        if not self._check_pqc_available():
            return

        print("Funding node (mining 101 blocks)...")
        self._fund_node()

        print("Testing generatepqccommitment...")
        self._test_generate_commitment()

        print("Testing sendpqccommitment...")
        txc_txid = self._test_send_commitment()

        print("Testing sendpqcreveal...")
        self._test_send_reveal(txc_txid)

        print("Testing dilithium2 alias flow...")
        self._test_dilithium2_alias_flow()

        print("All PQC send RPC tests PASSED")


if __name__ == "__main__":
    PQCSendRPCTest().main()
