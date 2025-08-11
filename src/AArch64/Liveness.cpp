#include "AArch64/Liveness.h"

namespace riscy::aarch64 {

LivenessMap Liveness::analyze(const Block &b) const {
  LivenessMap map;
  auto touch = [&](VReg v, uint32_t pos) {
    auto &lr = map[v];
    if (lr.start == 0 && lr.end == 0) {
      lr.start = pos;
      lr.end = pos;
    }
    if (pos < lr.start)
      lr.start = pos;
    if (pos > lr.end)
      lr.end = pos;
  };

  uint32_t pos = 0;
  for (const auto &I : b.instrs) {
    for (const auto &op : I.ops) {
      if (std::holds_alternative<OpRegV>(op)) {
        touch(std::get<OpRegV>(op).id, pos);
      } else if (std::holds_alternative<OpMem>(op)) {
        auto base = std::get<OpMem>(op).base.id;
        // Base.id == 0 denotes x0 (state pointer), not a vreg; skip it.
        if (base != 0)
          touch(base, pos);
      }
    }
    ++pos;
  }
  // Terminator
  switch (b.term.kind) {
  case TermKind::CBr: {
    auto t = std::get<TermCBr>(b.term.data);
    touch(t.cond, pos);
    break;
  }
  case TermKind::BrIndirect: {
    auto t = std::get<TermBrIndirect>(b.term.data);
    touch(t.target, pos);
    break;
  }
  default:
    break;
  }
  return map;
}

} // namespace riscy::aarch64
