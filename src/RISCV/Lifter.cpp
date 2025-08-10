#include "RISCV/Lifter.h"

namespace riscy::riscv {

static inline int64_t getImm(const Operand &op) {
  return std::get<Imm>(op).value;
}
static inline uint8_t getReg(const Operand &op) {
  return std::get<Reg>(op).index;
}
static inline const Mem &getMem(const Operand &op) { return std::get<Mem>(op); }

static inline ir::ValueId nextId(std::vector<ir::Instr> &insts) {
  return static_cast<ir::ValueId>(insts.size());
}

static ir::Instr createConst(ir::Type ty, uint64_t v, ir::ValueId id) {
  ir::Instr I{};
  I.dest = id;
  I.payload = ir::Const{ty, v};
  return I;
}

static ir::Instr createReadReg(uint8_t r, ir::ValueId id) {
  ir::Instr I{};
  I.dest = id;
  I.payload = ir::ReadReg{r};
  return I;
}

static ir::Instr createWriteReg(uint8_t r, ir::ValueId v) {
  ir::Instr I{};
  I.payload = ir::WriteReg{r, v};
  return I;
}

static ir::Instr createBin(ir::BinOpKind k, ir::Type ty, ir::ValueId a,
                           ir::ValueId b, ir::ValueId id) {
  ir::Instr I{};
  I.dest = id;
  I.payload = ir::BinOp{k, a, b, ty};
  return I;
}

static ir::Instr createIcmp(ir::ICmpCond c, ir::ValueId a, ir::ValueId b,
                            ir::ValueId id) {
  ir::Instr I{};
  I.dest = id;
  I.payload = ir::ICmp{c, a, b};
  return I;
}

static ir::Instr createLoad(ir::Type ty, ir::ValueId base, int64_t off,
                            ir::ValueId id) {
  ir::Instr I{};
  I.dest = id;
  I.payload = ir::Load{base, off, ty};
  return I;
}

static ir::Instr createStore(ir::Type ty, ir::ValueId v, ir::ValueId base,
                             int64_t off) {
  ir::Instr I{};
  I.payload = ir::Store{v, base, off, ty};
  return I;
}

static ir::Instr createGetPC(ir::ValueId id) {
  ir::Instr I{};
  I.dest = id;
  I.payload = ir::GetPC{};
  return I;
}

ir::Block Lifter::lift(const BasicBlock &bbIn) const {
  ir::Block out{};
  out.start = bbIn.start;

  auto readReg = [&](uint8_t r) {
    ir::ValueId id = nextId(out.insts);
    out.insts.push_back(createReadReg(r, id));
    return id;
  };
  auto writeReg = [&](uint8_t r, ir::ValueId v) {
    if (!isX0(r))
      out.insts.push_back(createWriteReg(r, v));
  };
  auto imm = [&](ir::Type ty, uint64_t v) {
    ir::ValueId id = nextId(out.insts);
    out.insts.push_back(createConst(ty, v, id));
    return id;
  };
  auto bin = [&](ir::BinOpKind k, ir::Type ty, ir::ValueId a, ir::ValueId b) {
    ir::ValueId id = nextId(out.insts);
    out.insts.push_back(createBin(k, ty, a, b, id));
    return id;
  };
  auto icmp = [&](ir::ICmpCond c, ir::ValueId a, ir::ValueId b) {
    ir::ValueId id = nextId(out.insts);
    out.insts.push_back(createIcmp(c, a, b, id));
    return id;
  };
  auto load = [&](ir::Type ty, ir::ValueId base, int64_t off) {
    ir::ValueId id = nextId(out.insts);
    out.insts.push_back(createLoad(ty, base, off, id));
    return id;
  };
  auto store = [&](ir::Type ty, ir::ValueId v, ir::ValueId base, int64_t off) {
    out.insts.push_back(createStore(ty, v, base, off));
  };
  auto getpc = [&]() {
    ir::ValueId id = nextId(out.insts);
    out.insts.push_back(createGetPC(id));
    return id;
  };

  for (const auto &inst : bbIn.insts) {
    switch (inst.opcode) {
    case Opcode::ADDI: {
      auto rd = getReg(inst.operands[0]);
      auto rs1 = getReg(inst.operands[1]);
      auto immv = static_cast<uint64_t>(getImm(inst.operands[2]));
      auto v1 = readReg(rs1);
      auto c = imm(ir::Type::i64(), immv);
      auto sum = bin(ir::BinOpKind::Add, ir::Type::i64(), v1, c);
      writeReg(rd, sum);
      break;
    }
    case Opcode::ADD: {
      auto rd = getReg(inst.operands[0]);
      auto rs1 = getReg(inst.operands[1]);
      auto rs2 = getReg(inst.operands[2]);
      auto v1 = readReg(rs1);
      auto v2 = readReg(rs2);
      auto sum = bin(ir::BinOpKind::Add, ir::Type::i64(), v1, v2);
      writeReg(rd, sum);
      break;
    }
    case Opcode::SUB: {
      auto rd = getReg(inst.operands[0]);
      auto rs1 = getReg(inst.operands[1]);
      auto rs2 = getReg(inst.operands[2]);
      auto v1 = readReg(rs1);
      auto v2 = readReg(rs2);
      auto diff = bin(ir::BinOpKind::Sub, ir::Type::i64(), v1, v2);
      writeReg(rd, diff);
      break;
    }
    case Opcode::AND: {
      auto rd = getReg(inst.operands[0]);
      auto rs1 = getReg(inst.operands[1]);
      auto rs2 = getReg(inst.operands[2]);
      auto v1 = readReg(rs1);
      auto v2 = readReg(rs2);
      auto r = bin(ir::BinOpKind::And, ir::Type::i64(), v1, v2);
      writeReg(rd, r);
      break;
    }
    case Opcode::OR: {
      auto rd = getReg(inst.operands[0]);
      auto rs1 = getReg(inst.operands[1]);
      auto rs2 = getReg(inst.operands[2]);
      auto v1 = readReg(rs1);
      auto v2 = readReg(rs2);
      auto r = bin(ir::BinOpKind::Or, ir::Type::i64(), v1, v2);
      writeReg(rd, r);
      break;
    }
    case Opcode::XOR: {
      auto rd = getReg(inst.operands[0]);
      auto rs1 = getReg(inst.operands[1]);
      auto rs2 = getReg(inst.operands[2]);
      auto v1 = readReg(rs1);
      auto v2 = readReg(rs2);
      auto r = bin(ir::BinOpKind::Xor, ir::Type::i64(), v1, v2);
      writeReg(rd, r);
      break;
    }
    case Opcode::LW: {
      auto rd = getReg(inst.operands[0]);
      const auto &m = getMem(inst.operands[1]);
      auto base = readReg(m.base);
      auto v32 = load(ir::Type::i32(), base, m.offset);
      // sign-extend to i64 in RV64 for LW
      ir::Instr I{};
      ir::ValueId id = nextId(out.insts);
      I.dest = id;
      I.payload = ir::SExt{v32, ir::Type::i64()};
      out.insts.push_back(std::move(I));
      writeReg(rd, id);
      break;
    }
    case Opcode::LWU: {
      auto rd = getReg(inst.operands[0]);
      const auto &m = getMem(inst.operands[1]);
      auto base = readReg(m.base);
      auto v32 = load(ir::Type::i32(), base, m.offset);
      ir::Instr I{};
      ir::ValueId id = nextId(out.insts);
      I.dest = id;
      I.payload = ir::ZExt{v32, ir::Type::i64()};
      out.insts.push_back(std::move(I));
      writeReg(rd, id);
      break;
    }
    case Opcode::LD: {
      auto rd = getReg(inst.operands[0]);
      const auto &m = getMem(inst.operands[1]);
      auto base = readReg(m.base);
      auto v = load(ir::Type::i64(), base, m.offset);
      writeReg(rd, v);
      break;
    }
    case Opcode::SW: {
      const auto &m = getMem(inst.operands[0]); // Mem first for stores
      auto rs = getReg(inst.operands[1]);
      auto base = readReg(m.base);
      auto val = readReg(rs);
      store(ir::Type::i32(), val, base, m.offset);
      break;
    }
    case Opcode::SD: {
      const auto &m = getMem(inst.operands[0]);
      auto rs = getReg(inst.operands[1]);
      auto base = readReg(m.base);
      auto val = readReg(rs);
      store(ir::Type::i64(), val, base, m.offset);
      break;
    }
    case Opcode::BEQ:
    case Opcode::BNE:
    case Opcode::BLT:
    case Opcode::BGE:
    case Opcode::BLTU:
    case Opcode::BGEU: {
      // Handled after loop as terminator, but compute compare now.
      // We still build the compare value to reuse as terminator condition.
      auto rs1 = getReg(inst.operands[0]);
      auto rs2 = getReg(inst.operands[1]);
      auto v1 = readReg(rs1);
      auto v2 = readReg(rs2);
      ir::ICmpCond c{};
      switch (inst.opcode) {
      case Opcode::BEQ:
        c = ir::ICmpCond::EQ;
        break;
      case Opcode::BNE:
        c = ir::ICmpCond::NE;
        break;
      case Opcode::BLT:
        c = ir::ICmpCond::SLT;
        break;
      case Opcode::BGE:
        c = ir::ICmpCond::SGE;
        break;
      case Opcode::BLTU:
        c = ir::ICmpCond::ULT;
        break;
      case Opcode::BGEU:
        c = ir::ICmpCond::UGE;
        break;
      default:
        c = ir::ICmpCond::EQ;
      }
      // Save the compare as last value for use by terminator.
      (void)icmp(c, v1, v2);
      break;
    }
    case Opcode::AUIPC: {
      auto rd = getReg(inst.operands[0]);
      auto immv = static_cast<uint64_t>(getImm(inst.operands[1]));
      auto pcv = getpc();
      auto c = imm(ir::Type::i64(), immv);
      auto sum = bin(ir::BinOpKind::Add, ir::Type::i64(), pcv, c);
      writeReg(rd, sum);
      break;
    }
    case Opcode::JAL: {
      auto rd = getReg(inst.operands[0]);
      auto ra = imm(ir::Type::i64(), inst.pc + 4);
      writeReg(rd, ra);
      // terminator handled after loop
      break;
    }
    case Opcode::JALR: {
      auto rd = getReg(inst.operands[0]);
      const auto &m = getMem(inst.operands[1]);
      auto base = readReg(m.base);
      auto off = imm(ir::Type::i64(), static_cast<uint64_t>(m.offset));
      auto tgt = bin(ir::BinOpKind::Add, ir::Type::i64(), base, off);
      // clear LSB: target &= ~1
      auto ones = imm(ir::Type::i64(), ~1ull);
      auto tgtMasked = bin(ir::BinOpKind::And, ir::Type::i64(), tgt, ones);
      // write return address if rd != x0
      auto ra = imm(ir::Type::i64(), inst.pc + 4);
      writeReg(rd, ra);
      // Save target id as last value for terminator usage
      (void)tgtMasked;
      break;
    }
    case Opcode::ECALL:
    case Opcode::EBREAK:
      // no-op here; terminator set later
      break;
    default:
      // For now, ignore unimplemented ops in the skeleton.
      break;
    }
  }

  // Terminator from bbIn.term and last instruction context
  switch (bbIn.term) {
  case TermKind::Branch: {
    // Find the last ICmp value (it should be the last value-producing instr)
    ir::ValueId cond = 0;
    for (int i = static_cast<int>(out.insts.size()) - 1; i >= 0; --i) {
      if (out.insts[i].dest &&
          std::holds_alternative<ir::ICmp>(out.insts[i].payload)) {
        cond = *out.insts[i].dest;
        break;
      }
    }
    ir::TermCBr t{};
    t.cond = cond;
    t.t = bbIn.succs.size() > 0 ? bbIn.succs[0] : 0;
    t.f = bbIn.succs.size() > 1 ? bbIn.succs[1] : 0;
    out.term.kind = ir::TermKind::CBr;
    out.term.data = t;
    break;
  }
  case TermKind::Jump: {
    ir::TermBr t{};
    t.target = bbIn.succs.empty() ? 0 : bbIn.succs[0];
    out.term.kind = ir::TermKind::Br;
    out.term.data = t;
    break;
  }
  case TermKind::IndirectJump: {
    // Find the last produced value (the masked target from JALR lowering)
    ir::ValueId target = 0;
    if (!out.insts.empty() && out.insts.back().dest)
      target = *out.insts.back().dest;
    ir::TermBrIndirect t{target};
    out.term.kind = ir::TermKind::BrIndirect;
    out.term.data = t;
    break;
  }
  case TermKind::Return: {
    out.term.kind = ir::TermKind::Ret;
    break;
  }
  case TermKind::Trap: {
    out.term.kind = ir::TermKind::Trap;
    break;
  }
  case TermKind::Fallthrough:
  case TermKind::None: {
    // Treat as unconditional branch to successor if present.
    ir::TermBr t{};
    t.target = bbIn.succs.empty() ? 0 : bbIn.succs[0];
    out.term.kind = ir::TermKind::Br;
    out.term.data = t;
    break;
  }
  }

  return out;
}

} // namespace riscy::riscv
