#include "inc.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char *argv[]) {
  // tokenize
  Token t = tokenize(argv[1]);
  // fprintf(stderr, "Tokens:\n");
  // print_token(t);

  // parse
  Node root = parse(t);
  // fprintf(stderr, "AST:\n");
  // print_ast(root, 0);

  // code generate
  codegen(root);

  exit(0);
}