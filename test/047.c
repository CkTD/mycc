int main() {
  int a[10], i, sum = 0;
  for (i = 0; i < 10; i = i + 1) {
    a[i] = i;
    int* p = &a[i];
    sum = sum + *p + a[i];
  }
  printf("%d\n", sum);
  return 0;
}