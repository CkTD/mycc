int main() {
  int a;
  int b;
  a = 0;
  b = 0;
  do {
    a = a + 1;
    if (a == 2)
      continue;
    b = b + 1;
  } while (a < 5);
  printf("%d\n", b);
  return 0;
}