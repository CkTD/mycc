int main() {
  int a = (1, 2);
  printf("%d", a);
  int i, j, sum = 0;
  for (i = 1, j = 1; i = i + 1, j = j + 1, i + j < 10;)
    sum = sum + 1;
  printf("%d", sum);
  return 0;
}