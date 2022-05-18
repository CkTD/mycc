/*************
 *   utils   *
 *************/
void error(char*, ...);
void warn(char*, ...);

const char* stringn(const char* s, int n);
const char* string(const char* s);

typedef struct type* Type;
typedef struct node* Node;

/*************
 *   type    *
 *************/
enum {
  TY_VOID,
  TY_CHAR,
  TY_SHRT,
  TY_INT,
  TY_LONG,
  TY_UCHAR,
  TY_USHRT,
  TY_UINT,
  TY_ULONG,
  //
  TY_POINTER,
  TY_ARRAY,
};

extern Type voidtype;
extern Type chartype;
extern Type shorttype;
extern Type inttype;
extern Type longtype;
extern Type uchartype;
extern Type ushorttype;
extern Type uinttype;
extern Type ulongtype;

struct type {
  int kind;
  int size;
  Type base;
};

Type type(int kind, Type base, int size);
Type ptr_type(Type base);
Type deref_type(Type ptr);
Type array_type(Type base, int n);
int is_ptr(Type t);
int is_array(Type t);
Type array_to_ptr(Type a);
int is_ptr_compatiable(Type a, Type b);
int is_signed(Type t);
int is_unsigned(Type t);
int is_integer(Type t);
int is_arithmetic(Type t);
int is_scalar(Type t);
Type integral_promote(Type t);
Type usual_arithmetic_type(Type t1, Type t2);

/*************
 * tokenize  *
 *************/
enum {
  /***** punctuation *****/
  TK_OPENING_BRACES,
  TK_CLOSING_BRACES,
  TK_OPENING_PARENTHESES,
  TK_CLOSING_PARENTHESES,
  TK_OPENING_BRACKETS,
  TK_CLOSING_BRACKETS,
  TK_STAR_EQUAL,
  TK_SLASH_EQUAL,
  TK_PLUS_EQUAL,
  TK_MINUS_EQUAL,
  TK_LEFT_SHIFT_EQUAL,
  TK_RIGHT_SHIFT_EQUAL,
  TK_AND_EQUAL,
  TK_CARET_EQUAL,
  TK_BAR_EQUAL,
  TK_ELLIPSIS,
  TK_COMMA,
  TK_SIMI,
  TK_COLON,
  TK_QUESTIONMARK,
  TK_CARET,
  TK_STAR,
  TK_PLUS_PLUS,
  TK_MINUS_MINUS,
  TK_PLUS,
  TK_MINUS,
  TK_SLASH,
  TK_PERCENT,
  TK_AND_AND,
  TK_BAR_BAR,
  TK_AND,
  TK_BAR,
  TK_LEFT_SHIFT,
  TK_RIGHT_SHIFT,
  TK_EQUAL_EQUAL,
  TK_EQUAL,
  TK_NOT_EQUAL,
  TK_GREATER_EQUAL,
  TK_GREATER,
  TK_LESS_EQUAL,
  TK_LESS,
  TK_EXCLAMATION,
  TK_TILDE,
  /***** keyword *****/
  TK_VOID,
  TK_CHAR,
  TK_UNSIGNED,
  TK_SIGNED,
  TK_INT,
  TK_SHORT,
  TK_LONG,
  TK_IF,
  TK_ELSE,
  TK_WHILE,
  TK_DO,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_RETURN,
  /***** other *****/
  TK_IDENT,
  TK_NUM,
  TK_STRING,
};

typedef struct token* Token;
struct token {
  int kind;
  Token next;

  int value;
  const char* name;
};

extern const char* token_str[];
const char* escape(const char* s);
Token tokenize(const char* input);

/*************
 *   parse   *
 *************/
enum {
  /***** expressions *****/
  // 17 left
  A_COMMA,
  // 16 right
  A_ASSIGN,
  A_TERNARY,
  // 15 left
  A_L_OR,
  // 14 left
  A_L_AND,
  // 13 left
  A_B_INCLUSIVEOR,
  // 12 left
  A_B_EXCLUSIVEOR,
  // 11 left
  A_B_AND,
  // 10 left
  A_EQ,
  A_NE,
  // 9 left
  A_LT,
  A_GT,
  A_LE,
  A_GE,
  // 7 left
  A_LEFT_SHIFT,
  A_RIGHT_SHIFT,
  // 6 left
  A_ADD,
  A_SUB,
  // 5 left
  A_MUL,
  A_DIV,
  A_MOD,
  // 3 right
  // prefix inc/dec are implemeted by A_XX_ASSIGN and A_ADD/A_SUB
  // A_PREFIX_INC
  // A_PREFIX_DEC
  A_ADDRESS_OF,
  A_DEFERENCE,
  A_MINUS,
  A_PLUS,
  A_L_NOT,
  A_B_NOT,
  // 2 left
  A_FUNC_CALL,
  A_ARRAY_SUBSCRIPTING,
  A_POSTFIX_INC,
  A_POSTFIX_DEC,
  // primary
  A_NUM,
  A_IDENT,
  A_STRING_LITERAL,
  /***** statement *****/
  A_IF,
  A_DOWHILE,
  A_FOR,
  A_BREAK,
  A_CONTINUE,
  A_RETURN,
  /***** auxiliary *****/
  A_DLIST,
  A_BLOCK,
  A_EXPR_STAT,
  A_CONVERSION,
  /***** other *****/
  A_VAR,
  A_VARARG,
  A_FUNCTION,
};

struct node {
  int kind;
  // expressions
  // A_FUNCDEF(return type)
  Type type;
  // function or variable name
  // string literal label name
  const char* name;

  // linked in compound-statement's body list(statement)
  // linked in global object list(A_FUNC_DEF, A_VAR)
  // linked in local var list(A_VAR)
  // A_DLIST
  Node next;

  // A_DLIST
  Node prev;

  // expressions
  Node left;
  Node right;

  // A_FOR, A_IF
  Node cond;
  Node then;
  Node els;
  Node init;
  Node post;

  // A_BLOCK, A_EXPR_STAT, A_EXPR_STAT, A_FUNC_ARG
  // A_FUNC_DEF, A_WHILE, A_DOWHILE, A_FOR
  Node body;

  // A_NUM
  int intvalue;

  // A_STRING_LITERAL
  const char* string_value;

  // A_VAR
  Node scope_next;  // linked in scope(for local var)
  int offset;       // stack offset(for local var)
  int is_global;
  Node init_value;

  // A_FUNC_CALL
  const char* callee_name;
  Node args;

  // A_ARRAY_SUBSCRIPTING
  Node array;  // array / pointer
  Node index;

  // A_FUNC_DEF
  Node globals;
  Node protos;
  Node locals;
  int stack_size;
  int is_function;
};

#define list_for_each(head, node) \
  for (node = head->next; node != head; node = node->next)
#define list_for_each_reverse(head, node) \
  for (node = head->prev; node != head; node = node->prev)
#define list_empty(head) (head->next == head)
int list_length(Node head);

Node parse(Token t);

/*********************
 *   code generate   *
 *********************/
void codegen(Node r);