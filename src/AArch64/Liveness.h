#pragma once

#include "AArch64/Instr.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace riscy::aarch64 {

struct LiveRange {
  uint32_t start = 0;
  uint32_t end = 0;
};

// Map VReg -> live interval within a block
using LivenessMap = std::unordered_map<VReg, LiveRange>;

class Liveness {
public:
  LivenessMap analyze(const Block &b) const;
};

} // namespace riscy::aarch64
