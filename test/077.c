struct s {
  struct s* pself;
  int a;
};

int main() {
  struct s s1;
  s1.pself = &s1;
  s1.pself->a = 123456;
  printf("%d", s1.a);
  return 0;
}