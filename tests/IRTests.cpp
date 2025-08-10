#include "catch2/catch_all.hpp"

#include "IR/IR.h"
#include "RISCV/CFG.h"
#include "RISCV/Lifter.h"

static riscy::riscv::DecodedInst
mkInst(uint64_t pc, riscy::riscv::Opcode op,
       std::initializer_list<riscy::riscv::Operand> ops) {
  riscy::riscv::DecodedInst di{};
  di.pc = pc;
  di.opcode = op;
  di.operands.assign(ops.begin(), ops.end());
  return di;
}

TEST_CASE("Lifter: ADDI + BEQ lowers to IR", "[ir]") {
  // Build a small BB: x5 = x6 + 42; if (x5 == x7) goto T else F
  // Using RV encodings: we directly use decoded form.
  riscy::riscv::BasicBlock bb{};
  bb.start = 0x1000;
  // addi x5, x6, 42
  bb.insts.push_back(mkInst(
      0x1000, riscy::riscv::Opcode::ADDI,
      {riscy::riscv::Reg{5}, riscy::riscv::Reg{6}, riscy::riscv::Imm{42}}));
  // beq x5, x7, +8 (target 0x1008)
  bb.insts.push_back(mkInst(
      0x1004, riscy::riscv::Opcode::BEQ,
      {riscy::riscv::Reg{5}, riscy::riscv::Reg{7}, riscy::riscv::Imm{8}}));
  bb.term = riscy::riscv::TermKind::Branch;
  bb.succs = {0x100c /*t*/, 0x1008 /*f (fallthrough next)*/};

  riscy::riscv::Lifter lifter;
  auto irbb = lifter.lift(bb);

  // Basic sanity on printed form.
  auto s = riscy::ir::toString(irbb);
  INFO(s);
  REQUIRE(irbb.start == 0x1000);
  // Expect at least: readreg x6, const 42, add, writereg x5, readreg x5,
  // readreg x7, icmp eq, cbr
  REQUIRE(irbb.insts.size() >= 7);
  REQUIRE(irbb.term.kind == riscy::ir::TermKind::CBr);
  auto t = std::get<riscy::ir::TermCBr>(irbb.term.data);
  REQUIRE(t.t == 0x100c);
  REQUIRE(t.f == 0x1008);
}

TEST_CASE("Lifter: JALR lowers to br_indirect and ra write", "[ir]") {
  // jalr x1, 0(x10)
  riscy::riscv::BasicBlock bb{};
  bb.start = 0x2000;
  bb.insts.push_back(mkInst(0x2000, riscy::riscv::Opcode::JALR,
                            {riscy::riscv::Reg{1}, riscy::riscv::Mem{10, 0}}));
  bb.term = riscy::riscv::TermKind::IndirectJump;

  riscy::riscv::Lifter lifter;
  auto irbb = lifter.lift(bb);

  auto s = riscy::ir::toString(irbb);
  INFO(s);
  REQUIRE(irbb.term.kind == riscy::ir::TermKind::BrIndirect);
}
