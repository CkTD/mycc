int main() {
  char c1;
  c1 = 12;
  char c2;
  c2 = 21;
  char* pc;
  pc = &c1;
  char** ppc;
  ppc = &pc;
  *ppc = &c2;
  int a;
  a = *pc;
  printf("%d\n", a);
  return 0;
}