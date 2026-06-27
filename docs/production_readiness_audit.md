# OpenDLMS production readiness audit

Date: 2026-06-27
Branch: full-rework

## Current status

The core in-process DLMS/COSEM stack and the primary TCP-wrapper examples are
buildable and test-green in the current repository state. The repository is
closer to a production-ready stack, but broader protocol coverage, CI gates, and
legacy/manual test classification still need to be closed before release.

The repository contains a compilable core library, a legacy meter simulator, a newer
high-level client/server API, Electron Studio bindings, and tests. These pieces do
not currently form one consistently verified product:

- `examples/reader_lab` previously referenced missing `client/lib/opendlms_reader.c`
  and `client/include/opendlms_reader.h`. These files have been restored as a
  buildable TCP-wrapper reader API.
- The default CMake test target builds the active Catch2 suite. Several checked-in
  legacy/manual tests still need either migration or explicit exclusion metadata.
- A clean MinGW test build compiles after C++ `nullptr` fixes, and the full
  `cosemtest` executable passes.
- `OPEN_DLMS_BUILD_CLIENT=ON` now builds the restored `opendlms_reader` API.
  The old JSON CLI is isolated behind `OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT`.
- Legacy COSEM-TCP simulator code uses the IEC 62056 TCP WPDU header, while the
  newer TCP transport previously implemented only the LLC `E6 E6/E7 00` wrapper.
- The high-level `csm_client_connect()` previously opened only the socket and did
  not perform ACSE AARQ/AARE association.
- Several public scripting APIs were stubs.
- Clock now has a minimal real registry handler for logical_name/time GET and
  time SET. Some other IC handlers remain partial implementations.

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
- Added a simulator port override via argv or `OPENDLMS_METER_PORT`, allowing
  isolated live tests without touching an already-running simulator.
- Split the restored C reader API into an `opendlms_reader` CMake target and
  made the legacy JSON CLI opt-in with an explicit Gurux header diagnostic.
- Added an Electron `test:live` smoke script that connects LuaBridge to a live
  simulator and performs `getObjectList()`.
- Removed public-header enum arithmetic warnings from `csm_association.h`.
- Replaced the Clock registry stub with a real minimal handler for logical_name
  and time, and removed unused generic IC stub infrastructure.
- Implemented Clock `adjust_to_quarter` and made unavailable preset-time action
  return a DLMS error instead of reporting a successful no-op.
- Made unsupported Security Setup key generation methods return a DLMS error
  instead of reporting a successful no-op.
- Mapped SET/ACTION database errors to more specific DLMS data-access-result
  values for object-undefined, read-write-denied, and temporary-failure.
- Increased the IC registry capacity so all built-in classes, including class
  8200 Table Manager, are registered.
- Fixed BER length encoding for long-form lengths and switched long
  octet-string IC buffers to standard AXDR length encoding/decoding.
- Implemented Register Activation `add_register` and `add_mask` ACTION handling
  instead of returning successful no-ops.
- Implemented Parameter Monitor `add_entry` ACTION handling and made unsupported
  Arbitrator `request_action` fail with a standard DLMS error instead of a
  successful no-op.
- Implemented Image Transfer `image_transfer_init` payload parsing and block
  status initialization instead of accepting any structure as success.
- Validated Profile Filter and Table Manager `retrieve_entries_by_row` selector
  payloads so malformed ACTION requests return DLMS errors instead of being
  accepted silently.
- Implemented Image Transfer `image_block_transfer` payload parsing and progress
  accounting instead of accepting any data as success.
- Implemented Compact Data `capture` by reading configured capture objects
  through the IC registry instead of only incrementing counters.

## Verified

- `cmake --build build-audit-tests --target cosemlib cosemserver metersimulator cosemtest -j2`
  completes.
- Targeted channel lifecycle test passes:
  `./build-audit-tests/tests/cosemtest.exe "csm_channel_ctx API"`.
- Full in-process test suite passes:
  `./build-audit-tests/tests/cosemtest.exe -r compact`
  reports `Passed all 137 test cases with 952 assertions`.
- Full integration suite passes:
  `./build-audit-tests/tests/cosemtest.exe "[integration]" -r compact`
  reports `Passed all 66 test cases with 411 assertions`.
- CTest passes:
  `ctest --test-dir build-audit-client --output-on-failure`
  reports `100% tests passed, 0 tests failed out of 1`.
- Integration tests now verify Clock time AXDR payload and SET/GET round-trip.
- Integration tests now verify Clock action handling for quarter-hour adjustment
  and preset-time failure behavior.
- Integration tests now verify read-only SET denial, unsupported Security Setup
  key-generation failure, and Table Manager built-in registration.
- BER unit tests now cover short/long-form length boundaries, and integration
  tests verify 130-byte AXDR octet-string SET/GET round-trips for Utility
  Tables, Compact Data, and Table Manager.
- Integration tests now verify Register Activation `add_register` and `add_mask`
  mutate their object and mask lists through the normal ACTION service path.
- Integration tests now verify Parameter Monitor `add_entry` mutates its monitor
  list and unsupported Arbitrator `request_action` returns a DLMS error.
- Integration tests now verify Image Transfer init parses the image identifier
  and image size structure and initializes transferred-block status.
- Integration tests now verify Profile Filter and Table Manager row retrieval
  selector validation for both valid and malformed ACTION payloads.
- Integration tests now verify Image Transfer block transfer updates transferred
  block count, block status bitmap, and first-not-transferred block number.
- Integration tests now verify Compact Data `capture` stores real captured AXDR
  object values in the buffer and updates entries-in-use.
- Client/API build passes:
  `cmake -S . -B build-audit-client -G Ninja -DOPEN_DLMS_BUILD_CLIENT=ON -DOPEN_DLMS_BUILD_TESTS=ON -DOPEN_DLMS_BUILD_READER_LAB=ON -DOPEN_DLMS_BUILD_METER_SIM=ON`
  followed by `cmake --build build-audit-client -j2`.
- Legacy JSON CLI dependency failure is explicit:
  `OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT=ON` reports the missing Gurux
  `JsonReader.h`, `JsonValue.h`, `JsonWriter.h`, and `Util.h` headers.
- Electron native addon rebuilds successfully.
- Electron native tests pass:
  `npm test` reports `44 tests, 44 passed, 0 failed`.
- LuaBridge live smoke passes against a freshly built simulator on port 4165:
  `npm run test:live -- 4165` reports a non-empty object-list response.
- `reader_lab` builds from a clean CMake directory with
  `OPEN_DLMS_BUILD_READER_LAB=ON`.
- `reader_lab` live GET passes against a freshly built simulator on port 4165:
  `reader_lab public 127.0.0.1 4165 0.0.1.0.0.255` reports `Association OK`
  and `GET OK`.

## Still open

- Some checked-in tests are not part of the default Catch2/CTest gate:
  `test_aes128gcm.cpp` uses Unity, `test_streebog_debug.cpp` has its own `main`,
  `examples/metersimulator/tests/test_fs.cpp` uses embUnit, and
  `test_clock.cpp`/`test_cosem_read_by_block.cpp` reference missing external
  headers (`date.h`, Gurux JSON/Common helpers).
- TCP-wrapper live coverage currently proves AARQ/AARE, GET clock, and
  LuaBridge `getObjectList()`. It still needs automated live SET, ACTION, error
  response, and disconnect assertions.
- Several IC handlers remain partial implementations and must either be
  completed or documented as unsupported with standard COSEM errors.

## Required before calling this production-ready

1. Make CMake build every intended test file, or explicitly mark tests as
   experimental/disabled with a reason.
2. Add end-to-end TCP WPDU tests covering AARQ/AARE, GET object list, GET error
   response, SET, ACTION, and disconnect.
3. Complete remaining partial IC handlers or document unsupported operations
   with standard COSEM errors.
4. Add CI gates for clean MinGW, live simulator smoke, and native addon builds.
