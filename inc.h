// utils
void error(char *, ...);
char *stringn(char *s, int n);
char *string(char *s);

// tokenize
enum {
  // punc
  TK_OPENING_BRACES,
  TK_CLOSING_BRACES,
  TK_OPENING_PARENTHESES,
  TK_CLOSING_PARENTHESES,
  TK_SIMI,
  TK_STAR,
  TK_ADD,
  TK_SUB,
  TK_SLASH,
  TK_EQUAL,
  TK_EQUALEQUAL,
  TK_NOTEQUAL,
  TK_GREATER,
  TK_GREATEREQUAL,
  TK_LESS,
  TK_LESSEQUAL,
  // keywords
  TK_PRINT,
  TK_IDENT,
  TK_INT,
  TK_IF,
  TK_ELSE,
  TK_WHILE,
  //
  TK_EOI,
  // constant
  TK_NUM,
};

extern const char *token_str[];

typedef struct token *Token;
struct token {
  int kind;
  Token next;

  int value;
  char *name;
};

Token tokenize(char *input);
void print_token(Token t);

// Symbol table
typedef struct symbol *Symbol;
struct symbol {
  char *name;
};

void mksym(char *s);
Symbol findsym(char *s);

// AST
enum {
  // 16 right
  A_ASSIGN,
  // 10 left
  A_EQ,
  A_NOTEQ,
  // 9 left
  A_LT,
  A_GT,
  A_LE,
  A_GE,
  // 6 left
  A_ADD,
  A_SUB,
  // 5 left
  A_MUL,
  A_DIV,
  // others
  A_NUM,
  A_PRINT,
  A_IDENT,
  A_LVIDENT,
  A_IF,
  A_WHILE,
};

extern char *ast_str[];

typedef struct node *Node;
struct node {
  int op;
  Node left;
  Node mid;
  Node right;
  Node next;

  int intvalue;
  char *sym;
};

Node parse(Token t);
void print_ast(Node r, int indent);

//
//

void genglobalsym(char *s);
void codegen(Node r);