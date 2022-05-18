int main() {
  char x1;
  signed char x2;
  char signed x3;
  unsigned char x4;
  char unsigned x5;
  short x6;
  signed short x7;
  signed short int x8;
  unsigned long long int x9;
  long int unsigned long x10;
  printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", sizeof x1, sizeof x2, sizeof x3,
         sizeof x4, sizeof x5, sizeof x6, sizeof x7, sizeof x8, sizeof x9,
         sizeof x10);
  return 0;
}