# Metrics Mapping: Libdogecoin SPV vs Dogecoin Core

This document explains how the metrics from the libdogecoin dashboard specification have been adapted for Dogecoin Core (full node).

## Source Specification

Reference: https://raw.githubusercontent.com/edtubbs/pups/d6f530e74f76a63a1eb4c64c2b98a800e374b27c/dashboard/manifest.json

## Implemented Metrics

### ✅ Chain Tip Metrics

| SPV Metric | Core Implementation | Notes |
|------------|---------------------|-------|
| chain_tip_height | chain_tip_height | Direct mapping from chainActive.Height() |
| chain_tip_difficulty | chain_tip_difficulty | Direct mapping from GetDifficulty() |
| chain_tip_time | chain_tip_time | ISO-8601 formatted from tip->GetBlockTime() |
| chain_tip_bits_hex | chain_tip_bits_hex | Hex formatted from tip->nBits |

### ✅ Mempool Metrics

| SPV Metric | Core Implementation | Notes |
|------------|---------------------|-------|
| smpv_mempool_txs | smpv_mempool_txs | mempool.size() |
| smpv_total_bytes | smpv_total_bytes | mempool.DynamicMemoryUsage() |
| smpv_types_p2pkh | smpv_types_p2pkh | Counted from mempool transactions |
| smpv_types_p2sh | smpv_types_p2sh | Counted from mempool transactions |
| smpv_types_multisig | smpv_types_multisig | Counted from mempool transactions |
| smpv_types_op_return | smpv_types_op_return | Counted from mempool transactions |
| smpv_types_nonstandard | smpv_types_nonstandard | Counted from mempool transactions |
| smpv_types_vout_total | smpv_types_vout_total | Total outputs in mempool |

### ✅ Rolling Statistics (100 blocks)

| SPV Metric | Core Implementation | Notes |
|------------|---------------------|-------|
| stats_blocks | stats_blocks | Number of blocks analyzed (up to 100) |
| stats_transactions | stats_transactions | Sum of transactions in analyzed blocks |
| stats_tps | stats_tps | transactions / time_span |
| stats_volume | stats_volume | Sum of all output values in DOGE |
| stats_outputs | stats_outputs | Total outputs in analyzed blocks |
| stats_bytes | stats_bytes | Total serialized size of blocks |
| stats_median_fee_per_block | stats_median_fee_per_block | Median of block fees |
| stats_avg_fee_per_block | stats_avg_fee_per_block | Average of block fees |

### ✅ Uptime

| SPV Metric | Core Implementation | Notes |
|------------|---------------------|-------|
| uptime_sec | uptime_sec | GetTime() - GetStartupTime() |

## Not Implemented (SPV-Specific)

These metrics are specific to SPV wallet functionality and don't apply to a full node:

### ❌ Wallet Metrics
- **chaintip** - SPV-specific format with hash
- **addresses** - Wallet-specific, not available globally in full node
- **balance** - Wallet-specific, not available globally in full node
- **utxos** - Wallet-specific, not available globally in full node
- **transactions** - Wallet-specific transaction list

### ❌ SPV Session Metrics
- **headers_bytes** - SPV downloads only headers; full node stores complete blocks
- **blocks_total** - Could be implemented but less relevant for full node
- **transactions_total** - Could be implemented but less relevant for full node
- **outputs_total** - Could be implemented but less relevant for full node
- **output_value_total** - Could be implemented but less relevant for full node
- **fees_total** - Could be implemented but less relevant for full node
- **block_bytes_total** - Could be implemented but less relevant for full node
- **approx_chain_bytes** - Full node has exact size via CalculateCurrentUsage()

### ❌ SMPV (Simple Mempool View) Specific
- **smpv_enabled** - SPV feature flag, always true for full node mempool
- **smpv_watchers** - SPV internal counter
- **smpv_confirmed** - SPV session tracking
- **smpv_unconfirmed** - SPV session tracking
- **smpv_last_seen_age_sec** - SPV-specific metric
- **smpv_last_seen_txid** - SPV-specific metric
- **smpv_coinbase_txs** - Not typically in mempool for full nodes
- **metadata** - SPV-specific field

### ❌ Other
- **disk_used_pct** - Could be implemented but requires filesystem-specific code

## Implementation Notes

### Performance Optimizations

1. **Fee Calculation**: Uses coinbase analysis instead of expensive per-transaction lookups
2. **Mempool Locking**: Minimal lock duration for thread safety
3. **Block Analysis**: Reads only what's needed from disk

### Data Type Considerations

- **Counts and sizes**: int64_t for precise large values
- **Rates and ratios**: double for fractional calculations
- **Amounts**: CAmount/UniValue for proper DOGE precision
- **Times**: ISO-8601 strings for compatibility

### Thread Safety

- All blockchain access protected by `LOCK(cs_main)`
- Mempool access protected by `LOCK(mempool.cs)`
- No risk of deadlocks due to consistent lock ordering
