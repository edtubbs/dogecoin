// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pqc/pqc_commitment.h"

#include "crypto/sha256.h"
#include "test/test_bitcoin.h"
#include "utilstrencodings.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pqc_commitment_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(pqc_compute_commitment_matches_sha256_pubkey_sig)
{
    const std::vector<unsigned char> public_key = ParseHex("02112233445566");
    const std::vector<unsigned char> signature = ParseHex("3045022100aabbccddeeff");

    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(public_key, signature, commitment));

    unsigned char expected[CSHA256::OUTPUT_SIZE];
    CSHA256()
        .Write(public_key.data(), public_key.size())
        .Write(signature.data(), signature.size())
        .Finalize(expected);
    BOOST_CHECK(std::equal(expected, expected + CSHA256::OUTPUT_SIZE, commitment.begin()));
}

BOOST_AUTO_TEST_CASE(pqc_build_extract_falcon_commitment_roundtrip)
{
    const std::vector<unsigned char> bytes = ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    const uint256 commitment(bytes);

    CScript script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, script));
    BOOST_CHECK_EQUAL(HexStr(script.begin(), script.end()), "6a24464c433100112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");

    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    BOOST_CHECK(PQCExtractCommitment(script, parsed_type, parsed_commitment));
    BOOST_CHECK(parsed_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK(parsed_commitment == commitment);
}

BOOST_AUTO_TEST_CASE(pqc_build_extract_dilithium_commitment_roundtrip)
{
    const std::vector<unsigned char> bytes = ParseHex("ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100");
    const uint256 commitment(bytes);

    CScript script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::DILITHIUM2, commitment, script));
    BOOST_CHECK_EQUAL(HexStr(script.begin(), script.end()), "6a2444494c32ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100");

    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    BOOST_CHECK(PQCExtractCommitment(script, parsed_type, parsed_commitment));
    BOOST_CHECK(parsed_type == PQCCommitmentType::DILITHIUM2);
    BOOST_CHECK(parsed_commitment == commitment);
}

BOOST_AUTO_TEST_CASE(pqc_build_extract_raccoong_commitment_roundtrip)
{
    const std::vector<unsigned char> bytes = ParseHex("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
    const uint256 commitment(bytes);

    CScript script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::RACCOONG44, commitment, script));
    BOOST_CHECK_EQUAL(HexStr(script.begin(), script.end()), "6a24524347341234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");

    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    BOOST_CHECK(PQCExtractCommitment(script, parsed_type, parsed_commitment));
    BOOST_CHECK(parsed_type == PQCCommitmentType::RACCOONG44);
    BOOST_CHECK(parsed_commitment == commitment);
}

BOOST_AUTO_TEST_CASE(pqc_extract_rejects_noncanonical_script)
{
    CScript script = CScript() << OP_RETURN << std::vector<unsigned char>(36, 0x00);
    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    BOOST_CHECK(!PQCExtractCommitment(script, parsed_type, parsed_commitment));
}

BOOST_AUTO_TEST_CASE(pqc_extract_from_tx_roundtrip)
{
    const std::vector<unsigned char> bytes = ParseHex("11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff");
    const uint256 commitment(bytes);

    CScript script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, script));

    CMutableTransaction mtx;
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = script;
    const CTransaction tx(mtx);

    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    uint32_t output_index = 0;
    BOOST_CHECK(PQCExtractCommitmentFromTx(tx, parsed_type, parsed_commitment, output_index));
    BOOST_CHECK(parsed_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK(parsed_commitment == commitment);
    BOOST_CHECK_EQUAL(output_index, 1U);
    BOOST_CHECK_EQUAL(std::string(PQCCommitmentTypeToString(parsed_type)), "FALCON512/FLC1");
}

BOOST_AUTO_TEST_CASE(pqc_parse_commitment_type_aliases)
{
    PQCCommitmentType type;
    BOOST_CHECK(ParsePQCCommitmentType("falcon512", type));
    BOOST_CHECK(type == PQCCommitmentType::FALCON512);
    BOOST_CHECK(ParsePQCCommitmentType("FLC1", type));
    BOOST_CHECK(type == PQCCommitmentType::FALCON512);
    BOOST_CHECK(ParsePQCCommitmentType("dilithium2", type));
    BOOST_CHECK(type == PQCCommitmentType::DILITHIUM2);
    BOOST_CHECK(ParsePQCCommitmentType("dil2", type));
    BOOST_CHECK(type == PQCCommitmentType::DILITHIUM2);
    BOOST_CHECK(ParsePQCCommitmentType("raccoong44", type));
    BOOST_CHECK(type == PQCCommitmentType::RACCOONG44);
    BOOST_CHECK(ParsePQCCommitmentType("RCG4", type));
    BOOST_CHECK(type == PQCCommitmentType::RACCOONG44);
    BOOST_CHECK(!ParsePQCCommitmentType("unknown", type));
}

BOOST_AUTO_TEST_CASE(pqc_verify_commitment_from_witness_pair_success)
{
    const std::vector<unsigned char> public_key = ParseHex("02112233445566");
    const std::vector<unsigned char> signature = ParseHex("3045022100aabbccddeeff");
    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(public_key, signature, commitment));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptWitness.stack.push_back(ParseHex("aa"));
    mtx.vin[0].scriptWitness.stack.push_back(public_key);
    mtx.vin[0].scriptWitness.stack.push_back(signature);
    mtx.vin[0].scriptWitness.stack.push_back(ParseHex("bbcc"));
    const CTransaction tx(mtx);

    uint32_t input_index = 0;
    uint32_t pubkey_item_index = 0;
    uint32_t signature_item_index = 0;
    BOOST_CHECK(PQCVerifyCommitmentFromWitness(tx, commitment, input_index, pubkey_item_index, signature_item_index));
    BOOST_CHECK_EQUAL(input_index, 0U);
    BOOST_CHECK_EQUAL(pubkey_item_index, 1U);
    BOOST_CHECK_EQUAL(signature_item_index, 2U);
}

BOOST_AUTO_TEST_CASE(pqc_verify_commitment_from_witness_pair_failure)
{
    const std::vector<unsigned char> public_key = ParseHex("02112233445566");
    const std::vector<unsigned char> signature = ParseHex("3045022100aabbccddeeff");
    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(public_key, signature, commitment));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptWitness.stack.push_back(ParseHex("deadbeef"));
    mtx.vin[0].scriptWitness.stack.push_back(ParseHex("cafebabe"));
    const CTransaction tx(mtx);

    uint32_t input_index = 0;
    uint32_t pubkey_item_index = 0;
    uint32_t signature_item_index = 0;
    BOOST_CHECK(!PQCVerifyCommitmentFromWitness(tx, commitment, input_index, pubkey_item_index, signature_item_index));
}

// --- Carrier mode tests ---

BOOST_AUTO_TEST_CASE(pqc_carrier_redeemscript_is_6_bytes)
{
    CScript redeemScript;
    BOOST_CHECK(PQCBuildCarrierRedeemScript(redeemScript));
    // OP_DROP(5) + OP_TRUE = 6 bytes
    BOOST_CHECK_EQUAL(redeemScript.size(), 6U);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_scriptpubkey_is_p2sh)
{
    CScript scriptPubKey;
    BOOST_CHECK(PQCBuildCarrierScriptPubKey(scriptPubKey));
    // P2SH: OP_HASH160 <20-byte-hash> OP_EQUAL = 23 bytes
    BOOST_CHECK_EQUAL(scriptPubKey.size(), 23U);
    BOOST_CHECK(scriptPubKey[0] == OP_HASH160);
    BOOST_CHECK(scriptPubKey[22] == OP_EQUAL);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_parts_needed_calculation)
{
    // Each part carries 3 x 520 = 1560 bytes
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(0), 0);
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(1), 1);
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(1560), 1);
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(1561), 2);
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(3120), 2);
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(3121), 3);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_build_parse_falcon_roundtrip)
{
    // Simulate a Falcon-512 keypair: pk=897 bytes, sig=652 bytes, total=1549 => fits in 1 part
    std::vector<unsigned char> pubkey(897, 0x09);
    std::vector<unsigned char> sig(652, 0x39);
    // Set some distinguishing bytes
    pubkey[0] = 0x09; pubkey[1] = 0xb2;
    sig[0] = 0x39; sig[1] = 0x13;

    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(pubkey.size() + sig.size()), 1);

    CScript partScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::FALCON512, pubkey, sig, 0, partScriptSig));

    // Parse it back
    PQCCommitmentType parsed_type;
    PQCCarrierHeader hdr;
    std::vector<unsigned char> payload;
    BOOST_CHECK(PQCParseCarrierPartScriptSig(partScriptSig, parsed_type, hdr, payload));
    BOOST_CHECK(parsed_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK_EQUAL(hdr.version, 0x01);
    BOOST_CHECK_EQUAL(hdr.part_index, 0);
    BOOST_CHECK_EQUAL(hdr.part_total, 1);
    BOOST_CHECK_EQUAL(hdr.pk_len, 897);
    BOOST_CHECK_EQUAL(hdr.full_len, 1549);

    // Verify payload contains pubkey || sig
    BOOST_CHECK_EQUAL(payload.size(), 1549U);
    BOOST_CHECK(std::equal(pubkey.begin(), pubkey.end(), payload.begin()));
    BOOST_CHECK(std::equal(sig.begin(), sig.end(), payload.begin() + 897));
}

BOOST_AUTO_TEST_CASE(pqc_carrier_detect_tag)
{
    std::vector<unsigned char> pubkey(100, 0xAA);
    std::vector<unsigned char> sig(100, 0xBB);

    CScript partScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::DILITHIUM2, pubkey, sig, 0, partScriptSig));

    PQCCommitmentType detected;
    BOOST_CHECK(PQCDetectCarrierScriptSig(partScriptSig, detected));
    BOOST_CHECK(detected == PQCCommitmentType::DILITHIUM2);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_validate_commitment_from_tx)
{
    // Build a simulated TX_R with carrier scriptSig
    std::vector<unsigned char> pubkey(897, 0x09);
    std::vector<unsigned char> sig(652, 0x39);

    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pubkey, sig, commitment));

    CScript commitScript;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, commitScript));

    CScript carrierScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::FALCON512, pubkey, sig, 0, carrierScriptSig));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = carrierScriptSig;
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = commitScript;
    const CTransaction tx(mtx);

    // Validate commitment from carrier
    PQCCommitmentType carrier_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len = 0;
    uint16_t sig_len = 0;
    BOOST_CHECK(PQCValidateCommitmentFromCarrier(tx, commitment, carrier_type, carrier_input, pk_len, sig_len));
    BOOST_CHECK(carrier_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK_EQUAL(carrier_input, 0U);
    BOOST_CHECK_EQUAL(pk_len, 897);
    BOOST_CHECK_EQUAL(sig_len, 652);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_validate_commitment_failure_wrong_data)
{
    std::vector<unsigned char> pubkey(100, 0xAA);
    std::vector<unsigned char> sig(100, 0xBB);

    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pubkey, sig, commitment));

    // Build carrier with DIFFERENT data
    std::vector<unsigned char> wrong_pubkey(100, 0xCC);
    std::vector<unsigned char> wrong_sig(100, 0xDD);

    CScript carrierScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::FALCON512, wrong_pubkey, wrong_sig, 0, carrierScriptSig));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = carrierScriptSig;
    mtx.vout.resize(1);
    const CTransaction tx(mtx);

    PQCCommitmentType carrier_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len = 0;
    uint16_t sig_len = 0;
    BOOST_CHECK(!PQCValidateCommitmentFromCarrier(tx, commitment, carrier_type, carrier_input, pk_len, sig_len));
}

BOOST_AUTO_TEST_CASE(pqc_carrier_multipart_dilithium2)
{
    // Dilithium2: pk=1312, sig=2420, total=3732 bytes => needs 3 parts (3732/1560 = 2.39)
    std::vector<unsigned char> pubkey(1312, 0x44);
    std::vector<unsigned char> sig(2420, 0x55);

    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(pubkey.size() + sig.size()), 3);

    // Build all 3 parts
    CScript part0, part1, part2;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::DILITHIUM2, pubkey, sig, 0, part0));
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::DILITHIUM2, pubkey, sig, 1, part1));
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::DILITHIUM2, pubkey, sig, 2, part2));

    // Build a TX_R with 3 carrier inputs
    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pubkey, sig, commitment));

    CMutableTransaction mtx;
    mtx.vin.resize(3);
    mtx.vin[0].scriptSig = part0;
    mtx.vin[1].scriptSig = part1;
    mtx.vin[2].scriptSig = part2;
    mtx.vout.resize(1);
    const CTransaction tx(mtx);

    PQCCommitmentType carrier_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len = 0;
    uint16_t sig_len = 0;
    BOOST_CHECK(PQCValidateCommitmentFromCarrier(tx, commitment, carrier_type, carrier_input, pk_len, sig_len));
    BOOST_CHECK(carrier_type == PQCCommitmentType::DILITHIUM2);
    BOOST_CHECK_EQUAL(pk_len, 1312);
    BOOST_CHECK_EQUAL(sig_len, 2420);
}

BOOST_AUTO_TEST_SUITE_END()
