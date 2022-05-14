int a;
int func(int a, int b) {
  {
    int a;
    a = 3;
    printf("%d\n", (a));
  }
  printf("%d\n", (a));
}

int main() {
  printf("%d\n", (a));
  func(1, 2);
  return 0;
}