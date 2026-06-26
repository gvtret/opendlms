cmake_minimum_required(VERSION 3.10)
project(opendlms VERSION 1.1.0 LANGUAGES C CXX)

# ── Options ──────────────────────────────────────────────────────────────────
option(OPEN_DLMS_BUILD_TESTS      "Build unit tests"             ON)
option(OPEN_DLMS_BUILD_SERVER     "Build server library"         ON)
option(OPEN_DLMS_BUILD_METER_SIM  "Build meter simulator exe"   OFF)
option(OPEN_DLMS_BUILD_CLIENT     "Build client tool (needs Gurux)" OFF)
option(OPEN_DLMS_BUILD_READER_LAB "Build reader_lab TCP client" OFF)
option(OPEN_DLMS_BUILD_STUDIO     "Build Shaddam Studio (needs Qt)" OFF)
option(OPEN_DLMS_BUILD_DOCS       "Build WASM doc targets (needs Emscripten)" OFF)
option(COSEMLIB_SHARED             "Build cosemlib as shared library" OFF)

# ── Core library ─────────────────────────────────────────────────────────────
add_subdirectory(cosemlib)

# ── Server library ───────────────────────────────────────────────────────────
if(OPEN_DLMS_BUILD_SERVER)
    add_subdirectory(server)
endif()

# ── Unit tests ───────────────────────────────────────────────────────────────
if(OPEN_DLMS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Meter simulator example ──────────────────────────────────────────────────
if(OPEN_DLMS_BUILD_METER_SIM)
    add_subdirectory(examples/metersimulator)
endif()

# ── Client tool (requires external JSON libs, currently opt-in) ──────────────
if(OPEN_DLMS_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(OPEN_DLMS_BUILD_READER_LAB)
    add_subdirectory(examples/reader_lab)
endif()

# ── Shaddam Studio (requires Qt) ─────────────────────────────────────────────
if(OPEN_DLMS_BUILD_STUDIO)
    add_subdirectory(studio)
endif()

# ── WASM docs / libmetersim (requires Emscripten) ────────────────────────────
if(OPEN_DLMS_BUILD_DOCS)
    add_subdirectory(docs)
endif()
