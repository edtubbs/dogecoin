# Cross-Validating Dogecoin Core PQC Transactions with libdogecoin CLI

This document describes how to use the libdogecoin `such` CLI tool to
independently validate PQC commitment transactions created by Dogecoin Core.

## Prerequisites

Build libdogecoin with PQC support from the
[copilot/run-end-to-end-tests-dilithium2-raccoon-g](https://github.com/edtubbs/libdogecoin/tree/copilot/run-end-to-end-tests-dilithium2-raccoon-g)
branch:

```bash
cd libdogecoin
./autogen.sh
./configure --enable-liboqs       # Falcon-512 + Dilithium2
# or: ./configure --enable-liboqs-raccoon  # adds Raccoon-G-44
make
```

## Workflow Overview

Dogecoin Core creates PQC transactions in a two-step process:

1. **TX_C (Commitment):** Sends payment + OP_RETURN commitment + P2SH carrier outputs.
2. **TX_R (Reveal):** Spends carrier outputs, revealing full PQ public key + signature.

The details popup in Core already performs full validation:
- Recomputes `TX_BASE` from `TX_C` (strips OP_RETURN + carrier outputs, restores carrier cost to vout[0])
- Computes `sighash32 = SignatureHash(scriptPubKey, TX_BASE, 0, SIGHASH_ALL)`
- Verifies `OQS_SIG_verify(pk, sighash32, sig)`
- Shows **PASSED** (green) or **FAILED** (red) with specific failure reasons

Old transactions (created before the TX_BASE sighash fix) will show **FAILED —
OQS_SIG_verify() failed** because they signed a different message. New
transactions from Core or libdogecoin will show **PASSED**.

## Step-by-Step Cross-Validation Using libdogecoin CLI

### Step 1: Extract TX_C raw hex from Core

```bash
dogecoin-cli getrawtransaction <TX_C_txid>
```

### Step 2: Compute sighash32 from TX_C using libdogecoin

```bash
# Derive the sighash32 for input 0 of the base transaction
./such -c tx_sighash32 \
  -x <raw_tx_hex> \
  -s <scriptPubKey_of_input0_hex> \
  -i 0 \
  -h 1
```

Note: The raw tx passed to `tx_sighash32` should be the TX_BASE (before
OP_RETURN and carrier outputs were appended). You can reconstruct TX_BASE
by stripping the OP_RETURN output and any P2SH carrier outputs from TX_C,
then adding the carrier cost back to vout[0].

### Step 3: Verify the PQC signature

Using the public key and signature extracted from TX_R carrier scriptSig:

```bash
# Falcon-512
./such -c falcon_verify -p <pubkey_hex> -x <sighash32_hex> -s <signature_hex>

# Dilithium2
./such -c dilithium2_verify -p <pubkey_hex> -x <sighash32_hex> -s <signature_hex>

# Raccoon-G-44
./such -c raccoong_verify -p <pubkey_hex> -x <sighash32_hex> -s <signature_hex>
```

### Step 4: Verify the commitment

```bash
# Falcon-512
./such -c falcon_commit -p <pubkey_hex> -s <signature_hex>
# Output: commitment32 hex — compare against OP_RETURN payload in TX_C

# Dilithium2
./such -c dilithium2_commit -p <pubkey_hex> -s <signature_hex>

# Raccoon-G-44
./such -c raccoong_commit -p <pubkey_hex> -s <signature_hex>
```

The output should match the 32-byte commitment in the OP_RETURN output of TX_C.

### Step 5: Parse carrier scriptSig (optional)

```bash
./such -c pqc_carrier_parsepart -x <TX_R_scriptSig_hex>
```

This extracts the TAG8, HDR8 (with part_index/part_total/pk_len/full_len),
and payload chunks from the carrier scriptSig.

## Multi-Part Reassembly (Dilithium2/Raccoon-G)

For algorithms requiring multiple carrier parts:

| Algorithm    | pk_len | sig_len | full_len | part_total | Carrier Tag  |
|-------------|--------|---------|----------|------------|-------------|
| Falcon-512  | 897    | ~690    | ~1587    | 1          | `FLC1FULL`  |
| Dilithium2  | 1312   | 2420    | 3732     | 3          | `DIL2FULL`  |
| Raccoon-G-44| 16144  | 20768   | 36912    | 24         | `RCG4FULL`  |

Parse each carrier vin's scriptSig with `pqc_carrier_parsepart`, concatenate
payload chunks in `part_index` order (0 to part_total-1), truncate to
`full_len` bytes, then split into `pk[0..pk_len)` and `sig[pk_len..full_len)`.

## Expected Results

| Transaction Age | Sighash | OQS_SIG_verify | Overall |
|----------------|---------|----------------|---------|
| Old (pre-fix)  | Signed wrong message | **FAILED** (red) | **FAILED — OQS_SIG_verify() failed** |
| New (Core)     | Signed TX_BASE sighash32 | **PASSED** (green) | **PASSED — commitment and cryptographic verification both verified** |
| New (libdogecoin) | Signed TX_BASE sighash32 | **PASSED** (green) | **PASSED — commitment and cryptographic verification both verified** |

## Automated Cross-Validation

The test script `qa/rpc-tests/pqc_testnet_checkpoint_scan.py` can perform
automated cross-validation:

```bash
python3 qa/rpc-tests/pqc_testnet_checkpoint_scan.py \
  --srcdir /path/to/dogecoin/src \
  --txid <txid> \
  --height <height> \
  --commitment-type FLC1 \
  --pubkey-hex <pubkey_hex> \
  --signature-hex <signature_hex>
```

See `doc/spec/pqc-validation-log-template.md` for the full log format.
