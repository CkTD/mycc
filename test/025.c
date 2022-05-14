int main() {
  int a;
  int b;
  a = 0;
  b = 0;
  while (a < 5) {
    a = a + 1;
    if (a == 2)
      break;
    b = b + 1;
  }
  printf("%d\n", b);
  return 0;
}