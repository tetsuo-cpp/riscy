#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "MemoryReaders.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"
#include "TestUtils.h"

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

TEST_CASE("RV64I W-ops decode", "[decoder]") {
  std::vector<unsigned char> code;

  // ADDIW x5, x4, 16
  appendWordLE(code, encodeI(16, 4, 0x0, 5, 0x1B));
  // SLLIW x6, x5, 7
  appendWordLE(code, encodeShiftI(0x00, 7, 5, 0x1, 6, 0x1B));
  // SRLIW x7, x6, 3
  appendWordLE(code, encodeShiftI(0x00, 3, 6, 0x5, 7, 0x1B));
  // SRAIW x8, x7, 12
  appendWordLE(code, encodeShiftI(0x20, 12, 7, 0x5, 8, 0x1B));
  // ADDW x9, x7, x6
  appendWordLE(code, encodeR(0x00, 6, 7, 0x0, 9, 0x3B));
  // SUBW x10, x9, x5
  appendWordLE(code, encodeR(0x20, 5, 9, 0x0, 10, 0x3B));
  // SLLW x11, x10, x4
  appendWordLE(code, encodeR(0x00, 4, 10, 0x1, 11, 0x3B));
  // SRLW x12, x11, x3
  appendWordLE(code, encodeR(0x00, 3, 11, 0x5, 12, 0x3B));
  // SRAW x13, x12, x2
  appendWordLE(code, encodeR(0x20, 2, 12, 0x5, 13, 0x3B));

  uint64_t base = 0x3000;
  riscy::SpanMemoryReader mem(base, code.data(), code.size());
  riscy::riscv::Decoder dec;

  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::ADDIW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 5);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 4);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 16);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 4, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SLLIW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 6);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 5);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 7);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 8, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SRLIW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 7);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 6);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 3);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 12, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SRAIW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 8);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 7);
    CHECK(std::get<riscy::riscv::Imm>(I.operands[2]).value == 12);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 16, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::ADDW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 9);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 7);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 6);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 20, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SUBW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 10);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 9);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 5);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 24, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SLLW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 11);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 10);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 4);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 28, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SRLW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 12);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 11);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 3);
  }
  {
    riscy::riscv::DecodedInst I{};
    riscy::riscv::DecodeError E;
    REQUIRE(dec.decodeNext(mem, base + 32, I, E));
    CHECK(I.opcode == riscy::riscv::Opcode::SRAW);
    REQUIRE(I.operands.size() == 3);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[0]).index == 13);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[1]).index == 12);
    CHECK(std::get<riscy::riscv::Reg>(I.operands[2]).index == 2);
  }
}
