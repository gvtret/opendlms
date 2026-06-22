# HANDOFF

## Test Status
- **Total tests:** 414
- **Passing:** 411
- **Skipped/TODO:** 3 (Streebog HLS9 vectors pending GF(2^64) L fix)

## GOST Crypto Status
- **Kuznyechik (GOST R 34.12-2015):** ✅ Implemented and passing
  - Block cipher: RFC 7801 test vector verified
  - CMAC mode: implemented
  - CTR mode: implemented
- **Streebog-256 (GOST R 34.11-2012):** ⚠️ Partial
  - Empty hash: ✅ Passing
  - HLS9 C vector: ❌ TODO (GF(2^64) L transformation issue)
  - HLS9 S vector: ❌ TODO (GF(2^64) L transformation issue)
  - Algorithm structure is correct; L layer constants need audit

## Changes in this session
1. AARQ encoder fix — added comment documenting plaintext-only per IEC 62056-5-3
2. GOST crypto primitives — new files in `cosemlib/crypto/` + test file
3. Cross-platform fixes — POSIX includes, `INVALID_SOCKET`, `SD_BOTH` defines
4. CMakePresets.json — debug and debug-mingw presets
5. cmake/opendlms_sources.cmake — centralized source lists
6. Session config secret replacement — remaining passwords replaced with placeholders

## Known issues
- Streebog GF(2^64) L transformation produces incorrect deep test vectors
- `test_cosem_catalog.cpp` and `test_push_loopback.cpp` are stub files (need real tests)
