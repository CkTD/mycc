void* malloc(int);

int main() {
  int* buff = (int*)malloc(32 * sizeof(int));
  for (int i = 0; i < 32; i++)
    buff[i] = i + 250;
  for (int i = 0; i < 32; i++) {
    int n = (int)(char)buff[i];
    printf("%d,", n);
  }
  int c;
  (void)c;
  return 0;
}