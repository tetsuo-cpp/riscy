// Minimal arithmetic sample without libc
void _start(void) {
  volatile long a = 1;
  volatile long b = 2;
  volatile long c = 0;
  c = a + b;
  c = (c << 3) - 5;
  (void)c;
}
