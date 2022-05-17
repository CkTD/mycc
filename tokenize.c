#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "inc.h"

const char* token_str[] = {
    // punctuation
    [TK_OPENING_BRACES] = "{",
    [TK_CLOSING_BRACES] = "}",
    [TK_OPENING_PARENTHESES] = "(",
    [TK_CLOSING_PARENTHESES] = ")",
    [TK_OPENING_BRACKETS] = "[",
    [TK_CLOSING_BRACKETS] = "]",
    [TK_ELLIPSIS] = "...",
    [TK_COMMA] = ",",
    [TK_SIMI] = ";",
    [TK_COLON] = ":",
    [TK_QUESTIONMARK] = "?",
    [TK_CARET] = "^",
    [TK_STAR] = "*",
    [TK_PLUS] = "+",
    [TK_MINUS] = "-",
    [TK_SLASH] = "/",
    [TK_PERCENT] = "%",
    [TK_AND_AND] = "&&",
    [TK_BAR_BAR] = "||",
    [TK_AND] = "&",
    [TK_BAR] = "|",
    [TK_LEFT_SHIFT] = "<<",
    [TK_RIGHT_SHIFT] = ">>",
    [TK_EQUAL_EQUAL] = "==",
    [TK_EQUAL] = "=",
    [TK_NOT_EQUAL] = "!=",
    [TK_GREATER_EQUAL] = ">=",
    [TK_GREATER] = ">",
    [TK_LESS_EQUAL] = "<=",
    [TK_LESS] = "<",
    [TK_EXCLAMATION] = "!",
    [TK_TILDE] = "~",
    // keywords
    [TK_VOID] = "void",
    [TK_CHAR] = "char",
    [TK_UNSIGNED] = "unsigned",
    [TK_SIGNED] = "signed",
    [TK_INT] = "int",
    [TK_SHORT] = "short",
    [TK_LONG] = "long",
    [TK_IF] = "if",
    [TK_ELSE] = "else",
    [TK_WHILE] = "while",
    [TK_DO] = "do",
    [TK_FOR] = "for",
    [TK_BREAK] = "break",
    [TK_CONTINUE] = "continue",
    [TK_RETURN] = "return",
    // others
    [TK_IDENT] = "identifier",
    [TK_NUM] = "number",
    [TK_STRING] = "string"};

static Token mktoken(int kind, const char* name) {
  Token t = malloc(sizeof(struct token));
  t->kind = kind;
  t->name = name;
  t->next = NULL;
  return t;
}

static const char* p;
static char buff[4096];

static Token string_literal() {
  char quote = *p++;
  char* out = buff;
  while (*p != quote && *p != 0 && *p != '\n') {
    if (*p == '\\') {  // escape-sequence
      p++;
      switch (*p) {
        case '\'':
        case '"':
          *out++ = *p;
          break;
        case 'a':
          *out++ = '\a';
          break;
        case 'b':
          *out++ = '\b';
          break;
        case 'f':
          *out++ = '\f';
          break;
        case 'n':
          *out++ = '\n';
          break;
        case 'r':
          *out++ = '\r';
          break;
        case 't':
          *out++ = '\t';
          break;
        case 'v':
          *out++ = '\v';
          break;
        default:
          // TODO other escape sequence
          // http://port70.net/~nsz/c/c99/n1256.html#6.4.4.4
          error("unknown escape sequence");
          break;
      }
    } else {
      *out++ = *p;
    }
    p++;
  }

  if (*p++ != quote)
    error("missing terminating %c character", quote);

  return mktoken(TK_STRING, stringn(buff, out - buff));
}

const char* escape(const char* s) {
  char* out = buff;
  for (; *s; s++) {
    switch (*s) {
      case '\\':
        *out++ = '\\';
        *out++ = '\\';
        break;
      case '"':
        *out++ = '\\';
        *out++ = '"';
        break;
      case '\a':
        *out++ = '\\';
        *out++ = 'a';
        break;
      case '\b':
        *out++ = '\\';
        *out++ = 'b';
        break;
      case '\f':
        *out++ = '\\';
        *out++ = 'f';
        break;
      case '\n':
        *out++ = '\\';
        *out++ = 'n';
        break;
      case '\r':
        *out++ = '\\';
        *out++ = 'r';
        break;
      case '\t':
        *out++ = '\\';
        *out++ = 't';
        break;
      case '\v':
        *out++ = '\\';
        *out++ = 'v';
        break;
      default:
        *out++ = *s;
    }
  }
  return stringn(buff, out - buff);
}

Token tokenize(const char* src) {
  p = src;
  struct token head = {};
  Token last = &head;

  while (*p) {
    Token n = NULL;

    // space or tab
    while (isspace(*p)) {
      p++;
    }

    // number
    if (isdigit(*p)) {  // 0-9
      n = mktoken(TK_NUM, stringn(p, 0));
      const char* b = p;
      while (isdigit(*++p))
        ;
      n = mktoken(TK_NUM, stringn(b, p - b));
      n->value = strtol(b, NULL, 10);
    }
    // string literal
    else if (*p == '"') {
      n = string_literal();
    }
    // punc
    else if (ispunct(*p)) {
      for (int k = TK_OPENING_BRACES; k <= TK_TILDE; k++) {
        int len = strlen(token_str[k]);
        if (!strncmp(p, token_str[k], len)) {
          p += len;
          n = mktoken(k, token_str[k]);
          break;
        }
      }
    }
    // keywords or identifer
    else if (isalpha(*p)) {
      const char* b = p;
      do {
        p++;
      } while (isalpha(*p) || isdigit(*p) || *p == '_');
      const char* name = stringn(b, p - b);
      n = NULL;
      for (int kind = TK_VOID; kind <= TK_RETURN; kind++) {
        if (name == string(token_str[kind]))
          n = mktoken(kind, name);
      }
      n = n ? n : mktoken(TK_IDENT, name);
    }

    if (!n)
      error("tokenize: syntax error, unknown \"%c\"", *p);

    last = last->next = n;
  }

  return head.next;
}