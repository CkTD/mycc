int main() {
  int a;
  int b;
  int c;
  a = 0;
  b = 0;
  while (a < 5) {
    a = a + 1;
    c = 0;
    while (1) {
      c = c + 1;
      if (c == a)
        break;
    }
    b = b + c;
  }
  printf("%d\n", b);
  return 0;
}