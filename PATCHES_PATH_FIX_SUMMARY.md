# Fix for Custom Patch Directory Support - CI Build Failure

## Problem

The CI build was failing during native_qt preprocessing with:

```
/bin/sh: 1: cd: can't cd to /__w/dogecoin/dogecoin/depends/patches/native_qt
cp: cannot stat 'dont_hardcode_pwd.patch': No such file or directory
/bin/sh: 1: cd: can't cd to /__w/dogecoin/dogecoin/depends/patches/native_qt
cp: cannot stat 'qtbase_skip_tools.patch': No such file or directory
/bin/sh: 1: cd: can't cd to /__w/dogecoin/dogecoin/depends/patches/native_qt
cp: cannot stat 'rcc_hardcode_timestamp.patch': No such file or directory
/bin/sh: 1: cd: can't cd to /__w/dogecoin/dogecoin/depends/patches/native_qt
cp: cannot stat 'qttools_skip_dependencies.patch': No such file or directory
make: *** [funcs.mk:247: .../native_qt/6.7.3-e7a513cad49/.stamp_preprocessed] Error 1
```

## Root Cause Analysis

### The Package Configuration

Both `native_qt.mk` and `qt.mk` packages share patches from a common `qt` directory:

**depends/packages/native_qt.mk:**
```makefile
include packages/qt_details.mk
$(package)_patches_path := $(qt_details_patches_path)
$(package)_patches := dont_hardcode_pwd.patch
$(package)_patches += qtbase_skip_tools.patch
$(package)_patches += rcc_hardcode_timestamp.patch
$(package)_patches += qttools_skip_dependencies.patch
```

**depends/packages/qt_details.mk:**
```makefile
qt_details_patches_path := $(PATCHES_PATH)/qt
```

So `native_qt` explicitly sets its `patches_path` to `patches/qt/` because it shares patches with the regular `qt` package.

### The Build System Issue

The depends build system in `funcs.mk` had two places where patch paths were hardcoded:

**1. Line 40 - Checksum calculation:**
```makefile
$(eval $(1)_all_file_checksums:=$(shell $(build_SHA256SUM) ... $(addprefix $(PATCHES_PATH)/$(1)/,$($(1)_patches)) ...))
```
This assumed patches are in `patches/<package_name>/`

**2. Line 179 - Preprocessing (copying patches):**
```makefile
$(AT)$(foreach patch,$($(1)_patches),cd $(PATCHES_PATH)/$(1); cp $(patch) $($(1)_patch_dir) ;)
```
This tried to `cd $(PATCHES_PATH)/$(1)` which for native_qt means `patches/native_qt`

### Why It Failed

1. `native_qt` defines `patches_path` pointing to `patches/qt`
2. But the build system ignores this variable and uses `patches/native_qt`
3. The directory `patches/native_qt` doesn't exist
4. The `cd` command fails, and patches can't be copied

## Directory Structure

**Actual structure:**
```
depends/
├── patches/
│   ├── qt/                         ← Patches are here
│   │   ├── dont_hardcode_pwd.patch
│   │   ├── qtbase_skip_tools.patch
│   │   ├── rcc_hardcode_timestamp.patch
│   │   └── qttools_skip_dependencies.patch
│   ├── boost/
│   ├── openssl/
│   └── ...
└── packages/
    ├── native_qt.mk               ← Wants to use patches/qt/
    ├── qt.mk                      ← Wants to use patches/qt/
    └── ...
```

**What the build system was looking for:**
```
depends/
└── patches/
    └── native_qt/                 ← Doesn't exist!
        ├── dont_hardcode_pwd.patch
        └── ...
```

## The Solution

Added support for custom patch directories with backward compatibility:

### 1. Added Default patches_path (Line 82)

**depends/funcs.mk:**
```makefile
#default commands
$(1)_patches_path ?= $(PATCHES_PATH)/$(1)
```

This provides a default value if the package doesn't define `patches_path`.
- The `?=` operator means "assign only if not already defined"
- For packages like `boost` or `openssl` that don't define it: defaults to `patches/<package_name>`
- For packages like `native_qt` that do define it: uses the custom value

### 2. Updated Checksum Calculation (Line 40)

**Before:**
```makefile
$(addprefix $(PATCHES_PATH)/$(1)/,$($(1)_patches))
```

**After:**
```makefile
$(addprefix $($(1)_patches_path)/,$($(1)_patches))
```

Now uses the `patches_path` variable which can be custom or default.

### 3. Updated Preprocessing (Line 180)

**Before:**
```makefile
$(AT)$(foreach patch,$($(1)_patches),cd $(PATCHES_PATH)/$(1); cp $(patch) $($(1)_patch_dir) ;)
```

**After:**
```makefile
$(AT)$(foreach patch,$($(1)_patches),cd $($(1)_patches_path); cp $(patch) $($(1)_patch_dir) ;)
```

Now uses the `patches_path` variable to cd into the correct directory.

## How The Fix Works

### For Packages with Custom patches_path

**Example: native_qt**

1. Package defines: `$(package)_patches_path := $(PATCHES_PATH)/qt`
2. Default line is skipped (already defined)
3. Result: `native_qt_patches_path = depends/patches/qt`
4. Patches are found in `patches/qt/` ✓

### For Packages without Custom patches_path

**Example: boost, openssl, etc.**

1. Package doesn't define `patches_path`
2. Default applies: `$(package)_patches_path ?= $(PATCHES_PATH)/$(package)`
3. Result: `boost_patches_path = depends/patches/boost`
4. Patches are found in `patches/boost/` ✓

## Impact

### Packages Fixed
- ✅ **native_qt** - Can now find patches in `patches/qt/`
- ✅ **qt** - Can now find patches in `patches/qt/`

### Compatibility
- ✅ All other packages work unchanged (default path still applies)
- ✅ No breaking changes
- ✅ Clean design pattern for shared patches

### Use Cases Enabled

This fix enables packages to:
1. **Share patches** - Multiple packages can use the same patch directory
2. **Organize patches** - Logical grouping beyond package names
3. **Reduce duplication** - Native and cross-compile variants share patches

## Verification

The fix can be verified by checking that:

1. **native_qt patches_path is defined:**
   ```bash
   $ grep patches_path depends/packages/native_qt.mk
   $(package)_patches_path := $(qt_details_patches_path)
   ```

2. **Patches exist in qt directory:**
   ```bash
   $ ls depends/patches/qt/
   dont_hardcode_pwd.patch
   qtbase_skip_tools.patch
   rcc_hardcode_timestamp.patch
   qttools_skip_dependencies.patch
   ```

3. **Build system uses the variable:**
   ```bash
   $ grep patches_path depends/funcs.mk
   $(1)_patches_path ?= $(PATCHES_PATH)/$(1)
   ... $($(1)_patches_path)/ ...
   ```

## Related Changes

This is part of the Qt 6.7.3 upgrade series:

1. **Configure fix**: Removed obsolete Qt6PlatformSupport library check
2. **TAR variable fix**: Added missing build_TAR variable definition
3. **This fix**: Support for custom patch directories

All three were needed for the Qt 6 build to proceed.

## Files Changed

```
depends/funcs.mk  | 3 ++-  (Added default, updated 2 uses)
1 file changed, 3 insertions(+), 2 deletions(-)
```

**Changes:**
- Line 82: Added `$(1)_patches_path ?= $(PATCHES_PATH)/$(1)`
- Line 40: Changed to use `$($(1)_patches_path)/`
- Line 180: Changed to use `$($(1)_patches_path)`

## Design Pattern

This establishes a pattern for the depends system:

```makefile
# In qt_details.mk (shared config):
qt_details_patches_path := $(PATCHES_PATH)/qt

# In native_qt.mk:
$(package)_patches_path := $(qt_details_patches_path)  # Share qt patches
$(package)_patches := dont_hardcode_pwd.patch
$(package)_patches += qtbase_skip_tools.patch

# In qt.mk:
$(package)_patches_path := $(qt_details_patches_path)  # Share qt patches
$(package)_patches := dont_hardcode_pwd.patch
$(package)_patches += qtbase_skip_tools.patch
$(package)_patches += qtbase_avoid_qmain.patch          # Plus extra patches

# In funcs.mk:
$(1)_patches_path ?= $(PATCHES_PATH)/$(1)              # Default
... use $($(1)_patches_path) everywhere ...            # Apply
```

This allows flexible patch management while maintaining simplicity for simple cases.

## Status

✅ **Fix implemented and committed**
✅ **Backward compatible**
✅ **Minimal change (3 lines)**
✅ **Ready for CI testing**

The CI should now successfully preprocess native_qt and proceed with the build.
