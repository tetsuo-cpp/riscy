#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "MemoryReaders.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"

namespace riscy::riscv {

enum class TermKind {
  None,
  Fallthrough,  // implicit fallthrough to next block
  Branch,       // conditional branch with two successors
  Jump,         // direct jump
  IndirectJump, // JALR to non-RA; resolved at runtime via jump table
  Return,       // JALR x0, 0(ra)
  Trap          // ECALL/EBREAK or decode failure
};

struct BasicBlock {
  uint64_t start = 0;
  std::vector<DecodedInst> insts;
  TermKind term = TermKind::None;
  std::vector<uint64_t> succs; // 0,1, or 2 successors depending on term
};

struct CFG {
  uint64_t entry = 0;
  std::vector<BasicBlock> blocks;
  std::unordered_map<uint64_t, size_t> indexByAddr;
};

class CFGBuilder {
public:
  CFG build(const MemoryReader &mem, uint64_t entry) const;

private:
  static bool isCondBranch(Opcode op);
  static bool isJump(Opcode op);
  static bool isIndirect(const DecodedInst &inst);
  static bool isReturn(const DecodedInst &inst);
  static bool isTrap(Opcode op);
  static bool isTerminator(const DecodedInst &inst);
};

} // namespace riscy::riscv
