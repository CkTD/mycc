int main() {
  int (*apfi[3])(int*, int*);
  int (*fpfi(int (*)(long), int))(int, ...);

  printf("%d,%d", sizeof(int (*[3])(int*, int*)),
         sizeof(int (*(*)(int (*)(long), int))(int, ...)));
  return 0;
}