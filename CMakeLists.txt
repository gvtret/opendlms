cmake_minimum_required(VERSION 3.10)
project(opendlms VERSION 1.1.0 LANGUAGES C CXX)

# ── Options ──────────────────────────────────────────────────────────────────
option(OPEN_DLMS_BUILD_TESTS      "Build unit tests"             ON)
option(OPEN_DLMS_BUILD_SERVER     "Build server library"         ON)
option(OPEN_DLMS_BUILD_METER_SIM  "Build meter simulator exe"   OFF)
option(OPEN_DLMS_BUILD_CLIENT     "Build client reader API library" OFF)
option(OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT "Build legacy JSON client tool (needs Gurux JSON/Common headers)" OFF)
option(OPEN_DLMS_BUILD_READER_LAB "Build reader_lab TCP client" OFF)
option(OPEN_DLMS_BUILD_STUDIO     "Build legacy Qt Studio if the studio/ directory is present" OFF)
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

if(OPEN_DLMS_BUILD_TESTS AND OPEN_DLMS_BUILD_METER_SIM AND OPEN_DLMS_BUILD_READER_LAB)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME reader_lab_live_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/live_reader_lab_smoke.py
                    $<TARGET_FILE:metersimulator>
                    $<TARGET_FILE:reader_lab>
        )
    endif()
endif()

# ── Legacy Qt Studio (removed from the default production surface) ───────────
if(OPEN_DLMS_BUILD_STUDIO)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/studio/CMakeLists.txt")
        add_subdirectory(studio)
    else()
        message(FATAL_ERROR
            "OPEN_DLMS_BUILD_STUDIO requires the legacy studio/ directory, "
            "which is not present in this checkout. Use electron-studio via npm "
            "or disable OPEN_DLMS_BUILD_STUDIO.")
    endif()
endif()

# ── WASM docs / libmetersim (requires Emscripten) ────────────────────────────
if(OPEN_DLMS_BUILD_DOCS)
    add_subdirectory(docs)
endif()
