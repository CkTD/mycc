#include "ctype.h"
#include "inc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

const char* token_str[] = {[TK_OPENING_BRACES] = "{",
                           [TK_CLOSING_BRACES] = "}",
                           [TK_OPENING_PARENTHESES] = "(",
                           [TK_CLOSING_PARENTHESES] = ")",
                           [TK_COMMA] = ",",
                           [TK_SIMI] = ";",
                           [TK_STAR] = "*",
                           [TK_ADD] = "+",
                           [TK_SUB] = "-",
                           [TK_SLASH] = "/",
                           [TK_EQUAL] = "=",
                           [TK_EQUALEQUAL] = "==",
                           [TK_NOTEQUAL] = "!=",
                           [TK_GREATER] = ">",
                           [TK_GREATEREQUAL] = ">=",
                           [TK_LESS] = "<",
                           [TK_LESSEQUAL] = "<=",
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
                           //
                           [TK_EOI] = "eoi",
                           [TK_IDENT] = "identifier",
                           [TK_NUM] = "number"};

Token mktoken(int kind, const char* name) {
  Token t = malloc(sizeof(struct token));
  t->kind = kind;
  t->name = name;
  t->next = NULL;
  return t;
}

Token tokenize(char* p) {
  struct token head = {};
  Token last = &head;
  Token n;
  while (*p) {
    if (isspace(*p)) {  // space or tab
      p++;
    }
    // number
    else if (isdigit(*p)) {  // 0-9
      n = mktoken(TK_NUM, stringn(p, 0));
      char* b = p;
      while (isdigit(*++p))
        ;
      n = mktoken(TK_NUM, stringn(b, p - b));
      n->value = strtol(b, NULL, 10);
    }
    // punc
    else if (*p == '{') {
      n = mktoken(TK_OPENING_BRACES, stringn(p, 1));
      p++;
    } else if (*p == '}') {
      n = mktoken(TK_CLOSING_BRACES, stringn(p, 1));
      p++;
    } else if (*p == '(') {
      n = mktoken(TK_OPENING_PARENTHESES, stringn(p, 1));
      p++;
    } else if (*p == ')') {
      n = mktoken(TK_CLOSING_PARENTHESES, stringn(p, 1));
      p++;
    } else if (*p == ',') {
      n = mktoken(TK_COMMA, stringn(p, 1));
      p++;
    } else if (*p == '+') {
      n = mktoken(TK_ADD, stringn(p, 1));
      p++;
    } else if (*p == '-') {
      n = mktoken(TK_SUB, stringn(p, 1));
      p++;
    } else if (*p == '*') {
      n = mktoken(TK_STAR, stringn(p, 1));
      p++;
    } else if (*p == '/') {
      n = mktoken(TK_SLASH, stringn(p, 1));
      p++;
    } else if (*p == ';') {
      n = mktoken(TK_SIMI, stringn(p, 1));
      p++;
    } else if (*p == '=' && p[1] == '=') {
      n = mktoken(TK_EQUALEQUAL, stringn(p, 2));
      p += 2;
    } else if (*p == '=') {
      n = mktoken(TK_EQUAL, stringn(p, 1));
      p++;
    } else if (*p == '!' && p[1] == '=') {
      n = mktoken(TK_NOTEQUAL, stringn(p, 2));
      p += 2;
    } else if (*p == '>' && p[1] == '=') {
      n = mktoken(TK_GREATEREQUAL, stringn(p, 2));
      p += 2;
    } else if (*p == '>') {
      n = mktoken(TK_GREATER, stringn(p, 1));
      p++;
    } else if (*p == '<' && p[1] == '=') {
      n = mktoken(TK_LESSEQUAL, stringn(p, 2));
      p += 2;
    } else if (*p == '<') {
      n = mktoken(TK_LESS, stringn(p, 1));
      p++;
    }
    // keywords or  identifer
    else if (isalpha(*p)) {
      char* b = p;
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
    } else {
      error("tokenize: syntax error, unknown %c", *p);
    }

    last = last->next = n;
  }
  last->next = mktoken(TK_EOI, NULL);
  return head.next;
}

void print_token(Token t) {
  for (; t; t = t->next) {
    fprintf(stderr, "%10s  \"%s", token_str[t->kind], t->name);
    fprintf(stderr, "\"\n");
  }
}