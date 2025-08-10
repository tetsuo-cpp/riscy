#include "RISCV/CFG.h"

#include <queue>

namespace riscy::riscv {

static inline int64_t getImm(const Operand &op) {
  return std::get<Imm>(op).value;
}

bool CFGBuilder::isCondBranch(Opcode op) {
  switch (op) {
  case Opcode::BEQ:
  case Opcode::BNE:
  case Opcode::BLT:
  case Opcode::BGE:
  case Opcode::BLTU:
  case Opcode::BGEU:
    return true;
  default:
    return false;
  }
}

bool CFGBuilder::isJump(Opcode op) { return op == Opcode::JAL; }

bool CFGBuilder::isIndirect(const DecodedInst &inst) {
  return inst.opcode == Opcode::JALR && !isReturn(inst);
}

bool CFGBuilder::isReturn(const DecodedInst &inst) {
  if (inst.opcode != Opcode::JALR || inst.operands.size() < 2)
    return false;
  // JALR rd, offset(rs1)
  // Return is encoded as JALR x0, 0(ra)
  const auto &rd = std::get<Reg>(inst.operands[0]);
  if (!std::holds_alternative<Mem>(inst.operands[1]))
    return false;
  const auto &m = std::get<Mem>(inst.operands[1]);
  return rd.index == 0 && m.base == 1 /*ra*/ && m.offset == 0;
}

bool CFGBuilder::isTrap(Opcode op) {
  return op == Opcode::ECALL || op == Opcode::EBREAK;
}

bool CFGBuilder::isTerminator(const DecodedInst &inst) {
  return isCondBranch(inst.opcode) || isJump(inst.opcode) || isIndirect(inst) ||
         isReturn(inst) || isTrap(inst.opcode);
}

CFG CFGBuilder::build(const MemoryReader &mem, uint64_t entry) const {
  CFG cfg{};
  cfg.entry = entry;
  Decoder dec;

  std::queue<uint64_t> worklist;
  std::unordered_set<uint64_t> leaders;
  worklist.push(entry);
  leaders.insert(entry);

  auto enqueue = [&](uint64_t addr) {
    if (leaders.insert(addr).second) {
      worklist.push(addr);
    }
  };

  while (!worklist.empty()) {
    uint64_t start = worklist.front();
    worklist.pop();
    if (cfg.indexByAddr.count(start))
      continue; // already built

    BasicBlock bb{};
    bb.start = start;

    for (uint64_t pc = start;; pc += 4) {
      if (pc != start && (leaders.count(pc) || cfg.indexByAddr.count(pc))) {
        bb.term = TermKind::Fallthrough;
        bb.succs.push_back(pc);
        enqueue(pc);
        break;
      }

      DecodedInst inst{};
      DecodeError err{};
      if (!dec.decodeNext(mem, pc, inst, err)) {
        // Stop block on decode error / OOB
        bb.term = TermKind::Trap;
        break;
      }
      bb.insts.push_back(inst);

      if (isCondBranch(inst.opcode)) {
        // operands: rs1, rs2, imm
        int64_t off = getImm(inst.operands[2]);
        uint64_t t = inst.pc + static_cast<uint64_t>(off);
        uint64_t f = inst.pc + 4;
        bb.term = TermKind::Branch;
        bb.succs = {t, f};
        enqueue(t);
        enqueue(f);
        break;
      }
      if (isJump(inst.opcode)) {
        // operands: rd, imm
        int64_t off = getImm(inst.operands[1]);
        uint64_t t = inst.pc + static_cast<uint64_t>(off);
        bb.term = TermKind::Jump;
        bb.succs = {t};
        enqueue(t);
        break;
      }
      if (isIndirect(inst)) {
        bb.term = TermKind::IndirectJump;
        break;
      }
      if (isReturn(inst)) {
        bb.term = TermKind::Return;
        break;
      }
      if (isTrap(inst.opcode)) {
        bb.term = TermKind::Trap;
        break;
      }
    }

    cfg.indexByAddr[bb.start] = cfg.blocks.size();
    cfg.blocks.push_back(std::move(bb));
  }

  return cfg;
}

} // namespace riscy::riscv
