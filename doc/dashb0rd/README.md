# Dashboard Metrics for Dogebox Integration

## Overview

This implementation provides comprehensive blockchain metrics for the Dogebox dashboard, based on the [libdogecoin dashboard specification](https://github.com/edtubbs/pups/blob/dashb0rd/dashboard/manifest.json).

## RPC Method

### `getdashboardmetrics`

Returns blockchain and network metrics formatted for dogebox dashboard integration.

**Arguments:** None

**Result:**
```json
{
  "chain_tip_height": 5234567,
  "chain_tip_difficulty": 8912345.67,
  "chain_tip_time": "2026-02-06T02:00:00",
  "chain_tip_bits_hex": "0x1a01ffff",
  "smpv_mempool_txs": 1234,
  "smpv_total_bytes": 5678900,
  "smpv_types_p2pkh": 4500,
  "smpv_types_p2sh": 123,
  "smpv_types_multisig": 45,
  "smpv_types_op_return": 12,
  "smpv_types_nonstandard": 3,
  "smpv_types_vout_total": 4683,
  "stats_blocks": 100,
  "stats_transactions": 23456,
  "stats_tps": 3.89,
  "stats_volume": 45678901.23,
  "stats_outputs": 67890,
  "stats_bytes": 98765432,
  "stats_median_fee_per_block": 1.23,
  "stats_avg_fee_per_block": 1.45,
  "uptime_sec": 86400
}
```

## Metrics Description

### Chain Tip Metrics

- **chain_tip_height** (integer): Current blockchain height
- **chain_tip_difficulty** (float): Network mining difficulty
- **chain_tip_time** (string): Timestamp of the most recent block (ISO-8601 format)
- **chain_tip_bits_hex** (string): Compact difficulty target in hexadecimal format

### Mempool Metrics

- **smpv_mempool_txs** (integer): Number of transactions in the mempool
- **smpv_total_bytes** (integer): Total memory usage of the mempool in bytes
- **smpv_types_p2pkh** (integer): Count of Pay-to-PubKey-Hash outputs in mempool
- **smpv_types_p2sh** (integer): Count of Pay-to-Script-Hash outputs in mempool
- **smpv_types_multisig** (integer): Count of multisig outputs in mempool
- **smpv_types_op_return** (integer): Count of OP_RETURN outputs in mempool
- **smpv_types_nonstandard** (integer): Count of nonstandard outputs in mempool
- **smpv_types_vout_total** (integer): Total number of outputs across all mempool transactions

### Rolling Statistics (Last 100 Blocks)

- **stats_blocks** (integer): Number of blocks analyzed (up to 100)
- **stats_transactions** (integer): Total transactions across analyzed blocks
- **stats_tps** (float): Estimated transactions per second (transactions / time span)
- **stats_volume** (float): Sum of all output values in DOGE
- **stats_outputs** (integer): Total number of transaction outputs
- **stats_bytes** (integer): Total size of analyzed blocks in bytes
- **stats_median_fee_per_block** (float): Median miner fee per block in DOGE
- **stats_avg_fee_per_block** (float): Average miner fee per block in DOGE

### Uptime

- **uptime_sec** (integer): Node uptime in seconds since startup

## Usage Examples

### Command Line
```bash
dogecoin-cli getdashboardmetrics
```

### RPC Call
```bash
curl --user myuser:mypass --data-binary '{"jsonrpc":"2.0","id":"dashboard","method":"getdashboardmetrics","params":[]}' -H 'content-type: text/plain;' http://127.0.0.1:22555/
```

## Integration with Dogebox

This RPC endpoint is designed to be called periodically by the Dogebox monitoring system to provide real-time blockchain statistics for dashboard display.

### Key Differences from SPV Implementation

This implementation is adapted for Dogecoin Core (full node) rather than an SPV (Simplified Payment Verification) node:

- **No wallet-specific metrics**: Core doesn't track specific addresses, balances, or UTXOs globally
- **Full blockchain access**: Can calculate accurate statistics from actual block data
- **Mempool analysis**: Can analyze all mempool transactions and categorize output types
- **Historical statistics**: Can compute rolling statistics from the last 100 blocks

## Performance Considerations

- Rolling statistics are calculated on-demand from the last 100 blocks
- Mempool analysis iterates through all current mempool transactions
- Fee calculations are optimized using coinbase analysis rather than expensive transaction lookups
- All blockchain data access is protected by appropriate locks

## Compatibility

- No breaking changes to existing RPC methods
- Optional endpoint that doesn't affect normal node operation
- Safe for production use
- Provides meaningful metrics regardless of node synchronization state
