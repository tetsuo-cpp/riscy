#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <elfio/elfio.hpp>

namespace riscy {

// Thin executable image backed by ELFIO executable sections.
class ELFImage {
public:
  ELFImage() = default;

  bool load(const std::filesystem::path &path, std::string &err);

  uint64_t getEntry() const { return entry; }
  bool isLoaded() const { return loaded; }

  // Read n bytes from VA into Dst. Returns false if address is unmapped or OOB.
  bool read(uint64_t va, void *dst, size_t n) const;

private:
  struct SectionSpan {
    uint64_t va = 0;
    size_t size = 0;
    const char *data = nullptr; // pointer owned by ELFIO reader
  };

  ELFIO::elfio reader;
  std::vector<SectionSpan> execSections;
  uint64_t entry = 0;
  bool loaded = false;
};

// Adapter providing 32-bit reads for the decoder.
class ELFMemoryReader {
public:
  explicit ELFMemoryReader(const ELFImage &img) : img(img) {}

  bool read32(uint64_t addr, uint32_t &out) const {
    unsigned char buf[4];
    if (!img.read(addr, buf, sizeof(buf)))
      return false;
    // Little-endian assemble
    out = static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
          (static_cast<uint32_t>(buf[2]) << 16) |
          (static_cast<uint32_t>(buf[3]) << 24);
    return true;
  }

private:
  const ELFImage &img;
};

} // namespace riscy
