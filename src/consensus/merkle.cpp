// Copyright (c) 2015-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "merkle.h"
#include "hash.h"
#include "utilstrencodings.h"

/*     WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. Assuming no double-SHA256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
*/

/* This implements a constant-space merkle root/path calculator, limited to 2^32 leaves. */
static void MerkleComputation(const std::vector<uint256>& leaves, uint256* proot, bool* pmutated, uint32_t branchpos, std::vector<uint256>* pbranch) {
    if (pbranch) pbranch->clear();
    if (leaves.size() == 0) {
        if (pmutated) *pmutated = false;
        if (proot) *proot = uint256();
        return;
    }
    bool mutated = false;
    // count is the number of leaves processed so far.
    uint32_t count = 0;
    // inner is an array of eagerly computed subtree hashes, indexed by tree
    // level (0 being the leaves).
    // For example, when count is 25 (11001 in binary), inner[4] is the hash of
    // the first 16 leaves, inner[3] of the next 8 leaves, and inner[0] equal to
    // the last leaf. The other inner entries are undefined.
    uint256 inner[32];
    // Which position in inner is a hash that depends on the matching leaf.
    int matchlevel = -1;
    // First process all leaves into 'inner' values.
    while (count < leaves.size()) {
        uint256 h = leaves[count];
        bool matchh = count == branchpos;
        count++;
        int level;
        // For each of the lower bits in count that are 0, do 1 step. Each
        // corresponds to an inner value that existed before processing the
        // current leaf, and each needs a hash to combine it.
        for (level = 0; !(count & (((uint32_t)1) << level)); level++) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            mutated |= (inner[level] == h);
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
        }
        // Store the resulting hash at inner position level.
        inner[level] = h;
        if (matchh) {
            matchlevel = level;
        }
    }
    // Do a final 'sweep' over the rightmost branch of the tree to process
    // odd levels, and reduce everything to a single top value.
    // Level is the level (counted from the bottom) up to which we've sweeped.
    int level = 0;
    // As long as bit number level in count is zero, skip it. It means there
    // is nothing left at this level.
    while (!(count & (((uint32_t)1) << level))) {
        level++;
    }
    uint256 h = inner[level];
    bool matchh = matchlevel == level;
    while (count != (((uint32_t)1) << level)) {
        // If we reach this point, h is an inner value that is not the top.
        // We combine it with itself (Bitcoin's special rule for odd levels in
        // the tree) to produce a higher level one.
        if (pbranch && matchh) {
            pbranch->push_back(h);
        }
        CHash256().Write(h.begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
        // Increment count to the value it would have if two entries at this
        // level had existed.
        count += (((uint32_t)1) << level);
        level++;
        // And propagate the result upwards accordingly.
        while (!(count & (((uint32_t)1) << level))) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
            level++;
        }
    }
    // Return result.
    if (pmutated) *pmutated = mutated;
    if (proot) *proot = h;
}

uint256 ComputeMerkleRoot(const std::vector<uint256>& leaves, bool* mutated) {
#if defined(USE_AVX2_8WAY)
    // Optimized path for 8-way parallel processing when we have enough leaves
    // and don't need branch computation
    if (leaves.size() >= 16) {
        // Use a level-by-level approach that's easier to batch
        std::vector<uint256> current_level = leaves;
        bool level_mutated = false;
        
        while (current_level.size() > 1) {
            std::vector<uint256> next_level;
            next_level.reserve((current_level.size() + 1) / 2);
            
            size_t i = 0;
            // Process pairs in batches of 8
            while (i + 15 < current_level.size()) {
                // We have at least 8 pairs to process
                const unsigned char* batch_inputs[8];
                size_t batch_lengths[8];
                unsigned char* batch_outputs[8];
                unsigned char batch_input_storage[8][64];
                uint256 batch_results[8];
                
                // Prepare 8 pairs for batch processing
                for (size_t j = 0; j < 8; j++) {
                    size_t idx = i + j * 2;
                    // Check for mutation (left == right)
                    if (current_level[idx] == current_level[idx + 1]) {
                        level_mutated = true;
                    }
                    // Concatenate two hashes
                    memcpy(batch_input_storage[j], current_level[idx].begin(), 32);
                    memcpy(batch_input_storage[j] + 32, current_level[idx + 1].begin(), 32);
                    batch_inputs[j] = batch_input_storage[j];
                    batch_lengths[j] = 64;
                    batch_outputs[j] = batch_results[j].begin();
                }
                
                // Process batch
                CHash256Batch::Finalize8(batch_inputs, batch_lengths, batch_outputs, 8);
                
                // Add results to next level
                for (size_t j = 0; j < 8; j++) {
                    next_level.push_back(batch_results[j]);
                }
                
                i += 16;  // Processed 8 pairs (16 hashes)
            }
            
            // Process remaining pairs sequentially
            while (i < current_level.size()) {
                uint256 result;
                if (i + 1 < current_level.size()) {
                    // Normal pair
                    if (current_level[i] == current_level[i + 1]) {
                        level_mutated = true;
                    }
                    CHash256().Write(current_level[i].begin(), 32)
                             .Write(current_level[i + 1].begin(), 32)
                             .Finalize(result.begin());
                    i += 2;
                } else {
                    // Odd one out - hash with itself
                    CHash256().Write(current_level[i].begin(), 32)
                             .Write(current_level[i].begin(), 32)
                             .Finalize(result.begin());
                    i += 1;
                }
                next_level.push_back(result);
            }
            
            current_level = std::move(next_level);
        }
        
        if (mutated) *mutated = level_mutated;
        return current_level.empty() ? uint256() : current_level[0];
    }
#endif
    
    // Fallback to original algorithm for small trees or when 8-way is not available
    uint256 hash;
    MerkleComputation(leaves, &hash, mutated, -1, NULL);
    return hash;
}

std::vector<uint256> ComputeMerkleBranch(const std::vector<uint256>& leaves, uint32_t position) {
    std::vector<uint256> ret;
    MerkleComputation(leaves, NULL, NULL, position, &ret);
    return ret;
}

uint256 ComputeMerkleRootFromBranch(const uint256& leaf, const std::vector<uint256>& vMerkleBranch, uint32_t nIndex) {
    uint256 hash = leaf;
    for (std::vector<uint256>::const_iterator it = vMerkleBranch.begin(); it != vMerkleBranch.end(); ++it) {
        if (nIndex & 1) {
            hash = Hash(BEGIN(*it), END(*it), BEGIN(hash), END(hash));
        } else {
            hash = Hash(BEGIN(hash), END(hash), BEGIN(*it), END(*it));
        }
        nIndex >>= 1;
    }
    return hash;
}

uint256 BlockMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetHash();
    }
    return ComputeMerkleRoot(leaves, mutated);
}

uint256 BlockWitnessMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    leaves[0].SetNull(); // The witness hash of the coinbase is 0.
    for (size_t s = 1; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetWitnessHash();
    }
    return ComputeMerkleRoot(leaves, mutated);
}

std::vector<uint256> BlockMerkleBranch(const CBlock& block, uint32_t position)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetHash();
    }
    return ComputeMerkleBranch(leaves, position);
}
