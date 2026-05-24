# DLMS/Cosem stack

This is an implementation of the DLMS/Cosem protocol in the MIT Open Source and permissive license. This protocol is mainly used in gas/water/electricity meters but is enough generic to target any IoT device.

See the official organization group to learn more: http://www.dlms.com.

This repository provides C code, mainly framing encoding/decoding functions. There is no any integration in a complete stand-alone examples. See other repositories for that purpose.

## Development goals

This Cosem stack has the following goals :

  * Pure portable and stand-alone ANSI C99 code
  * Fully unit tested with pre-defined vectors
  * Client/server implementation, LogicalName referencing, LLS, HLS3, 4 and 5, security policy 1
  * Examples using Cosem over TCP/IP
  * Memory efficient / no dynamic allocation (static, configurable at build-time) / no buffer copy
  * Full traces
  * Memory protected against buffer overflow using array utility

## What is working so far

  
  * BER coder/decoder
  * Association coders and decoders AARQ/AARE/RLRQ/RLRE (LLS)
  * Secure HLS5 GMAC Authentication
  * Get Request normal/by block
  * Set request normal
  * Action service
  * Exception response in case of problem
  * HDLC framing utility
  * Serial port HAL (Win32/Linux)
  * Utilities
  * Client reader over HDLC with full logging and XML output
  * Server example as a meter simulator
  * Scripting GUI tool prototype

## Directory organization

- `cosemlib`: kernel files of the DLMS/Cosem protocol
- `client`: specific utilities to write a DLMS/Cosem client
- `server`: specific utilities to write a DLMS/Cosem server
- `examples`: Client/server examples
- `studio`: Graphical utility with Lua scripting (WIP)
- `tests`: Unit tests for the protocol with raw frames and application objects

# How to build

## Prerequisites

- **CMake** >= 3.10
- A C99 / C++17 compiler (GCC, Clang, MSVC)

Optional:
- **Emscripten** (for WASM doc targets, `-DOPEN_DLMS_BUILD_DOCS=ON`)
- **Qt 5/6** (for Shaddam Studio, `-DOPEN_DLMS_BUILD_STUDIO=ON`)
- **Gurux JSON library** (for client tool, `-DOPEN_DLMS_BUILD_CLIENT=ON`)

## Quick start

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest --output-on-failure
```

To restrict what gets built, toggle the `OPEN_DLMS_BUILD_*` options:

```bash
cmake .. \
  -DOPEN_DLMS_BUILD_TESTS=ON \
  -DOPEN_DLMS_BUILD_SERVER=ON \
  -DOPEN_DLMS_BUILD_METER_SIM=ON \
  -DOPEN_DLMS_BUILD_CLIENT=OFF \
  -DOPEN_DLMS_BUILD_STUDIO=OFF \
  -DOPEN_DLMS_BUILD_DOCS=OFF
```

## Build outputs

| Target                     | Description                              |
|----------------------------|------------------------------------------|
| `cosemlib`                 | Core protocol library (static)           |
| `cosemserver`              | Server library (static)                  |
| `cosemtest`                | Unit test runner                         |
| `metersimulator`           | Example meter over TCP/IP                |
| `cosemclient` (opt-in)     | Client command-line utility              |

# How to edit & code

This project uses VSCode with pre-configured build tasks (see `.vscode/`).

Code should follow existing conventions:

- **C99** for library code, no dynamic allocation
- **C++** (with `extern "C"` wrappers) for tests and examples
- MIT license header on every file
- Buffer overrun protection via `csm_array` instead of raw pointer arithmetic

# Integration hints

## Project structure

```
opendlms/
├── cosemlib/         # Kernel protocol library (pure C99)
│   ├── src/          # Core: array, BER, AXR codec, association, channel, security, services
│   ├── crypto/       # AES-GCM, SHA-256/1, MD5, CMAC (derived from mbed TLS)
│   ├── hdlc/         # HDLC framing encoder/decoder
│   └── util/         # OS abstractions, bitfield, clock, endian helpers
├── server/           # Server-side COSEM object database
│   ├── application/  # Calendar, database glue
│   └── database/     # COSEM object implementations (associations, clock, image transfer)
├── client/           # Client command-line tool
├── examples/         # Example applications
│   ├── metersimulator/  # TCP/IP meter simulator
│   ├── cosemreader/     # HDLC-based meter reader with XML export
│   └── embeddedmeter/   # FreeRTOS port example
├── tests/            # Unit tests (Catch2 framework)
├── studio/           # Shaddam GUI (Qt, WIP)
└── docs/             # WASM meter simulator web UI (Preact + Tailwind)
```

## Adding a new COSEM object class

1. Add a database module in `server/database/` implementing the object interface
2. Register the object in `app_database.c`
3. Add test vectors in `tests/`

## HAL interface

The library calls into application-provided HAL functions declared in `csm_security.h` and `csm_channel.h`. Stub implementations for testing are in `tests/cosem_tests_hal.c`; a production implementation is in `examples/metersimulator/src/cosem_server_hal.c`.

Required HAL callbacks:

| Function                  | Purpose                                    |
|---------------------------|--------------------------------------------|
| `csm_sys_init`            | One-time initialisation                     |
| `csm_sys_get_system_title`| Return the 8-byte system title              |
| `csm_sys_get_key`         | Return a key by SAP and key ID              |
| `csm_sys_get_mechanism_id`| Return the authentication mechanism for SAP |
| `csm_hal_get_random_u8`   | Cryptographically secure RNG (uint8 range)  |
| `csm_hal_md5`             | MD5 hash                                    |
| `csm_hal_sha256`          | SHA-256 hash                                |
| `csm_hal_get_lls_password`| LLS password for a given SAP                |

# Development schedule

## Version 1.0

- [x] BER coder/decoder
- [x] Association control (AARQ/AARE/RLRQ/RLRE)
- [x] LLS authentication
- [x] HLS 5 GMAC authentication
- [x] Get (normal / by block)
- [x] Set (normal)
- [x] Action service
- [x] Exception response
- [x] HDLC framing
- [ ] LN with ciphering (Security Policy 0)
- [ ] HLS 3, 4, 6 authentication

## Future

- Multiple logical devices
- ACCESS service
- GBT (General Block Transfer)
- ECDSA key exchange
- ECDSA data transport ciphering

## License

MIT License — see [LICENSE](LICENSE).

Crypto code in `cosemlib/crypto/` is derived from [mbed TLS](https://tls.mbed.org) (Apache 2.0).
