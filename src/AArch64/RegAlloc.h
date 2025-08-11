#pragma once

#include "AArch64/Instr.h"
#include "AArch64/Liveness.h"
#include <unordered_map>
#include <vector>

namespace riscy::aarch64 {

// Result of allocation within a block
struct RegAssignment {
  std::unordered_map<VReg, PReg> v2p;
};

class RegAlloc {
public:
  // Simple per-block linear-scan over caller-saved pool {x9..x15}; no spills
  // yet
  RegAssignment allocate(const Block &b, const LivenessMap &live) const;
};

} // namespace riscy::aarch64
