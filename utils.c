#include <execinfo.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error(char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  *((int *)0) = 1;
  exit(1);
}

// TODO: keep exists string in a hash table
char *stringn(char *s, int n) {
  char *r = malloc(n + 1);
  strncpy(r, s, n);
  r[n] = 0;
  return r;
}

char *string(char *s) { return stringn(s, strlen(s)); }