#include "IR/IR.h"

#include <sstream>

namespace riscy::ir {

static inline const char *tyStr(TypeKind k) {
  switch (k) {
  case TypeKind::I1:
    return "i1";
  case TypeKind::I8:
    return "i8";
  case TypeKind::I16:
    return "i16";
  case TypeKind::I32:
    return "i32";
  case TypeKind::I64:
    return "i64";
  }
  return "i64";
}

static inline const char *binopStr(BinOpKind k) {
  switch (k) {
  case BinOpKind::Add:
    return "add";
  case BinOpKind::Sub:
    return "sub";
  case BinOpKind::And:
    return "and";
  case BinOpKind::Or:
    return "or";
  case BinOpKind::Xor:
    return "xor";
  case BinOpKind::Shl:
    return "shl";
  case BinOpKind::LShr:
    return "lshr";
  case BinOpKind::AShr:
    return "ashr";
  }
  return "binop";
}

static inline const char *icmpStr(ICmpCond c) {
  switch (c) {
  case ICmpCond::EQ:
    return "eq";
  case ICmpCond::NE:
    return "ne";
  case ICmpCond::ULT:
    return "ult";
  case ICmpCond::ULE:
    return "ule";
  case ICmpCond::UGT:
    return "ugt";
  case ICmpCond::UGE:
    return "uge";
  case ICmpCond::SLT:
    return "slt";
  case ICmpCond::SLE:
    return "sle";
  case ICmpCond::SGT:
    return "sgt";
  case ICmpCond::SGE:
    return "sge";
  }
  return "icmp";
}

std::string toString(const Block &bb) {
  std::ostringstream os;
  os << "block @0x" << std::hex << bb.start << std::dec << "\n";
  auto printValue = [&](ValueId v) { os << "%" << v; };

  for (const auto &ins : bb.insts) {
    if (ins.dest)
      os << "%" << *ins.dest << " = ";
    std::visit(
        [&](auto &&node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, Const>) {
            os << "const " << tyStr(node.ty.kind) << " " << node.value;
          } else if constexpr (std::is_same_v<T, ReadReg>) {
            os << "readreg x" << unsigned(node.reg);
          } else if constexpr (std::is_same_v<T, WriteReg>) {
            os << "writereg x" << unsigned(node.reg) << ", ";
            printValue(node.value);
          } else if constexpr (std::is_same_v<T, BinOp>) {
            os << binopStr(node.kind) << " " << tyStr(node.ty.kind) << " ";
            printValue(node.lhs);
            os << ", ";
            printValue(node.rhs);
          } else if constexpr (std::is_same_v<T, ICmp>) {
            os << "icmp " << icmpStr(node.cond) << " ";
            printValue(node.lhs);
            os << ", ";
            printValue(node.rhs);
          } else if constexpr (std::is_same_v<T, ZExt>) {
            os << "zext ";
            printValue(node.src);
            os << " to " << tyStr(node.to.kind);
          } else if constexpr (std::is_same_v<T, SExt>) {
            os << "sext ";
            printValue(node.src);
            os << " to " << tyStr(node.to.kind);
          } else if constexpr (std::is_same_v<T, Trunc>) {
            os << "trunc ";
            printValue(node.src);
            os << " to " << tyStr(node.to.kind);
          } else if constexpr (std::is_same_v<T, Load>) {
            os << "load " << tyStr(node.ty.kind) << ", base=";
            printValue(node.base);
            os << ", off=" << node.offset;
          } else if constexpr (std::is_same_v<T, Store>) {
            os << "store " << tyStr(node.ty.kind) << ", ";
            printValue(node.value);
            os << ", base=";
            printValue(node.base);
            os << ", off=" << node.offset;
          } else if constexpr (std::is_same_v<T, GetPC>) {
            os << "get_pc";
          }
        },
        ins.payload);
    os << "\n";
  }

  os << "  term ";
  switch (bb.term.kind) {
  case TermKind::None:
    os << "none";
    break;
  case TermKind::Trap:
    os << "trap";
    break;
  case TermKind::Ret:
    os << "ret";
    break;
  case TermKind::Br: {
    const auto &t = std::get<TermBr>(bb.term.data);
    os << "br @0x" << std::hex << t.target << std::dec;
    break;
  }
  case TermKind::CBr: {
    const auto &t = std::get<TermCBr>(bb.term.data);
    os << "cbr ";
    printValue(t.cond);
    os << ", @0x" << std::hex << t.t << ", @0x" << t.f << std::dec;
    break;
  }
  case TermKind::BrIndirect: {
    const auto &t = std::get<TermBrIndirect>(bb.term.data);
    os << "br_indirect ";
    printValue(t.target);
    break;
  }
  }
  os << "\n";

  return os.str();
}

} // namespace riscy::ir
