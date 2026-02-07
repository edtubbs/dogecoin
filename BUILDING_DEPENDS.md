# Building Dogecoin Depends

This guide explains how to build the Dogecoin Core dependencies system.

## Overview

The depends system builds all dependencies from source in a deterministic way, ensuring reproducible builds across different platforms. This is especially important for cross-compilation and release builds.

## Prerequisites

### Ubuntu/Debian

Install the basic build tools:

```bash
sudo apt-get update
sudo apt-get install -y autoconf automake make binutils ca-certificates \
    curl git-core libtool pkg-config python3 bison g++
```

### For Linux Builds (x86_64)

```bash
sudo apt-get install -y g++-multilib gcc-multilib
```

### For Windows Cross-Compilation

```bash
sudo apt-get install -y g++-mingw-w64 mingw-w64 nsis zip
```

### For macOS Cross-Compilation

```bash
sudo apt-get install -y cmake imagemagick fonts-tuffy libz-dev libbz2-dev \
    libcap-dev librsvg2-bin libtiff-tools libtinfo5 xorriso
```

## Building Dependencies

### Native Build (Current Architecture)

To build dependencies for your current system:

```bash
cd depends
make -j$(nproc)
```

This will:
1. Download all source tarballs
2. Verify their checksums
3. Extract and patch the sources
4. Build each dependency in order
5. Install to `depends/<host-triplet>/`

### Cross-Compilation

For Windows (64-bit):
```bash
cd depends
make HOST=x86_64-w64-mingw32 -j$(nproc)
```

For Windows (32-bit):
```bash
cd depends
make HOST=i686-w64-mingw32 -j$(nproc)
```

For macOS:
```bash
cd depends
make HOST=x86_64-apple-darwin11 -j$(nproc)
```

For ARM64 Linux:
```bash
cd depends
make HOST=aarch64-linux-gnu -j$(nproc)
```

## Using Built Dependencies

After building depends, use the prefix with configure:

```bash
./autogen.sh
./configure --prefix=$(pwd)/depends/x86_64-pc-linux-gnu
make -j$(nproc)
```

## Build Options

### Disable Qt GUI

To build without Qt (no GUI wallet):

```bash
make NO_QT=1 -j$(nproc)
```

### Disable Wallet

To build without wallet functionality:

```bash
make NO_WALLET=1 -j$(nproc)
```

### Disable UPnP

To build without UPnP support:

```bash
make NO_UPNP=1 -j$(nproc)
```

### Debug Build

For a debug build with less optimization:

```bash
make DEBUG=1 -j$(nproc)
```

## Qt 6 Support

This version of Dogecoin Core uses **Qt 6.7.3** for the GUI wallet.

### Qt 6 Packages

The depends system includes:
- **qtbase** - Core Qt framework
- **qttools** - Qt tools (linguist, etc.)
- **qttranslations** - Qt translations
- **native_qt** - Native Qt for cross-compilation

### Qt 6 Patches

The following patches are applied to Qt:
- `dont_hardcode_pwd.patch` - Remove hardcoded paths
- `qtbase_avoid_qmain.patch` - Avoid qmain wrapper
- `qtbase_platformsupport.patch` - Platform support fixes
- `qtbase_plugins_cocoa.patch` - macOS Cocoa plugin fixes
- `qtbase_plugins_windows11style.patch` - Windows 11 style support
- `qtbase_skip_tools.patch` - Skip unnecessary Qt tools
- `qttools_skip_dependencies.patch` - Skip unnecessary dependencies
- `rcc_hardcode_timestamp.patch` - Reproducible resource builds
- `static_fixes.patch` - Static linking fixes

### Linux-Specific Qt Dependencies

On Linux, Qt 6 requires additional X11/XCB libraries:
- libxcb, xcb_proto, libXau, xproto
- freetype, fontconfig
- libxkbcommon
- libxcb_util (cursor, render, keysyms, image, wm)

## Package List

### Core Packages (Always Built)
- **boost** (1.63.0) - C++ libraries
- **openssl** - Cryptography library
- **libevent** - Event notification library
- **zeromq** - Messaging library

### Qt Packages (Built with Qt GUI)
- **qt** (6.7.3) - Qt framework
- **qrencode** - QR code generation
- **zlib** - Compression library
- Plus X11/XCB libraries on Linux

### Wallet Packages (Built with Wallet)
- **bdb** (Berkeley DB) - Wallet database

### Optional Packages
- **miniupnpc** - UPnP support (NO_UPNP=0)
- **intel-ipsec-mb** - AVX2 crypto acceleration (experimental)
- **native_nasm** - Required for AVX2 builds

### Native Build Tools
- **native_ccache** - Compiler cache
- **native_qt** - Host Qt for cross-compilation (when cross-compiling)

### macOS-Specific Packages
- native_biplist, native_ds_store, native_mac_alias
- native_cctools, native_cdrkit, native_libdmg-hfsplus (when not on macOS)

## Troubleshooting

### Download Failures

If downloads fail, the system tries a fallback mirror at `https://depends.dogecoincore.org`.

You can also manually download sources to `depends/sources/` and the build will use them.

### Clean Build

To clean and rebuild everything:

```bash
cd depends
make clean
make -j$(nproc)
```

### Clean Specific Package

To rebuild just one package (e.g., qt):

```bash
cd depends
rm -rf work/build/x86_64-pc-linux-gnu/qt
rm -rf built/x86_64-pc-linux-gnu/qt
make -j$(nproc)
```

### Check What Will Be Built

To see what packages will be built:

```bash
cd depends
make print-host-triplet
# Then check depends/packages/packages.mk for the package list
```

## Build Output

The build system creates:
- `depends/sources/` - Downloaded source tarballs
- `depends/work/` - Extracted sources and build artifacts
- `depends/built/` - Built package files
- `depends/<host-triplet>/` - Final installation prefix

## Network Requirements

Building depends requires internet access to download source packages from:
- download.qt.io (Qt sources)
- sourceforge.net (Boost)
- github.com (Various packages)
- depends.dogecoincore.org (Fallback mirror)

## Time Estimates

Build times vary by system and configuration:
- Native Linux build: 30-60 minutes
- Windows cross-compile: 40-70 minutes
- macOS cross-compile: 50-80 minutes
- With NO_QT=1: ~20 minutes

Using ccache can significantly speed up rebuilds.

## Integration with CI

The CI system pre-builds and caches dependencies using the hash of:
- `depends/packages/*` files
- `.github/workflows/ci.yml` configuration

When these change, the cache is invalidated and depends is rebuilt.

## More Information

- See `depends/README.md` for detailed depends system documentation
- See `depends/packages.md` for package format documentation
- Check individual package files in `depends/packages/*.mk` for specifics
- Review patches in `depends/patches/` for applied modifications
