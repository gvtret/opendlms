# OpenDLMS

Open-source DLMS/COSEM protocol stack (IEC 62056) in pure C99, MIT-licensed.

**Version 1.1.0** — DLMS/COSEM core, reader/server APIs, simulator examples, and Electron Studio tooling for smart meters, head-end tools, and gateways.

## Features

- **Pure C99** — portable, allocation-free core protocol logic, zero-copy buffers
- **DLMS/COSEM core** — BER/AXDR codec, associations, GET/SET/ACTION, block transfer
- **Security** — LLS, HLS/GMAC (complete 4-pass handshake), AES-GCM, Kuznyechik, CMAC
- **Context-based APIs** — isolated client/server contexts for thread safety
- **COSEM IC model** — 33 registered interface classes (IEC + SPODUS)
- **Block Transfer (GBT)** — GET and SET with automatic block splitting/assembly
- **Transport** — TCP/IP, HDLC framing, COSEM-TCP wrapper
- **215+ tests** — Catch2, CTest smoke tests, and live reader/simulator coverage

## Quick Start

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure
```

For the full local production gate, including reader API, simulator, and live
reader_lab smoke, use:

```bash
cmake --preset production
cmake --build --preset production
ctest --test-dir build-production --output-on-failure
```

### Install

```bash
cmake --install . --prefix /usr/local
```

### Use in your project

**CMake:**
```cmake
find_package(OpenDLMS REQUIRED)
target_link_libraries(myapp OpenDLMS::cosemlib)
```

When `OPEN_DLMS_BUILD_CLIENT=ON` is enabled and installed, the reader
API is exported as `OpenDLMS::opendlms_reader`.

**pkg-config:**
```bash
gcc -o myapp myapp.c $(pkg-config --cflags --libs opendlms)
```

**Single header:**
```c
#include "cosemlib.h"
```

## API Overview

### Server

```c
#include "cosemlib.h"

csm_server *server = csm_server_create(&transport, 0, CSM_FRAMING_WRAPPER);
csm_server_register_db(server, my_db_handler);

while (running) {
    csm_server_poll(server, 100);
}

csm_server_destroy(server);
free(server);
```

### Client

```c
#include "cosemlib.h"

csm_client *client = csm_client_create(&transport, 0, CSM_FRAMING_WRAPPER);
csm_client_connect(client, 5000);

/* GET with automatic block transfer */
uint8_t resp[2048];
int len = csm_client_get_block(client, 1, 3, &obis, 2, resp, sizeof(resp));

/* SET with automatic block transfer */
csm_client_set_block(client, 2, 3, &obis, 2, data, data_len, resp, sizeof(resp));

csm_client_destroy(client);
free(client);
```

### Block Transfer

```c
/* Server-side: data too large for single PDU */
csm_block_state block;
csm_block_start_server(&block, invoke_id, data, data_size, 0);
while (csm_block_is_active(&block)) {
    csm_array arr;
    csm_block_encode_next(&block, &arr, max_pdu_size);
    send(&arr);
}

/* Client-side: receive blocks */
csm_block_state rx;
csm_block_start_get_receive(&rx, invoke_id, 0);
while (csm_block_is_active(&rx)) {
    csm_block_get_receive_data(&rx, block_data, block_size, is_last);
}
const uint8_t *complete;
uint32_t complete_size;
csm_block_get_received_data(&rx, &complete, &complete_size);
```

## Directory Structure

```
opendlms/
├── cosemlib/          # Core protocol library (C99)
│   ├── src/           # Headers and source files
│   ├── crypto/        # AES, SHA, Kuznyechik (Apache 2.0)
│   ├── hdlc/          # HDLC framing
│   ├── ic/            # COSEM IC class implementations
│   ├── model/         # YAML catalog parser
│   ├── profile/       # SPODES/SPODUS capture templates
│   └── util/          # OS abstractions, clock, bitfield
├── server/            # Server-side COSEM objects
├── client/            # DLMS client library and reader API
├── common/            # Shared transport: TCP client/server, serial port
├── examples/          # Example applications
│   ├── metersimulator/
│   ├── reader_lab/
│   └── embeddedmeter/
├── tests/             # Unit tests (Catch2)
└── electron-studio/   # Electron Studio native bindings and Lua bridge
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `OPEN_DLMS_BUILD_TESTS` | ON | Build unit tests |
| `OPEN_DLMS_BUILD_SERVER` | ON | Build server library |
| `OPEN_DLMS_BUILD_METER_SIM` | OFF | Build meter simulator |
| `OPEN_DLMS_BUILD_CLIENT` | OFF | Build `opendlms_reader` C API |
| `OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT` | OFF | Build legacy JSON CLI; requires external Gurux headers |
| `OPEN_DLMS_BUILD_READER_LAB` | OFF | Build TCP-wrapper reader example |
| `OPEN_DLMS_BUILD_STUDIO` | OFF | Build Qt GUI (deprecated, use electron-studio) |
| `OPEN_DLMS_BUILD_DOCS` | OFF | Build WASM doc targets (needs Emscripten) |
| `COSEMLIB_SHARED` | OFF | Build shared library |

## HAL Interface

The library requires application-provided HAL functions:

| Function | Purpose |
|----------|---------|
| `csm_hal_get_random_u8(min, max)` | Cryptographic RNG |
| `csm_hal_get_lls_password(sap, buf, size)` | LLS password lookup |
| `csm_hal_sha256(data, len, out)` | SHA-256 hash |
| `csm_hal_sha1(data, len, out)` | SHA-1 hash |
| `csm_hal_md5(data, len, out)` | MD5 hash |
| `csm_hal_decode_selective_access(req, array)` | Selective access decoder |
| `csm_sys_get_system_title()` | 8-byte system title |
| `csm_sys_set_system_title(buf)` | Set system title |
| `csm_sys_get_key(sap, key_id)` | Key by SAP and ID (HAL must zeroize after use) |
| `csm_sys_get_mechanism_id(sap)` | Mechanism ID by SAP |
| `csm_sys_gcm_init(ch, sap, key, mode, iv, aad, aad_len)` | GCM init |
| `csm_sys_gcm_update(ch, plain, len, crypt)` | GCM update |
| `csm_sys_gcm_finish(ch, tag)` | GCM finish + tag |

## License

MIT License — see [LICENSE](LICENSE).

Crypto code in `cosemlib/crypto/` is derived from [mbed TLS](https://tls.mbed.org) (Apache 2.0).
See [THIRD-PARTY-LICENSES.txt](THIRD-PARTY-LICENSES.txt) for details.
