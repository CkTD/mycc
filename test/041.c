int main() {
  int* p;
  int a;
  a = 0;
  p = &a;
  *p = 123;
  printf("%d\n", a);
  return 0;
}