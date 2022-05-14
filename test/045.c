int main() {
  int a = 12345, b, *p;
  b = a;
  p = &b;
  printf("%d\n", *p);
  return 0;
}