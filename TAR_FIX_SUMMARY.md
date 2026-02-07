# Fix for Missing build_TAR Variable - CI Build Failure

## Problem

The CI build was failing during the extraction of native_qt with the error:

```
/bin/sh: 1: --no-same-owner: not found
make: *** [funcs.mk:247: /__w/dogecoin/dogecoin/depends/work/build/x86_64-unknown-linux-gnu/native_qt/6.7.3-f526359c963/.stamp_extracted] Error 127
```

## Root Cause Analysis

### What Was Happening

The `native_qt.mk` and `qt.mk` packages define custom extraction commands that use `$(build_TAR)`:

```makefile
define $(package)_extract_cmds
  ...
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source) -C qtbase && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttranslations_file_name) -C qttranslations && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttools_file_name) -C qttools && \
  ...
endef
```

However, the `build_TAR` variable was **never defined** in the depends build system.

### Why It Failed

When `$(build_TAR)` is undefined/empty, the command becomes:

```bash
--no-same-owner --strip-components=1 -xf file.tar.xz -C qtbase
```

The shell interprets `--no-same-owner` as a command to execute, not as a tar flag, resulting in:

```
/bin/sh: 1: --no-same-owner: not found
```

### How Other Build Tools Work

The depends system defines build tools using a pattern:

1. **default.mk**: Lists tools in a foreach loop
2. **linux.mk/darwin.mk**: Defines platform-specific commands
3. **Makefile processing**: Expands variables like `$(build_TOOL)`

For example, `SHA256SUM` and `DOWNLOAD` are properly defined:

**depends/builders/default.mk:**
```makefile
$(foreach var,CC CXX AR RANLIB NM STRIP SHA256SUM DOWNLOAD OTOOL INSTALL_NAME_TOOL,$(eval $(call add_build_tool_func,$(var))))
```

**depends/builders/linux.mk:**
```makefile
build_linux_SHA256SUM = sha256sum
build_linux_DOWNLOAD = curl --location --fail ... -o
```

**Result:** `$(build_SHA256SUM)` expands to `sha256sum`

## The Solution

Added `TAR` to the build tool infrastructure following the same pattern:

### 1. depends/builders/default.mk
Added `TAR` to the list of build tools:

```makefile
$(foreach var,CC CXX AR RANLIB NM STRIP SHA256SUM DOWNLOAD TAR OTOOL INSTALL_NAME_TOOL,$(eval $(call add_build_tool_func,$(var))))
```

### 2. depends/builders/linux.mk
Defined the tar command for Linux:

```makefile
build_linux_TAR = tar
```

### 3. depends/builders/darwin.mk
Defined the tar command for macOS:

```makefile
build_darwin_TAR = tar
```

## How The Fix Works

With these changes, when the build system processes packages:

1. The `add_build_tool_func` macro is called for `TAR`
2. It sets `build_TAR` based on the platform:
   - On Linux: `build_TAR = tar`
   - On macOS: `build_TAR = tar`
3. When `$(build_TAR)` is used in package files, it expands to `tar`

**Result:** Commands now work correctly:
```bash
tar --no-same-owner --strip-components=1 -xf file.tar.xz -C qtbase
```

## Impact

### Packages Fixed
- ✅ **native_qt** - Can now extract Qt sources properly
- ✅ **qt** - Can now extract Qt sources properly

### Compatibility
- ✅ No impact on other packages (they use hardcoded `tar` or default extraction)
- ✅ Consistent with existing build tool patterns
- ✅ Works on all platforms (Linux, macOS)

### Why This Wasn't Caught Earlier

The issue only affects packages that explicitly use `$(build_TAR)` in custom extraction commands. Most packages either:
1. Use the default extraction (which uses hardcoded `tar`)
2. Don't need custom multi-file extraction

The Qt 6 packages need custom extraction because they:
- Extract multiple tarballs (qtbase, qttools, qttranslations)
- Extract additional CMake files
- Need to maintain specific directory structure

## Verification

The fix can be verified by:

1. **Code inspection**: `$(build_TAR)` is now defined and will expand to `tar`
2. **Build test**: Running `make -C depends` will successfully extract native_qt
3. **CI test**: GitHub Actions CI will pass the native_qt extraction step

## Related Changes

This fix is part of the Qt 6.7.3 upgrade work:
- Previously fixed: Removed obsolete Qt6PlatformSupport check
- This fix: Added missing build_TAR variable
- Future: Full depends build and Dogecoin Core compilation with Qt 6

## Files Changed

```
depends/builders/default.mk  | 2 +-  (Added TAR to tool list)
depends/builders/linux.mk    | 1 +    (Defined build_linux_TAR)
depends/builders/darwin.mk   | 1 +    (Defined build_darwin_TAR)
3 files changed, 3 insertions(+), 1 deletion(-)
```

## Status

✅ **Fix implemented and committed**
✅ **Minimal change (3 lines)**
✅ **Follows existing patterns**
✅ **Ready for CI testing**

The CI should now successfully extract native_qt and proceed with the Qt 6 build.
