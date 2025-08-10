// Minimal memory load/store sample without libc
void _start(void) {
  static volatile long buf[8];
  for (int i = 0; i < 8; ++i) {
    buf[i] = i * 3;
  }
  volatile long acc = 0;
  for (int i = 0; i < 8; ++i) {
    acc += buf[i];
  }
  (void)acc;
}
