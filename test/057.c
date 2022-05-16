int main() {
  int a = 3;
  char b = 5;
  printf("%d,%d,%d", a | b, a ^ b, a & b);
  printf("%d,%d,%d", a | b & b, a | b ^ b, a ^ b & b);
  return 0;
}