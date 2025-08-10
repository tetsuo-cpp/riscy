#pragma once

#include "RISCV/CFG.h"
#include "RISCV/DecodedInst.h"

namespace riscy::riscv {

const char *opcodeName(Opcode op);
std::string formatOperand(const Operand &op);
std::string formatInst(const DecodedInst &inst);
std::string formatBlock(const BasicBlock &bb);

} // namespace riscy::riscv
