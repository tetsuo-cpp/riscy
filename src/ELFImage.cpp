#include "ELFImage.h"

namespace riscy {

bool ELFImage::load(const std::filesystem::path &path, std::string &err) {
  loaded = false;
  err.clear();
  execSections.clear();
  if (!reader.load(path)) {
    err = "Failed to load ELF file: " + path.string();
    return false;
  }

  if (reader.get_class() != ELFIO::ELFCLASS64) {
    err = "Unsupported ELF class (need ELF64)";
    return false;
  }
  if (reader.get_machine() != ELFIO::EM_RISCV) {
    err = "Unsupported machine (need RISC-V)";
    return false;
  }
  // Optional: ensure little-endian
  if (reader.get_encoding() != ELFIO::ELFDATA2LSB) {
    err = "Unsupported endianness (need little-endian)";
    return false;
  }

  entry = reader.get_entry();

  // Collect executable sections with SHF_EXECINSTR
  for (const auto &secPtr : reader.sections) {
    const ELFIO::section *sec = secPtr.get();
    if (!sec)
      continue;
    auto flags = sec->get_flags();
    if ((flags & ELFIO::SHF_EXECINSTR) == 0)
      continue;
    const char *data = sec->get_data();
    if (data == nullptr)
      continue;
    SectionSpan span;
    span.va = sec->get_address();
    span.size = static_cast<size_t>(sec->get_size());
    span.data = data;
    execSections.push_back(span);
  }

  if (execSections.empty()) {
    err = "No executable sections found";
    return false;
  }

  loaded = true;
  return true;
}

bool ELFImage::read(uint64_t va, void *dst, size_t n) const {
  if (!loaded)
    return false;
  for (const auto &s : execSections) {
    if (va < s.va)
      continue;
    uint64_t end = s.va + s.size;
    if (va + n > end)
      continue;
    size_t off = static_cast<size_t>(va - s.va);
    std::memcpy(dst, s.data + off, n);
    return true;
  }
  return false;
}

} // namespace riscy
