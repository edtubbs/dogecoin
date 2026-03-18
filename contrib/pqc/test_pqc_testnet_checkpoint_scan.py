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
commitment_type: DIL2
pubkey_hex: aa
signature_hex: bb
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
        self.assertEqual(parsed["commitment_type"], "DIL2")

        tx = {
            "vout": [
                {
                    "scriptPubKey": {
                        "hex": "6a2444494c32f57f1487be9770eaf01853fbc3f7ad0181875d8548c197f45e5327488084d39c",
                        "addresses": ["nXBQ8M5xFf7f8sM2jQY5q2f2uvvE4nStQd"],
                    }
                }
            ]
        }
        self.assertTrue(
            MODULE.tx_contains_script(
                tx, "6a2444494c32f57f1487be9770eaf01853fbc3f7ad0181875d8548c197f45e5327488084d39c"
            )
        )
        self.assertTrue(MODULE.tx_contains_address(tx, "nXBQ8M5xFf7f8sM2jQY5q2f2uvvE4nStQd"))


if __name__ == "__main__":
    unittest.main()
