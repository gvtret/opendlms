# OpenDLMS production readiness audit

Date: 2026-06-26
Branch: full-rework

## Current status

The core in-process DLMS/COSEM stack is now buildable and test-green in the
current repository state. The repository is closer to a production-ready stack,
but external dependency wiring, CI gates, and live Electron/simulator validation
still need to be closed before release.

The repository contains a compilable core library, a legacy meter simulator, a newer
high-level client/server API, Electron Studio bindings, and tests. These pieces do
not currently form one consistently verified product:

- `examples/reader_lab` previously referenced missing `client/lib/opendlms_reader.c`
  and `client/include/opendlms_reader.h`. These files have been restored as a
  buildable TCP-wrapper reader API.
- The CMake test target builds only part of the checked-in tests.
- A clean MinGW test build compiles after C++ `nullptr` fixes, and the full
  `cosemtest` executable passes.
- `OPEN_DLMS_BUILD_CLIENT=ON` fails because the Gurux JSON dependency is not
  declared or discovered by CMake.
- Legacy COSEM-TCP simulator code uses the IEC 62056 TCP WPDU header, while the
  newer TCP transport previously implemented only the LLC `E6 E6/E7 00` wrapper.
- The high-level `csm_client_connect()` previously opened only the socket and did
  not perform ACSE AARQ/AARE association.
- Several public scripting APIs were stubs.
- Some IC handlers remain registry stubs or partial implementations.

## Changes made in this pass

- Added `CSM_FRAMING_TCP_WRAPPER` for IEC 62056 TCP WPDU framing.
- Made TCP client `open` establish the socket connection instead of silently
  leaving the channel disconnected.
- Added timeout-aware TCP receive handling so client calls return timeout errors
  instead of hanging indefinitely.
- Made high-level `csm_client_connect()` send AARQ, receive AARE, and mark the
  association as established only when accepted.
- Switched Electron Studio LuaBridge to TCP WPDU framing and raw APDU client
  framing.
- Implemented Lua `setCosem()` and `action()` using the real client API.
- Fixed Studio script open flow in a prior pass so `Open` loads file content into
  the editor.
- Fixed C++ test null pointer comparisons for modern GCC.
- Fixed `csm_channel_disconnect_ctx()` to handle both active channel IDs and
  legacy zero-based indexes.
- Fixed legacy Association LN object list encoding to write each object's OBIS
  instead of repeating the Association object's OBIS.
- Added GET error response encoding for database failures instead of silent
  no-response behavior.
- Restored `opendlms_reader.h`, `opendlms_reader.c`, and `csm_keyring.c`, and
  wired `reader_lab` into top-level CMake with `OPEN_DLMS_BUILD_READER_LAB`.
- Restored the legacy service execution path for `csm_channel_execute()` when
  the legacy global `csm_services_init()` database handler is used.
- Hardened `csm_asso_init()` to clear the whole association state before setting
  defaults, preventing stale dedicated-key/auth fields from corrupting AARQ.
- Fixed SET/ACTION request decoding so payload bytes are passed to IC handlers
  instead of re-decoding the request header.
- Updated integration expectations for standard GET error responses with
  `data-access-result` instead of exception APDUs.

## Verified

- `cmake --build build-audit-tests --target cosemlib cosemserver metersimulator cosemtest -j2`
  completes.
- Targeted channel lifecycle test passes:
  `./build-audit-tests/tests/cosemtest.exe "csm_channel_ctx API"`.
- Full in-process test suite passes:
  `./build-audit-tests/tests/cosemtest.exe -r compact`
  reports `Passed all 119 test cases with 715 assertions`.
- Full integration suite passes:
  `./build-audit-tests/tests/cosemtest.exe "[integration]" -r compact`
  reports `Passed all 49 test cases with 188 assertions`.
- CTest passes:
  `ctest --test-dir build-audit-tests --output-on-failure`
  reports `100% tests passed, 0 tests failed out of 1`.
- Electron native addon rebuilds successfully.
- LuaBridge can connect to the local meter simulator and decode AARE.
- `reader_lab` builds from a clean CMake directory with
  `OPEN_DLMS_BUILD_READER_LAB=ON`.

## Still open

- Lua `getObjectList()` and `reader_lab` live GET need to be re-verified against
  a freshly built simulator process.
- The old `metersimulator.exe` process on port 4063 could not be terminated from
  this session (`Access is denied`), so live simulator verification was performed
  against the already-running process.
- Build output still contains enum arithmetic warnings in `csm_association.h`.

## Required before calling this production-ready

1. Re-run end-to-end live simulator validation with a freshly built
   `metersimulator.exe`, `reader_lab`, and Electron LuaBridge.
2. Make CMake build every intended test file, or explicitly mark tests as
   experimental/disabled with a reason.
3. Add end-to-end TCP WPDU tests covering AARQ/AARE, GET object list, GET error
   response, SET, ACTION, and disconnect.
4. Replace registry IC stubs with real handlers or document them as unsupported
   and make unsupported operations return standard COSEM errors.
5. Make all optional external dependencies discoverable at configure time with
   clear failure messages.
6. Add CI gates for clean MinGW and native addon builds.
