int main() {
  int a[10], i, *p, sum = 0;
  p = a;
  for (i = 0; i < 10; i = i + 1) {
    *(p + i) = i;
    sum = sum + a[i];
  }
  printf("%d\n", sum);
  return 0;
}