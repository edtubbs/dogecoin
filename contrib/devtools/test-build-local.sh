#!/usr/bin/env bash
#
# Copyright (c) 2024 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Test build script for local validation before pushing to CI.
# This script performs basic build checks to catch errors early.
#
# Usage: ./contrib/devtools/test-build-local.sh [HOST]
#
# Examples:
#   ./contrib/devtools/test-build-local.sh                     # Native build
#   ./contrib/devtools/test-build-local.sh x86_64-pc-linux-gnu # Linux build
#   ./contrib/devtools/test-build-local.sh i686-w64-mingw32    # Windows 32-bit

set -e

export LC_ALL=C

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default to native build if no host specified
HOST="${1:-}"

echo -e "${GREEN}=== Dogecoin Build Test ===${NC}"
echo "Testing build configuration..."
echo ""

# Check if we're in the repository root
if [ ! -f "configure.ac" ]; then
    echo -e "${RED}Error: Must be run from repository root${NC}"
    exit 1
fi

# Step 1: Run autogen
echo -e "${YELLOW}Step 1/4: Running autogen.sh...${NC}"
if ! ./autogen.sh > /tmp/autogen.log 2>&1; then
    echo -e "${RED}✗ autogen.sh failed${NC}"
    cat /tmp/autogen.log
    exit 1
fi
echo -e "${GREEN}✓ autogen.sh succeeded${NC}"
echo ""

# Step 2: Test configure
echo -e "${YELLOW}Step 2/4: Testing configure...${NC}"
CONFIGURE_OPTS="--disable-tests --disable-bench --with-gui=qt6"
if [ -n "$HOST" ]; then
    # If building with depends
    echo "Building for host: $HOST"
    if [ -d "depends/$HOST" ]; then
        CONFIGURE_OPTS="--prefix=$(pwd)/depends/$HOST $CONFIGURE_OPTS"
    else
        echo -e "${YELLOW}Warning: depends/$HOST not found. You may need to build depends first:${NC}"
        echo "  make -C depends HOST=$HOST"
    fi
fi

if ! ./configure $CONFIGURE_OPTS > /tmp/configure.log 2>&1; then
    echo -e "${RED}✗ configure failed${NC}"
    echo "Last 50 lines of config.log:"
    tail -50 config.log 2>/dev/null || true
    echo ""
    echo "Last 50 lines of configure output:"
    tail -50 /tmp/configure.log
    exit 1
fi
echo -e "${GREEN}✓ configure succeeded${NC}"
echo ""

# Step 3: Test make (just compilation, not full build)
echo -e "${YELLOW}Step 3/4: Testing compilation...${NC}"
echo "Note: This only tests compilation of a few files, not a full build"
if ! make -j$(nproc) src/bitcoind > /tmp/make.log 2>&1; then
    echo -e "${RED}✗ compilation failed${NC}"
    echo "Last 100 lines of build output:"
    tail -100 /tmp/make.log
    exit 1
fi
echo -e "${GREEN}✓ compilation test succeeded${NC}"
echo ""

# Step 4: Summary
echo -e "${GREEN}=== Build Test Summary ===${NC}"
echo -e "${GREEN}✓ All basic build checks passed${NC}"
echo ""
echo "Build artifacts created in:"
echo "  - configure script"
echo "  - Makefiles"
echo "  - src/bitcoind (partial)"
echo ""
echo -e "${YELLOW}Note: This is a quick build test. For full validation:${NC}"
echo "  1. Build depends: make -C depends HOST=<host>"
echo "  2. Configure:     ./configure --prefix=\$(pwd)/depends/<host>"
echo "  3. Build:         make -j\$(nproc)"
echo "  4. Test:          make check"
echo ""
echo -e "${GREEN}Ready to push!${NC}"
