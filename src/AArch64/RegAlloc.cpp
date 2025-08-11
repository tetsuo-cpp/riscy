#include "AArch64/RegAlloc.h"
#include <algorithm>

namespace riscy::aarch64 {

RegAssignment RegAlloc::allocate(const Block &b,
                                 const LivenessMap &live) const {
  // Collect intervals
  struct Item {
    VReg v;
    LiveRange lr;
  };
  std::vector<Item> items;
  items.reserve(live.size());
  for (auto &kv : live)
    items.push_back({kv.first, kv.second});
  std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
    return a.lr.start < b.lr.start;
  });

  RegAssignment asg{};
  // Expanded pool: use x1..x28 excluding x0 (arg), x19 (LR save), x21 (mem
  // base), x29 (fp), x30 (lr) Do not include x1 in pool; it is used to pass
  // target PC in indirect branches
  std::vector<PReg> pool = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                            15, 16, 17, 18, 20, 22, 23, 24, 25, 26, 27, 28};
  struct Active {
    PReg p;
    LiveRange lr;
    VReg v;
  };
  std::vector<Active> active;

  auto expire = [&](uint32_t cur) {
    // Return expired physical registers to the pool
    std::vector<PReg> freed;
    active.erase(std::remove_if(active.begin(), active.end(),
                                [&](const Active &a) {
                                  if (a.lr.end <= cur) {
                                    freed.push_back(a.p);
                                    return true;
                                  }
                                  return false;
                                }),
                 active.end());
    for (auto p : freed)
      pool.push_back(p);
  };

  for (const auto &it : items) {
    expire(it.lr.start);
    if (pool.empty()) {
      // Out of registers: fail hard for now so we can diagnose.
      fprintf(stderr,
              "RegAlloc error: out of physical registers; vreg %d cannot be "
              "assigned\n",
              it.v);
      abort();
    } else {
      PReg p = pool.back();
      pool.pop_back();
      asg.v2p[it.v] = p;
      active.push_back({p, it.lr, it.v});
    }
  }
  return asg;
}

} // namespace riscy::aarch64
