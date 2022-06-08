#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*************
 *   utils   *
 *************/

#define max(x, y) (((x) > (y)) ? (x) : (y))

typedef struct type* Type;
typedef struct node* Node;
typedef struct token* Token;

void error(char*, ...);
void warn(char*, ...);
void info(char*, ...);
void errorat(Token tok, char* msg, ...);
void warnat(Token tok, char* msg, ...);
void infoat(Token tok, char* msg, ...);

const char* stringn(const char* s, int n);
const char* string(const char* s);

struct options {
  const char* input_filename;
  const char* output_filename;
};
extern struct options options;
void parse_arguments(int argc, char* argv[]);

int read_source(char** dst);
void output(const char* fmt, ...);

/*************
 *   symbs   *
 *************/

enum scope {
  SCOPE_FILE,
  SCOPE_INNER,
  SCOPE_ALL,
};

extern Node globals;
extern Node current_func;
void enter_scope();
void exit_scope();
void enter_func();
void exit_func();
Node find_symbol(Token tok, int kind, int scope);
void install_symbol(Node n, int scope);

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
  TY_CONST,
  TY_FUNCTION,
  TY_VARARG,
  TY_PLACEHOLDER,
  TY_STRUCT,
  TY_UNION,
  TY_ENUM,
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
extern Type voidptrtype;

typedef struct proto* Proto;
struct proto {
  Type type;
  Token token;
  const char* name;
  Proto next;
};

typedef struct member* Member;
struct member {
  Type type;
  Token token;
  const char* name;
  Member next;
  int offset;
};

struct type {
  int kind;
  int size;
  Type base;

  const char* str;
  Proto proto;
  const char* tag;  // for struct/union/enum
  Member member;
};

Type type(int kind, Type base, int size);
Type ptr_type(Type base);
Type deref_type(Type ptr);
Type array_type(Type base, int n);
Type array_to_ptr(Type a);
Type function_type(Type t, Proto p);
Type const_type(Type t);
Type unqual(Type t);
Type struct_or_union_type(Member member, const char* tag, int kind);
Member get_struct_or_union_member(Type t, const char* name);
void update_struct_or_union_type(Type ty, Member member);
Type enum_type(const char* tag);

int is_ptr(Type t);
int is_array(Type t);
int is_funcion(Type t);
int is_signed(Type t);
int is_unsigned(Type t);
int is_integer(Type t);
int is_arithmetic(Type t);
int is_scalar(Type t);
int is_qual(Type t);
int is_const(Type t);
int is_struct(Type t);
int is_union(Type t);
int is_struct_or_union(Type t);
int is_struct_with_const_member(Type t);
int is_enum(Type t);
int is_compatible_type(Type t1, Type t2);
Type composite_type(Type t1, Type t2);
Type integral_promote(Type t);
Type usual_arithmetic_type(Type t1, Type t2);
Type default_argument_promoe(Type t);

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
  TK_DOT,
  TK_ARROW,
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
  TK_CONST,
  TK_VOLATILE,
  TK_RESTRICT,
  TK_STRUCT,
  TK_UNION,
  TK_ENUM,
  TK_TYPEDEF,
  TK_SIZEOF,
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

struct token {
  int kind;
  Token next;
  const char* name;

  const char* line;
  int line_no, char_no;
};

Token token();
void set_token(Token t);
Token match(int kind);
Token match_specifier();
Token expect(int kind);
Token consume(int kind);
void tokenize();
const char* escape(const char* s);

/*************
 *   parse   *
 *************/
enum {
  A_ANY,
  /***** expressions *****/
  // 17 left
  A_COMMA,
  // 16 right
  A_ASSIGN,
  A_INIT,  // init local variable(by assign)
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
  A_SIZE_OF,
  // 2 left
  A_FUNC_CALL,
  A_ARRAY_SUBSCRIPTING,
  A_POSTFIX_INC,
  A_POSTFIX_DEC,
  A_DOT,
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
  A_NOOP,
  A_DLIST,
  A_BLOCK,
  A_EXPR_STAT,
  A_CONVERSION,
  /***** other *****/
  A_VAR,
  A_TAG,
  A_VARARG,
  A_FUNCTION,
  A_MEMBER_SELECTION,
  A_ENUM_CONST,
  A_TYPEDEF,
};

struct node {
  int kind;
  Token token;

  // for expression, variable and function
  Type type;

  // function or variable name
  // string literal label name
  // callee name
  const char* name;

  // linked in compound-statement's body list(statement)
  // linked in global object list(A_FUNCTION, A_VAR, A_STRING_LITERAL)
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

  // A_WHILE, A_DOWHILE, A_FOR
  // A_FUNCTION, A_CONVERSION, A_DLIST, A_EXPR_STAT, A_BLOCK
  Node body;

  // A_NUM, A_ENUM_CONST
  unsigned long long intvalue;

  // A_STRING_LITERAL
  const char* string_value;

  // A_VAR
  Node scope_next;  // linked in scope(for local var)
  int offset;       // stack offset(for local var)
  int is_global;
  Node init_value;

  // A_FUNC_CALL
  Node args;

  // A_ARRAY_SUBSCRIPTING
  Node array;  // array / pointer
  Node index;

  // A_MEMBER_SELECTION
  Node structure;
  Member member;

  // A_FUNCTION
  Node globals;
  Node params;
  Node locals;
  int stack_size;

  // A_IDNET
  Node ref;
};

#define list_for_each(head, node) \
  for (node = head->next; node != head; node = node->next)
#define list_for_each_reverse(head, node) \
  for (node = head->prev; node != head; node = node->prev)
#define list_empty(head) (head->next == head)
int list_length(Node head);

void parse();

/*********************
 *   code generate   *
 *********************/
void codegen();