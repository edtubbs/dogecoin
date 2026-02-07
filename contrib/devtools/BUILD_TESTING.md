# Testing Builds Before Pushing

To avoid CI failures, it's important to test builds locally before pushing changes that affect the build system.

## Quick Build Test

We provide a quick build test script that validates your changes:

```bash
./contrib/devtools/test-build-local.sh
```

This script will:
1. Run `autogen.sh` to generate configure script
2. Run `configure` with common options
3. Test compilation of core binaries
4. Report any errors

## Setting Up Pre-Push Hook (Recommended)

To automatically remind yourself to test builds before pushing, install the pre-push hook:

```bash
cp contrib/devtools/pre-push.sample .git/hooks/pre-push
chmod +x .git/hooks/pre-push
```

This hook will:
- Detect if you're pushing build-related changes
- Remind you to test the build
- Ask for confirmation before pushing

You can bypass it with `git push --no-verify` if needed (not recommended).

## Full Build Testing

For comprehensive testing, especially when modifying Qt or dependencies:

### 1. Build Dependencies

For Linux (native):
```bash
make -C depends HOST=x86_64-pc-linux-gnu
```

For Windows cross-compile:
```bash
make -C depends HOST=x86_64-w64-mingw32
```

For macOS cross-compile (requires macOS SDK):
```bash
make -C depends HOST=x86_64-apple-darwin11
```

### 2. Configure

```bash
./autogen.sh
./configure --prefix=$(pwd)/depends/<HOST> --with-gui=qt6
```

Replace `<HOST>` with the appropriate host triplet (e.g., `x86_64-pc-linux-gnu`).

### 3. Build

```bash
make -j$(nproc)
```

### 4. Test

```bash
make check
```

## Common Build Issues

### Qt 6 Related Issues

If you're working on Qt 6 changes, make sure:
- All Qt 6 patches are present in `depends/patches/qt/`
- `depends/packages/qt_details.mk` has correct version and hashes
- `depends/packages/native_qt.mk` exists for cross-compilation
- `configure.ac` references Qt 6, not Qt 5

### Dependency Issues

If depends build fails:
- Check `depends/packages/*.mk` files for syntax errors
- Verify all referenced patches exist
- Check SHA256 hashes match downloaded files

### Configure Issues

If configure fails:
- Check `config.log` for detailed error messages
- Verify all AC_CHECK_LIB calls reference libraries that exist
- Ensure autoconf macros in `build-aux/m4/` are correct

## CI Matrix

The CI tests multiple configurations:
- Linux: x86_64, i686, ARM, AArch64
- Windows: 32-bit and 64-bit (MinGW cross-compile)
- macOS: x86_64 (cross-compile)
- With and without Qt, wallet, ZMQ, experimental features

Your changes should work with all configurations unless you're specifically targeting one platform.

## Debugging CI Failures

If CI fails after you've pushed:

1. Check the GitHub Actions logs for the specific error
2. Identify which build configuration failed
3. Try to reproduce locally with the same configuration
4. Fix the issue and test again before pushing

Example for Windows build:
```bash
make -C depends HOST=x86_64-w64-mingw32
./configure --prefix=$(pwd)/depends/x86_64-w64-mingw32 --enable-gui=qt6
make -j$(nproc)
```

## Tips

- **Always run autogen after pulling changes** to `configure.ac` or `Makefile.am`
- **Clean builds** occasionally with `make clean` or `make distclean`
- **Test with minimal options** first, then add more complex configurations
- **Check both with and without Qt** if you're modifying build files
- **Use ccache** to speed up rebuilds: `sudo apt-get install ccache`

## Resources

- [Depends README](../depends/README.md) - Detailed information about the depends system
- [Building Dogecoin](../../doc/build-unix.md) - Platform-specific build instructions
- [CI Configuration](../../.github/workflows/ci.yml) - See what CI tests
