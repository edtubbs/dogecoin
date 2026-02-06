// Copyright (c) 2013-2016 The Bitcoin Core developers
// Copyright (c) 2021-2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "crypto/common.h"
#include "crypto/hmac_sha512.h"
#include "crypto/sha256.h"
#include "pubkey.h"

#if defined(USE_AVX2_8WAY)
// Forward declaration of the 8-way processing function
extern "C" void sha256_process_x8_avx2_wrapper(uint32_t* states[8], const unsigned char* data[8], size_t blocks);

// Helper to compute double-SHA256 for 8 pairs of hashes (merkle tree node computation)
// This is optimized for the common case of hashing two 32-byte values (64 bytes total)
void CHash256Batch::Finalize8(const unsigned char* inputs[8], 
                               const size_t input_lengths[8],
                               unsigned char* outputs[8],
                               size_t count)
{
    if (count == 0 || count > BATCH_SIZE) return;
    
    // SHA256 initial hash values
    static const uint32_t sha256_init[8] = {
        0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul,
        0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul
    };
    
    // Check if all inputs are 64 bytes (merkle tree case - two 32-byte hashes)
    bool all_64_bytes = true;
    for (size_t i = 0; i < count; i++) {
        if (input_lengths[i] != 64) {
            all_64_bytes = false;
            break;
        }
    }
    
    // Fast path for 64-byte inputs (merkle tree case)
    if (all_64_bytes) {
        // Allocate storage for states
        uint32_t states_storage[8][8];
        uint32_t* states[8];
        unsigned char intermediate[8][32];
        unsigned char padded_intermediate[8][64];
        
        // Initialize states for first SHA256 pass
        for (size_t i = 0; i < 8; i++) {
            states[i] = states_storage[i];
            memcpy(states[i], sha256_init, 32);
        }
        
        // Setup data pointers for first pass (pad unused lanes with first input)
        const unsigned char* data_ptrs[8];
        for (size_t i = 0; i < 8; i++) {
            data_ptrs[i] = (i < count) ? inputs[i] : inputs[0];
        }
        
        // First SHA256 pass: Process 64-byte inputs (exactly 1 block)
        sha256_process_x8_avx2_wrapper(states, data_ptrs, 1);
        
        // Extract intermediate hashes (convert from state to bytes)
        for (size_t i = 0; i < count; i++) {
            for (int j = 0; j < 8; j++) {
                WriteBE32(intermediate[i] + j * 4, states[i][j]);
            }
        }
        
        // Prepare for second SHA256 pass: pad intermediate hashes
        // SHA256 padding for 32-byte input: data + 0x80 + zeros + length (256 bits)
        for (size_t i = 0; i < count; i++) {
            memcpy(padded_intermediate[i], intermediate[i], 32);
            padded_intermediate[i][32] = 0x80;  // Padding bit
            memset(padded_intermediate[i] + 33, 0, 31 - 8);  // Zeros
            // Length in bits = 256 bits = 0x100 (big-endian at end)
            WriteBE64(padded_intermediate[i] + 56, 256);
        }
        
        // Reset states for second SHA256 pass
        for (size_t i = 0; i < 8; i++) {
            memcpy(states[i], sha256_init, 32);
            data_ptrs[i] = padded_intermediate[(i < count) ? i : 0];
        }
        
        // Second SHA256 pass: Process padded intermediate hashes (1 block)
        sha256_process_x8_avx2_wrapper(states, data_ptrs, 1);
        
        // Extract final hashes
        for (size_t i = 0; i < count; i++) {
            for (int j = 0; j < 8; j++) {
                WriteBE32(outputs[i] + j * 4, states[i][j]);
            }
        }
    } else {
        // Fallback to sequential processing for non-64-byte inputs
        for (size_t i = 0; i < count; i++) {
            CHash256().Write(inputs[i], input_lengths[i]).Finalize(outputs[i]);
        }
    }
}
#endif // USE_AVX2_8WAY


inline uint32_t ROTL32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash)
{
    // The following is MurmurHash3 (x86_32), see http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
    uint32_t h1 = nHashSeed;
    if (vDataToHash.size() > 0)
    {
        const uint32_t c1 = 0xcc9e2d51;
        const uint32_t c2 = 0x1b873593;

        const int nblocks = vDataToHash.size() / 4;

        //----------
        // body
        const uint8_t* blocks = &vDataToHash[0] + nblocks * 4;

        for (int i = -nblocks; i; i++) {
            uint32_t k1 = ReadLE32(blocks + i*4);

            k1 *= c1;
            k1 = ROTL32(k1, 15);
            k1 *= c2;

            h1 ^= k1;
            h1 = ROTL32(h1, 13);
            h1 = h1 * 5 + 0xe6546b64;
        }

        //----------
        // tail
        const uint8_t* tail = (const uint8_t*)(&vDataToHash[0] + nblocks * 4);

        uint32_t k1 = 0;

        switch (vDataToHash.size() & 3) {
        case 3:
            k1 ^= tail[2] << 16;
            // Falls through
        case 2:
            k1 ^= tail[1] << 8;
            // Falls through
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = ROTL32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
        }
    }

    //----------
    // finalization
    h1 ^= vDataToHash.size();
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

void BIP32Hash(const ChainCode &chainCode, unsigned int nChild, unsigned char header, const unsigned char data[32], unsigned char output[64])
{
    unsigned char num[4];
    num[0] = (nChild >> 24) & 0xFF;
    num[1] = (nChild >> 16) & 0xFF;
    num[2] = (nChild >>  8) & 0xFF;
    num[3] = (nChild >>  0) & 0xFF;
    CHMAC_SHA512(chainCode.begin(), chainCode.size()).Write(&header, 1).Write(data, 32).Write(num, 4).Finalize(output);
}

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define SIPROUND do { \
    v0 += v1; v1 = ROTL(v1, 13); v1 ^= v0; \
    v0 = ROTL(v0, 32); \
    v2 += v3; v3 = ROTL(v3, 16); v3 ^= v2; \
    v0 += v3; v3 = ROTL(v3, 21); v3 ^= v0; \
    v2 += v1; v1 = ROTL(v1, 17); v1 ^= v2; \
    v2 = ROTL(v2, 32); \
} while (0)

CSipHasher::CSipHasher(uint64_t k0, uint64_t k1)
{
    v[0] = 0x736f6d6570736575ULL ^ k0;
    v[1] = 0x646f72616e646f6dULL ^ k1;
    v[2] = 0x6c7967656e657261ULL ^ k0;
    v[3] = 0x7465646279746573ULL ^ k1;
    count = 0;
    tmp = 0;
}

CSipHasher& CSipHasher::Write(uint64_t data)
{
    uint64_t v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];

    assert(count % 8 == 0);

    v3 ^= data;
    SIPROUND;
    SIPROUND;
    v0 ^= data;

    v[0] = v0;
    v[1] = v1;
    v[2] = v2;
    v[3] = v3;

    count += 8;
    return *this;
}

CSipHasher& CSipHasher::Write(const unsigned char* data, size_t size)
{
    uint64_t v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
    uint64_t t = tmp;
    int c = count;

    while (size--) {
        t |= ((uint64_t)(*(data++))) << (8 * (c % 8));
        c++;
        if ((c & 7) == 0) {
            v3 ^= t;
            SIPROUND;
            SIPROUND;
            v0 ^= t;
            t = 0;
        }
    }

    v[0] = v0;
    v[1] = v1;
    v[2] = v2;
    v[3] = v3;
    count = c;
    tmp = t;

    return *this;
}

uint64_t CSipHasher::Finalize() const
{
    uint64_t v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];

    uint64_t t = tmp | (((uint64_t)count) << 56);

    v3 ^= t;
    SIPROUND;
    SIPROUND;
    v0 ^= t;
    v2 ^= 0xFF;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}

uint64_t SipHashUint256(uint64_t k0, uint64_t k1, const uint256& val)
{
    /* Specialized implementation for efficiency */
    uint64_t d = val.GetUint64(0);

    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1 ^ d;

    SIPROUND;
    SIPROUND;
    v0 ^= d;
    d = val.GetUint64(1);
    v3 ^= d;
    SIPROUND;
    SIPROUND;
    v0 ^= d;
    d = val.GetUint64(2);
    v3 ^= d;
    SIPROUND;
    SIPROUND;
    v0 ^= d;
    d = val.GetUint64(3);
    v3 ^= d;
    SIPROUND;
    SIPROUND;
    v0 ^= d;
    v3 ^= ((uint64_t)4) << 59;
    SIPROUND;
    SIPROUND;
    v0 ^= ((uint64_t)4) << 59;
    v2 ^= 0xFF;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}
