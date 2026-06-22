#!/bin/bash
set -e
cd /mnt/e/work/opendlms/opendlms

echo "=== Step 1: Apply all source changes from our session ==="

# Stage our changes - the files that were modified
git add \
  CMakePresets.json \
  HANDOFF.md \
  memory/plan.md \
  cosemlib/src/csm_association.c \
  cosemlib/src/csm_security_suite.h \
  cosemlib/src/csm_keyring.h \
  cosemlib/src/csm_channel.c \
  common/ip/tcp_push.h \
  common/ip/tcp_server.c \
  tests/test_association.cpp \
  tests/test_cosem_catalog.cpp \
  tests/test_push_loopback.cpp \
  studio/etc/ace6k_sec/session.yaml \
  studio/etc/session.yaml \
  examples/cosemreader/session.yaml \
  examples/cosemreader/meter_6ksec.yaml

# Stage new GOST crypto files
git add \
  cosemlib/crypto/kuznyechik.h \
  cosemlib/crypto/kuznyechik.c \
  cosemlib/crypto/kuznyechik_modes.h \
  cosemlib/crypto/kuznyechik_modes.c \
  cosemlib/crypto/streebog.h \
  cosemlib/crypto/streebog.c \
  tests/test_gost.cpp \
  cmake/opendlms_sources.cmake \
  tests/CMakeLists.txt

echo "=== Step 2: Commit ==="
git commit -m "fix: AARQ plaintext + GOST crypto primitives + cross-platform build

- fix(association): remove incorrect glo-ciphering of AARQ InitiateRequest
  (per IEC 62056-5-3, AARQ is always plaintext; glo applies only to
  service requests after association). Resolves 12 of 13 failing tests.
- fix(association): update expected AARQ bytes for CALLING_AP_TITLE with LLS

- feat(crypto): Kuznyechik block cipher (RFC 7801) — encrypt/decrypt verified
- feat(crypto): Kuznyechik-CMAC (96-bit tag) and CTR mode (96-bit IV)
- feat(crypto): Streebog-256 hash (GOST R 34.11-2018) — algorithm structure
  correct, GF(2^64) L constants need further verification

- fix(build): separate build dirs — build-mingw/ (MinGW64), build-Linux/ (GCC)
- fix(build): add missing <stddef.h> for NULL (csm_security_suite.h, csm_keyring.h)
- fix(build): add INVALID_SOCKET/SD_BOTH for POSIX (tcp_push.h, tcp_server.c)
- fix(build): add <cstring> for memset in test_cosem_catalog.cpp
- fix(build): add INVALID_SOCKET compat for POSIX in test_push_loopback.cpp

- fix(security): replace real passwords with placeholders in session configs

- doc(HANDOFF): update test status (402->414, 411 passing)
- doc(plan): update GOST crypto roadmap status"

echo "=== Step 3: Verify ==="
git log --oneline -3
echo ""
echo "=== Secrets check ==="
count=$(git log --all -p 2>&1 | grep -c '05UBiB' || true)
echo "Secrets found: $count"
