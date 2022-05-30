#include "inc.h"

static const char* token_str[] = {
    // punctuation
    [TK_OPENING_BRACES] = "{",
    [TK_CLOSING_BRACES] = "}",
    [TK_OPENING_PARENTHESES] = "(",
    [TK_CLOSING_PARENTHESES] = ")",
    [TK_OPENING_BRACKETS] = "[",
    [TK_CLOSING_BRACKETS] = "]",
    [TK_STAR_EQUAL] = "*=",
    [TK_SLASH_EQUAL] = "/=",
    [TK_PLUS_EQUAL] = "+=",
    [TK_MINUS_EQUAL] = "-=",
    [TK_LEFT_SHIFT_EQUAL] = "<<=",
    [TK_RIGHT_SHIFT_EQUAL] = ">>=",
    [TK_AND_EQUAL] = "&=",
    [TK_CARET_EQUAL] = "^=",
    [TK_BAR_EQUAL] = "|=",
    [TK_ELLIPSIS] = "...",
    [TK_DOT] = ".",
    [TK_ARROW] = "->",
    [TK_COMMA] = ",",
    [TK_SIMI] = ";",
    [TK_COLON] = ":",
    [TK_QUESTIONMARK] = "?",
    [TK_CARET] = "^",
    [TK_STAR] = "*",
    [TK_PLUS_PLUS] = "++",
    [TK_MINUS_MINUS] = "--",
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
    [TK_CONST] = "const",
    [TK_VOLATILE] = "volatile",
    [TK_RESTRICT] = "restrict",
    [TK_STRUCT] = "struct",
    [TK_UNION] = "union",
    [TK_ENUM] = "enum",
    [TK_TYPEDEF] = "typedef",
    [TK_SIZEOF] = "sizeof",
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
    [TK_STRING] = "string",
};

// token currently being processed
static Token ct;

Token token() {
  return ct;
}

void set_token(Token t) {
  ct = t;
}

int match(int kind) {
  return ct->kind == kind;
}

int match_specifier() {
  return ct->kind >= TK_VOID && ct->kind <= TK_TYPEDEF;
}

Token expect(int kind) {
  Token t = ct;

  if (!match(kind))
    error("parse: token of %s expected but got %s", token_str[kind],
          token_str[ct->kind]);

  ct = ct->next;
  return t;
}

Token consume(int kind) {
  if (match(kind)) {
    Token t = ct;
    ct = ct->next;
    return t;
  }
  return NULL;
}

static char* cc;  // character currently being processed
static char* ec;  // last character
static char* buff;

static Token mktoken(int kind, const char* name) {
  Token t = malloc(sizeof(struct token));
  t->kind = kind;
  t->name = name;
  t->next = NULL;
  return t;
}

static Token string_literal() {
  char quote = *cc++;
  char* out = buff;
  while (cc < ec && *cc != quote && *cc != '\n') {
    if (!*cc) {
      error("null character");
    } else if (*cc == '\\') {  // escape-sequence
      cc++;
      switch (*cc) {
        case '\'':
        case '"':
          *out++ = *cc;
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
      *out++ = *cc;
    }
    cc++;
  }

  if (*cc++ != quote)
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

static Token integer_constant() {
  const char* begin = cc;

  if (*cc == '0' && isdigit(cc[1])) {  // octal
    while (*cc >= '0' && *cc <= '7')
      ++cc;
  } else if (*cc == '0' && (cc[1] == 'x' || cc[1] == 'X')) {  // hexadecimal
    cc += 2;
    while ((*cc >= '0' && *cc <= '9') || (*cc >= 'a' && *cc <= 'f') ||
           (*cc >= 'A' && *cc <= 'F'))
      ++cc;
  } else {  // decimal
    while ((*cc >= '0' && *cc <= '9'))
      ++cc;
  }

  while (*cc == 'l' || *cc == 'L' || *cc == 'u' || *cc == 'U')
    cc++;
  return mktoken(TK_NUM, stringn(begin, cc - begin));
}

void tokenize() {
  int length = read_source(&cc);
  ec = cc + length;
  buff = malloc(length);

  Token* tail = &ct;

  while (cc < ec) {
    Token n = NULL;

    if (!*cc)
      error("null character");

    // space or tab
    if (isspace(*cc)) {
      cc++;
      continue;
    }

    // comments
    if (*cc == '/' && cc[1] == '/') {
      while (cc < ec && *cc != '\n')
        ++cc;
      continue;
    }

    if (*cc == '/' && cc[1] == '*') {
      cc += 2;
      while (cc < ec && !(*cc == '*' && cc[1] == '/'))
        ++cc;
      if (*cc != '*' || cc[1] != '/')
        error("unterminated comment");
      cc += 2;
      continue;
    }

    // number
    if (isdigit(*cc)) {  // 0-9
      n = integer_constant();
    }
    // string literal
    else if (*cc == '"') {
      n = string_literal();
    }
    // punc
    else if (ispunct(*cc)) {
      for (int k = TK_OPENING_BRACES; k <= TK_TILDE; k++) {
        int len = strlen(token_str[k]);
        if (!strncmp(cc, token_str[k], len)) {
          cc += len;
          n = mktoken(k, token_str[k]);
          break;
        }
      }
    }
    // keywords or identifer
    else if (isalpha(*cc)) {
      const char* b = cc;
      do {
        cc++;
      } while (isalpha(*cc) || isdigit(*cc) || *cc == '_');
      const char* name = stringn(b, cc - b);
      n = NULL;
      for (int kind = TK_VOID; kind <= TK_RETURN; kind++) {
        if (name == string(token_str[kind]))
          n = mktoken(kind, name);
      }
      n = n ? n : mktoken(TK_IDENT, name);
    }

    if (!n)
      error("tokenize: syntax error, unknown \"%c\"", *cc);

    tail = &(*tail = n)->next;
  }
}
