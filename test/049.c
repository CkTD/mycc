int main() {
  int a[10][10], i, j, k = 0, sum = 0, *p;
  for (i = 0; i < 10; i = i + 1) {
    for (j = 0; j < 10; j = j + 1) {
      p = &a[i][j];
      *p = k;
      k = k + 1;
    }
  }

  for (i = 0; i < 10; i = i + 1)
    for (j = 0; j < 10; j = j + 1)
      sum = sum + a[i][j];
  printf("%d\n", sum);
  return 0;
}