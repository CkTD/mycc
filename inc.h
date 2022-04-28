/*************
 *   utils   *
 *************/
void error(char*, ...);
char* stringn(char* s, int n);
char* string(char* s);

typedef struct type* Type;
typedef struct node* Node;

/*************
 *   type    *
 *************/
enum {
  TY_INT,
};
extern Type inttype;
Type ty(Node n);
struct type {
  int kind;
  int size;
  Type base;
};

int is_int(Type ty);

/*************
 * tokenize  *
 *************/
enum {
  // punc
  TK_OPENING_BRACES,
  TK_CLOSING_BRACES,
  TK_OPENING_PARENTHESES,
  TK_CLOSING_PARENTHESES,
  TK_COMMA,
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
  TK_DO,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_VOID,
  TK_RETURN,
  //
  TK_EOI,
  // constant
  TK_NUM,
};

typedef struct token* Token;
struct token {
  int kind;
  Token next;

  int value;
  char* name;
};

extern const char* token_str[];
Token tokenize(char* input);

/*************
 *   parse   *
 *************/
typedef struct var* Var;
struct var {
  Var next;        // linked in global/local variable list
  Var scope_next;  // linked in scope
  Type type;
  char* name;
  int offset;
  int is_global;
};
enum {
  // 16 right
  A_ASSIGN,
  // 10 left
  A_EQ,
  A_NE,
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
  A_EXPR_STAT,
  A_NUM,
  A_PRINT,
  A_VAR,
  A_LVIDENT,
  A_IF,
  A_DOWHILE,
  A_FOR,
  A_BREAK,
  A_CONTINUE,
  A_RETURN,
  A_FUNCDEF,
  A_FUNCCALL,
  A_BLOCK,
};

struct node {
  int kind;
  Node next;
  Type type;

  // Used by expr(A_ASSIGN, A_ADD, ...)
  Node left;
  Node right;

  // A_FOR and A_IF
  Node cond;
  Node then;
  Node els;
  Node init;
  Node post;

  // compound-statement
  Node body;

  // A_NUM
  int intvalue;

  // A_FUNCTION
  char* funcname;
  Var globals;
  Var params;
  Var locals;
  int stacksize;

  // A_FUNCTIONCALL
  char* callee;
  Node args;

  // A_VAR
  Var var;
  char* name;
};

// global variables are accumulated to this list during parsing
extern Var globals;
Node parse(Token t);

/*********************
 *   code generate   *
 *********************/
void codegen(Node r);