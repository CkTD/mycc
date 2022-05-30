int main() {
  unsigned int a = 0x0fffffffu;
  unsigned long b = 0X01234567890abcdeful;
  unsigned long c = 01234567lu;

  printf("%x,%lx,%lo,%u,%u,%u,%u", a, b, c, sizeof 0x80000000,
         sizeof 2147483648, sizeof 2147483648u, sizeof 0x80000000l);
  return 0;
}