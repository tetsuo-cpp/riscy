#pragma once

#include "IR/IR.h"
#include "RISCV/CFG.h"

namespace riscy::riscv {

// Block-local lifter: converts a RISC-V BasicBlock into a minimal IR block.
class Lifter {
public:
  ir::Block lift(const BasicBlock &bb) const;

private:
  static inline bool isX0(uint8_t reg) { return reg == 0; }
};

} // namespace riscy::riscv
