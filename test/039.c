int main() {
  short x;
  x = 65535;
  int xx;
  xx = x;
  printf("%d\n", xx);

  unsigned int y;
  y = 4294967295;
  int yy;
  yy = y;
  printf("%d\n", yy);

  long z;
  z = 0 - 1;
  int zz;
  zz = z;
  printf("%d\n", zz);
  return 0;
}