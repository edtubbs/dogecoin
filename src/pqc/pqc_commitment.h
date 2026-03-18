// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_PQC_COMMITMENT_H
#define DOGECOIN_PQC_COMMITMENT_H

#include "script/script.h"
#include "primitives/transaction.h"
#include "uint256.h"

#include <stdint.h>
#include <vector>

enum class PQCCommitmentType {
    FALCON512,
    DILITHIUM2,
};

static const unsigned int PQC_COMMITMENT_BYTES = 32;

/** SHA256(public_key || signature). */
bool PQCComputeCommitment(const std::vector<unsigned char>& public_key,
                          const std::vector<unsigned char>& signature,
                          uint256& commitment_out);

/** Build canonical OP_RETURN commitment script:
 * OP_RETURN, 0x24, TAG4, COMMIT32.
 */
bool PQCBuildCommitmentScript(PQCCommitmentType type,
                              const uint256& commitment,
                              CScript& script_out);

/** Parse canonical OP_RETURN commitment script:
 * OP_RETURN, 0x24, TAG4, COMMIT32.
 */
bool PQCExtractCommitment(const CScript& script,
                          PQCCommitmentType& type_out,
                          uint256& commitment_out);

const char* PQCCommitmentTypeToString(PQCCommitmentType type);

bool PQCExtractCommitmentFromTx(const CTransaction& tx,
                                PQCCommitmentType& type_out,
                                uint256& commitment_out,
                                uint32_t& output_index_out);

#endif // DOGECOIN_PQC_COMMITMENT_H
