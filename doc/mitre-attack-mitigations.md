# MITRE ATT&CK Cyber Table Top Analysis - Dogecoin Core

## Overview

This document presents the findings of a MITRE ATT&CK framework-based cyber
table top exercise performed on the Dogecoin Core codebase. The analysis
identifies attack vectors relevant to cryptocurrency node software and the
mitigations that have been implemented. The second iteration of this CTT
incorporates findings from the Least Authority security audit of the Dogecoin
Node Upgrade (April 2023).

## Threat Model

Dogecoin Core operates as a peer-to-peer network node that:
- Listens for and relays transactions and blocks over the P2P network
- Exposes a JSON-RPC interface for wallet and node management
- Stores private keys and wallet data on disk
- Runs as a long-lived daemon process

## MITRE ATT&CK Techniques Analyzed

### T1548 - Abuse Elevation Control Mechanism (Privilege Escalation)

**Risk:** Running the daemon as root allows an attacker who compromises the
process to gain full system access.

**Mitigation Implemented:** Added root user detection at startup
(`src/init.cpp`). When the process detects it is running as UID 0, it emits
a warning to both the log and UI, advising the operator to use a dedicated
unprivileged user account.

### T1003 - OS Credential Dumping (Credential Access)

**Risk:** Sensitive cryptographic material (private keys, wallet master key)
held in process memory may be written to swap space or core dump files by the
OS, where it can be recovered by an attacker with disk access.

**Mitigations Implemented:**
1. **Memory locking** (`src/init.cpp`): Added `mlockall(MCL_CURRENT | MCL_FUTURE)`
   at startup to lock all current and future process memory pages into RAM,
   preventing them from being swapped to disk. Failure is non-fatal but logged.
   This complements the existing per-allocation `mlock()` used by
   `LockedPoolManager` for wallet key storage.
2. **Core dump disabling** (`src/init.cpp`): Added `setrlimit(RLIMIT_CORE, 0)`
   and `prctl(PR_SET_DUMPABLE, 0)` at startup to prevent core dump files from
   being generated. This addresses **Least Authority Audit Issue B** (sensitive
   data in old core dump files). The existing `MADV_DONTDUMP` in
   `src/support/lockedpool.cpp` only protects locked pool allocations; this new
   mitigation protects the entire process address space.

### T1110 - Brute Force (Credential Access)

**Risk:** The JSON-RPC interface accepts password authentication. An attacker
with network access to the RPC port can attempt brute-force credential attacks.
The previous mitigation was a fixed 250ms delay per failed attempt.

**Mitigations Implemented:**
1. **Per-IP rate limiting** (`src/httprpc.cpp`): Tracks failed authentication
   attempts per source IP address. After `RPC_AUTH_MAX_FAILURES` (5) consecutive
   failures, the IP is locked out for `RPC_AUTH_LOCKOUT_SECONDS` (300 seconds).
2. **Escalating delay**: Failed attempts incur an increasing sleep proportional
   to the number of failures (250ms × failure_count, capped at 750ms).
3. **Success reset**: A successful authentication resets the failure counter for
   that IP.

**Risk:** Wallet encryption uses key derivation to protect private keys. Low
iteration counts allow faster brute-force attacks on the wallet passphrase.

**Mitigation Implemented:** Increased the minimum KDF iteration count from
25,000 to 100,000 (`src/wallet/crypter.h`, `src/wallet/wallet.cpp`). The
calibration benchmark was also scaled up (from 2.5M to 10M target operations)
to ensure new wallets use appropriately high iteration counts for modern
hardware. This applies to both new wallet encryption and passphrase changes.

### T1499 - Endpoint Denial of Service (Impact)

**Risk:** A malicious peer can flood the node with protocol messages, consuming
CPU and memory resources and degrading service for legitimate peers.

**Mitigation Implemented:** Added per-peer message rate limiting
(`src/net_processing.cpp`, `src/net_processing.h`). Each non-whitelisted peer
is tracked with a sliding time window (`PEER_MSG_RATE_WINDOW` = 10 seconds).
If a peer sends more than `MAX_PEER_MSG_RATE` (200) messages within a window,
the peer receives a misbehavior score increase (`PEER_MSG_RATE_DOS_SCORE` = 10).
Accumulation of misbehavior scores leads to automatic banning via the existing
DoS framework.

### T1040 - Network Sniffing (Credential Access)

**Risk:** The JSON-RPC interface uses HTTP Basic authentication over an
unencrypted connection. Credentials transmitted in Base64 can be intercepted
by a network-level attacker.

**Mitigation Implemented:** Added startup warnings (`src/init.cpp`) when the
RPC server is active:
- A general warning that RPC does not use TLS encryption.
- An additional warning when `-rpcallowip` is configured, indicating that RPC
  is accessible from non-localhost addresses.

These warnings inform operators to use SSH tunnels, VPNs, or other secure
transports when accessing the RPC interface remotely.

### T1485 - Data Destruction (Impact)

**Risk:** Wallet data loss from disk corruption, accidental deletion, or
ransomware can result in permanent loss of funds if no backup exists.

**Mitigation Implemented:** Added automatic wallet backup on startup
(`src/wallet/wallet.cpp`). Each time the node starts with wallet enabled, a
copy of the wallet file is saved to the configured backup directory as
`wallet-startup-backup.dat`. This addresses **Least Authority Audit Issue D**
(wallet recovery not implemented). Can be disabled with `-disableautobackup`.

### T1552 - Unsecured Credentials (Credential Access)

**Risk:** Legacy (non-HD) wallets use non-deterministic key generation. Each
new key is independent and requires a separate backup. If a backup is taken
after key generation but before use, funds sent to new keys may be lost.

**Mitigation Implemented:** Added startup warning when a non-HD wallet is
detected (`src/wallet/wallet.cpp`). This addresses **Least Authority Audit
Issue C** (key derivation path unchanged). Users are advised to create a new
HD wallet for improved key management and recovery capabilities.

## Least Authority Audit Findings Addressed

The following table maps Least Authority audit findings to implemented mitigations:

| Audit Finding | Description | Mitigation | Location |
|--------------|-------------|------------|----------|
| **Issue B** | Sensitive data in core dump files | Disable core dumps with RLIMIT_CORE=0 and PR_SET_DUMPABLE=0 | `src/init.cpp` |
| **Issue C** | Key derivation path unchanged (legacy wallets) | Startup warning for non-HD wallets | `src/wallet/wallet.cpp` |
| **Issue D** | Wallet recovery not implemented | Automatic wallet backup on startup | `src/wallet/wallet.cpp` |
| **Suggestion 3** | Consider replace-by-fee | Enable wallet RBF by default | `src/wallet/wallet.h` |

### Findings Not Requiring Code Changes

| Audit Finding | Description | Assessment |
|--------------|-------------|------------|
| **Issue A** | Dust rule changes may execute stuck transactions | Inherent to consensus rule evolution; documented risk |
| **Issue E** | Increased default transaction fee | Intentional design decision for Dogecoin economics |
| **Issue F** | Discard threshold replaces dust limit | Proper separation of relay policy from consensus |
| **Issue G** | Increased fee requirements from miners | Network-level policy; configurable per-node |
| **Issue H** | Dust consensus derived from recommended fee | Dogecoin-specific fee structure; intentional |
| **Suggestion 1** | Lower discard threshold | Current threshold (0.01 DOGE) appropriate for Dogecoin |
| **Suggestion 2** | Lower soft dust limit | Current limits balance spam prevention with usability |

## Pre-Existing Security Controls

The following security controls were already present and validated during the
analysis:

| Control | Location | MITRE Technique |
|---------|----------|-----------------|
| Peer banning system | `src/net.cpp` | T1499 DoS |
| DoS misbehavior scoring | `src/net_processing.cpp` | T1499 DoS |
| Address rate limiting | `src/net_processing.cpp` | T1499 DoS |
| Connection limits | `src/net.h` | T1499 DoS |
| Message size limits | `src/net.h` | T1499 DoS |
| Umask 077 on Unix | `src/init.cpp` | T1222 File Permissions |
| Cookie-based RPC auth | `src/httprpc.cpp` | T1110 Brute Force |
| AES-256-CBC wallet encryption | `src/wallet/crypter.cpp` | T1005 Data from Local System |
| Secure memory allocator | `src/support/lockedpool.cpp` | T1003 Credential Dumping |
| MADV_DONTDUMP for locked pool | `src/support/lockedpool.cpp` | T1003 Credential Dumping |
| PIE/NX/RELRO binary hardening | `configure.ac` | T1055 Process Injection |
| Stack protector flags | `configure.ac` | T1203 Exploitation |
| FORTIFY_SOURCE=2 | `configure.ac` | T1203 Exploitation |
| ChaCha20 CSPRNG | `src/random.cpp` | T1496 Resource Hijacking |
| HD wallet (BIP32/BIP44) | `src/wallet/wallet.cpp` | T1552 Unsecured Credentials |
| BIP125 RBF support | `src/policy/rbf.cpp` | T1565 Data Manipulation |
| Wallet backup RPC | `src/wallet/rpcwallet.cpp` | T1485 Data Destruction |
| Wallet dump/import | `src/wallet/rpcdump.cpp` | T1485 Data Destruction |

## Recommendations for Future Work

1. **TLS for RPC**: Implement native TLS support for the JSON-RPC interface to
   prevent credential interception (T1040).
2. **AppArmor/SELinux profiles**: Provide mandatory access control profiles to
   limit process capabilities (T1548).
3. **Hardware wallet integration**: Support external signing devices to keep
   private keys off the host system (T1005).
4. **Audit logging**: Add structured logging of security-relevant RPC operations
   for incident detection (T1078).
5. **Encrypted wallet backup**: Encrypt automatic backups to prevent credential
   extraction from backup files (T1552).
6. **Automatic HD migration**: Provide a migration path from legacy wallets to
   HD wallets to address Least Authority Issue C more completely.
