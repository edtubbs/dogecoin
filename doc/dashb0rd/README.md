# Dashboard Metrics for Dogebox Integration

## Overview

This feature adds support for generating dashboard metrics compatible with the Dogebox dashboard system. The metrics provide key blockchain status information in a format optimized for dashboard display.

## RPC Method

### `getdashboardmetrics`

Returns blockchain metrics formatted for dogebox dashboard integration.

**Arguments:** None

**Result:**
```json
{
  "chain": "main",
  "blocks": 4567890,
  "headers": 4567890,
  "difficulty": 12345678.90,
  "verification_progress": "99.95%",
  "initial_block_download": "false",
  "chain_size_human": "78.45 GB"
}
```

**Fields:**
- `chain` (string): Current network name (main, test, regtest)
- `blocks` (integer): Current synchronized block height
- `headers` (integer): Total number of validated headers
- `difficulty` (float): Current network difficulty
- `verification_progress` (string): Blockchain verification progress as a percentage
- `initial_block_download` (string): Whether node is in Initial Block Download mode ("true" or "false")
- `chain_size_human` (string): Total blockchain size in human-readable format (e.g., "78.45 GB")

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

This RPC method is designed to be called periodically by the Dogebox monitoring system to update dashboard displays with current node status. The metrics align with the fields specified in the Dogebox pup manifest for the Dogecoin Core node.

## Metric Descriptions

### Chain
The network the node is operating on. Typical values: "main" (mainnet), "test" (testnet), "regtest" (regression test network).

### Blocks and Headers
- **Blocks**: The height of the highest fully validated block in the active chain
- **Headers**: The height of the highest validated block header (may be ahead of blocks during sync)

### Difficulty
Current mining difficulty. Higher values indicate more computational power is required to mine blocks.

### Verification Progress
Indicates how far the node has progressed in validating the blockchain, expressed as a percentage. 100% means the node is fully synced.

### Initial Block Download
Indicates whether the node is still downloading and validating the blockchain for the first time. This will be "true" during initial sync and "false" once the node is caught up.

### Chain Size
The total disk space used by the blockchain data, shown in human-readable format (automatically scales to B, KB, MB, GB, or TB).
