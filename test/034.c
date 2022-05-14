int a;

void pglobala() {
  printf("%d\n", a);
}

int main() {
  int a;
  a = 2;
  printf("%d\n", a);
  pglobala();
  return 0;
}