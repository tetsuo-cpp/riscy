#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "MemoryReaders.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"

static void appendWordLE(std::vector<unsigned char> &buf, uint32_t w) {
  buf.push_back(static_cast<unsigned char>(w & 0xFF));
  buf.push_back(static_cast<unsigned char>((w >> 8) & 0xFF));
  buf.push_back(static_cast<unsigned char>((w >> 16) & 0xFF));
  buf.push_back(static_cast<unsigned char>((w >> 24) & 0xFF));
}

// Encoders for a few instruction formats
static uint32_t encodeU(uint32_t imm20, uint8_t rd, uint8_t opcode) {
  return (imm20 << 12) | (static_cast<uint32_t>(rd) << 7) | opcode;
}

static uint32_t encodeI(std::int32_t imm12, uint8_t rs1, uint8_t funct3,
                        uint8_t rd, uint8_t opcode) {
  uint32_t i = static_cast<uint32_t>(imm12 & 0xFFF);
  return (i << 20) | (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}

static uint32_t encodeR(uint8_t funct7, uint8_t rs2, uint8_t rs1,
                        uint8_t funct3, uint8_t rd, uint8_t opcode) {
  return (static_cast<uint32_t>(funct7) << 25) |
         (static_cast<uint32_t>(rs2) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}

static uint32_t encodeS(std::int32_t imm12, uint8_t rs2, uint8_t rs1,
                        uint8_t funct3, uint8_t opcode) {
  uint32_t i = static_cast<uint32_t>(imm12 & 0xFFF);
  uint32_t immHi = (i >> 5) & 0x7F;
  uint32_t immLo = i & 0x1F;
  return (immHi << 25) | (static_cast<uint32_t>(rs2) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) | (immLo << 7) | opcode;
}

static uint32_t encodeB(std::int32_t immBytes, uint8_t rs2, uint8_t rs1,
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

static uint32_t encodeShiftI(uint8_t funct7, uint8_t shamt, uint8_t rs1,
                             uint8_t funct3, uint8_t rd, uint8_t opcode) {
  return (static_cast<uint32_t>(funct7) << 25) |
         (static_cast<uint32_t>(shamt) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(funct3) << 12) |
         (static_cast<uint32_t>(rd) << 7) | opcode;
}

TEST_CASE("RV64I basic decode", "[decoder]") {
  std::vector<unsigned char> code;

  // ADDI x1, x0, 1
  appendWordLE(code, encodeI(1, 0, 0x0, 1, 0x13));
  // LUI x2, 0x10 (=> 0x00010000 << 12)
  appendWordLE(code, encodeU(0x10, 2, 0x37));
  // ADD x3, x1, x2
  appendWordLE(code, encodeR(0x00, 2, 1, 0x0, 3, 0x33));
  // BEQ x0, x0, 0 (zero offset)
  appendWordLE(code, 0x00000063);

  uint64_t base = 0x1000;
  riscy::SpanMemoryReader mem(base, code.data(), code.size());
  riscy::riscv::Decoder dec;

  // 1) ADDI
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::ADDI);
    REQUIRE(I.operands.size() == 3);
    REQUIRE(std::holds_alternative<riscy::riscv::Reg>(I.operands[0]));
    REQUIRE(std::holds_alternative<riscy::riscv::Reg>(I.operands[1]));
    REQUIRE(std::holds_alternative<riscy::riscv::Imm>(I.operands[2]));
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 1);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 0);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 1);
  }

  // 2) LUI
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 4, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::LUI);
    REQUIRE(I.operands.size() == 2);
    REQUIRE(std::holds_alternative<riscy::riscv::Reg>(I.operands[0]));
    REQUIRE(std::holds_alternative<riscy::riscv::Imm>(I.operands[1]));
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 2);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[1]).value ==
          static_cast<std::int64_t>(0x10u << 12));
  }

  // 3) ADD
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 8, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::ADD);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 1);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 2);
  }

  // 4) BEQ (offset 0)
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 12, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::BEQ);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 0);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 0);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 0);
  }
}

TEST_CASE("RV64I more decode", "[decoder]") {
  std::vector<unsigned char> code;

  // SRLI x4, x3, 7
  appendWordLE(code, encodeShiftI(0x00, 7, 3, 0x5, 4, 0x13));
  // SRAI x5, x3, 12
  appendWordLE(code, encodeShiftI(0x20, 12, 3, 0x5, 5, 0x13));
  // LD x6, 8(x1)
  appendWordLE(code, encodeI(8, 1, 0x3, 6, 0x03));
  // SD x6, 24(x2)
  appendWordLE(code, encodeS(24, 6, 2, 0x3, 0x23));
  // BEQ x1, x2, +16
  appendWordLE(code, encodeB(16, 2, 1, 0x0, 0x63));
  // SUB x7, x6, x1
  appendWordLE(code, encodeR(0x20, 1, 6, 0x0, 7, 0x33));
  // ORI x8, x7, 1234
  appendWordLE(code, encodeI(1234, 7, 0x6, 8, 0x13));
  // ECALL, EBREAK
  appendWordLE(code, 0x00000073);
  appendWordLE(code, 0x00100073);

  uint64_t base = 0x2000;
  riscy::SpanMemoryReader mem(base, code.data(), code.size());
  riscy::riscv::Decoder dec;

  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SRLI);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 4);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 3);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 7);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 4, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SRAI);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 5);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 3);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 12);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 8, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::LD);
    REQUIRE(I.operands.size() == 2);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 6);
    CHECK(std::holds_alternative<riscy::riscv::Mem>(I.operands[1]));
    CHECK(std::get<riscy::riscv::Mem>(I.operands[1]).base == 1);
    CHECK(std::get<riscy::riscv::Mem>(I.operands[1]).offset == 8);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 12, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SD);
    REQUIRE(I.operands.size() == 2);
    CHECK(std::holds_alternative<riscy::riscv::Mem>(I.operands[0]));
    CHECK(std::get<riscy::riscv::Mem>(I.operands[0]).base == 2);
    CHECK(std::get<riscy::riscv::Mem>(I.operands[0]).offset == 24);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 6);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 16, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::BEQ);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 1);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 2);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 16);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 20, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SUB);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 7);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 6);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 1);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 24, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::ORI);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 8);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 7);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 1234);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 28, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::ECALL);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 32, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::EBREAK);
  }
}
