struct s {
  long a;
  int b[2];
  struct {
    int innerc;
  } c;
};

int main() {
  struct s s1;
  s1.a = 123456;
  s1.b[0] = 123;
  s1.b[1] = 456;
  s1.c.innerc = 789;
  struct s s2 = s1;
  s2.b[0] += s2.b[1];
  printf("%ld,%d,%d,%d,%ld,%d,%d,%d", s1.a, s1.b[0], s1.b[1], s1.c.innerc, s2.a,
         s2.b[0], s2.b[1], s2.c.innerc);
  return 0;
}