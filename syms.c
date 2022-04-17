#include "inc.h"
#include "string.h"

#define NSYMBOLS 1024

static struct symbol symtab[NSYMBOLS];
static int next = 0;

void mksym(char *s) {
  symtab[next++].name = s;
  genglobalsym(s);
}

Symbol findsym(char *s) {
  for (int i = 0; i < next; i++) {
    if (strcmp(s, symtab[i].name) == 0) {
      return &symtab[i];
    }
  }

  return NULL;
}