# Quick Start Guide

## Building Dogecoin with Dashboard

After cloning or pulling this branch, follow these steps:

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libtool autotools-dev automake pkg-config \
    libssl-dev libevent-dev bsdmainutils libboost-all-dev libdb++-dev \
    libminiupnpc-dev libzmq3-dev libqt5gui5 libqt5core5a libqt5dbus5 \
    qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler
```

### 2. Build

```bash
./autogen.sh
./configure --with-gui=qt5
make -j$(nproc)
```

### 3. Run

**With GUI (includes dashboard):**
```bash
./src/qt/dogecoin-qt
```

**Daemon only:**
```bash
./src/dogecoind
```

### 4. Access Dashboard

**From GUI:**
- Click "Dashb0rd" button in toolbar
- Or press Alt+5

**From RPC:**
```bash
./src/dogecoin-cli getdashboardmetrics
```

## Common Issues

### "undefined reference to main"

This means you need to regenerate the build system:
```bash
make clean
./autogen.sh
./configure --with-gui=qt5
make -j$(nproc)
```

See **TROUBLESHOOTING.md** for detailed help.

### Build fails with "cannot find -lboost_system"

Install Boost libraries:
```bash
sudo apt-get install libboost-all-dev
```

### Configure fails with "Qt dependencies not found"

Install Qt5 development packages:
```bash
sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools
```

## What's New in This Branch

This branch adds comprehensive dashboard functionality:

- **RPC Endpoint**: `getdashboardmetrics` returns 21 metrics
- **Qt GUI**: Dashboard tab with real-time metrics and charts
- **Metrics**: Chain tip, mempool, rolling stats, uptime

## Documentation

- **QUICK_START.md** (this file) - Get started fast
- **TROUBLESHOOTING.md** - Fix common build errors
- **BUILD_INSTRUCTIONS.md** - Detailed build guide
- **DASHBOARD_IMPLEMENTATION_README.md** - Complete feature documentation
- **doc/dashb0rd/README.md** - User guide for dashboard

## Need Help?

1. Check **TROUBLESHOOTING.md** for common errors
2. Read **BUILD_INSTRUCTIONS.md** for detailed build steps
3. See **DASHBOARD_IMPLEMENTATION_README.md** for feature details

## Testing

After building, verify everything works:

```bash
# Run unit tests
make check

# Run RPC tests
./qa/pull-tester/rpc-tests.py

# Test dashboard RPC
./src/dogecoin-cli getdashboardmetrics
```

## Development

If you're developing:

```bash
# Clean build
make clean

# After changing code
make -j$(nproc)

# Full rebuild
make distclean
./autogen.sh
./configure --with-gui=qt5
make -j$(nproc)
```

## Quick Commands Reference

```bash
# Build from scratch
./autogen.sh && ./configure --with-gui=qt5 && make -j$(nproc)

# Rebuild after code changes
make -j$(nproc)

# Clean and rebuild
make clean && make -j$(nproc)

# Complete rebuild
make distclean && ./autogen.sh && ./configure --with-gui=qt5 && make -j$(nproc)

# Run tests
make check

# Run GUI
./src/qt/dogecoin-qt

# Run daemon
./src/dogecoind

# Check dashboard metrics
./src/dogecoin-cli getdashboardmetrics
```
