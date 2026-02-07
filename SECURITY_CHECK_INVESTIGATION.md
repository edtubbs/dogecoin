# Security Check Investigation

## Problem Statement
User reports: "The security check fails in ci" and "Across all the targets"

## Current Status
- Latest commit: `122c740` (includes lief API fix)
- No recent completed CI runs visible with latest changes
- Previous lief API fix (commit b5602d5) updated security-check.py and symbol-check.py

## Possible Root Causes

### 1. lief API Usage Issues

**Potential Problem Areas:**
```python
# Line 253-254 in security-check.py:
etype = binary.format  # Is this still correct in lief 0.15+?
arch = binary.abstract.header.architecture  # Path might have changed?
```

**Investigation Needed:**
- Verify `binary.format` returns `lief.Binary.FORMATS.ELF/PE/MACHO`
- Verify `binary.abstract.header.architecture` works in lief 0.15+
- Check if property access syntax changed

### 2. Qt 6 Binary Characteristics

**Qt 6 might have:**
- New section names not in `EXPECTED_FLAGS` (security-check.py line 58-91)
- Different memory layout
- Additional libraries that aren't in allowed lists
- Different security feature implementation

**Current EXPECTED_FLAGS sections:**
```
.init, .plt, .plt.got, .plt.sec, .text, .fini (R+X)
.interp, .note.*, .gnu.*, .dynsym, .dynstr, .rela.*, .rodata, .eh_frame*, .qtmetadata, .gcc_except_table, .stapsdt.base (R)
.init_array, .fini_array, .dynamic, .got, .data, .bss (R+W)
```

**Qt 6 might add new sections like:**
- `.qt.metadata` (different from `.qtmetadata`?)
- Qt 6 specific sections
- CMake-related sections

### 3. Security Feature Detection

**Checks performed:**
- **ELF:** RELRO, BIND_NOW, Canary, NX, PIE, Separate Code
- **PE:** Canary, ASLR, NX, PIE
- **Mach-O:** PIE, NX, (Control Flow commented out)

**Potential Issues:**
- Qt 6 binaries built without hardening flags
- Static Qt library changing binary characteristics
- CMake not passing security flags correctly

### 4. Symbol Check Issues

Symbol-check.py might also be affected:
- Checks for allowed/disallowed library dependencies
- Checks for symbol version requirements (glibc, libstdc++, etc.)
- Qt 6 might pull in newer symbol versions

## Diagnostic Steps

### Step 1: Get Specific Error Messages
Need to know:
1. Which binary is failing? (dogecoind, dogecoin-qt, dogecoin-cli, etc.)
2. Which specific check is failing?
3. What is the exact error message?
4. Which platform(s)? (Linux, Windows, macOS)

### Step 2: Check lief API
```python
import lief
binary = lief.parse("dogecoin-qt")
print(f"Format: {binary.format}")
print(f"Arch: {binary.abstract.header.architecture}")
print(f"Format type: {type(binary.format)}")
print(f"Arch type: {type(binary.abstract.header.architecture)}")
```

### Step 3: Manual Security Check
Run security-check.py locally on Qt 6 binaries:
```bash
python3 contrib/devtools/security-check.py src/qt/dogecoin-qt
```

### Step 4: Compare Qt 5 vs Qt 6 Binaries
```bash
readelf -a dogecoin-qt-qt5 > qt5.txt
readelf -a dogecoin-qt-qt6 > qt6.txt
diff qt5.txt qt6.txt
```

## Potential Fixes

### Fix 1: lief API Correction
If property access changed:
```python
# Current:
etype = binary.format
arch = binary.abstract.header.architecture

# Might need to be:
etype = binary.abstract.format  # or binary.get_format()
arch = binary.abstract.architecture  # without .header
```

### Fix 2: Add Qt 6 Sections to EXPECTED_FLAGS
If Qt 6 has new sections:
```python
EXPECTED_FLAGS = {
    # ... existing ...
    '.qt6metadata': R,  # Example
    '.cmake_build': R,  # Example
}
```

### Fix 3: Ensure Security Flags in Qt Build
Add to qt.mk cmake_opts:
```makefile
$(package)_cmake_opts += -DCMAKE_POSITION_INDEPENDENT_CODE=ON
$(package)_cmake_opts += -DCMAKE_EXE_LINKER_FLAGS="-Wl,-z,relro -Wl,-z,now"
```

### Fix 4: Update Symbol Allowed Lists
If symbol-check.py is failing, might need to update:
- ELF_ALLOWED_LIBRARIES
- MAX_VERSIONS (glibc, libstdc++, etc.)

## Testing Plan

1. **Local Test:**
   - Build Qt 6 locally
   - Run security-check.py on binaries
   - Compare with Qt 5 binaries

2. **CI Test:**
   - Push potential fix
   - Check CI logs for detailed errors
   - Iterate based on specific failures

3. **Platform-Specific:**
   - Test on Linux first (easiest to debug)
   - Then Windows (PE format)
   - Finally macOS (Mach-O format)

## Next Steps

**Immediate:**
1. Get specific error messages from CI logs
2. Identify which check is failing
3. Determine if it's a parse error or check failure

**Once we have details:**
1. Apply appropriate fix from above
2. Test locally if possible
3. Push and verify in CI

## Reference

- lief 0.15 release notes: https://github.com/lief-project/LIEF/releases/tag/0.15.0
- Qt 6 CMake documentation: https://doc.qt.io/qt-6/cmake-get-started.html
- Security hardening flags: https://wiki.debian.org/Hardening
