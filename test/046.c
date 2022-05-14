char ca = 1, cb;
int ia, ib = 2;
short sa, sb = 3;
long la, lb;
int* p;
int main() {
  cb = ca;
  ia = ib;
  sa = sb;
  la = lb = 4;
  int z = ca + ia + sa + la;
  p = &z;
  printf("%d\n", *p);
  return 0;
}
