# OpenDLMS Development Plan

## Current Sprint: Phase 1 — GOST Crypto + Cross-platform

### Status: Complete (with known Streebog L issue)

### Completed
- [x] AARQ encoder fix — plaintext-only per IEC 62056-5-3
- [x] Kuznyechik block cipher (RFC 7801) — block, CMAC, CTR
- [x] Streebog-256 hash (GOST R 34.11-2012) — structure correct, empty hash passing
- [x] Cross-platform POSIX fixes (tcp_server, tcp_push headers)
- [x] CMakePresets.json for Linux and MinGW builds
- [x] Centralized cmake/opendlms_sources.cmake
- [x] Secret replacement in session configs

### Blocked
- [ ] Streebog HLS9 test vectors — GF(2^64) L transformation constants need audit

### Test Results
| Suite | Total | Pass | Fail | Skip |
|-------|-------|------|------|------|
| Kuznyechik | 4 | 4 | 0 | 0 |
| Streebog | 3 | 1 | 0 | 2 (TODO) |
| Existing | 407 | 406 | 0 | 1 |
| **Total** | **414** | **411** | **0** | **3** |

### Next Steps
1. Fix Streebog GF(2^64) L transformation (compute correct iter_c constants)
2. Add real tests for test_cosem_catalog.cpp
3. Add real tests for test_push_loopback.cpp
4. Implement Kuznyechik decryption for full bidirectional support
