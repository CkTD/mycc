union u {
  int a;
  int b;
  struct {
    int array[10];
  } c;
};

int main() {
  union u u1;
  u1.a = 123456;
  int is_same = (&u1.a == &u1.b) && (&u1.a == &u1.c.array[0]);
  printf("%d,%d,%d,%d,", u1.a, u1.b, u1.c.array[0], is_same);

  int *p = u1.c.array, i;
  for (i = 0; i < 10; ++i)
    p[i] = i;

  union u u2, *pu2 = &u2;
  *pu2 = u1;
  printf("%d,%d,", pu2->a, pu2->b);
  for (i = 0; i < 10; i++)
    printf("%d", pu2->c.array[i]);

  return 0;
}