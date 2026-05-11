/* sha2.h compat shim — exposes the C-shaped sha256_raw / hmac_sha256 /
 * hmac_sha512 API the libdogecoin Raccoon-G port expects, implemented in
 * raccoon_g/dogecoin_compat.cpp on top of dogecoin-core's CSHA256 /
 * CHMAC_SHA256 / CHMAC_SHA512 classes.
 */
#ifndef DOGECOIN_RACCOON_G_COMPAT_SHA2_H
#define DOGECOIN_RACCOON_G_COMPAT_SHA2_H

#include <stddef.h>
#include <stdint.h>

#include <dogecoin/dogecoin.h>

LIBDOGECOIN_BEGIN_DECL

#ifndef SHA256_DIGEST_LENGTH
#  define SHA256_DIGEST_LENGTH 32
#endif
#ifndef SHA512_DIGEST_LENGTH
#  define SHA512_DIGEST_LENGTH 64
#endif

void sha256_raw(const uint8_t* msg, size_t msglen, uint8_t out[SHA256_DIGEST_LENGTH]);

/* HMAC-SHA256 ---------------------------------------------------------- */
typedef struct hmac_sha256_context {
    void* impl;  /* opaque CHMAC_SHA256* */
} hmac_sha256_context;

void hmac_sha256_init(hmac_sha256_context* hctx, const uint8_t* key, uint32_t keylen);
void hmac_sha256_write(hmac_sha256_context* hctx, const uint8_t* msg, uint32_t msglen);
void hmac_sha256_finalize(hmac_sha256_context* hctx, uint8_t* hmac);
void hmac_sha256(const uint8_t* key, size_t keylen,
                 const uint8_t* msg, size_t msglen,
                 uint8_t* hmac);

/* HMAC-SHA512 ---------------------------------------------------------- */
typedef struct hmac_sha512_context {
    void* impl;  /* opaque CHMAC_SHA512* */
} hmac_sha512_context;

void hmac_sha512_init(hmac_sha512_context* hctx, const uint8_t* key, uint32_t keylen);
void hmac_sha512_write(hmac_sha512_context* hctx, const uint8_t* msg, uint32_t msglen);
void hmac_sha512_finalize(hmac_sha512_context* hctx, uint8_t* hmac);
void hmac_sha512(const uint8_t* key, size_t keylen,
                 const uint8_t* msg, size_t msglen,
                 uint8_t* hmac);

LIBDOGECOIN_END_DECL

#endif /* DOGECOIN_RACCOON_G_COMPAT_SHA2_H */
