int main() {
  const int a = 1;
  int b = 2;
  const int* pa = &a;
  int* pa2 = &a;
  int* pb = &b;
  printf("%d", a, b, *pa, *pa2, *pb);
  return 0;
}