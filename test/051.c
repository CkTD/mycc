int main() {
  int a[3], *p;
  a[0] = 1;
  a[1] = 2;
  a[2] = 3;
  p = &a[1];
  printf("%d\n", p[0 - 1]);
  return 0;
}