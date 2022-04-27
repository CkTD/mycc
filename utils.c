#include <execinfo.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error(char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  *((int*)0) = 1;
  exit(1);
}

/*****************************
 * string management         *
 *****************************/
static unsigned long hash(unsigned char* str, unsigned n) {
  unsigned long hash = 5381;

  for (int i = 0; i < n; i++)
    hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

  return hash;
}

typedef struct string* String;
struct string {
  char* s;
  String next;
};

static String bucket[128];

char* stringn(char* s, int n) {
  unsigned h = hash((unsigned char*)s, n) % 128;
  for (String i = bucket[h]; i; i = i->next) {
    if (strcmp(i->s, s) == 0)
      return i->s;
  }

  String new = malloc(sizeof(struct string));
  new->s = calloc(n + 1, sizeof(char));
  strncpy(new->s, s, n);
  new->next = bucket[h];
  bucket[h] = new;

  return new->s;
}

char* string(char* s) {
  return stringn(s, strlen(s));
}