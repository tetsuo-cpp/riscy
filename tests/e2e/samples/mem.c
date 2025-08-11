// Function-style memory load/store sample: initialize and sum first min(n,8)
long test(long n) {
  static volatile long buf[8];
  for (int i = 0; i < 8; ++i) {
    // Avoid MUL or shifts: express 3*i as i+i+i
    buf[i] = i + i + i;
  }
  long acc = 0;
  int lim = (n < 8) ? (int)n : 8;
  for (int i = 0; i < lim; ++i) {
    acc += buf[i];
  }
  return acc;
}
