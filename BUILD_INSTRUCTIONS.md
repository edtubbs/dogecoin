# Build Instructions After Dashboard Integration

## Issue: "undefined reference to main" Errors

If you encounter linker errors like:
```
/usr/bin/ld: undefined reference to `main'
make[2]: *** [Makefile:4066: bench/bench_dogecoin] Error 1
make[2]: *** [Makefile:4096: qt/dogecoin-qt] Error 1
```

This is because the build system needs to be regenerated after adding new source files.

## Solution: Regenerate Build System

After pulling this branch with the dashboard changes, you need to regenerate the build system:

### Step 1: Clean Previous Build
```bash
make clean
```

### Step 2: Regenerate Configure Script
```bash
./autogen.sh
```

### Step 3: Reconfigure
```bash
./configure [your configure options]
```

Common configure options:
- `--with-gui=qt5` - Enable Qt GUI
- `--enable-debug` - Enable debug build
- `--disable-wallet` - Disable wallet features (if not needed)
- `--with-incompatible-bdb` - Use system BDB (if needed)

Example:
```bash
./configure --with-gui=qt5
```

### Step 4: Build
```bash
make -j$(nproc)
```

## Why This Is Necessary

The dashboard integration added several new Qt source files:
- `src/qt/dashb0rd.cpp` and `.h`
- `src/qt/dashb0rdpage.cpp` and `.h`
- `src/qt/sparklinewidget.cpp` and `.h`

These files are listed in `src/Makefile.qt.include`, but the actual `Makefile` needs to be regenerated from the `.am` and `.include` files by running `./configure`.

## Modified GUI Files

The integration also modified:
- `src/qt/bitcoingui.cpp` and `.h` - Added dashboard tab
- `src/qt/walletframe.cpp` and `.h` - Integrated dashboard widget

## Alternative: If autogen.sh Fails

If `./autogen.sh` fails, you may need to install autotools:

```bash
# On Ubuntu/Debian
sudo apt-get install autoconf automake libtool

# On macOS with Homebrew
brew install autoconf automake libtool
```

## Verification

After successful build, you should be able to:

1. Run the Qt GUI:
   ```bash
   ./src/qt/dogecoin-qt
   ```

2. Access the dashboard via:
   - Click "Dashb0rd" button in the toolbar
   - OR press Alt+5

3. Use the RPC endpoint:
   ```bash
   ./src/dogecoin-cli getdashboardmetrics
   ```

## Troubleshooting

If you still get errors after regenerating:

1. **Try a completely clean build:**
   ```bash
   make distclean
   ./autogen.sh
   ./configure [options]
   make -j$(nproc)
   ```

2. **Check for missing dependencies:**
   - Qt5 development libraries
   - Boost libraries
   - BDB libraries (if wallet enabled)
   - libevent

3. **Check configure output:**
   Make sure Qt5 was found during configuration:
   ```
   checking for Qt5... yes
   ```

## Summary

The "undefined reference to main" error is a build system issue, not a code issue. The source files are correct, but the generated `Makefile` is out of sync. Simply regenerate it with `./autogen.sh` and `./configure`.
