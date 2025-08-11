#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

// Simple linear search jump table. Optimize later.
static int find_block(uint64_t pc) {
  for (uint64_t i = 0; i < riscy_num_blocks; ++i) {
    if (riscy_block_addrs[i] == pc) return (int)i;
  }
  return -1;
}

void riscy_indirect_jump(RiscyGuestState* st, uint64_t target_pc) {
  int idx = find_block(target_pc);
  printf("ijump target=0x%llx idx=%d\n", (unsigned long long)target_pc, idx);
  fflush(stdout);
  if (idx < 0) {
    __asm__ __volatile__("brk #0");
    return;
  }
  void (*fn)(RiscyGuestState*) = riscy_block_ptrs[idx];
  fn(st);
  printf("ijump return from idx=%d\n", idx);
  fflush(stdout);
}

void riscy_trace(uint64_t pc) {
  printf("trace pc=0x%llx\n", (unsigned long long)pc);
  fflush(stdout);
}

// Standalone entry point to run translated code
static int parse_u64(const char *s, uint64_t *out) {
  if (!s || !out) return -1;
  // Allow 0x... hex or decimal
  char *end = NULL;
  int base = 10;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
  unsigned long long v = strtoull(s, &end, base);
  if (!end || *end != '\0') return -1;
  *out = (uint64_t)v;
  return 0;
}

int main(int argc, char **argv) {
  RiscyGuestState st = {0};
  // Allocate 64MB guest memory (enough to cover typical small program address ranges)
  unsigned long mem_sz = 64 * 1024 * 1024;
  uint8_t *mem = (uint8_t *)malloc(mem_sz);
  st.mem_size = mem_sz;
  // Adjust mem base so host EA = mem + (guest_addr - image_base)
  uint64_t image_base = riscy_block_addrs[0];
  st.mem = (uint8_t *)((uintptr_t)mem - image_base);
  // Initialize guest stack pointer and frame pointer to top of our buffer
  st.x[2] = image_base + (mem_sz - 0x4000); // sp, leave 16KB stack
  st.x[8] = st.x[2];                         // fp/s0
  // Start at ELF entry point provided by emitted module
  uint64_t start_pc = riscy_entry_pc;
  int verbose = 0;
  // Optional: allow choosing which regs to dump after execution
  int dump_regs[32];
  int dump_count = 0;
  for (int i = 0; i < 32; ++i) dump_regs[i] = -1;
  int next_arg_to_a = 0; // positional args become a0..a7
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--verbose") == 0) { verbose = 1; continue; }
    if (strncmp(argv[i], "--dump=", 7) == 0) {
      const char *list = argv[i] + 7;
      const char *p = list;
      while (*p) {
        while (*p == ',' || *p == ' ') p++;
        if (!*p) break;
        const char *q = p;
        while (*q && *q != ',' && *q != ' ') q++;
        char regname[8];
        size_t len = (size_t)(q - p);
        if (len == 0 || len >= sizeof(regname)) {
          fprintf(stderr, "bad --dump token near: %s\n", p);
          return 2;
        }
        memcpy(regname, p, len);
        regname[len] = '\0';
        // support xN or aN (a0..a7 -> x10..x17)
        int rx = -1;
        if (regname[0] == 'x' || regname[0] == 'X') {
          char *end = NULL; long v = strtol(regname + 1, &end, 10);
          if (end && *end == '\0' && v >= 0 && v < 32) rx = (int)v;
        } else if (regname[0] == 'a' || regname[0] == 'A') {
          char *end = NULL; long v = strtol(regname + 1, &end, 10);
          if (end && *end == '\0' && v >= 0 && v <= 7) rx = (int)(10 + v);
        }
        if (rx < 0) {
          fprintf(stderr, "unknown register in --dump: %s\n", regname);
          return 2;
        }
        if (dump_count < 32) dump_regs[dump_count++] = rx;
        p = q;
      }
      continue;
    }
    // Positional numeric: assign to a0..a7
    uint64_t v = 0;
    if (parse_u64(argv[i], &v) == 0) {
      if (next_arg_to_a < 8) {
        st.x[10 + next_arg_to_a] = v;
        next_arg_to_a++;
      }
      continue;
    }
    // unrecognized args ignored for forward-compat
  }
  if (verbose) {
    printf(
        "runner: start_pc=0x%llx image_base=0x%llx blocks=%llu sp=0x%llx\n",
        (unsigned long long)start_pc, (unsigned long long)image_base,
        (unsigned long long)riscy_num_blocks, (unsigned long long)st.x[2]);
    fflush(stdout);
  }
  // Call the translated code
  riscy_entry(&st, start_pc);
  // Always print the architectural return value (a0 = x10) as signed 64-bit
  printf("RET %" PRId64 "\n", (int64_t)st.x[10]);
  for (int i = 0; i < dump_count; ++i) {
    int rx = dump_regs[i];
    if (rx >= 0 && rx < 32) {
      // Structured line for tests to parse easily
      printf("OUT x%d=%" PRIu64 "\n", rx, (uint64_t)st.x[rx]);
    }
  }
  return 0;
}
