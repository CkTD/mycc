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
    [TK_STAR] = "*",
    [TK_ADD] = "+",
    [TK_SUB] = "-",
    [TK_SLASH] = "/",
    [TK_AND] = "&",
    [TK_EQUALEQUAL] = "==",
    [TK_EQUAL] = "=",
    [TK_NOTEQUAL] = "!=",
    [TK_GREATEREQUAL] = ">=",
    [TK_GREATER] = ">",
    [TK_LESSEQUAL] = "<=",
    [TK_LESS] = "<",
    // keywords
    [TK_VOID] = "void",
    [TK_CHAR] = "char",
    [TK_UNSIGNED] = "unsigned",
    [TK_SIGNED] = "signed",
    [TK_INT] = "int",
    [TK_SHORT] = "short",
    [TK_LONG] = "long",
    [TK_PRINT] = "print",
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

Token mktoken(int kind, const char* name) {
  Token t = malloc(sizeof(struct token));
  t->kind = kind;
  t->name = name;
  t->next = NULL;
  return t;
}

Token tokenize(const char* p) {
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
      const char* b = ++p;
      while (*p != '"' && *p != 0)
        p++;
      if (*p == 0)
        error("missing terminating \" character");
      n = mktoken(TK_STRING, stringn(b, p - b));
      p++;
    }
    // punc
    else if (ispunct(*p)) {
      for (int k = TK_OPENING_BRACES; k <= TK_LESS; k++) {
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