Contents
========
This directory contains tools for developers working on this repository.

check-doc.py
============

Check if all command line args are documented. The return value indicates the
number of undocumented args.

clang-format-diff.py
===================

A script to format unified git diffs according to [.clang-format](../../src/.clang-format).

For instance, to format the last commit with 0 lines of context,
the script should be called from the git root folder as follows.

```
git diff -U0 HEAD~1.. | ./contrib/devtools/clang-format-diff.py -p1 -i -v
```

copyright\_header.py
====================

Provides utilities for managing copyright headers of `The Bitcoin Core
developers` in repository source files. It has three subcommands:

```
$ ./copyright_header.py report <base_directory> [verbose]
$ ./copyright_header.py update <base_directory>
$ ./copyright_header.py insert <file>
```
Running these subcommands without arguments displays a usage string.

copyright\_header.py report \<base\_directory\> [verbose]
---------------------------------------------------------

Produces a report of all copyright header notices found inside the source files
of a repository. Useful to quickly visualize the state of the headers.
Specifying `verbose` will list the full filenames of files of each category.

copyright\_header.py update \<base\_directory\> [verbose]
---------------------------------------------------------
Updates all the copyright headers of `The Bitcoin Core developers` which were
changed in a year more recent than is listed. For example:
```
// Copyright (c) <firstYear>-<lastYear> The Bitcoin Core developers
```
will be updated to:
```
// Copyright (c) <firstYear>-<lastModifiedYear> The Bitcoin Core developers
```
where `<lastModifiedYear>` is obtained from the `git log` history.

This subcommand also handles copyright headers that have only a single year. In
those cases:
```
// Copyright (c) <year> The Bitcoin Core developers
```
will be updated to:
```
// Copyright (c) <year>-<lastModifiedYear> The Bitcoin Core developers
```
where the update is appropriate.

copyright\_header.py insert \<file\>
------------------------------------
Inserts a copyright header for `The Bitcoin Core developers` at the top of the
file in either Python or C++ style as determined by the file extension. If the
file is a Python file and it has  `#!` starting the first line, the header is
inserted in the line below it.

The copyright dates will be set to be `<year_introduced>-<current_year>` where
`<year_introduced>` is according to the `git log` history. If
`<year_introduced>` is equal to `<current_year>`, it will be set as a single
year rather than two hyphenated years.

If the file already has a copyright for `The Bitcoin Core developers`, the
script will exit.

gen-manpages.sh
===============

A small script to automatically create manpages in ../../doc/man by running the release binaries with the -help option.
This requires help2man which can be found at: https://www.gnu.org/software/help2man/

git-subtree-check.sh
====================

Run this script from the root of the repository to verify that a subtree matches the contents of
the commit it claims to have been updated to.

To use, make sure that you have fetched the upstream repository branch in which the subtree is
maintained:
* for `src/secp256k1`: https://github.com/bitcoin-core/secp256k1.git (branch master)
* for `src/leveldb`: https://github.com/bitcoin-core/leveldb.git (branch bitcoin-fork)
* for `src/univalue`: https://github.com/bitcoin-core/univalue.git (branch master)
* for `src/crypto/ctaes`: https://github.com/bitcoin-core/ctaes.git (branch master)

Usage: `git-subtree-check.sh DIR (COMMIT)`

`COMMIT` may be omitted, in which case `HEAD` is used.

github-merge.py
===============

A small script to automate merging pull-requests securely and sign them with GPG.

For example:

  ./github-merge.py 3077

(in any git repository) will help you merge pull request #3077 for the
bitcoin/bitcoin repository.

What it does:
* Fetch master and the pull request.
* Locally construct a merge commit.
* Show the diff that merge results in.
* Ask you to verify the resulting source tree (so you can do a make
check or whatever).
* Ask you whether to GPG sign the merge commit.
* Ask you whether to push the result upstream.

This means that there are no potential race conditions (where a
pullreq gets updated while you're reviewing it, but before you click
merge), and when using GPG signatures, that even a compromised GitHub
couldn't mess with the sources.

Setup
---------
Configuring the github-merge tool for the bitcoin repository is done in the following way:

    git config githubmerge.repository bitcoin/bitcoin
    git config githubmerge.testcmd "make -j4 check" (adapt to whatever you want to use for testing)
    git config --global user.signingkey mykeyid (if you want to GPG sign)

optimize-pngs.py
================

A script to optimize png files in the bitcoin
repository (requires pngcrush).

security-check.py and test-security-check.py
============================================

Perform basic ELF security checks on a series of executables.

symbol-check.py
===============

A script to check that the (Linux) executables produced by gitian only contain
allowed gcc, glibc and libstdc++ version symbols. This makes sure they are
still compatible with the minimum supported Linux distribution versions.

Example usage after a gitian build:

    find ../gitian-builder/build -type f -executable | xargs python contrib/devtools/symbol-check.py 

If only supported symbols are used the return value will be 0 and the output will be empty.

If there are 'unsupported' symbols, the return value will be 1 a list like this will be printed:

    .../64/test_bitcoin: symbol memcpy from unsupported version GLIBC_2.14
    .../64/test_bitcoin: symbol __fdelt_chk from unsupported version GLIBC_2.15
    .../64/test_bitcoin: symbol std::out_of_range::~out_of_range() from unsupported version GLIBCXX_3.4.15
    .../64/test_bitcoin: symbol _ZNSt8__detail15_List_nod from unsupported version GLIBCXX_3.4.15

update-translations.py
======================

Run this script from the root of the repository to update all translations from transifex.
It will do the following automatically:

- fetch all translations
- post-process them into valid and committable format
- add missing translations to the build system (TODO)

See doc/translation-process.md for more information.

test-build-local.sh
===================

Quick build validation script for testing changes before pushing to CI.
This helps catch build errors early and reduces CI failures.

Usage:
```bash
./contrib/devtools/test-build-local.sh [HOST]
```

The script will:
1. Run autogen.sh to generate configure script
2. Run configure with Qt6 options
3. Test basic compilation
4. Report any errors with helpful diagnostics

For more details, see [BUILD_TESTING.md](BUILD_TESTING.md).

pre-push.sample
===============

Git pre-push hook that reminds developers to test build-related changes
before pushing. This helps prevent CI failures from untested changes.

To install:
```bash
cp contrib/devtools/pre-push.sample .git/hooks/pre-push
chmod +x .git/hooks/pre-push
```

The hook will:
- Detect changes to build-related files (*.cpp, *.h, configure.ac, Makefile.am, etc.)
- Prompt to confirm build has been tested
- Can be bypassed with `git push --no-verify` if needed

BUILD_TESTING.md
================

Comprehensive documentation for testing builds locally before pushing.
Covers:
- Quick build testing procedures
- Full build and test workflows
- Common build issues and solutions
- CI matrix configurations
- Debugging tips for CI failures

Essential reading for anyone modifying:
- Build system files (configure.ac, Makefile.am)
- Dependencies (depends/ directory)
- Qt-related code

validate-depends.sh
===================

Validates the depends system configuration before attempting to build.
This helps catch configuration errors early without needing network access
or a full build.

Usage:
```bash
./contrib/devtools/validate-depends.sh
```

The script checks:
- All required package definitions exist (*.mk files)
- Qt 6 packages are correctly configured
- XCB libraries for Linux Qt builds are present
- Qt patches exist and are referenced correctly
- Configuration files include proper dependencies

Returns:
- Exit code 0 if all checks pass
- Exit code 1 if critical errors are found

This is especially useful before attempting to build depends to ensure
all files are present and correctly configured.
