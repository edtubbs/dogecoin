// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pqc/pqc_commitment.h"

#include "crypto/sha256.h"
#include "hash.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "support/experimental.h"

#include <string.h>
#include <algorithm>

#if ENABLE_LIBOQS
EXPERIMENTAL_FEATURE
#include <oqs/oqs.h>
#endif

static const unsigned char PQC_COMMITMENT_OP_RETURN = OP_RETURN;
static const unsigned char PQC_COMMITMENT_PUSH_LEN = 36;
static const unsigned char PQC_TAG_FALCON[4] = {'F', 'L', 'C', '1'};
static const unsigned char PQC_TAG_DILITHIUM[4] = {'D', 'I', 'L', '2'};
#ifdef ENABLE_LIBOQS_RACCOON
static const unsigned char PQC_TAG_RACCOON[4] = {'R', 'C', 'G', '4'};
#endif
static const unsigned int PQC_TAG_BYTES = 4;
static const unsigned int PQC_SCRIPT_BYTES = 2 + PQC_TAG_BYTES + PQC_COMMITMENT_BYTES;

static const unsigned char* GetTagForType(PQCCommitmentType type)
{
    switch (type) {
    case PQCCommitmentType::FALCON512:
        return PQC_TAG_FALCON;
    case PQCCommitmentType::DILITHIUM2:
        return PQC_TAG_DILITHIUM;
#ifdef ENABLE_LIBOQS_RACCOON
    case PQCCommitmentType::RACCOONG44:
        return PQC_TAG_RACCOON;
#endif
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
#ifdef ENABLE_LIBOQS_RACCOON
    } else if (memcmp(data, PQC_TAG_RACCOON, PQC_TAG_BYTES) == 0) {
        type_out = PQCCommitmentType::RACCOONG44;
#endif
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
#ifdef ENABLE_LIBOQS_RACCOON
    case PQCCommitmentType::RACCOONG44:
        return "RACCOONG44/RCG4";
#endif
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
#ifdef ENABLE_LIBOQS_RACCOON
    if (type == "raccoong44" || type == "RACCOONG44" || type == "raccoong" || type == "RACCOONG" || type == "rcg4" || type == "RCG4") {
        type_out = PQCCommitmentType::RACCOONG44;
        return true;
    }
#endif
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

// --- P2SH Data-Carrier (Carrier Mode) Implementation ---

/** Carrier redeemScript: OP_DROP OP_DROP OP_DROP OP_DROP OP_DROP OP_TRUE (6 bytes).
 *  Cleans TAG8, HDR8, CHUNK0, CHUNK1, CHUNK2 from stack and pushes true.
 */
bool PQCBuildCarrierRedeemScript(CScript& script_out)
{
    script_out = CScript();
    script_out << OP_DROP << OP_DROP << OP_DROP << OP_DROP << OP_DROP << OP_TRUE;
    return true;
}

bool PQCBuildCarrierScriptPubKey(CScript& script_out)
{
    CScript redeemScript;
    if (!PQCBuildCarrierRedeemScript(redeemScript)) {
        return false;
    }
    script_out = GetScriptForDestination(CScriptID(redeemScript));
    return true;
}

uint8_t PQCCarrierPartsNeeded(size_t payload_size)
{
    if (payload_size == 0) return 0;
    return static_cast<uint8_t>((payload_size + PQC_CARRIER_PAYLOAD_PER_PART - 1) / PQC_CARRIER_PAYLOAD_PER_PART);
}

static const unsigned char* GetCarrierTagForType(PQCCommitmentType type)
{
    switch (type) {
    case PQCCommitmentType::FALCON512:
        return PQC_CARRIER_TAG_FALCON;
    case PQCCommitmentType::DILITHIUM2:
        return PQC_CARRIER_TAG_DILITHIUM;
#ifdef ENABLE_LIBOQS_RACCOON
    case PQCCommitmentType::RACCOONG44:
        return PQC_CARRIER_TAG_RACCOON;
#endif
    default:
        return nullptr;
    }
}

static void EncodeCarrierHeader(const PQCCarrierHeader& hdr, unsigned char out[8])
{
    out[0] = hdr.version;
    out[1] = hdr.part_index;
    out[2] = hdr.part_total;
    out[3] = hdr.reserved;
    out[4] = static_cast<unsigned char>((hdr.pk_len >> 8) & 0xFF);
    out[5] = static_cast<unsigned char>(hdr.pk_len & 0xFF);
    out[6] = static_cast<unsigned char>((hdr.full_len >> 8) & 0xFF);
    out[7] = static_cast<unsigned char>(hdr.full_len & 0xFF);
}

static bool DecodeCarrierHeader(const unsigned char data[8], PQCCarrierHeader& hdr)
{
    hdr.version = data[0];
    if (hdr.version != 0x01) return false;
    hdr.part_index = data[1];
    hdr.part_total = data[2];
    hdr.reserved = data[3];
    hdr.pk_len = (static_cast<uint16_t>(data[4]) << 8) | data[5];
    hdr.full_len = (static_cast<uint16_t>(data[6]) << 8) | data[7];
    if (hdr.part_total == 0 || hdr.part_index >= hdr.part_total) return false;
    return true;
}

bool PQCBuildCarrierPartScriptSig(PQCCommitmentType type,
                                   const std::vector<unsigned char>& pubkey,
                                   const std::vector<unsigned char>& signature,
                                   uint8_t part_index,
                                   CScript& script_sig_out)
{
    const unsigned char* carrier_tag = GetCarrierTagForType(type);
    if (carrier_tag == nullptr) return false;
    if (pubkey.empty() || signature.empty()) return false;

    // Build full payload: pubkey || signature
    std::vector<unsigned char> full_payload;
    full_payload.reserve(pubkey.size() + signature.size());
    full_payload.insert(full_payload.end(), pubkey.begin(), pubkey.end());
    full_payload.insert(full_payload.end(), signature.begin(), signature.end());

    uint8_t parts_needed = PQCCarrierPartsNeeded(full_payload.size());
    if (parts_needed == 0 || part_index >= parts_needed) return false;
    if (full_payload.size() > 0xFFFF || pubkey.size() > 0xFFFF) return false;

    // Build HDR8
    PQCCarrierHeader hdr;
    hdr.version = 0x01;
    hdr.part_index = part_index;
    hdr.part_total = parts_needed;
    hdr.reserved = 0x00;
    hdr.pk_len = static_cast<uint16_t>(pubkey.size());
    hdr.full_len = static_cast<uint16_t>(full_payload.size());

    unsigned char hdr_bytes[8];
    EncodeCarrierHeader(hdr, hdr_bytes);

    // Extract this part's payload slice
    size_t part_offset = static_cast<size_t>(part_index) * PQC_CARRIER_PAYLOAD_PER_PART;
    size_t part_remaining = (part_offset < full_payload.size()) ? (full_payload.size() - part_offset) : 0;
    size_t part_len = std::min(part_remaining, static_cast<size_t>(PQC_CARRIER_PAYLOAD_PER_PART));

    // Split into up to 3 chunks of max 520 bytes each
    std::vector<unsigned char> chunks[PQC_CARRIER_CHUNKS_PER_PART];
    size_t consumed = 0;
    for (unsigned int c = 0; c < PQC_CARRIER_CHUNKS_PER_PART; ++c) {
        if (consumed < part_len) {
            size_t chunk_len = std::min(part_len - consumed, static_cast<size_t>(PQC_CARRIER_MAX_CHUNK_BYTES));
            chunks[c].assign(full_payload.begin() + part_offset + consumed,
                             full_payload.begin() + part_offset + consumed + chunk_len);
            consumed += chunk_len;
        }
        // Empty chunks left as empty vectors
    }

    // Build scriptSig: TAG8 HDR8 CHUNK0 CHUNK1 CHUNK2 redeemScript
    CScript redeemScript;
    if (!PQCBuildCarrierRedeemScript(redeemScript)) return false;

    std::vector<unsigned char> tag_vec(carrier_tag, carrier_tag + 8);
    std::vector<unsigned char> hdr_vec(hdr_bytes, hdr_bytes + 8);

    script_sig_out = CScript();
    script_sig_out << tag_vec << hdr_vec;
    for (unsigned int c = 0; c < PQC_CARRIER_CHUNKS_PER_PART; ++c) {
        if (chunks[c].empty()) {
            script_sig_out << OP_0;
        } else {
            script_sig_out << chunks[c];
        }
    }
    // Push the redeemScript as the final element
    std::vector<unsigned char> rsBytes(redeemScript.begin(), redeemScript.end());
    script_sig_out << rsBytes;
    return true;
}

bool PQCDetectCarrierScriptSig(const CScript& scriptSig, PQCCommitmentType& type_out)
{
    // Decode the scriptSig to get push elements
    std::vector<std::vector<unsigned char> > stack;
    CScript::const_iterator it = scriptSig.begin();
    while (it != scriptSig.end()) {
        std::vector<unsigned char> data;
        opcodetype opcode;
        if (!scriptSig.GetOp(it, opcode, data)) break;
        stack.push_back(data);
    }
    // Need at least 6 pushes: TAG8 HDR8 CHUNK0 CHUNK1 CHUNK2 redeemScript
    if (stack.size() < 6) return false;

    // First push must be an 8-byte carrier tag
    if (stack[0].size() != 8) return false;

    if (memcmp(stack[0].data(), PQC_CARRIER_TAG_FALCON, 8) == 0) {
        type_out = PQCCommitmentType::FALCON512;
        return true;
    }
    if (memcmp(stack[0].data(), PQC_CARRIER_TAG_DILITHIUM, 8) == 0) {
        type_out = PQCCommitmentType::DILITHIUM2;
        return true;
    }
#ifdef ENABLE_LIBOQS_RACCOON
    if (memcmp(stack[0].data(), PQC_CARRIER_TAG_RACCOON, 8) == 0) {
        type_out = PQCCommitmentType::RACCOONG44;
        return true;
    }
#endif
    return false;
}

bool PQCParseCarrierPartScriptSig(const CScript& scriptSig,
                                   PQCCommitmentType& type_out,
                                   PQCCarrierHeader& header_out,
                                   std::vector<unsigned char>& payload_out)
{
    // Decode pushes
    std::vector<std::vector<unsigned char> > stack;
    CScript::const_iterator it = scriptSig.begin();
    while (it != scriptSig.end()) {
        std::vector<unsigned char> data;
        opcodetype opcode;
        if (!scriptSig.GetOp(it, opcode, data)) break;
        stack.push_back(data);
    }
    if (stack.size() < 6) return false;

    // TAG8 (8 bytes)
    if (stack[0].size() != 8) return false;
    if (!PQCDetectCarrierScriptSig(scriptSig, type_out)) return false;

    // HDR8 (8 bytes)
    if (stack[1].size() != 8) return false;
    if (!DecodeCarrierHeader(stack[1].data(), header_out)) return false;

    // Concatenate CHUNK0 + CHUNK1 + CHUNK2 (indices 2, 3, 4)
    payload_out.clear();
    for (int c = 2; c < 5 && c < (int)stack.size(); ++c) {
        payload_out.insert(payload_out.end(), stack[c].begin(), stack[c].end());
    }
    return true;
}

// Internal helper: gather carrier parts from all inputs and reconstruct full payload.
// Returns the extracted pubkey, sig, detected type, and first carrier input index.
static bool ReconstructCarrierPayload(const CTransaction& tx,
                                       PQCCommitmentType& type_out,
                                       std::vector<unsigned char>& pubkey_out,
                                       std::vector<unsigned char>& sig_out,
                                       uint32_t& carrier_input_index_out)
{
    struct CarrierPart {
        uint8_t part_index;
        PQCCarrierHeader header;
        std::vector<unsigned char> payload;
        uint32_t input_index;
    };

    std::vector<CarrierPart> parts;
    PQCCommitmentType detected_type = PQCCommitmentType::FALCON512;
    bool type_set = false;

    for (uint32_t i = 0; i < tx.vin.size(); ++i) {
        PQCCommitmentType input_type;
        PQCCarrierHeader hdr;
        std::vector<unsigned char> payload;

        if (!PQCParseCarrierPartScriptSig(tx.vin[i].scriptSig, input_type, hdr, payload)) {
            continue;
        }

        if (!type_set) {
            detected_type = input_type;
            type_set = true;
        } else if (input_type != detected_type) {
            continue; // Mixed types in same tx -- skip
        }

        CarrierPart part;
        part.part_index = hdr.part_index;
        part.header = hdr;
        part.payload = payload;
        part.input_index = i;
        parts.push_back(part);
    }

    if (parts.empty()) return false;

    // Sort by part_index
    std::sort(parts.begin(), parts.end(),
              [](const CarrierPart& a, const CarrierPart& b) {
                  return a.part_index < b.part_index;
              });

    // Verify contiguous parts
    uint8_t expected_total = parts[0].header.part_total;
    if (parts.size() != expected_total) return false;
    for (uint8_t p = 0; p < expected_total; ++p) {
        if (parts[p].part_index != p) return false;
    }

    // Reconstruct full payload
    std::vector<unsigned char> full_payload;
    for (size_t p = 0; p < parts.size(); ++p) {
        full_payload.insert(full_payload.end(), parts[p].payload.begin(), parts[p].payload.end());
    }

    uint16_t pk_len = parts[0].header.pk_len;
    if (pk_len > full_payload.size()) return false;

    pubkey_out.assign(full_payload.begin(), full_payload.begin() + pk_len);
    sig_out.assign(full_payload.begin() + pk_len, full_payload.end());

    if (pubkey_out.empty() || sig_out.empty()) return false;

    type_out = detected_type;
    carrier_input_index_out = parts[0].input_index;
    return true;
}

bool PQCValidateCommitmentFromCarrier(const CTransaction& tx,
                                       const uint256& commitment,
                                       PQCCommitmentType& type_out,
                                       uint32_t& carrier_input_index_out,
                                       uint16_t& pk_len_out,
                                       uint16_t& sig_len_out)
{
    std::vector<unsigned char> pubkey, sig;
    if (!ReconstructCarrierPayload(tx, type_out, pubkey, sig, carrier_input_index_out))
        return false;

    // Recompute commitment
    uint256 recomputed;
    if (!PQCComputeCommitment(pubkey, sig, recomputed)) return false;
    if (recomputed != commitment) return false;

    pk_len_out = static_cast<uint16_t>(pubkey.size());
    sig_len_out = static_cast<uint16_t>(sig.size());
    return true;
}

bool PQCExtractKeyMaterialFromCarrier(const CTransaction& tx,
                                       PQCCommitmentType& type_out,
                                       std::vector<unsigned char>& pubkey_out,
                                       std::vector<unsigned char>& signature_out)
{
    uint32_t carrier_input_index = 0;
    return ReconstructCarrierPayload(tx, type_out, pubkey_out, signature_out, carrier_input_index);
}

bool PQCVerifySignatureFromCarrier(const CTransaction& tx,
                                    const uint256& commitment,
                                    const unsigned char* message, size_t message_len,
                                    PQCCommitmentType& type_out,
                                    uint32_t& carrier_input_index_out,
                                    uint16_t& pk_len_out,
                                    uint16_t& sig_len_out)
{
    std::vector<unsigned char> pubkey, sig;
    if (!ReconstructCarrierPayload(tx, type_out, pubkey, sig, carrier_input_index_out))
        return false;

    // Step 1: Validate SHA256(pk || sig) == commitment
    uint256 recomputed;
    if (!PQCComputeCommitment(pubkey, sig, recomputed)) return false;
    if (recomputed != commitment) return false;

    pk_len_out = static_cast<uint16_t>(pubkey.size());
    sig_len_out = static_cast<uint16_t>(sig.size());

    // Step 2: Verify the PQC signature over the message using liboqs
    if (!message || message_len == 0) return false;
    return PQCVerify(type_out, pubkey, message, message_len, sig);
}

// --- liboqs PQC Cryptographic Operations ---

const char* PQCGetOQSAlgorithmName(PQCCommitmentType type)
{
#if ENABLE_LIBOQS
    switch (type) {
    case PQCCommitmentType::FALCON512:
        return OQS_SIG_alg_falcon_512;
    case PQCCommitmentType::DILITHIUM2:
        return OQS_SIG_alg_ml_dsa_44;
#ifdef ENABLE_LIBOQS_RACCOON
    case PQCCommitmentType::RACCOONG44:
        return "Raccoon-G-44";
#endif
    default:
        return nullptr;
    }
#else
    (void)type;
    return nullptr;
#endif
}

bool PQCGenerateKeypair(PQCCommitmentType type,
                         std::vector<unsigned char>& public_key_out,
                         std::vector<unsigned char>& secret_key_out)
{
#if ENABLE_LIBOQS
    const char* alg_name = PQCGetOQSAlgorithmName(type);
    if (!alg_name) return false;

    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (!sig) return false;

    public_key_out.resize(sig->length_public_key);
    secret_key_out.resize(sig->length_secret_key);

    OQS_STATUS status = OQS_SIG_keypair(sig, public_key_out.data(), secret_key_out.data());
    OQS_SIG_free(sig);

    if (status != OQS_SUCCESS) {
        public_key_out.clear();
        secret_key_out.clear();
        return false;
    }
    return true;
#else
    (void)type;
    (void)public_key_out;
    (void)secret_key_out;
    return false;
#endif
}

bool PQCSign(PQCCommitmentType type,
             const std::vector<unsigned char>& secret_key,
             const unsigned char* message, size_t message_len,
             std::vector<unsigned char>& signature_out)
{
#if ENABLE_LIBOQS
    const char* alg_name = PQCGetOQSAlgorithmName(type);
    if (!alg_name) return false;
    if (secret_key.empty() || !message || message_len == 0) return false;

    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (!sig) return false;

    if (secret_key.size() != sig->length_secret_key) {
        OQS_SIG_free(sig);
        return false;
    }

    signature_out.resize(sig->length_signature);
    size_t sig_len = 0;

    OQS_STATUS status = OQS_SIG_sign(sig, signature_out.data(), &sig_len,
                                      message, message_len, secret_key.data());
    OQS_SIG_free(sig);

    if (status != OQS_SUCCESS) {
        signature_out.clear();
        return false;
    }
    signature_out.resize(sig_len);
    return true;
#else
    (void)type;
    (void)secret_key;
    (void)message;
    (void)message_len;
    (void)signature_out;
    return false;
#endif
}

bool PQCVerify(PQCCommitmentType type,
               const std::vector<unsigned char>& public_key,
               const unsigned char* message, size_t message_len,
               const std::vector<unsigned char>& signature)
{
#if ENABLE_LIBOQS
    const char* alg_name = PQCGetOQSAlgorithmName(type);
    if (!alg_name) return false;
    if (public_key.empty() || !message || message_len == 0 || signature.empty()) return false;

    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (!sig) return false;

    if (public_key.size() != sig->length_public_key) {
        OQS_SIG_free(sig);
        return false;
    }

    OQS_STATUS status = OQS_SIG_verify(sig, message, message_len,
                                        signature.data(), signature.size(),
                                        public_key.data());
    OQS_SIG_free(sig);
    return status == OQS_SUCCESS;
#else
    (void)type;
    (void)public_key;
    (void)message;
    (void)message_len;
    (void)signature;
    return false;
#endif
}
