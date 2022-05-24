struct sglobal;
struct sglobal {
  char a;
  long b;
};
struct sglobal;

struct sscope {
  int a;
};

int main() {
  struct sscope {
    int a;
    int b;
    int c;
  };

  struct snested {
    char b;
    struct inner {
      int c;
      int d;
    } c;
  };

  return 0;
}