// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pqc/pqc_commitment.h"

#include "crypto/sha256.h"
#include "script/script.h"

#include <string.h>

static const unsigned char PQC_COMMITMENT_OP_RETURN = OP_RETURN;
static const unsigned char PQC_COMMITMENT_PUSH_LEN = 36;
static const unsigned char PQC_TAG_FALCON[4] = {'F', 'L', 'C', '1'};
static const unsigned char PQC_TAG_DILITHIUM[4] = {'D', 'I', 'L', '2'};
static const unsigned int PQC_TAG_BYTES = 4;
static const unsigned int PQC_SCRIPT_BYTES = 2 + PQC_TAG_BYTES + PQC_COMMITMENT_BYTES;

static const unsigned char* GetTagForType(PQCCommitmentType type)
{
    switch (type) {
    case PQCCommitmentType::FALCON512:
        return PQC_TAG_FALCON;
    case PQCCommitmentType::DILITHIUM2:
        return PQC_TAG_DILITHIUM;
    default:
        return nullptr;
    }
}

bool PQCComputeCommitment(const std::vector<unsigned char>& public_key,
                          const std::vector<unsigned char>& signature,
                          uint256& commitment_out)
{
    if (public_key.empty() || signature.empty()) {
        return false;
    }

    CSHA256()
        .Write(public_key.data(), public_key.size())
        .Write(signature.data(), signature.size())
        .Finalize(commitment_out.begin());
    return true;
}

bool PQCBuildCommitmentScript(PQCCommitmentType type,
                              const uint256& commitment,
                              CScript& script_out)
{
    const unsigned char* tag = GetTagForType(type);
    if (tag == nullptr) {
        return false;
    }

    std::vector<unsigned char> raw(PQC_SCRIPT_BYTES);
    raw[0] = PQC_COMMITMENT_OP_RETURN;
    raw[1] = PQC_COMMITMENT_PUSH_LEN;
    memcpy(&raw[2], tag, PQC_TAG_BYTES);
    memcpy(&raw[2 + PQC_TAG_BYTES], commitment.begin(), PQC_COMMITMENT_BYTES);
    script_out = CScript(raw.begin(), raw.end());
    return true;
}

bool PQCExtractCommitment(const CScript& script,
                          PQCCommitmentType& type_out,
                          uint256& commitment_out)
{
    if (script.size() != PQC_SCRIPT_BYTES) {
        return false;
    }

    if (script[0] != PQC_COMMITMENT_OP_RETURN || script[1] != PQC_COMMITMENT_PUSH_LEN) {
        return false;
    }

    const unsigned char* data = &script[2];
    if (memcmp(data, PQC_TAG_FALCON, PQC_TAG_BYTES) == 0) {
        type_out = PQCCommitmentType::FALCON512;
    } else if (memcmp(data, PQC_TAG_DILITHIUM, PQC_TAG_BYTES) == 0) {
        type_out = PQCCommitmentType::DILITHIUM2;
    } else {
        return false;
    }

    std::vector<unsigned char> commitment_bytes(PQC_COMMITMENT_BYTES);
    memcpy(commitment_bytes.data(), data + PQC_TAG_BYTES, PQC_COMMITMENT_BYTES);
    commitment_out = uint256(commitment_bytes);
    return true;
}

const char* PQCCommitmentTypeToString(PQCCommitmentType type)
{
    switch (type) {
    case PQCCommitmentType::FALCON512:
        return "FALCON512/FLC1";
    case PQCCommitmentType::DILITHIUM2:
        return "DILITHIUM2/DIL2";
    default:
        return "UNKNOWN";
    }
}

bool ParsePQCCommitmentType(const std::string& type, PQCCommitmentType& type_out)
{
    if (type == "falcon512" || type == "FALCON512" || type == "flc1" || type == "FLC1") {
        type_out = PQCCommitmentType::FALCON512;
        return true;
    }
    if (type == "dilithium2" || type == "DILITHIUM2" || type == "dil2" || type == "DIL2") {
        type_out = PQCCommitmentType::DILITHIUM2;
        return true;
    }
    return false;
}

bool PQCExtractCommitmentFromTx(const CTransaction& tx,
                                PQCCommitmentType& type_out,
                                uint256& commitment_out,
                                uint32_t& output_index_out)
{
    for (uint32_t i = 0; i < tx.vout.size(); ++i) {
        if (PQCExtractCommitment(tx.vout[i].scriptPubKey, type_out, commitment_out)) {
            output_index_out = i;
            return true;
        }
    }
    return false;
}
