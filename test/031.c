int acc(int i) {
  if (i == 1)
    return 1;
  return i + acc(i - 1);
}

int main() {
  printf("%d\n", acc(5));
  return 0;
}