// Compatibility implementation for the libdogecoin Raccoon-G port.
//
// Implements the C-shaped sha256_raw / hmac_sha256 / hmac_sha512 API and the
// dogecoin_random_bytes thunk that the upstream port (kept verbatim under
// src/raccoon_g/) expects, on top of this tree's existing C++ crypto.

#include "raccoon_g/dogecoin/sha2.h"
#include "raccoon_g/dogecoin/random.h"

#include "crypto/sha256.h"
#include "crypto/hmac_sha256.h"
#include "crypto/hmac_sha512.h"
#include "random.h"

#include <new>
#include <stdlib.h>
#include <string.h>

// ---- SHA-256 raw --------------------------------------------------------
extern "C" void sha256_raw(const uint8_t* msg, size_t msglen, uint8_t out[SHA256_DIGEST_LENGTH])
{
    CSHA256 h;
    h.Write(msg, msglen);
    h.Finalize(out);
}

// ---- HMAC-SHA-256 -------------------------------------------------------
extern "C" void hmac_sha256_init(hmac_sha256_context* hctx, const uint8_t* key, uint32_t keylen)
{
    hctx->impl = new CHMAC_SHA256(key, keylen);
}

extern "C" void hmac_sha256_write(hmac_sha256_context* hctx, const uint8_t* msg, uint32_t msglen)
{
    static_cast<CHMAC_SHA256*>(hctx->impl)->Write(msg, msglen);
}

extern "C" void hmac_sha256_finalize(hmac_sha256_context* hctx, uint8_t* hmac)
{
    CHMAC_SHA256* h = static_cast<CHMAC_SHA256*>(hctx->impl);
    h->Finalize(hmac);
    delete h;
    hctx->impl = nullptr;
}

extern "C" void hmac_sha256(const uint8_t* key, size_t keylen,
                            const uint8_t* msg, size_t msglen,
                            uint8_t* hmac)
{
    CHMAC_SHA256 h(key, keylen);
    h.Write(msg, msglen);
    h.Finalize(hmac);
}

// ---- HMAC-SHA-512 -------------------------------------------------------
extern "C" void hmac_sha512_init(hmac_sha512_context* hctx, const uint8_t* key, uint32_t keylen)
{
    hctx->impl = new CHMAC_SHA512(key, keylen);
}

extern "C" void hmac_sha512_write(hmac_sha512_context* hctx, const uint8_t* msg, uint32_t msglen)
{
    static_cast<CHMAC_SHA512*>(hctx->impl)->Write(msg, msglen);
}

extern "C" void hmac_sha512_finalize(hmac_sha512_context* hctx, uint8_t* hmac)
{
    CHMAC_SHA512* h = static_cast<CHMAC_SHA512*>(hctx->impl);
    h->Finalize(hmac);
    delete h;
    hctx->impl = nullptr;
}

extern "C" void hmac_sha512(const uint8_t* key, size_t keylen,
                            const uint8_t* msg, size_t msglen,
                            uint8_t* hmac)
{
    CHMAC_SHA512 h(key, keylen);
    h.Write(msg, msglen);
    h.Finalize(hmac);
}

// ---- random -------------------------------------------------------------
extern "C" dogecoin_bool dogecoin_random_bytes(uint8_t* buf, uint32_t len, uint8_t /*update_seed*/)
{
    if (buf == nullptr) return 0;
    GetStrongRandBytes(buf, static_cast<int>(len));
    return 1;
}
