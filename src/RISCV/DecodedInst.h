#pragma once

#include <variant>
#include <vector>

namespace riscy::riscv {

enum class Opcode : uint16_t {
  LUI,
  AUIPC,
  JAL,
  JALR,
  BEQ,
  BNE,
  BLT,
  BGE,
  BLTU,
  BGEU,
  LB,
  LH,
  LW,
  LD,
  LBU,
  LHU,
  LWU,
  SB,
  SH,
  SW,
  SD,
  ADDI,
  SLTI,
  SLTIU,
  XORI,
  ORI,
  ANDI,
  SLLI,
  SRLI,
  SRAI,
  ADD,
  SUB,
  SLL,
  SLT,
  SLTU,
  XOR,
  SRL,
  SRA,
  OR,
  AND,
  ADDIW,
  SLLIW,
  SRLIW,
  SRAIW,
  ADDW,
  SUBW,
  SLLW,
  SRLW,
  SRAW,
  FENCE,
  ECALL,
  EBREAK,
  UNKNOWN
};

struct Reg {
  uint8_t index = 0;
};
struct Imm {
  int64_t value = 0;
};
struct Mem {
  uint8_t base = 0;
  int64_t offset = 0;
};

using Operand = std::variant<Reg, Imm, Mem>;

struct DecodedInst {
  uint64_t pc = 0;
  uint32_t raw = 0;
  Opcode opcode = Opcode::UNKNOWN;
  std::vector<Operand> operands; // Convention: dest first (Mem for stores)
};

} // namespace riscy::riscv
