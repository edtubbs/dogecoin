# Dogecoin Core Dashboard Implementation

## Overview

This branch (`copilot/add-core-metrics-dashb0rd`) implements a comprehensive dashboard for Dogecoin Core, providing both a Qt GUI dashboard and an RPC endpoint for external monitoring systems.

## Features

### 1. RPC Endpoint: `getdashboardmetrics`

Returns 21 comprehensive metrics in JSON format:

**Chain Tip Metrics (4):**
- `chain_tip_height` - Current blockchain height
- `chain_tip_difficulty` - Current mining difficulty  
- `chain_tip_time` - Chain tip timestamp (ISO-8601)
- `chain_tip_bits_hex` - Compact difficulty bits in hex

**Mempool Metrics (8):**
- `mempool_tx_count` - Transaction count in mempool
- `mempool_total_bytes` - Total mempool size in bytes
- `mempool_p2pkh_count` - P2PKH outputs in mempool
- `mempool_p2sh_count` - P2SH outputs in mempool
- `mempool_multisig_count` - Multisig outputs in mempool
- `mempool_op_return_count` - OP_RETURN outputs in mempool
- `mempool_nonstandard_count` - Nonstandard outputs in mempool
- `mempool_output_count` - Total outputs in mempool

**Rolling Statistics (8) - Last 100 blocks:**
- `stats_blocks` - Number of blocks analyzed
- `stats_transactions` - Total transactions in window
- `stats_tps` - Estimated transactions per second
- `stats_volume` - Sum of output values in DOGE
- `stats_outputs` - Total outputs in window
- `stats_bytes` - Total block bytes in window
- `stats_median_fee_per_block` - Median fee per block
- `stats_avg_fee_per_block` - Average fee per block

**Uptime (1):**
- `uptime_sec` - Node uptime in seconds

### 2. Qt GUI Dashboard

Integrated dashboard accessible from the main GUI:
- **Access:** Click "Dashb0rd" button in toolbar or press Alt+5
- **Features:**
  - Real-time metric updates (1 second polling)
  - Sparkline charts showing trends
  - Scrollable interface showing all 21 metrics
  - Works without wallet (blockchain data only)

## Usage

### RPC Command Line
```bash
dogecoin-cli getdashboardmetrics
```

### RPC via curl
```bash
curl --user myuser:mypass \
  --data-binary '{"jsonrpc":"2.0","id":"dashboard","method":"getdashboardmetrics","params":[]}' \
  -H 'content-type: text/plain;' \
  http://127.0.0.1:22555/
```

### Qt GUI
1. Launch `dogecoin-qt`
2. Click "Dashb0rd" in toolbar
3. View all metrics with sparkline charts

## Build Instructions

**IMPORTANT:** After pulling this branch, you must regenerate the build system:

```bash
# Step 1: Clean previous build
make clean

# Step 2: Regenerate configure script
./autogen.sh

# Step 3: Reconfigure
./configure --with-gui=qt5  # Add your other options

# Step 4: Build
make -j$(nproc)
```

See `BUILD_INSTRUCTIONS.md` for detailed build instructions and troubleshooting.

## Files Modified/Added

### RPC Implementation
- `src/rpc/blockchain.cpp` - Added `getdashboardmetrics` RPC method (+213 lines)

### Qt GUI Dashboard
- `src/qt/dashb0rd.cpp/h` - Dashboard container widget
- `src/qt/dashb0rdpage.cpp/h` - Main dashboard page with all metrics (340 lines)
- `src/qt/sparklinewidget.cpp/h` - Chart widget for visualizations
- `src/qt/bitcoingui.cpp/h` - Added dashboard action and navigation
- `src/qt/walletframe.cpp/h` - Integrated dashboard into wallet frame

### Build System
- `src/Makefile.qt.include` - Added dashboard files to build

### Core Functions
- `src/core_read.cpp` - Added ParseScriptFlags() and FormatScriptFlags()
- `src/core_io.h` - Added function declarations

### Test Files (Fixed)
- `src/test/script_tests.cpp` - Removed forward declarations
- `src/test/transaction_tests.cpp` - Removed duplicate code

### Documentation
- `doc/dashb0rd/README.md` - User guide with usage examples
- `contrib/dashb0rd/example_output.json` - Example RPC output
- `BUILD_INSTRUCTIONS.md` - Build system regeneration guide
- `BUILD_FIX_SUMMARY.md` - Summary of compilation fixes
- `DASHBOARD_IMPLEMENTATION_README.md` - This file

## Technical Details

### Performance Optimization
- Fee calculation uses coinbase analysis (O(1) vs O(n) transaction lookups)
- Efficient mempool iteration with proper locking
- Minimal overhead on node operations

### Thread Safety
- Uses `LOCK(cs_main)` for blockchain access
- Uses `LOCK(mempool.cs)` for mempool iteration
- Safe for concurrent RPC calls

### Compatibility
- Based on libdogecoin dashboard specification
- Adapted for Dogecoin Core full node architecture
- Works with or without wallet enabled

## Integration with dogebox

This implementation provides the metrics needed for dogebox monitoring, adapted from the libdogecoin dashboard specification. The RPC endpoint returns data in the same format expected by external monitoring systems.

## Troubleshooting

### Build Error: "undefined reference to main"

This means the build system needs regeneration. Solution:
```bash
./autogen.sh && ./configure [options] && make
```

See `BUILD_INSTRUCTIONS.md` for details.

### Dashboard Not Appearing in GUI

Make sure:
1. Qt5 was enabled during configure: `./configure --with-gui=qt5`
2. Build completed successfully
3. Check for "Dashb0rd" button in toolbar

### RPC Method Not Found

Make sure:
1. You're running the correct dogecoind binary from this branch
2. The node is fully started and synchronized
3. RPC is properly configured in dogecoin.conf

## Future Enhancements

Potential improvements for future versions:
- Add more detailed mempool statistics
- Include network peer information
- Add transaction fee estimation metrics
- Implement metric history persistence
- Add configurable update intervals

## Credits

Implementation based on:
- libdogecoin dashboard specification
- Dogecoin Core RPC framework
- Bitcoin Core Qt GUI framework

## License

This code is released under the MIT License, consistent with Dogecoin Core.
