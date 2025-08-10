// Minimal branches/loops sample without libc
void _start(void) {
  volatile int sum = 0;
  for (int i = 0; i < 10; ++i) {
    if ((i & 1) == 0)
      sum += i;
    else
      sum -= i;
  }
  (void)sum;
}
