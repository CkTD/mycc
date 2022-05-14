int somefunc(int x, int y, char z);

int func(int x);

int func(int x) {
  return x + 1;
}
int main() {
  printf("%d\n", func(1));
  return 0;
}