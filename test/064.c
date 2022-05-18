int main() {
  int a = 0;
  int b = a++;
  int c = a++ + b++;
  int d = a-- + b-- + c--;
  int x[5];
  int i;
  for (i = 0; i < 5; i++)
    x[i] = i;

  int* p = x;
  int* q = p++;

  printf("%d,%d,%d,%d", a, b, c++, d--, *p++, *q--);
  return 0;
}