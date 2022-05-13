#include "inc.h"

int main(int argc, char* argv[]) {
  Token t = tokenize(argv[1]);

  Node root = parse(t);

  codegen(root);

  return 0;
}