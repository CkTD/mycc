int main() {
  int i;
  int s;
  i = 0;
  s = 0;
  for (; i < 10;) {
    s = s + 1;
    i = i + 1;
  }
  printf("%d\n", s);
  return 0;
}