int main() {
  const int a = 1;
  int b = 2;
  const int* pa = &a;
  int* pb = &b;
  printf("%d", a, b, *pa, *pb);
  return 0;
}