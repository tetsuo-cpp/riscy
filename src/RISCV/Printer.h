#pragma once

#include "RISCV/DecodedInst.h"

namespace riscy::riscv {

const char *opcodeName(Opcode op);
std::string formatOperand(const Operand &op);
std::string formatInst(const DecodedInst &inst);

} // namespace riscy::riscv
