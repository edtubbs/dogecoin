# Troubleshooting Build Errors

## "undefined reference to main" Error

### Symptom
```
/usr/bin/ld: undefined reference to `main'
make[2]: *** [Makefile:4066: bench/bench_dogecoin] Error 1
```

### Root Cause

This error occurs when the build system's generated Makefile is out of sync with the source files. It typically happens after:
- Pulling new code changes
- Adding new source files
- Switching branches
- Interrupted or incomplete builds

### Solution

**Quick Fix:**
```bash
make clean
./autogen.sh
./configure --with-gui=qt5  # Add your configure options here
make -j$(nproc)
```

**If that doesn't work, do a complete rebuild:**
```bash
make distclean  # Remove all generated files
./autogen.sh
./configure --with-gui=qt5  # Add your configure options here
make -j$(nproc)
```

### Explanation

The error "undefined reference to main" means the linker cannot find the main() function. In the case of `bench_dogecoin`, the main() function is in `src/bench/bench_bitcoin.cpp` (lines 10-21).

The function exists in the source code, but the build system is not including it properly in the link command. This happens when:

1. **Object file missing**: `bench/bench_bitcoin.o` wasn't compiled
2. **Link command wrong**: The Makefile's link command doesn't list the object file
3. **Build state corrupted**: Previous build left inconsistent state

### Verification

After rebuilding, verify the bench binary works:
```bash
./src/bench/bench_dogecoin --help
```

## Similar Errors

### "undefined reference to main" for qt/dogecoin-qt

Same solution applies. The main() function is in `src/qt/bitcoin.cpp`.

### "undefined reference to main" for test executables

Same solution applies. Run `./autogen.sh` and `./configure` to regenerate build files.

## Prevention

To avoid these issues:
1. Always run `./autogen.sh` and `./configure` after pulling changes
2. Run `make clean` if you switch branches
3. If in doubt, do a clean build with `make distclean && ./autogen.sh && ./configure && make`

## Still Having Issues?

If the above doesn't fix the problem, check:

1. **Dependencies**: Make sure all required dependencies are installed
   ```bash
   sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
   ```

2. **Disk space**: Ensure you have enough disk space for the build

3. **Permissions**: Make sure you have write permissions in the build directory

4. **Compiler**: Verify your compiler works:
   ```bash
   gcc --version
   g++ --version
   ```

5. **Configuration log**: Check `config.log` for any configuration errors

## Getting Help

If none of the above solutions work:
1. Share your `config.log` file
2. Share the full build output (not just the error)
3. Specify your OS and version
4. List the exact configure options you used
