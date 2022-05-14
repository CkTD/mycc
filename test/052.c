char* gc = "a string literal";
int main() {
  char* c = "abcdefg";
  int z = c[3];
  printf("%d\n", z);
  c = gc;
  z = *(gc + 7);
  printf("%d\n", z);
  return 0;
}