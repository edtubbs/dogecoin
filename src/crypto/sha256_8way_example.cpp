// Example: How to use the Intel 8-way SHA256 AVX2 assembly optimization
//
// This example demonstrates the correct way to use the sha256_oct_avx2 function
// to process 8 independent SHA256 hashes in parallel.
//
// The key insight is that the 8-way optimization is designed for batch processing
// of multiple INDEPENDENT hashes, not for accelerating sequential blocks of a 
// single hash.

#include <cstdint>
#include <cstring>
#include <cstdio>

// Structure matching Intel assembly expectations
typedef struct {
    uint32_t digest[8][8];              // Transposed digest: [word][lane]
    const unsigned char* data_ptr[8];   // 8 data pointers
} SHA256_ARGS_AVX2;

// External assembly function (implemented in sha256_oct_avx2.asm)
extern "C" void sha256_oct_avx2(SHA256_ARGS_AVX2 *args, uint64_t num_blocks);

// SHA256 initial hash values
static const uint32_t SHA256_INIT[8] = {
    0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul,
    0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul
};

// Example: Process 8 independent hashes in parallel
void example_8way_parallel() {
    printf("Example: Processing 8 independent SHA256 hashes in parallel\n\n");
    
    // Prepare 8 different input data blocks (each 64 bytes)
    unsigned char input_data[8][64];
    for (int i = 0; i < 8; i++) {
        memset(input_data[i], i, 64);  // Fill with different patterns
    }
    
    // Prepare 8 independent hash states
    uint32_t states[8][8];
    for (int i = 0; i < 8; i++) {
        memcpy(states[i], SHA256_INIT, sizeof(SHA256_INIT));
    }
    
    // Setup the SHA256_ARGS_AVX2 structure
    SHA256_ARGS_AVX2 args;
    
    // Step 1: Transpose input states into digest array
    // digest[word][lane] where word is a-h (0-7) and lane is the hash index (0-7)
    for (int word = 0; word < 8; word++) {
        for (int lane = 0; lane < 8; lane++) {
            args.digest[word][lane] = states[lane][word];
        }
    }
    
    // Step 2: Setup data pointers for each lane
    for (int lane = 0; lane < 8; lane++) {
        args.data_ptr[lane] = input_data[lane];
    }
    
    // Step 3: Call the 8-way assembly function
    // Process 1 block (64 bytes) for each of the 8 hashes
    sha256_oct_avx2(&args, 1);
    
    // Step 4: Transpose output back to individual states
    for (int word = 0; word < 8; word++) {
        for (int lane = 0; lane < 8; lane++) {
            states[lane][word] = args.digest[word][lane];
        }
    }
    
    // Now states[0..7] contain the updated hash states for each of the 8 independent hashes
    printf("✓ Successfully processed 8 independent hashes in parallel\n");
    
    // Print first few bytes of each resulting state
    printf("\nResulting states (first 4 words):\n");
    for (int i = 0; i < 8; i++) {
        printf("  Hash %d: %08x %08x %08x %08x ...\n", 
               i, states[i][0], states[i][1], states[i][2], states[i][3]);
    }
}

// COMMON MISTAKE: Trying to use 8-way for sequential blocks of a single hash
// This is NOT the correct use of the 8-way optimization!
void example_incorrect_usage() {
    printf("\n\n=== INCORRECT USAGE (for illustration only) ===\n");
    printf("DO NOT use 8-way to process sequential blocks of a single hash!\n");
    printf("The 8-way function expects 8 INDEPENDENT hashes, not 8 sequential blocks.\n");
    
    // Wrong approach: trying to hash 8 sequential blocks for one hash
    // This doesn't work because:
    // 1. Each lane must maintain its own independent state
    // 2. Sequential blocks need the previous block's output as input
    // 3. The 8-way function processes all lanes independently and in parallel
}

int main() {
    printf("Intel 8-Way SHA256 AVX2 Optimization - Usage Example\n");
    printf("======================================================\n\n");
    
    printf("Key Points:\n");
    printf("  1. The 8-way optimization processes 8 INDEPENDENT hashes in parallel\n");
    printf("  2. Each 'lane' has its own state and input data\n");
    printf("  3. All 8 lanes process the same number of blocks simultaneously\n");
    printf("  4. Data must be transposed in/out: digest[word][lane] format\n\n");
    
    example_8way_parallel();
    example_incorrect_usage();
    
    printf("\n\nFor sequential blocks of a single hash, use single-block functions instead:\n");
    printf("  - sha256_block_avx() for AVX\n");
    printf("  - sha256_block_sse() for SSE\n");
    
    return 0;
}
