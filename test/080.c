typedef struct s {
  int a;
  int b;
} s;
typedef int TINT, *TPINT;
typedef TINT TAOFINT5[5];

int main() {
  TINT a;
  TPINT p = &a;
  *p = 123456;
  printf("%d,", a);

  TAOFINT5 a5;
  int i;
  for (i = 0; i < 5; i++)
    a5[i] = i;
  for (i = 0; i < 5; i++)
    printf("%d,", a5[i]);

  s s;
  struct s s1;
  s.a = 123;
  s.b = 456;
  s1.a = 123;
  s1.b = 456;
  printf("%d,%d,", s.a, s.b, s1.a, s1.b);

  typedef TPINT TT;
  TT pp;
  pp = a5 + 2;
  for (i = 0; i < 3; i++)
    printf("%d,", pp[i]);
  return 0;
}