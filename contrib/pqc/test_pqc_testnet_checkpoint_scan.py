#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import importlib.util
import os
import tempfile
import unittest


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPT_PATH = os.path.join(REPO_ROOT, "qa", "rpc-tests", "pqc_testnet_checkpoint_scan.py")

SPEC = importlib.util.spec_from_file_location("pqc_testnet_checkpoint_scan", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class PqcTestnetCheckpointScanTests(unittest.TestCase):
    def test_compute_script_matches_known_vector(self) -> None:
        script_hex = MODULE.compute_pqc_script("falcon512", "aa55", "bb66")
        self.assertEqual(
            script_hex,
            "6a24464c4331b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2",
        )

    def test_parse_log_and_tx_helpers(self) -> None:
        log_content = """txid: 01
height: 5900001
commitment_type: FLC1
pubkey_hex: aa55
signature_hex: bb66
wallet_address: nXBQ8M5xFf7f8sM2jQY5q2f2uvvE4nStQd
"""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp:
            tmp.write(log_content)
            path = tmp.name
        try:
            parsed = MODULE.parse_log_file(path)
        finally:
            os.unlink(path)

        self.assertEqual(parsed["txid"], "01")
        self.assertEqual(parsed["height"], "5900001")
        self.assertEqual(parsed["commitment_type"], "FLC1")

        tx = {
            "vout": [
                {
                    "scriptPubKey": {
                        "hex": "6a24464c4331b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2",
                        "addresses": ["nXBQ8M5xFf7f8sM2jQY5q2f2uvvE4nStQd"],
                    }
                }
            ]
        }
        self.assertTrue(
            MODULE.tx_contains_script(
                tx, "6a24464c4331b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2"
            )
        )
        self.assertTrue(MODULE.tx_contains_address(tx, "nXBQ8M5xFf7f8sM2jQY5q2f2uvvE4nStQd"))

    def test_compute_commitment_and_write_log_file(self) -> None:
        commitment_hex = MODULE.compute_commitment_hex("aa55", "bb66")
        self.assertEqual(commitment_hex, "b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2")

        with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp:
            output_path = tmp.name
        try:
            MODULE.write_log_file(output_path, {"z": "last", "a": "first"})
            with open(output_path, "r", encoding="utf-8") as log_file:
                lines = log_file.read().splitlines()
        finally:
            os.unlink(output_path)
        self.assertEqual(lines, ["a: first", "z: last"])

    def test_pick_field_prefers_cli_value_then_log_values(self) -> None:
        parsed = {"txid": "from_log", "address": "from_log_address"}
        self.assertEqual(MODULE.pick_field("from_cli", parsed, "txid"), "from_cli")
        self.assertEqual(MODULE.pick_field(None, parsed, "txid"), "from_log")
        self.assertEqual(MODULE.pick_field(None, parsed, "wallet_address", "address"), "from_log_address")

    def test_default_output_log_path_uses_txid_filename(self) -> None:
        path = MODULE.default_output_log_path("abcd1234")
        self.assertTrue(path.endswith("core-e2e-validation-abcd1234.log"))


if __name__ == "__main__":
    unittest.main()
