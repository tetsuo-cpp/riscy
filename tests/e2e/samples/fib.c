// Function-style Fibonacci: iterative to stay in base ISA
long test(long n) {
  if (n <= 1) return n;
  long a = 0;
  long b = 1;
  for (long i = 2; i <= n; ++i) {
    long c = a + b;
    a = b;
    b = c;
  }
  return b;
}

