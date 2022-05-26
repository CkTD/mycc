#include "inc.h"

int main(int argc, char* argv[]) {
  Token t = tokenize(argv[1]);

  parse(t);

  codegen();

  return 0;
}