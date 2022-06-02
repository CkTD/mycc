#include "inc.h"

static void msg(char* kind, char* fmt, va_list ap) {
  fprintf(stderr, "%s: ", kind);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
}

static void msgat(char* kind, Token tok, char* fmt, va_list ap) {
  fprintf(stderr, "%s:%d:%d: %s: ", options.input_filename, tok->line_no,
          tok->char_no + 1, kind);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");

  fprintf(stderr, "    ");
  for (const char* p = tok->line; *p && *p != '\n'; ++p)
    fputc(*p, stderr);
  fprintf(stderr, "\n");
  fprintf(stderr, "    %*s%s\n", tok->char_no, "", "^");
}

void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  msg("error", fmt, ap);
  va_end(ap);
  *((int*)0) = 1;
}

void warn(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  msg("warn", fmt, ap);
  va_end(ap);
}

void info(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  msg("info", fmt, ap);
  va_end(ap);
}

void errorat(Token tok, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  msgat("error", tok, fmt, ap);
  va_end(ap);
  *((int*)0) = 1;
}
void warnat(Token tok, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  msgat("warn", tok, fmt, ap);
  va_end(ap);
}
void infoat(Token tok, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  msgat("info", tok, fmt, ap);
  va_end(ap);
}

static unsigned long hash(unsigned char* str, unsigned n) {
  unsigned long hash = 5381;

  for (int i = 0; i < n; i++)
    hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

  return hash;
}

#define STSIZE 128
static struct string {
  char* s;
  struct string* next;
} * string_table[STSIZE];

const char* stringn(const char* s, int n) {
  unsigned h = hash((unsigned char*)s, n) % STSIZE;
  for (struct string* i = string_table[h]; i; i = i->next) {
    if (strlen(i->s) == n && strncmp(i->s, s, n) == 0)
      return i->s;
  }

  struct string* new = malloc(sizeof(struct string));
  new->s = calloc(n + 1, sizeof(char));
  strncpy(new->s, s, n);
  new->next = string_table[h];
  string_table[h] = new;

  return new->s;
}

const char* string(const char* s) {
  return stringn(s, strlen(s));
}

int read_source(char** dst) {
  int length;
  FILE* f;

  if (!(f = fopen(options.input_filename, "rb")))
    error("can't open input file");
  if (fseek(f, 0, SEEK_END))
    error("can't seek input file");
  if ((length = ftell(f)) == -1)
    error("can't tell input file");
  if (fseek(f, 0, SEEK_SET))
    error("can't seek input file");
  if (!(*dst = calloc(1 + length, 1)))
    error("can't allocate memory");
  if (fread(*dst, 1, length, f) != length)
    error("can't read intput file");
  fclose(f);

  return length;
}

void output(const char* fmt, ...) {
  static FILE* f = NULL;
  if (!f && !(f = fopen(options.output_filename, "wb")))
    error("can't open output file");

  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
}

static const char* basename(const char* path) {
  char* s = strrchr(path, '/');
  return s ? s + 1 : path;
}

static const char* extension(const char* path) {
  char* s = strrchr(basename(path), '.');
  return s ? s + 1 : s + strlen(s);
}

static char* strdup(const char* name) {
  int n = strlen(name);
  char* s = malloc(n + 1);
  strcpy(s, name);
  s[n] = 0;
  return s;
}

struct options options;
void parse_arguments(int argc, char* argv[]) {
  int idx = 1;
  while (idx < argc) {
    if (strcmp(argv[idx], "-o") == 0) {
      if (++idx == argc)
        error("missing file name after -o");
      options.output_filename = argv[idx++];
    } else {
      if (options.input_filename)
        error("more than one input file");
      options.input_filename = argv[idx++];
    }
  }

  if (!options.input_filename)
    error("no input file");
  if (strcmp(extension(options.input_filename), "c") != 0)
    error("input file should have exension \".c\"");
  if (!options.output_filename) {
    char* name = strdup(basename(options.input_filename));
    name[strlen(name) - 1] = 's';
    options.output_filename = name;
  }
}
