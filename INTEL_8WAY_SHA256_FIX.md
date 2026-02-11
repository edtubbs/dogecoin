# Fix for Intel 8-Way SHA256 Assembly Data Structure Issue

## Problem
The code attempted to use Intel's 8-way AVX2 assembly (`sha256_oct_avx2`) but the data structure used to pass data in and out of the assembly didn't match what the assembly expected. This caused data corruption and incorrect hash results.

## Root Cause
The C++ code defined `SHA256_ARGS` incorrectly:

```cpp
// INCORRECT (old code)
typedef struct {
    void **data;      // Wrong: pointer to pointer array
    void **state;     // Wrong: pointer to pointer array  
    uint64_t inp_size;
} SHA256_ARGS;
```

The Intel assembly (`sha256_oct_avx2.asm`) expects a different structure layout defined in `intel/include/mb_mgr_datastruct.inc`:

```assembly
; SHA256_ARGS structure (from mb_mgr_datastruct.inc)
START_FIELDS
FIELD _digest_sha256,        SHA256_DIGEST_SIZE,        32  ; transposed digest
FIELD _data_ptr_sha256,      PTR_SZ*MAX_SHA256_LANES,   8   ; array of pointers
END_FIELDS
```

This translates to:
- **Offset 0**: Transposed digest data (256 bytes for 8-way)
  - Layout: `digest[word][lane]` where word=0-7 (a-h) and lane=0-7
- **Offset 256**: Array of 8 data pointers (64 bytes)

## Solution
Fixed the C++ structure to match the assembly expectations:

```cpp
// CORRECT (new code)
typedef struct {
    uint32_t digest[8][8];              // 256 bytes: transposed digest
    const unsigned char* data_ptr[8];   // 64 bytes: 8 data pointers
} SHA256_ARGS_AVX2;
```

### Data Layout Details
- **Total size**: 320 bytes
- **digest**: 8 digest words × 8 lanes × 4 bytes/word = 256 bytes
  - `digest[0][0..7]`: 'a' values for lanes 0-7
  - `digest[1][0..7]`: 'b' values for lanes 0-7
  - ...
  - `digest[7][0..7]`: 'h' values for lanes 0-7
- **data_ptr**: 8 pointers × 8 bytes/pointer = 64 bytes
  - `data_ptr[0]`: pointer to input data for lane 0
  - `data_ptr[1]`: pointer to input data for lane 1
  - ...
  - `data_ptr[7]`: pointer to input data for lane 7

## How to Use the 8-Way Function

The 8-way optimization is designed to process **8 independent hashes in parallel**, not to accelerate sequential blocks of a single hash.

### Correct Usage Pattern

```cpp
// Helper function added to sha256.cpp
static void sha256_process_x8_avx2(
    uint32_t* states[8],           // Array of 8 state pointers
    const unsigned char* data[8],  // Array of 8 data pointers
    size_t blocks                  // Number of blocks per hash
) {
    SHA256_ARGS_AVX2 args;
    
    // Transpose states into digest array
    for (int word = 0; word < 8; word++) {
        for (int lane = 0; lane < 8; lane++) {
            args.digest[word][lane] = states[lane][word];
        }
    }
    
    // Setup data pointers
    for (int lane = 0; lane < 8; lane++) {
        args.data_ptr[lane] = data[lane];
    }
    
    // Call assembly function
    sha256_oct_avx2(&args, blocks);
    
    // Transpose output back
    for (int word = 0; word < 8; word++) {
        for (int lane = 0; lane < 8; lane++) {
            states[lane][word] = args.digest[word][lane];
        }
    }
}
```

### When to Use 8-Way
- ✅ **Mining**: Computing hashes for many different nonces in parallel
- ✅ **Batch validation**: Verifying multiple transactions/blocks at once
- ✅ **Parallel workloads**: Any scenario with 8+ independent hashes

### When NOT to Use 8-Way
- ❌ **Sequential blocks**: Processing blocks 1-8 of a single hash (won't work correctly)
- ❌ **Single hash**: When you only have one hash to compute
- ❌ **Less than 8 hashes**: Overhead isn't worth it

## Changes Made

1. **src/crypto/sha256.cpp**:
   - Fixed `SHA256_ARGS_AVX2` structure definition (lines 57-66)
   - Added `sha256_process_x8_avx2()` helper function (lines 70-105)
   - Updated `Transform_AVX2()` to document correct usage (lines 309-327)

2. **src/crypto/sha256_8way_example.cpp** (new file):
   - Added comprehensive usage example
   - Documents correct vs incorrect usage patterns

## Verification

Created test program (`/tmp/test_sha256_struct.cpp`) that verified:
- ✅ Structure size: 320 bytes
- ✅ digest offset: 0 bytes  
- ✅ digest size: 256 bytes
- ✅ data_ptr offset: 256 bytes
- ✅ data_ptr size: 64 bytes

## Build Configuration

To enable 8-way AVX2 support:
```bash
./configure --with-intel-avx2-8way
```

This enables the `USE_AVX2_8WAY` preprocessor flag and links the assembly files.

## Performance Notes

The 8-way optimization provides significant speedup when:
- Processing 8 or more independent hashes
- Using CPUs with AVX2 support (Intel Haswell and newer, AMD Excavator and newer)
- Input data is already prepared (minimal setup overhead)

Expected speedup: 4-6x compared to processing 8 hashes sequentially, depending on CPU.

## References

- Assembly implementation: `src/intel/avx_t1/sha256_oct_avx2.asm`
- Data structure definition: `src/intel/include/mb_mgr_datastruct.inc`  
- Constants: `src/intel/include/constants.inc`
- Original Intel Multi-Buffer Crypto library documentation
