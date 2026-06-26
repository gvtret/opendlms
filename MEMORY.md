# MEMORY.md - OpenDLMS Project Memory

## Project Overview
- **OpenDLMS**: C99 DLMS/COSEM protocol stack (IEC 62056), MIT license
- **Repo**: E:\work\opendlms\opendlms
- **Version**: 1.1.0 (2026-06-19)
- **Tests**: 402 test cases, 3256 assertions — ALL PASSING (MinGW-w64 GCC)
- **Build**: `cmake --preset debug-mingw && cmake --build build && build/tests/cosemtest`

## Project Structure (v1.1.0, 3-layer architecture)
```
opendlms/
├── cosemlib/         # Core protocol library (C99, no dynamic alloc, zero-copy)
│   ├── src/          # BER/AXDR, association, channel, security, services
│   ├── ic/           # 100 IC classes (98 IEC + 2 SPODUS), 33 semantic handlers, 67 generic
│   ├── model/        # YAML catalog parser, registry, object_list import
│   ├── profile/      # SPODES/SPODUS OBIS + capture templates
│   ├── crypto/       # AES-GCM-128/256, HLS 3/4/5/6, SHA-384/512, DRBG, CMAC
│   ├── hdlc/         # HDLC framing encoder/decoder
│   └── util/         # OS abstractions, clock, endian helpers
├── server/lib/       # Server wiring: associations, push, event notify, catalog legacy bridge
├── server/application/ # app_database.c (dispatch), app_calendar.c
├── client/           # DLMS client library + CLI (cosemclient), HDLC/TCP
├── common/           # Shared transport: TCP client/server, serial port
├── examples/         # metersimulator, tcpcli, dlmscli, embeddedmeter, cosemreader
├── studio/           # Shaddam GUI (Qt6, Lua scripting)
├── tests/            # 60 test files, Catch2 C++17
├── cmake/            # Build helpers, embed functions, source lists
└── docs/             # ARCHITECTURE.md, RELEASE.md, spodes_catalog_reference.md
```

## Architecture (golden rule)
**cosemlib** = all DLMS/COSEM + SPODES/SPODUS logic. **server/** and **client/** = wiring only (HAL, transport, poll). Applications (examples/) = concrete instance data, OBIS, demo data.

Key design: Pure portable C99, no dynamic allocation, zero-copy buffers, `csm_array` overflow protection, optional printf-style tracing per module.

## Key APIs
- IC layer: `db_cosem_ic_inst_create()`, `db_cosem_ic_dispatch()`, `db_cosem_ic_service(ms)`
- Catalog: `db_cosem_catalog_load_yaml("object_list.yaml")`, `db_cosem_catalog_parse_buffer()`
- Push: `db_cosem_push_set_transport_cb()`, `db_cosem_push_trigger()`, `db_cosem_push_service(ms)`
- Profile: `db_cosem_ic_profile_set_capture_cb()`
- Values: `db_cosem_register_get/set_value`, `db_cosem_data_get/set_u8`, `db_cosem_clock_get/set_datetime`
- Client: `csm_client_decode()`, `csm_channel_client_encode_access()`, `csm_channel_client_process_dn()`
- TCP push: `cosem_wpdu_encode()`, `tcp_push_send()`, `tcp_push_session_bind_ex()`

## What's Working (v1.1.0)
- **Codec**: BER/AXDR, HDLC framing (verified per IEC 62056-46), COSEM TCP Wrapper
- **Association**: AARQ/AARE/RLRQ/RLRE, Association LN v3, SN map
- **Security**: HLS 3/4/5(GMAC)/6, AES-GCM-128/256, SHA-384/512, DRBG, CMAC, Key Ring, AES Key Wrap
- **Services**: GET (normal/block), SET (normal/block), ACTION, ACCESS (batch), GBT, Exception Response
- **Ciphering**: End-to-end glo-* envelope (Green Book 9.2.7.2), per-association IC, replay protection, IC persistence via HAL
- **Push**: Confirmed-mode, ciphered push, GBT wrapping, trigger poll, DataNotification codec
- **IC Layer**: 100 IC classes, catalog YAML materialization, Profile Generic selective access + streaming datablock
- **SPODES/SPODUS**: Table Manager (IC 8200/8201), aggregated event profiles, IVKE journals, push triggers
- **Studio**: Qt6 GUI, TCP/HDLC, Lua scripting, push listener, ciphering, ACCESS batch GET

## Durable Gotchas
- **Cipher+GBT ordering**: Outgoing must be Execute → Cipher → GBT wrap (not reverse). GBT tag 0xE0 has no glo mapping. [ses_1150331f8ffe]
- **HDLC_LEN_HI mask**: Was 0x03, must be 0x07 (11-bit length field: L10-L0). Frames >=256 bytes would decode wrong. [Phase 5.5]
- **csm_client_init() bug**: Does not clear csm_request — uninitialized fields cause spurious "Array Full". Workaround: memset(&request, 0, sizeof(request)) in encode_get_request. [Phase 2]
- **GCM return values**: HAL returns TRUE=1 for success, not CSM_SEC_OK=0. Check `!gcm_ret` not `gcm_ret != CSM_SEC_OK`. [Phase 8a]
- **csm_axdr_decode_tags**: Always returns FALSE — use callback state instead. [Phase 10]

## Discovered Durable Knowledge
- **IC registry**: 98 class IDs from IEC 62056-6-2 Table 4 (Blue Book), +2 SPODUS extensions (8200/8201) = 100 total
- **Security Policy bit layout**: Bits 0-1 reserved, bits 2-7 = {auth, encrypt, sign} x {request, response}
- **Security Setup v0 vs v1**: v0 = enum (0-3), v1 = bitfield
- **Table Manager methods**: `add_update_entries`/`remove_entries`/`retrieve_number_of_entries`/`retrieve_entries` (not AddRow/RemoveRow)
- **OBIS (IEC 62056-6-1 ED4)**: 6 value groups A-F; A=medium (0=abstract, 1=electricity), B=channel, C=channel number (Abstract: 0-89=context, 96=general, 97=error)
- **SPODES vs SPODUS**: Different standards — STO 006 = SPODES (meters), STO 013 = SPODUS (concentrators/IVKE). Never use interchangeably.
- **R 1323565.1 Suite 8**: KUZN_CTR + KUZN_CMAC; Suite 9: HLS GOST34112018-256 + GOST34102018-256
- **csm_config.h**: Memory footprint documentation (at `cosemlib/src/csm_config.h`)

## MCP Doc-Search
- **Server**: http://doc-mcp.misc-server:3333/mcp (user-doc-rag-remote)
- **Protocol**: `initialize` → `tools/list` → `tools/call` (requires `tools/call` wrapper, bare RPCs don't work)
- **Tool**: `doc_search(query, top_k)`
- **Docs**: Green Book 8.3, Blue Book 12.1, IEC 62056-5-3/6-1/6-2/7-6 ED4, GOST R 58940, SPODES (006), SPODUS (013), R 1323565.1

## Lab Meter (LAN)
- **Host**: 192.168.1.116:4059, Auto HDLC/COSEM Wrapper on TCP
- **Associations**: Public (SAP 16, no security), Reader (SAP 32, LLS, password), Configurator (SAP 48, HLS GMAC)
- **Keys (suite 0 = AES-GCM-128)**: GUEK=`303132333435363738393A3B3C3D3E3F`, GAK=`404142434445464748494A4B4C4D4E4F`, KEK=`31313131313131313131313131313131`

## Build & CI
```bash
cmake --preset debug-mingw && cmake --build build    # Build
build/tests/cosemtest                                 # Tests
cmake --preset release-full                           # Full examples (no Qt)
```
- CI: Linux GCC/Clang/ASan + Windows MinGW, clang-format lint
- PGO: preset `pgo-msvc` (GL + LTCG)
- 10 CMake presets: debug, release, MinGW, ASan, UBSan, Clang, etc.

## User Preferences
- DLMS/COSEM document access via MCP doc-search for encoder/decoder development
- MEMORY.md for cross-session persistence
- Configs use YAML (not JSON), except IDE/tooling (.vscode/, CMakePresets.json)

## License
- Core (cosemlib/): MIT
- Crypto: Apache 2.0 (mbed TLS derived, compatible)
- studio/Zip.cpp: GPLv3 (isolated)
- Total: 216+ files audited, LICENSE.txt includes third-party components
