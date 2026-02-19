# Dashboard Implementation - Complete Summary

## Overview

This branch implements a comprehensive dashboard system for Dogecoin Core, providing both RPC and Qt GUI interfaces for monitoring blockchain and network metrics.

## ⚠️ Important: Build Instructions

After cloning or pulling this branch, **you must regenerate the build system**:

```bash
./autogen.sh
./configure --with-gui=qt5
make -j$(nproc)
```

**Do not skip `./autogen.sh` and `./configure`** - these steps are required because new source files have been added.

## Quick Links

- **New to this branch?** → Start with [QUICK_START.md](QUICK_START.md)
- **Build errors?** → See [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Detailed build guide** → Read [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)
- **Feature documentation** → Check [DASHBOARD_IMPLEMENTATION_README.md](DASHBOARD_IMPLEMENTATION_README.md)

## Features

### RPC Endpoint: `getdashboardmetrics`

Returns 21 comprehensive metrics in JSON format:

**Chain Tip (4 metrics):**
- Height, Difficulty, Time, Bits

**Mempool (8 metrics):**
- Transaction count, Bytes, Output type breakdown (P2PKH, P2SH, Multisig, OP_RETURN, Nonstandard, Total)

**Rolling Statistics (8 metrics):**
- Last 100 blocks analysis: Transactions, TPS, Volume, Outputs, Bytes, Median fee, Average fee

**Uptime (1 metric):**
- Node uptime in seconds

### Qt GUI Dashboard

- Visual display of all 21 metrics
- Real-time updates (1-second polling)
- Sparkline charts showing trends
- Accessible via toolbar button or Alt+5

## Usage

### From GUI

1. Launch: `./src/qt/dogecoin-qt`
2. Click "Dashb0rd" button in toolbar (or press Alt+5)
3. View real-time metrics and charts

### From RPC

```bash
# Command line
./src/dogecoin-cli getdashboardmetrics

# Via HTTP
curl --user user:pass --data-binary '{"jsonrpc":"2.0","id":"1","method":"getdashboardmetrics","params":[]}' http://127.0.0.1:22555/
```

## Files Changed

### Core Implementation (13 files)

**RPC:**
- `src/rpc/blockchain.cpp` (+213 lines) - Dashboard metrics endpoint

**Qt GUI:**
- `src/qt/dashb0rd.cpp/h` - Dashboard container
- `src/qt/dashb0rdpage.cpp/h` - Main dashboard page (340 lines)
- `src/qt/sparklinewidget.cpp/h` - Chart visualization
- `src/qt/bitcoingui.cpp/h` - Toolbar integration
- `src/qt/walletframe.cpp/h` - Page navigation

**Core Functions:**
- `src/core_read.cpp` (+74 lines) - ParseScriptFlags/FormatScriptFlags
- `src/core_io.h` - Function declarations

**Build System:**
- `src/Makefile.qt.include` - Added dashboard files

**Tests:**
- `src/test/script_tests.cpp` - Cleanup
- `src/test/transaction_tests.cpp` - Cleanup

### Documentation (8 files)

**User Guides:**
- `QUICK_START.md` - Fast setup guide
- `TROUBLESHOOTING.md` - Error solutions
- `BUILD_INSTRUCTIONS.md` - Detailed build guide
- `DASHBOARD_IMPLEMENTATION_README.md` - Complete feature docs
- `BUILD_FIX_SUMMARY.md` - Build fixes applied

**Dashboard Docs:**
- `doc/dashb0rd/README.md` - User guide
- `contrib/dashb0rd/example_output.json` - Example RPC output

**This File:**
- `README_DASHBOARD.md` - You are here

## Common Issues

### "undefined reference to main" Error

**Cause:** Build system not regenerated after pulling new code

**Solution:**
```bash
make clean
./autogen.sh
./configure --with-gui=qt5
make -j$(nproc)
```

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for details.

### Missing Dependencies

**Solution:** Install required packages
```bash
sudo apt-get install build-essential libtool autotools-dev automake pkg-config \
    libssl-dev libevent-dev bsdmainutils libboost-all-dev libdb++-dev \
    libminiupnpc-dev libzmq3-dev libqt5gui5 libqt5core5a libqt5dbus5 \
    qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler
```

## Technical Details

### Performance
- O(1) fee calculation using coinbase analysis (not O(n) transaction iteration)
- Efficient mempool traversal
- Minimal blockchain lock time

### Thread Safety
- Proper `LOCK(cs_main)` for blockchain access
- `LOCK(mempool.cs)` for mempool iteration
- No race conditions

### Compatibility
- Works with or without wallet
- No breaking changes to existing RPC
- Optional Qt GUI integration

## Integration

### dogebox Compatibility

This implementation provides equivalent metrics to the libdogecoin dashboard used in dogebox, adapted for Dogecoin Core's full node architecture.

### Differences from libdogecoin

**Not Implemented (SPV-specific):**
- Wallet SPV features (address, balance, UTXOs, transactions)
- SMPV (Simple Mempool View) specific features

**Implemented (Full node equivalent):**
- Chain tip metrics
- Mempool analysis (renamed from smpv_* to mempool_*)
- Rolling blockchain statistics
- Node uptime

## Testing

### Verify Build
```bash
./src/bench/bench_dogecoin --help
./src/qt/dogecoin-qt --version
./src/dogecoind --version
```

### Test Dashboard
```bash
# Start daemon
./src/dogecoind -daemon

# Test RPC
./src/dogecoin-cli getdashboardmetrics

# View in GUI
./src/qt/dogecoin-qt
# Click "Dashb0rd" button
```

### Run Tests
```bash
make check
./qa/pull-tester/rpc-tests.py
```

## Development

### Building
```bash
# Initial build
./autogen.sh
./configure --with-gui=qt5
make -j$(nproc)

# After code changes
make -j$(nproc)

# Clean rebuild
make clean && make -j$(nproc)

# Complete rebuild
make distclean && ./autogen.sh && ./configure --with-gui=qt5 && make -j$(nproc)
```

### Modifying Dashboard

**To add new metrics:**
1. Add to `getdashboardmetrics()` in `src/rpc/blockchain.cpp`
2. Update help text
3. Update example in `contrib/dashb0rd/example_output.json`
4. Add display in `src/qt/dashb0rdpage.cpp`

**To modify GUI:**
1. Edit `src/qt/dashb0rdpage.cpp` for layout/display
2. Edit `src/qt/sparklinewidget.cpp` for charts
3. Rebuild: `make -j$(nproc)`

## Support

### Getting Help

1. **Build errors** → [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. **Usage questions** → [doc/dashb0rd/README.md](doc/dashb0rd/README.md)
3. **Feature details** → [DASHBOARD_IMPLEMENTATION_README.md](DASHBOARD_IMPLEMENTATION_README.md)
4. **Quick start** → [QUICK_START.md](QUICK_START.md)

### Reporting Issues

When reporting issues, include:
- Your OS and version
- Full build output (not just the error)
- Configure options used
- `config.log` if configuration fails

## License

This code follows the same license as Dogecoin Core (MIT License).

## Credits

Based on the libdogecoin dashboard specification and adapted for Dogecoin Core.

---

**For the fastest start:** Read [QUICK_START.md](QUICK_START.md)

**Having build issues?** Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
