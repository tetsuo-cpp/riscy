// Function-style branches/loops: sum over [0..n)
// if even add i, else subtract i
long test(long n) {
  long sum = 0;
  for (long i = 0; i < n; ++i) {
    if ((i & 1) == 0)
      sum += i;
    else
      sum -= i;
  }
  return sum;
}
