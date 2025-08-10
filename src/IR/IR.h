#pragma once

#include <vector>

namespace riscy::ir {

enum class TypeKind { I1, I8, I16, I32, I64 };

struct Type {
  TypeKind kind = TypeKind::I64;
  static inline Type i1() { return {TypeKind::I1}; }
  static inline Type i8() { return {TypeKind::I8}; }
  static inline Type i16() { return {TypeKind::I16}; }
  static inline Type i32() { return {TypeKind::I32}; }
  static inline Type i64() { return {TypeKind::I64}; }
};

using ValueId = uint32_t; // virtual register id local to a block

struct Const {
  Type ty{};
  uint64_t value = 0; // use unsigned container; signedness handled by ops
};

struct ReadReg {
  uint8_t reg = 0; // guest reg index (RISC-V xN)
};

struct WriteReg {
  uint8_t reg = 0;
  ValueId value = 0;
};

enum class BinOpKind {
  Add,
  Sub,
  And,
  Or,
  Xor,
  Shl,
  LShr,
  AShr,
};

struct BinOp {
  BinOpKind kind{};
  ValueId lhs = 0;
  ValueId rhs = 0;
  Type ty{};
};

enum class ICmpCond {
  EQ,
  NE,
  ULT,
  ULE,
  UGT,
  UGE,
  SLT,
  SLE,
  SGT,
  SGE,
};

struct ICmp {
  ICmpCond cond{};
  ValueId lhs = 0;
  ValueId rhs = 0;
};

struct ZExt {
  ValueId src = 0;
  Type to{};
};

struct SExt {
  ValueId src = 0;
  Type to{};
};

struct Trunc {
  ValueId src = 0;
  Type to{};
};

struct Load {
  // addr = base + offset
  ValueId base = 0;
  int64_t offset = 0;
  Type ty{}; // size of load result
};

struct Store {
  ValueId value = 0;
  ValueId base = 0;
  int64_t offset = 0;
  Type ty{}; // size of store source
};

struct GetPC {};

// Generic instruction payloads. dest is optional; non-producing ops
// (WriteReg/Store) don't define a dest.
struct Instr {
  std::optional<ValueId> dest{};
  std::variant<Const, ReadReg, WriteReg, BinOp, ICmp, ZExt, SExt, Trunc, Load,
               Store, GetPC>
      payload{};
};

enum class TermKind {
  None,
  Br,         // unconditional direct
  CBr,        // conditional branch
  BrIndirect, // indirect jump by value
  Ret,        // return to caller
  Trap,       // ecall/ebreak or invalid
};

struct TermBr {
  uint64_t target = 0; // guest PC
};

struct TermCBr {
  ValueId cond = 0; // i1
  uint64_t t = 0;
  uint64_t f = 0;
};

struct TermBrIndirect {
  ValueId target = 0; // i64 target PC
};

struct Terminator {
  TermKind kind = TermKind::None;
  std::variant<std::monostate, TermBr, TermCBr, TermBrIndirect> data{};
};

struct Block {
  uint64_t start = 0;
  std::vector<Instr> insts;
  Terminator term;
};

// A simple printer for tests/debug
std::string toString(const Block &bb);

} // namespace riscy::ir
