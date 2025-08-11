#pragma once

#include "AArch64/Instr.h"
#include "IR/IR.h"
#include <cstdint>
#include <string>
#include <vector>

namespace riscy::aarch64 {

// Instruction selector: IR.Block -> AArch64 block with virtual registers.
class ISel {
public:
  // Select a single block. Assumes x0 holds `struct RiscyGuestState*`.
  Block select(const ir::Block &bb) const;
};

} // namespace riscy::aarch64
