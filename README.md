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

- Decode linearly:
  `./build/riscy path/to/input.elf`
  Prints decoded instructions (address, raw word, formatted mnemonic/operands).

- Dump CFG (reachable blocks from entry):
  `./build/riscy --cfg path/to/input.elf`
  Prints basic blocks, instructions, terminators, and successors.

- Lift to IR (with disassembly):
  `./build/riscy --ir path/to/input.elf`
  Prints decoded instructions alongside their SSA IR representation.

- Translate to AArch64:
  `./build/riscy --aarch64 output.s path/to/input.elf`
  Generates AArch64 assembly code from RISC-V ELF binary.

## Layout
- `src/`: Core library (`ELFImage.*`, `MemoryReaders.h`, `RISCV/*` decoder/printer/CFG/lifter, `IR/*`, `AArch64/*` backend)
- `tools/`: CLI entry (`riscy.cpp`)
- `tests/`: Catch2 unit tests and `tests/e2e` (pytest + sample C programs)
- `third_party/`: `ELFIO`, `Catch2` (git submodules)

## Design
High‑level pipeline for RISC‑V to AArch64 translation:
- Decode RISC‑V instructions from ELF.
- Build CFG and lift to SSA IR.
- Select AArch64 instructions, allocate registers, and emit assembly.

This is a PoC; syscalls, PIC, and dynamic linking are out of scope.

## Execution Model

Riscy performs ahead-of-time translation of RISC-V binaries to native AArch64 assembly. The translation pipeline consists of several stages:

### Translation Pipeline
1. **ELF Loading**: Parse RISC-V ELF binary and extract executable sections
2. **Decoding**: Decode RISC-V instructions using a table-driven decoder
3. **CFG Construction**: Build control flow graph by analyzing branches and jumps
4. **IR Lifting**: Convert RISC-V instructions to SSA intermediate representation
5. **Instruction Selection**: Lower IR operations to AArch64 machine instructions
6. **Register Allocation**: Assign physical AArch64 registers using liveness analysis
7. **Code Emission**: Generate final AArch64 assembly with runtime integration

### Runtime System
The generated AArch64 assembly includes a lightweight runtime system that provides:

- **Guest State Management**: RISC-V architectural state (32 x-registers) stored in `RiscyGuestState` struct
- **Memory Management**: Single linear memory space with host-guest address translation
- **Block-based Execution**: Translated code organized into basic blocks with jump tables
- **Indirect Jump Handling**: Runtime dispatch for computed jumps via `riscy_indirect_jump()`
- **Tracing Support**: Optional execution tracing via `riscy_trace()` calls

### Generated Assembly Structure
The emitted assembly contains:
- `riscy_entry`: Main entry point that initializes execution at the original ELF entry address
- `riscy_block_*`: Translated basic blocks as AArch64 functions
- `riscy_block_addrs[]`: Array mapping block indices to original RISC-V PCs
- `riscy_block_ptrs[]`: Function pointer table for indirect jumps
- `riscy_entry_pc`: Original ELF entry point address

### Execution Flow
1. Runtime allocates guest memory and initializes RISC-V register state
2. Stack pointer (x2) and frame pointer (x8) set to top of allocated memory
3. Argument registers (a0-a7) populated from command line arguments
4. Execution starts at translated entry block corresponding to ELF entry point
5. Direct branches jump between translated blocks; indirect jumps use runtime dispatch
6. Program returns via standard AArch64 calling convention with result in a0 (x10)

The translator handles RV64I base instruction set including 32-bit operations (W-suffix instructions) and preserves original program semantics while executing natively on AArch64.

Notes:
- Decoder supports RV64I base ISA, including 32-bit ops (ADDIW/SLLIW/SRLIW/SRAIW, ADDW/SUBW/SLLW/SRLW/SRAW).
- E2E builds samples with base ISA flags (`-march=rv64i -mabi=lp64 -mno-relax`) and `-O0` to preserve control flow.

## Contributing
Follow LLVM coding standards; see `AGENTS.md` for project structure, commands, and testing guidelines.
