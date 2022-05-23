int main() {
  int a = -1;
  void* p = &a;
  unsigned* pu = p;
  int r = *pu - 1;
  int* pempty = 0;
  int* pa = &a;
  int eq = pempty == pa;
  printf("%d,%d", r, eq);
  return 0;
}