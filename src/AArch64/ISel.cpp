#include "AArch64/ISel.h"
#include <sstream>

namespace riscy::aarch64 {

static inline int guest_reg_offset_bytes(uint8_t r) {
  return static_cast<int>(r) * 8;
}

static inline Instr make1(Op op, Operand a) {
  return Instr{op, {std::move(a)}};
}
static inline Instr make2(Op op, Operand a, Operand b) {
  return Instr{op, {std::move(a), std::move(b)}};
}
static inline Instr make3(Op op, Operand a, Operand b, Operand c) {
  return Instr{op, {std::move(a), std::move(b), std::move(c)}};
}

Block ISel::select(const ir::Block &bb) const {
  Block out{};
  out.guest_pc = bb.start;

  // Map IR ValueId -> virtual reg
  std::vector<VReg> vmap(bb.insts.size() + 1, 0);
  auto vreg_of = [&](ir::ValueId id) {
    if (id >= vmap.size())
      vmap.resize(id + 1, 0);
    if (!vmap[id])
      vmap[id] = static_cast<VReg>(id + 1);
    return vmap[id];
  };

  for (const auto &I : bb.insts) {
    if (std::holds_alternative<ir::Const>(I.payload)) {
      if (I.dest) {
        auto v = vreg_of(*I.dest);
        auto &C = std::get<ir::Const>(I.payload);
        // Use Mov/MovZ/MovK sequence; start with MovZ low 16 bits, then MovK
        uint64_t val = C.value;
        if ((val >> 16) == 0) {
          out.instrs.push_back(make2(Op::Mov, OpRegV{v}, OpImm{val}));
        } else {
          out.instrs.push_back(make2(Op::MovZ, OpRegV{v}, OpImm{val & 0xffff}));
          uint16_t hi1 = (val >> 16) & 0xffffu;
          uint16_t hi2 = (val >> 32) & 0xffffu;
          uint16_t hi3 = (val >> 48) & 0xffffu;
          if (hi1)
            out.instrs.push_back(
                make3(Op::MovK, OpRegV{v}, OpImm{hi1}, OpImm{16}));
          if (hi2)
            out.instrs.push_back(
                make3(Op::MovK, OpRegV{v}, OpImm{hi2}, OpImm{32}));
          if (hi3)
            out.instrs.push_back(
                make3(Op::MovK, OpRegV{v}, OpImm{hi3}, OpImm{48}));
        }
      }
    } else if (std::holds_alternative<ir::ReadReg>(I.payload)) {
      if (I.dest) {
        auto v = vreg_of(*I.dest);
        auto r = std::get<ir::ReadReg>(I.payload).reg;
        out.instrs.push_back(
            make2(Op::LdrX, OpRegV{v},
                  OpMem{OpRegV{0}, guest_reg_offset_bytes(
                                       r)})); // base vreg 0 denotes x0 (state)
      }
    } else if (std::holds_alternative<ir::WriteReg>(I.payload)) {
      auto W = std::get<ir::WriteReg>(I.payload);
      out.instrs.push_back(
          make2(Op::StrX, OpRegV{vreg_of(W.value)},
                OpMem{OpRegV{0}, guest_reg_offset_bytes(W.reg)}));
    } else if (std::holds_alternative<ir::BinOp>(I.payload)) {
      auto &B = std::get<ir::BinOp>(I.payload);
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        switch (B.kind) {
        case ir::BinOpKind::Add:
          out.instrs.push_back(make3(Op::Add, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::Sub:
          out.instrs.push_back(make3(Op::Sub, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::And:
          out.instrs.push_back(make3(Op::And, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::Or:
          out.instrs.push_back(make3(Op::Orr, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::Xor:
          out.instrs.push_back(make3(Op::Eor, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::Shl:
          out.instrs.push_back(make3(Op::Lsl, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::LShr:
          out.instrs.push_back(make3(Op::Lsr, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        case ir::BinOpKind::AShr:
          out.instrs.push_back(make3(Op::Asr, OpRegV{vd},
                                     OpRegV{vreg_of(B.lhs)},
                                     OpRegV{vreg_of(B.rhs)}));
          break;
        }
      }
    } else if (std::holds_alternative<ir::ICmp>(I.payload)) {
      auto &C = std::get<ir::ICmp>(I.payload);
      out.instrs.push_back(
          make2(Op::Cmp, OpRegV{vreg_of(C.lhs)}, OpRegV{vreg_of(C.rhs)}));
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        switch (C.cond) {
        case ir::ICmpCond::EQ:
          out.instrs.push_back(make1(Op::CsetEq, OpRegV{vd}));
          break;
        case ir::ICmpCond::NE:
          out.instrs.push_back(make1(Op::CsetNe, OpRegV{vd}));
          break;
        case ir::ICmpCond::ULT:
          out.instrs.push_back(make1(Op::CsetLo, OpRegV{vd}));
          break;
        case ir::ICmpCond::ULE:
          out.instrs.push_back(make1(Op::CsetLs, OpRegV{vd}));
          break;
        case ir::ICmpCond::UGT:
          out.instrs.push_back(make1(Op::CsetHi, OpRegV{vd}));
          break;
        case ir::ICmpCond::UGE:
          out.instrs.push_back(make1(Op::CsetHs, OpRegV{vd}));
          break;
        case ir::ICmpCond::SLT:
          out.instrs.push_back(make1(Op::CsetLt, OpRegV{vd}));
          break;
        case ir::ICmpCond::SLE:
          out.instrs.push_back(make1(Op::CsetLe, OpRegV{vd}));
          break;
        case ir::ICmpCond::SGT:
          out.instrs.push_back(make1(Op::CsetGt, OpRegV{vd}));
          break;
        case ir::ICmpCond::SGE:
          out.instrs.push_back(make1(Op::CsetGe, OpRegV{vd}));
          break;
        }
      }
    } else if (std::holds_alternative<ir::ZExt>(I.payload)) {
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        auto ps = vreg_of(std::get<ir::ZExt>(I.payload).src);
        auto &Z = std::get<ir::ZExt>(I.payload);
        if (Z.to.kind == ir::TypeKind::I64) {
          out.instrs.push_back(make2(Op::Uxtw, OpRegV{vd}, OpRegV{ps}));
        } else {
          out.instrs.push_back(make2(Op::Mov, OpRegV{vd}, OpRegV{ps}));
        }
      }
    } else if (std::holds_alternative<ir::SExt>(I.payload)) {
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        auto ps = vreg_of(std::get<ir::SExt>(I.payload).src);
        auto &S = std::get<ir::SExt>(I.payload);
        if (S.to.kind == ir::TypeKind::I64) {
          out.instrs.push_back(make2(Op::Sxtw, OpRegV{vd}, OpRegV{ps}));
        } else {
          out.instrs.push_back(make2(Op::Mov, OpRegV{vd}, OpRegV{ps}));
        }
      }
    } else if (std::holds_alternative<ir::Trunc>(I.payload)) {
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        auto ps = vreg_of(std::get<ir::Trunc>(I.payload).src);
        out.instrs.push_back(make2(Op::Mov, OpRegV{vd}, OpRegV{ps}));
      }
    } else if (std::holds_alternative<ir::Load>(I.payload)) {
      auto &L = std::get<ir::Load>(I.payload);
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        // Compute guest EA: base + mem_base(x21); carry IR offset in
        // OpMem.offset
        auto vbase = vreg_of(L.base);
        VReg vaddr = static_cast<VReg>(vmap.size() + 1);
        vmap.push_back(vaddr);
        out.instrs.push_back(
            make3(Op::Add, OpRegV{vaddr}, OpRegV{vbase}, OpRegP{21}));
        Op op = Op::LdrX;
        switch (L.ty.kind) {
        case ir::TypeKind::I64:
          op = Op::LdrX;
          break;
        case ir::TypeKind::I32:
          op = Op::LdrW;
          break;
        case ir::TypeKind::I16:
          op = Op::LdrH;
          break;
        case ir::TypeKind::I8:
          op = Op::LdrB;
          break;
        default:
          op = Op::LdrX;
          break;
        }
        out.instrs.push_back(
            Instr{op,
                  {OpRegV{vd},
                   OpMem{OpRegV{vaddr}, static_cast<int32_t>(L.offset)}}});
      }
    } else if (std::holds_alternative<ir::Store>(I.payload)) {
      auto &S = std::get<ir::Store>(I.payload);
      // Compute guest EA: base + mem_base(x21); carry IR offset in OpMem.offset
      auto vbase = vreg_of(S.base);
      VReg vaddr = static_cast<VReg>(vmap.size() + 1);
      vmap.push_back(vaddr);
      out.instrs.push_back(
          make3(Op::Add, OpRegV{vaddr}, OpRegV{vbase}, OpRegP{21}));
      Op op = Op::StrX;
      switch (S.ty.kind) {
      case ir::TypeKind::I64:
        op = Op::StrX;
        break;
      case ir::TypeKind::I32:
        op = Op::StrW;
        break;
      case ir::TypeKind::I16:
        op = Op::StrH;
        break;
      case ir::TypeKind::I8:
        op = Op::StrB;
        break;
      default:
        op = Op::StrX;
        break;
      }
      out.instrs.push_back(
          Instr{op,
                {OpRegV{vreg_of(S.value)},
                 OpMem{OpRegV{vaddr}, static_cast<int32_t>(S.offset)}}});
    } else if (std::holds_alternative<ir::GetPC>(I.payload)) {
      if (I.dest) {
        auto vd = vreg_of(*I.dest);
        out.instrs.push_back(make2(Op::Mov, OpRegV{vd}, OpImm{bb.start}));
      }
    }
  }

  // Terminator
  switch (bb.term.kind) {
  case ir::TermKind::Br: {
    auto t = std::get<ir::TermBr>(bb.term.data);
    std::stringstream ss;
    ss << std::hex << t.target;
    out.term.kind = TermKind::Br;
    out.term.data = TermBr{"__riscy_block_0x" + ss.str()};
    break;
  }
  case ir::TermKind::CBr: {
    auto t = std::get<ir::TermCBr>(bb.term.data);
    std::stringstream sst, ssf;
    sst << std::hex << t.t;
    ssf << std::hex << t.f;
    out.term.kind = TermKind::CBr;
    out.term.data = TermCBr{vreg_of(t.cond), "__riscy_block_0x" + sst.str(),
                            "__riscy_block_0x" + ssf.str()};
    break;
  }
  case ir::TermKind::BrIndirect: {
    auto t = std::get<ir::TermBrIndirect>(bb.term.data);
    out.term.kind = TermKind::BrIndirect;
    out.term.data = TermBrIndirect{vreg_of(t.target)};
    break;
  }
  case ir::TermKind::Ret:
    out.term.kind = TermKind::Ret;
    break;
  case ir::TermKind::Trap:
    out.term.kind = TermKind::Trap;
    break;
  case ir::TermKind::None:
    out.term.kind = TermKind::None;
    break;
  }

  return out;
}

} // namespace riscy::aarch64
