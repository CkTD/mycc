int main() {
  int a = 0;
  int* p = &a;
  if (a && 1)
    printf("1");
  if (p && a)
    printf("2");
  if (p && 1)
    printf("3");
  if (p && 1 && a)
    printf("4");
  if (p && *p)
    printf("5");
  if (a || 1)
    printf("6");
  if (p || a)
    printf("7");
  if (p || 1)
    printf("8");
  if (p || 1 || a)
    printf("9");
  if (p || *p)
    printf("10");
  if (1 || 0 && 0)
    printf("11");
  if (1 && 0 || 0)
    printf("12");
  if (3 > 2 && 2 > 1)
    printf("13");
  printf("%d", 2 > 3);
  printf("%d", 2 < 3);
  return 0;
}