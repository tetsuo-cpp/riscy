#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RiscyGuestState {
  uint64_t x[32];
  uint8_t *mem;      // guest memory base
  uint64_t mem_size; // size in bytes
} RiscyGuestState;

// External tables provided by emitted assembly
extern const uint64_t riscy_num_blocks;
extern const uint64_t riscy_block_addrs[];
extern void (*const riscy_block_ptrs[])(RiscyGuestState *);
extern const uint64_t riscy_entry_pc;

// Indirect jump entry used by emitted code. x0=state, x1=target PC
void riscy_indirect_jump(RiscyGuestState *st, uint64_t target_pc);
void riscy_trace(uint64_t pc);

// Entry: x0=state, x1=start PC (also provided as asm label riscy_entry)
void riscy_entry(RiscyGuestState *st, uint64_t start_pc);

#ifdef __cplusplus
}
#endif
