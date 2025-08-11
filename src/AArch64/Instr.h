#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace riscy::aarch64 {

using VReg = uint32_t; // virtual register id

// Physical registers are encoded as 0..30 for x0..x30 (x31 is sp/zero, avoid)
using PReg = int;

enum class Op {
  Mov,
  MovZ,
  MovK,
  Add,
  Sub,
  And,
  Orr,
  Eor,
  Lsl,
  Lsr,
  Asr,
  LdrX,
  LdrW,
  LdrB,
  LdrH,
  LdrSW,
  StrX,
  StrW,
  StrB,
  StrH,
  Cmp,
  CsetEq,
  CsetNe,
  CsetLo,
  CsetLs,
  CsetHi,
  CsetHs,
  CsetLt,
  CsetLe,
  CsetGt,
  CsetGe,
  Sxtw,
  Uxtw,
  Bl,
  Br,
  B,
  Bne,
  Beq,
  Ret,
  Brk,
  // Pseudo for labels
  Label,
};

struct OpRegV {
  VReg id = 0;
};
struct OpRegP {
  PReg id = 0;
};
struct OpImm {
  uint64_t value = 0;
};
struct OpMem {
  OpRegV base;
  int32_t offset = 0;
};
struct OpLabel {
  std::string name;
};

using Operand = std::variant<OpRegV, OpRegP, OpImm, OpMem, OpLabel>;

struct Instr {
  Op op{};
  std::vector<Operand> ops{};
};

enum class TermKind { None, Br, CBr, BrIndirect, Ret, Trap };

struct TermBr {
  std::string target;
};
struct TermCBr {
  VReg cond;
  std::string t, f;
};
struct TermBrIndirect {
  VReg target;
};

struct Terminator {
  TermKind kind = TermKind::None;
  std::variant<std::monostate, TermBr, TermCBr, TermBrIndirect> data{};
};

struct Block {
  uint64_t guest_pc = 0;
  std::vector<Instr> instrs;
  Terminator term;
};

} // namespace riscy::aarch64
