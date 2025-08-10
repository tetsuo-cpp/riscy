#include <filesystem>
#include <iostream>

#include "ELFImage.h"
#include "IR/IR.h"
#include "MemoryReaders.h"
#include "RISCV/CFG.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"
#include "RISCV/Lifter.h"
#include "RISCV/Printer.h"

int main(int argc, char **argv) {
  bool dumpCfg = false;
  bool dumpIR = false;
  int argi = 1;
  while (argi < argc && argv[argi][0] == '-') {
    std::string flag = argv[argi];
    if (flag == "--cfg") {
      dumpCfg = true;
    } else if (flag == "--ir") {
      dumpIR = true;
    } else {
      std::cerr << "unknown flag: " << flag << "\n";
      std::cerr << "usage: riscy [--cfg] [--ir] <input-elf>\n";
      return 1;
    }
    ++argi;
  }
  if (argc - argi < 1) {
    std::cerr << "usage: riscy [--cfg] [--ir] <input-elf>\n";
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

  if (dumpCfg || dumpIR) {
    // Dump blocks in address order
    std::vector<uint64_t> addrs;
    addrs.reserve(cfg.blocks.size());
    for (const auto &kv : cfg.indexByAddr)
      addrs.push_back(kv.first);
    std::sort(addrs.begin(), addrs.end());
    for (auto a : addrs) {
      const auto &bb = cfg.blocks[cfg.indexByAddr[a]];
      if (dumpCfg) {
        std::cout << riscy::riscv::formatBlock(bb);
      }
      if (dumpIR) {
        riscy::riscv::Lifter lifter;
        auto irbb = lifter.lift(bb);
        std::cout << riscy::ir::toString(irbb);
      }
    }
  }

  return 0;
}
