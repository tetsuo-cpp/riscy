#include "AArch64/Emitter.h"
#include <algorithm>
#include <sstream>

namespace riscy::aarch64 {

static std::string pc_hex(uint64_t pc) {
  std::stringstream ss;
  ss << std::hex << pc;
  return ss.str();
}

static std::string rx(int p) { return "x" + std::to_string(p); }
static std::string rw(int p) { return "w" + std::to_string(p); }

static int map_v(const RegAssignment &asg, VReg v) {
  auto it = asg.v2p.find(v);
  if (it == asg.v2p.end()) {
    fprintf(stderr,
            "Emitter error: vreg %d has no assigned physical register\n", v);
    abort();
  }
  return it->second;
}

static int map_any_reg(const RegAssignment &asg, const Operand &op) {
  if (std::holds_alternative<OpRegV>(op))
    return map_v(asg, std::get<OpRegV>(op).id);
  if (std::holds_alternative<OpRegP>(op))
    return std::get<OpRegP>(op).id;
  return 9;
}

ModuleAsm Emitter::emit(const std::vector<Block> &blocks,
                        const std::vector<RegAssignment> &assignments,
                        uint64_t entry_pc) const {
  ModuleAsm out{};
  std::stringstream s;
  s << ".text\n";
  s << ".global _riscy_entry\n";
  s << "// x0 = struct RiscyGuestState*; x1 = start guest PC\n";
  s << "_riscy_entry:\n";
  s << "  mov x19, x30\n";
  s << "  bl _riscy_indirect_jump\n";
  s << "  ret x19\n\n";

  // Tables
  s << ".data\n";
  s << ".align 3\n";
  s << ".global _riscy_entry_pc\n";
  s << "_riscy_entry_pc:\n  .quad 0x" << pc_hex(entry_pc) << "\n";
  s << ".global _riscy_num_blocks\n";
  s << "_riscy_num_blocks:\n  .quad " << blocks.size() << "\n";
  s << ".global _riscy_block_addrs\n";
  s << "_riscy_block_addrs:\n";
  for (auto &b : blocks)
    s << "  .quad 0x" << pc_hex(b.guest_pc) << "\n";
  s << ".global _riscy_block_ptrs\n";
  s << "_riscy_block_ptrs:\n";
  for (auto &b : blocks)
    s << "  .quad __riscy_block_0x" << pc_hex(b.guest_pc) << "\n";
  s << "\n.text\n";

  // Emit blocks
  for (size_t i = 0; i < blocks.size(); ++i) {
    const auto &b = blocks[i];
    const auto &asg = assignments[i];
    s << "__riscy_block_0x" << pc_hex(b.guest_pc) << ":\n";
    // Load guest memory base (offset 256 after 32 xregs)
    s << "  ldr x21, [x0, #256]\n";
    for (const auto &I : b.instrs) {
      switch (I.op) {
      case Op::Mov: {
        const auto &a = I.ops[0];
        const auto &b0 = I.ops[1];
        int pd = map_v(asg, std::get<OpRegV>(a).id);
        if (std::holds_alternative<OpRegV>(b0) ||
            std::holds_alternative<OpRegP>(b0)) {
          int ps = map_any_reg(asg, b0);
          s << "  mov " << rx(pd) << ", " << rx(ps) << "\n";
        } else if (std::holds_alternative<OpImm>(b0)) {
          s << "  mov " << rx(pd) << ", #" << std::get<OpImm>(b0).value << "\n";
        }
        break;
      }
      case Op::MovZ: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        auto imm = std::get<OpImm>(I.ops[1]).value;
        s << "  movz " << rx(pd) << ", #" << imm << "\n";
        break;
      }
      case Op::MovK: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        auto imm = std::get<OpImm>(I.ops[1]).value;
        auto lsl = std::get<OpImm>(I.ops[2]).value;
        s << "  movk " << rx(pd) << ", #" << imm << ", lsl #" << lsl << "\n";
        break;
      }
      case Op::Add:
      case Op::Sub:
      case Op::And:
      case Op::Orr:
      case Op::Eor:
      case Op::Lsl:
      case Op::Lsr:
      case Op::Asr: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        int pa = map_any_reg(asg, I.ops[1]);
        int pb = map_any_reg(asg, I.ops[2]);
        const char *mn = I.op == Op::Add   ? "add"
                         : I.op == Op::Sub ? "sub"
                         : I.op == Op::And ? "and"
                         : I.op == Op::Orr ? "orr"
                         : I.op == Op::Eor ? "eor"
                         : I.op == Op::Lsl ? "lsl"
                         : I.op == Op::Lsr ? "lsr"
                                           : "asr";
        s << "  " << mn << " " << rx(pd) << ", " << rx(pa) << ", " << rx(pb)
          << "\n";
        break;
      }
      case Op::LdrX:
      case Op::LdrW:
      case Op::LdrB:
      case Op::LdrH:
      case Op::LdrSW: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        const auto &mem = std::get<OpMem>(I.ops[1]);
        int pbase = mem.base.id ? map_v(asg, mem.base.id) : 0; // vreg 0 -> x0
        const char *mn = I.op == Op::LdrX   ? "ldr"
                         : I.op == Op::LdrW ? "ldr"
                         : I.op == Op::LdrB ? "ldrb"
                         : I.op == Op::LdrH ? "ldrh"
                                            : "ldrsw";
        auto reg = (I.op == Op::LdrW || I.op == Op::LdrB || I.op == Op::LdrH)
                       ? rw(pd)
                       : rx(pd);
        s << "  " << mn << " " << reg << ", [" << rx(pbase) << ", #"
          << mem.offset << "]\n";
        break;
      }
      case Op::StrX:
      case Op::StrW:
      case Op::StrB:
      case Op::StrH: {
        int pv = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        const auto &mem = std::get<OpMem>(I.ops[1]);
        int pbase = mem.base.id ? map_v(asg, mem.base.id) : 0;
        const char *mn = I.op == Op::StrX   ? "str"
                         : I.op == Op::StrW ? "str"
                         : I.op == Op::StrB ? "strb"
                                            : "strh";
        auto reg = (I.op == Op::StrW || I.op == Op::StrB || I.op == Op::StrH)
                       ? rw(pv)
                       : rx(pv);
        s << "  " << mn << " " << reg << ", [" << rx(pbase) << ", #"
          << mem.offset << "]\n";
        break;
      }
      case Op::Cmp: {
        int pa = map_any_reg(asg, I.ops[0]);
        int pb = map_any_reg(asg, I.ops[1]);
        s << "  cmp " << rx(pa) << ", " << rx(pb) << "\n";
        break;
      }
      case Op::CsetEq:
      case Op::CsetNe:
      case Op::CsetLo:
      case Op::CsetLs:
      case Op::CsetHi:
      case Op::CsetHs:
      case Op::CsetLt:
      case Op::CsetLe:
      case Op::CsetGt:
      case Op::CsetGe: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        const char *cc = I.op == Op::CsetEq   ? "eq"
                         : I.op == Op::CsetNe ? "ne"
                         : I.op == Op::CsetLo ? "lo"
                         : I.op == Op::CsetLs ? "ls"
                         : I.op == Op::CsetHi ? "hi"
                         : I.op == Op::CsetHs ? "hs"
                         : I.op == Op::CsetLt ? "lt"
                         : I.op == Op::CsetLe ? "le"
                         : I.op == Op::CsetGt ? "gt"
                                              : "ge";
        s << "  cset " << rx(pd) << ", " << cc << "\n";
        break;
      }
      case Op::Sxtw: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        int ps = map_v(asg, std::get<OpRegV>(I.ops[1]).id);
        s << "  sxtw " << rx(pd) << ", " << rw(ps) << "\n";
        break;
      }
      case Op::Uxtw: {
        int pd = map_v(asg, std::get<OpRegV>(I.ops[0]).id);
        int ps = map_v(asg, std::get<OpRegV>(I.ops[1]).id);
        s << "  uxtw " << rx(pd) << ", " << rw(ps) << "\n";
        break;
      }
      default:
        break;
      }
    }

    // Terminator
    switch (b.term.kind) {
    case TermKind::Br: {
      auto t = std::get<TermBr>(b.term.data);
      s << "  b " << t.target << "\n";
      break;
    }
    case TermKind::CBr: {
      auto t = std::get<TermCBr>(b.term.data);
      int pc = map_v(asg, t.cond);
      s << "  cmp " << rx(pc) << ", #0\n";
      s << "  b.ne " << t.t << "\n";
      s << "  b " << t.f << "\n";
      break;
    }
    case TermKind::BrIndirect: {
      auto t = std::get<TermBrIndirect>(b.term.data);
      int pt = map_v(asg, t.target);
      s << "  mov x1, " << rx(pt) << "\n";
      s << "  bl _riscy_indirect_jump\n";
      break;
    }
    case TermKind::Ret:
      s << "  ret\n";
      break;
    case TermKind::Trap:
      s << "  brk #0\n";
      break;
    case TermKind::None:
      break;
    }

    s << "\n";
  }

  out.text = s.str();
  return out;
}

} // namespace riscy::aarch64
