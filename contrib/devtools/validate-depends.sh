#!/usr/bin/env bash
#
# Validate depends configuration for Dogecoin Core
# This script checks that all package definitions and patches are correctly configured
#

set -e

export LC_ALL=C

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ERRORS=0
WARNINGS=0

echo -e "${GREEN}=== Depends Configuration Validator ===${NC}"
echo ""

# Check if we're in the repository root
if [ ! -f "depends/Makefile" ]; then
    echo -e "${RED}Error: Must be run from repository root${NC}"
    exit 1
fi

cd depends

echo -e "${YELLOW}Checking package definitions...${NC}"

# Check that all referenced packages have .mk files
for pkg in boost openssl libevent zeromq qrencode zlib qt bdb miniupnpc; do
    if [ -f "packages/$pkg.mk" ]; then
        echo -e "${GREEN}✓${NC} packages/$pkg.mk exists"
    else
        echo -e "${RED}✗${NC} packages/$pkg.mk MISSING"
        ERRORS=$((ERRORS + 1))
    fi
done

# Check Qt 6 specific packages
echo ""
echo -e "${YELLOW}Checking Qt 6 packages...${NC}"
for pkg in qt qt_details native_qt; do
    if [ -f "packages/$pkg.mk" ]; then
        echo -e "${GREEN}✓${NC} packages/$pkg.mk exists"
    else
        echo -e "${RED}✗${NC} packages/$pkg.mk MISSING"
        ERRORS=$((ERRORS + 1))
    fi
done

# Check xcb packages for Linux Qt builds
echo ""
echo -e "${YELLOW}Checking XCB packages (Linux Qt dependencies)...${NC}"
for pkg in libxcb xcb_proto libXau xproto freetype fontconfig libxkbcommon \
           libxcb_util libxcb_util_cursor libxcb_util_render libxcb_util_keysyms \
           libxcb_util_image libxcb_util_wm; do
    if [ -f "packages/$pkg.mk" ]; then
        echo -e "${GREEN}✓${NC} packages/$pkg.mk exists"
    else
        echo -e "${RED}✗${NC} packages/$pkg.mk MISSING"
        ERRORS=$((ERRORS + 1))
    fi
done

# Check Qt patches
echo ""
echo -e "${YELLOW}Checking Qt patches...${NC}"
for patch in dont_hardcode_pwd.patch qtbase_avoid_qmain.patch qtbase_platformsupport.patch \
             qtbase_plugins_cocoa.patch qtbase_plugins_windows11style.patch qtbase_skip_tools.patch \
             qttools_skip_dependencies.patch rcc_hardcode_timestamp.patch static_fixes.patch; do
    if [ -f "patches/qt/$patch" ]; then
        echo -e "${GREEN}✓${NC} patches/qt/$patch exists"
    else
        echo -e "${RED}✗${NC} patches/qt/$patch MISSING"
        ERRORS=$((ERRORS + 1))
    fi
done

# Check qt_details.mk content
echo ""
echo -e "${YELLOW}Checking Qt version configuration...${NC}"
if [ -f "packages/qt_details.mk" ]; then
    QT_VERSION=$(grep "^qt_details_version" packages/qt_details.mk | cut -d'=' -f2 | tr -d ' ')
    QT_PATCHES_PATH=$(grep "^qt_details_patches_path" packages/qt_details.mk)
    
    if [ -n "$QT_VERSION" ]; then
        echo -e "${GREEN}✓${NC} Qt version: $QT_VERSION"
    else
        echo -e "${RED}✗${NC} Qt version not found in qt_details.mk"
        ERRORS=$((ERRORS + 1))
    fi
    
    if [[ "$QT_PATCHES_PATH" == *"/qt" ]]; then
        echo -e "${GREEN}✓${NC} Qt patches path correctly configured"
    else
        echo -e "${RED}✗${NC} Qt patches path incorrect: $QT_PATCHES_PATH"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo -e "${RED}✗${NC} packages/qt_details.mk not found"
    ERRORS=$((ERRORS + 1))
fi

# Check that qt.mk references qt_details.mk
echo ""
echo -e "${YELLOW}Checking qt.mk references qt_details...${NC}"
if grep -q "include packages/qt_details.mk" packages/qt.mk 2>/dev/null; then
    echo -e "${GREEN}✓${NC} qt.mk includes qt_details.mk"
else
    echo -e "${RED}✗${NC} qt.mk doesn't include qt_details.mk"
    ERRORS=$((ERRORS + 1))
fi

# Check that native_qt.mk references qt_details.mk
if grep -q "include packages/qt_details.mk" packages/native_qt.mk 2>/dev/null; then
    echo -e "${GREEN}✓${NC} native_qt.mk includes qt_details.mk"
else
    echo -e "${RED}✗${NC} native_qt.mk doesn't include qt_details.mk"
    ERRORS=$((ERRORS + 1))
fi

# Check packages.mk configuration
echo ""
echo -e "${YELLOW}Checking packages.mk configuration...${NC}"
if grep -q "qt_x86_64_linux_packages.*libxcb_util" packages/packages.mk; then
    echo -e "${GREEN}✓${NC} Linux Qt packages include XCB utilities"
else
    echo -e "${YELLOW}⚠${NC} Linux Qt packages may not include all XCB utilities"
    WARNINGS=$((WARNINGS + 1))
fi

if grep -q "qt_native_packages.*:=.*native_qt" packages/packages.mk; then
    echo -e "${GREEN}✓${NC} native_qt configured for cross-compilation"
else
    echo -e "${YELLOW}⚠${NC} native_qt may not be configured"
    WARNINGS=$((WARNINGS + 1))
fi

# Summary
echo ""
echo -e "${GREEN}=== Validation Summary ===${NC}"
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "Depends configuration is correct and ready to build."
    echo ""
    echo "To build depends:"
    echo "  make -C depends HOST=x86_64-pc-linux-gnu -j\$(nproc)"
    echo ""
    echo "Or for Windows:"
    echo "  make -C depends HOST=x86_64-w64-mingw32 -j\$(nproc)"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ Validation passed with $WARNINGS warning(s)${NC}"
    echo ""
    echo "Depends configuration should work, but please review warnings."
    exit 0
else
    echo -e "${RED}✗ Validation failed with $ERRORS error(s) and $WARNINGS warning(s)${NC}"
    echo ""
    echo "Please fix the errors before attempting to build depends."
    exit 1
fi
