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
- Removed the global `opendlms_reader` transport context so each reader session
  owns its own transport callback state.
- Fixed high-level client receive timeout propagation so
  `csm_client_connect()` and subsequent client requests use the configured
  transport timeout instead of a hard-coded value.
- Added public `opendlms_reader_set()` and `opendlms_reader_action()` wrappers
  so the restored C reader API covers GET, SET, and ACTION service calls.
- Extended `reader_lab` so examples cover GET by default and optional SET/ACTION
  calls via `set-u32=N`, `action=N`, and `class=N` flags.
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
- Made `reader_lab sync-ic` fail before opening TCP and made the C reader reject
  invocation-counter sync sessions until the full security-client handshake is
  available, avoiding a misleading no-op configuration.
- Implemented Profile Generic `capture` with real capture-object reads through
  the IC registry instead of storing `NULL` values while reporting success.
- Reset all built-in IC handler pool counters from `db_ic_init()` so repeated
  sessions/tests in one process do not exhaust static instance pools.
- Added Image Transfer state guards for start, verify, and activate so
  out-of-sequence ACTION calls return DLMS errors instead of false success.
- Added generic validation for ACTION methods declared as `AXDR_TAG_NULL` so
  unexpected payloads return a DLMS error instead of being ignored.
- Made delete-style ACTION methods for Register Activation masks and Parameter
  Monitor entries report a DLMS error when the requested item does not exist.
- Implemented Schedule `insert`/`delete` ACTION handling with persistent AXDR
  entry storage keyed by entry id.
- Implemented Special Days `insert`/`delete` ACTION handling with persistent
  AXDR entry storage keyed by day id.
- Made Script Table `execute` validate its script id parameter and report a
  DLMS error when the target script is not present instead of returning a
  successful no-op.
- Implemented Sensor Manager `reset` as a real state reset instead of reporting
  a successful no-op.
- Made Register Table `capture` validate its configured active target and read
  it through the IC registry instead of returning unconditional success.
- Made SAP Assignment `connect_logical_device` validate that the requested SAP
  exists in the assignment list instead of accepting any SAP id.
- Implemented Activity Calendar passive-calendar SET so
  `activate_passive_calendar` can promote real configured data instead of only
  copying the initial NULL placeholder.
- Implemented Table Manager `retrieve_entries_by_row` over configured
  `table_data` instead of returning an empty successful result for every valid
  selector.
- Implemented Profile Filter `retrieve_entries_by_row` over configured filter
  entries by reading target object attributes through the IC registry.
- Implemented Register Monitor SET support for thresholds, monitored value, and
  actions so its reset/action behavior operates on configured state.
- Implemented Single Action Schedule SET support for executed script, schedule
  type, and execution time.
- Fixed high-level client SET/ACTION request encoding so normal requests include
  the required presence flags before payload data.
- Extended `reader_lab` with raw attribute, SAP, destination, and AXDR
  `set-hex` flags so live examples can exercise non-default attributes and
  raw COSEM payloads.
- Made legacy `common/ip/tcp_server` accumulate TCP wrapper frames instead of
  assuming each `recv()` returns exactly one complete WPDU, and made socket
  writes drain the full response buffer.
- Fixed client response initialization and GET/ACTION response decoding for
  data-access-result and action return-parameter discriminators.
- Implemented mutable simulator Clock SET and an ACTION method path for live
  TCP smoke coverage.
- Added optional CTest `reader_lab_live_smoke` covering TCP-wrapper AARQ/AARE,
  GET, SET, ACTION, GET error response, and client disconnect against a real
  `metersimulator` process.
- Moved `test_clock.cpp` into the default Catch2 gate and fixed
  `clk_last_dow()` for February in leap years.
- Moved AES-128 GCM/GMAC vector coverage from Unity into the default Catch2
  gate.
- Moved the standalone Streebog L-transform debug check into the default Catch2
  gate.
- Hardened `csm_array_read_buff()` against short buffers and fixed read/write
  jumps that land exactly at the logical end of the buffer.
- Hardened `csm_array_get()` against null array/output pointers while
  preserving out-of-range zeroing for valid output pointers.
- Hardened `csm_array_set()` against null array pointers.
- Hardened `csm_array_read_buff()` and `csm_array_write_buff()` against null
  array/buffer inputs.
- Hardened scalar `csm_array` jump, read, size, and data-pointer helpers
  against null inputs.
- Hardened `csm_array_dump()` against null input.
- Hardened generic `CSM_FRAMING_NONE` frame/deframe helpers against null
  inputs before memory copies or output pointer writes.
- Hardened generic `CSM_FRAMING_HDLC` dispatch against null inputs and short
  output buffers before calling the legacy HDLC encoder.
- Hardened TCP transport receive entry points against null or zero-length
  output buffers before accept/read/extract work.
- Hardened shared LLC wrapper frame/deframe helpers to reject zero-length
  APDUs consistently with TCP wrapper framing.
- Hardened TCP transport send entry points against null or zero-length
  payloads before framing or socket writes.
- Hardened TCP client initialization to reject null or empty hosts before
  allocating transport context state.
- Hardened high-level client GET/SET/ACTION and block-transfer entry points
  against null OBIS, response buffers, and inconsistent payload pointers before
  encoding or transport send.
- Hardened server send paths to reject empty APDUs before transport send and to
  propagate response send failures from `csm_server_poll()`.
- Fixed high-level SET block transfer to encode octet-string chunk lengths via
  BER length encoding instead of truncating chunks above 255 bytes.
- Hardened SET/GET block receive accumulation against integer wraparound in
  receive-buffer capacity checks.
- Hardened block encode paths against zero-sized chunks and corrupted offsets
  before calculating remaining transfer sizes.
- Hardened public service helper entry points against null response/array inputs
  and made HLS rejection helpers fail closed on null output arrays.
- Hardened BER decode helpers against null inputs and truncated object
  identifier buffers.
- Hardened AXDR decode/encode helpers against null inputs and truncated BER
  length fields before reading or writing output state.
- Hardened keyring add/find helpers against corrupted entry counts before
  indexing fixed-size key storage.
- Hardened security auth encrypt/decrypt helpers against null inputs, truncated
  security headers, and missing AAD prefix space before in-place cipher work.
- Moved the historical block-read AXDR test into the default Catch2 gate,
  fixed `csm_axdr_decode_tags()` success reporting, and made the tag decoder
  recursively walk arrays and structures after block-transfer reassembly.
- Hardened `hdlc_decode()` against null, empty, truncated, and below-minimum
  frame buffers before reading frame-format or FCS fields.
- Hardened reader auth preset helpers against null output pointers and moved
  the preset role mapping into the default Catch2 gate.
- Added default-gate TCP wrapper tests for WPDU version, source/destination
  ports, payload length, malformed version, and truncated frame detection.
- Fixed `csm_transport_tcp` receive handling so TCP wrapper frames split across
  multiple socket reads are accumulated until a complete APDU is available.
- Implemented Studio Lua `delay(ms)` as a real non-negative sleep instead of a
  no-op and added native addon tests for wait and validation behavior.
- Fixed TCP server accept paths so receive and manual accept honor configured
  timeouts instead of blocking indefinitely when no client connects.
- Hardened public TCP transport connect/accept helpers against null transport
  contexts.
- Added explicit casts and default-gate round-trip coverage for the shared
  big-endian 64-bit utility helpers.
- Added bounded casts for decoded ACSE authentication-value lengths after the
  existing challenge-size validation.
- Removed the temporary global database-handler swap from explicit service
  execution so independent channel/server contexts dispatch through their own
  handlers without mutating shared state.
- Replaced the reader HAL 16-byte hex-key parser with strict bounded parsing
  and added a dedicated CTest smoke for valid, short, long, and non-hex inputs.
- Replaced `reader_lab` OBIS parsing with strict bounded component parsing and
  moved CLI validation ahead of platform/TCP setup so malformed input fails
  before opening a network connection.
- Fixed the simulator HAL to use its SAP configuration for keys, LLS password,
  and mechanism lookup, added GCM bounds checks, and aligned `reader_lab reader`
  with a working 32-to-1 LLS association covered by live smoke.
- Implemented HAL selective-access payload pass-through for simulator/test
  builds so services no longer reject every selective-access request during
  decoding.
- Hardened reader, simulator, and test HAL crypto/system hooks against null
  system-title input, invalid random ranges, null GCM keys, invalid channels,
  and GCM update/finish errors.
- Added active-state tracking and cleanup for HAL GCM contexts so update/finish
  calls fail before init, repeated init frees the prior context, and finish
  releases the active context.
- Rejected all service traffic while HLS association is pending, preventing
  pre-HLS GET/SET/ACTION execution on Configurator sessions before pass 3/4
  completes.
- Returned a standard DLMS exception response for rejected pending-HLS service
  traffic and made the restored reader API surface decoded exceptions as
  failed operations.
- Made the restored reader API return operation failure for decoded DLMS
  GET/SET/ACTION error results instead of treating every decoded response as
  success.
- Fixed the legacy TCP simulator server to dispatch received frames to the
  accepted peer's channel instead of always using channel zero.
- Corrected public README/example documentation to describe the current
  production-gated reader profiles and fail-closed Configurator/HLS status
  instead of implying a completed secure-client path.
- Corrected public README wording around thread safety to point users at
  context-based production APIs while acknowledging legacy compatibility
  helpers.
- Corrected reader auth profile header comments so Configurator helpers no
  longer imply completed HLS service access.
- Hardened the restored C reader TCP-wrapper transport to reject malformed,
  zero-length, and wrong-wPort response frames, and made `reader_lab` drain
  partial socket writes.
- Hardened the live meter simulator TCP wrapper handler against null buffers,
  invalid channel ids, and payload sizes outside the supplied memory buffer.
- Replaced `reader_lab` numeric option parsing with strict bounded decimal
  parsing so malformed ports, SAPs, class ids, attributes, SET values, and
  ACTION ids fail before network setup.
- Hardened the shared COSEM-TCP wrapper deframer to reject zero-length APDUs.
- Hardened the shared COSEM-TCP wrapper framer to reject zero-length APDUs,
  keeping frame/deframe contracts consistent.
- Replaced single global HAL invocation counters in simulator/test builds with
  per-SAP, per-direction counters.
- Replaced the missing legacy Qt Studio `add_subdirectory(studio)` failure with
  an explicit CMake diagnostic pointing users to `electron-studio`.
- Fixed installed `cosemlib` public headers so downstream projects using
  `find_package(OpenDLMS)` can include `cosemlib.h`, transport, framing, and
  security headers from the install prefix.
- Added install/export rules for `opendlms_reader` so downstream projects can
  link `OpenDLMS::opendlms_reader` when `OPEN_DLMS_BUILD_CLIENT=ON`.
- Expanded Linux CI coverage to build the restored reader API, meter simulator,
  reader_lab live smoke, install tree, CMake package consumer, pkg-config
  consumer, and deterministic Electron dependency install via `npm ci`.
- Added Electron Studio native addon tests to CI after the native build, before
  packaging.
- Hardened Electron Studio Lua `obis()` and table OBIS parsing against malformed
  strings and out-of-range components.
- Hardened Electron Studio Lua `connect()` against invalid TCP port values
  before transport setup.
- Added a `production` CMake preset that enables the full local production gate:
  tests, server, restored reader API, meter simulator, and reader_lab live smoke.
- Added live `reader_lab config` smoke coverage that proves Configurator
  associations no longer execute GET before HLS pass 3/4 completes.
- Hardened `reader_lab` profile parsing so unknown profile names fail early
  instead of silently using the public association.
- Replaced `rand()`-based ACSE/HLS challenge bytes in the active reader and
  simulator HALs with OS CSPRNG bytes and unbiased range sampling.
- Fixed ACSE AARQ/AARE BER length finalization so HLS challenge sizes that
  push payloads past 127 bytes use definite long-form lengths.
- Made `opendlms_reader_init()` initialize the bundled reader HAL so installed
  static `OpenDLMS::opendlms_reader` consumers pull the HAL symbols needed by
  `cosemlib`.
- Removed duplicate Windows `advapi32` linkage from the `opendlms_reader`
  CMake target.
- Classified `examples/metersimulator/tests` as manual/legacy coverage with an
  in-directory README, pointing production live coverage at
  `tests/live_reader_lab_smoke.py`.
- Strengthened CI so the installed CMake consumer links
  `OpenDLMS::opendlms_reader` directly and the Windows build covers the reader
  API, meter simulator, and `reader_lab` targets.
- Classified the checked-in Streebog research/debug probes in `tests/README.md`;
  maintained crypto coverage remains in the default Catch2 gate.

## Verified

- `cmake --build build-audit-tests --target cosemlib cosemserver metersimulator cosemtest -j2`
  completes.
- Targeted channel lifecycle test passes:
  `./build-audit-tests/tests/cosemtest.exe "csm_channel_ctx API"`.
- Full in-process test suite passes:
  `./build-audit-tests/tests/cosemtest.exe -r compact`
  reports `Passed all 176 test cases with 1557 assertions`.
- Full integration suite passes:
  `./build-audit-tests/tests/cosemtest.exe "[integration]" -r compact`
  reports `Passed all 83 test cases with 775 assertions`.
- CTest passes:
  `ctest --test-dir build-audit-client --output-on-failure`
  reports `100% tests passed, 0 tests failed out of 3`.
- Full production preset passes:
  `cmake --preset production && cmake --build --preset production && ctest --test-dir build-production --output-on-failure`
  reports `100% tests passed, 0 tests failed out of 3`.
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
- Integration tests now verify Profile Generic `capture` stores real captured
  AXDR object values in the profile buffer.
- Integration tests now verify repeated Image Transfer setup across test cases
  after IC pool reset and end-to-end Image Transfer start/verify/activate state
  guards.
- Integration tests now verify `AXDR_TAG_NULL` ACTION methods reject unexpected
  payloads and still accept an explicit AXDR NULL parameter.
- Integration tests now verify failed Register Activation mask deletion and
  Parameter Monitor entry deletion leave their lists unchanged.
- Integration tests now verify Schedule and Special Days insert/delete mutate
  their entry arrays through the normal ACTION service path, including missing
  delete target errors.
- Integration tests now verify Script Table execute returns a DLMS error when
  the requested script id is not present.
- Integration tests now verify Sensor Manager `reset` clears previously SET
  active-variant, retry-count, and data-protection state.
- Integration tests now verify Disconnect Control remote disconnect/reconnect
  mutate output state and reject unexpected ACTION payloads without changing it.
- Integration tests now verify Register Table `capture` fails without a
  configured readable target, succeeds for a real Register object attribute, and
  rejects an out-of-range active index.
- Integration tests now verify SAP Assignment `connect_logical_device` fails for
  unassigned SAP ids and succeeds after the SAP assignment list is configured.
- Integration tests now verify Activity Calendar passive-calendar SET followed
  by activate promotes the configured calendar into active-calendar GET.
- Integration tests now verify Table Manager row retrieval returns selected
  table-data rows and rejects out-of-range selectors.
- Integration tests now verify Profile Filter row retrieval returns configured
  target object values and rejects out-of-range selectors.
- Integration tests now verify Register Monitor SET/GET for thresholds/actions
  and reset clearing monitored value.
- Integration tests now verify Single Action Schedule SET/GET for executed
  script, schedule type, and execution time.
- Live `reader_lab`/`metersimulator` smoke passes through CTest:
  `ctest --test-dir build-audit-client -R reader_lab_live_smoke --output-on-failure`
  reports `100% tests passed, 0 tests failed out of 1`.
- Clock utility tests now run in the default Catch2 gate and verify leap-year
  last-day-of-week behavior.
- AES-128 GCM/GMAC tests now run in the default Catch2 gate and verify both the
  NIST GCM tag vector and the GreenBook HLS5 GMAC tag vector.
- Streebog L-transform regression coverage now runs in the default Catch2 gate.
- Array utility tests now cover exact-end reads/writes and short-buffer read
  rejection.
- Block-read AXDR tests now run in the default Catch2 gate and verify partial
  block detection, block-transfer payload reassembly, recursive nested AXDR
  traversal, and flat tag decoder success behavior.
- Block transfer tests now verify SET and GET receive accumulation buffers are
  stored per block state instead of sharing one global receive buffer.
- HDLC tests now verify undersized frame rejection before parser reads beyond
  the provided buffer.
- Reader auth tests now verify null-output safety and Public, Reader,
  Configurator, and plain Configurator association profile mappings.
- TCP wrapper tests now verify frame/deframe round-trips and malformed WPDU
  rejection in the default transport gate.
- TCP transport tests now verify split WPDU receive reassembly across multiple
  socket reads.
- TCP transport tests now verify server-side receive accept and manual accept
  return timeout when no client connects.
- TCP transport tests now verify public connect/accept helpers reject null
  transport contexts.
- TCP transport tests now verify manual client connect succeeds through the
  timeout-aware public connect path.
- TCP transport tests now verify multiple client transports keep independent
  contexts instead of sharing one overwritten static client state.
- Generic framing tests now verify raw passthrough frame/deframe null-input
  rejection.
- Generic framing tests now verify HDLC dispatch rejects null pointers and
  undersized output buffers before invoking the encoder.
- TCP transport tests now verify receive rejects null and zero-length output
  buffers without waiting for socket activity.
- TCP transport tests now verify send rejects null and zero-length payloads on
  a connected client path.
- TCP transport tests now verify client initialization rejects null or empty
  hosts without mutating the transport instance.
- Client transport tests now verify request APIs reject invalid inputs before
  any transport send/receive callbacks are invoked.
- Server transport tests now verify direct send rejects invalid APDUs before
  invoking the transport send callback.
- Client transport tests now verify multi-block SET chunks above 255 bytes use
  long-form BER octet-string length encoding.
- Block-transfer tests now verify receive accumulation rejects overflowed
  offsets for both SET and GET receive paths.
- Block-transfer tests now verify GET and SET encode paths reject zero max
  sizes and overflowed offsets.
- Cosemlib umbrella tests now verify client service helpers reject null inputs.
- Cosemlib service tests now verify failed client response decoding preserves
  the input cursor and caller-owned response state.
- Cosemlib BER tests now verify null input rejection and truncated object
  identifier handling.
- AXDR read-by-block tests now verify null input rejection and truncated BER
  length handling.
- Cosemlib AXDR tests now verify boolean values are encoded with the standard
  BOOLEAN tag rather than as octet strings.
- Cosemlib AXDR tests now verify failed primitive reads preserve the read
  cursor instead of consuming bytes from malformed input.
- Cosemlib AXDR tests now verify nested structure decoding rejects excessive
  recursion depth.
- Core array tests now verify invalid initialization parameters and corrupted
  bounds fail closed without pointer underflow.
- Core array tests now verify oversized reader/writer jumps and writes cannot
  wrap buffer indices.
- Cosemlib keyring tests now verify corrupted counts cannot drive out-of-range
  fixed-array access.
- Cosemlib security suite tests now verify supported/unsupported suites, null
  algorithm outputs, and boolean support predicate semantics.
- Cosemlib catalog tests now verify YAML buffers without a trailing newline are
  parsed completely and overlong lines fail without retaining partial entries.
- Cosemlib catalog numeric parsing now rejects overflowing class/version/OBIS
  values before integer wraparound.
- Cosemlib catalog file loading now rejects null filenames, seek/size errors,
  short reads, and file sizes that would overrun the fixed parser buffer.
- Cosemlib object-list tests now verify exported Association LN object lists
  can be imported back with access-rights fields consumed correctly and reject
  trailing data.
- Crypto/security tests now verify auth encrypt/decrypt reject null and
  short-prefix inputs before invoking GCM.
- Restored reader API tests now verify connect rejects incomplete IO
  configurations before sending an AARQ.
- Wrapper tests now verify LLC frame/deframe helpers reject zero-length APDUs.
- Core utility tests now verify 64-bit big-endian write/read round-trips.
- Client/API build passes:
  `cmake -S . -B build-audit-client -G Ninja -DOPEN_DLMS_BUILD_CLIENT=ON -DOPEN_DLMS_BUILD_TESTS=ON -DOPEN_DLMS_BUILD_READER_LAB=ON -DOPEN_DLMS_BUILD_METER_SIM=ON`
  followed by `cmake --build build-audit-client -j2`.
- `opendlms_reader` and `reader_lab` targeted build passes:
  `cmake --build build-audit-client --target opendlms_reader reader_lab -j2`.
- Transport tests now verify client connect propagates the configured receive
  timeout to the transport receive callback.
- Legacy JSON CLI dependency failure is explicit:
  `OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT=ON` reports the missing Gurux
  `JsonReader.h`, `JsonValue.h`, `JsonWriter.h`, and `Util.h` headers.
- Electron native addon rebuilds successfully.
- Electron native tests pass:
  `npm test` reports `51 tests, 51 passed, 0 failed`.
- Electron native HAL now uses the core MD5/SHA1/SHA256 and AES-GCM
  primitives, stores configured 128-bit/256-bit security keys, and fails secure
  GCM setup when no key is configured instead of silently using zero keys or
  pass-through data.
- LuaBridge live smoke passes against a freshly built simulator on port 4165:
  `npm run test:live -- 4165` reports a non-empty object-list response.
- `reader_lab` builds from a clean CMake directory with
  `OPEN_DLMS_BUILD_READER_LAB=ON`.
- `reader_lab` live GET passes against a freshly built simulator on port 4165:
  `reader_lab public 127.0.0.1 4165 0.0.1.0.0.255` reports `Association OK`
  and `GET OK`.

## Still open

- Checked-in Streebog research/debug probes are now explicitly documented as
  outside the default Catch2/CTest gate; remove them before release if the
  project does not want to ship implementation research artifacts.
- TCP-wrapper live coverage currently proves AARQ/AARE, public GET clock,
  reader LLS GET clock, Configurator pre-HLS GET rejection, SET clock, ACTION
  clock, GET error response, client disconnect, and LuaBridge `getObjectList()`.
  It still needs full ciphered security-profile live coverage for HLS pass 3/4
  and protected GET/SET/ACTION after HLS completion.
- Configurator invocation-counter discovery/synchronization is deliberately
  gated off in the restored C reader API until the full security-client
  handshake path is implemented; callers must seed the counter explicitly.
- Several IC handlers remain partial implementations and must either be
  completed or documented as unsupported with standard COSEM errors.

## Required before calling this production-ready

1. Decide whether to keep or remove documented research/debug probes before
   release packaging.
2. Extend end-to-end TCP WPDU tests with ciphered security-profile live
   coverage and gateway/proxy scenarios.
3. Complete remaining partial IC handlers or document unsupported operations
   with standard COSEM errors.
4. Add a clean MinGW CI gate in addition to the existing Linux/Windows/macOS
   and Electron gates.
