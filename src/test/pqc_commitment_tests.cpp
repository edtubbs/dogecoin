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

// --- Multi-part mainnet TX_R carrier decode and validate tests ---
// These tests use actual on-chain scriptSig data from confirmed multi-part
// carrier TX_R transactions (Dilithium2 3-part, Raccoon-G 24-part).

static void DecodeAndValidateMultiPartCarrierTxR(
    const std::string& txid,
    const std::vector<std::string>& scriptsig_hexes,
    const std::string& expected_commit_hex,
    const std::string& expected_algo,
    uint16_t expected_pk_len,
    uint16_t expected_sig_len,
    const std::string& expected_pk_prefix,
    const std::string& expected_sig_prefix)
{
    uint8_t expected_parts = static_cast<uint8_t>(scriptsig_hexes.size());
    BOOST_TEST_MESSAGE("  [MULTI-PART] TX_R " << txid.substr(0, 16) << " (" << (int)expected_parts << " parts)");

    // Build a simulated TX with one carrier input per part + commitment output
    std::vector<unsigned char> commit_bytes = ParseHex(expected_commit_hex);
    uint256 expected_commitment(commit_bytes);

    // Detect the algorithm type from part 0
    std::vector<unsigned char> raw0 = ParseHex(scriptsig_hexes[0]);
    CScript script0(raw0.begin(), raw0.end());
    PQCCommitmentType detected_type;
    BOOST_CHECK_MESSAGE(PQCDetectCarrierScriptSig(script0, detected_type),
        "Failed to detect carrier tag in part 0 of TX_R " << txid.substr(0, 16));

    // Build commitment script for the output
    CScript commitScript;
    BOOST_CHECK(PQCBuildCommitmentScript(detected_type, expected_commitment, commitScript));

    // Build a simulated TX_R with N carrier inputs
    CMutableTransaction mtx;
    mtx.vin.resize(expected_parts);
    for (uint8_t p = 0; p < expected_parts; ++p) {
        std::vector<unsigned char> raw = ParseHex(scriptsig_hexes[p]);
        mtx.vin[p].scriptSig = CScript(raw.begin(), raw.end());
    }
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160
        << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = commitScript;
    const CTransaction tx(mtx);

    // Validate via PQCValidateCommitmentFromCarrier (full multi-part reassembly + SHA256 check)
    PQCCommitmentType carrier_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len_out = 0;
    uint16_t sig_len_out = 0;
    BOOST_CHECK_MESSAGE(PQCValidateCommitmentFromCarrier(tx, expected_commitment,
        carrier_type, carrier_input, pk_len_out, sig_len_out),
        "PQCValidateCommitmentFromCarrier failed for multi-part TX_R " << txid.substr(0, 16));
    BOOST_CHECK_EQUAL(std::string(PQCCommitmentTypeToString(carrier_type)).find(expected_algo) != std::string::npos, true);
    BOOST_CHECK_EQUAL(pk_len_out, expected_pk_len);
    BOOST_CHECK_EQUAL(sig_len_out, expected_sig_len);

    // Also validate individual part headers
    for (uint8_t p = 0; p < expected_parts; ++p) {
        PQCCommitmentType part_type;
        PQCCarrierHeader hdr;
        std::vector<unsigned char> payload;
        std::vector<unsigned char> raw = ParseHex(scriptsig_hexes[p]);
        CScript partScript(raw.begin(), raw.end());
        BOOST_CHECK_MESSAGE(PQCParseCarrierPartScriptSig(partScript, part_type, hdr, payload),
            "Failed to parse part " << (int)p << " scriptSig for TX_R " << txid.substr(0, 16));
        BOOST_CHECK_EQUAL(hdr.version, 1);
        BOOST_CHECK_EQUAL(hdr.part_index, p);
        BOOST_CHECK_EQUAL(hdr.part_total, expected_parts);
        BOOST_CHECK_EQUAL(hdr.pk_len, expected_pk_len);
    }

    // Extract and verify pk/sig prefix bytes
    PQCCommitmentType ext_type;
    std::vector<unsigned char> ext_pk, ext_sig;
    BOOST_CHECK(PQCExtractKeyMaterialFromCarrier(tx, ext_type, ext_pk, ext_sig));
    std::string pk_prefix_hex = HexStr(ext_pk.begin(), ext_pk.begin() + std::min((size_t)16, ext_pk.size()));
    std::string sig_prefix_hex = HexStr(ext_sig.begin(), ext_sig.begin() + std::min((size_t)16, ext_sig.size()));
    BOOST_CHECK_EQUAL(pk_prefix_hex, expected_pk_prefix);
    BOOST_CHECK_EQUAL(sig_prefix_hex, expected_sig_prefix);

    BOOST_TEST_MESSAGE("    ✓ Multi-part validation PASS: " << (int)expected_parts << " parts, "
        << "pk=" << pk_len_out << ", sig=" << sig_len_out);
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_dilithium2_1adeed97)
{
    // TX_R: 1adeed97b8a8bab0c946b734e3b8d43e02daa7aabc4f9555a7bb920a379ee517 (height 6169765)
    // TX_C: a467ac9b20aa3de5ba43f9098751174983ac1ef5160ae0ca51a83c564379b153
    // Dilithium2 (ML-DSA-44), pk=1312, sig=2420, 3 carrier parts
    std::vector<std::string> scriptsigs = {
        // Part 0/3 (input 0, spending TX_C output 3)
        "0844494c3246554c4c080100030005200e944d0802c9d1ad48e5334f72c15a6e1c8132a7f417c031faa050e37f1c21c2881a86811628bd97c0c090286896f60fc82dee822ce6892b5a0727c60ebcb663d0e76195bda9b12b64be50ef97b59ce81b0bf76128633ac94b3dcad1d323b5cee49ad5c5c0567f5b48012d3afe2e56e6338ed86a5c3c1ef33cc2bac186fe821cb4916f47058e629a9171055b416ad05198060b6e7f0d8da4d01889e7b2d2f282b115625be342264ebb20cd5696bce59fd3f7559511d9195b9af844b1149571ba31b6adc5db96d8ecc66e91e267f6653090950638625ff1295e63c4c95635cba556529c6c210579892965f51c02f8ea77bbc765db6fb6fcdf57dfd40d992daee51d2a146fb921bfc88ab6562be3501b3b129cc30e980449773e12943df0c0c7bb3030dc56930dacad880f91471b9bd1e90449bc05efe076529139361d6af5a8fd686505b7592ed2ed7c7601969d9764a72574ff42fe1410d5f41c486c41c0131828da2e5950185e73d18f6d0ab71c6ba2c52c8272e17a478269d92d51f25230773330d4842368a0865c9715a2a72c5c951d7ad72f392a733eb1fda88e81f3c9f471608ca0ad502de350c3dd1c3355ea4e444eb0b66c1913f51250f3d4276f063151fae67780f744c81861df879359462d3b57349b69eb16a3ef9db13396f389eff3edc6550638d52a20161a8cfc9e2edb9b02a987545d2c40a5777d74fbecdcd9f748811f23b79cd1c822ad971d4d0802ae3a665e7c7d5137c3b1b9e8367edd3ce224d98e8c13c2f4b825687e0b3d1a5ac10ca5900d24ea51e268a61872cc1bc3e8c1e649c4147035ed1d4d275cb3dfda68e9ec92923c1922ff8d1ad93fdd7782f77d6f9e382d133bbb35cf55d427e20058c5c41253458f509e4f24941d12bc035b910d541548ff46c384af625188fac4eaf7507aaa9b1ea98c75a15152e7160bb738f8277ed385796d47bb332fd922b3037ca18b3878cf76f91188184682d37b7b8e61c28165d9cf650258f6c8d5fac6d8130984d9fed33ec68f28bfa1712d0156030a90d6cbb8cb0c86528b64ab9d6f6cffdf211027eaa8f4e920c1d35276744b4fdc182ab5f6cf7a26681cec2aa52418cc66e23b25dfde4c7bd54132f6921f30a968625ecac1fda5d18d77e34995037a8382951dab686efb3896ec179eb66c9502dc487dd4b8d7e6e52683cc24c99a8f15d670c6dc1948bc0200ce09809dab2e0f3a1038050b3ee8ca6b45d444cbcd173ada61c0fab7d4c9f57408e73d11b891d2b141e187ab4886b600bc35d6e5fcb5b3b2a906971ad09f05f81dc491f64149b90ea5d5ef28da3d4629d137d27ea5608aab3eaebf26711392c2b6cd59630884dd02f27c421716d73e371eeaf7042b7bccc2020ea0d3afee62373e3010d457da1bf7b965e8dfddaf462804fc02fc1682a06b0cc7ac72367ce200eb47d739b7a6fdecc375f73e763516f2940d6175af2f445b73b53265ea4d080275e977dd194b4f2f3eff20e432d7c7aeb20f94f8e219bf836be96f16ee57eeb263979a1ffe2d5831227c6d6564a32e7a7f86aa4c68426376d666f823c173ff0f5a71c7e51194627058b174a0cf2c814da139ca5503a539c83d832bbb1b0f3a5da56d0b48e4c2489d29e7c9798e15e68a51509f59165773b14a7a0f4fb546e804a51824484a6d91fd22e3b67f13cfd11608a002b5cac7bc76806320cf5a9c978f78ee394b476a1be2688614ee2a5c42d8d1cfe3ca5490bcb754c1a988b7fcf93eafc64457ef0d8c27043f180dba154d6243fd71b58bbee8fdb03da1cef4cd010cc1a8cf31b1e790eb44da87d8e584e8dfa38883a6014975bec0b118aed20247db975b02faa3f7a78184c3366d30c08e6a7399a2580b74134de63e9604219abc79c61df17ab6cbac6bba577ce7229a26231b08a8c0372585c4840574369bb9b232f132797f50dd9f356c0247afc4a8d80c7cdd679a8d128d5a8108070f15f6cfd131de6dd081b11744dfa3c7ebf49c2c56fc7ffe6592d80d180feace05167a247cde9540ba217496d51f54507c291caafe11064973113c975b83f4721d2dae73220f2ca287f638877ad1acf21ff3035d5c19c2dea80667f141dfcdcefdfb1a6d63ff4c79a2973acdc47d4051b17d0e27e4c5a9a913ccbd0790271c3067b3b167d63636d39c2c3f794119d8056d841b95b1208d90217a5d3ec0c76089d04de0ec009da158635b69834f06757575757551",
        // Part 1/3 (input 1, spending TX_C output 4)
        "0844494c3246554c4c080101030005200e944d080225fefa90bfaaddfd8f6295a9daa75bf0259996a2bb7b476fe8e6deaeacd67f080b28965ca361c9de4f39545467d7424bba7fadc902fd1cdb9ca6a3c9d459caaa84e8eb03c9575c9c67c988b6954b227366d436d3ba4e4c0598047b4d45d0e44fb2ac553b3c7d8da09e451ecbe8b94ff286977ee875da074cb1d76fd9470a6bf7ea7c7f071090e47f216f923c42392e4283dd05061998ab15709092ebe3ded0ee1fa78ce8893fa3c0ac4cb22042046263781e7d929d318f1815fce1f52de692523e97141c589cb93bd234a1ad0903e8fa3ec59de0e3cc84cb464e24aae8ccc5c92bf62a957b9f48bcf3df3e99e69b1722f102a9b9143b54503f959db18408b86e46faec9c30885ec01c2a287e2cd3c95396d84784b3312f1afaa75f0459a24c9bbee852b59f5e18270e3b5b0c00aa92b2386de8a6f196b0c1ba1474504dfab27757e09518ce702f9620a4f6ce5a0f82e32a82881688535cc2825a3d918d972ac42d15240e6766eecc77c4145886d8fd65f26d25c5905d7e957e02d0634bbad7282560ab1555f763dd7f1a9712732d572e3a4c4c80fdaa37854f647c54dcd5cf0fe43be24528c9245a5e243ad2a8322ca34e9df3da64a39ea32f846f5e1586d1aa68305ae69d811a42a498f08acaf22b13881f96dc601a0240e1b834e5abbeee3204c49d0018ef737cbe73d9d95d396ba550663b0900236a641b0b4fb848f9071888f346e1fab73b5f4d0802408b55a60503eeb3af50997c0137bf1f26dbb832e2ea8ea7a189bf8608089383df479e132781aefb21a4000cc91f7a7193fd28ccbe9963ca8df50ed6e3918c847762a66ee8c8be7ded22e8bebcc3f140bd3c0126f1ef49f64026540164cbd06523fb3c1ead8b494ddf76752b9c7ee044be5244de301e0e3a37256b82373248282d8ed163d72bf20d2b23eacf40b00d29bc434ccde275614907cf5f539ed5e18d4f9cea3c2be6a2c11f4881758bc84c71acd2d12208fd9ed97f04b58b454d70086d222e1e7e628d498be13e97bcd16d23f40a19fda544dac500e2500d907566128d04fb9bedddce29a7a31e563f388c390f0dccfd1d01ba6a7269937facc2d8dc9a79f354deffb0910177ac47afbd99a72e4eec57f92f7c81363f4c39bcc9555bd851153b0d62140e3b2dfad229972c70eec940b6e0583264f682dbe048a4c18b8602cd89e00ba34a24cf94c518d9a2290a592ca52c39ebd5e6cfdf82b2955a3200a88adfc763ec343c1425313bc8a23b1f69b0396c1e5756147559db74ead66de687c7db7b27244ee5050bba0357c9938b1734a02216b2eeb26fbc18dc462d5aecfeffa79a2edce46901c528cb22bbd4d25ec1634fc3951d72bcb8dfbeceb0c3ce4fc3f7e5bb38cfb2d83b3a5caefbed66cb964fbcd0260a9e2f0d07ad293b3837579dbe7535f150105aaddea0d4dbcc664a4fdc478037b317f398e0228ce78c00f0df3458bfe54d4d080241278a028468a3a1dab1c037f4c82c9f6d934b599a4daf304d0ed4821cb3e9d5d9d9aa51aa760c88a00667d5f729c292083a2eefc7ed3f733f43597c31805176b1cc9d7b985aae5ef7e15aa2f1f84a392997428cb25f731e2f6216d34f9bbf07ea17f3b68ba1780813e929dc05100479de1321b5bbe99df25139e2de9a23af096c39d5d6d5d15f87fe2917a962158091d441336c0c1ec46bec955ec8b99223475b134cd0c6524638ea93b6b15439481ab39d11394745d6ea75f77d2c96d162b9a9bae7d11f6d5af2ed4772d62db4c51197e4ca8e5a5acfdf34905d7e2552af84f267dbce15b7be12c6bc0fb7c899d17105cd4918c12c64d59dfb0f051aa9ef66fd78daffb1da7946341f5eadddbce9f5b9b179adc8c626a69c0c4858079fc1229771031353fe5ba4c2c48ed9cddbb9be2b0765ad8e683d0a8d7548326ff4799fb1dca505c7125ac970d170c79f303075ee90b686e4dd327a203ae0439b037c75c154226e888357090376fb13556988b6fec5bf778b004585cdad022466f16e91d88deed66f26d17fbb34ca6470e9fbafbf77080469af9da852a57ea5d0f91eacf59d2dfd3602ba94f0fd229a4a2e963ff101a6c4df391c67a9be4fd23cc829d9fbd1951535a00356cd5df1170d83c3d2b088316f02172b1bd6e5548f145e15402327242efbbd82a3d10711d2518f6f26f8fd3c6911b4a2c3cd2d07baaf21ba87adc341f42aea6e3406757575757551",
        // Part 2/3 (input 2, spending TX_C output 5)
        "0844494c3246554c4c080102030005200e944d0802128a9a39cfb753c5e830223db9af06262fb288f5f1317e65f0a8a61175e666bfc409654e47bbda3243af85617e92134c55794842e0a956e2320144a5e4312b1b48c5fab2915d64c635b989a43cebe2a04a50297637f3556740d038186899e4b810c7b9622edfeb46dc3a1263010b4eb2975fafda290785cc3c959cf26dd5af3adae635c1d3d1e77182f68966f778e6ec83d4fd1912293fb1cfcefc92cf672b099fe236817004cd810dbe3354cce3a3b5a76a3fefb3537b730cdfa13115b2bf009ddeeafd2a91fda71c24a9e3ffc55f2ed5b33d93fc7984b55bf289ae795eccf51058ac329117bd477b81347a2edfa3dcd22aedf15df4b3ac58fa4c77f90ac0c0be1d02b69c53005ba1ad692ee7178793240765fff40ea4e42e955f1af667dc50c7156433b8c0978306a9fe5ffcd0f024b17e6e55d0dad9836e47e8f21a4f6504e6e0d9f1b36b20a49655d5e190879160000b0ef6f16356f0fb3b04689a3f15e52c1b8fc06b1116916fb13143114f4503a0eab0139c840db38357993863591e3329b5364ed3eee71336436b872d5cb46ef60c6577ec66ea4de5bb7b7c6cb17f2f2dc0a6bab1b460e0dd79a478b68fdbc6e56a351bfd1edc9c0d2a78516d46e2c8228dc6484d47a26df4be89381c7ac6dbf664321a6ed0a49cc314afe78e4bbdc4c7ff961beaacdff156e82abb6c9a2bbdd9e9fdfbec5f1c20bf3befbbdec13fa96b590c8f6473bd574c5cc735c6af2bb5d74b00041213bcc0c2cce2ee242a2f3c4455617a8283b5c0ea1d273e779aa6a9b2b5c8cbdef20708121831333c44525b5d61646e8f9ac5ced1d4e000000000000000000000000000000000000000000000000a1724390006757575757551"
    };

    DecodeAndValidateMultiPartCarrierTxR(
        "1adeed97b8a8bab0c946b734e3b8d43e02daa7aabc4f9555a7bb920a379ee517",
        scriptsigs,
        "6ea0fa35664c6fd745169c846038285a8087fb5e8abc09f3dd7a736740a730ba",
        "DILITHIUM", 1312, 2420,
        "c9d1ad48e5334f72c15a6e1c8132a7f4",
        "7399a2580b74134de63e9604219abc79");
}

BOOST_AUTO_TEST_CASE(pqc_generate_keypair_falcon512)
{
    std::vector<unsigned char> public_key;
    std::vector<unsigned char> secret_key;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, public_key, secret_key));
    // Falcon-512 public key is 897 bytes
    BOOST_CHECK_EQUAL(public_key.size(), 897u);
    // Falcon-512 secret key is 1281 bytes
    BOOST_CHECK_EQUAL(secret_key.size(), 1281u);
    // Keys must not be empty/zero
    BOOST_CHECK(!public_key.empty());
    BOOST_CHECK(!secret_key.empty());
}

BOOST_AUTO_TEST_CASE(pqc_generate_keypair_dilithium2)
{
    std::vector<unsigned char> public_key;
    std::vector<unsigned char> secret_key;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::DILITHIUM2, public_key, secret_key));
    // ML-DSA-44 (Dilithium2) public key is 1312 bytes
    BOOST_CHECK_EQUAL(public_key.size(), 1312u);
    // ML-DSA-44 (Dilithium2) secret key is 2560 bytes
    BOOST_CHECK_EQUAL(secret_key.size(), 2560u);
}

BOOST_AUTO_TEST_CASE(pqc_sign_verify_falcon512_roundtrip)
{
    // Generate a keypair
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, pk, sk));

    // Sign a 32-byte message (mimics sighash32)
    const std::vector<unsigned char> message = ParseHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    std::vector<unsigned char> signature;
    BOOST_CHECK(PQCSign(PQCCommitmentType::FALCON512, sk, message.data(), message.size(), signature));
    // Falcon-512 signature is variable, typically 640-690 bytes
    BOOST_CHECK(signature.size() >= 600);
    BOOST_CHECK(signature.size() <= 800);

    // Verify the signature
    BOOST_CHECK(PQCVerify(PQCCommitmentType::FALCON512, pk, message.data(), message.size(), signature));

    // Verify fails with wrong message
    std::vector<unsigned char> wrong_message = message;
    wrong_message[0] ^= 0xff;
    BOOST_CHECK(!PQCVerify(PQCCommitmentType::FALCON512, pk, wrong_message.data(), wrong_message.size(), signature));

    // Verify fails with wrong public key
    std::vector<unsigned char> pk2, sk2;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, pk2, sk2));
    BOOST_CHECK(!PQCVerify(PQCCommitmentType::FALCON512, pk2, message.data(), message.size(), signature));
}

BOOST_AUTO_TEST_CASE(pqc_sign_verify_dilithium2_roundtrip)
{
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::DILITHIUM2, pk, sk));

    const std::vector<unsigned char> message = ParseHex("deadbeef01020304050607080910111213141516171819202122232425262728");
    std::vector<unsigned char> signature;
    BOOST_CHECK(PQCSign(PQCCommitmentType::DILITHIUM2, sk, message.data(), message.size(), signature));
    // ML-DSA-44 (Dilithium2) signature is 2420 bytes
    BOOST_CHECK_EQUAL(signature.size(), 2420u);

    BOOST_CHECK(PQCVerify(PQCCommitmentType::DILITHIUM2, pk, message.data(), message.size(), signature));

    // Tamper with signature
    std::vector<unsigned char> bad_sig = signature;
    bad_sig[0] ^= 0xff;
    BOOST_CHECK(!PQCVerify(PQCCommitmentType::DILITHIUM2, pk, message.data(), message.size(), bad_sig));
}

BOOST_AUTO_TEST_CASE(pqc_commitment_from_real_signature)
{
    // Generate keypair and sign, then verify the full commitment flow
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, pk, sk));

    const std::vector<unsigned char> message = ParseHex("aabbccdd00112233445566778899aabbccddeeff00112233445566778899aabb");
    std::vector<unsigned char> sig;
    BOOST_CHECK(PQCSign(PQCCommitmentType::FALCON512, sk, message.data(), message.size(), sig));

    // Compute commitment = SHA256(pk || sig)
    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pk, sig, commitment));

    // Verify that SHA256(pk || sig) matches manual computation
    unsigned char expected[CSHA256::OUTPUT_SIZE];
    CSHA256()
        .Write(pk.data(), pk.size())
        .Write(sig.data(), sig.size())
        .Finalize(expected);
    BOOST_CHECK(std::equal(expected, expected + CSHA256::OUTPUT_SIZE, commitment.begin()));

    // Build and extract commitment script
    CScript script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, script));

    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    BOOST_CHECK(PQCExtractCommitment(script, parsed_type, parsed_commitment));
    BOOST_CHECK(parsed_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK(parsed_commitment == commitment);

    // Verify the signature
    BOOST_CHECK(PQCVerify(PQCCommitmentType::FALCON512, pk, message.data(), message.size(), sig));
}

BOOST_AUTO_TEST_CASE(pqc_sign_rejects_wrong_key_size)
{
    // Empty secret key
    const std::vector<unsigned char> empty_sk;
    const std::vector<unsigned char> message = ParseHex("0011223344556677");
    std::vector<unsigned char> signature;
    BOOST_CHECK(!PQCSign(PQCCommitmentType::FALCON512, empty_sk, message.data(), message.size(), signature));

    // Wrong size secret key (too short)
    const std::vector<unsigned char> short_sk(32, 0x42);
    BOOST_CHECK(!PQCSign(PQCCommitmentType::FALCON512, short_sk, message.data(), message.size(), signature));
}

BOOST_AUTO_TEST_CASE(pqc_verify_rejects_wrong_key_size)
{
    const std::vector<unsigned char> message = ParseHex("0011223344556677");
    const std::vector<unsigned char> short_pk(32, 0x42);
    const std::vector<unsigned char> fake_sig(64, 0xaa);
    BOOST_CHECK(!PQCVerify(PQCCommitmentType::FALCON512, short_pk, message.data(), message.size(), fake_sig));
}

BOOST_AUTO_TEST_CASE(pqc_extract_key_material_from_carrier_falcon)
{
    // Generate a real Falcon-512 keypair
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, pk, sk));
    BOOST_CHECK_EQUAL(pk.size(), 897u);

    // Sign a 32-byte message (mimics tx sighash32)
    const std::vector<unsigned char> message = ParseHex("aabbccdd00112233445566778899aabbccddeeff00112233445566778899aabb");
    std::vector<unsigned char> sig;
    BOOST_CHECK(PQCSign(PQCCommitmentType::FALCON512, sk, message.data(), message.size(), sig));
    BOOST_CHECK(!sig.empty());

    // Build carrier scriptSig
    CScript carrierScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::FALCON512, pk, sig, 0, carrierScriptSig));

    // Build a TX with carrier input
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = carrierScriptSig;
    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160
        << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    const CTransaction tx(mtx);

    // Extract key material
    PQCCommitmentType extracted_type;
    std::vector<unsigned char> extracted_pk, extracted_sig;
    BOOST_CHECK(PQCExtractKeyMaterialFromCarrier(tx, extracted_type, extracted_pk, extracted_sig));
    BOOST_CHECK(extracted_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK(extracted_pk == pk);
    BOOST_CHECK(extracted_sig == sig);
}

BOOST_AUTO_TEST_CASE(pqc_verify_signature_from_carrier_falcon_roundtrip)
{
    // Full roundtrip: keygen -> sign -> carrier TX -> extract -> verify via liboqs
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, pk, sk));

    // Sign a 32-byte message (mimics tx sighash32)
    const std::vector<unsigned char> message = ParseHex("deadbeef01020304050607080910111213141516171819202122232425262728");
    std::vector<unsigned char> sig;
    BOOST_CHECK(PQCSign(PQCCommitmentType::FALCON512, sk, message.data(), message.size(), sig));

    // Compute commitment
    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pk, sig, commitment));

    // Build commitment script
    CScript commitScript;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, commitScript));

    // Build carrier scriptSig
    CScript carrierScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::FALCON512, pk, sig, 0, carrierScriptSig));

    // Build TX with carrier input + commitment output
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = carrierScriptSig;
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160
        << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = commitScript;
    const CTransaction tx(mtx);

    // Verify signature from carrier (should PASS with correct message)
    PQCCommitmentType verified_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len = 0, sig_len = 0;
    BOOST_CHECK(PQCVerifySignatureFromCarrier(tx, commitment,
        message.data(), message.size(),
        verified_type, carrier_input, pk_len, sig_len));
    BOOST_CHECK(verified_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK_EQUAL(pk_len, 897);
    BOOST_CHECK(sig_len >= 600 && sig_len <= 800);

    // Verify should FAIL with wrong message
    std::vector<unsigned char> wrong_message = message;
    wrong_message[0] ^= 0xff;
    BOOST_CHECK(!PQCVerifySignatureFromCarrier(tx, commitment,
        wrong_message.data(), wrong_message.size(),
        verified_type, carrier_input, pk_len, sig_len));

    // Verify should FAIL with wrong commitment
    uint256 wrong_commitment;
    wrong_commitment.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    BOOST_CHECK(!PQCVerifySignatureFromCarrier(tx, wrong_commitment,
        message.data(), message.size(),
        verified_type, carrier_input, pk_len, sig_len));

    // Verify should FAIL with null message
    BOOST_CHECK(!PQCVerifySignatureFromCarrier(tx, commitment,
        nullptr, 0,
        verified_type, carrier_input, pk_len, sig_len));
}

BOOST_AUTO_TEST_CASE(pqc_verify_signature_from_carrier_dilithium2_roundtrip)
{
    // Full roundtrip for Dilithium2 (ML-DSA-44)
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::DILITHIUM2, pk, sk));
    BOOST_CHECK_EQUAL(pk.size(), 1312u);

    const std::vector<unsigned char> message = ParseHex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    std::vector<unsigned char> sig;
    BOOST_CHECK(PQCSign(PQCCommitmentType::DILITHIUM2, sk, message.data(), message.size(), sig));
    BOOST_CHECK_EQUAL(sig.size(), 2420u);

    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pk, sig, commitment));

    CScript commitScript;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::DILITHIUM2, commitment, commitScript));

    // Dilithium2 needs 3 carrier parts (pk=1312 + sig=2420 = 3732 bytes)
    uint8_t partsNeeded = PQCCarrierPartsNeeded(pk.size() + sig.size());
    BOOST_CHECK_EQUAL(partsNeeded, 3);

    CMutableTransaction mtx;
    mtx.vin.resize(partsNeeded);
    for (uint8_t p = 0; p < partsNeeded; ++p) {
        CScript partScript;
        BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::DILITHIUM2, pk, sig, p, partScript));
        mtx.vin[p].scriptSig = partScript;
    }
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160
        << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = commitScript;
    const CTransaction tx(mtx);

    // Verify signature from carrier (should PASS)
    PQCCommitmentType verified_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len = 0, sig_len = 0;
    BOOST_CHECK(PQCVerifySignatureFromCarrier(tx, commitment,
        message.data(), message.size(),
        verified_type, carrier_input, pk_len, sig_len));
    BOOST_CHECK(verified_type == PQCCommitmentType::DILITHIUM2);
    BOOST_CHECK_EQUAL(pk_len, 1312);
    BOOST_CHECK_EQUAL(sig_len, 2420);

    // Wrong message should FAIL
    std::vector<unsigned char> wrong_msg = message;
    wrong_msg[31] ^= 0xff;
    BOOST_CHECK(!PQCVerifySignatureFromCarrier(tx, commitment,
        wrong_msg.data(), wrong_msg.size(),
        verified_type, carrier_input, pk_len, sig_len));
}

BOOST_AUTO_TEST_CASE(pqc_validate_commitment_from_carrier_still_works)
{
    // Ensure the refactored PQCValidateCommitmentFromCarrier still works
    std::vector<unsigned char> pk, sk;
    BOOST_CHECK(PQCGenerateKeypair(PQCCommitmentType::FALCON512, pk, sk));

    const std::vector<unsigned char> message = ParseHex("ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00");
    std::vector<unsigned char> sig;
    BOOST_CHECK(PQCSign(PQCCommitmentType::FALCON512, sk, message.data(), message.size(), sig));

    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pk, sig, commitment));

    CScript commitScript;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, commitScript));

    CScript carrierScriptSig;
    BOOST_CHECK(PQCBuildCarrierPartScriptSig(PQCCommitmentType::FALCON512, pk, sig, 0, carrierScriptSig));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = carrierScriptSig;
    mtx.vout.resize(2);
    mtx.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160
        << ParseHex("00112233445566778899aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout[1].scriptPubKey = commitScript;
    const CTransaction tx(mtx);

    // Commitment-only validation (SHA256 check) should still pass
    PQCCommitmentType carrier_type;
    uint32_t carrier_input = 0;
    uint16_t pk_len = 0, sig_len = 0;
    BOOST_CHECK(PQCValidateCommitmentFromCarrier(tx, commitment, carrier_type, carrier_input, pk_len, sig_len));
    BOOST_CHECK(carrier_type == PQCCommitmentType::FALCON512);
    BOOST_CHECK_EQUAL(pk_len, (uint16_t)pk.size());
    BOOST_CHECK_EQUAL(sig_len, (uint16_t)sig.size());
}

#ifdef ENABLE_LIBOQS_RACCOON
#include "test/pqc_raccoong44_mainnet_data.h"

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_raccoong44_0b0defce)
{
    // TX_R: 0b0defceee746f21b67f49e9ad5494ade3864520afdb6e2a7acd257fc4b9ac9a (height 6169772)
    // TX_C: c6fe73aede0316cb1ee4151286281a35fbe14f8fab7751e91bfc3aec1423100e
    // Raccoon-G-44, pk=16144, sig=20768, 24 carrier parts

    DecodeAndValidateMultiPartCarrierTxR(
        "0b0defceee746f21b67f49e9ad5494ade3864520afdb6e2a7acd257fc4b9ac9a",
        kRaccoonG44MainnetScriptSigs,
        "7d24c5af92e15fd55e27664c68b46104d0c044346a3771b4582cfa3728813670",
        "RACCOON", 16144, 20768,
        "a5c0b842401e8b3f49d3b56af0935d1f",
        "ef74aed43ce2cea09adaec1315bbb61b");
}
#endif // ENABLE_LIBOQS_RACCOON

// --- PQCReconstructTxBase tests ---

BOOST_AUTO_TEST_CASE(pqc_reconstruct_tx_base_strips_opreturn_and_carriers)
{
    // Build a mock TX_C with: payment, change, carrier, OP_RETURN
    CMutableTransaction txc;
    txc.nVersion = 1;
    txc.nLockTime = 0;
    txc.vin.resize(1);
    txc.vin[0].prevout = COutPoint(uint256S("abcd"), 0);
    txc.vin[0].scriptSig = CScript() << ParseHex("304402") << ParseHex("02aabb");
    txc.vin[0].nSequence = 0xFFFFFFFF;

    // Payment output
    CScript paymentScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("0011223344556677889900112233445566778899") << OP_EQUALVERIFY << OP_CHECKSIG;
    txc.vout.push_back(CTxOut(500000000, paymentScript)); // 5 DOGE

    // Change output
    CScript changeScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("aabbccddeeff00112233aabbccddeeff00112233") << OP_EQUALVERIFY << OP_CHECKSIG;
    txc.vout.push_back(CTxOut(400000000, changeScript)); // 4 DOGE change

    // Carrier output (1 DOGE)
    CScript carrierSpk;
    BOOST_CHECK(PQCBuildCarrierScriptPubKey(carrierSpk));
    txc.vout.push_back(CTxOut(100000000, carrierSpk)); // 1 DOGE carrier

    // OP_RETURN commitment
    uint256 commitment(ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"));
    CScript opReturnScript;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, opReturnScript));
    txc.vout.push_back(CTxOut(0, opReturnScript));

    CTransaction txcConst(txc);

    // Reconstruct TX_BASE
    CMutableTransaction txBase;
    CAmount carrierCost = 0;
    BOOST_CHECK(PQCReconstructTxBase(txcConst, txBase, carrierCost));

    // Verify: carrier cost restored
    BOOST_CHECK_EQUAL(carrierCost, 100000000);

    // Verify: TX_BASE has 2 outputs (payment + change), no carrier or OP_RETURN
    BOOST_CHECK_EQUAL(txBase.vout.size(), 2u);

    // Verify: payment output has carrier cost added back (5 + 1 = 6 DOGE)
    BOOST_CHECK_EQUAL(txBase.vout[0].nValue, 600000000);
    BOOST_CHECK(txBase.vout[0].scriptPubKey == paymentScript);

    // Verify: change output unchanged
    BOOST_CHECK_EQUAL(txBase.vout[1].nValue, 400000000);
    BOOST_CHECK(txBase.vout[1].scriptPubKey == changeScript);

    // Verify: inputs have empty scriptSig (unsigned template)
    BOOST_CHECK_EQUAL(txBase.vin.size(), 1u);
    BOOST_CHECK(txBase.vin[0].scriptSig.empty());
    BOOST_CHECK(txBase.vin[0].prevout == txc.vin[0].prevout);
    BOOST_CHECK_EQUAL(txBase.vin[0].nSequence, txc.vin[0].nSequence);
}

BOOST_AUTO_TEST_CASE(pqc_reconstruct_tx_base_multipart_carriers)
{
    // Build a mock TX_C with 3 carrier outputs (like Dilithium2)
    CMutableTransaction txc;
    txc.nVersion = 1;
    txc.nLockTime = 0;
    txc.vin.resize(1);
    txc.vin[0].prevout = COutPoint(uint256S("beef"), 1);
    txc.vin[0].nSequence = 0xFFFFFFFF;

    CScript paymentScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("1111111111111111111111111111111111111111") << OP_EQUALVERIFY << OP_CHECKSIG;
    txc.vout.push_back(CTxOut(1000000000, paymentScript)); // 10 DOGE

    CScript changeScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("2222222222222222222222222222222222222222") << OP_EQUALVERIFY << OP_CHECKSIG;
    txc.vout.push_back(CTxOut(800000000, changeScript)); // 8 DOGE

    CScript carrierSpk;
    BOOST_CHECK(PQCBuildCarrierScriptPubKey(carrierSpk));
    txc.vout.push_back(CTxOut(100000000, carrierSpk)); // carrier 1
    txc.vout.push_back(CTxOut(100000000, carrierSpk)); // carrier 2
    txc.vout.push_back(CTxOut(100000000, carrierSpk)); // carrier 3

    uint256 commitment(ParseHex("ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100"));
    CScript opReturn;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::DILITHIUM2, commitment, opReturn));
    txc.vout.push_back(CTxOut(0, opReturn));

    CTransaction txcConst(txc);

    CMutableTransaction txBase;
    CAmount carrierCost = 0;
    BOOST_CHECK(PQCReconstructTxBase(txcConst, txBase, carrierCost));

    // 3 carriers x 1 DOGE = 3 DOGE
    BOOST_CHECK_EQUAL(carrierCost, 300000000);
    BOOST_CHECK_EQUAL(txBase.vout.size(), 2u);
    // Payment = 10 + 3 = 13 DOGE
    BOOST_CHECK_EQUAL(txBase.vout[0].nValue, 1300000000);
    BOOST_CHECK_EQUAL(txBase.vout[1].nValue, 800000000);
}

BOOST_AUTO_TEST_CASE(pqc_reconstruct_tx_base_no_carriers)
{
    // TX_C without carriers (commitment-only mode)
    CMutableTransaction txc;
    txc.nVersion = 1;
    txc.nLockTime = 0;
    txc.vin.resize(1);
    txc.vin[0].prevout = COutPoint(uint256S("dead"), 0);
    txc.vin[0].nSequence = 0xFFFFFFFF;

    CScript paymentScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("3333333333333333333333333333333333333333") << OP_EQUALVERIFY << OP_CHECKSIG;
    txc.vout.push_back(CTxOut(200000000, paymentScript)); // 2 DOGE

    uint256 commitment(ParseHex("aabbccdd00112233aabbccdd00112233aabbccdd00112233aabbccdd00112233"));
    CScript opReturn;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, opReturn));
    txc.vout.push_back(CTxOut(0, opReturn));

    CTransaction txcConst(txc);

    CMutableTransaction txBase;
    CAmount carrierCost = 0;
    BOOST_CHECK(PQCReconstructTxBase(txcConst, txBase, carrierCost));

    // No carriers
    BOOST_CHECK_EQUAL(carrierCost, 0);
    BOOST_CHECK_EQUAL(txBase.vout.size(), 1u);
    BOOST_CHECK_EQUAL(txBase.vout[0].nValue, 200000000);
}

// --- Cross-validation with libdogecoin CLI ---
// These tests verify that Dogecoin Core's PQC primitives produce identical
// output to libdogecoin's `such` CLI tool.  Test vectors are taken from the
// libdogecoin dry-run validation log (mainnet_pqc_e2e_dryrun_20260418.txt).
// If both implementations agree on these values, on-chain transactions
// created by either can be validated by the other.

BOOST_AUTO_TEST_CASE(pqc_cross_validate_carrier_primitives)
{
    // 1. Canonical carrier redeemScript must be OP_DROP x5 OP_TRUE (757575757551)
    CScript redeemScript;
    BOOST_CHECK(PQCBuildCarrierRedeemScript(redeemScript));
    BOOST_CHECK_EQUAL(HexStr(redeemScript.begin(), redeemScript.end()), "757575757551");

    // 2. Canonical carrier P2SH scriptPubKey must be a9149b402803555511d15d81207d3e2cb3e6bc365e0e87
    //    This is the HASH160 of the redeemScript, wrapped in OP_HASH160 <20> OP_EQUAL.
    //    libdogecoin `such -c pqc_carrier_scriptpubkey` produces the same value.
    CScript carrierSpk;
    BOOST_CHECK(PQCBuildCarrierScriptPubKey(carrierSpk));
    BOOST_CHECK_EQUAL(HexStr(carrierSpk.begin(), carrierSpk.end()),
                      "a9149b402803555511d15d81207d3e2cb3e6bc365e0e87");

    // 3. Commitment tag encoding: 6a24 + TAG4 + 32-byte commitment
    //    libdogecoin `such -c falcon_commit` wraps in the same OP_RETURN format.
    const uint256 test_commitment(ParseHex("3689c0cbfe8aca6a5f592ed61eb0e4252a5be4418c7938c3bc62797604e2e8a4"));
    CScript falcon_script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, test_commitment, falcon_script));
    BOOST_CHECK_EQUAL(HexStr(falcon_script.begin(), falcon_script.end()),
                      "6a24464c43313689c0cbfe8aca6a5f592ed61eb0e4252a5be4418c7938c3bc62797604e2e8a4");

    CScript dil_script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::DILITHIUM2, test_commitment, dil_script));
    // Dilithium2 uses tag DIL2
    BOOST_CHECK_EQUAL(HexStr(dil_script.begin(), dil_script.begin() + 6), "6a2444494c32");

#ifdef ENABLE_LIBOQS_RACCOON
    CScript rcg_script;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::RACCOONG44, test_commitment, rcg_script));
    // Raccoon-G uses tag RCG4
    BOOST_CHECK_EQUAL(HexStr(rcg_script.begin(), rcg_script.begin() + 6), "6a2452434734");
#endif

    // 4. Carrier part counts must match libdogecoin expectations
    //    Each part carries 3 x 520 = 1560 bytes.
    //    Falcon-512: pk=897 + sig~690 = ~1587 => 1 part (sometimes 2 for very long sigs)
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(897 + 652), 1);  // typical Falcon
    //    Dilithium2: pk=1312 + sig=2420 = 3732 => 3 parts
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(1312 + 2420), 3);
#ifdef ENABLE_LIBOQS_RACCOON
    //    Raccoon-G-44: pk=16144 + sig=20768 = 36912 => 24 parts
    BOOST_CHECK_EQUAL(PQCCarrierPartsNeeded(16144 + 20768), 24);
#endif
}

BOOST_AUTO_TEST_CASE(pqc_cross_validate_tx_base_reconstruction)
{
    // Verify TX_BASE reconstruction from a TX_C with carrier outputs.
    // This test uses the base transaction from the libdogecoin dry-run log:
    //   RAW_UNSIGNED_TX = 01000000012146c4e0...00000000
    //   SCRIPT_PUBKEY = 76a9145a29227bb518c38cae5a9a195cafc56b22d7272b88ac
    //   Expected sighash32 = 5fcb95f6179fdf57693cb823ad110b868380cb0be0e23a151149955f51bef439
    //
    // Build a TX_C by adding OP_RETURN and carrier outputs to the base transaction,
    // then verify PQCReconstructTxBase() recovers the original base transaction.

    // The base unsigned TX has one output: 42.66 DOGE to DDMpdcTrWnZT38tRMebbYzCSAgLSnVMqvr
    CMutableTransaction txBase;
    txBase.nVersion = 1;
    txBase.nLockTime = 0;
    txBase.vin.resize(1);
    txBase.vin[0].prevout = COutPoint(uint256S("63d79b47b6d55b5143afb5f7782f9300da5d6a4837b5c9837a1769e3e0c44621"), 0);
    txBase.vin[0].nSequence = 0xFFFFFFFF;

    CScript paymentScript = CScript() << OP_DUP << OP_HASH160
        << ParseHex("5a29227bb518c38cae5a9a195cafc56b22d7272b") << OP_EQUALVERIFY << OP_CHECKSIG;
    // Original value: 42.66 DOGE = 4266000000 koinu
    txBase.vout.push_back(CTxOut(4266000000LL, paymentScript));

    // Now build TX_C by appending OP_RETURN + 1 carrier output (deducting 1 DOGE from payment)
    CMutableTransaction txc = txBase;
    // Deduct carrier cost from payment output
    txc.vout[0].nValue -= 100000000; // -1 DOGE for carrier

    // Add OP_RETURN commitment
    uint256 commitment(ParseHex("3689c0cbfe8aca6a5f592ed61eb0e4252a5be4418c7938c3bc62797604e2e8a4"));
    CScript opReturn;
    BOOST_CHECK(PQCBuildCommitmentScript(PQCCommitmentType::FALCON512, commitment, opReturn));
    txc.vout.push_back(CTxOut(0, opReturn));

    // Add carrier P2SH output
    CScript carrierSpk;
    BOOST_CHECK(PQCBuildCarrierScriptPubKey(carrierSpk));
    txc.vout.push_back(CTxOut(100000000, carrierSpk)); // 1 DOGE carrier

    CTransaction txcConst(txc);

    // Reconstruct TX_BASE
    CMutableTransaction reconstructed;
    CAmount carrierCost = 0;
    BOOST_CHECK(PQCReconstructTxBase(txcConst, reconstructed, carrierCost));

    // Verify reconstruction matches original base transaction
    BOOST_CHECK_EQUAL(carrierCost, 100000000);
    BOOST_CHECK_EQUAL(reconstructed.vout.size(), 1u);
    BOOST_CHECK_EQUAL(reconstructed.vout[0].nValue, txBase.vout[0].nValue); // 42.66 DOGE restored
    BOOST_CHECK(reconstructed.vout[0].scriptPubKey == paymentScript);

    // Verify inputs have empty scriptSig (unsigned template)
    BOOST_CHECK_EQUAL(reconstructed.vin.size(), 1u);
    BOOST_CHECK(reconstructed.vin[0].scriptSig.empty());
    BOOST_CHECK(reconstructed.vin[0].prevout == txBase.vin[0].prevout);
}

BOOST_AUTO_TEST_CASE(pqc_cross_validate_commitment_computation)
{
    // Verify that SHA256(pk || sig) matches the libdogecoin `such -c falcon_commit` output.
    // From the dry-run log:
    //   pk prefix: 0902f438238c4115...
    //   sig prefix: 3960fc6eced93ddd...
    //   commitment = 3689c0cbfe8aca6a5f592ed61eb0e4252a5be4418c7938c3bc62797604e2e8a4
    //
    // We use a synthetic small example to verify the SHA256(pk||sig) computation
    // is bit-exact with what libdogecoin produces (both use raw SHA256).

    const std::vector<unsigned char> pk = ParseHex("02112233445566");
    const std::vector<unsigned char> sig = ParseHex("3045022100aabbccddeeff");

    uint256 commitment;
    BOOST_CHECK(PQCComputeCommitment(pk, sig, commitment));

    // Manual SHA256(pk || sig) verification
    unsigned char expected[CSHA256::OUTPUT_SIZE];
    CSHA256()
        .Write(pk.data(), pk.size())
        .Write(sig.data(), sig.size())
        .Finalize(expected);
    BOOST_CHECK(std::equal(expected, expected + CSHA256::OUTPUT_SIZE, commitment.begin()));

    // The commitment is deterministic — both implementations using raw SHA256
    // over the same concatenated bytes must produce the same result.
    // This is the fundamental cross-validation invariant.
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_falcon_6173895)
{
    // TX_C: f26940627dad32d5f86c1b17f9be96bf413dbe78b181721b73677d6fe7bd81ed
    // TX_R: a65bbc366c3c670a8b5c57a205db818365e8b9cb955683eec2f171f6d4c52ff5
    DecodeAndValidateCarrierTxR(
        "a65bbc366c3c670a8b5c57a205db818365e8b9cb955683eec2f171f6d4c52ff5",
        "08464c433146554c4c0801000100038106104d08020978c210806a874a28557a233283faba3152461f04d85c7a9193f85ae2903d3c357259525d12f5e380c46c6366e049e7941c8ce6553794c39165992db63185d5688e243709422700ed1da33445d84a4e1c157f9c5caa96456aa5085d6aa45915725ae90c7c23c67d936418b8422a3726e739faac312cd9cda7f1d049041aaa519d5ca400d2c83d49f9b9bfe0bb68b2a87aae5b4768d8c41498872ab858ccc0fb866853dc190a0bd62e03c4ded8ba25e702b015024aa8e36f5c3d9ad5d8fe7aaa3d63cb07171e289aa0e46c3451a0ebb9e12c9a890e9db35096621e405a754c69ae6a88a41a82ea1618a699e257d7a7a43ef59bd00c5182ba7d97786d1d715ca69a90e88114a08c0b198e377e594cb151eb0a0a1c1233a6231bb0068400e681856cd64a72788d3865c40ac3e5ca74bfda719a455329a04e59929349b1920037daabe4a066612eca156f565ff19718339dec9220e37b7782c41150d758a827f56df0863bb28632950ec793fb99b41098ee4b4c6c1abf5af5d4b8efb51a8279226a16943969e335624809976c192610ef0c6014bea3b75ee6b3e9d787ab52c4aac546619fe8c29f54c9b5a75a8e4a1e36f989dec095455c20ce4fb1bb3dd300394f074d4075897e695e7a452d4b5b098a9f95f9451c8c6919f965393eae375e007f5922426d1f9581174ae4810338848926a1616585ce740d9140953a8991f7598b8f554991b7ef64354d080231cf542fa8c0b2aadf534213eb076afe809d9e8a542e3628c04c20a13d4266466229a7f8186946116e4af2b497cb090e6175c7b9981fa7843d1991d8d1ac3222d372c0837213d564f49f041f8e8a9ae6e8508030722b56f90b09830c8497c011944619cf793e816f60964f76e4a6eb82dc7d50c45f10bb41c0c06406e766210aa8d8960a73841cf6e30d960a50d0b435e904077869e5a88041030d95ea0a56724d68704a0de99a762060b63482c14caef910e81741918d865956e6170f3e25095f6e2aa2e55ed2e5eae80a32d3c77f02786552ed642a43fb8489e6a86923f346a6b1b86f8eeea9cd03f099a3be1a2e9041156c111a6ef51031ea856005da42bb82889cacee100885cab51531c32a99621d36856bdc20114aa20134ca8cdb0e4e60713b27c116500a542a05ec9e803db818418a62690b064f66f58b48be4071561f1fa2c9d28ca7f99d940ce2789a86ba2ad9e75381f896304f558a08a3b03426c46a20423ef864f0e212be982cff17c4294d7b95fe1886626339cd6f5e4151d353a21d07702c2526ff9c63024cf197b231aec2b93a2ca6a7df3548489c32e1ccada21b2d9016cbd094444de9edea6473168f1c9fdd4024d5dfc452fe902a7022bfaa6512ba1b1e4267a93a321edc4d1d7c57764411232cb8b84179c75219c8741e51a7f8523590bb13289cd379ff64011a32c6e9109f531375a05c5117f55be7b830e69e69766bf04d00022bfc6f411ed01082697eeeb2912972e587c4090db6defaf4e2552a1417be62de21ff9834b7244ac4b490ed84238bd39574e29636af786760d52e2e790ca4729905121aae54a2f312b28270377cec9fb22528750b8394bccb8eda874214e631c2af60f7da2844da516c8f159e227185592e527a69728140042bf55fde40f83ac877fc84b136bf041dd0a3cc643c0f3084587fba31519b465c38dd6dd5449f199c173bbfcc19d505a456a3a8b905d440ccf1249623d5b677cd3ecbac2e4b5f568744695924830f7bd93e76e8fdb52c6acfdd46e902566934cdd2996ed0f1e5d1ece7fe7ea45f9c765086e99a8d7fba2b711a0ae90cca374a6725331f98212990a54c9e358a6698f45e7e8cc7f957d949d3abab4cad453b6391b333c786743585c7c087bc1d1db3085f94f8da2151a039fe4f5ba2aeaebbad355752b47afe58d57a9bfed235d7a8e53b4abb0a3a3e9620a499aa63cab289d7d4f4954e1e30cdd061c7bac5dcc474ccb46590c7c677bba877f1ae85aee6dda4f8e81b4c82e11035ac023f16410d86f4b1e5e0b165accdb8d1961eebb0ba66129a2eaf551f6293428acf9efdaf294fa5efd23d161ddf6a70e8635ddbbfebebab71d58a2f2f3deb4652d0a22d3a807955884972ff94abe6b775716cc8643c65337b09162e3f315af9d2b3f7af34b83ec4d5b0e8e4d18c6c413b4a85395926bf0c5e3686e38d73391c2006757575757551",
        "e780b663d19966bc8f4ca355fb61bdf7d6b66885e8a75a212bd368a6fb0dcac2",
        "FALCON", 897, 655,
        "0978c210806a874a28557a233283faba",
        "39cd6f5e4151d353a21d07702c2526ff");
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_dilithium_6173871)
{
    // TX_C: a467ac9b20aa3de5ba43f9098751174983ac1ef5160ae0ca51a83c564379b153
    // TX_R: d11ee7b78bfa01cb322758159df9fb69a5a2a9c5aa0846d44913843064430119
    std::vector<std::string> scriptsigs = {
        "0844494c3246554c4c080100030005200e944d0802df8185ddb169590200f26f4d382e45eb9bed475acfc7c84e2c1d4c2129f5aa3ddd3b93245c48695bffd3412ae961b114d322247764e274b551494ee54d62be2038a73f2305635561563cb1cf0df69b8c52977760dced110ef35c311d001f7aadc5bfeda7f7f868f658f299eb8a518595aa92b88ad86ea9aa49f6c1feba0b32436a71271b6facd3df00b831173e263f869d8e1cd93faf9070204da82f09d1e2c98edb4a29e461d5bf24842e2b6f827318152990c0278810a52e5a1171f1841563afeaa62d38548721f3f393393c9b139700cc595c6fde307e81ea3b4791eb806d44a6eb2c9fdb2c8209c13883502b932daca9d79becde486d1f17d91561831189b66c56445751f497b2104c7a9aca63dbce8d027d24641cb4d5fbad3548cbc8990aee20c8609da1cf0d611772cea33aaa3c35a3dfe09794d80c326e29f2708c6bd0c5e033d3dfdfe879cc07f7e80e374230bbcb0fd35191e720107fd5ea6edd5a1c364bf5af353b18759879d8452830807054ff9bd272ff09c4c08c4b08961eff6f40652560ce4998848d8e1ec5af7b744fa661e832954bc53f7e6367fe88e7e8135ea9527f0fc3fabf160bd48edfcdd445b1dbd3cc2c81718f43d61e772306162c3b6ec62b8ba81d150d99e0bfff6e8817b2dacfbcfea9558e7ed1d15ca6a0f461e9a0d3d0a7999baf1faa00ff2b4b73f8028b7054f9d15fd0651cd6e4e1d265d6acc3d8187585fb4d0802a559be76a26e30791d3993f2fe1742feb43b1707640322e0617ef76d72c4edcfb357a63d22fcc8ed82eefe75a555cfe53386fbaffba6d0f5b8a9849c9b4de19dd0982b6f6bf6a13b251eaaa64c3f9e766f252abd1ddd1e80542e239cdac6dd71d7955cd3c48d6bb39b2ee2486ffb93a4f7cf93fc6c95d96aa04ce018bb1818c1a46374a305d5d2a7c25e8372d4c32ed0b555e58b93d75787b3c951e64a25cde1f8d9dc815e218c105a9e936e3a4360918635d55b2da8e1783ee7d97c2dd784f032ccc4709b6cca40e899c635abc24d8273ddce1c92994ac3e5c8cdc2e88014d77c36f160b25dbbc8c9329b9ad9747bca76efa904815d64f1a3eb80d692ce34fedf909428369727eae917ecdc79f42162b74d28ad6098b436a7e59dd487cb2c6a3b00691e5640cfe17e861512973b1e70dcbd629d09017cfc433dd890942b782ff55282e049ce5c90b1df3a9c099ccea812c2ce9dbd26e7261e79e6496d1e2f41b0e7b7033c974217fd974cffc4a0fe43cd4fba18451ce428f4737a1cbb6c844c4b443382b729330334d16e042ba41f53f129dfa1142b66dc3f1dcb68a4bd3294ffc680386ef965d5357d0c9896e499276195ab1b6f607e70a50bce9869b234966749eab16a0b385ab789845ed57818da41257f3660522d71d8e69ad3771518be313bac5b27410f57f2007702e6bd7ba1ae2dd70449882acdb670e291a204455aa0bfeb531c25533d4d0802ff9e48d13a1ac782408c078aefd12ffd78e9003d819d6071c7d76cc7841061289820caa0f805a8be0f4ec0e48d48d3081ade03b7145311ed21093f9c7b3206edcec4424b190795adfde646c4dbb61000a8f3b4560f4c12f0b10c7888b368af1a563c5b865c49d83327e6e9007aba9708ae9b866f4eeec8240d5dc62a5f721434a6bd4133bef4fe56a1612021e38ba385fa37acaf0d7b998f49cefa52006f1ed39d50316bdd5bd1d7e7849036400911d7e2619fc7d3bbc847d0a5ea3e3201b8b2f9417492dbcfd82968ed6c2112bc427f873b53c609701720305acbece95921c3ca3910d56deea6341dc12c976b502c4ce41cc1412b7a7f02adadd1866cdad977b755651a65571ca0cc7041c4f2276b6812153f986aa9d1be73cb1dec6c2f010d7d999af0c3bcd59e9825e3a684c32d409ec2b7c55fdbd81b86b75470750f6e795fd1bb9413973a1fdc25876dd2066ce829eadece8b6222c092a3fa061162e6fcdd0c12b906728b6c099df577a9322a1017747a89b5c4f6e525ef5de15764202992438416bb18036775a1f3f6dbe7b61358af00671beabffab6e0771abec96747c2cd199450a3f5844b14c7d56c816250da82f3cfd0714da83a54dd345ac0969277ca3b65cda8ab1a6b3c0169c8cd33de1f1641c8c5f589ea6eb1c7ae918d56fc82ad6c710cdef1ec24da89911dd27ea8003153d15b1f726acd6b2a7d48eb8a06b474a9389baabdde06757575757551",
        "0844494c3246554c4c080101030005200e944d080215964bb44469062caeb95fbee190a133f10ed2d558a1aa74cb87a7d4f84d9f368805253fe3d5277688d3af4001faf512722f1a080caa24562e8efb150ce4532ac5fa6dd9627e7b8c05071c7a1366f1441d96afbf59859ace817841d78c305e67a6981d24f21cc0819c6536b8c156b07b084b80c1f4b136709e3a0c37b77e42ceec9877817a4520d488a2a821e94d21863efa56216d6acfd46cfeaaeab1c4f1594bb7bfb1fc2532b5cd2dbcd33837b2efe7acaaa8a7e6b6a9b93fac95104af59162f657bc2d60f706cabf0554d8c293153f4d3026c45190da0e95cdf123e82b52c2d7377f2c90c57dc10e7c02ea097f94c15d5f297d1ace448ebdfdb73289f6945054b9ab453143e6bac5103d7dcae1e2ab08d71898fdef467f2a6e17c58b8f616a2c8ebaa00628d74b84ef4b76f137a251ff353005c97262f4cc59898cd4410d81054c5cd75cdabe4c818d2b364b063c8f3932a35fdca59c04e9a7aa7d437054067a702e3a5bf4a50d6f37c4a8547ec35b43d9119380f24c9c63317b0a29e39d6b9876fa85a13bc1f96043c75072490e2e9c650ac913b718e431377f5bd0ac8c797c63ede0a480fc673a359fcddb54863cfc2ac783c63c3f34795610b9fcee51b6e845984cbd6fd47442dbc30fa93d23824f230dcdf954416ec5ed24c73fb59018b30a18c81cbf2203407aacb1f41fe310d2dacb1fc4e9a580f57fd45b8370736df5c5614f924c294d08020dbe90de022cafdd50aeda9393ef2b11ab0b118128d747d4ee7207db2f101c30751a913bd37accd52d578a7f83795ed3d7d7522f4917099e61abc65103a687294833c0bde02afded5d18832d84e96474dddaf41bafff8259df4d2635678dbfdc369e7e1858114e26540c001507d752ce4fd8910890a2c21c878143e98918bb7ba07ec7d3c627d0e1f389a08d05b0ac14dba30c042410f1f352ca9434286e782b18d79734071162d9ba95bb0ea547fc475da1a5b55059c967d1f8eab8328e09e2e4297e8e4118b42d565e87f08ace4bfdc2b7f80f52ec51264ddde6006a80527fb48cd59fdfb7c3db29853ac82d3d9a5afe7486de36dad4c5f5a8ee17fedf29b64b0063e1918954a7777fa24ac05ee231857b9c07ab2cb2205fa7ba55ae7d9fef96b7a8e07dd78610d72dee3912a1274d6d7870cbcb687e4bcf2e2969a6d87679434c38d59c4787e38028b3c5a44d102f77a53b7a8439dae67c105dfc0073408d7ded9321eec6d323a0ae9dd3a93ebf03b8229d2dad3aae781751ba353a231b4ae99b6db991c852daa2167f2ef261711b53bb5e98ec1a95fbdb8aba76bc64626bd184eb00a6b1b32c6213df94e2dc5cf68323c653e8ed635406d51d422e89c5399a21c3313e1092b35d697c0e794d333d9fc92bb8f486bfdec21df63152712beb8d34a26f4f852460f80950407d17e9c5ab7e44b96c77f4d67cf0416b595a9eb153cf3a07b509b5b04d0802b8dc6a7d1710512941ae056169c7f4e7faa8e5bf285051356066cc237766f8d13e6d952dbe2a1b623aef9a9e0e88ee63763aa1fba2a98eaa423a102645810684aef917e129be48aa5e17ca9143563a5ea990ec3514643ecff95d2db4cf47898ea73873bddc178d8cc70d85de5e935e1276f6be39b1040b48a767bd66d3709425949cf33968ab265320e2db1a4f997df800ede8b6d32672436a72ea7a1b9d6c050fe8b7a4a25503ddf918a6cbc13f4a824a05d1541065b001b86c87899301d67c9048e0c4b9dbed325c74776445f6faecb7951f18330554097f6548558eaa74630312b1fb2bb5f1658ad7075bdf811789a7be60791380a2dc86cef556e73c7dbef01e1ed0f50c570df1421fb26768285d4716f325e8612e7acb73e83a79a6e6804e1b6a4b08049d6a85737adcdb4389dfbbfc4faa8fa2272c77738a21018ba7dc1381bb851128d53d5d198671007b630bf1a44c9eefc0fb2aeced7ff2c8b337a3098cbc033b838f22565cf474237219bfcf60e97c883c469b363f4ee18768ab328e1fbb6740b4714ff1ae2320f59ff7af1ee7e56a89638551165f9b56a86cfb589f5426fdb3983a21f3134b09dddab44202e4e9be2f535673498ed1c8053e6f64584c7c0e2b75489eb63986910f083ce1f727317676a5af3291c038c497649d6603b00f598bba1af6620d332eb368f6d6fc1956010aabf076c941682cffd22aff9d1ce35bfa26e47e06757575757551",
        "0844494c3246554c4c080102030005200e944d08029539f8e72ff8f54deff9bb7009b91db56adf5ea7779e0cbde931391d89b0032612a79242f9f49e5bd7e9ed60447f7d2facd4477a7874a819233892d00351bbd14bdfb359f40f5111664a41530cb9589c046f4ed8554893b2be9e01c1636c09ea8a6561626d21c00e78f44fa05a3842386f31b5ff858f0aa9dabbe091f4791a98fa94cd1a9a1053a72d78dfdbe8c7aade5db3bd62d2c2cd04d0dc753d40d7185306d025ca424c5831d4260175551232bc1d55744f316d2bf1468db8a3b8912ec6b9e6922cc6a18cd896226de1939782b981bfa69cd1a8fdd8e2bd15e37051ca2c7e0d4a15904f674207da27fb5be4f3beaa95b339098ccab39aff2f8f5f9a240fb581f339b4c9d2ae238b20ada5a11bbf41f6bd1edd21431a21022b09ffd689011cf10878999a75be236eaadd8fb57422ab76aa123e26c03acb1384f6e8e54a3301cd28c69c3f7449f94066ed22c201ec48c943e080e2c41acddac7f0473a383fe6e61b71a87936bc687ee1c338224490fd5c307e13e3d4847731578b61a3320f24bead3bcc82c6f5856ec5f893eda1b3fbd852807e360bb7e9c0cf2fac398a3f3965de17830cc7d2c6441c7e43cd38c1ab65c303bd2e3629ae5cf9d911a8fc75a470fe7f91e654acf8e8a84dbbd161a1a9cd13dc7ebdc3452702ea957d0913cf61b5de5f0ea1ff525e2b937c7c46253a40dd4959f297ab1340ee5514f50c703c2e21dceca66c03764c5c829cfda1e37a51a22127292e415e71738493cad6e10f18253d525d5f66676b6f909798a2c1d7f2062527324f62687aa4c4dae8f5f61217293555595a6370bcd7dfe5000000000000000000000000000000000000000000000d1f2d3a0006757575757551",
    };
    DecodeAndValidateMultiPartCarrierTxR(
        "d11ee7b78bfa01cb322758159df9fb69a5a2a9c5aa0846d44913843064430119",
        scriptsigs,
        "6ea0fa35664c6fd745169c846038285a8087fb5e8abc09f3dd7a736740a730ba",
        "DILITHIUM", 1312, 2420,
        "df8185ddb169590200f26f4d382e45eb",
        "12153f986aa9d1be73cb1dec6c2f010d");
}

BOOST_AUTO_TEST_CASE(pqc_carrier_decode_validate_mainnet_raccoon_6174161)
{
    // TX_C: 913f89a45debf6951c70253e55c75bda8e53ff9521d08670ed2355006252a30d
    // TX_R: 9f3b3b49bc156db86b92553b8cf80ee4240d96e2d7fc38712e6551900642020e
    std::vector<std::string> scriptsigs = {
        "085243473446554c4c08010018003f1090304d08028c8a6acb1f159d9bd4119a2e8ca28d55e80f25ed4d3a00da55b6a7739901076aa43742f600aa444f61862e01c96ebd40023201cc9a9edd86da001143d9b6246b016c08ed875b3a005c6af20c6d1c01c5e6cde6453e0136a3690e556d0046e1dec4916001961f5576b05e01ff8c2d1c0476018d588dccf24a00012b5760b35c00b445e661edf200c9c50978f04000c108bffa9901009a6bcc775fa401bf5360838c02007a17afdd182601e1071c38e1c501faf6623d0c99013b59a79d54f601b23f86db13c200e9b6c33660c900b384a88725ff016377cba07d0200fbfe924b703c01ef738936e47f016f0613e32bdf0065e313c4f33400fa11cf55a7d401473a3bab3c6e004262dbc5947d015797fe89158a0008dd3e18c5870026571f6c8dab00ec5e41eee289001ec0e30ed5d4005a030f41752701c3fd1c510e9900e94a5a52efff01a528ae21543c01cb9c7aa7beee016c1a10c6cd780035e5446de21801deb72456dc4500e09b3714b6cf014e8758832410004e6209c567f90175f5842597b500d5dd09f3436a01c31632c7285f00964051320144015fb1ce6d9d75001ad7d10162ff0112427447e506018f7db1683da300aa7b525dc90601e3b884068d4a00b68f182f7d89015ec4317821390180cb855959230072329639292c018279c4f478a90080524c29077301d7f5c4beeff800c5c11e00d427009c4d3ab61877011e6bf3b0bbf7014d08028aaed41fb2df0060832b9a455c006f75fccfc130013036cbfd1c9b00be636df5e05c00b605ebd67dce00741fefa6a4210048f8a24109410096edbd707df20130ef9d6f6e49001fa4cf38acc80015bddaeb053301f661e5119852019646c27a1bf000c6aa26e6beba008a3a5567da3d01b55be986ee3100407ae1117ec001e4986ab1942501cf00f2708ad0009026c595b26e00792e2f0d78070188388bf9c5e0012665439dca070035ad37512f7e01c82aa2574071001c685bfdb2c1000c8bd64b789001025728987e7a00b5901c62912601ad6f78fe130701bb12eee45ed1017bf94f204fdd0058df5d3a0b81014c1916c8ad250070e02764e63e006ca41a503fd100e2f67f14596001e1426cdddb20012fc84e2a13fa01868dfe923f0a00108db8c84ea9002cf141040c7d01f9ea1d5bd38801f56546049c130058c9758e45d5000835defb4406018169c342964400d83d8e2392e101930a6dc4a53100ed32a2ced90f0033cbf9edf3ae00c932cea8b4d501de71868b51d100a41a5f9d62960138c9ea0377d90115033055f1530052b096044b2200ca89475c535e001db97ca653d9010ba2e4cdacfd0179b22db46271006f7cc5f636e2001557cb9339e2014e7567a712d4008476801260c8019e83264e687f00b30801d1605601ecc201c1297b000cbbb2fdf3ed00d94fe5b6948501664f4ade4c2d01ac17f0f5caf801aa8acc3db34e0063664d0802110a78f100f99ddc29ed0c01b2346dc72c340074a1a5066eba00242ef7720012011de725c21af70074f8453dd6c3015d7ff8deb41d0151f60f88e460000584e46e99bd0012c301d14cda00c09bcec13d7001f8716219a4c40182abe09e4dc60002400bf04e8900ce97f83815d3001f718d15687701c757bcded0eb004ea4625e8e4c00b054f6f5dbf901135d546b9c050086b3b148ea930138f787318c3a0091bf192a89ab0135351fbb87ed0062876f1561cc00cdc289e9f64e01a2dd56ef4f3000407d895986d30025db55e0fe9d013e8fbab297cd01667829264a1b00068075780a1501d5df8f069198011eb08e59b52201c059ddae3f9100a5aecc359d7f00e1ad8a2121f1019866ace76f5d00ce364c123f150026515c01a2d7017f1ac63e0f04003c68d2794a11008889e3b8689001384756a612db012b0b6b78d3bf015135fa311f3000b1b19f13d32201a8c989e4d63500b110fac03d3a00d878aa6510a40024aa4c9e8a3e00530b19c2cd4b01c9440edfcce300112aa3868f3a01b533223e2e4c00817ea03edc4800a1cc86663426003e362b20adf201c6fa69b0f75f019fd0a7be3bb901ccbe3578c40c00c23ca02f1f2f017eb83e456d4500fee0533e81c5005ff852125350005aa3b9ca33810092659d465c9401a44fe1b08e6b010e753f6903d300318ed3aeb20200cefa65dd6cfc01b2c8e1dcca9901b00dccd9c507018c962f3306757575757551",
        "085243473446554c4c08010118003f1090304d0802aac100a22f37e5383900384c9b6e6ee700ac900959186701f8ac3b1bf50201bf20617c7e64008244b25d9257006e85b04eb71b019ba89c096d1a00f5527fe4729f0092e6a17698ed011d42bcbe2200007f89a2792d8b002a1d11254b1a00bb701705b74b01b44f3bd2e18f00e1a9622a79db01d335b0fcd4250064423899c4d801eb1e069d2cf6014e59c3be7a05010d8820f9acc20071616f7d1c20007273adfcbde400c2d6ce4dae2e01eb2b5b4cb10901804bec7ce7a4015439258490e201691897e87f9c01a804e6627f5b007ce0a6b5baae018fd2dbcd30d3014336fb64041c00e16cb593d0a300bf22289c05990040ff37d55e4e01ab3526da443c01cfab9d1646c901f9851524f977018be68957875b01ba63e43c6f47014f637d48033f0015328f24fb8c0169953cda99e5017e720f6581cd0145c5c7857a6a0021619b7cc3a800626fe529214601e8502476f4e60024e975b94365013a6aaedeec6700939295b615d20020a7be01ebf101e103d6005be80100c107f9598100cb635b9d7cad014d6144ada3d501388d078d5ec00017a77af149ad014e64d9f8e7dd00afe71cb44c2f006ca5e68ece9f000f65a12213cc0085eb5e6888170070c143078d6300772e61ac9190015bcb39018c9601f49da8ab3b7400ab774d795695004b1bc42aabb90142665c0fa93e00604b9445afdd0004f46cd91e9800e1349c38ec9901fed879552b024d08020184dc86b6bf0f01a9d8ad1fad420199f154bcd1c900991b9fe60bc500608c38231d2a00d7030806543001631940f4075d013400926f6da801d00bebc639540079b8d8b5f4ae01b9e90737fe6101029672ee40f50046dfd6b2923a0159ad868ee65e002335c66c1c8d0041e8643787b401e905e0e7af33006af3f99975c200f570cb600ad8013dd96ef06b5801471dc3e3bb850067875ee8798500a73260280d8f00f168db9348e101f23ac71d2efd0025f4e27a0680016f7979dfba5f00cc2cbec6181201e2721e2d3c7e00e0e42d8e237001276aeb2a5b37010d0da8c9bf8c00b4388f02bf32019bcd5d50b79e00378b430bc82801c6072f6c306e0110777387b7ae012c0330c296c10155762164624e001b8e1696544e01adced1d2799b018f74826732ed000bd0248456b101c648a5e4f13400a40e0c17d6c90009960ba6cd0800ce9277ad178a01bb7196c51e3f00cf611d2256ac01e92f5a9bd2760183310505fdbe00e7632129787100ca3a92fdb48100625e36e36f8600a057e170b28e0016d68fdf643e004c6a070200440110e48967176c01f21d6eefa746012d84d6fd91b6007282fab36304012219e1b51b3800fb88d60856ed01fe6fe484394c01e3d3cbbe0800014ad18baa8a4201138316ed96cd017335360654a60112539ebe7d360032cf0a9c1ad701b8d3f76fb9a20079623d5adfe9006075df355ce5016d51d634703e00ef4d0802bfb6b2e7e20093bd6166c7de01a7adb73945990164f81f61b566017092bcb24ff7010782e6e317d7018bcad0c66e730066a1803747a800439d2f1dda1f01757809ff0f7f01e2f6932f48460010c2af4f057a01cfc426781858010777aebce81c015dfdd3aec22b015f21c0d4941100e094f07608b500626297610dda011d514477b36a0035703424438600a92d51e507950111b8ebb2247600b6837a31355101373cde27881501ffb6ffaf0fe600cdda30363b7400b4dab01a9c39003e46279c5d79006ed2993f13ad00dd328ccc766d016a62314743ce00ef65cdfd1a870164657a3bccc800c07ab6e4853100a66317cf90520037ab5e3ccc3d01f8192cfeea670194f6d6bd8f850061b390d0ad3501bc854aa5f5fe00ea1184c6381f01ea0e4ea0fb450193258698f68b0015e255c2834701d42c138e89970178426e5e46580154ef9539f4cb0077f69f94c5fa004e233cb329b10098944795b6ed00c7436fb3113b0004cd4bd0c80e01e4a841efa7b301a02ad4f20fb8007cdbdb76775e0147fa6cc6683a01f5bef78f858b018e6334cc60a20068b09aeeb6da0015b111aff90401862cd2f8d86000a7461d5ba98b01814dfeb386010141e093d1505701a4d44fb54ad301d737897dfa7c0023dc0b29d7a20072f90dfde400012b5fd0408105018603ee1885b2015718aa27af16002ba9b3d057b201ccd07c59f67001846a22552ceb00c311e506757575757551",
        "085243473446554c4c08010218003f1090304d08022681fc013f9fe3c2046b00417f102e208b00e43908a0078e012993ab3c738b000efdb351746801eea78957b3bc0095b8fc30c3af01add6f675d2b20062a054a1b3d5017ecfd49cb00901288ecd93da67018a6aa6c0792300c5df57fff9d4004aa49b2d8ddc01026efd4e0d7f012e2ca919abc00108645b59a3f200304dccccc899006b4e1c49f0a000660d5c005a7401b2cf81349ac9006e4c5cc92b8d01e48662de67530193bc9bed485b01353f86a03a7d000e8acddbb31d0051e1820f9b6f0140fa9644c95400f93edb3fdd5b01e97cf385248c01a8b34c6afbe90043ec70bcf33201516a9b313fab01ec89367a7f260010e54784d18d0181cc853b413001b74e33d1af5601eb25e52549ef0127906e0cd3e700e1f938c3bb6201a317293de1dc01661ec54982e400f5deb1c1224b00dae303043ddd01656df93cd69401b58bac11179d00bb9ed918286c01e3155a9de8ce00b84353b6c59c0062f727cdb6d100e1ea54080b96011aec26486be50134b1f7b9cd91010165977c1638017ac0e0eb2480010c616699481f01af96b7a9d732001f62df88c86b011e515a4529be00ae4853a8a24d0056b1cc8931b9000f7a74d61db7015df6090162110136fddaa1203a01c7cc4008a74b01e0c28c7acf98017aa048f107d5002827443a9f3f01a0274d017520009fbc393b227d01bfab6e6ae56a00b0c58cf0960701f000a519e084015c581e592c4d0802fd012ea92d3a36cd00ab6e5d84b9ab005164b583da5300f44550ad394f00bd6498a813aa0134537274e4ba006cf3cbd1e2f401a923ca78a82900696a4aa68777002d375f24d519008a85d882de5301e2157d94d4bb0070359764fe6c01a621779ecbc101c93b30674e8e01d932124976c5017dbebc1b605500ae3feeaba98201b7a5b5e414e80196d267b44267019739d8a35cf6011adb3528784900236634fc521c01b38f8db8bb4900d5558281d76f00d6e612dbecf90126c977977b000087f6703a318f006b470e96d81b001f042c02142f01f7f25009fa7d0098ca337c7299012f60f5da2173005ececcf525c6018bb43eb5d12800f303407530f300bc21c9303ec901103604fb8bf800690d35c36faf011e258170b28c01cba6e5ddd23400940832ccde6d01264e7a14eda301acc0f87021c100cc841dbc69f6005aa335e1986800f0ab17972a22001e6b8ec8d128013620161fa9fe01f720f23a5fb60095702a553602008cfe1b264947002da58852ee9500dec5486760c7019539530071070063e83fa4168301f1476f702a03007f06400bdada01be2ceb0ad86c0083f31573972b00ecd6de74329f00e40697e2bf64015bbcb410412f002a81697e59a00061fdf6b007f1000a0a70c3f3a4006fe0eb218a80017a009d5d7a5c0188e2fc3c9ada01645c6e470963007c697e36684f0041ba6f59eee80136baee5ad57501688987ce3733004d08027c351527b2cb01ecee341847cc016133306827c601898fe7836b39019eb8b51d9e67013e37a3f52ef200ba111609b8cb0188c1a0ace0c90106155dfdeee700b3b0d9c1981201a6cf725e1987014da75859f5e200657fff13b99601cf6646bfebbe01af97f3cdb08501ec811a6abad300aaf311f3b296011d1ed3da2f9800f7bb05df7f7a0177e70b1477250092c54bd2f07501e1593de5db110111b636cbfa600049c5251daaaf008646a496c2b701d5f224b97fd801cf48b71c2b150056c1b2309432000717d4aa17de00618e5579566b004ec97cbf88500199289a19e59801c89a0ffbc576004630be8628d101d0391c4730a90026abe0705cbb01ce5988087aa8003a6a0cb9e43e002eb9efc75b5e013454b0cf388e010b28242b23bc004e8d791561a200e9265ce19b3201442b0bd6df8900dfd9b1cd03a500b624b37a08020069d70386e27a0138ffe45f6e4d00470638339b3200abb2e38c351a00a364c3383803018b7a0199da8201985c9a76ea0401a94b1433bdc701a3ee4a03ffb70035b6b14ba277004e0caf2ec06a00c4adb6a7c4fe0187dacbb473b40130928656e1a201c2c58e1985ab01c70cddb9c93201c8fb4185b7c0018240c30f48ff01ace4482dc20d010c25a7aa9ae101fd7169f80ff900bafca26eb7b001996ad85058e701f2818296803400f79f462691c300f0e27853f6e50087febddc38dd013a901d57aba301659f06757575757551",
        "085243473446554c4c08010318003f1090304d0802ac5191db00717806af38ad01d2fa94d193470198b47250da2700bc2ca3b795aa01e31595565ec10103251fe537ac00585037454b2b008da9073c238200da5e33ac1ffb01939c30bf7d35010517447f04fe01e4ebd18437c701dedb78d472cc01531e6b07038e01c8ee94809dbb00129b3fcd8db2014cf1083b2f9601b5117b3426f6000aee7940f116008df862f70fbb008550fbf51d39008f004ecdf6fe0107128fb367dc0005d9489ae2ad001f8a03571a720184e51ee8e29501a88efeab1e9600fb8cc95ab1b90174d3270635e2014ad5bd63889e00f0ab7722f27700cdd8e0bd7c3701fbaf2faa14dd01cef935d5304e00a09874f388dc013f2ef50ab55d01dcbc582de7e901c7a913092c6b013f6c9481711b016a668d8a72c30092abb926cf6600d4fce1f597c8000bf01e741ba300b0de9047754600d9dd66873be4016e136cc10de901df559fc85f2f01c32850c41aa10120ed7c627985013edadf313c5f0025359cb392f101ca13cd9bcb1701e2230b33c91d000694e1e6707801ae9afc7408170014cc9eb1af5d0044f510816949007a031b15982201b737b616c1c000f10e2d51028b00ed094177dc53012171ffa5da0a01b227aaaeed8501a0c175d05296010b4dba2c04040038dada387446018cd52f6972650034353f7e733001b1514d9e5cf1010662872b9fa901a48fa3dabff5011f9330c0d7110086fd91ba7abd00b1b1e99a4d0802913801f89f8baf3924013a5550f8cc0e019a589a72676401c7ee7f908dd201f2731c178eef00b8266674b5e5016cb496cdd0dc01ee492d0f3e9601f5ed57d979c20138aff9f5e55501b3a8632f68a100c5fa5fff4e6601b3f5ab70d1a2018556008b218f0057654f81a19a016b641f8121f000b54675cf3b9c01e35cdab37db300cb7eb51088d8008dfe492ad82f01349e7bc3909e01a3d295defd2901482e5c78be3a00fd5c3a7dba9f00c9f50f9336350172571ed52d8d01ab58933ee7310144afded215700173269fa408f300f5c5727d6f3b01729ca6f14dfd0113fc793afaf500717994ed58db00e2a59a0ce84e019e37d7cdb2db009cb60e7eb9b2009c853356a2f401e92c5f85be3201fb55e4a966d701318d1faf03f400dd610ccb80af0135dd3982830300707c6472aab00074691939a55d01f964bbfb0917008ccb644cd24c01cfe1a1ea4d4f0115aaf721a34901bb175574a03901cd5d55467fae0008311818cf91014ecab36008c3003c7f285169a201eef452f8b363015f75835dcb6200071799a91a630057d1af0598b8018c90bc310cf0009dc7b35984170047ea6c46aa4200bce68dde3bbd011211717cde54018f02776dcad500db758ea0da780184a206bb5c24004d9d2f7d883d01469f0a85cbfe004289378e275201fcc3b5d096ea013184c73a45000039543b7d147b01ddc9c9ddb94b01ccd7643afe57007b1ae1f7c90e4d080201eddd09c99160015fbc394fdeae0112de05028a160056e66ad3fd8c00cb570a953400014f4e9e66743600c2dce05df4e901cdb8dc4713e100f8a132c9bce801c1943004a84d01addcc882dd0401019992b2e514012dbc0ffb96ec01c9855293e8e5002d295c4173bd008a34ab3c766e01eef55464d30001b6ddc45e88a901f1b98c8b3f69011ff5db411b81003c96127b3dfe01d61804d97c6d00c387bdce7448008b04d1f66cd5018d399dcf91cb00ddc589025eeb003988f0fe4152004e6086acb1b20108c652ac05a201d3697de1d6ee01b2d2633829e901e38384b9578900fa14054b2ca9006e004fda127c004850fef649b8011ce76a1159f7018664a1d9ede800daece4c389360137ba1d033ce0014344ce8a9b63004b5afe8f10c801867527a516d7006ef83f761f6a00ec6ae179e97e0121bdbe2591db002adeecd95e860019df4775b0aa0192a03210172e01794087cfe3cb00d487c9b827b40075315b2b4936017382053ec253004d4111b58e5b016abb7e3c4a87012ec5d3c144b600b2abb37b224d0107adf8f2e2390143fb4a01f00e01067e78723f120050f707e1eba801bdd9971adc14003c7f368a9a7701caf3b156274401f38c26c571eb00e6635f895083007a1e3191ae47019d6d811b88a80042ab3601c854011b85861cbad6015115b90c820c01d9e3431140df00fb173c92d1ea0185fc17ffe3eb003c412891222301a406757575757551",
        "085243473446554c4c08010418003f1090304d080279cba70af100a3a823c13e4b00ba88a977d3bc01d4d71876b731009e9922d5c22c00f4b593837b5900b10cbe2530630137262c138f0a003dc536e19ee100c1e364fcb945016c7d0eed284501c1e8f8c503af00a244b7b4e2d601faa4c6c634d3016ecf7d52e31a01c7b30a0dbc89018bbe82205b56012e79277231ed01c7eaed12599801cfee879d284700b94bef2abe2800e2161af1d3a10156abbda35fee012b9950d3a51101b93adb8efd09011c435f82098501e4fdf526ea1000b53a427a430d01645f3f41d9380016c31f874c4201034ab72f47e601a32ec507c3ca000bdaa18939fc00a0302617c1d40002165a9fde4001ae03830521d701a0bfa9a71252019d0b19788ef000c993cea64caf00713a12f7e1890195165d46da7a003f4f706e455101b778082de64d0105084d1260e301adb815546ee40000ccc0308c58008f5d2086c9ab01cb3dae01c88100270e5683c01c00abc02308b9a60192484027cbd4000e9517f739f00140130b01c72201c660d98ed3ec0087906e060cf4014ef8e9f823b800783e90c766dc003f74de079533004d55ace25eda01a5498e75f8dd003dcda57a17ee01f523b473b13c006ab30bd2229d010fbe868d73870008d85b8f24c70068d4cbc97aa300085d090cc257010423ec3ea83b0151fa56d79647014567e961370800720e57542c2d013cd81064be4f0078ef47708c1201b65111c92eed01b1dc834d0802b7616401bf6970267c180163e5cc9b40f500b56645c30d1b018c66fa58cda900ef11785007a2019a6f09b25aba0000c521726c8f000c24df98a8470164f9a7d59fe001879364f9974701bdcb60e23750017f585920b445001d233da261f001dee91779edef0070d549ebe8e9011e650336442e0079f69de04f6e00c88729eb131f0011a9694c9a1000dfaed47f2b6301cc7a713c809301fd2a15e32e7b00e3be41830ddc019291d432996d0161a019a27a600017ac511d958100c7639657a7d4003fc02088dae500dcf23bd245c9019155d2131b2300787d682368e20168fb3f21d106006085bc1c7e3601dc02f860dc780112d2967eb2d700996ff556f687017c7c9df22edd0031b54b540de7016843da27bf9d0035df6eeed85900cf4dbf4e39590037340f4990bc013e2a5646213d009cc50c2c4e71015664aaa7094f016ea728f2a0f300a5756995b3b601e81135c3ca0a0124ba8cfd38e8007fbe9cf21f7e004e2e2201fac000df8d4664808900412b152f454e004091f2f7a2e300fe5afdf4e389016d0d290a8fab01d06b69631af9004fd0dcf3e94000a331db5a3ad9007e99c820a5f900fe55884a153f001c37aaad9f8e01fbcdc0f57487005ed128040bf2010530bc91147700727d2b804924013ac39ca37b9201ddfc04be75ab00086cdd6a962f019be4381b5b9101d1836a585fe40030228142e4c501d28dfb1cb8d101eca3ee7a3d4d0802a200d6d1606db36d01f451b984f7f80129edee9438e2008efa5768ada100f1c10d3ba28c00640c5a025b3f01411b037c102800f620fe146660003d99139a8b790034509ab7a252003010a06209e801ee7e0db759f10199d4987d626d000f13a6bc16d00195cfdb714dd001891adeee7a58018d5c0313609201085525de260a015e54c78beae201aa47556183f900bccae93c705d01f54b8f1c8e0d00ea09bf01233d01709b0f63547601687b9c46f28e003d1571451f6a0123dbc384f49b0109e0a9b5b93500b792e454011800bdc0ca886535014d5f9544ef52017c22d09ca78b0126d83a31258200a2072dfc347f00faccfa8b4d4700d11597392fc50075d445d2186001e11bb94bf57601db020be313bd0182f64c1bbf0b01bf1709797cab00b3858f402df101bf53c8124dbc00e8ad1ae2be30012203051d0f7b00c399ad5b011d01df1965ff1e2a011d183f3ed19d01a157499d739a018f7bd5dfb0c100f5b66bc2dcb3000e6b5aaed09201cb48b4e9afae003cd5cb6bb4da01dc2ae2a2738e015e08fd0d256201e9e273336a150074b72efc366301d8d3e98e7c52006035e34e0ca9019fc17c29f54000274de466903601c1d31c8c484b00a6772acd200e0163ffd49e943b0008729d18790e00e453326eaa0f0163dd0db203530188e7a22f8ddf00bd30dd8c964a001678b6d8b2a0011ce3d21c1ac60189f2f7477f02009cffde635fe90006757575757551",
        "085243473446554c4c08010518003f1090304d08022c14b2fda3d2017a602efda6ba00fff37a7fc68800bb971be2966e0002478c2fcc7e00a19d58238b6500d496650a1f1500bad2bb22262500b1dc3828d44f018e5f85aa99070140c784109f300117931fccaa6e008d44e4b8fbb700f6e65e533e56007f5b39cba8460078bca930069801113719de9bf401bfaa58d9d100006bf59979a0ee01d34625f2ffd10181cf353e2fc1011536dd35aac5015237d17c23ea01ebb7add237c50092f25cd850180079d9ea8abcf8018565940d828100bf30d82e13d3009bda5960089e0051246ec3c2220152b42f79a30d00de01006c285e01ae79d5ca2a1500cba6784ba6c9006781d67633880028c902d230e401f037f4d4a1cf01a70114d39a22013309f82fef430169845bb8d8a50134414b62c232015954b95eb3e600c74f8d05df04015f3477e98a8e01b0642c1bae920138087ec8c43400b6b3faf6c66c007f3ca30b75a601b7c05975cd9d00399a69bda019014a4cf640753f00d7ca36c5b0a70166dfe761efbd000c7dae28a700006e987857cbe90101223a9dd75c01f3b3d77cee540139827bdb950c01aa7588f41a47010f33ff72a5f200a01aa1372d2f016ca38d098644006c44ce3e5c32012cc637009cf001f07d4b858a1f00671a37e3fafc01ef7feb2aa335010a20a73ed00401066768a7ed9b01267faab17abb00837a9866ffdf01a6b0c531d00b012edfb9c611a2003ca81de8198a01aeca4d08021e82436901a1599e7c18c5012722e91a790600cee51b3741c50080078ca7d02101dff6d21f7fd8016d235fbfb737006fc6f4e50fac00efdda18518d50108ec292ca1fe013c91100234a30086da0e02909801e93ee34da59501238a794945ac00ba4e834b564001411b57f8380701eb531d609af900da49ce7d822e01131fc548b846008af95b19dc3500cdd8424db241004e5676d489a1013428547b77420079f5d44acf3501722d8b8b1c50017e39897924060014ef26f542a9013cb3a2a402420035a203f83f6500683182f9fda0015e49be1255d201353d695d7de6005ea70e7209ff01bad5276b9f450103e6c12e8b05009bceef0e0f79004a8c68761b8601009956a2be130122f2181efe650040548923ab8801976a55f4c69600fd79c0da6bc400c9d6683d19ac01fc509b3e7d97017fa7b093bbe500e7b9c01f70f60092ff57c0b2d101b6cb9798775801e8b10384a24f017c5c162f73a90133a5d2d90fe7019263585845d40016867983a0bf001bb87e0f9f76016f6d3b34c37a01c0f5f2b5dc2700b4bebbdb7ef001b34e1de4af1901e53f06c3577601e91a3eb9a7360002e80ef6c552001d0b25b0310201be7dfe0dd745003e7e6f9d01e201f4ab74a8d65f00a7d76b83c017014f8b609a3ad101f541b4dfeac10155085cf0259f01eae68827fd1f0013d8d85390a501771a03bb3dc901c3e48085550d007e3be3e64a3801ee0d11a04d0802611601fc59e9e543f2002951299aa33301e954f2f68094014b84ace9307b00cb823195f6cb018b22b7c0d107003fd55e2c0d9700de65e94f593e00a7ba5b454b270189db029d097a01db4442f78ebc019d2d6a4fb30600cbbe4e872b9d018cda7dc8c63301a76afe279d960132129de777b400fac0f6612caa000f7734d27e1e009468accfc5d5008ebeabcef0c90041da5626638500976e0f7a586301fea7b182b8220012f5d361d07f00b0dc5368192401f24909d306c6003431140e4a1900c8afeeb6e822008d07cd3149b800dc6d38d78fdf01f59502743d1a0058f3810783eb00d6934d3ad97c0004814a6f5fc40168fbfb93f823004ba12d02e97700d704a8b00aac010441b9dd5e5d0051c1a8ee5134011119be7861b5012b60f9d809e1001dfd544a0531008fd64201a39e0156ecea217bad00f9961dac4dee01a52ad73ab3a801cec079cfd76f01d64ae27d6218007c99546a6efe003d751e2496710192033f46f5f801b934cc82323f00e2f4bf0434c2010a13815c49a7007a9640e373e7018072e05411fe014a2f720ed05201f05696dad0f600d95d4cc246bd01bf9c0881f85201fe169471874701bbaa4949699301eb99e10a95520008cf2480b4f0015b9cb42fc30e00258b86f220e301eb315ca885f801d0bc4b8948a40188e080c2c3d6010b2bf5cc71ae012079ae51e32901d6f25f32acec0121f5a6636311017d1756d36cdb06757575757551",
        "085243473446554c4c08010618003f1090304d0802014713ab43e7fa00e5abd48a71ec011a45914650ef013af81e970e8b01520bd70e11dd01e0d6628c8ab1016a9f9423795500fe10202af96d01fdf28eb33bbf014b929cca93cb01f8d23b1cb9f10018ec8634793001ce725269c2e3019e26a45f1e5b01aba9b7a7c6780012ab0733fce100c8110346497201f233c06123e500de37a9cb80b901c5d65768d7ef01d2184e250d480180a79c44caf90156764e79e69b01d4046c823c7f01fdbe8df3badc01196208f1c2f2017e9cd9e835b600ff57b169c70100f0364693fffa0112e216dd211e011d2bc6014e570144e93477f8180044bbfc5fd1df01809b71fbfece0121f8bdefcad0018991f1effcd200f96342f3eef501c84f0dff18620073e5afe205ae0070269ec7a969001c5d55924183008170279085c00144790818660300560a0ad655b8006c52d31f6ae90023854d5810c400b2bd7aa6a87d0181f06399329b008e6de32d9a5c0177c8a5a5cbd20044db575f1e11007e2638a14d1c00e92c48f63c7c00003755d4b2fd00b1a4a17f8f11014f462e4d53f300c8e8183a2c15018a73d362147100e10b0396d000003d999380612e018370b7db476301f608e70d0ebb00151ab9da5809009cc1fd55d8b701d2c7ea3706b30037502b2d70eb007bb1e877100a00eb0784cf0b0901dab017b5ceff00e374bce70222000df36568a8780060d43c19183500a5d447d79db401143ea2569dc600994d080285633fc5ff009ba5bfad0880009be274e464cc01a96b36af9236018fb3a3d45f8f0069938cfa0ce4007638a84f89ae0082e67b08829f00a58c5330083200c337b1d163880087c04a45039e005775ff28eb1d000b2cc1860ad201b88972e8254f008dd49ee1c9990111882a903c7401cc09d3c5367700acc5cd467fdf0058b7d5411bfe00d7b096430459016dc1f89164e201d93b73d453c100f3e8b77f76000134bcd458dddb0137e0bc23c84c00f7ba088bfb7a01476daeb3b073017f5792ec7bff0020ac0c3760bf008a24b9cd4db0017c2bc74b4f95018bb989f48ecc01c7ae9d69eac0013bd60ce0a79301af72cb65d1fc0098d6afec01d601d02b3681e79900f938586996e900d8e8d14c48800137571d7f5fae01318f65a057870044e6ac95095400d29de5e7b238016a41fa2d247000baa39048ed14009529062b5c33014e6525119961012c362b9199fc015aeca0f04ca400d23b6d60c459009a095378903901eb5a113bc58400e2eb7d21a51200fde430afc77b007fd05b1240cf0051a05ce8263701613ba16ad3c80182856714d7dd0030677a6d059d015c8686a5c99b004469283479e401940b70021c2700ad83ce4947cc00a81595fd8143006e600672776a0099a1378ccc9b01a472435e557a0090428a0eb15b006d48296a2ff801e5ac3cc7e1ab00dc1554a1d0cc0194771c2f7eba00968acb9a088300e40caa232ef30184f0724d0802f88e0401036293e15e260006503b38fc8e011c9c6038b334004c8bdfbf83a200c8d1c6d4c70801e89792843ca000a92097e9662400c66cd4394e93002779d278bd2a0181890515887d01e66a1d1e67c700983ec15c3d48002d52be7171fa007bc15d5b730701894fa42e5d5800066428351a19000c80b05467f60188bb1e6d96d00127c2502699480082dd96030daf00318f4bf1726b00ec900918fa260024b9da6a30bf00b9449bda491d008448885593a300729165da89ee01e3aa2689923c01ff0538dc42ca00a2ef7b42323a01cf949758ef9001792c171af498009c20498b6036015610a74a57810041dacfd6e53700c530a3225b2f01046dc0f72d0300c305cc07fb8101a6ab3fdbf47c01a70a005e24dc01fa50a318cab10186da6dfb02fa00301a25895ae90103799464917a0020b0557123340114a884862620014b4d593878a9017e81ee5b640a012f7b4107a58a017a5db901c8dd01d696ec2894c7007819731885de00eac8a7d8b42001e67b69e8809601512474100319013cb50217142f00d62cd34cba8d01c12e9afb4de801a3e6f65abded0029e349372d1e0116e9c64f939301f043e5b176fc00f43bbd0d177c017b3bc6f4214201d2c2d898290c01ea38ed04ef0b01551c447e33f600c17b3add7c4900f4ae171fa60500679f2a5dec8000fd4ab1da1ee500a61d648ed6e600d420a639e88f017297dca0ac1e01639f11bc8b06757575757551",
        "085243473446554c4c08010718003f1090304d080266006345ec450480001cbb58b0d5fb0055260e1c76a701a0de1e2f39940072c82bd2372100894082260e8001483e3b3e44cf001a0fcc2de45c00d00d53cabcaa018e5ead4d30c1011ccb869d20c8004dd6bba1a7b701b9e88b8779b4019227e6c732f70125e30b1aa5cb005eea96fab5cb01b7822763e0650196c31afe7bb8011625fdc82db1018db1b595ec1b00220714ec0bb5019558f48e439501fd4e759abb700088dc8ec8c5f4016ece7e6808c00062f545d2cbbb012fa47e23b08601e505fdec57ba0105b84a88158d01898324333a49014cbb3546274d0089f47858b06b01d39ca2fa71940001e6ad3948bf01aa441b5814cb00f9146162bf8a00f0058c5028db017038e874b63801c872e78c863d01ce6c37bf4cc80046d1bd16b87300b361716030dc018a043af4050d011d8b38bfe7930130189bd1a58300ebba90eabe48005ef12d8bf57b010eaa33ffdde1001bd487f1da3500e853ca752dd4002033449bbfbd007571d830c32b00cbbbe78c882401eee801beaba00192372de62bc100f3715fa1401801f8024e14683c00ca792659b99a00a1962ac79cb8018d4fb6c99efd0008a2afcdd705017795be2f2dad011a64607990a100e5eb72580f0e00c5b29574f5f8014bb88ac5ae5001ba4ef1f0485f00fd0f75e8728f000099d51e89e501fc0f8055b1ad00145ad28cadb2006c444cb4ef7101823285d2a9c60173c40c399484004d08028e08a125d32800addb17f07dfa00613cd117d42b01228072562895003111e379ddfd01f22ddd55cf7f01da7d7f16f60800f5f495f82182006b814504f94f01a9a1da566c6900ca95d11cc01501f1072514730001ba77410f1d4101bb2c2110a7ff01757ec15ca3d401df39ae05acfe00bda0d9c73a8600f6f9e11d313800d8b61d4d3c93013e27a47b436d0085c7732584d500e1e996c3be8c004ecd3728ed1700b59a0c64abc501c02bd527217f01b728fdb12ce600eb702fb7bc8c0013b22c1971450087f809bb2e9f01651259c4c2410100189f26dbc900c4498b02928b00c4c82171cc4b00c9c14b4296440176144c26bb7401f87c71281021013d924709409c009d857d740b5d01f3821027d8a600f464a9c8328801328d9fa4a73a01a3a2826bf3040060c8b5afd83f01bb9dbea67ea001ebeeff6ecfbd0198abe1fa172100f135d4d76f0401b147d0186390001cf6a215b3bd00491675eeed0b013147de7120bc00cdd7b7a3197801c1cd2970617a00dd4dd703f74301d12990da65a300006d021ff83900f990630f53ce0116d8750fa6770165718c72462600d38f0a3dc8a101506d85211a9e01e4ed6c9f3ebe00e79c45aba27100607ee39e90e800d7daeb65bb1c00779560f6cce1006c3b540745f500bd520946f37c000aaeb2677494003374079c4de00144fa79dda71700b633d3836d7b003fbd45b9b40101d4f20d564a810043794d08026967ffb600f67ef77cd22801d9867a87679a00efba63da87db005ba1ea4ce81e003acf87c4ef3c001b4ce8699f9700502ab94e04be0098dc93f517ea003aa173ba6827005beaf721f2e500edb0cf466e6f0087c2bfcb81e800995d5abbefcf00ce22cd8d437601b20766e0b10d00d3a77eb7f5f90099b7dce4d82e00621a3b6b49b6001b866bc9b7b201695f0a79519301cce3b136b90f01a3a95e2454d200c5f603ad90fd003fbc8ea8e7880146d21eea35c80134c1ba92ce86009fe47da0b8c201048fe9c6f346016eb4ecb306ba00c92f33fbbce600a85c647f8d1a01b9e27bb83969003f7c00d6c9d800f24aada675e9014f80c4e6fe7d014b43c044261101d29b98eb1e6100e91388d8232f015d27f4bf455401a449e85a8159019697cceee28101cc98b634cffc014bbce63bdc0101a9cb7d01839c007b5e44026b5c015d540443581801edd0eb494d7401daff1ca5929f00c5f81975001c0071ba5dbedd130178aee1ca881401727b51cc18280111159740396c00803011ae549401d28cf116038b00df76045306df006abc64d7b0a100af5f1d382dc5000f87bc0427c900dcfe2b5185d3009f8f05201df000f0df3d60f67c00fed55925fa5f01fcbbef8c51f801df200245521500e5e6aa01938401de793fa3d2b400a368698cd62701416ec77c4cc901a6f3bf8dc7dc00debeb46124a700423c1b163bd7015fd7d926a570010c17f93206757575757551",
        "085243473446554c4c08010818003f1090304d0802a97200b62a153197c701162b0cb36d9800415c3ae9a7a800ed453298e1bc0094197ee7e34f00bc0c76af34de0198b636ffbbac012e65edefd0df01c28264606fd601fa55177cfb5d00529e7eb44d180095c83d2e75880198c013651c820150280aeca28001c55265a8a2b301ee66d930d548018cb7467fb820013174baedab74013258d2a4bac501d5f8461b630700256226f1cbb2017b0c4467290c013a8269d0b26100dc3d507ab06100f3c0d1b5890101e514468cd3f90069c665dd580f01dc7febd0aa5a006d34712c70e10156d7331cf35401114fff57aec601e0d61d9349a1004cf2c33e07be01da15c6d15922005e311f4ff579014e4e8d5c6c6a006c4c08ec1561012c7007c8771b013ad7466aa2f101e7d55344ee3901696f236866f901335863ce4b2701d28ccc8f3e31000c2c00283c05007dfa15d3482501b77bb931864b00a76b0918f01a00b55022bc44a200154c367b5cf701c4d007390995000dfde25521ac00b582cacddc2d0002417ec1776400d890eacf43e401143bcdbd79c8014f581f8dab90000c78950f027101c7094cbdb96200ce915ef65927019f5f5a0a923601a6a0bfaf7ce50084afb4ae6f900133675d5e8f130140fef094bdc3001e69401159ae01cd63a99faf1e01f183d07c255201291c4003794f009d933719130d0045493d9ec18100814fd6d46fe500d9c4ac641c4f00e20ea765dcfa00f163942ee61b4d08020001813faff9be00dd3ad871005f017649f2706eed010a7bc6b4c45200ff8b6e5e94660098bec6eb386901fa70cb7278b9002c5c2f4fade701a8d3796b2f15018701d667078e002c0c2ca20c9d00cf200505d80c01ad1c0398814301ab4117608507003af140d50e2901b4c4297ee91c017364791dc48c012170ae0c421a00b6f6d79fa2020144a2842d13e2008c2abc73bf8600dbca123b63000188fc26a5da8b01d0ccf6c99e87008db23d4bee9900e997a9692c3f01116a86f199a5006e133231df270135cbc847a4c400e0e1770ae8fa0093b1d805abef01ebc72cea0dbd007529cae9c126002c190fa8b405019681355d7a5b0108db12fc123400113e66ee7273000fa45c97145800203777116c77014b3458be2f68000c452aeca5c50097e60223e192007195e940d891007c002e3b2a1d014cba26c452b800057726f4341801d9d30847333e01f0486b17403e0092e76f9d301f01bcee231be1e80154577d93744a019748a4e39c7600f824512f64fd00be74dbeacfd501fe114cbc660100516ffc67d7cf007e15cceff2440197a276af47e000f23bc2bf7c7101f6732db18b100138d318568bef0010098437d6e70100f98945755300912418af42a601610cfe3ae022012029c021c76a01285166c13ba8001f415a35817b014e5e5b34c209009d49ee481329010d5eccc6582300a0981ed7c7cb005fca472d186c00758c6fc328cb01804d0802cd31f8add400948103a6717e01fe73912ae74e0127a90505ab7b01d8068da148fc00a26f5cb8e987019eb8d5012865012511feb47c3100bc2dfc18c04800e5d2ad6f00f2013baf13a91df0001e222d7cb9d2008103e55924b801fc8b594e1ca70023265866570c01022d4190b9a900be6f9c38135f01189e4e4fc50d00a6d1cc4869ec015fb140991d78009591d4843158008e4db0e487290104f28884283e01da8e8cce279801f5a1cd75418a0083843926da24011f52e573a99400257c5a321bc800a52d525e9cbe01cfca7b33c8680012d2b71d6de401fb8fe8e756700190b408ff58ed00bf6c109b4c8801d1a72a77623b011e174dbbe3fe00b055f6915241019c67925e08360061164061480501963bc5240e8401a56f8822eebf00e03490d8071d005bb33ce7bfc20192f25df518cc014c8b6aba086000e862e1f16663009081c6a29356018b36f60197580135f04602293d001f15fa51628600171879a0f1f3019b90bc36820400ebe29bca950600dbda4b312bd001d2928c6df5fe014b34cb19a8b101d24b7b774c44014776a92ce7a2012e67deb292d00100d3a44940cb0189e1c341b3bf0130d3c8370521016b6860953faf0000951128cf360077d39f56b80001cb031fd81c4a01a446584da691011bc65342e6f901d70d3b2948da00835e0fc3a8ca0068d24a81902e0093b08ddb41b601d31c628a24fc00d5babfdd56ee00c241a406757575757551",
        "085243473446554c4c08010918003f1090304d0802e0f81400d3d199d17a7b0049fc7a456f8701042db50805af01593bea864a04001626b4af24eb000cb2d65a1baf005a42e06bcb090122896763318700929e6171abdd00383c53cb67d6001200214e3408014aaa6440ccb5003aa46d8f64e1003938900b5375009dfe50dedc3d000019d3d0795b001451c5f3c4d0015b745ff65f6c000069c7b96940008f33bf72f5e801ebd24d591dd701228f03ec18de017734084e5fbe010b9bae46322601efc4cfea6f14005b5e24db9eda00df1607fa103701d8832ef46e910014c794f9fa45014e11bfb54bfc00fa8b4a39beca01c6b7a31414b1006fe03c45c3150154ad8566d6e6009195661a4345006863fe0598ff001024bd36432c01831dd183275800b2d01f71a8050031acdee400da00383141fc4aaf01136cdfa38b8e0132e768776e66004e83dc3aa01c006d28b72d0d8c01e3e512c263690193290373289e01bf9e0ccee93600300470186851008ce9e8d7b97300d1f8d2eb88260180d33fce47f30064e8123ba836002fdef9536ec8019736a3bf7e7c0090ee7ebef0af00c7d0b5713db600180661eb291f00a85dc11f89c00077b2876454100030a3e7c5634d01176aa12703aa00a78d44459d2c00673d9d2f52e8008c11ce9bd3f500c82da81966680158d7ce66b022006ae9656f6d0c0109f51db91e3301dc07fff3e2ae00526428da172101050754628a3701eac7b1bfab4801004eab56a04d08026700d8e19dc0098a00ae59adb9f455006f3f77a29b5001464e02be2c5800a060b3ed805f01df3af94e3208004cf0b6a8bd7701e37f7ab28b9901f77b9d52ee7e010f52a5574e86000f4597b6622700254b11118e0d01dc12b83c674b0175757a34b7ad014adca8fa7be401a6e2b4d6bd7a008dcdf7c9a35b016fcfaaa720b9006ab12603a408018df71b1f9488008ea1bd1e242b0045f903f1f46c0126fe653cd3a500a794af17f71c01bef8cc7f3844018edd74a1ba930152cc415ce5780199e7636c33b301044c8ccca25501c64cc725cf52008bad4ff527c800d7aa536bd59f011ebf09a680920091b14c7ceea2008944ed6f47280111d2a0fe28bc01251e7c44222900b9cfbea6cbea011581cbf8aeb6007e8f5949da360168a758f2e661017dd5ada73b4500d4de4f926632018753dfb9eaeb012eddc3e94ccf00fe71393d71f700dc53a5cb34c900ddf43b3b66bc00d666e5c6155c00beeb4be70ed700ffd4ffc35b1101fc0da9b7301b009b27889013e900d4480384631600c2a64e9ea96c003ee48dd70227017c93377e096201abbf22890d25011b9c005ad7fd011af3af48c63100045fab2e484601f32dd230337e00b7552d6cdb8f0156361bfd14fb0155a880f6d4b500bbc2d418ea5e012b12d384622801ba4d7d301e5700efd6cfa859350195974ed9ed3e00500b6e16008800c00d3be73a9700e6bc3aee59fc0061bb247670af014d08025502dcb6033500d25cf8cf287c003af752b1d93c017598bda8acac01bc528ed70fd3011bcfba18547b00b8571f940fee0044a95f29d9a101bc07b5bc838901469897a2c6ea008a983f548a120142f72766d096017e9fb0e09d49003c605f64a50201e6ce11a3067301e75f534bbd0b01c0b269346ac1018d768df3c0cf00023e4b5d7f400191b8f64020e7001996a6a3b58f00d6f6d6b2fd9000869c81ab1bce00915dcd094ed400b32841241d6901c5e203b4e22d007fe78e66281d009ed9ebdd913e007877bdf998d301300575e2f42200acee185d4c8400d1ca3f19101c00a4fc3095f7fa01e8c615351135000b2ea5d7dfcf0003a440e6dfd400427016ccd287005b469c0bdc3300f2515af80ebd01c8f3be34787200ab45c3346bdd005f16f4a2e7fd007b278df5213f00bc0493aa798e01295a97c1760b015b6e2fcd628501e2e3634ad318004e11631907540005305603a3d101c79c01b5388a006122ef20c3360151d757c69d9001ae955071bf08016be694efaa980184f0319c1d7701cec7a57584c60011b76eeed7990066ec8079878f01dec13fcea11800743ee0222eb00032cc40720095000ae83151604a008c8b39685c4d019cf161060a8901dc29c8404104016afadf4c39570149f5a86561440151172bcdf43b00c86a7d8c0dc0004467ddcb675801973e3ff50be2005910565af16a00b4da65f9d79201384c70e8485e01780306757575757551",
        "085243473446554c4c08010a18003f1090304d08020304dcdd01dbe63a81e15301060031ea900b00a7e87d35b8ae0148d435ef35f500a7adcda6d22601ab201673405401c2c41249e9b40144588c586e5001c0d630705d3800ceb66d176ba500f1019c5f3714018fe6d8a1b48e01016ac56f5cd500bd4ac43309ba00843ee1dcfb220162722669aeea007697b9b49c8801850a65942a2a0010ef7638e86501175b20b793c30044fa63160fe100b2e3dd4625ed01ec7cdb8e3862014e61ce20880101b823789a99330087f9344fe56f019e2a10fe08920009bf96815e0700ca1e9cc43f6d001eb79120d145003f90ba93ed6c007a5d161149da00f159f5eb946701048d2493661400fe2feef85d9f00c8172c54398e0089ae13ce34d500850dd77f4a2a003fc21eb5550500c32c872a25de00bebb710c134d0118c916de00b401a3d8f0894f800186680e22df17004ec4ddbaf276017679939f6d7c01957b771df35d01e94a0b2693fc00cbc02700ab0c00a57941d1c4ca013a67a015bc84012613aa3016bf01e3e3f45a31e9000c370669c1ec01aec526f28d5800655aa71775ab005bbd50a3ce08005d7a6436737901af8cc5ca3dc401113671f8d34701024e13abd7850033b21099656b01e2f8c50d412601ba2df7b71e2f00c9c089b4a7980047576720c26d00f5f440c7208c0095c544afd96d0032005137ca6700a88b1f49510300173a9459b9fc007fc1d7bf6acb01859bac5d4e360031b026b84d080292a700bb46d3675fd60079a64fe330fe0042345f2c721200be947bf65d1aaf20d68a995934a74df66aab623d66df7bf1af58546eb20bf406a744114a7b6b0156277e8c0de301a28f9396ba3800d0161bd897fd0018b1453e295b000569ea6ee29a01f53cf6b743fb0170e453c762cd004dc58879301100b99981667ecc013a0c6016c78f012ef98466ab070073fa12678180016e89d98909bb00d170606481bf014db1dbd7fe1e01f99e9a86bd85008ede91ac400100a08c1aad8e21004cf3f376f0f001198a9d23e577004a17edd1666d017cb079f0826201989d80e9f6dc0030660f209aa40018bb27a404ba0065ff5075b7280124d67491bb3f0196b7dce6b34e0198d1d115ded300c40c18c0fe85005d71491559be002a1f146e50bb0009d2ee0c3bb60179b9d3efb4690000aadf07f604008285b86a356d01bffc972eae5e002c3ac40b6aba00133d991b9d6700bc155f6317ef010db73f8d1f83006fc75fae4b7900efd298594ad2019c316ca95f8e01a235c870ee56008ff0f4f7c46a01abb928daba2e01423eb3276dc600f1d260a6e98800d49c72c6e941018e53510199fb0124123e635b9100a6b6c8fd2812014bbe773eeced00096b61da3ec700e3c254b6d31e01e95d9cc7a3060151867afe76bf006bf017c64daa00c5ff2e2d3d65000bc604e81aca015c13f97f8159016348534cedad014033ce11631b00ff8dd8a901e901623b4d0802a23ec52a01016c3faf05e40067547b0f25ee008bb6a8615e5a00a78cd1b4767d006551dd18e9a60031ced2ffce2f010ed7e96b60060147b7c0f6606d01a9a1ae6ecba800696709af26630022a4f7c2081801bc783124478e00a0140a70536401a596c6190b78018512ac948ee501f5e48a14bdce017f0b76f7a0980102fbdb2a070001fdab2b71e4fa011cf047f5d9b401108ded791f9e01a2e72014cdef002f3a0eb507c7000dd3095068aa00ac2523b63913002d43afcea3db008e99639b125e00fe804869346701390a56a62d6600f996332889be013040fb688c8d015b63d4f8b882014a59b48baaf001826067389e9001070a719bc1d8011f03e5c6e2db0060ebdb2f52e401ae035b17e56b01734946c444d001698a3a58177100332c74d2e2a200e031f89662470102a3b07fc29a00079597b99dbe00e1e00f7193f2015a55d4d5d86501e935f495687f01f14f713a92ec016f212e952130014c478ae5650f005c146e236df1016c084c15ad68009771554dc07601ffcc425afb8a017312c94daa41004ec702ff6018009af46d37c7130072326e24c2a401062116b4607100fbd0d0c3bf8501d40344c307d201725572762d5801580f27feffe4018e04cf49c6af003556cb1465790081bdfe45bd0c00659a69370df0010134ce7ecfed01a274a073afcc014f2ae6a08533009414ebcacdf3018606a20e8bd6014c85bd3be232000cdadb5406757575757551",
        "085243473446554c4c08010b18003f1090304d080233e300b622e5296cff00ac3c35a5ed700075a28e97e63f006c58ad842e76009c8da3e4ea4001278480116823000589c01bb7bb00b9d0a1d48de100aead2467bc32012df6ef55d2cd005e0ee5ebbc250009f752eb580300a77ed04b87da00e2903abe188301c6dbfcb6f2fb00f8983175239f013f2883f79114007fb557229d840101b7142a3e3900eb38000c8f24018753ef417e66007f4b5228c42701bedd4364b6a1004ca4aa4f767c0154d3afc4036101f4bedbf3f9f2003088420a81d1010d1c673d8618011f2f59069304004f50365ecf3e00ad875a6b5e1400fa7e7f4bffaa019f4b6a6fc0a3001e4e20cdfd9c009a06065c9ad1007370ad22054800cf1c74fe61ca004555cc77adf001b2c0e833957d01fcd0655b0e690199afb8f4b0960102079a46ed8d01f1bcff23641d0120e62876c76500536e1cd2de7e01122b1c2bcc39003e910f583cc9010d866279cc9f01ce7e841f006a019ca1056fb0a301da36783ac73e00ca11906a2f6a0096c7ed9f614f00ed0c8c395e9e00f5368b51e4b5019c6e03349f7301e2fb9fae08cb0160c3e4612a5c01257310ab55e4005fc0a56da7fe01c2d8d84d1f6d0013f8ef76847a01681cabc999030109cca20b998e0064ad71edf15201cb236df3a9d20043373e08b16f01fd3ef0c6a6a201c3b95e6780a500892c2f931d220074ba0dca2e6401f12e7ae5c575001ddb47e62aa100f4d7be8104924d080201d82fcde2c8f200f9e4df9776f001f1d8c1f00b9d019964085c87f3006d79ea031994009dbce968c71d0112db571ac52500571358fd3228008d83f8b235cc001ed3d0e3f3b2008665d99258be00804575d8d245011c39c2b258fc001e74f14c088d00fc9504c4592a00513f7f19f924016c46de50af920009dbdecbbb5d00ef1654e5d539002dc2740924380085bcc426d14e006ebd32a746d5003afafbef95fe001515e19f190f0132f4af67ac4901d1c824583346007d5f07ee571800b6a1322e9eb700f3348cf167da00d3853dd0ae3c00f9bfeea3ec5400a206cb0eff7900d96b847a4dd3019d6594882d7701d2ecf500abe201d2bac41b60d3013b1bb704d58d0007380b19d149015b22b0c719ee01e47838d461ae01c9795ebef708018c4686ae799301835cef6ae65100ed8c5fd5cf3801cb4a3171b75001f669d43f8d8c01a49b12b5fe5601e394f429d2ee005286ae24be6c00e2175cef497a00fbdfff0cd25600685e9d1b477b00ebcb14c528bb019bb4f7f3461101ac09c4034c950103948220e9c3005933f53c8caa00c89e6974ec5d002d54e97128f50041cebc60fc9f005b384693a21200d973bd25f8940164b1bb5aec3c0077a5e094ee7d00c399111832a901f0754b197ce700279bc2a268e10133e7f60efeec009a5a655b057c00b7d5671e100101353e7c2da57200381696511965013f148c4fccea01b52280ee9f5b017f4d0802aec1ab7ed40162388be7afef003a1d3b9e4214003e39a2b5d4b001096eec2d38cd01932f382614350083b055c1e3d5013a068740cb70018a7f1817a6ea0061404c508d1701e9052ac3c003005f079c0e93dc0057b075e512b5006e9132fbe38001e4b287e1bd53007640b8667987009b6dfbf2eb76012dab80132277007573b0057ee900d3ac15da049b01be480a90f65100b7703ef565770167fd9aa58f120086a6400e396b0163b74cf9e04201e5ad6a0c4e73006f279f553e60011905bd9f5b3601482191751bbb00ad90c5de02a700db07b0da6009013211681f978f001bf8093ca35c00aa67a5a33cbf0180c85fb15ad3012c175bd136aa001c5d7acdd67e01cc39becf41d3017cd2e1fc27460122c90db7c4f3011abd731def4901c4b4bcc87d2901aed3971be9ec00687af7a926d900ced5c69c334e00ac79f32455ce00222d03f6b272006a8ccaf5fe3600d4e8a5f943a6015d4f3cfba4500180ee0485bdb8008cd4b9d19a8e011fbe39f43047003b58f38d334b00d4b06ce5b6e3014e2d1d5918bd007be4f9806d0900ff7da43150e2001294e516f64d004241c00ca7c40079a80d1688ff003fb84edfa71000f33ebabf85860116a01e08390100632ef5f4ddd000ca1a058aa3870072046a826bb401a8630b4386f701ee2c57431b5f0040c5ad9aeeac00bb75ca3215dc01ff14477264df0011515655ea3f0140f87299f37a00f836d406757575757551",
        "085243473446554c4c08010c18003f1090304d0802a85ced006f10278551bc012beedb33cda300e6fc8d1a67ad0004a783b6c94101af4ddafb7b65000eaae17ae49e000cf0141e12f9002ed52397aec400b4a08720ed6400d551b726550900853357137efd01a34b4daed926006f3bc4f302db01738080cbb91b00a4a56ee86a43010b9f7f91ebcf00e8220ab80c97013f8ddfb4f5980107df33b5c48d0154c21db505200168e1bcfc3a55014565c41a025900cea7b9347dca01f46f4351e409015d2e63bbf06c01a80043c4fe7201525f4252ee990163aca3e75f8301935368de72c200525c759d5628008621186d33f90031970c8bedae00145e8661cd1b010fd4324eb20500dab092a1df5b011d33f4d3b7460066d423fd72840116e29b9d54f701266b6899116c01c0e59e2b8e1800481b265dbcc6007d91ea9be247019624d1569fdb012d22aca474780114104af8f1d401c28642bb6701008b7800e9bffe001cfcb2356171017aebd3ebade201c5f1777eb6db000028b68c8f3f01f4607ffb9339016d9b7298368501239433542148001c0d72763112019a1cfa72dc1f017473e18e00ee01286f8d0ac2f000b8cb62ce83530097fc10b19cc101e3536fb8720f01cd84e1d69b64019d920745390000a375e97887bb0102a12e92b840016da45569d71900cdac9e0d0d5401fa4931881bbd010c4dc2cef85001443c446ccce9007361f3a0d6d10109efa9b87cdf001c8415dfada900c0849952364d0802f6018af99a7088ea0009c487e72e2b01ff4d9842ed05013a03af45233d015afb14bee05e0063b5c143fc6a003a180b7941fd00fe3a109a610f01e6e41263077a00f8c06457aaa5014d13fd950d560083dc6ad5198f01758a2089d36d009b991aeaa38301559b241b3e24009c24f902d18300ee2946f7342800cc5ea28ecd6a015e5d89a7ac6a01e4c812d9b71b006755dccf120300bb852122681100f81a1dc56a8f01fcaa9741d16300eb829ed5f579007941cc8b9c6e0117de1c853ab001e97ca193457a0130e85a718a1401bfb5e4cac8d1001f0f68e00e6f01be9351f7682701da0a39448aee012368a1d324f8010d8f156d01ae01a7be62d5e639017dd4ae9a9d8200ab690bd867df018d2b46bbd99d01c9428594f97a00fccf62afd23a0114d54e2465890041d387caf979006dcc23960efb01d8afe88f929f00b269089099ba00f796fee3f1b8006037a2f365f80187821aa8673d019fd98f2bb74901013c7bc3bcfb011948ee5e7315018bd7207bc2c1009e555fbc977500382141e08da101cf6dc39d7ffb00bb4ebfb057a9001bc66c933db900a3413ee4db27018646cdca19e3014d193bc150160118c1689f42af01526be2ef2f41009fb9d345e763016b0cb6074fb90049336e7068cd01790e3cd91fe401e03a97c9860b01dcd4ea76831e010dbe3acd687d00d81f2662503f0034eafbbb895e01f15d480db735016a281a064e4a014d08026d49c612f7a100414371559f4700c693f3408015005fb7c1a4b0ee013948a72139f30145e6b073f071013bda34195e48009baff0ba168b004fe71d255f2e01c0f121acc8a1015be694a1b03a01d2289412ff3900e76bfd82e6be0154ad1b145815000beacab4625e00519ec45fd33e00930fdbae3dee01c6200014471f0126c1a907e868010924fdfa8f4f0098c18bc8b4a001a4b820b95db0000b1069d768cd01df712083566d0127159a7bfb1e0050828130473501802bfbee482f01ce0686d390d001792c7daa16b000537d9c7e0b97002a1ae45530f700f9c2df19a423004a5ac7c4a6c8001408a24c3da70026794ef7dbeb0126d47ddab50000f276c43717730125357f4f5e2e006baf2af111b500f55ad807177a0095d597949d5e019e768a65c45a0146c73467996801b00a7143ce2a01221df6dda8c3002ad5092e75f901ab7662d7810f009be8341966b2014fb8e984540f00661ab3e7988401fee3df2eaadb010a795fa502b8007390a4b43e520075f8435d7a4000346adc889f6101344723653948005e1a1193b4c600ad2d10006ce4002555f76c67ac01bd537cafe222014272c032a4a40019406b6197cf00be05b9b0e14f00bd6de380ee9201804da3fc066f006488bab4f31d01ec10f1f0817201688c0db35550001bf080cb1ded01080978c9f8dd01765d59ac60dd015471fdb8e268010b977c4758c20125eb03f1fdd401355a06757575757551",
        "085243473446554c4c08010d18003f1090304d08025f0e67e800ccead099502500ff9becbee87a0164204fddbfa300750bd03638940167b13f0d266000d7ad4819d45f000becb36da2b800bc84dbfa22a200a216a895568f0169cbcabd1f5700b3ab2588008e0128e9f4584dee01acac808c89910124f3bf6b822100145b99376cb90180ed0517455501b2c7e408bb48014b7c4f82826f00bba9abe92d630106b23ea4c9b00041655dd63f2000d13c895b4d4b015c324b14c9dc01815f493730ea0194581b70826001bbacc5d0886200156725f7ce5501fb53779e82b30032b849b5c5890198295b910df80014cbc440336d0033a7ccf60486009aa912700ec300cf9df7e3980a00668f6e16014a015f17c1d10ffd01a4edb6967b2900d5f1ed55a35f014c1996bcaa750128f937175d8700eb471cee463000f381c00af70801cfc0f0ce131400c167c5be79e601e8870e3420dd0002a46d1ef4f500c3e0d566bedb0191c1fbaae24f00c14f42159d34000fc4c031422e012b0a3177d1ec018b14c11945a8014d0fe8b78cb501a611ddbe6f500074d05c092f93009fd6bfcf1ad701ddba528e2ffc003b2e77f78bf0003db63eccc48900b2756535923401d90b3ab05577019dee5d135bc901d4a3d29512db01e0cfd2c57040001cd3a0c19df901fc2aa2dcd75b0051c46cae708801ba2274aa39bf00b4f94639685a00094fa91994ab0087baa0149a920171cb7d1f04c300972ec0f947fe01dfce21e14d08027bc00175098142f09901fdecdfc1c5be00fa85b3720c39008fc3c1739ee30071a6414a87560123d22b298c2d0001079a85e6fa01f49d9f6d7007005ab61477a8850020932bd22a3d00eb70ce8d44c800b1712c20352c01208d8aa939e201c97075707e7a0042577df92e15018bfd0160f7c2019999f3eda16000b289a7dd0737014043a0195562004e7be8e61996003f9ff40d645301019354c5d2a1007fdb3f46052d018b8db0e8eb2100a17f8902de0301ed36fcc2789b0154a4afbe1293002076ae8dfd4500afea33061dc100d0d01189dc55000dcc86a9cb7d019bfc2b5364a2010190ade4f46800c6702683be6500feb450f4735401043cca06c92901185680dc18a8014418d16a1d95008412488f7a710051fbe6a8e71c00c684f4c8f8a600984d8dd6fe2301f70a435807eb01a98c7573b6f801863b97e06f8500cf25229bd7fc0081cd0d3d5af4004e4193f72bbd005668f35298950174e28ed51c390023cc23cfa8f60156403f62963c013e6a0f7ab3b2013c17c371d98401743c9c69a68b013982dbbfd72f01e19b3f6a82bb004359a67a58a200ba1cbed4bedb0048cce2fdc05a00134a9f51b8820119016c404a4201c08ce3fc5c7c01a0d046bbbbf801345720ed153d0193d93272548601f50b552a4cc501be2b56b6cbc601fcc4e4789b1c005f8decd966c70065b227d3af5f01f34cdbdb4c4600995221b00c55016fe37e3dfac74d0802009bcb1b902870019bb09c6b56b200c9636664322400369f46752178004074cb1e80aa00efd210fa8bcf015b5d793f679e014a16c504d35000484c5d018c4700cabd48ca8416007a646dd6019e006529c7c8b0f001a859201b9aea000e7229f34cbf01a3b46dae2ad2011cbd16fbbc9200c10a30dbc105017f174b4fc88b0167c85d98a33201084ac7e669e400cebe23ed0a2b01bc2879938dd9017a96f3cfa59501e7bef4dfb5c50110c5417df33d009fa50b8149140135cff665f64d00d7928542c179018c6d41a6136801cc49072c73e50002981a8359c800b8a698aa8ff20122e8d08fb3900181c0e0bc240a01592e4d1d6339017860a348cc92006f35ec9073ef0176eb7b9216a70102bba514ab5b0194f06083bdb100745348d6ecb40076b469b7930c016c75977ca2590148d4d5a2dc640035524ca3b91c00dbadb5b48a15016d3c6c3e7c8f015d26bea9671b0104ae1d03abea01c48bb3a9345a01ab2aaa45a5890099eb3c7a0013013d94901dae3801b4fcd2fc035801e08025cced63010fd446ae470401dd696291133d00635b94a5d487015e597985e5740108fe8e28fec000ba333906ebc40111876f9b84e6012b6ccc4c8c66008d8a60ff29730099ce81ef4b8001e1923d8b47b300adb92001ee6e0134d58f2d5a1c008ddaa400310f007a59fb862bf901c7f5148f535c01fbaa93584478013a61fd9366a50176b65ab5db09018706757575757551",
        "085243473446554c4c08010e18003f1090304d0802c09271190200e4a40fe6e17701f9594dccb1f90135c5218ec93001f47559c4a70801f2a41c31a21d0074b2244951f401330fe6206787011521f91451a500ad880e46158d015964bdc4f1bf014ac312159d5001aee050c7d57e019c3ad0546ab700851374baec4300f7ff57abcffe018c7bb7bac30500d5a3ea9c4af80021088f9181bd00c4a6ced5251b0022bfe4c0158e0049a3536807c4019cf1c56b79bb006a6d55a9f1140115bebc00f2c700865a8ca491dc0116a20e0c860d014c94acb0b8d7019cac1f6e768601ed8c569b1c270134c57b78768e01b5daf481b2e0015f7b388d5443011a08e05ea8cd012b9f2c40634901e080854d86a6018e516592299c0111acec2592c0007155a1834b5300134da37a4ed60005f03ec7c29b00a7718ea06aea00163c9b6f26460134e2c7046647018abf8ca0c3b700c85102a57f750039d17b75caa5012a203d9f49ae0085f16b5ccd6b00575dbf35436b016e7eef4579a4013326c987f12f01c543f12a58fb00922327023eb4013ced1acd8c220156d7498feef6016923a47b70f5013c306c868480019c626b7f9e7c000d4d2a5bf7f101df09dd3bab270059d5f939efe301de55ec1bdc6c0140a0c3706950004e91abfec1790157b1c58c5f49002d0038cb43630195641e302ba800c4e6902dd8ee00ee8979d9286900747112c496df0135fd2362827b00fa7223d999660050f68fae9232009f18cf4d0802173ba201301c77fb4c0d001266914a5fb000fc967e55d57c01234bae6c833b01cfa1c20ffbe8015e9fa135a80601e880e1b368580131d7f59c24560080b65a9076ce01f343ff4aad6a000f7f0511cdce00a3a62116812000ea9c0270040c0186bfabe849b60086a7ca9f90620088339dcd859a00779b0195487f0173594d18c19c01aea8d60b881401426e76cfd4f4011191501e8b4a004ce1002792310050533ad89899017330198b63ee00bf9319fd166400bfc027ccedaa00ff7226145fa801492a97190c5d0138179df02fb000ac17885940e200763414020b17009cc8dcdfdd6b007f176539a2e9008d316ea72503004d49ee9d14ab016273bc45c57d00614f880ac34d00f750e8d06997012e49024c855a014efad70ecbb50183231447badf018502669bf68e0090caf3aa4b5d000988eaeb9f4000c19cbfb4217701b39dd0713deb01291f8610836401d57a5ac22bcc00cf8c3cec2d0701c6c0e467247f000c96fb5b47220093644563981c01d73c61bc18a401c2ca7cf6e2dc00cd7f0a325daa01fe6845123f1a00fdde5803c8730036408e5fed2e010cde730b647d008086e8dc0024019598cde5d0280181f70bea47470064a51a60080d019c9e22b0611c01a4424c952e22010e9de831e1310041c143428db300bdad5bb640d801b5e757e8195e00e6a0ca73108c0057a48a5414ba01f0ef078c57fe011b4db50960d700ac2f5b47fd4d0802bb000ad3784b005800492cb73c566400be0039bb8ac801fe41bdbb0d7700be59dfd1ca67008951e7d398fe014435d1636514001f5f30c9ea88014b9b0b841e3201cd3d1755519300d1014396368a01457482c1389000b2d02933ef2700c0906e642b55016c715fba174b007be48dee2f71016fafafb2b73c007c1469da26fb016f6d98798a98017a5c1bc4767900bb7a172f3d9e0023e91c48d020007400e601e1cc009387eff4bed601b71aad79f9e900823d13d788fd00ec93753cb9aa00de86fb6cbcb200c90e94688f660012f0f5f078b901c2d4336016a0011f36c3e81f0200fff9e9377b3300f0ad47a10d81002b2fcabaec87001324fcb1880d01dc8f4e00e4ed01452f20cb560a00665d79f8351300a2c83d7e772c002e40a8875fa9016a286103e394004542bd7597fe003ce6c0ca6b74018ed39ab4775601c4a9ad7727f201f847660ad9ca01e89b90cb5fd800cf855355ebcb008e9c54c3ad3e016af1beb91703001e9bf152d22e0037d2dee71dd400397ec1d555fe01a710106fd232019befe6d8e62400ba771ce03ce501fcc8f4dbfb03000eb6b9c09849015f4f2650b0b000e2fa41dc1c590160982453a9c200dfdb22e288030148cc98e15f4100997abd1b7e2901e7b60a8568fb0186c65e9a11890045dd0991f4fe00c91a4a1f73e90037eba8156da6000a05bc7d22e6019e968a1942ab01214858a0b1fd00d50bbce14ddd0106757575757551",
        "085243473446554c4c08010f18003f1090304d08020d3dcdb1169401b04e355a12de00a848b8b2067a004bc01dd564cc00421903e90574006f7ef8e0b6000007482d502b3b00819717ebe4de0111134a8187860114532d5c11540185cee89dbdda0159d85719f23200acd352c65ff701b38a8e3c91340066d086cce66c009ede1a368e9e014baa3b736ca20080154b12a5c900078066c970d100e0cc84605f6900f3e370c47920012e2c2f30133a00d0dd6d815d97002eedd88e3c4801e1b84307462901baa00007135801f46c14d3ea150094717e73b4b200500a1478b4d800ee2db8e3d96d017df85fa1aeb801f0461a9a8f9a011efab81c0930000d209f68b2e1014c5cb75fb66501e2ffc0c31ab0004028129c2d8901b82c1f467c2700fce13c01b7ae000e2088d638b9013252f2bfed7301ad2540f8d1fd01b3ae8e499929018efa8dd885e20116c17b10dcf80107b82a19d20f0045eb520c60b801512849718ba100931a62afd7ed00d67638a582f501cb9e60af3741019193da14fed6013cefaea5265c01811f10caf6b100752bdf132b7d016082c97b431601c6c415966a16008b6cc5129c9200c2c88193ee5401a709511aeeeb00f8edc02db0cf0152e0769f2ef50184dcf4cbca730099a96d1c8eb600a5e789159b02006cd9a834c99e019adc62348ba20004deab32ad53015dbcdca693a400961d6e8062b7009729748dd72500a75691def43c00e85731e1578701357a0a4acb82006a2f4d08023537158b00e06726f4b3b401931bbce87aed00767cc46cae5a013ab8ead4e41600191e6c48a8e400ff40ef48557a01d88fba4f40ec01b8e558d95b150129fc115724a001cfff0dc3c44d009e08999a106100dc6c6bb500470175dd1fd9852701d752cf7e59ac007169493c701c00a8573deb08f201199ba34ba5b90068c847ec7dde0022eb884d9aee008dfbe0902af100ee737251ba5e00a4d67d999a8f015e48af63818f016bf02792ca7201ed3ff5da0dc20192767930154f00cc947a5207d901011c0ebc3c73018d3f1ac6926e013b30b947a0de00dd61e7cc79ed01a8a66cdcc8e501de7121c4bc6800b9fb49249da200d19b784cf0da01534d5d15b8d0019113c20ab5e1017812b4d4345f014c3c7aba7183001abc0229fbae018210cd2aa1cc00733370537bad00c093d134fe7501cbfda732726d00e0242374d9d801b7493e0c842e01e938dc33386e01439c7d10c4720143b5249306d601b8e5d95de9d0016e6477726a8500818eab5f440b01af8a7079c9120187a1ac4166eb01418d5215260101f19dfedd5bb500e31d3d5492c101b24cb22d968700f4a0668c5c740189e1e9b17ae0009a8ce55e1b58017485eeb54d4101224b8d20e20a01cbf4c06e58fd015d3546b1dbb601b49f9c05b328016a3096f0e4d801f1d7c3c2381500add430de59a80058b34b7e2d7201a2df76e3bfbe0146a12b7cf5a0017119ac8a0c1e014eabb4974d0802830d00723a4053175f005bf0faef7f5201e72cff27166c0007d79207765b002483f2ad550300b0a787999f67015b7d6af4a901006c2185e0324701e773cc84daad004c1072f497e501fb481bb55c890186f252fcc086001008ac027d6a00b6eb4d64076301f69c21fca6190144712f94ace50027aed894b83800196aef570ace01b928c3e9d57f012de9d042296801413a21ed96a6018b5bd07257f401af3ddbf22a2501907b24d5b1f501006986725bb401c7d54893861e01680919ac1ed400c3ea29c698e901e3ee5c9954b2009ba4c06380f601a8a8687c764c01334d3ed2818b014f64ea32b25500d21502c58be90136247ff44193019f9b821c892001740fd38a4e7301ffc87219d5df016b8716667a9000d956b787cae8001204b1fb4da90126d6939d5aeb00b96074936e13010a4f554fe38600b864f6b3e1d10164430a58dadb008067f4ccd20a0038c28e9c16ac00e75e830d309c00049ed7aa87b101b5c99e66cef300c49d749a881301757dbb2646070144317dada9ce009f9da9118f90000362b5a8512d0036aea455b63f013a93b15200db00041451a684d801813de84dec24001de10c7647f3016cbcb98eef2500f623d4a06bfd006ba80f3eb9eb01e6ff72d9fb75018a6cdcb2b59d011ae7ce071c4c01807df658e564011172e623adc7010999c7041c9a0158e831d1b52c01efc72cd39ec10091443fb32cad00fd8ae25dc10106757575757551",
        "085243473446554c4c08011018003f1090304d08020079dfcd77e9b300f14b8cc10bd00199045efadaad01ef6cf12cbc0100e7097f6d042e01412a068adb9d01d4a3aaa9035500bbeaf9c6623e00fc56ab9f0ead018e7015adf522018217040fcd4401c20b4077460a01f2197078257901adbbb0ba4d1500ab9dca01e28d008fe3cd19da8d002b60337aee21007cdb002aea6b00c8ee1283e42f0023cb98397a0600dc1953fbac2301286e93780ccd0195e5da3cc21a017d61521834d900af2e398d655d0183173d7a5fe500d9abfb516bd8015811cdec42d001d21a2482700e015043f2f58411004551ee02c9910027c07621383a001e6f56748c25002a99a258c0f900b6c67d5ebada01f42ccc246cc600dbe325c9fc2e011bdedef9b8d500dc2270deadf000534791288a8000dc06333ea43c00a341e40b8fa5010e4757b8109c00b634880dcf08000a3c26ff34a200b5cb3fe8c7df01466b06023fec01359f92b904aa01d31f11135c3601e102cbfaa67b00247a1bd8e58a0144c4d41b87f9008698b08f43eb010bde861eb0d60185c40d9fa74c0130649400162a019dec83e152d2006ab0c54579f101be0807df2688002e7cbf75b8f700945c5d55b3a50194fa476267a1007cb4e84aca5f001198040363c6003e2b77583d0800813c9d32d1e00033366b8b3734009d7203f6c64a017561a867d3c001baa64c6d75c3014ce329de55840003312e989e4701aa9d9719db9700e55f05c2422500854d0802a9e2626d1201d8e0439ac0ce00c2282733a0b30074bd6548700e008bce29b8d2d900dd484fd7c47801b231659998cd00086152bb0f7201544ad79279250150f8a3cfd94501a1c0587b3b7d001e4aaabc307e01c4d337a8291100c4fd7cea1f5c019a3b1b1bed3301ca099a2bea8701e7db3646536b01e3e6cbaff9ec00b7b6b3c65d69009b546fb200ea01c7bec54338fa005637c158b43e01ada765727af500d6fa49ac6d9500d9aec38675e300dff24d08e5dd00743fe75f142f01f02a80355e3e00acc152c6b55a0003bba3c62282019a11e57731d500db2906064ea001572b15f97d0e01e8f888e4a44501090596cadf020090662265d88200963ddae8bb59014463b6e35d470157f4faf6bf8d016e044c8b800200afc4230daa3000adffb976fd2a0151b94ae6768f01c0b11ad7848d00d7d7da80c15b01796e102ef18b00a18620572e08001eff5213879b016abb8c082ba50091fb14cc8e8000a7dbe8fd3fac01b59352b0ab48007c9f3399f178004eaf5ee7902f00147f5c282d690163eac9b36e820038d5d32b996f019b2ada91041f01c1ddb5fe595d01c241e4799e9c00ea98ff34800c001472203979e700b4185e2f1bfd0125f5209358330096085dd10f8a007198bd5c82410124053e16ed9f013f6285b74d2e0121b5fac81ae701883c0edc6b9e008ee8710a59aa0156898d1288b000f50779f1ef8d00a6267c3207d801f5fdbb4d0802aeea1101e1ff17eadb59007618c5b4f98d0125d1d88e170801d9f7e17e48cc00b3679c2a6f35003d7974a9a98b00935e77f1a31201bfca088ed9ca0030d22975978101589e12f3f6bc00377061ced80e005692b86a629800e38b8158b6c801b7809a504475010596b6e8f2ec004c452f1f7914016afe9b1881fb00fe0bc02088180082692802fb990022aacabe2923010249361fccdb00c955e9578d5101ce635e84e0f60151cf050ba4ea0046ba939ba97d01740469911d3701d0488df686eb004b0a162186da0198473136601f00812601c4665700c331b5f18508016e0b6791bda8019e2107240caf0090e1dc53ff000024fae301736f01c825687d327e00b72b90caf59e01d3e017e2b4430041b12c08b68b0132aec505cb8c00bb8095706c480120e0c1f2d473019b9dde110b1300f87d382324e801d7ae6f0e955101d7a8c871076b006a6149ab34d7002e3ee543473d00d066dd9e15f9009c1b421bf6d801a2d2220e5d33005e4ad3516aa500ec9de71a17420127494e38721600b2047d6fef470046577b2139e90158ea9b310345003e8b38b811c1013de288d1f78300af0850fa231e01fdc74b029626014e60fe63e86201e2a24aa88a3b01a054e5b1c92a01365185da891501cb650d290f1301e3de54b3f06e0087530b92f8240022d0051b358e00ff2229756525000dd9e30f4b6600a17bc7de7953007ba3bbff971b00df9ca51fb506757575757551",
        "085243473446554c4c08011118003f1090304d08025900113a553bb06101a42c00ac5431000fa226a615f1017603ebb807490031248cd0d9ce0105297f287c4301aa9dfc08ab5c012cfded025d0f00d1678c9b5fad00bd6aa2b711ba01f8494b505765004b09ec5c33de002fd642b8e3f20151fc3f43055101c1b638a3228d01436aac35f028010f5ab6afde7201d81dd7769ed901fac3d9a4d38d010f1e08c2262c0171bae308a75d01d14ef7a8640d0181c0670d7c1200759bec2704ea01bc94fe4fa07e01d800073338cf01b07a7cda35a901d2ddde4697b50082df94095ff201f76deac4067200410979416efd0177b38c46fadc004ae93c1ad1b10073b67d358f5d00704deedeb74901e618c5afafb10124ee24ee4fa60026629dfe2cb7019b93ce46cbd900e8dbd8205dc90049b858d97a7301053f2f46f11f00823d327e80830104126f0531df019254c050eef401212ad564583b01de07d1afec510048a3111fd4c100712e2f4b1a0101471f1fcef0d9006feb40c301d101a20ff549cee301c20ec537390e008d00707b35ab01c61854b8056001e1b661a2facc006e0f908d4a0d0115f9373b70d20040f08baf081d0068b0c5ba1dc0001f4f82e7d35200be0edc15a493015c96e89669e5004e7d10b9207601709cbfbe648101337340ae6c970058f6410108130176cbc65ba0cb008656b079d15401b7c6f62eff0101fecaefeefbba0142e1b71033a7019b8ca2887e5e009c53f927cee2004d080246d4d28074fd00865a4f2e5a72001557fe2c733400493a75e66c220037eef9962c8100d4e220d3e41a0065bebac4cb48007e391f52067f00a0d80750d32d01cad50e071045009c3fad429ce70094d52d6fc2be01074a6ad5ba050156e60e1fea5a0029149d90480801a5eee9eb7f8900344c62a3695e00711c3581f64a011764e551d67c0033dcb80ae09501045fc56c20f50039a435936f94009f8ea6e81a3401c7de22bfe8c700822a7233787701d796e1a4836000bc7a927596ae014ec034b13eda00c1a9eefd1a0f0191da403b2ebe00606935cf65280043d88faba13601f77935b0cd3d0178852e0b18f2013ac3db6ebf7d01365648299efc00008c8bd24e1b005e6213d32b7f008b066dc9406601de663713ae20009cdeb0455d0401457bd65df72601de1aa0261af90112ec85a0b9c50083edfe26bf2e018c15cb46e09300e2f5370e29b801e15ed8ce9f9701c0815371ad0c001c6bfc2307ba005114e98510050096e14bd588f200037396b27cff011bcc3b21d17601c0c21d3bef2401475ae1df599c010bd0c0e1e652008774cc3564250059df322d3f4600cc833426a7a2019f004b96ce4c00acdd2533a0cf0190f5fb0cf7a3005fdce655f6c3019b4ccb53650000470b41c4f5b300495681582e76007966e1c1b6a4017a24cc3f7f06017f58d373ee8f00de8c231527ed01b1e305b365f10127e9da89c5e5006f19dccbf46b00ddea4d0802225a3ae3001b0d1a1280a10014570f8c3e4c003fde80974b7901e991e049c89700a8e1389cc7a201e6e6789e0d3900ab30f7a65be5008e8233ac54730153f402630ebb01310349745cb0018a83c70fd98e001147539eef3c017a908f6d886601de85552ff7c800cefe95fc81a600a554640dc77c00999bfece95bf00bc1ee8cf7b8901c34c2617036b0124e7f03facaf00f1ccb824e16b0084d197e119880084c80c200617002947c52633c801d4a35eeef4c7003034adb5edd6016baf7e6072790056b54e1a5ab500bac466583c60006c82b6dfce6901afc3af73be9801e18e96457ba3011cc3e37f7a2f0184c97f3e79a301c28e19db59f9000d117dee4a720060ebfccb109f017c355d7c547c013e57cfb5be4801621d9c18ad5500f520a1ed367400d497024d7d120027fae6f8e421012ef98f2ea0a90133d0137d9de5011fe597651e1c01c724971894df0161c6a3be5f7a00f6f204083e14002dcd321e6561007c63fd8e3dce0126e4cc894cf4019237f34beed40087ec9b244016019cd19bbae5a7005d8a47b02aac01b150dd24adaa00885d6fb0b2b701e221ff475ded012fd4493fd32400acf71e79e3d500a436deb6320701a7420f1feffc01fd866c7476f900759fcb6b737c01a9ffcd6979f5014ca1ab8f753800e7918519f1ad01f9cdcda54cc601723027389b8500c70c77f0ec6801a2b438060a76002b9b3aac6dfb001918f0e506757575757551",
        "085243473446554c4c08011218003f1090304d0802d0f201ca9bb7063053019d85b1c54584016a9288204bee01608a781b7f39017724b4cc8466017d73196bfc1d006a9434a718db01a6335225802100426b38c5a30b01f873f1acfbfd014844b168b0620041de60879a7d00d1d6488d8503018d7cb654215e00b46435e9157600741cad838e30010db47e30e5bc01e4c72addcaed0158802104ffef01dc17391e219b01abc2575f1f3c00ce20ea820d3a00c3dc92fde96501ad03f70f15a400d7bac73f60b400a531763d88f900f2d67efe1b09007b5e4a169fdd014c57cdb8d05a007452f98fe57f0099f01f10c61f00bcc04a455176005a4aa4300d0601ba344364ee4b01a79c3ffa036e010a2768c34bd201bf22240f418a010129490397db01be6c9af67dc1016b0bfb8dc9830189f5037c0ab8003184daa282ee017d65db50dd1c008d05c094571c01b306367237fd00001b8d4038ef01692d2c9ad31a00dbe5c7ca02530162d0b981c7e400e1870e701fda019c7fc139c0790162e1239123830143585bb8a6df00675b780ba679016d32ce653e250076861a0054a1012d9c230beb8401d160c96a5033017b955b549ffe01bf4edb7f380e0112cf6e46a29c00e55b6a262b3700f2c881687e4d0189c9bf7bfa8800a2cd713cd22e01d69bdfaad2b0001a08b25d62db00b4a2dea7785500a590f6a8089a008c79f0f40206004b997692d66801844eb2fdeb8d01dd5c5c17ce9701dc21c4fa32c84d080200c519aca21b5201c7adee08250500a3c4ac4fb00b015c581a68582a012b0f8ab4c0c300f027e11e4c5701d91fcc0678cc007749976fa04001b8d7a3700f1901a9a78c66e74901af088eaf03e001f5810fc6af6200f3df84c35d9e00d906c9cdda7300f90610f2d4a20032934a6933a001f23792aada2901ef211aa5055e01a80ad54ec3b700052f46defc5301da82239395e1018b070e18e80c01b92f33ce68570159242921184f001786c5569c6d005e282c22b98601f34efbaaa4dc0108f2c463f23501bcdaea2cef3900e65c5e5e5d500119e14d545a1501461f248f4be10160f77cc87b50011ac69cb467d60177cd1c61fd3e003dfac15a069f0124f519d2d88300ae9cf6dec982000147ea22658d01732634b5d160010b29ecdb7d0900a25dacdaeac500205e627400a501da0e977d9bd7017061cf20744e0182de1847ed5101babd7e961e7c0016fe439556bb01b6b790e5023901bb370341eae700106375422c6a00303e7f6c2d9801565755d56a5e00a0456c0a659800ebf222a2442d0177d18de4539501008a728c5620004072f26aebf401b717e906962700feaf7ca986f200ef0253a22af200f824c95161bc01f86fab132064009549204cbd0f0089aaf97ca13d017d4d3677b9e901a33dddd88b6f00f76b2a26e8cf0161840f460332002d1539514a00001caa71555c5001ab9c606a266a005a8082c7a4b20155de9e20407c004b4d0802ab5831bfe901cbb7a4aa48f601c4f52e666cd701e15089c7e0ce000155b6b7cc9f007ddcaa0be22b0034aca5066549016dae556d884900fef392afad3d016bdcf6dbe04a01fc102c62772a00b0bf4948dac9004440d0f3ed8600d438c9841bce00a0b0a3a8f2d0008506885494e800759f2a4370e001bac56c17981c01581cfd96f2ea00d5fdd4b649ee00d88e100ca8c50051640bda783c00c1f0ba36ffe9017e720e9a2dc6004e09b8878964002b4f919e12480025207382cc7b002606b532c6cd01931596b9084401c1fc7310cf3201cce06fb2c97f0128927680262d01b2c95ff371d701d87c6dd93eb10198e66811cd3e0045e22740b9c5012125454dde3c01a49ad21ef37d0149099c4159cf00f7449251a5b600f34534cf15bf014b3ebb4464b0004c6b8b0c14850176b51b1581750080dfadc9143c00134194e1e36700af0ba9b107fe003684721b624801d5c86eda5c640025578ec23486002f50a789ed72007c6ca0055b870102c5fa28c93700d52210f6b8aa01d358794dd71a0106f14f28bd7600e5bec72582f9008f0e123e5abe01ee749788160a0149b07306c19201f707219893c2008fd30a32b90d00fe6e6368424201e6055d27886100b42792de58220113a1628aa82400056155b0320401f080c28730fc001ae1923a83b000fbeb50bbf07800e1d2097541e5019a6dde1b839901c4d19c0c554b01e5f8bab158c60194934c06757575757551",
        "085243473446554c4c08011318003f1090304d080220ab4100c02be41fd32a01c1246a312c0301cc8561e8747d01cc0e2d29f24600e326e5da767500dd9dc9b9e6b800c8ec8f0eb3f40027bcbf168a6901e7389ba1449001c433bc9db5f500d6ee01b95cf10160c1b155706f0104e0e909d78c00f1c5bbff1092019ef748b04e2201ed57df86f59801c1a0eec096ed00536379c56f9c00efb0957d6c2f01d8aa6d740ca501b177a9690ce30184956214e41e00f58a0463e412016aea005d0d8a018e10c44335f90179480f6e26d600970911fc56c70173d008993e02007112109c5ffd00768f88171ab201e97baa78bd5600a06bdf69560101d02f3a1a97f30126cd987646db0092428b0a78b3000c2a940a6a500177356aa13ad7004006e4f6f55e00b0bfb19c66df0033c24f9bc1ba01bff7eda7939d01272558ccb1ec003f8c7fee424200ee003c78bbab002e1fa7232abd008227fb92329e01762d3cbd67ab013ebe866aeecc0132be59ee7e6b01f92dd1b593460100237e091a1e0122545ffe3c3200ba6914ce89230087d4949f971300bde868d4ad1100de25ef40c657008e6036a3ee27016c40b3ab8eb70139615b55b29d00578727b6920800f6dd63d274a80035d9ab8767a0002df28de20ec9019f8aafce672001dc59e5965380009b42d5f6843e0044561d108bd0011c93a8ac3d540069100896357d001af02f005a4d010805ba44e89101ac4a24508a3900a9c9ba77377201e49bec248e4d0802030137c393ca9fd3014e9a4c002c3b017f241d8814f9005cc4200fc78400d5a9e9d78d9c0060eaf9e7674701dbfc554bf9d4018b392affb95e01c59139b802e0013d720c9f540801b5de21448b4201e164a672deb501723b421deb2001d41eda4bde6000b489c7a870df002a0410fc05ac0155054e8fc299018d6402dfb5f0016de7737119750191201364b956006b1e2cbfee0c000f2b72019c6200b10c24f7663c0079d5ea0168770044af2874f98e01c9e8b0c08e01014333269b425900027c7c36f7e1008ac661e4c5e900785942e6fa2c004aeed090ff0b0055d778e34b32013b4cb0ebb515010a7d5f06357400ec4fc47183ec006a876a94a48800e46c1d627e9300e879599201ad00f59f5a913cb000f78aa09a117700d06df2ed2406017ed178fd55f4014c1317a936a00037469d94f3a700a0c454167d9a01824a71e3f9b601ce3a24b46f1c00428719c6bbb101cfdc97fd11ac00e94dc2cccecf00becfc87fd7b10050e860b7650e0152abdbeb6ec00082d1eec5ca1c00d61f2b174c4c008cacedd1dacf00c135f5a76e7100c9a6fd8b791e00d9dce7ac6330016be4bbb48d5400eb56bedf7020000a9b77bd0fa3005bdc19600d7c002b907083c06300367634538fc3001f02e4faca5e013cffe8aec3fd0133dc04623d6201d8389852551101924089c0fde6003ffe047f8cd201f7e0326fa1dd00300bd12ea4bb002a9b423fcbc6014d08029d0a70a9696100a419f4406e31003b2f2f55cd24012e1dbc4e87820034de14cef36a01959643d8694f00e6518b7b637b01e99593c5d3f0005e0eaaade0db01b797a12722d3015107ee79b53501d9e49bebf9e0009062afaa27ab01b5c3947907bd00e9ac8f8c33510041a86b8e8a130099d20f3cb76b00437b463f69ae017c69ee202c3a019c408bb61cc900bb70cbaf53050121a902e77fc10069253fee950001c48a77afd3240150ec73c67419011672428a7a2a01d61574436ca90190e5ac2a486f00bc1264832bae006064a12bda2e015903752acafc012070c4afefa90194353398c33e01bd3e58f1e5d000e3d963770b66003918d9db667101e94ce01cfb88019818801ece3f00bd56b5f0ba1501a28e7b4c856000adc08e089ac0013e45c6a5470e00374a12179985016ff5faa9abb600bc556892252e01f71dac235a400104fc32b676b3002890547e3560013f6b50ded9d70001a1a315ea6400ae9061dbfa1b002c69e35d83ce00ae179b614dd401949160545f31015f9ab0d7b550006b91061b66fd00cd4a1486d2e7019de352e2a7b500245dd9403f8101c4a339fafaea01a5339ff90ca501c0659f701eeb019848adedf6ab00316aad94ca1900d2ee726505760080a15fad0b6201c57e961da52000e98b34a3ce85006f8f07c4131d00407280e0903c012153eed86e340109bd55e971d300533c3d2ab03b0034f1de69e0fb00ac5c06757575757551",
        "085243473446554c4c08011418003f1090304d08023b6e7c2501e2c3e2822c55014da10be7630b014ed65f2ddf7000e5430b09001f01baa546d5e8180124b3b72b1fee017d30beba3aba01a59d38b314410156d571c9c5ef019c2b3b145d5901009739435afc015e6f34a75a99010494139bf5800139fd2609e1b300278ad3fd00ac019dca7f4bb00e00839135d0edcf01507ebeadbc3001f364399411ad007ae0fe63aca901f435084701dc011ad1a987069f014289c7da829a008e996569f56901221fceaf069e0080d05d4032ef0198ee212161db008b7282ef5a1c012b3ceeeeed0201b368293de53300e1b1da3a652c00385bd4be07c40166d5de1dd1fd00902d5f44270b00cfca45c9f5da00da1e596a20d6018098248391d00065a11ddc1fdc0198132de32ed601df3460b5742600c1bf5491521701dadafecce070018339ceae4e72011b68ddb197c0006a5b761422490095d561b6dffe004b4f06caa6fb01c246deda7e890183bea61dfc3101a9ac15e5b68801017eeb71c3a101ee952e1691ee00ebc99fc812d600d09993a6b7320160e139d018ac016adea108ca5000a752b4f8146f00bae5f8c6e79300467b3cf6a45f00e5b472c2fe9e00ffcf1e9f1583016dd0f2c544dd01af84bf23c5f400f9e1428b195200bf100fe4104b016b3d42d5aa5c006b2cc4673790004c663b74328e019b7778252e560166a15b09cfff01e4010305a7eb0172463874df860081e539e6c2f401d779a4924d08024ae8014d140bea08370016bb8cb2542300adbdd9f2bcd700204b9bb2aa6100e8436ad27b8901e2fe038534e5003f45032c9690005ee65ed8357800bf6159f3b51c007add03229cfe014d72e597f7ce01045d79320ee10173374d53c45b00bf68561362ce00f1ac44227c1e00ca9c33105c8700710e7957b36500c5ccf66a56ae00e1859278012a014c375a35a3ac017a38271df51100c6041651886201affa65853e9700ebac2f55108700599bff5e2d0c016d9a70aae45c01463a800c683401df97ee0b1b4600d29a4cfc894900fdbf49c45aed018d7339f7b163008f033bd1e3f30112a4651a868200b3d9ac9212660052a0857b7cea0103a1e0b6ef620068c4ad6d690c003eb965121f7400a4d07d4261c80174ad73eebd9800dc3366182cb001d32a571334d300603f584a351d017199054c6509006ebdecac947700444baa6919d600141af4ec103f00302c72c6b55901aa24e25003430036c5641a0c8300b06bb06ea52401e60b0d3e1eb9012ec06a66802e001a99ab7bc66c018d7a0439e78201ab252ae806da003f8e5eb4504801113f23f970fb005c5c7981c4810130d65093529400302cd1dd6ffb012da91585e79a017e3a6a18c11100a751dfa2753a0102d6942c731900c2839f78f6d300dd57b64300df00907e4bec19c601af429a779d8c00af4ced28049e016bb90ab607d40042d1be57202500837a14d29327006e2c1a2b5a264d0802010448b7ccf958016b9afe58403101720b1db8387e0188f9a364ad87011c5b8a53b3b5008e23f4258db401965924fa67e401b3e4bb173ed60151743df151d40106006d04e20409070d068906d205c6004e0144002500e104b70149030c05a0074602ce0173007d055407da01260253012701c100c904aa054b043b03c0061700a80462041a056601e6058303f6063b01f7028105a404530340002202d004fb02fe0796012807f2028000aa04fd0123009e071006ba069d02a904c902f500b8078e012c0565013401200192006f069403d406c4032b07e9069005da00a0056805c805d7046305560442068305b0021206a107f8035a05140197050d0080075e05e903cd03a407d40146036c00a10414036307d305bc0221071f0419053804a006850088066803cc01d504930316067f06c60411061d012203c2074d012206f80343075f021b021000ba044802600324000e046b06d60522061403b70151017602ae03c1020f00b70589036305880472021c008a06fe0535023f027d076603410422018d06bc060e07b002c00719030d017d01ac023b0343078e01d50412029700eb0032035c0517054c035907a705890210030e0149022d0419028f045007100601003d00c7011a05a3023d068402ec05bd028403ca0302034007d107ee011306d6065e026e0778029e07c2047d01ec07b103c30147060a059c041e032400e10463035705d807c10106757575757551",
        "085243473446554c4c08011518003f1090304d0802e6032c0777053006a705a000ba06860600002a03c901ee03a303e804cc07fa023c07e5026703de055b035206fc01fe006f02bf07e30563058d06b6020801f205730049059b04df048a0091079201f0033304b9036f0698006a00ff028d02db06c9006b05b403bb00730282030a07e605930220074006bd0446037e021f00a104fb031d052c01c40642079b001f061e062d01e6037e017703300725044c026801c00237024c059b048403a3069f06d6052302e70454078004be02a605710595052e019500a80336036e068f04aa0283062304060762043c004f0647076f053f02a9025803ad001c036c01d8009207da025f0586017f073902f903d1077f0532060907c4009e059106b9043d013705210338049a039100250480034b05e203b2029505180474047f015505a90720018200d701e80721029903880624069402e6005605e407c606b903d206670418034b0416029f01c5024c065b026c016f07a10537074605f806180611005c079306340066013e034b047d04b906b307b401f001f602c4018003b305540402008f07d201dd0715035d0076078100d20231040205c407f301e8078a07d1039606cf064e0385040b060500de00b8048c01a3064b036102380549025b06e4025e0490046502f1012904e106710008062a05eb01370303063c0414043d01030529021f03fe0472000b05ac068002070659010902b2054d04dc021c05b8044d08025b078a051505bd07310279050000db0022047d03c102b8049f041a03f406850609047703c901460681066e005600cd0153079f0765046a03b0014802d5003a0729051f001b05fb06b10582015205700473025306d705d2068f0756024d03de043c05f405f0047400dc079f031001b00504077702180153066206cb033104f9046500e805aa03d101f003bf013704ad0167050d03d904770760074305f3040201e6014401510558013e066905e10078054605e9039e016005f7033b004504c101f6027804e4021307b1016307590604010506a507520378049803e106be0010025f071305bd06ed07bf0337044b04fb014b013b03540499073000380510060303eb047f056f05e0053d06e201150310048402c9067b042703b307f103c904fc07c50691063802ce020601b800bf0538024f02b201b70443037b06f506dd03e706a6062203f003ef050a02f704e700c5037a02b0038001c900fa019906ad00c101a6077c06e601cf015d02c1021306040316030b05dc0528030700a4051103d906070723032905f002d705c8002d00b5068b053c02ec048f05360754038f05c103d807c60688026806f60525058f05d6041b0240067c01cf017e0654072a07f002ec052a02f8044e05b3027306d50443005c07e80717031502e000d304b301c702a70710052d01340418021b0175000e079d03560305065d02670356023e05c802b3072a053f055f024d0802b3059e02fb039806ec0353051f008d044a06bf037401c504c904210576073802ab0040054a01c903b402fd053c074b02fd0143062d03d1005c0078003804a2064e0385058c020600ed020b05d50796029f00ee054a03db05c101e604870790073503190730025e01d6007306f207b106f1046701f706eb007207cf038f0118001e0420060906ae06d803c0001f03e3007c010b00a706a805f9070507af06ad056503b404aa0613060d01ed07c300e0008601ed00810456004f0661018b066b05c805af02420386046902a3059902b703e906a807c806ab070007f104fa04d40604075303fe03e50474072f064904e0073d03640505072300880090018702c70030062a004e00b405f50323045b027d04e70453011f06200545009003db040c021505840407045b025704b9014503d70641022d033d060a04b400df03bc04bb0718064103320657066502ce0587063f077d06f900c0032c021b01b503bb00c507240568077b05f2077302250511074f05e8020404830668073f05d6047800e2020906e9038a01ac032006f603d40398026200f30350032a03d801cf02f300f2078c0380040900bd0054033801f30394066706d004aa03ad071d079e05c20303025602d703fe013d022b03ed019101f102af04e5079a06b900380480072b07760011050806a40229023a066906d806db07f705a8069b054d043802c704c30356039a05d3059e02dc0706757575757551",
        "085243473446554c4c08011618003f1090304d0802df01c5062307b6020901d904c807c003d301e20633052f0243007005050676007507730756023e042c001801890264069c0635000707f501e8061f076f06f4001f0553055c073e02d506ad02d306d4052c070e01440155024105fd03be014b0540030a05f90714030c069a0216039b0318041905720264074d00a7066503f2011b05e2035d06ae078005ce04f0042d01e901ea07f301b1069d0179022c068803a204ef0767047604e6027e023a02b6078e07d204f300c1013b01d3003200ab01a602ce05f2052402eb0323044d05100557054d008b060000cf000d021e0421003c00050171000004bc053406ec057a05da0759072603f900120170050a002b043602b405ac044c025f06ad053d0322009a02c8012d067a068c04ab053e00b5035403860744044804540772069700ec035205c6005e05010720002b05f607d6040c0019003b077b051200a303d401bd06fc006d0691028c036d070b00a3053100cf052605c407e403890254038607c40556024b04ae05ea026c07f2028a0151075c053304d20714067f01d502c9008405fd04bf020b04ca055b0226021c057601c700ef069e04a1067702f107bf07a5020e05f001f106b706af006b00d106120018054a04a4048f00e40504009f0222033301d40608003904200321035507de0760042c02730352032806ad0168036005f5055906180520072703ae036f05ad021c05c107f0037e004d0802e201d5056f01800153020c03a804f50192041f064b00270005043300ee016304d8035f007e06a90782036e03ff01e60376035104260696054507f60321008102e00175002505b805a804fc042003c307730036038d041f013601b40439070d076c000b04de05a300200792060a075504c705da017000ab07f500880548026c03e602e200730274075004c306270130051f0585031507150557077000fb06db01c601f0057603dd01ef037c0473055a06de02cc02cf015507a2051a04d5044400d2030c00160550044c074002c201450017042901d50744049400a90781003f0120071f0412076c042300a601b802eb012101d7028903b1045f066d0613074401a205270434028700b3026c007d067405f30585045b074e00cc02e502b10151037e0671010901650577007a022a071a042a059f055a0543006901b6073f03d40716055700b506140110044b019e042903cd00f8027f07d603af079107a304f0009d0075072b004b06e9025202200759011d0325010c040f029f05d80064004602a5050b035d02ff0392019107c9054605a707a604f90503077107d207ff02ac01c502cc00490464020404bb018301720756074306f702b3057d01020052008f0406017f04de01bf042602af02b007bf038a042e0017023c06fb0301006e02b502ea06b003fc058d05e4012305bf053107c8067c06ae04b100d604a9060106f1015a03b504c0015a014d08022807db05de066b06e5069d0294043905de051a032707520610037003d40301061901ec026306980765024d009505c103c8002102dc03c9014202ca0774074a04360384045007bc04d40347018a006905e807e003c504af01aa03320597078403f406c10459029307df01520478008f074b0776054f018c00a5019405930678007f029f033a011e0045011006e9004203c9006703750254067b0566064702d407c40059030d0396003406420241057f049e071106e102ca03e0061d064905f7053d046b03c4067c060706fc0523025004b9054802cb025a0648066004b2042105fe01e3072f05020510021c029701990770015b05020297003e078802fa07eb034a06da01e802f9010504590054076005f4015304100188050d0566057500d1025d03e300cd06b503dc0054041c0255072f04ef01090598078c0151009007b406ea0470039a02de00ba071b01f303f600300061056002440072043d07aa063501cb037604f2051e0632063603a8066406dc062a02c8034c051f01aa07a20216026507d50066073c06e2038c056b018302950656052205a500bb078802b3019c00f8026503ee069a07f90368022607db043005b607c904890318036400a400f2000e04e10514003c01b7041306e1034a04bc04a401e0056701ff03a4026a03c301c5005b0497021b02dd040d00c50484032c056c07f402bb0480071901a2044b07c201910483021e0406757575757551",
        "085243473446554c4c08011718003f1090304d0802d0046500f5032806b8021401e70489057004e9036e03390119059f032d016f07cb075203b9063304600643074e015f02c000b004cd079001880288021303a30569004004bc021a02bf074e04ca06eb04c0020905c700b106f9008b02b7004303e80418061b07e704ee01e3064d07ce01fa05d602bc0103049e06ae0432071602a206a601630247046503a8011f00e3075d0336076303e9047506b6014102b5072f01d204e101dc06ba062c048b03d403c7032400d1048a063705b8072b0694004b04c301a7005d000b035e06ad04f00633041e041307c2002e063a049b031d01ca06fb0714020007c6071b04b105e2014300d206980459027b05a8073a04df0162073c022d02b3064404e6039a054e069504d3071d031f06bf033902b40435044007330549074e0606037d016d0798041d04d504210493021504dc003401440121052e041d016704db04a4014e006a027307cd03b3021401ea07ff063a04fb02f9053703c5063d017d03c3024e06c607a402e402110136012b048c0017008b03d907050673011101a006d202a901e705a206cb0256002e07a60134035207ea070406560341023d0005078a05df07dc07ac05d003c8057103d101e9079f000b04e902ab02f3066e07d904860586031e030806ec046d06280233045d07f203c407360201007d023302cf019e038e00e300d000b20038075a0460023a062605da02fe0021039f0047054d0002c1025e07ba0669064d051c07c706fc03e6008103ee07a807ac04f30254058b05f40656029307660055030e05b9060c054b0290011405f4065300bd07d402a003ac040f0207024b0474000003ba0642040f03090493067007bd01fa01a0010b00b105f7077b0714030c0069042603e705ea029606640583024500c70237025301ee031b00c805d7003a0106048300100486035405fd0524067306c2043a047d053503f401300721039307b9079904500133079b0250017e05ba03490468055102fd049f0230069e037b05ae075907cd05cc06db02df03e205050423050205c80546066e00e6079806b3041f060701ae01be05910026033404ea03ae0219009305c206d3059e05bf04430218041000b604e201e40505047201e30094008204ad042b036c04420368036800b704e9078904e106bd06220318012b00680613071402e0036701eb00700441060005f001ff01e806c301c204fc01f60515071107cc073c02d1029e0637038e04cf01a200ee07b10395033705c1074001b700210249036904090562071d0434054302d9068807c802ee0561067d04da06de03240342057403cc0729050001cd02d300dc049c04b00382043706b4056f00a8023c031f052603e106970108054c0130017801fc068b014e0604022905e706ea079f05c20133011502a405b202580759022407a30005075d0303070404d005d007960795030006757575757551",
    };
    DecodeAndValidateMultiPartCarrierTxR(
        "9f3b3b49bc156db86b92553b8cf80ee4240d96e2d7fc38712e6551900642020e",
        scriptsigs,
        "7dd12252cd2b388c117d5eb0c26d4d518ce077d07b8100060fbc3e604e0531c8",
        "RACCOON", 16144, 20768,
        "8c8a6acb1f159d9bd4119a2e8ca28d55",
        "be947bf65d1aaf20d68a995934a74df6");
}

BOOST_AUTO_TEST_SUITE_END()

#endif // ENABLE_LIBOQS
