int func();
int func(int[], int);

int func(int* p, int index) {
  return p[index] + *(p + index);
}

int func2(int a[], int index) {
  return a[index];
}

void print(int n) {
  printf("%d,", n);
}

int main() {
  int a[10];
  a[5] = 5;
  print(func(a, 5));
  print(func(a, 5));
  print(get1());
  return 0;
}

int get1(void) {
  return 1;
}