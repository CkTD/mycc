int i = 3210;
int main() {
  printf("%d,", i);
  int i = 123456;
  for (int i = 0, j = 0; i + j < 20; i++, j++)
    printf("%d,", i);
  printf("%d,", i);
  return 0;
}