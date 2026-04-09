// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config/bitcoin-config.h"

#if ENABLE_LIBOQS

#include "pqc/pqc_commitment.h"

#include "crypto/sha256.h"
#include "support/experimental.h"
#include "test/test_bitcoin.h"
#include "utilstrencodings.h"

#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <string>

EXPERIMENTAL_FEATURE

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

#ifdef ENABLE_LIBOQS_RACCOON
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
#endif

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
#ifdef ENABLE_LIBOQS_RACCOON
    BOOST_CHECK(ParsePQCCommitmentType("raccoong44", type));
    BOOST_CHECK(type == PQCCommitmentType::RACCOONG44);
    BOOST_CHECK(ParsePQCCommitmentType("RCG4", type));
    BOOST_CHECK(type == PQCCommitmentType::RACCOONG44);
#endif
    BOOST_CHECK(!ParsePQCCommitmentType("unknown", type));
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

// --- Confirmed mainnet TX_R carrier decode and validate tests ---
// These tests use actual on-chain scriptSig data from confirmed carrier TX_R
// transactions to verify the full decode → extract pk/sig → validate commitment flow.

static void LogCarrierDecode(const std::string& txid_short,
                             PQCCommitmentType type,
                             const PQCCarrierHeader& hdr,
                             uint16_t pk_len,
                             uint16_t sig_len,
                             const std::string& pk_prefix,
                             const std::string& sig_prefix,
                             const std::string& commitment_hex,
                             const std::string& expected_commit_hex)
{
    BOOST_TEST_MESSAGE("  [DECODE] TX_R " << txid_short);
    BOOST_TEST_MESSAGE("    Algorithm: " << PQCCommitmentTypeToString(type));
    BOOST_TEST_MESSAGE("    HDR8: version=" << (int)hdr.version
        << " part=" << (int)hdr.part_index << "/" << (int)hdr.part_total
        << " pk_len=" << hdr.pk_len << " full_len=" << hdr.full_len);
    BOOST_TEST_MESSAGE("    PK extracted: " << pk_len << " bytes (prefix=" << pk_prefix << ")");
    BOOST_TEST_MESSAGE("    Sig extracted: " << sig_len << " bytes (prefix=" << sig_prefix << ")");
    BOOST_TEST_MESSAGE("    Commitment:   " << commitment_hex);
    BOOST_TEST_MESSAGE("    TX_C commit:  " << expected_commit_hex);
    bool match = (commitment_hex == expected_commit_hex);
    BOOST_TEST_MESSAGE("    VALIDATION:   " << (match ? "PASS" : "FAIL"));
}

static void DecodeAndValidateCarrierTxR(
    const std::string& txid,
    const std::string& scriptsig_hex,
    const std::string& expected_commit_hex,
    const std::string& expected_algo,
    uint16_t expected_pk_len,
    uint16_t expected_sig_len,
    const std::string& expected_pk_prefix,
    const std::string& expected_sig_prefix)
{
    // Parse the raw scriptSig bytes into a CScript
    std::vector<unsigned char> raw = ParseHex(scriptsig_hex);
    CScript scriptSig(raw.begin(), raw.end());

    // Step 1: Detect carrier tag
    PQCCommitmentType detected_type;
    BOOST_CHECK_MESSAGE(PQCDetectCarrierScriptSig(scriptSig, detected_type),
        "Failed to detect carrier tag in TX_R " << txid.substr(0, 16));

    // Step 2: Parse carrier header + payload
    PQCCarrierHeader hdr;
    std::vector<unsigned char> payload;
    BOOST_CHECK_MESSAGE(PQCParseCarrierPartScriptSig(scriptSig, detected_type, hdr, payload),
        "Failed to parse carrier scriptSig for TX_R " << txid.substr(0, 16));

    // Step 3: Extract pubkey and signature from payload
    BOOST_CHECK(hdr.pk_len <= payload.size());
    std::vector<unsigned char> pubkey(payload.begin(), payload.begin() + hdr.pk_len);
    std::vector<unsigned char> sig(payload.begin() + hdr.pk_len,
                                    payload.begin() + std::min((size_t)hdr.full_len, payload.size()));

    // Step 4: Compute SHA256(pk || sig)
    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pubkey, sig, commitment));
    std::string commitment_hex = commitment.GetHex();

    // Step 5: Log decoded details
    std::string pk_prefix_hex = HexStr(pubkey.begin(), pubkey.begin() + std::min((size_t)16, pubkey.size()));
    std::string sig_prefix_hex = HexStr(sig.begin(), sig.begin() + std::min((size_t)16, sig.size()));
    LogCarrierDecode(txid.substr(0, 16), detected_type, hdr,
                     (uint16_t)pubkey.size(), (uint16_t)sig.size(),
                     pk_prefix_hex, sig_prefix_hex,
                     commitment_hex, expected_commit_hex);

    // Step 6: Validate all decoded fields
    BOOST_CHECK_EQUAL(std::string(PQCCommitmentTypeToString(detected_type)).find(expected_algo) != std::string::npos, true);
    BOOST_CHECK_EQUAL((uint16_t)pubkey.size(), expected_pk_len);
    BOOST_CHECK_EQUAL((uint16_t)sig.size(), expected_sig_len);
    BOOST_CHECK_EQUAL(pk_prefix_hex, expected_pk_prefix);
    BOOST_CHECK_EQUAL(sig_prefix_hex, expected_sig_prefix);
    BOOST_CHECK_EQUAL(hdr.version, 1);
    BOOST_CHECK_EQUAL(hdr.part_total, 1);
    BOOST_CHECK_EQUAL(hdr.part_index, 0);

    // KEY VALIDATION: SHA256(pk || sig) must match TX_C OP_RETURN commitment
    BOOST_CHECK_EQUAL(commitment_hex, expected_commit_hex);

    // Step 7: Also validate via PQCValidateCommitmentFromCarrier using a simulated TX
    // Build a TX_C OP_RETURN commitment script for the expected commitment
    std::vector<unsigned char> commit_bytes = ParseHex(expected_commit_hex);
    uint256 expected_commitment(commit_bytes);

    CScript commitScript;
    BOOST_CHECK(PQCBuildCommitmentScript(detected_type, expected_commitment, commitScript));

    // Build a simulated TX_R with this carrier scriptSig and the commitment output
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = scriptSig;
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160
        << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = commitScript;
    const CTransaction tx(mtx);

    PQCCommitmentType carrier_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len_out = 0;
    uint16_t sig_len_out = 0;
    BOOST_CHECK_MESSAGE(PQCValidateCommitmentFromCarrier(tx, expected_commitment,
        carrier_type, carrier_input, pk_len_out, sig_len_out),
        "PQCValidateCommitmentFromCarrier failed for TX_R " << txid.substr(0, 16));
    BOOST_CHECK_EQUAL(pk_len_out, expected_pk_len);
    BOOST_CHECK_EQUAL(sig_len_out, expected_sig_len);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_tx_r_6551ccce)
{
    // TX_R: 6551ccceeceb32b77b5930efeaeedea3d6d7a05040f7a84ce2c652ed9fe4c51c (height 6156389)
    // TX_C: f2497ee767d25b51352ef6ba1e90f8d7bef16549fb4f8b0c8aa58384bb1c3396
    // Falcon-512, pk=897, sig=652
    DecodeAndValidateCarrierTxR(
        "6551ccceeceb32b77b5930efeaeedea3d6d7a05040f7a84ce2c652ed9fe4c51c",
        "08464c433146554c4c08010001000381060d4d080209b26e7841a14148109e36c32561227cfae293328e324be594f32fe0c0346d41c9386ebd025c5480311ff98c9804c18c5945be552976cd90e1ac9d46177a44745e8005aa9387047c6d0a073a4ad310045762b52a014f66389170a056763402b63d52cc8f5acd8080cbbfb2694006646192b6094361a8ddcc76329e407b8a850d095388a2c9d8af39cac23f6df0585276024f5ff803e165f0eee9f030681d7a01634d99e4be27b69ef90ac83e61b503143d66becbf54f7d737659e8f46e7b91f419b9400d7fb3b4c904be5e5005c84a26c34f321b95a59358a22134a1f1994581099a1553c2ca964a3fc0c39a564af5301e70c5a721f9851282621bd163b76894c9fdd95aaedd7fa77a4f449e71d05226239c147c80672e4b757dd62ef16ca50f0936319712e05100727a5709879e4f86cd160ac9e0205abb998f07942e063cf13ea1cf981cc9b142aac5a8e08ff5de213737621bf2b76a58652547104c2856986473e18b9bfc11d5dd89dc4be9bcae61dab120f0b98a9a196185d940b9d49bf885bfae4d5ca9a14f223aece2544fe7d897b92359364c0c4c8d76647adee77b847282372560494449f9271d9c1a31ea2bd7eaccb4d5be57ac8e34a5dcc1e8fb996fabd5b2f3ee91473fc6ffb65fc6270bc20906b8198974e446931fe05c2fb832643f0e406d4453a25ed2f7326a054169907c5429f54350195c425935d795277a462a152a730700794d0802cee083e2ab9935d63a062d98112ec6cc0656416910e9e91156d29a02c8d85774879c94fc169001811310c6b026546be7953cff5a35a031b71d06394956d20cd4f34ace607473704b9611cd923c8e8216bf46d130000f3271e88d605aa5c8f46da153f5d900235c270005d6eaed05c821a439f2cb62ef583c018204c8ad7a685bb19454f04e3296a5828cf06204ef7b230f2f8bbe60a50e4cb944b0a046c165264df9cc9208a45bbb4ef869faac5fae86454a9709a58d42e9a34f255839dd75d455171628b1a2b41fc7042a360cd01288684e0d25249644f86efc131b2e85fc3d313be3129ab11ab03a7a269dae6e19cc47c588740cb662563e466daabc1204d3e6983064e729b98fc40a90bf352b15206ce023e5f327cf422d85266ee0f9bac4eb68dc165b99ce87b9fe06ef84764b77bf50f232e9e8808fc39d6fa1eb128d626e6efd3575b8965fa868c2248527cc712aaabb4b1887b4860983a9ab656e04a2ba6820749168f5db04c1a36ce5a1b7bb4ee6a0a9b39aa617393913b5adffe778500ac0e252705a81b0d293681395e0556f5599437d2c947197e2b26775791d91c4d8a5402353161ec6ea522bf00427f284cc99326f524036505f3d0e6f1635ab2592014ba468667068613c51983d528f00c710d95e68ab5b218dba65a27ff434ad4f4559408ad93699223558bbfa8c69e3f7b9f240905e6af65ffe5a3ccf3626e30c49a9546c4bd04dfd01ed763cd6c374c545d1feda5283b72cf28da866111f9c3be5147557e5c352bfc275851b2acf2d9a43475b3516c5dd46c7e4a951f253ff8c6a448ab5b2ac68738d4d7d4a8c6a20f8cdcee6d91cde70eeb2882caba66be54d7eef9b3fc33d9b531bb7b39fcf04897bae7363bb54578301357c74f484671366d513c5f0a41eec42e2a72311a9a9e54ad846316c31533eb2115ee8472c5ea8bc264f3cc9480916aba8a3b20b7cad0d573f6f9f46d7148d59643af6acd958f124a5769a117d8f80cfe8d2cc308aa6e69b5f9dc163521dd39994ab13477f90e06c77cdab1132a8be067c83e1b2aafeeda668529649e942377bd83645d34254eeb33491a254d41f6cdba102af5cbdc15beb2f9fb9aa28b98396df904f4e87dcd995ce6565bc679ef3b324cb38e65aa96549b0b9e28c81ffe20b3f256618980dc3396e91a9f8ba5eea9d0a43fcf9f72cd2313a483cdbe9562f739933b8e863ce8a4aca5b700fae12523e1b188a02addfd176d2ecc669b6051101e47a641f5a5581b6bfbd26c478a03bdf07db117d854754db2364e33034fea631f1f770af0bb687bab8443fbbd24913430c0bd823aa967d9ef0cf6edc8a257be0d465c7db74b3235f8cba3864aa7136e34d297f723abcf11a46a5388f9d49da6597bd2c022f6f5ff42e4dd3a086c0176cebcd1bdaff7dd956de91ba22ed3cf9dbed315e755492f5f184358df169e006757575757551",
        "492c7dcba00e3e48cb22ce55ee4954312017ace60f2a40b523db9c17bfef85ef",
        "FALCON", 897, 652,
        "09b26e7841a14148109e36c32561227c",
        "3913b5adffe778500ac0e252705a81b0");
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_tx_r_238fb58b)
{
    // TX_R: 238fb58b352a7a2bbcd7eac7565f894ebeba020410b74cc76277f9336041b477 (height 6156425)
    // TX_C: 7e92f99648825613ea2f9d25ea1c41ab1103a31967637e46676c2be230ec1fd6
    // Falcon-512, pk=897, sig=660
    DecodeAndValidateCarrierTxR(
        "238fb58b352a7a2bbcd7eac7565f894ebeba020410b74cc76277f9336041b477",
        "08464c433146554c4c0801000100038106154d08020952300ef52dc8d199b86c62fd98f199244a92b926954aa0c513e309e85c1021e40adf7f14d2c7d4606a3b3a31cb90de9e036068b3a98cd1360f1226f101e04e933180069365aca0f82611a860105ab64c90f1c44323fc1d19840da177748994ccabd77b16d2e3fa24909bdc4d8a200dc3621d1fd657d16e17726c9900a026a46c8094c8a8fe8531db3b95566a84d561a3bc4ec1b5a0839886c9dc225426fb66459e023840c19452999e62ad98ed9beba08598585562947381d9f376ec7f55e54ba3e84c3c83d54a5aa2d187bece471087014e63680b381590c09e086c0b7a1a1b5e70a8aa044bf3062214ab67127aaaf8fd969a0404a42557a0d1ade4b09eaf977561e4b8961c1222d21914e62058cfcadab87a6f7351ef7b3c79a1339fce3c553e6f45454d34026d8747160f97a6b2f1847008030bd9326591445e502d6601c5c0edbfc68e4547cd34a41007b89d192aa1adc75a5b6cb872e6bac085a517232d9f9b40deac1cc15699bb8277bc78ad8715caf40216b345cedb820e851d881aa33a2e6e5538ba689c4dc894e4e514e4702892715a622442362d84210a883ea63628e326555568fb86cbca690f0d555719d5d9b8615a712991e4436c9cf8fd197d7015ce2a97c7551a596f3ad00808a47de359dd69eb76d499b7c68c1e3f0e0af1e5c01480826b32b811e06dfa7d68dd818c99f98c09f8a8157d05d63b5b15b7bf62238bd1dbd91a4d0802298a88546422125826331dd86a2e3a9855efd97f74ee3905cf58767ef9b485b367a766da5de32ed69e2fdd9196d496fa9ab5cc55baad6c76660c0be89440794a16a5aeceeab9deb4d00c47dd87ed0b124baa3a03f0de24cf497cb105d4a6d1a394b9905dd172d6ddbb91a4b6ca501c531e07e873613cbdaab0b64d55640b5d6e12eca5001dec6e60392ece9a2988993be2f40441a839764d8775ec1a3088db15b5808e47cb5987431541f477c54b4f7efc3577db3308953c10676fc73f4657d592df39264d9901c619815e650d0b53e3382c01eb7809c84c7342d900e3e3a43dbd7087370dbd7b4827a50c13ee44401bc3e46dfcab064e56fe873a555c29778243c46092d1f68d59960ad9dbeaa8c0895ee6f106ccd8eb03389fe43320a8bb1217e8582484a120a43a3244aa2efc6c79aeaf6f6892a11ad99c92a1a21d95d905a1322843c2339cf389581f411445e6a15d94c5f45b8631bca75526907664f4a4450a20583622d45386e5dca6d2f6720a4a443fd618581b62223956a8334554881d551118e122a98d23946741c10a8217936bfb5b9b8e764c243b15887c28b85fde55a9944c51de20becff5e898f651306cbae14dc647d44fd382bfae0ccbd0f578d473173e7be074529a358b441db85d1e041dfde119799c9b3b0fd22537c6b0d3fc7ad88584fa4e786934ad9219eac26e9de2597329f8fd483ace5475345e220f372f5becbe39854d05023dd765c68f9cf7afae3358777fd28ddb5d94489138decbcdd83ba8a2315a20c6bfa6d426f07675a1332d0a0d357922fcc7f51c1e1248d829087b11c0554a95310a2a901347d8236b3e7757e541f0cd2689bfe3b0e788626838bdb41d1670922f670acb46f5b127c9f03a7573d55f2bf67ed16d448986dfca88702910487a7b9c6e975d3e7a1bdeb4dada5c94fb03b36f8cba242a0a06f120b8f1338b3f49027964b0b9e6c57a51f8a59274876a5f8c419964ab1bc375d5f0ff99052202e5bc9b530abcc13686ce062d0b5bad43c1c1a80e4f42064468d7f45897dd70acc49135f435efbff1c620e40a2478264fccf09747692a336684e9a7a984a38f46c2a4de499fc5c86190a794942f703f15e9edc2a8a451b042d1fc3f86e53f4cfbec2580e240d0ac23ccea1a759553707f7b6c2ca11d35c899ec94f1cbfe7325f1a4db560aaed863797c98938fbff76731d29929aba8bd09254efeb3ab4d3e2d64e5169e12e767d4a1689c5b689a56da6fd64fc99569d428b8bc1343a859de272b47254ab36966a209e94878aa62e326d1455764e0f9edfaeb7da56181b2af6c7d386ea6ced143efca999b42c2c096c99c51f47f16692356ab5222136ec990d7ebad58c6b135bc6c7a9354335d1ee77e8ebdd3bc47950609248d3566356a902d834ad56c8a04666ad25d3a3b84a182dd3e11a5525fc17f343004ddedb8e885ce36e49ea8b3eea6c0a006757575757551",
        "d87439a60ba163f5e27fd9e259557b7fca77a5196494be649637310cd67206e5",
        "FALCON", 897, 660,
        "0952300ef52dc8d199b86c62fd98f199",
        "3956a8334554881d551118e122a98d23");
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_tx_r_77fc29fd)
{
    // TX_R: 77fc29fd9e5124fa85bcc62bb9ab49b8b95b4f4544b521e7589ca980dd1725b4 (height 6156450)
    // TX_C: e7793c5a133ea80e8302f7df547b8f2bf095de346f4196fbb134c7269720b7d8
    // Falcon-512, pk=897, sig=656
    DecodeAndValidateCarrierTxR(
        "77fc29fd9e5124fa85bcc62bb9ab49b8b95b4f4544b521e7589ca980dd1725b4",
        "08464c433146554c4c0801000100038106114d0802096d02fd877d228e433eaf692c25388740e0e657c311864885a9794f1364fa80888f5d2a100648f25901d543724398632d2143698e4b27427f087a99405c6a7d35edc53afa29ff3bf4885740a0d531495ec04d5fa266ee57021a0179b7a03f6475e3996859e3cafac4f152e5b3ba35e75d2338e135cacc567c8c2855b3060f0bcef9d4ffdb2a9052ba5b3ec98aa8f2a5b9dc60df6bb9bd54bb0ac94ca80951665a34aa49bb23052e89136a7d89ca68573cd209b3f104a3a6e026129da0702916513536e3c58c3d425395dbc8a94d7a5288d9db69e1bca479da208278542b929d552fd56674095356ad3c2dcac329171c05e4e166e1036c6987185b59dc439cfd35e327b308aa19e73f9217942ee1021757baabde0903bbc59373525674d9e06da1b89a3abb1cdc332e2f36bf454e8fa955726654e790c97003812340b885e302d7511d11752258a291af0b1650a0b8ebe20ae0a3d74aa87704900f36629f57050adb5a426e3f9b043351c4e1ab27c97be95b0241a1493f428fec3f63f8aa910a1ac09435320a3e0fba20fe13f9bc8d5852dea0401213ecb8adc0e14ca1f24b446f15a1c803276c77791711e317cc3e459e05263ed05dd726ea15a51e88ab296da227f5bf37322b4dbc15ee758d1a7f70ee9ef9f7807fa54da75179074d01352aa289876d377ed0b174288b0574596146aca694226f0a200199a06db8c5bd82cf23f53245f4088e484d0802d9f65206a02a0d59d5f5ff172c10f271537d8d4b595c1e9750614965b6d44447d8f9890047e178198d254438194aee4c2ecae164f783b71c1a1118441a65158e96b1599acf18d67ba2a4a706b48e7e6bb052483ab525e5e0a7eb7e5069591514aaaa1e7d74e313296db832d2a993ad798a2f917bc6a659b83b411ae66331a481e4d89d5916244e97cf50733249a493e3c96e216c09b8da57641efa3287f7a3a18370e9a6e92cc4498cae91aa9dc89cdabad59fa8b1b69ec1d33cd1d5a44590967e446b20d414e59e4f58b2b5dd99e919d46cd862d8028f098eda2190b08d5f5a40e9536be9e986f5e91357d51644fc831a2629d33b55922581e1844f1561f357a2eba509c55b8fc57014d5c0ca15965f158e0a05150ae82e66c23bb22ca7745ebfc140c676abe1a4d6774c96a558f8a3ece2d5b34c4bab3f120c8419565b22e6bf5bd4fc68ea1ed49e647009b60ce3b006a042eccf5b7d55baeb43d07e0e603563b590bc3898c1462c42f36ec25623d242bab06187e9245d3f39a7c04c051fa2969b04ccfc0763bac0a9847929d9c9a21fd7637c4b10eff3f6431adb1582474ac9661f892e481136505a883b70f177dd9a2616a2cec77f3a167a5ffb5350a1f5dc41db63877442a1b37b9226e23818fec56e448bba58f68b336e6cb4954ce2c4b5f8996beecb2441e2c247dc93637a3a2c6e2f3b1dcf6bbd8f63df89b4a11ff801578c18e37dcca04d010263f3540cab5759203665ee455667d2553d468b310f8a8c223918f235603165de08875619bfee2783b3923f891d7a62de46ecd1331445391ca36eb976d9c84f012e469439b0dc320d92772351283dd83b2ab2c44f9d1efcaf6d66311342bce94b0cf1f9d365e4f21631a0a043309648fb0347816aa8697ebd70dd21981d0ea8f9282ce7690f5d1c9bda208699c56d9e264a370366776788677aafbf53ca0248ed19067f157b655e490e1550d8c5936b5e88c41bf5685712d9ced11243c8991028a8ff2ffbce551c24ade38fd71484e6152f212f8c5e273ffde79dbb4241a46d98ef05f1b56d4b9a794b5a2ead863b0cedcfe510cc3cba4e64f12ee18f4e66f0b562d73699f05dd220d6b939652e93d6f7dd1c3929fe44d0f4ddfc18aa77fe79b2ed184aa4e31eae1359a100196ca331cbda2659a80a090e7364d866f622d52798595128214cb21b4ccaa2362d725cf5b5c7ba078bccdcd065a11770d06d8cad230a35ab1d70d52a7d5fa36ec2373bc458c12c9dc7392f250747095f7b6d8cca32cd74517aa662ad2fa838f9ecd37bc4cd68245974962089f1551c87f795ab972150181400a9cebab9358ae25b3687da46a217d8d31fe993522c3e7615b0731dfedd8ac1a24924c773f8ae8ea588b5fdfd2432ce8aaa679577c93cd4a817892dcdea936aca91a561d3278776308a663bcf845af009e251c561b4e9445951d16e123806757575757551",
        "5a6bc01cfa36880a634bc370596c762ed1f36c9f10c4b0894fdbf1085d6edb1b",
        "FALCON", 897, 656,
        "096d02fd877d228e433eaf692c253887",
        "39a7c04c051fa2969b04ccfc0763bac0");
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_tx_r_c32635aa)
{
    // TX_R: c32635aafa32abf9c89b5e366d647231c143b3fb1b925e51b51a67b6133e7924 (height 6156750)
    // TX_C: cec2b43fb7694a2aed7aac55c42ad8bdf95a3899a1b32afb485366da96f2cd87
    // Falcon-512, pk=897, sig=657
    DecodeAndValidateCarrierTxR(
        "c32635aafa32abf9c89b5e366d647231c143b3fb1b925e51b51a67b6133e7924",
        "08464c433146554c4c0801000100038106124d0802091f7e11e2bbcb2329bd0c53e1544303c2d842d6c06fb45c3d510788f668418f6134805499701331b26bc280ccfaa5e06692a83557f191c0a788c558c2d89bf2ace0c2c22d1d64b03694b0d0a269092669eaf2845facddbe967095e478adad34ba0a0fad92d566fc4c126ef25d082a244e9e6ead05d56189182241511a63f1a7f590e32867e85d018e355a9c89936171b076c9530eaef4c43b6058803d9dd307a03bb244ab837000fbad5ad274394cdb7e185292dc8abf437d211587508ea73d4ad853099135888343e950e20729650bb164f24f767a24718593180d8334adc0120b7e65a310d3e09fa0c44b86c6331dbe41e1e5aa0143a576a8b955dc0f799f128193f59560bfba995a4d85de2967a9225f4829594adeed1113d2a7eaf90cf70ead66db8e8a9743d02ce7b91818a7d9ed050dcdb46af16f023fcb629d0c4822d78867519aa36239a48126f92432be0f807f496ea85c610045ec78c3cd4bb06fa54311b66cb03745b16589cfd67d6d60eb5e64fe9976d83851cade60b462b0252533b5e8c8b9920d8e725a424ae0d3f638316157f62ed215224e919bc1b23e210061cdee860a22ba98d4c4b99c75aa817d866430146660d38aa32382ebb0bc038b93900369892e731b05172a81dec55309bc520164c40cc2cfba946bbe53a44d89a389352ef8d098a3a7bc20e539a31b9bf9bb91d4920416c37c22f963bcd9087c6185c2b5c60dba4d0802fc51a5dfaf330a78402858a04492a5d244a4d18a31a5ab90d81a44a95926e65587a314243dc0b986d4b8695917e41d1aa2f55801c9d2849dc0ef60b65c46209a9b4bb3430c2582fe504869ec1d52d5d22e2308780d0ed48592ad240075000199850de9253bae875bbc0cf52878c2678274d245cc050eb3ea1cf01e4c662d229b17b682a6ba45d0998848c75a22bb7455d61970e1a6ab8e94f7a448e71a4814d00b51fef9f315a73e441ba7168a2443a002f9a19a402cc293d0c2ddb4953db618ee8970bcbd5742e346db88a26827981f3b97aa0c6459428c32bd093169a9325884c00abfaf0e6dae64716ce3fd874644692f24a21b043a24284a47074a9229276bfb1952da63862a2f785abd8a756ba7378cd110f3484d1fd18b007c144eb13149a3534614bff01d48882a90bfb2e2a19348d1bf181d4a4d0fbda7f6b0b11d423f18318379901c8985dedab55080291cc4f5b43b18c2bbcd9e00ff46f2326dee9550ac7e4d9a9edbbea4d6091a11066967026a081d77dd22343938a41d3ffc68a55583033a2be98c1174f2b26f907fe8979e9dda5bfc647f623f159ecf573bdd26619de9d5e5af4aa37f81e5116d989aef8c09f2f6bb2a4cb210b6d0d09c6a20d1cc35dabd4923e97036dcc6ccd1616c1dd78e38c4f765ba2508fd201d64e62e7a1345012686556bf8bc17c674dbbb9fd620df3cceffa467601e498587bb3151d462b06d216482514d02025aedea895e4f681f9449d6a1e651d67b357cd066980228ea6eb13212db5ed1853883372d3e76c79ee6a71a1a7ba5cea31c4eab40c0b6efb6a5592b2aaa97a054204b43c355a15b66def439cc30a4d573a95dcb9762478c5e7b707ca30a3e2e9e828cd3b5e7a106721595ff2541f777478f3beaa15cb0922a7bbab6b7d67ea4123ab52984da6fc8c8a61d0308acb15303ad49ae7ee33db78fefb422ed8c4e16c8f02b9a9da9833946ad9f7ad777afbdd14827b38665e7af269cf77704da4a7159755ba990605b9a23c7c6d7338863ec4471166215075091cdaff2f4c2e8e2b4d8131d3ce3274a055249e0e87db5d0f27bb14818c951c45b24443c73a04a4545578b9bb345d0d2dccb6cd8819e7fa6810a4bd75ac7431f1dc45cb7f00b2ab7f9eae1b706b659ad70392d66349a32fab83298589f1fc6cf7d0fcb4571bc9e0263d4bc67a358e63c6b513695066d9f69fc8a66b4d8681d1db4757d424f562257fcf498dcc269d2889c36f5b448aa71d89c9efd22a79b7aa585ada07119b43d7e38168a14be0d06bcb08c31013059a67ea3ecfc75165fdc87ad3a4320085b193c3d98ee553bcdf363d0480501f579b6ac411a187e2cbb69d70ab6dbe597edc7e7e82129849c4c2ba515ac4199ba76210810499ea3663666666b873105a8e8d7ddb5c600eb3184b509611262708e2c18ebf2bb48800d63cf6888c456e4f2e6c0a1eddb6c8006757575757551",
        "3a83c1c63136e11862220ce97e61f83e7deea2426a79767b70aeada9781058b3",
        "FALCON", 897, 657,
        "091f7e11e2bbcb2329bd0c53e1544303",
        "3938a41d3ffc68a55583033a2be98c11");
}

BOOST_AUTO_TEST_SUITE_END()

#endif // ENABLE_LIBOQS
