#include <filesystem>
#include <iostream>

#include "AArch64/Emitter.h"
#include "AArch64/ISel.h"
#include "ELFImage.h"
#include "IR/IR.h"
#include "MemoryReaders.h"
#include "RISCV/CFG.h"
#include "RISCV/DecodedInst.h"
#include "RISCV/Decoder.h"
#include "RISCV/Lifter.h"
#include "RISCV/Printer.h"
#include <fstream>

int main(int argc, char **argv) {
  bool dumpCfg = false;
  bool dumpIR = false;
  std::string outAsm;
  int argi = 1;
  while (argi < argc && argv[argi][0] == '-') {
    std::string flag = argv[argi];
    if (flag == "--cfg") {
      dumpCfg = true;
    } else if (flag == "--ir") {
      dumpIR = true;
    } else if (flag == "--aarch64") {
      if (argi + 1 >= argc) {
        std::cerr << "--aarch64 requires an output path argument\n";
        return 1;
      }
      outAsm = argv[++argi];
    } else {
      std::cerr << "unknown flag: " << flag << "\n";
      std::cerr
          << "usage: riscy [--cfg] [--ir] [--aarch64 <out.s>] <input-elf>\n";
      return 1;
    }
    ++argi;
  }
  if (argc - argi < 1) {
    std::cerr
        << "usage: riscy [--cfg] [--ir] [--aarch64 <out.s>] <input-elf>\n";
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

  // Collect blocks in address order once
  std::vector<uint64_t> addrs;
  addrs.reserve(cfg.blocks.size());
  for (const auto &kv : cfg.indexByAddr)
    addrs.push_back(kv.first);
  std::sort(addrs.begin(), addrs.end());

  if (dumpCfg || dumpIR) {
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

  if (!outAsm.empty()) {
    // Lower all blocks and emit assembly
    riscy::riscv::Lifter lifter;
    std::vector<riscy::aarch64::Block> blocks;
    std::vector<riscy::aarch64::RegAssignment> assigns;
    blocks.reserve(addrs.size());
    assigns.reserve(addrs.size());
    riscy::aarch64::ISel isel;
    riscy::aarch64::Liveness live;
    riscy::aarch64::RegAlloc ra;
    bool dumpLive = std::getenv("RISCY_DUMP_LIVENESS") != nullptr;
    for (auto a : addrs) {
      const auto &bb = cfg.blocks[cfg.indexByAddr[a]];
      auto irbb = lifter.lift(bb);
      auto blk = isel.select(irbb);
      auto lv = live.analyze(blk);
      if (dumpLive) {
        std::cout << "-- Liveness for block 0x" << std::hex << a << std::dec
                  << " (" << blk.instrs.size() << " instrs)\n";
        for (const auto &kv : lv) {
          std::cout << "  v" << kv.first << ": [" << kv.second.start << ", "
                    << kv.second.end << "]\n";
        }
      }
      auto asg = ra.allocate(blk, lv);
      blocks.push_back(std::move(blk));
      assigns.push_back(std::move(asg));
    }
    riscy::aarch64::Emitter emitter;
    auto mod = emitter.emit(blocks, assigns, image.getEntry());
    std::ofstream os(outAsm);
    if (!os) {
      std::cerr << "failed to open output asm: " << outAsm << "\n";
      return 1;
    }
    os << mod.text;
    std::cout << "wrote AArch64 assembly to " << outAsm << "\n";
  }

  return 0;
}
