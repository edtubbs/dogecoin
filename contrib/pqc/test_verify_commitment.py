#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os
import subprocess
import tempfile
import unittest


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPT_PATH = os.path.join(REPO_ROOT, "contrib", "pqc", "verify_commitment.py")


class VerifyCommitmentTests(unittest.TestCase):
    def test_log_file_input_and_match(self) -> None:
        log_content = """date_utc: 2026-03-18T00:00:00Z
network: testnet
txid: 1234
commitment_type: FLC1
script_pub_key_hex: 6a24464c4331b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2
commitment_hex: b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2
pubkey_hex: aa55
signature_hex: bb66
"""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp:
            tmp.write(log_content)
            log_path = tmp.name

        try:
            proc = subprocess.run(
                ["python3", SCRIPT_PATH, "--log-file", log_path],
                check=False,
                capture_output=True,
                text=True,
            )
        finally:
            os.unlink(log_path)

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("match_commitment: true", proc.stdout)
        self.assertIn("match_script: true", proc.stdout)

    def test_mismatch_returns_nonzero(self) -> None:
        proc = subprocess.run(
            [
                "python3",
                SCRIPT_PATH,
                "--algo",
                "dilithium2",
                "--pubkey",
                "010203",
                "--signature",
                "a1a2a3",
                "--expected-commitment",
                "00",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("does not match expected commitment", proc.stderr)

    def test_raccoong44_log_file_input_and_match(self) -> None:
        log_content = """date_utc: 2026-04-08T00:00:00Z
network: mainnet
txid: 5678
commitment_type: RCG4
script_pub_key_hex: 6a245243473461cbd3c9530ff2e4f3941fb7a757f19ff55347cb0534537ebbf78996f509ea5c
commitment_hex: 61cbd3c9530ff2e4f3941fb7a757f19ff55347cb0534537ebbf78996f509ea5c
pubkey_hex: cc77
signature_hex: dd88
"""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp:
            tmp.write(log_content)
            log_path = tmp.name

        try:
            proc = subprocess.run(
                ["python3", SCRIPT_PATH, "--log-file", log_path],
                check=False,
                capture_output=True,
                text=True,
            )
        finally:
            os.unlink(log_path)

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("match_commitment: true", proc.stdout)
        self.assertIn("match_script: true", proc.stdout)


if __name__ == "__main__":
    unittest.main()
