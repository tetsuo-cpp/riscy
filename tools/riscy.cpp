#include <filesystem>
#include <iostream>

#include "ELFImage.h"
#include "MemoryReaders.h"
#include "RISCV/CFG.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"
#include "RISCV/Printer.h"

int main(int argc, char **argv) {
  bool dumpCfg = false;
  int argi = 1;
  if (argc >= 2 && std::string(argv[1]) == std::string("--cfg")) {
    dumpCfg = true;
    argi++;
  }
  if (argc - argi < 1) {
    std::cerr << "usage: riscy [--cfg] <input-elf>\n";
    return 1;
  }

  std::filesystem::path path = argv[argi];

  riscy::ELFImage image;
  std::string err;
  if (!image.load(path, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  riscy::ElfMemoryReaderAdapter mem(image);
  riscy::riscv::CFGBuilder builder;
  auto cfg = builder.build(mem, image.getEntry());

  if (dumpCfg) {
    // Dump blocks in address order
    std::vector<uint64_t> addrs;
    addrs.reserve(cfg.blocks.size());
    for (const auto &kv : cfg.indexByAddr)
      addrs.push_back(kv.first);
    std::sort(addrs.begin(), addrs.end());
    for (auto a : addrs) {
      const auto &bb = cfg.blocks[cfg.indexByAddr[a]];
      std::cout << "block 0x" << std::hex << bb.start << std::dec << ":\n";
      for (const auto &I : bb.insts) {
        std::cout << "  0x" << std::hex << I.pc << ": 0x" << I.raw << std::dec
                  << "\t" << riscy::riscv::formatInst(I) << "\n";
      }
      std::cout << "  term: ";
      switch (bb.term) {
      case riscy::riscv::TermKind::None:
        std::cout << "none";
        break;
      case riscy::riscv::TermKind::Fallthrough:
        std::cout << "fallthrough";
        break;
      case riscy::riscv::TermKind::Branch:
        std::cout << "branch";
        break;
      case riscy::riscv::TermKind::Jump:
        std::cout << "jump";
        break;
      case riscy::riscv::TermKind::IndirectJump:
        std::cout << "indirect";
        break;
      case riscy::riscv::TermKind::Return:
        std::cout << "ret";
        break;
      case riscy::riscv::TermKind::Trap:
        std::cout << "trap";
        break;
      }
      if (!bb.succs.empty()) {
        std::cout << "; succs: ";
        for (size_t i = 0; i < bb.succs.size(); ++i) {
          if (i)
            std::cout << ", ";
          std::cout << "0x" << std::hex << bb.succs[i] << std::dec;
        }
      }
      std::cout << "\n";
    }
  }

  return 0;
}
