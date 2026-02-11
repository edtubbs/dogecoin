# Qt 6.7.3 Upgrade Status Report

## Current Status: BLOCKED

After applying 21 fixes, Qt .pc files are still not being generated, preventing configure from finding Qt.

## Problem Summary

Qt 6 builds successfully but doesn't generate pkg-config (.pc) files despite multiple attempts to enable the feature.

### What We've Tried:
1. Removed `-no-pkg-config` flag
2. Added explicit `-pkg-config` flag
3. Added `-DFEATURE_pkg_config=ON` (cmake)
4. Added `-DQT_FEATURE_pkg_config=ON` (cmake with QT_ prefix)
5. Fixed PKG_CONFIG_PATH in multiple places
6. Cache invalidation
7. Debug output at various stages

### Result:
- Qt builds ✓
- Qt installs `.pc.in` template files ✓
- Actual `.pc` files (Qt6Core.pc, etc.) **DO NOT EXIST** ✗
- Configure cannot find Qt ✗

## Root Cause Analysis

**Hypothesis:** Qt 6 static builds do not support pkg-config file generation.

The depends system builds Qt as a static library for cross-compilation. Qt 6's pkg-config feature may only work for shared/dynamic library builds.

Evidence:
1. `.pc.in` templates exist (feature code is there)
2. Multiple cmake variables tried (all ignored)
3. Qt builds without errors (not a configuration issue)
4. No .pc files generated (feature not activating)

## Recommended Solutions

### Option 1: Use Qt cmake Config Files (RECOMMENDED)
Qt 6 installs `Qt6Config.cmake` and related cmake files. We should:
1. Modify `configure.ac` to use `find_package(Qt6 COMPONENTS Core Gui Network Widgets)`
2. This is the modern way to detect Qt 6
3. Works for both static and shared builds

### Option 2: Examine Bitcoin Core's Approach
Bitcoin Core successfully upgraded to Qt 6. We should:
1. Check their `depends/packages/qt.mk`
2. Check their `configure.ac` Qt detection
3. Copy their working approach

### Option 3: Manual Path Specification
For depends builds, we could:
1. Hard-code Qt paths from `depends/HOST/`
2. Set Qt variables directly in `config.site.in`
3. Bypass Qt detection entirely

## Next Steps

1. **Immediate:** Examine Bitcoin Core's Qt 6 implementation
2. **Short-term:** Implement cmake-based Qt detection
3. **Long-term:** Update to Qt 6's recommended detection method

## Files Modified (21 Fixes)

1. `configure.ac` - Qt6PlatformSupport removal
2. `depends/builders/default.mk` - TAR, CC/CXX
3. `depends/builders/linux.mk` - TAR
4. `depends/builders/darwin.mk` - TAR
5. `depends/funcs.mk` - patches_path
6. `depends/packages/native_qt.mk` - cmake version
7. `depends/packages/qt.mk` - Multiple pkg-config attempts
8. `depends/packages/native_cdrkit.mk` - fcommon
9. `depends/config.site.in` - PKG_CONFIG_PATH
10. `.github/workflows/ci.yml` - cmake, Ubuntu, lief, CC/CXX, debug
11. `contrib/devtools/security-check.py` - lief API
12. `contrib/devtools/symbol-check.py` - lief API

## Conclusion

The pkg-config approach has been exhausted. We need to switch to a different Qt detection method that works with Qt 6 static builds.

---
*Document created: 2026-02-11*
*Status: Awaiting decision on alternative approach*
