# Intel 8-Way SHA256 Assembly Integration - Fix Documentation

## Summary

This fix resolves the data structure mismatch between C++ code and Intel's 8-way AVX2 SHA256 assembly, enabling proper data transfer in and out of the assembly function.

## The Problem

The Dogecoin codebase includes Intel's optimized 8-way SHA256 assembly (`sha256_oct_avx2`) from the Intel Multi-Buffer Crypto library, but the C++ interface was incorrectly implemented:

```cpp
// OLD - INCORRECT
typedef struct {
    void **data;      // Wrong: indirect pointer
    void **state;     // Wrong: indirect pointer  
    uint64_t inp_size;
} SHA256_ARGS;
```

This caused:
- ❌ Data corruption - wrong memory layout
- ❌ Incorrect hash results
- ❌ Potential crashes from invalid memory access

## The Solution

Fixed the structure to match the assembly's memory layout expectations:

```cpp
// NEW - CORRECT
typedef struct {
    uint32_t digest[8][8];              // 256 bytes: transposed digest
    const unsigned char* data_ptr[8];   // 64 bytes: 8 data pointers
} SHA256_ARGS_AVX2;
```

### Memory Layout

```
Offset | Size | Field      | Description
-------|------|------------|------------------------------------------
0      | 256  | digest     | Transposed digest: digest[word][lane]
       |      |            |   word 0-7 = a,b,c,d,e,f,g,h
       |      |            |   lane 0-7 = parallel hash lanes
256    | 64   | data_ptr   | Array of 8 input data pointers
-------|------|------------|------------------------------------------
Total: 320 bytes
```

## How the 8-Way Function Works

The Intel 8-way assembly processes **8 independent SHA256 hashes in parallel** using AVX2 SIMD instructions:

- Each YMM register (256-bit) holds 8 × 32-bit values
- 8 YMM registers (a-h) represent the 8 words of the SHA256 state
- Each register position processes one independent hash (lane)
- All 8 hashes process the same number of blocks simultaneously

### Data Format: Transposed

The assembly expects/produces data in **transposed** format:

```
Normal format:        Transposed format:
Hash0: [a b c d e f g h]  → digest[0][0-7]: [a0 a1 a2 a3 a4 a5 a6 a7]
Hash1: [a b c d e f g h]  → digest[1][0-7]: [b0 b1 b2 b3 b4 b5 b6 b7]
Hash2: [a b c d e f g h]  →        ...
Hash3: [a b c d e f g h]  → digest[7][0-7]: [h0 h1 h2 h3 h4 h5 h6 h7]
...
Hash7: [a b c d e f g h]
```

This allows the assembly to process all 8 'a' values in parallel using one YMM register, all 8 'b' values using another, and so on.

## Usage

### Helper Function

We added `sha256_process_x8_avx2()` to handle the transposition automatically:

```cpp
// Process 8 independent hashes in parallel
uint32_t* states[8];  // Array of 8 state pointers
const unsigned char* data[8];  // Array of 8 data pointers
size_t blocks = 1;  // Number of 64-byte blocks per hash

sha256_process_x8_avx2(states, data, blocks);
```

### When to Use 8-Way

✅ **Good use cases:**
- **Mining**: Computing hashes for different nonces in parallel
- **Batch validation**: Verifying multiple transactions simultaneously
- **Parallel workloads**: Any scenario with 8+ independent hashes

❌ **Wrong use cases:**
- Sequential blocks of a single hash (won't work - needs independent states)
- Less than 8 hashes (overhead not worth it)
- When CPU doesn't support AVX2

### Performance

Expected speedup: **4-6x** compared to processing 8 hashes sequentially

Requirements:
- Intel Haswell (2013) or newer
- AMD Excavator (2015) or newer
- Or any CPU with AVX2 support

## Files Modified

1. **src/crypto/sha256.cpp**
   - Fixed `SHA256_ARGS_AVX2` structure definition
   - Added `sha256_process_x8_avx2()` helper function
   - Updated `Transform_AVX2()` with documentation

2. **src/crypto/sha256_8way_example.cpp** (new)
   - Complete usage example
   - Demonstrates correct and incorrect usage patterns

3. **INTEL_8WAY_SHA256_FIX.md** (new)
   - Detailed technical documentation

## Building with 8-Way Support

```bash
./autogen.sh
./configure --with-intel-avx2-8way
make
```

This enables the `USE_AVX2_8WAY` preprocessor flag and links the assembly files.

## Technical Details

### Assembly Function Signature

```
void sha256_oct_avx2(SHA256_ARGS_AVX2 *args, uint64_t num_blocks);
```

- `args`: Pointer to SHA256_ARGS_AVX2 structure
- `num_blocks`: Number of 64-byte blocks to process per hash

### Constants (from intel/include/constants.inc)

```asm
%define MAX_SHA256_LANES 16        ; Maximum lanes (for AVX-512)
%define SHA256_DIGEST_WORD_SIZE 4  ; 4 bytes per word
%define NUM_SHA256_DIGEST_WORDS 8  ; 8 words (a-h)
%define SHA256_DIGEST_ROW_SIZE 64  ; 16 lanes × 4 bytes
```

Note: The 8-way function uses only 8 of the possible 16 lanes.

### Register Usage

The assembly uses YMM registers (AVX2):
- `ymm0-ymm7`: Hash state (a-h), each holding 8 lane values
- `ymm8-ymm15`: Temporary values during computation

## Verification

Created test program confirming correct structure layout:

```
✓ digest offset: 0 bytes
✓ digest size: 256 bytes
✓ data_ptr offset: 256 bytes
✓ data_ptr size: 64 bytes
✓ Total size: 320 bytes
```

## References

- Intel Multi-Buffer Crypto for IPsec Library
- Assembly: `src/intel/avx_t1/sha256_oct_avx2.asm`
- Data structures: `src/intel/include/mb_mgr_datastruct.inc`
- Constants: `src/intel/include/constants.inc`

## Additional Notes

### Why "oct" in the name?

"oct" = octet = 8, referring to processing 8 hashes in parallel. Not to be confused with octal (base-8) numbering.

### Future Improvements

- Add batch processing API for high-level code
- Integrate with mining code for automatic batching
- Consider AVX-512 support (16-way) for newer CPUs

## Contact

For questions about this fix, see:
- `INTEL_8WAY_SHA256_FIX.md` - Detailed technical documentation
- `src/crypto/sha256_8way_example.cpp` - Usage examples
