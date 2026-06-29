# Test layout

The production test gate is defined in `tests/CMakeLists.txt` and runs through
CTest as `cosemtest`, plus `reader_hal_smoke` when the reader API is enabled.

Files named `test_*.cpp`, `cosem_tests_hal.c`, and `reader_hal_smoke.c` are the
maintained gate tests unless a Catch2 tag explicitly disables a case, such as
`[.todo]`.

The `streebog_*.py` and standalone `streebog_*.c` files are research/debug
probes kept for the historical Streebog implementation work. They are not
compiled by CMake and are not part of the production gate. Maintained Streebog
coverage lives in `test_gost.cpp` and `test_streebog_debug.cpp`.
