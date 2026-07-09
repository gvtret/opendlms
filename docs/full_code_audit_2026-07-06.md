# Full Code and Documentation Audit — OpenDLMS

**Date:** 2026-07-06  
**Branch:** full-rework (d3d989b)  
**Uncommitted:** Association LN v2 fix in `cosemlib/ic/db_cosem_ic_association.c` (verified in working tree, not built/committed)  
**Codebase:** ~93K lines C/H + ~12K lines C++ across 338 files  
**Method:** Direct analysis + 4 parallel sub-agents (security/crypto, test coverage/CI, documentation accuracy, code quality)

---

## Executive Summary

The OpenDLMS codebase is a serious, functional DLMS/COSEM protocol stack with substantial coverage of the standard. The core codec, transport, association, and server-side HLS are solid and well-tested. However, there is a **significant gap between what the documentation claims and what the code actually delivers** — several documents are months out of date and make incorrect statements. Additionally, there are concrete security hardening opportunities (timing-safe comparisons, key zeroization) and a major architectural gap (the IC layer exists but isn't wired into any server binary).

### Top 10 Issues (by severity)

| # | Severity | Issue | Where |
|---|----------|-------|-------|
| 1 | 🔴 CRITICAL | **No monotonic IC enforcement** — server never validates invocation counter is strictly increasing; replay attacks possible against ciphered services | csm_security.c:42 |
| 2 | 🔴 CRITICAL | **GMAC tag comparison uses `memcmp()`** — timing side-channel allows byte-by-byte tag extraction | csm_security.c:153 |
| 3 | 🔴 CRITICAL | **LICENSE missing Apache 2.0 attribution** for mbed TLS crypto; Zip.cpp GPLv3 claim is stale (file doesn't exist) | LICENSE |
| 4 | 🟠 HIGH | **HDLC buffer over-read** — `hdlc_decode_info_field` doesn't bounds-check `index += size` against buffer | hdlc.c:392-460 |
| 5 | 🟠 HIGH | **`hdlc_encode` ignores buffer size** — `(void) size;` at line 542, writes without bounds checking | hdlc.c:542 |
| 6 | 🟠 HIGH | **RNG silent fallback** — when `/dev/urandom` unavailable, `csm_hal_get_random_u8` returns `min` (predictable HLS challenges) | reader_hal.c:398 |
| 7 | 🟠 HIGH | **AES keys never zeroized from memory** after GCM operations | csm_security.c |
| 8 | 🟠 HIGH | **MEMORY.md has 15+ incorrect claims** (test count 402→215, IC count 100→33, deleted studio/, non-existent docs, stale presets) | MEMORY.md |
| 9 | 🟠 HIGH | **README.md examples won't compile** — `csm_transport_tcp_init()` doesn't exist, HAL table lists phantom `csm_sys_init()`, HLS claim stale | README.md |
| 10 | 🟠 HIGH | **5 IC modules have zero test coverage** — push, status_mapping, data_protection, limiter, comms | tests/ |
| 11 | 🟠 HIGH | **14 weak test assertions** (`ret >= 0`) in ciphered tests — pass even when crypto is broken | test_integration.cpp |
| 12 | 🟠 HIGH | **IC layer not mounted in any server** — 31 of 33 classes unreachable at runtime | server/, examples/ |
| 13 | 🟠 HIGH | **No rate limiting on failed HLS attempts** — unlimited retries allowed | csm_channel.c:307-317 |
| 14 | 🟡 MEDIUM | **Kuznyechik GF(2^8) multiply is not constant-time** — cache-timing leakage | kuznyechik.c:49-61 |
| 15 | 🟡 MEDIUM | **Doxyfile INPUT missing** ic/, crypto/, hdlc/, util/, server/, client/ | Doxyfile |
| 16 | 🟡 MEDIUM | **No `-Werror` in CI** — warnings invisible | CMakePresets.json |
| 17 | 🟡 MEDIUM | **macOS CI missing CLIENT flag** — client/reader not built on macOS | ci.yml |
| 18 | 🟡 MEDIUM | **2 stub test files** (test_cosem_catalog, test_push_loopback) inflate test count | tests/ |

---

## 1. Documentation Audit

### 1.1 LICENSE (CRITICAL)

**Current state:** Top-level `LICENSE` is a plain MIT license with only "Copyright (c) 2024 Anthony Rabine". There is NO mention of:
- Apache 2.0 for mbed TLS-derived crypto code (which has its own `cosemlib/crypto/LICENSE` file)
- GPLv3 for `studio/Zip.cpp` (this file **does not exist** — was removed when Qt Studio was replaced by Electron)
- Any third-party component attribution at the top level

**What MEMORY.md claims (line 100-103):**
> - Core (cosemlib/): MIT
> - Crypto: Apache 2.0 (mbed TLS derived, compatible)
> - studio/Zip.cpp: GPLv3 (isolated)
> - Total: 216+ files audited, LICENSE.txt includes third-party components

**Reality:** There is no `LICENSE.txt`, no third-party attribution in the main LICENSE, and the Zip.cpp claim is stale.

**Fix needed:** Add a THIRD-PARTY-LICENSES section or NOTICE file that properly attributes mbed TLS (Apache 2.0) and any other dependencies.

### 1.2 README.md (HIGH — multiple stale claims)

| Line | Claim | Reality | Fix |
|------|-------|---------|-----|
| 9 | "no dynamic allocation" | `csm_server_create()` and `csm_client_create()` both use `malloc()` | Change to "core protocol logic avoids dynamic allocation" |
| 16 | "170+ tests" | 215 tests / 1950 assertions | Update to "215+ tests" |
| 56-57 | "Configurator/HLS associations are fail-closed until the full HLS pass 3/4" | HLS is COMPLETE since commit 5f66974 | Remove this caveat |
| HAL table (167-175) | Lists 6 functions | Actual HAL has 13+ functions (csm_sys_init, get_system_title, get_key, get_lls_password, get_random_u8, sha1, sha256, md5, gcm_init, gcm_update, gcm_finish, decode_selective_access, get_mechanism_id) | Expand the table |

### 1.3 MEMORY.md (HIGH — extensively stale)

**File:** `/mnt/e/work/opendlms/opendlms/MEMORY.md` — this is the project memory stored in-repo.

| Line | Stale Claim | Correct Value |
|------|-------------|---------------|
| 6 | Version 1.1.0 (2026-06-19) | Version hasn't bumped but codebase has evolved significantly |
| 7 | 402 test cases, 3256 assertions | 215 tests / 1950 assertions |
| 13 | "100 IC classes (98 IEC + 2 SPODUS)" | 33 IC classes registered |
| 15 | `server/lib/` listed with associations, push, event notify, catalog | associations removed; lib/ has push, event_notify, catalog_legacy |
| 21 | "server/lib/ # Server wiring: associations..." | Associations handler removed from lib/ |
| 25 | `examples/ # metersimulator, tcpcli, dlmscli, embeddedmeter, cosemreader` | tcpcli, dlmscli, cosemreader don't exist |
| 26 | `studio/ # Shaddam GUI (Qt6, Lua scripting)` | studio/ directory does NOT exist (replaced by electron-studio/) |
| 27 | "60 test files" | 18 test files |
| 29 | `docs/ # ARCHITECTURE.md, RELEASE.md, spodes_catalog_reference.md` | None of these files exist |
| 48 | "Association LN v3" | Association LN v2 (Blue Book §5.3.7) |
| 60 | "csm_axdr_decode_tags: Always returns FALSE" | Actually works correctly (returns TRUE on success) |
| 65 | "98 class IDs from IEC 62056-6-2 Table 4" | 33 classes registered |
| 68 | "Table Manager methods: add_update_entries/remove_entries/..." | Correct for СПОДУС 8200, but the claim about 100 classes is wrong |
| 87-89 | Build: `cmake --preset debug-mingw && cmake --build build` | Build dir is `build-mingw`, not `build` |
| 89 | `cmake --preset release-full` | This preset does not exist |
| 91 | "CI: Linux GCC/Clang/ASan + Windows MinGW, clang-format lint" | No clang-format lint in CI |
| 92 | "PGO: preset `pgo-msvc` (GL + LTCG)" | No such preset exists |
| 93 | "10 CMake presets" | 4 presets: debug, release, production, debug-mingw |
| 102 | "Crypto: Apache 2.0 (mbed TLS derived, compatible)" | Correct but not in top-level LICENSE |
| 103 | "studio/Zip.cpp: GPLv3 (isolated)" | File does not exist |
| 104 | "LICENSE.txt includes third-party components" | LICENSE.txt does not exist |

### 1.4 docs/production_readiness_audit.md (MEDIUM — partially stale)

The 2026-07-05 section is accurate but the original 2026-06-27 section contains items that have since been resolved:
- "Configurator/HLS associations are fail-closed" → RESOLVED
- "HLS pass 3/4 not implemented" → RESOLVED
- "AARQ calling-AP-title missing" → RESOLVED
- "Several IC handlers remain partial" → Many completed since

The document should be updated to mark resolved items clearly.

### 1.5 docs/ directory

The `docs/` directory is a **GitHub Pages web application** (app.js, gateway.ts, index.html, components/) with one embedded source file (libmetersim.c) and the audit document. It does NOT contain project documentation (no ARCHITECTURE.md, no API docs, no release notes).

### 1.6 Doxyfile (MEDIUM)

- INPUT = `cosemlib/src` only — misses `cosemlib/ic/`, `cosemlib/crypto/`, `cosemlib/hdlc/`, `cosemlib/util/`, `server/`, `client/`, `common/`
- PROJECT_NUMBER = "1.1.0" — no auto-increment
- No EXCLUDE for test files, build directories

### 1.7 Identity/template files (LOW)

IDENTITY.md, SOUL.md, USER.md, TOOLS.md, HEARTBEAT.md are all empty templates from an "OpenClaw" scaffolding. They add noise to the repo root with zero value.

---

## 2. Security Audit

### 2.1 Timing-Safe Tag Comparison (CRITICAL)

**File:** `cosemlib/src/csm_security.c:153`
```c
if (memcmp(tag, tag_read, 12U) != 0)
```

GMAC authentication tags are compared with `memcmp()`, which short-circuits on the first mismatched byte. This is a classic timing side-channel: an attacker who can measure response time differences between "wrong first byte" and "wrong last byte" can brute-force the tag byte-by-byte in ~12×256 = 3072 attempts instead of 256^12.

**Fix:** Use a constant-time comparison:
```c
static int ct_memcmp(const uint8_t *a, const uint8_t *b, uint32_t len) {
    uint8_t diff = 0;
    for (uint32_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff; /* 0 = match */
}
```

**Scope:** This affects ALL GMAC/HLS verification paths (server-side pass 3, pass 4 verify, and any ciphered-response verification).

### 2.2 Key Zeroization (CRITICAL)

**Files:** `cosemlib/src/csm_security.c`, `cosemlib/src/csm_channel.c`

AES keys retrieved via `csm_sys_get_key()` are used for GCM init/update/finish but **never zeroized** from the caller's perspective. The key material lives in the HAL-provided buffer, but the library never calls `memset(key, 0, 16)` after use. On platforms without secure memory, keys persist in the heap/stack until overwritten.

**Recommended:** Add a helper or document that HAL implementations must provide secure zeroization. At minimum, zeroize any local copies of key material.

### 2.3 Streebog L-Transform Broken (MEDIUM — documented)

`tests/test_gost.cpp:163,199` — HLS9 test vectors are `[.todo]` (skipped) because the GF(2^64) L-transformation produces incorrect results. This means:
- GOST Suite 9 (Streebog-256 based) is not functional
- Suite 9 `csm_sec_suite_is_supported(9)` returns 0 (correctly fail-closed)
- Suite 8 (Kuznyechik) works correctly

This is a known limitation, properly documented, and correctly fails closed.

### 2.4 No Network-Level Buffer Overflow Paths

The BER/AXDR decoder validates lengths against array bounds before reading. The `csm_array` abstraction enforces bounds on all read/write operations. The HDLC decoder validates frame length before parsing. No obvious network-reachable overflow paths were found.

### 2.5 HLS Handshake (VERIFIED CORRECT)

Client-side HLS pass 3/4 GMAC is implemented and verified end-to-end:
- `csm_client_connect()` → AARQ (with calling-AP-title) → AARE → pass 3 (f(StoC)) → pass 4 (verify f(CtoS))
- Server-side: `csm_channel_hls_action_ctx()` handles the reply_to_HLS ACTION directly
- Live-tested against metersimulator with both plain HLS and glo-ciphering

---

## 3. Code Quality Audit

### 3.1 malloc in "No Dynamic Allocation" Library (MEDIUM)

**Claim:** "Pure C99, no dynamic allocation, zero-copy" (README line 9)

**Reality:** Two `malloc()` calls in the core library:
- `csm_server_create()` at `csm_server.c:141` — allocates a `csm_server` on the heap
- `csm_client_create()` at `csm_server.c:451` — allocates a `csm_client` on the heap

Both have corresponding `free()` in destroy functions. The allocation is for the **top-level context** only — all internal buffers are static/stack. The `csm_client_create()` and `csm_server_create()` are convenience wrappers; the `_init()` variants work with caller-provided memory.

**Assessment:** The "no dynamic allocation" claim is approximately true for the core protocol logic. The heap allocation is limited to opaque context objects, not data buffers. This is acceptable but the documentation should be more precise.

### 3.2 Global Mutable State (MEDIUM)

Two global statics break the thread-safety model for legacy callers:
- `csm_channel.c:533`: `static csm_channel_ctx *g_default_ctx = NULL` — used by backward-compatible `_init()` / `_execute()` APIs
- `csm_services.c:17`: `static csm_db_access_handler database = NULL` — global service handler

These are documented as "single-instance only" backward-compatible APIs. The context-based APIs (`_ctx` variants) are thread-safe. This is a known design decision, not a bug, but should be documented clearly.

### 3.3 Unchecked Write Return Values (LOW)

In `csm_channel.c`, approximately 30% of `csm_array_write_u8()` calls don't check the return value (10 unchecked vs 7 checked in one file). Most are in error paths or are followed by other checked writes, but there are a few cases in the main HLS action handler where an unchecked write could silently truncate data.

### 3.4 No `sprintf` / `gets` / Dangerous Functions

Zero instances of `sprintf()`, `gets()`, `strcpy()`, or `strcat()` found in library code. The codebase uses bounded array operations consistently.

### 3.5 Dead Code

- `csm_axdr_decode_tags()` — MEMORY.md claimed it "always returns FALSE" but it actually works correctly. MEMORY.md was stale.
- `server/lib/opendlms_meter.c` — **deleted** in commit 44c81a1 (correct cleanup).
- The legacy backward-compatible global API functions (`csm_channel_init`, `csm_services_init`, etc.) are still present and functional, but no production code uses them.

---

## 4. Test Coverage Audit

### 4.1 Overall Numbers

| Metric | Count |
|--------|-------|
| Test files | 18 |
| Test cases | 215 |
| Assertions | 1950 |
| Test-only code lines | 7322 |
| Live smoke tests | 3 (reader_lab_live_smoke, cosemtest, reader_hal_smoke) |

### 4.2 Strong Coverage Areas

- **BER/AXDR codec**: Well-tested with round-trip, boundary, error, and malformed-input tests
- **AES-GCM/GMAC**: NIST vectors, HLS5 GreenBook vectors, null/short-input rejection, round-trip
- **Association**: AARQ/AARE encode/decode, dedicated key, multiple auth levels, HLS pending rejection
- **Block transfer**: SET and GET receive, encode, overflow guards
- **Transport**: TCP client/server, split WPDU reassembly, null/empty rejection, timeout
- **HLS**: Round-trip GMAC, pass 3/4 server-side, client loopback, live end-to-end
- **IC handlers**: 25+ integration tests covering most IC classes

### 4.3 Coverage Gaps (by severity)

**HIGH — Security paths with weak assertions:**
- `Integration_CipheredGetClockTime` (test_integration.cpp:1979): `REQUIRE(ret >= 0)` — doesn't check that the response is actually correctly decrypted
- `Integration_ClientCipherGetClockTime` (test_integration.cpp:2013): same weak assertion

**MEDIUM — Missing dedicated test coverage:**
- `csm_security.c`: No dedicated test for decryption with wrong key (auth failure path), no test for maximum-size GCM operations
- `csm_block_transfer.c`: No test for block transfer with max-size PDU boundary conditions
- `csm_transport_tcp.c`: No test for connection timeout behavior, no test for partial writes
- Clock methods (adjust_to_quarter, etc.): Only tested via IC integration, no dedicated clock algorithm tests

**MEDIUM — Stub tests:**
- `tests/test_push_loopback.cpp` (31 lines): Appears to be a minimal stub
- `tests/test_cosem_catalog.cpp` (20 lines): Appears to be a minimal stub

**LOW — Live coverage gaps:**
- Live smoke covers: public GET, reader LLS GET, config HLS (plain + glo), SET, ACTION, disconnect
- NOT covered live: block transfer, event notification, push, GOST crypto, HDLC framing, serial transport

---

## 5. CI/Build Audit

### 5.1 CI Pipeline (`.github/workflows/ci.yml`)

| Job | Platform | What it covers |
|-----|----------|----------------|
| build-linux | ubuntu-latest | GCC, Release, full gate + install + cmake consumer + pkg-config consumer |
| build-windows | windows-latest | MSVC, Release, full gate |
| build-macos | macos-latest | Release, tests only (no client/sim) |
| build-studio | ubuntu-latest | Electron native addon + packaging |
| sanitizers | ubuntu-latest | Clang ASan/UBSan, Debug, full gate with leak detection |

**Missing from CI:**
- Clang-format lint (mentioned in MEMORY.md but not in CI)
- MinGW cross-compile gate
- Valgrind / DR Memory
- Code coverage reporting (lcov/gcov)
- Static analysis (cppcheck, clang-tidy)
- PGO build validation
- No matrix builds (single compiler per platform)

### 5.2 CMake Presets

Only 4 configure presets exist: `default`, `debug`, `production`, `debug-mingw`. MEMORY.md claims 10 presets and a `release-full` / `pgo-msvc` preset — these don't exist.

### 5.3 No `-Werror`

No build configuration uses `-Werror`. Warnings during compilation are invisible in CI. This is a significant gap for production quality.

---

## 6. Interface Classes (IC) Audit

### 6.1 Coverage

33 IC classes registered (1,3,4,5,6,7,8,9,10,11,15,17,18,19,20,21,22,23,26,30,31,40,41,61,62,63,64,65,67,68,70,71,8200). This is a solid foundation covering the most commonly needed DLMS/COSEM objects.

### 6.2 Architectural Gap: IC Layer Not Mounted

`db_ic_register_all_builtins()` is called ONLY by `tests/test_integration.cpp`. No server binary (including metersimulator) mounts the IC layer. The metersimulator uses its own `gDataBaseList` with just Clock + Association LN via direct handlers.

This means 31 of 33 IC classes are **unreachable at runtime** — they exist only in tests.

### 6.3 Association LN Fix (IN PROGRESS, UNCOMMITTED)

The Association LN (class 15) had incorrect method numbering and version. A fix has been applied to the working tree:
- Version: 0 → 2 (matches the 11-attribute v2 set)
- Method dispatch corrected to match IEC 62056-6-2 §5.3.7
- method_count: 5 → 6 (added change_HLS_secret)

**Not yet built or tested** — was interrupted before verification.

### 6.4 Key Missing Classes

For TCP-based smart meters, the most notable absences are:
- IPv4 Setup (class 42) — commonly paired with TCP-UDP Setup (41)
- Association SN (class 12) — the LN-only design is intentional but limits applicability
- M-Bus classes (72-77) — only relevant for concentrators
- Prepayment classes (111-115) — only relevant for prepaid meters

---

## 7. Repository Hygiene

### 7.1 Stale Build Directories

12 build directories consuming ~270MB:
- `build-Linux/` (27MB), `build-asan/` (83MB), `build-san/` (82MB), `build-mingw/` (35MB), `build-production/` (11MB)
- Plus 7 stale audit build directories

These should be cleaned. They're gitignored but still waste disk space and can confuse tools.

### 7.2 OpenClaw Templates

IDENTITY.md, SOUL.md, USER.md, TOOLS.md, HEARTBEAT.md are empty scaffolding templates from an "OpenClaw" project management system. They add noise to the repo root.

### 7.3 Untracked Files

The `.mimocode/` directory is untracked (expected — it's session data).

---

## 8. Prioritized Fix Recommendations

### Phase 1 — Security (do first)
1. **Replace `memcmp()` with constant-time comparison** for GMAC tags in `csm_security.c`
2. **Add key zeroization** after GCM operations or document HAL responsibility
3. **Add `-Werror` to CI** (or at least `-Werror=return-type -Werror=implicit-function-declaration`)

### Phase 2 — Documentation (high impact, low risk)
4. **Rewrite MEMORY.md** — at least 15 claims are factually wrong
5. **Update README.md** — fix HLS claim, HAL table, test count
6. **Update LICENSE** — add third-party attribution section for mbed TLS
7. **Delete empty template files** (IDENTITY.md, SOUL.md, USER.md, TOOLS.md, HEARTBEAT.md)
8. **Update Doxyfile INPUT** to cover all source directories

### Phase 3 — Code Quality (medium priority)
9. **Build and commit the Association LN v2 fix** (currently unverified in working tree)
10. **Add `-Wall -Wextra`** to all build configurations
11. **Verify and strengthen weak ciphered test assertions** (`ret >= 0` → content checks)
12. **Clean up stale build directories**

### Phase 4 — Architecture (longer-term)
13. **Decide: mount IC layer in server, or document as reference-only**
14. **Add missing HAL functions to README** documentation
15. **Add CI jobs**: clang-tidy, code coverage, MinGW gate
16. **Update docs/production_readiness_audit.md** — mark resolved items, add new findings

---

## Addendum: Sub-Agent Deep Scan Findings

### A. Security — Additional Findings (from crypto/security audit)

**SEC-1 (CRITICAL): No monotonic invocation counter enforcement**
- `csm_security.c:42` — IC is read from packet and used in IV, but never validated against a stored monotonic counter. `csm_sec_context.client_ic` / `server_ic` exist but are unused for validation.
- **Impact:** Replay attacks possible against any ciphered service. An attacker who captures a valid encrypted APDU can replay it indefinitely.
- **Standard reference:** Green Book §9.2.7.4, Yellow Book §8.5.2 (SYMSEC_0_FraCount_1: "Message replay protection")
- **Fix:** After successful decryption, verify `ic > stored_client_ic`. Reject + disconnect on failure.

**TCP-2 (HIGH): Buffer over-read in HDLC info field parsing**
- `cosemlib/hdlc/hdlc.c:392-460` — `hdlc_decode_info_field` advances `index` by tag `size` without bounds-checking against `info_field_size`. Unknown tags (which don't increment `number_of_tags`) can cause the loop to read past buffer end.
- **Fix:** `if (index + size > info_field_size) { ret = HDLC_ERR; break; }` before advancing.

**TCP-3 (HIGH): `hdlc_encode` ignores buffer size parameter**
- `cosemlib/hdlc/hdlc.c:542` — `(void) size;` discards the buffer size. Writes to `buf` without bounds checking.
- **Fix:** Track `index` and check `if (index >= size) return HDLC_ERR;` before each write.

**RNG-1 (HIGH): Silent fallback to deterministic value on RNG failure**
- `client/lib/reader_hal.c:373-399` — If `reader_hal_random_byte()` fails all 32 retries, `csm_hal_get_random_u8()` returns `min` (line 398). This makes HLS CtoS challenges completely predictable.
- Same pattern in `examples/metersimulator/src/cosem_server_hal.c:288-309`.
- **Fix:** Return an error code; callers must abort the handshake.

**KUZ-1 (MEDIUM): Non-constant-time GF(2^8) multiply**
- `cosemlib/crypto/kuznyechik.c:49-61` — Variable-time loop and brute-force inverse could leak key material through cache-timing. Low risk for isolated DLMS meter networks.

**SEC-4 (MEDIUM): No rate limiting on failed HLS attempts**
- `csm_channel.c:307-317` — Failed pass 3 just logs an error; association stays pending with unlimited retries. Per Green Book §8.5, a lockout mechanism is recommended.

### B. Documentation — Additional Findings (from doc accuracy audit)

**README.md compilation-breaking issues:**
1. Line 78: `csm_transport_tcp_init()` does **not exist** — actual API is `csm_transport_tcp_server_init(&transport, 4056, CSM_FRAMING_WRAPPER)` (3 args, no host string for server)
2. HAL table line 170: `csm_sys_init()` listed but **has no declaration in any header** — it's a phantom function
3. HAL table missing 7 functions: `csm_sys_set_system_title`, `csm_hal_get_lls_password`, `csm_hal_sha1`, `csm_sys_get_mechanism_id`, `csm_sys_gcm_init/update/finish`
4. Directory structure missing: `cosemlib/model/`, `cosemlib/profile/`, `cosemlib/util/`, `common/`, `cmake/`

**HANDOFF.md** — completely stale: claims "Total tests: 414, Passing: 411" (actual: 215/1950). Should be deleted or regenerated.

**cosemlib/crypto/LICENSE** references `apache-2.0.txt` which **does not exist** in the repo — license compliance gap.

### C. Test Coverage — Additional Findings (from test/CI audit)

**5 IC modules with ZERO test coverage** (not even integration tests):
1. `db_cosem_ic_push.c` — Push Setup
2. `db_cosem_ic_status_mapping.c` — Status Mapping
3. `db_cosem_ic_data_protection.c` — Data Protection
4. `db_cosem_ic_limiter.c` — Limiter
5. `db_cosem_ic_comms.c` — IEC Local/HDLC/TCP-UDP Setup (3 classes in one file)

**14 weak assertions** in `test_integration.cpp` — all `REQUIRE(ret >= 0)` in ciphered test cases (lines 1979, 2007, 2038, 2068, 2097, 2155, 2187, 2214, 2233, 2446, 2465, 2475, 2668, 2856). These pass even when crypto is broken, since `csm_channel_execute` returns ≥ 0 for exception responses too.

**Stub tests inflating count:**
- `tests/test_cosem_catalog.cpp:19` — `REQUIRE(1)` (always passes, zero coverage)
- `tests/test_push_loopback.cpp` — only tests OS socket creation, not DLMS push

**macOS CI incomplete:** `ci.yml` line 159 — macOS build only enables `OPEN_DLMS_BUILD_SERVER=ON`, missing `CLIENT` and `READER_LAB`. Client library and reader lab not built/tested on macOS.

---

*This audit covers source code, documentation, tests, CI, security, and standards compliance. 4 parallel sub-agent deep scans were executed for code quality (abandoned — incomplete), test coverage + CI, documentation accuracy, and security/crypto correctness. All completed findings are incorporated above.*
