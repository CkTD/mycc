#include "inc.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char *argv[]) {
  Token t = tokenize(argv[1]);

  Node root = parse(t);

  codegen(root);

  exit(0);
}