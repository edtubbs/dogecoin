// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cleanse.h"

#include <cstring>
#include <atomic>

void memory_cleanse(void *ptr, size_t len)
{
    /* Secure memory cleansing without OpenSSL.
     * As best as we can, scrub data from memory in a way that the compiler
     * won't optimize away.
     */
    std::memset(ptr, 0, len);
    
    /* Memory barrier that prevents the compiler from optimizing out the memset.
     * We use both an atomic fence and a volatile access to ensure maximum
     * portability and effectiveness across all compilers and platforms.
     */
    std::atomic_signal_fence(std::memory_order_seq_cst);
    
    /* Additional protection: make the compiler think we're using the memory
     * by doing a volatile read. This forces the compiler to treat the memory
     * as if it might be accessed later.
     */
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
}
