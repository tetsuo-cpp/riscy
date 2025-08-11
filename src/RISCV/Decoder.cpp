#include "RISCV/Decoder.h"

namespace riscy::riscv {

static inline uint8_t rd(uint32_t x) { return (x >> 7) & 0x1F; }
static inline uint8_t funct3(uint32_t x) { return (x >> 12) & 0x7; }
static inline uint8_t rs1(uint32_t x) { return (x >> 15) & 0x1F; }
static inline uint8_t rs2(uint32_t x) { return (x >> 20) & 0x1F; }
static inline uint8_t funct7(uint32_t x) { return (x >> 25) & 0x7F; }

bool Decoder::decodeNext(const MemoryReader &mem, uint64_t pc,
                         DecodedInst &outInst, DecodeError &outErr) const {
  outErr = DecodeError::None;
  if ((pc & 0x3) != 0) {
    outErr = DecodeError::MisalignedPC;
    return false;
  }
  uint32_t insn = 0;
  if (!mem.read32(pc, insn)) {
    outErr = DecodeError::OOBRead;
    return false;
  }

  outInst = {};
  outInst.pc = pc;
  outInst.raw = insn;

  const uint32_t opc = insn & 0x7F;
  switch (opc) {
  case 0x37: { // LUI
    outInst.opcode = Opcode::LUI;
    std::int64_t imm = sext(static_cast<std::int64_t>(insn & 0xFFFFF000), 32);
    outInst.operands = {Reg{rd(insn)}, Imm{imm}};
    return true;
  }
  case 0x17: { // AUIPC
    outInst.opcode = Opcode::AUIPC;
    std::int64_t imm = sext(static_cast<std::int64_t>(insn & 0xFFFFF000), 32);
    outInst.operands = {Reg{rd(insn)}, Imm{imm}};
    return true;
  }
  case 0x6F: { // JAL
    outInst.opcode = Opcode::JAL;
    std::int32_t imm = 0;
    imm |= (static_cast<std::int32_t>(getBits(insn, 31, 31)) << 20);
    imm |= (static_cast<std::int32_t>(getBits(insn, 19, 12)) << 12);
    imm |= (static_cast<std::int32_t>(getBits(insn, 20, 20)) << 11);
    imm |= (static_cast<std::int32_t>(getBits(insn, 30, 21)) << 1);
    outInst.operands = {Reg{rd(insn)},
                        Imm{sext(static_cast<std::int64_t>(imm), 21)}};
    return true;
  }
  case 0x67: { // JALR
    outInst.opcode = Opcode::JALR;
    std::uint32_t imm12 = (insn >> 20) & 0xFFFu; // extract 12-bit immediate
    outInst.operands = {
        Reg{rd(insn)},
        Mem{rs1(insn), sext(static_cast<std::int64_t>(imm12), 12)}};
    return true;
  }
  case 0x63: { // BRANCH
    const auto f3 = funct3(insn);
    switch (f3) {
    case 0x0:
      outInst.opcode = Opcode::BEQ;
      break;
    case 0x1:
      outInst.opcode = Opcode::BNE;
      break;
    case 0x4:
      outInst.opcode = Opcode::BLT;
      break;
    case 0x5:
      outInst.opcode = Opcode::BGE;
      break;
    case 0x6:
      outInst.opcode = Opcode::BLTU;
      break;
    case 0x7:
      outInst.opcode = Opcode::BGEU;
      break;
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    std::int32_t imm = 0;
    imm |= (static_cast<std::int32_t>(getBits(insn, 31, 31)) << 12);
    imm |= (static_cast<std::int32_t>(getBits(insn, 7, 7)) << 11);
    imm |= (static_cast<std::int32_t>(getBits(insn, 30, 25)) << 5);
    imm |= (static_cast<std::int32_t>(getBits(insn, 11, 8)) << 1);
    outInst.operands = {Reg{rs1(insn)}, Reg{rs2(insn)},
                        Imm{sext(static_cast<std::int64_t>(imm), 13)}};
    return true;
  }
  case 0x03: { // LOAD
    const auto f3 = funct3(insn);
    switch (f3) {
    case 0x0:
      outInst.opcode = Opcode::LB;
      break;
    case 0x1:
      outInst.opcode = Opcode::LH;
      break;
    case 0x2:
      outInst.opcode = Opcode::LW;
      break;
    case 0x3:
      outInst.opcode = Opcode::LD;
      break;
    case 0x4:
      outInst.opcode = Opcode::LBU;
      break;
    case 0x5:
      outInst.opcode = Opcode::LHU;
      break;
    case 0x6:
      outInst.opcode = Opcode::LWU;
      break;
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
    outInst.operands = {
        Reg{rd(insn)},
        Mem{rs1(insn), sext(static_cast<std::int64_t>(imm12), 12)}};
    return true;
  }
  case 0x23: { // STORE
    const auto f3 = funct3(insn);
    switch (f3) {
    case 0x0:
      outInst.opcode = Opcode::SB;
      break;
    case 0x1:
      outInst.opcode = Opcode::SH;
      break;
    case 0x2:
      outInst.opcode = Opcode::SW;
      break;
    case 0x3:
      outInst.opcode = Opcode::SD;
      break;
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    std::int32_t imm = (static_cast<std::int32_t>(getBits(insn, 31, 25)) << 5) |
                       static_cast<std::int32_t>(getBits(insn, 11, 7));
    outInst.operands = {Mem{rs1(insn), sext(imm, 12)}, Reg{rs2(insn)}};
    return true;
  }
  case 0x13: { // OP-IMM
    const auto f3 = funct3(insn);
    switch (f3) {
    case 0x0:
      outInst.opcode = Opcode::ADDI;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x2:
      outInst.opcode = Opcode::SLTI;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x3:
      outInst.opcode = Opcode::SLTIU;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x4:
      outInst.opcode = Opcode::XORI;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x6:
      outInst.opcode = Opcode::ORI;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x7:
      outInst.opcode = Opcode::ANDI;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x1: { // SLLI
      outInst.opcode = Opcode::SLLI;
      outInst.operands = {
          Reg{rd(insn)}, Reg{rs1(insn)},
          Imm{static_cast<std::int64_t>(getBits(insn, 25, 20))}};
      return true;
    }
    case 0x5: { // SRLI/SRAI
      const auto f7 = funct7(insn);
      if (f7 == 0x00) {
        outInst.opcode = Opcode::SRLI;
        outInst.operands = {
            Reg{rd(insn)}, Reg{rs1(insn)},
            Imm{static_cast<std::int64_t>(getBits(insn, 25, 20))}};
        return true;
      }
      if (f7 == 0x20) {
        outInst.opcode = Opcode::SRAI;
        outInst.operands = {
            Reg{rd(insn)}, Reg{rs1(insn)},
            Imm{static_cast<std::int64_t>(getBits(insn, 25, 20))}};
        return true;
      }
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
  }
  case 0x1B: { // OP-IMM-32 (W)
    const auto f3 = funct3(insn);
    switch (f3) {
    case 0x0: // ADDIW
      outInst.opcode = Opcode::ADDIW;
      {
        std::uint32_t imm12 = (insn >> 20) & 0xFFFu;
        outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)},
                            Imm{sext(static_cast<std::int64_t>(imm12), 12)}};
      }
      return true;
    case 0x1: { // SLLIW
      const auto f7 = funct7(insn);
      if (f7 == 0x00) {
        outInst.opcode = Opcode::SLLIW;
        outInst.operands = {
            Reg{rd(insn)}, Reg{rs1(insn)},
            Imm{static_cast<std::int64_t>(getBits(insn, 24, 20))}};
        return true;
      }
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    case 0x5: { // SRLIW/SRAIW
      const auto f7 = funct7(insn);
      if (f7 == 0x00) {
        outInst.opcode = Opcode::SRLIW;
        outInst.operands = {
            Reg{rd(insn)}, Reg{rs1(insn)},
            Imm{static_cast<std::int64_t>(getBits(insn, 24, 20))}};
        return true;
      }
      if (f7 == 0x20) {
        outInst.opcode = Opcode::SRAIW;
        outInst.operands = {
            Reg{rd(insn)}, Reg{rs1(insn)},
            Imm{static_cast<std::int64_t>(getBits(insn, 24, 20))}};
        return true;
      }
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
  }
  case 0x33: { // OP
    const auto f3 = funct3(insn);
    const auto f7 = funct7(insn);
    switch (f3) {
    case 0x0:
      outInst.opcode = (f7 == 0x20 ? Opcode::SUB : Opcode::ADD);
      break;
    case 0x1:
      outInst.opcode = Opcode::SLL;
      break;
    case 0x2:
      outInst.opcode = Opcode::SLT;
      break;
    case 0x3:
      outInst.opcode = Opcode::SLTU;
      break;
    case 0x4:
      outInst.opcode = Opcode::XOR;
      break;
    case 0x5:
      outInst.opcode = (f7 == 0x20 ? Opcode::SRA : Opcode::SRL);
      break;
    case 0x6:
      outInst.opcode = Opcode::OR;
      break;
    case 0x7:
      outInst.opcode = Opcode::AND;
      break;
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)}, Reg{rs2(insn)}};
    return true;
  }
  case 0x3B: { // OP-32 (W)
    const auto f3 = funct3(insn);
    const auto f7 = funct7(insn);
    switch (f3) {
    case 0x0:
      outInst.opcode = (f7 == 0x20 ? Opcode::SUBW : Opcode::ADDW);
      break;
    case 0x1:
      outInst.opcode = Opcode::SLLW;
      break;
    case 0x5:
      outInst.opcode = (f7 == 0x20 ? Opcode::SRAW : Opcode::SRLW);
      break;
    default:
      outErr = DecodeError::InvalidOpcode;
      return false;
    }
    outInst.operands = {Reg{rd(insn)}, Reg{rs1(insn)}, Reg{rs2(insn)}};
    return true;
  }
  case 0x0F: // FENCE
    outInst.opcode = Opcode::FENCE;
    return true;
  case 0x73: { // SYSTEM
    const auto f3 = funct3(insn);
    if (f3 == 0) {
      if (getBits(insn, 31, 20) == 0) {
        outInst.opcode = Opcode::ECALL;
        return true;
      }
      if (getBits(insn, 31, 20) == 1) {
        outInst.opcode = Opcode::EBREAK;
        return true;
      }
    }
    outErr = DecodeError::InvalidOpcode;
    return false;
  }
  default:
    outErr = DecodeError::InvalidOpcode;
    return false;
  }
}

} // namespace riscy::riscv
