# MEMORY.md - OpenDLMS Project Memory

## Project Overview
- **OpenDLMS**: C99 DLMS/COSEM protocol stack (IEC 62056), MIT license
- **Repo**: E:\work\opendlms\opendlms
- **Version**: 1.1.0
- **Tests**: ~215 test cases, ~1950 assertions — ALL PASSING (GCC + Clang ASan/UBSan)
- **Build**: `cmake --preset production && cmake --build --preset production`
- **Tests**: `ctest --test-dir build-production`

## Project Structure (3-layer architecture)
```
opendlms/
├── cosemlib/          # Core protocol library (C99, allocation-free core logic)
│   ├── src/           # BER/AXDR, association, channel, security, services
│   ├── ic/            # 33 IC classes (IEC + СПОДУС 8200), registered but test-only
│   ├── model/         # YAML catalog parser, registry, object_list import
│   ├── profile/       # SPODES/SPODUS OBIS + capture templates
│   ├── crypto/        # AES-GCM-128/256, HLS 3/4/5(GMAC)/6, SHA-256/384/512, CMAC (Apache 2.0)
│   ├── hdlc/          # HDLC framing encoder/decoder
│   └── util/          # OS abstractions, clock, endian helpers
├── server/            # Server wiring: associations, push, event notify, catalog
├── client/            # DLMS client library + reader API, HAL, HDLC/TCP
├── common/            # Shared transport: TCP client/server, serial port
├── examples/          # metersimulator, reader_lab, embeddedmeter
├── tests/             # 18 test files, Catch2 C++17
└── electron-studio/   # Electron Studio native bindings and Lua bridge
```

## Architecture (golden rule)
**cosemlib** = all DLMS/COSEM + SPODES/SPODUS logic. **server/** and **client/** = wiring only (HAL, transport, poll). Applications (examples/) = concrete instance data, OBIS, demo data.

Key design: Portable C99, allocation-free core protocol logic (server/client convenience wrappers and mbed TLS use malloc), zero-copy buffers, `csm_array` overflow protection, optional printf-style tracing per module.

## Key APIs
- IC layer: `db_cosem_ic_inst_create()`, `db_cosem_ic_dispatch()`, `db_cosem_ic_service(ms)`
- Catalog: `db_cosem_catalog_load_yaml("object_list.yaml")`, `db_cosem_catalog_parse_buffer()`
- Push: `db_cosem_push_set_transport_cb()`, `db_cosem_push_trigger()`, `db_cosem_push_service(ms)`
- Profile: `db_cosem_ic_profile_set_capture_cb()`
- Values: `db_cosem_register_get/set_value`, `db_cosem_data_get/set_u8`, `db_cosem_clock_get/set_datetime`
- Client: `csm_client_connect()`, `csm_client_get_block()`, `csm_client_set_block()`, `csm_client_action()`
- Transport: `csm_transport_tcp_server_init()`, `csm_transport_tcp_client_init()`

## What's Working
- **Codec**: BER/AXDR, HDLC framing (IEC 62056-46), COSEM TCP Wrapper
- **Association**: AARQ/AARE/RLRQ/RLRE, Association LN v2, SN map
- **Security**: HLS 3/4/5(GMAC)/6, AES-GCM-128/256, Kuznyechik, CMAC, Key Ring, AES Key Wrap
- **Services**: GET (normal/block), SET (normal/block), ACTION, ACCESS (batch), GBT, Exception Response
- **Ciphering**: End-to-end glo-* envelope (Green Book 9.2.7.2), per-association IC, IC persistence via HAL
- **Client HLS**: Complete GMAC handshake (AARQ calling-AP-title → pass 3 f(StoC) → pass 4 verify f(CtoS))
- **Push**: Confirmed-mode, ciphered push, GBT wrapping, trigger poll, DataNotification codec
- **IC Layer**: 33 registered classes (1-11, 15-23, 26, 30-31, 40-41, 61-65, 67-68, 70-71, 8200)
- **SPODUS**: Table Manager (class 8200, СТО 34.01-5.1-013-2023)

## Security Notes
- GMAC tag comparison is constant-time (no timing oracle)
- Invocation counter replay protection: **not yet enforced** (SEC-1 in audit)
- Key zeroization: responsibility on HAL implementation
- Streebog Suite 9 (GOST 34.11-2018): not functional (GF L-transform broken), fails closed

## Durable Gotchas
- **Cipher+GBT ordering**: Outgoing must be Execute → Cipher → GBT wrap (not reverse)
- **csm_array_init**: 5th arg is buffer OFFSET, not rd_index (rd_index always starts at 0)
- **Security decrypt contract**: requires rd_index >= 17 after consuming 5-byte SC/IC header
- **GOST Suite 8**: KUZN-CTR-CMAC (implemented); Suite 9: fail-closed (missing Streebog VKO/signature)
- **СПОДЭС ≠ СПОДУС**: СТО 34.01-5.1-006 = СПОДЭС (meters), СТО 34.01-5.1-013 = СПОДУС (concentrators, defines classes 8200/8201)

## Lab Meter (LAN)
- **Host**: 192.168.1.116:4059, Auto HDLC/COSEM Wrapper on TCP
- **Associations**: Public (SAP 16, no security), Reader (SAP 32, LLS, password), Configurator (SAP 48, HLS GMAC)
- **Keys (suite 0 = AES-GCM-128)**: GUEK=`303132333435363738393A3B3C3D3E3F`, GAK=`404142434445464748494A4B4C4D4E4F`, KEK=`31313131313131313131313131313131`

## Build & CI
```bash
cmake --preset production                    # Configure
cmake --build --preset production            # Build
ctest --test-dir build-production            # Test
```
- CI: Linux GCC (full gate), Linux Clang ASan/UBSan (sanitizer gate), Windows MSVC, macOS, Electron
- 4 CMake presets: debug, release, production, debug-mingw

## License
- Core (cosemlib/): MIT — see LICENSE
- Crypto (cosemlib/crypto/): Apache 2.0 — see THIRD-PARTY-LICENSES.txt
- FreeRTOS (examples/embeddedmeter/rtos/): MIT — see THIRD-PARTY-LICENSES.txt

## User Preferences
- DLMS/COSEM document access via MCP doc-search for encoder/decoder development
- MEMORY.md for cross-session persistence
- Code formatting: .clang-format (tabs, CRLF, LLVM-based, column 160)
