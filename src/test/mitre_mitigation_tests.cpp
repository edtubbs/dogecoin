// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Unit tests for MITRE ATT&CK mitigations and Least Authority audit findings

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "net.h"
#include "net_processing.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/crypter.h"
#include "wallet/wallet.h"
#endif

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

#ifndef WIN32
#include <sys/resource.h>
#endif

BOOST_FIXTURE_TEST_SUITE(mitre_mitigation_tests, BasicTestingSetup)

#ifdef ENABLE_WALLET
// T1110: Verify increased KDF default iteration count
BOOST_AUTO_TEST_CASE(wallet_kdf_minimum_iterations)
{
    CMasterKey masterKey;
    // MITRE T1110: Default iterations should be at least 100000
    BOOST_CHECK(masterKey.nDeriveIterations >= 100000);
}

// T1110: Verify KDF still produces valid encryption/decryption with new defaults
BOOST_AUTO_TEST_CASE(wallet_kdf_encrypt_decrypt)
{
    CCrypter crypter;
    std::vector<unsigned char> vchSalt = ParseHex("0000deadbeef0000");
    BOOST_CHECK(vchSalt.size() == WALLET_CRYPTO_SALT_SIZE);

    // Use the new default iteration count
    CMasterKey masterKey;
    BOOST_CHECK(crypter.SetKeyFromPassphrase("test_passphrase", vchSalt,
                masterKey.nDeriveIterations, 0));

    // Test encrypt and decrypt roundtrip
    CKeyingMaterial vchPlaintext(32, 0x42);
    std::vector<unsigned char> vchCiphertext;
    BOOST_CHECK(crypter.Encrypt(vchPlaintext, vchCiphertext));

    CKeyingMaterial vchDecrypted;
    BOOST_CHECK(crypter.Decrypt(vchCiphertext, vchDecrypted));
    BOOST_CHECK(vchPlaintext == vchDecrypted);
}

// LA-S3: Verify wallet RBF is enabled by default
BOOST_AUTO_TEST_CASE(wallet_rbf_default_enabled)
{
    BOOST_CHECK_EQUAL(DEFAULT_WALLET_RBF, true);
}
#endif // ENABLE_WALLET

// T1499: Verify message rate limiting constants are reasonable
BOOST_AUTO_TEST_CASE(peer_message_rate_constants)
{
    // Rate window should be positive
    BOOST_CHECK(PEER_MSG_RATE_WINDOW > 0);
    // Max rate should be reasonable (not too low to break normal operation)
    BOOST_CHECK(MAX_PEER_MSG_RATE >= 100);
    // DoS score should be meaningful but not instant-ban
    BOOST_CHECK(PEER_MSG_RATE_DOS_SCORE > 0);
    BOOST_CHECK(PEER_MSG_RATE_DOS_SCORE < DEFAULT_BANSCORE_THRESHOLD);
}

#ifndef WIN32
// LA-B: Verify core dumps can be disabled via RLIMIT_CORE
BOOST_AUTO_TEST_CASE(core_dumps_can_be_disabled)
{
    struct rlimit rl = {0, 0};
    // Verify we can set RLIMIT_CORE to 0 (this is what init.cpp does at startup)
    BOOST_CHECK(setrlimit(RLIMIT_CORE, &rl) == 0);
    struct rlimit rl_check;
    BOOST_CHECK(getrlimit(RLIMIT_CORE, &rl_check) == 0);
    BOOST_CHECK_EQUAL(rl_check.rlim_cur, 0U);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
