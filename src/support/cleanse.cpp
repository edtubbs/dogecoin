// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cleanse.h"

#include <cstring>

void memory_cleanse(void *ptr, size_t len)
{
    /* Secure memory cleansing without OpenSSL.
     * As best as we can, scrub data from memory in a way that the compiler
     * won't optimize away. This is somewhat similar to the implementation in
     * libsodium and OpenSSL's OPENSSL_cleanse.
     */
    std::memset(ptr, 0, len);
    
    /* Memory barrier that scares the compiler away from optimizing out the memset.
     * Prevents the compiler from assuming that the memory is unused after this point.
     *
     * This is a portable approach that works on all platforms. We use a volatile
     * pointer to force the compiler to treat the memory as if it's being read.
     */
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
}
