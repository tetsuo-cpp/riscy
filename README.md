# Riscy

Riscy is a proof‑of‑concept RV64I (RISC‑V 64) to AArch64 (ARM 64) translator.

## Dependencies
- Build: CMake ≥ 3.16, C++17 compiler (Clang/GCC), Git (for submodules).
- Third‑party: ELFIO and Catch2 via `third_party/` git submodules.
- Tests: Python 3 with `pytest`; E2E requires `clang` with `riscv64-unknown-elf` target and `lld`.
- Tooling (optional): `clang-format` (LLVM style) for `format` target.

Initialize submodules:
`git submodule update --init --recursive`

## Quickstart
1) Configure + build
`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`
`cmake --build build -j`

2) Run unit tests (Catch2 via CTest)
`ctest --test-dir build -V`

3) Run tests (unit + e2e)
`ctest --test-dir build -V`
Run only the e2e decode test:
`ctest --test-dir build -V -R e2e_decode`

4) Format sources
`cmake --build build --target format`

## Usage
Build produces `build/riscy`:
`./build/riscy path/to/input.elf`
It prints decoded instructions (address, raw word, formatted mnemonic/operands).

## Layout
- `src/`: Core library (`ELFImage.*`, `MemoryReaders.h`, `RISCV/*` decoder/printer)
- `tools/`: CLI entry (`riscy.cpp`)
- `tests/`: Catch2 unit tests and `tests/e2e` (pytest + sample C programs)
- `third_party/`: `ELFIO`, `Catch2` (git submodules)

## Design
High‑level pipeline (targeted for future translation):
- Decode RISC‑V instructions from ELF.
- Build CFG and lift to SSA IR.
- Optimize, select AArch64 instructions, allocate registers.
- Emit AArch64 assembly.

This is a PoC; syscalls, PIC, and dynamic linking are out of scope.

## Contributing
Follow LLVM coding standards; see `AGENTS.md` for project structure, commands, and testing guidelines.
