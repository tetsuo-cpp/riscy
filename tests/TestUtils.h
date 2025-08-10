#pragma once

#include <cstdint>
#include <vector>

// Common test helpers for encoding RISC-V instructions and building buffers.

inline void appendWordLE(std::vector<unsigned char> &buf, uint32_t w) {
  buf.push_back(static_cast<unsigned char>(w & 0xFF));
  buf.push_back(static_cast<unsigned char>((w >> 8) & 0xFF));
  buf.push_back(static_cast<unsigned char>((w >> 16) & 0xFF));
  buf.push_back(static_cast<unsigned char>((w >> 24) & 0xFF));
}

inline uint32_t encodeU(uint32_t imm20, uint8_t rd, uint8_t opcode) {
  return (imm20 << 12) | (static_cast<uint32_t>(rd) << 7) | opcode;
}

inline uint32_t encodeI(std::int32_t imm12, uint8_t rs1, uint8_t funct3,
                        uint8_t rd, uint8_t opcode) {
  uint32_t i = static_cast<uint32_t>(imm12 & 0xFFF);
  return (i << 20) | (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}

inline uint32_t encodeR(uint8_t funct7, uint8_t rs2, uint8_t rs1,
                        uint8_t funct3, uint8_t rd, uint8_t opcode) {
  return (static_cast<uint32_t>(funct7) << 25) |
         (static_cast<uint32_t>(rs2) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}

inline uint32_t encodeS(std::int32_t imm12, uint8_t rs2, uint8_t rs1,
                        uint8_t funct3, uint8_t opcode) {
  uint32_t i = static_cast<uint32_t>(imm12 & 0xFFF);
  uint32_t immHi = (i >> 5) & 0x7F;
  uint32_t immLo = i & 0x1F;
  return (immHi << 25) | (static_cast<uint32_t>(rs2) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) | (immLo << 7) | opcode;
}

inline uint32_t encodeB(std::int32_t immBytes, uint8_t rs2, uint8_t rs1,
                        uint8_t funct3, uint8_t opcode) {
  uint32_t b = static_cast<uint32_t>(immBytes);
  uint32_t bit12 = (b >> 12) & 0x1;
  uint32_t bits10_5 = (b >> 5) & 0x3F;
  uint32_t bits4_1 = (b >> 1) & 0xF;
  uint32_t bit11 = (b >> 11) & 0x1;
  return (bit12 << 31) | (bits10_5 << 25) | (static_cast<uint32_t>(rs2) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) | (bits4_1 << 8) | (bit11 << 7) |
         opcode;
}

inline uint32_t encodeShiftI(uint8_t funct7, uint8_t shamt, uint8_t rs1,
                             uint8_t funct3, uint8_t rd, uint8_t opcode) {
  return (static_cast<uint32_t>(funct7) << 25) |
         (static_cast<uint32_t>(shamt) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}

inline uint32_t encodeJ(std::int32_t immBytes, uint8_t rd, uint8_t opcode) {
  // imm layout: [20|10:1|11|19:12]
  uint32_t b = static_cast<uint32_t>(immBytes);
  uint32_t bit20 = (b >> 20) & 0x1;
  uint32_t bits10_1 = (b >> 1) & 0x3FF;
  uint32_t bit11 = (b >> 11) & 0x1;
  uint32_t bits19_12 = (b >> 12) & 0xFF;
  return (bit20 << 31) | (bits19_12 << 12) | (bit11 << 20) | (bits10_1 << 21) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}
