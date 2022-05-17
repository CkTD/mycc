int main() {
  int a = 0;
  int b = ++a;
  int c = ++a + ++b;
  int d = --a;
  int e = --a + --d;
  printf("%d,%d,%d,%d,%d,%d", a, b, + + + ++c, d, - - - --e);
  return 0;
}