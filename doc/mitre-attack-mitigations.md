# MITRE ATT&CK Cyber Table Top Analysis - Dogecoin Core

## Overview

This document presents the findings of a MITRE ATT&CK framework-based cyber
table top exercise performed on the Dogecoin Core codebase. The analysis
identifies attack vectors relevant to cryptocurrency node software and the
mitigations that have been implemented.

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
held in process memory may be written to swap space by the OS, where it can
be recovered by an attacker with disk access.

**Mitigation Implemented:** Added `mlockall(MCL_CURRENT | MCL_FUTURE)` at
startup (`src/init.cpp`). This requests the OS to lock all current and future
process memory pages into RAM, preventing them from being swapped to disk.
Failure is non-fatal but logged as a warning. This complements the existing
per-allocation `mlock()` used by `LockedPoolManager` for wallet key storage.

### T1110 - Brute Force (Credential Access)

**Risk:** The JSON-RPC interface accepts password authentication. An attacker
with network access to the RPC port can attempt brute-force credential attacks.
The previous mitigation was a fixed 250ms delay per failed attempt.

**Mitigations Implemented:**
1. **Per-IP rate limiting** (`src/httprpc.cpp`): Tracks failed authentication
   attempts per source IP address. After `RPC_AUTH_MAX_FAILURES` (5) consecutive
   failures, the IP is locked out for `RPC_AUTH_LOCKOUT_SECONDS` (300 seconds).
2. **Escalating delay**: Failed attempts incur an increasing sleep proportional
   to the number of failures (250ms × failure_count, capped at 1250ms).
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
| PIE/NX/RELRO binary hardening | `configure.ac` | T1055 Process Injection |
| Stack protector flags | `configure.ac` | T1203 Exploitation |
| FORTIFY_SOURCE=2 | `configure.ac` | T1203 Exploitation |
| ChaCha20 CSPRNG | `src/random.cpp` | T1496 Resource Hijacking |

## Recommendations for Future Work

1. **TLS for RPC**: Implement native TLS support for the JSON-RPC interface to
   prevent credential interception (T1040).
2. **AppArmor/SELinux profiles**: Provide mandatory access control profiles to
   limit process capabilities (T1548).
3. **Hardware wallet integration**: Support external signing devices to keep
   private keys off the host system (T1005).
4. **Audit logging**: Add structured logging of security-relevant RPC operations
   for incident detection (T1078).
