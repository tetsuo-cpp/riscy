#include <filesystem>
#include <iostream>

#include "ELFImage.h"
#include "MemoryReaders.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"
#include "RISCV/Printer.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: riscy <input-elf>\n";
    return 1;
  }

  std::filesystem::path path = argv[1];

  riscy::ELFImage image;
  std::string err;
  if (!image.load(path, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  riscy::ElfMemoryReaderAdapter mem(image);
  riscy::riscv::Decoder dec;

  for (uint64_t pc = image.getEntry();; pc += 4) {
    riscy::riscv::DecodedInst inst;
    riscy::riscv::DecodeError errCode;
    if (!dec.decodeNext(mem, pc, inst, errCode)) {
      if (errCode == riscy::riscv::DecodeError::OOBRead) {
        // Treat out-of-bounds as EOF; stop silently.
        break;
      }
      std::cout << std::hex << pc << ": <decode error>" << std::dec << "\n";
      break;
    }
    std::cout << std::hex << inst.pc << ": 0x" << inst.raw << std::dec << "\t"
              << riscy::riscv::formatInst(inst) << "\n";
  }

  return 0;
}
