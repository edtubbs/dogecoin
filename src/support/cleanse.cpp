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
     * This uses a C++11 atomic fence which is portable across all platforms.
     */
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
