#pragma once

#include <cstddef>
#include <cstdint>

#include "ELFImage.h"

namespace riscy {

class MemoryReader {
public:
  virtual ~MemoryReader() = default;
  virtual bool read32(uint64_t addr, uint32_t &out) const = 0;
};

// Simple span-backed reader for tests and buffers
class SpanMemoryReader : public MemoryReader {
public:
  SpanMemoryReader(uint64_t baseAddr, const unsigned char *data, size_t size)
      : base(baseAddr), ptr(data), len(size) {}

  bool read32(uint64_t addr, uint32_t &out) const override {
    if (addr < base)
      return false;
    uint64_t off = addr - base;
    if (off + 4 > len)
      return false;
    const unsigned char *p = ptr + off;
    out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
    return true;
  }

private:
  uint64_t base;
  const unsigned char *ptr;
  size_t len;
};

// Adapter for using ElfImage with the decoder interface
class ElfMemoryReaderAdapter : public MemoryReader {
public:
  explicit ElfMemoryReaderAdapter(const riscy::ELFImage &img) : elf(img) {}
  bool read32(uint64_t addr, uint32_t &out) const override {
    return elf.read32(addr, out);
  }

private:
  riscy::ELFMemoryReader elf;
};

} // namespace riscy
