# Meter simulator manual tests

This directory is not part of the production CTest gate.

- `client.py` is a manual interoperability probe for a running simulator and
  the external Python `dlms_cosem` package. The production gate uses
  `tests/live_reader_lab_smoke.py` instead because it builds the in-tree
  simulator and reader example, chooses an isolated TCP port, and tears the
  simulator down deterministically.
- `test_fs.cpp` is a legacy embUnit test for the simulator filesystem helper.
  That filesystem layer is not part of the active DLMS/COSEM client/server
  stack gate. Restore it under a dedicated legacy-filesystem CMake option if
  the filesystem subsystem becomes a supported surface again.
