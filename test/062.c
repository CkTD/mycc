int main() {
  int a = 3332211;
  int x1, x2, x3, x4, x5, x6, x7, x8, x9;
  x1 = a *= 30, x2 = a /= 30, x3 = a += 30, x4 = a -= 30, x5 = a <<= 2,
  x6 = a >>= 2, x7 = a &= 15, x8 = a ^= 3, x9 = a |= 3;
  printf("%d,%d,%d,%d,%d,%d,%d,%d,%d", x1, x2, x3, x4, x5, x6, x7, x8, x9);
  return 0;
}