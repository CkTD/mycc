int main() {
  char c, *pc, ac[10];
  short s, *ps, as[10];
  int i, *pi, ai[10];
  long l, *pl, al[10];
  printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", sizeof c, sizeof pc,
         sizeof ac, sizeof s, sizeof ps, sizeof as, sizeof i, sizeof *pi,
         sizeof ai, sizeof l, sizeof pl, sizeof al, sizeof "this is a string",
         sizeof 32);
  return 0;
}