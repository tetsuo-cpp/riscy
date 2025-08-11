#pragma once

#include "AArch64/Instr.h"
#include "AArch64/RegAlloc.h"
#include <string>
#include <vector>

namespace riscy::aarch64 {

struct ModuleAsm {
  std::string text; // final .s
};

class Emitter {
public:
  // Emit a full translation unit with blocks and their assignments (same order)
  ModuleAsm emit(const std::vector<Block> &blocks,
                 const std::vector<RegAssignment> &assignments,
                 uint64_t entry_pc) const;
};

} // namespace riscy::aarch64
