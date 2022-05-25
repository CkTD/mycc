enum count {
  FIRST,
  SECOND,
  THIRD,
};

enum haha {
  WHAT = 100,
  A,
  BEAUTIFUL = 0,
  DAY,
};

int func(enum haha c) {
  return c;
}

int main() {
  enum count e1 = 123;
  enum haha c = ++e1;
  printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", FIRST, SECOND, THIRD, WHAT, A,
         BEAUTIFUL, DAY, e1, c, func(123), func(BEAUTIFUL));

  return 0;
}