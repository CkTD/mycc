struct s {
  long a;
  int b;
};

int main() {
  struct s s1;
  struct s* ps1 = &s1;
  ps1->a = 123456;
  ps1->b = 123;
  struct s s2;
  struct s* ps2 = &s2;
  *ps2 = *ps1;
  long int* pint = &ps2->a;
  ++*pint;
  printf("%ld,%d,%ld,%d", ps1->a, ps1->b, ps2->a, ps2->b);
  return 0;
}