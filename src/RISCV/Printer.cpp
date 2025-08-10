#include "RISCV/Printer.h"

#include <sstream>

namespace riscy::riscv {

const char *opcodeName(Opcode op) {
  switch (op) {
  case Opcode::LUI:
    return "LUI";
  case Opcode::AUIPC:
    return "AUIPC";
  case Opcode::JAL:
    return "JAL";
  case Opcode::JALR:
    return "JALR";
  case Opcode::BEQ:
    return "BEQ";
  case Opcode::BNE:
    return "BNE";
  case Opcode::BLT:
    return "BLT";
  case Opcode::BGE:
    return "BGE";
  case Opcode::BLTU:
    return "BLTU";
  case Opcode::BGEU:
    return "BGEU";
  case Opcode::LB:
    return "LB";
  case Opcode::LH:
    return "LH";
  case Opcode::LW:
    return "LW";
  case Opcode::LD:
    return "LD";
  case Opcode::LBU:
    return "LBU";
  case Opcode::LHU:
    return "LHU";
  case Opcode::LWU:
    return "LWU";
  case Opcode::SB:
    return "SB";
  case Opcode::SH:
    return "SH";
  case Opcode::SW:
    return "SW";
  case Opcode::SD:
    return "SD";
  case Opcode::ADDI:
    return "ADDI";
  case Opcode::SLTI:
    return "SLTI";
  case Opcode::SLTIU:
    return "SLTIU";
  case Opcode::XORI:
    return "XORI";
  case Opcode::ORI:
    return "ORI";
  case Opcode::ANDI:
    return "ANDI";
  case Opcode::SLLI:
    return "SLLI";
  case Opcode::SRLI:
    return "SRLI";
  case Opcode::SRAI:
    return "SRAI";
  case Opcode::ADD:
    return "ADD";
  case Opcode::SUB:
    return "SUB";
  case Opcode::SLL:
    return "SLL";
  case Opcode::SLT:
    return "SLT";
  case Opcode::SLTU:
    return "SLTU";
  case Opcode::XOR:
    return "XOR";
  case Opcode::SRL:
    return "SRL";
  case Opcode::SRA:
    return "SRA";
  case Opcode::OR:
    return "OR";
  case Opcode::AND:
    return "AND";
  case Opcode::ADDIW:
    return "ADDIW";
  case Opcode::SLLIW:
    return "SLLIW";
  case Opcode::SRLIW:
    return "SRLIW";
  case Opcode::SRAIW:
    return "SRAIW";
  case Opcode::ADDW:
    return "ADDW";
  case Opcode::SUBW:
    return "SUBW";
  case Opcode::SLLW:
    return "SLLW";
  case Opcode::SRLW:
    return "SRLW";
  case Opcode::SRAW:
    return "SRAW";
  case Opcode::FENCE:
    return "FENCE";
  case Opcode::ECALL:
    return "ECALL";
  case Opcode::EBREAK:
    return "EBREAK";
  case Opcode::UNKNOWN:
    return "UNKNOWN";
  }
  return "UNKNOWN";
}

static std::string regName(uint8_t r) {
  return "x" + std::to_string(static_cast<unsigned>(r));
}

std::string formatOperand(const Operand &op) {
  std::ostringstream os;
  if (std::holds_alternative<Reg>(op)) {
    const auto &r = std::get<Reg>(op);
    os << regName(r.index);
  } else if (std::holds_alternative<Imm>(op)) {
    const auto &i = std::get<Imm>(op);
    os << i.value;
  } else if (std::holds_alternative<Mem>(op)) {
    const auto &m = std::get<Mem>(op);
    os << m.offset << "(" << regName(m.base) << ")";
  }
  return os.str();
}

std::string formatInst(const DecodedInst &inst) {
  std::ostringstream os;
  os << opcodeName(inst.opcode);
  if (!inst.operands.empty()) {
    os << ' ';
    for (size_t i = 0; i < inst.operands.size(); ++i) {
      if (i)
        os << ", ";
      os << formatOperand(inst.operands[i]);
    }
  }
  return os.str();
}

} // namespace riscy::riscv
