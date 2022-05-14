int main() {
  int i;
  int s;
  i = s = 0;
  for (i = 0; i < 10; i = i + 1)
    s = s + 1;
  printf("%d\n", s);
  return 0;
}