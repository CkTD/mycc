#include "inc.h"

int main(int argc, char* argv[]) {
  parse_arguments(argc, argv);
  tokenize();
  parse();
  codegen();
  return 0;
}