#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Standalone unit tests for the generatepqccommitment RPC output contract.

These tests verify the commitment and scriptPubKey values that the
generatepqccommitment RPC produces, using pure Python — no running node
is required.  They document the exact byte-encoding contract:

  commitment  = SHA256(pubkey || signature) stored as uint256, returned by
                GetHex() which reverses the bytes (Bitcoin display convention).
  scriptPubKey = OP_RETURN (0x6a) PUSH36 (0x24) <4-byte tag> <32-byte SHA256
                 in natural / wire order>.

Algorithm tags
--------------
  falcon512  / flc1      -> b"FLC1"  (0x464c4331)
  dilithium2 / dil2      -> b"DIL2"  (0x44494c32)
  raccoong44 / rcg4      -> b"RCG4"  (0x52434734)
"""

import hashlib
import sys
import unittest


# Helpers mirroring the C++ implementation

_TAG_MAP = {
    "falcon512":  b"FLC1",
    "flc1":       b"FLC1",
    "dilithium2": b"DIL2",
    "dil2":       b"DIL2",
    "raccoong44": b"RCG4",
    "rcg4":       b"RCG4",
}


def _sha256(pubkey: bytes, signature: bytes) -> bytes:
    """Return SHA256(pubkey || signature) as 32 raw bytes (natural order)."""
    return hashlib.sha256(pubkey + signature).digest()


def expected_commitment_hex(pubkey_hex: str, sig_hex: str) -> str:
    """
    Return the 'commitment' field that generatepqccommitment RPC produces.

    The RPC calls uint256::GetHex() which reverses the raw SHA256 bytes
    (Bitcoin little-endian uint256 display convention).
    """
    raw = _sha256(bytes.fromhex(pubkey_hex), bytes.fromhex(sig_hex))
    return raw[::-1].hex()


def expected_script_hex(algo: str, pubkey_hex: str, sig_hex: str) -> str:
    """
    Return the 'scriptPubKey' field that generatepqccommitment RPC produces.

    Format: OP_RETURN(0x6a) PUSH36(0x24) TAG(4 bytes) SHA256_bytes(32 bytes,
    natural wire order — NOT reversed).
    """
    tag = _TAG_MAP[algo.lower()]
    raw = _sha256(bytes.fromhex(pubkey_hex), bytes.fromhex(sig_hex))
    script = bytes([0x6A, 0x24]) + tag + raw
    return script.hex()


# Test cases

class TestGeneratePQCCommitmentContract(unittest.TestCase):

    # Commitment hex (GetHex-reversed SHA256)

    def test_commitment_hex_falcon512_known_vector(self) -> None:
        """SHA256(aa55 || bb66) reversed = a2d956fb..."""
        result = expected_commitment_hex("aa55", "bb66")
        self.assertEqual(
            result,
            "a2d956fbdc65992378d63c67975c57dc1ed3f055fab65715c9db6660e11e81b8",
        )

    def test_commitment_hex_second_vector(self) -> None:
        """SHA256(aabb || ccdd) reversed."""
        result = expected_commitment_hex("aabb", "ccdd")
        self.assertEqual(
            result,
            "97d28856de6e5057b728f6b19ed1870c1794cd54fde7b63856d522c891d6708d",
        )

    def test_commitment_is_32_bytes_hex(self) -> None:
        result = expected_commitment_hex("deadbeef", "cafebabe")
        self.assertEqual(len(result), 64)

    def test_commitment_changes_with_different_pubkey(self) -> None:
        c1 = expected_commitment_hex("aa55", "bb66")
        c2 = expected_commitment_hex("aa56", "bb66")
        self.assertNotEqual(c1, c2)

    def test_commitment_changes_with_different_sig(self) -> None:
        c1 = expected_commitment_hex("aa55", "bb66")
        c2 = expected_commitment_hex("aa55", "bb67")
        self.assertNotEqual(c1, c2)

    def test_commitment_is_reversed_sha256(self) -> None:
        """GetHex() reverses raw SHA256 — verify both directions match."""
        pubkey = bytes.fromhex("aa55")
        sig = bytes.fromhex("bb66")
        raw_sha256 = hashlib.sha256(pubkey + sig).digest()
        rpc_commitment = expected_commitment_hex("aa55", "bb66")
        # Reversing back should give natural SHA256
        self.assertEqual(bytes.fromhex(rpc_commitment)[::-1].hex(), raw_sha256.hex())

    # scriptPubKey format

    def test_script_falcon512_known_vector(self) -> None:
        """OP_RETURN PUSH36 FLC1 <sha256 natural bytes>."""
        script = expected_script_hex("falcon512", "aa55", "bb66")
        self.assertEqual(
            script,
            "6a24464c4331b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2",
        )

    def test_script_dilithium2_known_vector(self) -> None:
        """OP_RETURN PUSH36 DIL2 <sha256 natural bytes>."""
        script = expected_script_hex("dilithium2", "aa55", "bb66")
        self.assertEqual(
            script,
            "6a2444494c32b8811ee16066dbc91557b6fa55f0d31edc575c97673cd678239965dcfb56d9a2",
        )

    def test_script_starts_with_op_return_push36(self) -> None:
        script = expected_script_hex("falcon512", "aa55", "bb66")
        self.assertTrue(script.startswith("6a24"))

    def test_script_falcon512_tag_position(self) -> None:
        """Bytes 2–5 (hex chars 4–11) must be 'FLC1' = 464c4331."""
        script = expected_script_hex("falcon512", "aa55", "bb66")
        self.assertEqual(script[4:12], "464c4331")

    def test_script_dilithium2_tag_position(self) -> None:
        """Bytes 2–5 must be 'DIL2' = 44494c32."""
        script = expected_script_hex("dilithium2", "aa55", "bb66")
        self.assertEqual(script[4:12], "44494c32")

    def test_script_raccoong44_tag_position(self) -> None:
        """Bytes 2–5 must be 'RCG4' = 52434734."""
        script = expected_script_hex("raccoong44", "aa55", "bb66")
        self.assertEqual(script[4:12], "52434734")

    def test_script_total_length(self) -> None:
        """Script is exactly 38 bytes = 76 hex chars (OP_RETURN+PUSH+4+32)."""
        script = expected_script_hex("falcon512", "deadbeef", "cafebabe")
        self.assertEqual(len(script), 76)

    def test_script_uses_natural_sha256_not_reversed(self) -> None:
        """scriptPubKey embeds raw SHA256 bytes, NOT the GetHex-reversed form."""
        pubkey_hex, sig_hex = "aa55", "bb66"
        raw_sha256 = hashlib.sha256(
            bytes.fromhex(pubkey_hex) + bytes.fromhex(sig_hex)
        ).digest()
        script = expected_script_hex("falcon512", pubkey_hex, sig_hex)
        # Last 64 hex chars of the script are the 32 SHA256 bytes in natural order
        embedded = script[-64:]
        self.assertEqual(embedded, raw_sha256.hex())
        # And it must differ from the reversed form
        self.assertNotEqual(embedded, raw_sha256[::-1].hex())

    # Algorithm alias handling (mirrors ParsePQCCommitmentType)

    def test_flc1_alias_matches_falcon512(self) -> None:
        self.assertEqual(
            expected_script_hex("flc1", "aa55", "bb66"),
            expected_script_hex("falcon512", "aa55", "bb66"),
        )

    def test_dil2_alias_matches_dilithium2(self) -> None:
        self.assertEqual(
            expected_script_hex("dil2", "aa55", "bb66"),
            expected_script_hex("dilithium2", "aa55", "bb66"),
        )

    def test_rcg4_alias_matches_raccoong44(self) -> None:
        self.assertEqual(
            expected_script_hex("rcg4", "aa55", "bb66"),
            expected_script_hex("raccoong44", "aa55", "bb66"),
        )

    # Cross-check against pqc_verify_commitment.py output

    def test_script_matches_pqc_verify_commitment_output(self) -> None:
        """
        pqc_verify_commitment.py computes script using natural SHA256 bytes.
        The 'commitment_hex' it prints is the NATURAL order (not reversed),
        while generatepqccommitment RPC 'commitment' field is REVERSED.
        Verify that both describe the same underlying 32-byte hash.
        """
        pubkey_hex, sig_hex = "aa55", "bb66"
        # 'commitment_hex' as printed by pqc_verify_commitment.py (natural order)
        natural_sha = hashlib.sha256(
            bytes.fromhex(pubkey_hex) + bytes.fromhex(sig_hex)
        ).digest().hex()
        # RPC commitment (reversed)
        rpc_commitment = expected_commitment_hex(pubkey_hex, sig_hex)
        # They must be byte-reverses of each other
        self.assertEqual(bytes.fromhex(natural_sha)[::-1].hex(), rpc_commitment)


if __name__ == "__main__":
    sys.argv = [sys.argv[0]]
    runner = unittest.TextTestRunner(stream=sys.stdout)
    result = runner.run(
        unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])
    )
    sys.exit(0 if result.wasSuccessful() else 1)
