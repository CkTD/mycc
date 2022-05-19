int main() {
  int a = 123;
  int* const p = &a;
  int* const* pp;
  pp = &p;
  printf("%d,%d,%d", a, *p, **pp);
  return 0;
}