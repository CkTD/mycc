void func(int* i) {
  printf("%d\n", *i);
}

int main() {
  int i;
  i = 12345;
  func(&i);
  return 0;
}