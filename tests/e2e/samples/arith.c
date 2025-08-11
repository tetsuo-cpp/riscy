// Function-style arithmetic: returns ((a0 + a1) << 3) - 5
long test(long a0, long a1) {
  long c = a0 + a1;
  c = (c << 3) - 5;
  return c;
}
