# Repository Guidelines

## Project Structure & Modules
- `src/`: C++17 library code (`riscy_lib`). Key areas: `RISCV/` (decoder, printer, CFG), `ELFImage.*`, `MemoryReaders.h`.
- `tools/`: CLI entrypoint (`riscy.cpp`) building the `riscy` binary.
- `tests/`: Unit tests (`DecoderTests.cpp`, Catch2) and e2e tests under `tests/e2e/` (pytest + sample C files).
- `third_party/`: Git submodules (`ELFIO`, `Catch2`). Run `git submodule update --init --recursive` after cloning.
- `build/`: Recommended out-of-source CMake build directory (also default for `compile_commands.json`).

## Build, Test, and Run
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`
- Build all: `cmake --build build` (produces `build/riscy`)
- Format: `cmake --build build --target format` (uses `.clang-format`)
- Tests (unit + e2e): `ctest --test-dir build -V` (CTest drives Catch2 and the e2e pytest). To run only e2e: `ctest --test-dir build -V -R e2e_decode`. E2E requires `clang` with RISC‑V target and `lld`.
  - Tip: on macOS/Homebrew LLVM, `export PATH="/opt/homebrew/opt/llvm/bin:$PATH"` and activate the repo venv `source .ve/bin/activate`.
- Run locally: `./build/riscy path/to/input.elf`
  - Dump CFG instead: `./build/riscy --cfg path/to/input.elf`

## Coding Style & Naming
- Standard: LLVM style; `.clang-format` enforces it. Use 2-space indents, no tabs.
- Language: C++17, no extensions.
- Naming: Types/structs in PascalCase (e.g., `ELFImage`, `Decoder`); functions/methods in camelCase (`decodeNext`, `formatInst`); namespaces are lowercase (`riscy::riscv`).
- Headers: Prefer `.h` for headers and `.cpp` for impl; keep public headers in `src/` and include via project-relative paths (`#include "RISCV/Decoder.h"`).

## Testing Guidelines
- Frameworks: Catch2 for unit tests; e2e is executed via CTest which invokes pytest.
- Run all tests through CTest; filter with `-R e2e_decode` for e2e-only.
- Add unit tests near feature area (e.g., new decode paths in `tests/DecoderTests.cpp`).
- CFG unit tests live in `tests/CFGTests.cpp`; common encoding helpers in `tests/TestUtils.h`.
- E2E samples live in `tests/e2e/samples/*.c`; add small, freestanding programs without libc.
- Naming: Use descriptive `TEST_CASE` names and `[decoder]` tags; for pytest modules, keep files as `test_*.py`.

## Tips & Configuration
- Warnings: `-DRISCY_WARNINGS=ON` adds `-Wall -Wextra -Wpedantic`.
- E2E toolchain probe happens automatically; failures will skip those tests locally.
- E2E builds use base ISA flags `-march=rv64i -mabi=lp64 -mno-relax` and `-O0` to preserve control flow; output dumps CFG for visibility.
