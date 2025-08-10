#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "MemoryReaders.h"
#include "RISCV/CFG.h"
#include "TestUtils.h"

TEST_CASE("CFG: simple branches and jumps", "[cfg]") {
  std::vector<unsigned char> code;
  // Layout (base = 0x1000):
  // 0x1000: ADDI x1, x0, 1
  // 0x1004: BEQ x0, x0, +16   -> 0x1014 (block2)
  // 0x1008: JAL x1, +20       -> 0x101C (block3)
  // 0x100C: (unused)
  // 0x1010: (unused)
  // 0x1014: SUB x2, x1, x0    (block2)
  // 0x1018: ECALL             (trap)
  // 0x101C: ORI x3, x0, 7     (block3)
  // 0x1020: EBREAK            (trap)

  // 0x1000
  code.reserve(4 * 8);
  appendWordLE(code, encodeI(1, 0, 0x0, 1, 0x13)); // ADDI
  // 0x1004: BEQ +16
  appendWordLE(code, encodeB(16, 0, 0, 0x0, 0x63));
  // 0x1008: JAL +20 (imm = 20)
  appendWordLE(code, encodeJ(20, 1, 0x6F));
  // 0x100C, 0x1010: padding
  appendWordLE(code, 0x00000013); // NOP (ADDI x0, x0, 0)
  appendWordLE(code, 0x00000013); // NOP
  // 0x1014: SUB x2, x1, x0
  appendWordLE(code, encodeR(0x20, 0, 1, 0x0, 2, 0x33));
  // 0x1018: ECALL
  appendWordLE(code, 0x00000073);
  // 0x101C: ORI x3, x0, 7
  appendWordLE(code, encodeI(7, 0, 0x6, 3, 0x13));
  // 0x1020: EBREAK
  appendWordLE(code, 0x00100073);

  uint64_t base = 0x1000;
  riscy::SpanMemoryReader mem(base, code.data(), code.size());
  riscy::riscv::CFGBuilder builder;
  riscy::riscv::CFG cfg = builder.build(mem, base);

  // Expect blocks at 0x1000, 0x1014, 0x101C
  REQUIRE(cfg.indexByAddr.count(base) == 1);
  REQUIRE(cfg.indexByAddr.count(base + 0x14) == 1);
  REQUIRE(cfg.indexByAddr.count(base + 0x1C) == 1);

  // Entry block checks
  const auto &b0 = cfg.blocks[cfg.indexByAddr[base]];
  CHECK(b0.start == base);
  REQUIRE(b0.insts.size() >= 2);
  CHECK(b0.term == riscy::riscv::TermKind::Branch);
  REQUIRE(b0.succs.size() == 2);
  // Branch target then fallthrough
  CHECK((b0.succs[0] == base + 0x14));
  CHECK((b0.succs[1] == base + 0x08));

  // Block at 0x1008 should be created due to branch fallthrough leader or
  // explicit jump origin We did not build a separate block starting at 0x1008;
  // JAL terminates the block at 0x1008. Ensure JAL successor
  {
    // Build a block starting at 0x1008 should exist due to fallthrough enqueue
    REQUIRE(cfg.indexByAddr.count(base + 0x08) == 1);
    const auto &b1 = cfg.blocks[cfg.indexByAddr[base + 0x08]];
    CHECK(b1.term == riscy::riscv::TermKind::Jump);
    REQUIRE(b1.succs.size() == 1);
    CHECK(b1.succs[0] == base + 0x1C);
  }

  // Block 0x1014 ends with trap
  {
    const auto &b2 = cfg.blocks[cfg.indexByAddr[base + 0x14]];
    CHECK(b2.term == riscy::riscv::TermKind::Trap);
    CHECK(b2.succs.empty());
  }

  // Block 0x101C ends with trap
  {
    const auto &b3 = cfg.blocks[cfg.indexByAddr[base + 0x1C]];
    CHECK(b3.term == riscy::riscv::TermKind::Trap);
    CHECK(b3.succs.empty());
  }
}
