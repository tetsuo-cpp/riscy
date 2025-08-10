#pragma once

#include "MemoryReaders.h"
#include "RISCV/DecodedInst.h"

namespace riscy::riscv {

enum class DecodeError {
  None,
  MisalignedPC,
  OOBRead,
  InvalidOpcode,
};

class Decoder {
public:
  bool decodeNext(const MemoryReader &mem, uint64_t pc, DecodedInst &outInst,
                  DecodeError &outErr) const;

private:
  static inline uint32_t getBits(uint32_t x, unsigned hi, unsigned lo) {
    const uint32_t mask = (hi == 31 && lo == 0)
                              ? 0xFFFFFFFFu
                              : (((1u << (hi - lo + 1)) - 1u) << lo);
    return (x & mask) >> lo;
  }
  static inline int64_t sext(int64_t x, unsigned bits) {
    const int64_t m = 1LL << (bits - 1);
    return (x ^ m) - m;
  }
};

} // namespace riscy::riscv
