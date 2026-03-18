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

BOOST_AUTO_TEST_CASE(pqc_extract_rejects_noncanonical_script)
{
    CScript script = CScript() << OP_RETURN << std::vector<unsigned char>(36, 0x00);
    PQCCommitmentType parsed_type;
    uint256 parsed_commitment;
    BOOST_CHECK(!PQCExtractCommitment(script, parsed_type, parsed_commitment));
}

BOOST_AUTO_TEST_SUITE_END()
