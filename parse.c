#include <stdlib.h>
#include <string.h>
#include "inc.h"

/***********************
 * handle input tokens *
 ***********************/
static Token t;  // token to be processed

static int match(int kind) {
  return t->kind == kind;
}

static Token expect(int kind) {
  Token c = t;

  if (!match(kind)) {
    error("parse: token of %s expected but got %s", token_str[kind],
          token_str[t->kind]);
  }
  t = t->next;
  return c;
}

static Token consume(int kind) {
  Token c = t;

  if (match(kind)) {
    t = t->next;
    return c;
  }
  return NULL;
}

int match_type_spec() {
  return t->kind >= TK_VOID && t->kind <= TK_RESTRICT;
}
/*******************
 * create AST Node *
 *******************/

// http://port70.net/~nsz/c/c99/n1256.html#6.3.2.1p1
static int is_lvalue(Node node) {
  switch (node->kind) {
    case A_VAR:
    case A_DEFERENCE:
    case A_ARRAY_SUBSCRIPTING:
      return 1;
    default:
      return 0;
  }
}

static int is_modifiable_lvalue(Node node, int ignore_qual) {
  Type t = ignore_qual ? unqual(node->type) : node->type;
  return is_lvalue(node) && !is_const(t) && !is_array(t);
}

static Node mknode(int kind) {
  Node n = calloc(1, sizeof(struct node));
  n->kind = kind;
  return n;
}

static Node mkicons(int intvalue) {
  Node n = mknode(A_NUM);
  n->type = inttype;
  n->intvalue = intvalue;
  return n;
}

static Node mkaux(int kind, Node body) {
  Node n = mknode(kind);
  n->body = body;
  return n;
}

static Node mkcvs(Type t, Node body) {
  if (body->type == t)
    return body;

  Node n = mkaux(A_CONVERSION, body);
  n->type = t;

  return n;
}

// usual arithmetic conversions for binary expressions
static Type usual_arithmetic_conversions(Node node) {
  Type t = usual_arithmetic_type(node->left->type, node->right->type);
  node->left = mkcvs(t, node->left);
  node->right = mkcvs(t, node->right);
  return t;
}

static Node unqual_array_to_ptr(Node n) {
  n = mkcvs(unqual(n->type), n);
  if (is_array(n->type))
    n = mkcvs(array_to_ptr(n->type), n);
  return n;
}

// kind:
//     A_ADDRESS_OF, A_DEREFERENCE
//     A_MINUS, A_PLUS, A_B_NOT, A_L_NOT
//     A_POSTFIX_INC, A_POSTFIX_DEC
//     A_SIZE_OF
static Node mkunary(int kind, Node left) {
  Node n = mknode(kind);
  n->left = (kind == A_ADDRESS_OF || kind == A_POSTFIX_INC ||
             kind == A_POSTFIX_DEC || kind == A_SIZE_OF)
                ? left
                : unqual_array_to_ptr(left);

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.3.2
  if (kind == A_ADDRESS_OF) {
    if (left->kind == A_DEFERENCE)
      return left->left;
    if (is_lvalue(left)) {
      n->type = ptr_type(left->type);
      return n;
    }
    error("invalid operand");
  }
  if (kind == A_DEFERENCE) {
    if (left->kind == A_ADDRESS_OF)
      return left->left;
    if (is_ptr(left->type)) {
      n->type = deref_type(left->type);
      return n;
    }
    error("invalid operand");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.3.3
  if (kind == A_MINUS || kind == A_PLUS) {
    if (is_arithmetic(n->left->type)) {
      n->left = mkcvs(integral_promote(n->left->type), n->left);
      n->type = n->left->type;
      return n;
    }
    error("invalid operand");
  }
  if (kind == A_B_NOT) {
    if (is_integer(n->left->type)) {
      n->left = mkcvs(integral_promote(n->left->type), n->left);
      n->type = n->left->type;
      return n;
    }
    error("invalid operand");
  }
  if (kind == A_L_NOT) {
    if (is_scalar(n->left->type)) {
      n->type = inttype;
      return n;
    }
    error("invalid operand");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.4
  if (kind == A_POSTFIX_INC || kind == A_POSTFIX_DEC) {
    if (!is_modifiable_lvalue(n->left, 0) ||
        !(is_arithmetic(n->left->type) || is_ptr(n->left->type)))
      error("invalid operand");
    n->left = mkcvs(unqual(n->left->type), left);
    n->type = n->left->type;
    return mkcvs(integral_promote(n->type), n);
  }

  if (kind == A_SIZE_OF) {
    Node s = mkicons(unqual(n->left->type)->size);
    s->type = uinttype;
    return s;
  }

  error("invalid unary operator");
  return NULL;
}

// kind:
//     A_COMMA
//     A_ASSIGN
//     A_L_OR, A_L_AND,
//     A_B_INCLUSIVE, A_B_EXCLUSIVE, A_B_AND
//     A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
//     A_LEFT_SHIFT, A_RIGHT_SHIFT
//     A_ADD, A_SUB
//     A_MUL, A_DIV
static Node mkbinary(int kind, Node left, Node right) {
  Node n = mknode(kind);
  n->left =
      (kind == A_ASSIGN || kind == A_INIT) ? left : unqual_array_to_ptr(left);
  n->right = unqual_array_to_ptr(right);

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.16
  if (kind == A_ASSIGN || kind == A_INIT) {
    if (!is_modifiable_lvalue(n->left, kind == A_INIT))
      error("invalid left operand of assignment");
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
    } else if (is_ptr(n->left->type) && n->right->kind == A_NUM &&
               n->right->intvalue == 0) {
      // http://port70.net/~nsz/c/c99/n1256.html#6.3.2.3p3
    } else if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
      if (kind == A_ASSIGN) {
        if (!is_const(deref_type(n->left->type)) &&
            is_const(deref_type(n->right->type)))
          error("assign discards const");
      }
      if (!is_compatible_type(unqual(n->left->type), n->right->type))
        error("assign with incompatible type");
    } else
      error("assign with incompatible type");

    n->type = unqual(n->left->type);
    n->right = mkcvs(n->type, n->right);

    if (kind == A_INIT) {
      n->kind = A_ASSIGN;
      n = mkaux(A_EXPR_STAT, n);
    }
    return n;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.17
  if (kind == A_COMMA) {
    n->type = n->right->type;
    return n;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.14
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.13
  if (kind == A_L_OR || kind == A_L_AND) {
    if (is_scalar(n->left->type) && is_scalar(n->right->type)) {
      n->type = inttype;
      return n;
    }
    error("invalid operands");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.10
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.11
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.12
  if (kind == A_B_AND || kind == A_B_INCLUSIVEOR || kind == A_B_EXCLUSIVEOR) {
    if (is_integer(n->left->type) && is_integer(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    error("invalid operand");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.9
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.8
  if (kind >= A_EQ && kind <= A_GE) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      usual_arithmetic_conversions(n);
      n->type = inttype;
      return n;
    }
    if (kind >= A_EQ && kind <= A_NE) {
      if ((is_ptr(n->left->type) &&
           (n->right->kind == A_NUM && n->right->intvalue == 0)) ||
          (is_ptr(n->right->type) &&
           (n->left->kind == A_NUM && n->left->intvalue == 0))) {
        n->type = inttype;
        return n;
      }
    }
    if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
      if (!is_compatible_type(n->left->type, n->right->type))
        error("incompatible type");
      n->type = inttype;
      return n;
    }
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.7
  if (kind == A_LEFT_SHIFT || kind == A_RIGHT_SHIFT) {
    if (is_integer(n->left->type) && is_integer(n->right->type)) {
      n->type = integral_promote(n->left->type);
      n->left = mkcvs(n->type, n->left);
      n->right = mkcvs(n->right->type, n->right);
      return n;
    }
    error("invalid operand");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.6
  if (kind == A_ADD) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    if (is_integer(n->left->type) && is_ptr(n->right->type)) {
      n->left = mkbinary(A_MUL, n->left, mkicons(n->right->type->base->size));
      n->type = n->right->type;
      return n;
    }
    if (is_ptr(n->left->type) && is_integer(n->right->type)) {
      n->right = mkbinary(A_MUL, n->right, mkicons(n->left->type->base->size));
      n->type = n->left->type;
      return n;
    }
    error("invalid operands to binary \"+\"");
  }
  if (kind == A_SUB) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    if (is_ptr(n->left->type) && is_integer(n->right->type)) {
      n->right = mkbinary(A_MUL, n->right, mkicons(n->left->type->base->size));
      n->type = n->left->type;
      return n;
    }
    if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
      if (!is_compatible_type(n->left->type, n->right->type))
        error("invalid operands to binary, incompatiable pointer");
      n->type = inttype;
      return mkbinary(A_DIV, n, mkicons(n->left->type->base->size));
    }

    error("invalid operands to binary \"-\"");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.5
  if (kind == A_MUL || kind == A_DIV) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    error("invalid operands to binary");
  }
  if (kind == A_MOD) {
    if (is_integer(n->left->type) && is_integer(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    error("invalid operands to binary");
  }

  error("invalid binary operator");
  return NULL;
}

static Node mkternary(Node cond, Node left, Node right) {
  Node n = mknode(A_TERNARY);
  n->cond = unqual_array_to_ptr(cond);
  n->left = unqual_array_to_ptr(left);
  n->right = unqual_array_to_ptr(right);

  if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
    n->type = usual_arithmetic_conversions(n);
    return n;
  }

  if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
    if (!is_compatible_type(n->left->type, n->right->type))
      error("pointer type mismatch");
    n->type = n->left->type;
    if (is_array(n->type))
      n->type = array_to_ptr(n->type);
    return n;
  }

  error("invalid operand");
  return NULL;
}

/**********************
 * double linked list *
 **********************/
// make a circular doubly linked list node
// if body is NULL, make a dummy head node
static Node mklist(Node body) {
  Node n = mkaux(A_DLIST, body);
  n->next = n->prev = n;
  return n;
}

// insert node to the end of list and return head
static Node list_insert(Node head, Node node) {
  head->prev->next = node;
  node->prev = head->prev;
  node->next = head;
  head->prev = node;
  return head;
}

int list_length(Node head) {
  int l = 0;
  for (Node n = head->next; n != head; n = n->next)
    l++;
  return l;
}

/***********************
 * variables and scope *
 ***********************/
// local variables are accumulated to this list during parsing current function
static Node locals;
// global variables/function/string-literal
// are accumulated to this list during parsing
static Node globals;

typedef struct scope* Scope;
struct scope {
  Node list;
  Scope outer;
};
static Scope scope;

static void enter_scope() {
  Scope new_scope = calloc(1, sizeof(struct scope));
  new_scope->outer = scope;
  scope = new_scope;
}

static void exit_scope() {
  scope = scope->outer;
}

static Node find_scope(const char* name, Scope s) {
  for (Node n = s->list; n; n = n->scope_next) {
    if (name == n->name)
      return n;
  }
  return NULL;
}

static Node find_global(const char* name) {
  for (Node n = globals; n; n = n->next) {
    if (name == n->name)
      return n;
  }
  return NULL;
}

static Node find_var(const char* name) {
  Node v;
  for (Scope s = scope; s; s = s->outer) {
    if ((v = find_scope(name, s)))
      return v;
  }

  v = find_global(name);
  if (v && v->kind == A_FUNCTION)
    error("%s is a function, variable expected", name);
  return v;
}

static Node mkvar(const char* name, Type ty) {
  Node n = mknode(A_VAR);
  n->name = name;
  n->type = ty;
  return n;
}

static Node mklvar(const char* name, Type ty) {
  if (find_scope(name, scope))
    error("redefine local variable \"%s\"", name);

  Node n = mkvar(name, ty);
  n->is_global = 0;
  n->next = locals;
  locals = n;
  n->scope_next = scope->list;
  scope->list = n;
  return n;
}

static Node mkgvar(const char* name, Type ty) {
  if (find_global(name))
    error("redefine global variable \"%s\"", name);

  Node n = mkvar(name, ty);
  n->is_global = 1;
  n->next = globals;
  globals = n;
  return n;
}

static Node mkfunc(const char* name, Type ty) {
  if (match(TK_OPENING_BRACES) && !ty->proto) {
    ty->proto = calloc(1, sizeof(struct proto));
    ty->proto->type = voidtype;
  }

  Node n = find_global(name);
  if (!n) {
    n = mknode(A_FUNCTION);
    n->name = name;
    n->type = ty;
    n->next = globals;
    globals = n;
  } else {
    if (!is_compatible_type(n->type, ty))
      error("conflicting types for function %s", name);
    n->type =
        n->body ? composite_type(n->type, ty) : composite_type(ty, n->type);
  }
  return n;
}

static Node mkstrlit(const char* str) {
  Node n = mknode(A_STRING_LITERAL);
  n->string_value = str;
  n->next = globals;
  n->type = array_type(chartype, strlen(str) + 1);
  globals = n;
  return n;
}

/*****************************
 * recursive parse procedure *
 *****************************/

// trans_unit:     { func_def | declaration }*
// =======================   Function DEF   =======================
// func_def:               declaration_specifiers declarator comp_stat <---+
// =======================   Declaration   =======================         |
// declaration:            declaration_specifiers                          |
//                         init_declarator { ',' init_declarator }* ';'    |
// declaration_specifiers: { type_specifier | type-qualifier }*            |
// type_specifier:         { 'void' | 'char' | 'int' | 'short' |           |
//                         'long' | 'unsigned'|'signed' }                  |
// type_qualifier:         { 'const' | 'volatile' | 'restrict' }           |
// init_declarator:        declarator { '=' expression }? -----------------+
// declarator:             pointer? direct_declarator suffix_declarator*
// pointer:                { '*' { type_qualifier }* }*
// direct_declarator:      '(' declarator ')' |  identifier | Empty
// suffix_declarator:      array_declarator | function_declarator
// array_declarator:       '[' expression ']'
// function_declarator:    '(' { parameter_list }* ')'
// parameter_list:         parameter_declaration { ',' parameter_declaration }*
// parameter_declaration:  declaration_specifiers declarator
// type_name:              declaration_specifiers declarator
// =======================   Statement   =======================
// statement:      expr_stat | comp_stat
//                 selection-stat | iteration-stat | jump_stat
// selection-stat: if_stat
// if_stat:        'if' '('  expr_stat ')' statement { 'else' statement }
// iteration-stat: while_stat | dowhile_stat | for_stat
// while_stat:     'while' '(' expr_stat ')' statement
// dowhile_stat:   'do' statement 'while' '(' expression ')' ';'
// for_stat:       'for' '(' {expression}; {expression}; {expression}')'
//                 statement
// jump_stat:      break ';' | continue ';' | 'return' expression? ';'
// comp_stat:      '{' {declaration}* {stat}* '}'
// expr_stat:      {expression}? ';'
// =======================   Expression   =======================
// expression:     comma_expr
// comma_expr:     assign_expr { ', ' assign_expr }*
// assign_expr:    conditional_expr { '=' assign_expr }
// conditional_expr: logical_or_expr { '?' expression : conditional_expr }?
// logical_or_expr:  logical_and_expr { '||' logical_and_expr }*
// logical_and_expr: equality_expr { '&&' inclusive_or_expr }*
// inclusive_or_expr:exclusive_or_expr { '|' exclusive_or_expr }*
// exclusive_or_expr:bitwise_and_expr { '^' bitwise_and_expr }*
// bitwise_and_expr: equality_expr { '&' equality_expr}*
// equality_expr:  relational_expr { '==' | '!='  relational_expr }*
// relational_expr:  sum_expr { '>' | '<' | '>=' | '<=' shift_expr}*
// shift_expr:     sum_expr { '<<' | '>>' sum_expr }*
// sum_expr        :mul_expr { '+'|'-' mul_exp }*
// mul_exp:        unary_expr { '*'|'/' | '%' unary_expr }*
// unary_expr:    { '*'|'&'|'+'|'- '|'~'|'!'|'++'|'--'|'sizeof' }? postfix_expr
// postfix_expr:  primary_expr { arg_list|array_subscripting|'++'|'--' }*
// primary_expr:  identifier | number | string-literal | '('expression ')'
// arg_list:       '(' expression { ',' expression } ')'
// array_subscripting: '[' expression ']'

static Node trans_unit();
static Node declaration();
static Type declaration_specifiers();
static Node init_declarator(Type ty);  // scope????
static Type declarator(Type ty, const char** name);
static Type pointer(Type ty);
static Type direct_declarator(Type ty, const char** name);
static Type array_declarator(Type base);
static Type function_declarator(Type ty);
static Type type_name();
static Node function(Node f);
static Node statement(int reuse_scope);
static Node comp_stat(int reuse_scope);
static Node if_stat();
static Node while_stat();
static Node dowhile_stat();
static Node for_stat();
static Node expr_stat();
static Node expression();
static Node comma_expr();
static Node assign_expr();
static Node conditional_expr();
static Node logical_or_expr();
static Node logical_and_expr();
static Node inclusive_or_expr();
static Node exclusive_or_expr();
static Node bitwise_and_expr();
static Node eq_expr();
static Node rel_expr();
static Node shift_expr();
static Node sum_expr();
static Node mul_expr();
static Node unary_expr();
static Node postfix_expr();
static Node primary_expr();
static Node func_call(Node n);
static Node array_subscripting(Node n);
static Node variable(Node n);

// current function being parsed
static Node current_func;

static Node trans_unit() {
  while (t)
    declaration();

  return globals;
}

// for local declaration(called by comp_stat)
//    make local variable(return node list for init assign)
// for global declaration(called by trans_unit)
//    make global variable(set it init_value member, return NULL)
//    make function node(return the function node)
// for function definition(called by trans_unit)
//    make function node && fill body(return node for the function)
static Node declaration() {
  Type ty = declaration_specifiers();
  Node n = init_declarator(ty);
  if (n && n->kind == A_FUNCTION) {
    if (match(TK_OPENING_BRACES))
      return function(n);
    n = NULL;
  }

  struct node head = {0};
  Node last = &head;
  last->next = n;
  while (last->next)
    last = last->next;
  while (!consume(TK_SIMI)) {
    expect(TK_COMMA);
    n = init_declarator(ty);
    last->next = (n && n->kind == A_FUNCTION) ? NULL : n;
    while (last->next)
      last = last->next;
  }
  return head.next;
}

#define get_specifier(specs, spec) (specs & spec)
#define set_specifier(specs, spec) (specs |= spec)
enum {
  // type specifiers
  SPEC_VOID = 1 << 0,
  SPEC_CHAR = 1 << 1,
  SPEC_SHORT = 1 << 2,
  SPEC_INT = 1 << 3,
  SPEC_LONG1 = 1 << 4,
  SPEC_LONG2 = 1 << 5,
  SPEC_SIGNED = 1 << 6,
  SPEC_UNSIGNED = 1 << 7,
  SPEC_VOLATILE = 1 << 8,
  SPEC_CONST = 1 << 9,
  SPEC_RESTRICT = 1 << 10
};

Type declaration_specifiers() {
  Type ty;
  int specifiers = 0;
  for (;;) {
    if (consume(TK_VOID)) {
      if (get_specifier(specifiers, SPEC_VOID))
        error("two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_VOID);
    } else if (consume(TK_CHAR)) {
      if (get_specifier(specifiers, SPEC_CHAR))
        error("two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_CHAR);
    } else if (consume(TK_SHORT)) {
      if (get_specifier(specifiers, SPEC_SHORT))
        error("two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_SHORT);
    } else if (consume(TK_INT)) {
      if (get_specifier(specifiers, SPEC_INT))
        error("two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_INT);
    } else if (consume(TK_LONG)) {
      if (get_specifier(specifiers, SPEC_LONG1)) {
        if (get_specifier(specifiers, SPEC_LONG2))
          error("two or more data types in declaration specifiers");
        else
          set_specifier(specifiers, SPEC_LONG2);
      } else {
        set_specifier(specifiers, SPEC_LONG1);
      }
    } else if (consume(TK_UNSIGNED)) {
      if (get_specifier(specifiers, SPEC_UNSIGNED))
        error("two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_UNSIGNED);
    } else if (consume(TK_SIGNED)) {
      if (get_specifier(specifiers, SPEC_SIGNED))
        error("two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_SIGNED);
    } else if (consume(TK_CONST)) {
      set_specifier(specifiers, SPEC_CONST);
    } else if (consume(TK_VOLATILE)) {
      set_specifier(specifiers, SPEC_VOLATILE);
    } else if (consume(TK_RESTRICT)) {
      set_specifier(specifiers, SPEC_RESTRICT);
    } else
      break;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.7.2
  int type_spec = specifiers & ((1 << 8) - 1);
  switch (type_spec) {
    case 0:
      error("no data type in declaration specifiers");
      break;
    case SPEC_VOID:
      ty = voidtype;
      break;
    case SPEC_CHAR:
    case SPEC_CHAR | SPEC_SIGNED:
      ty = chartype;
      break;
    case SPEC_CHAR | SPEC_UNSIGNED:
      ty = uchartype;
      break;
    case SPEC_SHORT:
    case SPEC_SIGNED | SPEC_SHORT:
    case SPEC_SHORT | SPEC_INT:
    case SPEC_SIGNED | SPEC_SHORT | SPEC_INT:
      ty = shorttype;
      break;
    case SPEC_UNSIGNED | SPEC_SHORT:
    case SPEC_UNSIGNED | SPEC_SHORT | SPEC_INT:
      ty = ushorttype;
      break;
    case SPEC_INT:
    case SPEC_SIGNED:
    case SPEC_INT | SPEC_SIGNED:
      ty = inttype;
      break;
    case SPEC_UNSIGNED:
    case SPEC_UNSIGNED | SPEC_INT:
      ty = uinttype;
      break;
    case SPEC_LONG1:
    case SPEC_SIGNED | SPEC_LONG1:
    case SPEC_LONG1 | SPEC_INT:
    case SPEC_SIGNED | SPEC_LONG1 | SPEC_INT:
      ty = longtype;
      break;
    case SPEC_UNSIGNED | SPEC_LONG1:
    case SPEC_UNSIGNED | SPEC_LONG1 | SPEC_INT:
      ty = ulongtype;
      break;
    case SPEC_LONG1 | SPEC_LONG2:
    case SPEC_SIGNED | SPEC_LONG1 | SPEC_LONG2:
    case SPEC_LONG1 | SPEC_LONG2 | SPEC_INT:
    case SPEC_SIGNED | SPEC_LONG1 | SPEC_LONG2 | SPEC_INT:
      ty = longtype;
      break;
    case SPEC_UNSIGNED | SPEC_LONG1 | SPEC_LONG2:
    case SPEC_UNSIGNED | SPEC_LONG1 | SPEC_LONG2 | SPEC_INT:
      ty = ulongtype;
      break;
    default:
      error("two or more data types in declaration specifiers");
  }

  // cache nothing in register, so all variable is VOLATILE?
  if (get_specifier(specifiers, SPEC_CONST))
    ty = const_type(ty);
  if (get_specifier(specifiers, SPEC_RESTRICT))
    error("not implemented: restrict");
  return ty;
}

// returned value for different kind of declarotor
//    function:        function node in global
//    local variable:  nodes for init assign
//    global variable: NULL
static Node init_declarator(Type ty) {
  const char* name = NULL;
  ty = declarator(ty, &name);
  if (!name)
    error("identifier name required in declarator");

  if (is_funcion(ty))
    return mkfunc(name, ty);

  Node n = current_func ? mklvar(name, ty) : mkgvar(name, ty);
  if (consume(TK_EQUAL)) {
    Node e = assign_expr();
    if (is_array(ty))
      error("not implemented: initialize array");
    if (current_func)
      return mkbinary(A_INIT, n, e);
    if (e->kind != A_NUM && e->kind != A_STRING_LITERAL)
      error(
          "global variable can only be initializd by integer constant or "
          "string literal");
    n->init_value = e;
    return NULL;
  }
  return NULL;
}

static Type placeholdertype =
    &(struct type){TY_PLACEHOLDER, -1, NULL, "placeholder"};

// replace placeholder type by real base type
static Type construct_type(Type base, Type ty) {
  if (ty == placeholdertype)
    return base;
  if (is_array(ty))
    return array_type(construct_type(base, ty->base),
                      ty->size / ty->base->size);
  if (is_ptr(ty))
    return ptr_type(construct_type(base, ty->base));

  if (is_funcion(ty))
    return function_type(construct_type(base, ty->base), ty->proto);

  error("can't construct type");
  return NULL;
}

// TODO: FIX THIS
// We can't parse an abstract function type (function declaration without name).
// The parenthese is ambiguous(complicated direct_declarator or function
// declarator suffix). For example, in "int *(int);", the '(' may be parsed as
// a complicated direct_declarator, however it should be a suffix indicate a
// function declarator.  It not a problem for function pointer, for example:
// "int (*)(int,...)".
// This should be supported because:
//   (http://port70.net/~nsz/c/c99/n1256.html#6.3.2.1p4)
static Type declarator(Type base, const char** name) {
  base = pointer(base);

  Type ty = direct_declarator(placeholdertype, name);

  if (match(TK_OPENING_PARENTHESES))
    base = function_declarator(base);
  if (match(TK_OPENING_BRACKETS))
    base = array_declarator(base);

  return construct_type(base, ty);
}

static Type pointer(Type ty) {
  while (consume(TK_STAR)) {
    ty = ptr_type(ty);
    int q_const = 0, q_restrict = 0;
    for (;;) {
      if (consume(TK_CONST))
        q_const = 1;
      else if (consume(TK_VOLATILE))
        ;
      else if (consume(TK_RESTRICT))
        q_restrict = 1;
      else {
        if (q_restrict)
          error("not implemented: restrict pointer");
        if (q_const)
          ty = const_type(ty);
        break;
      }
    }
  }
  return ty;
}

static Type direct_declarator(Type ty, const char** name) {
  Token tok;
  if ((tok = consume(TK_IDENT))) {
    *name = tok->name;
    return ty;
  } else if (consume(TK_OPENING_PARENTHESES)) {
    ty = declarator(ty, name);
    expect(TK_CLOSING_PARENTHESES);
    return ty;
  }
  return ty;
}

static Type array_declarator(Type base) {
  if (consume(TK_OPENING_BRACKETS)) {
    if (consume(TK_CLOSING_BRACKETS))
      return array_type(array_declarator(base), 0);

    Node n = expression();
    expect(TK_CLOSING_BRACKETS);
    if (n->kind != A_NUM)
      error("not implemented: non integer constant array length");
    if (n->intvalue == 0)
      error("array size shall greater than zeror");
    return array_type(array_declarator(base), n->intvalue);
  }
  return base;
}

static Proto mkproto(Type type, const char* name) {
  Proto p = calloc(1, sizeof(struct proto));
  p->type = type;
  p->name = name;
  return p;
}

static void link_proto(Proto head, Proto p) {
  if ((head->next && head->next->type == voidtype) ||  // other follow void
      (p->type == voidtype && head->next))             // void is not first
    error("void must be the only parameter");

  while (head->next) {
    if (p->name && head->next->name == p->name)
      error("redefinition of parameter %s", p->name);
    head = head->next;
  }
  head->next = p;
}

static Type function_declarator(Type ty) {
  struct proto head = {0};

  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    if (consume(TK_ELLIPSIS)) {
      if (!head.next)
        error("a named paramenter is required before ...");
      link_proto(&head, mkproto(type(TY_VARARG, NULL, 0), NULL));
      expect(TK_CLOSING_PARENTHESES);
      break;
    }

    const char* name = NULL;
    Type type = declarator(declaration_specifiers(), &name);
    if (is_array(type))
      type = array_to_ptr(type);
    link_proto(&head, mkproto(type, name));

    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  }

  return function_type(ty, head.next);
}

static Type type_name() {
  Type ty = declaration_specifiers();
  const char* name = NULL;
  ty = declarator(ty, &name);
  if (name)
    error("type name required");
  return ty;
}

static Node function(Node f) {
  if (f->body)
    error("redefinition of function %s", f->name);

  enter_scope();
  locals = NULL;
  // paramenter
  f->params = mklist(NULL);
  if (f->type->proto->type != voidtype)
    for (Proto p = f->type->proto; p; p = p->next) {
      if (!p->name)
        error("omitting parameter name in function definition");
      list_insert(f->params, mklist(mklvar(p->name, p->type)));
    }
  current_func = f;
  f->body = comp_stat(1);
  f->locals = locals;
  current_func = NULL;
  exit_scope();

  return NULL;
}

static Node statement(int reuse_scope) {
  // compound statement
  if (match(TK_OPENING_BRACES)) {
    return comp_stat(reuse_scope);
  }

  // if statement
  if (match(TK_IF)) {
    return if_stat();
  }

  // while statement
  if (match(TK_WHILE)) {
    return while_stat();
  }

  // do-while statement
  if (match(TK_DO)) {
    return dowhile_stat();
  }

  // for statement
  if (match(TK_FOR)) {
    return for_stat();
  }

  // jump_stat
  if (consume(TK_BREAK)) {
    expect(TK_SIMI);
    return mknode(A_BREAK);
  }
  if (consume(TK_CONTINUE)) {
    expect(TK_SIMI);
    return mknode(A_CONTINUE);
  }
  if (consume(TK_RETURN)) {
    Node n = mknode(A_RETURN);
    if (consume(TK_SIMI))
      return n;
    n->body = mkcvs(current_func->type->base, expression());
    expect(TK_SIMI);
    return n;
  }

  // expr statement
  return expr_stat();
}

static Node comp_stat(int reuse_scope) {
  struct node head = {0};
  Node last = &head;
  expect(TK_OPENING_BRACES);
  if (!reuse_scope)
    enter_scope();
  while (t->kind != TK_CLOSING_BRACES) {
    if (match_type_spec())
      last->next = declaration();
    else
      last->next = statement(0);
    while (last->next)
      last = last->next;
  }
  expect(TK_CLOSING_BRACES);
  if (!reuse_scope)
    exit_scope();
  return mkaux(A_BLOCK, head.next);
}

static Node if_stat() {
  Node n = mknode(A_IF);
  expect(TK_IF);
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  n->then = statement(0);
  if (consume(TK_ELSE)) {
    n->els = statement(0);
  }
  return n;
}

static Node while_stat() {
  Node n = mknode(A_FOR);
  expect(TK_WHILE);
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  n->body = statement(0);
  return n;
}

static Node dowhile_stat() {
  Node n = mknode(A_DOWHILE);
  expect(TK_DO);
  n->body = statement(0);
  expect(TK_WHILE);
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  expect(TK_SIMI);
  return n;
}

static Node for_stat() {
  Node n = mknode(A_FOR);
  expect(TK_FOR);
  expect(TK_OPENING_PARENTHESES);
  enter_scope();
  if (!consume(TK_SIMI))
    n->init = expr_stat();
  if (!consume(TK_SIMI)) {
    n->cond = expression();
    expect(TK_SIMI);
  }
  if (!consume(TK_CLOSING_PARENTHESES)) {
    n->post = mkaux(A_EXPR_STAT, expression());
    expect(TK_CLOSING_PARENTHESES);
  }
  n->body = statement(1);
  exit_scope();
  return n;
}

static Node expr_stat() {
  if (consume(TK_SIMI)) {
    return NULL;
  }

  Node n = expression();
  expect(TK_SIMI);
  return mkaux(A_EXPR_STAT, n);
}

static Node expression() {
  return comma_expr();
}

static Node comma_expr() {
  Node n = assign_expr();
  for (;;) {
    if (consume(TK_COMMA))
      n = mkbinary(A_COMMA, n, assign_expr());
    else
      return n;
  }
}

static Node assign_expr() {
  Node n = conditional_expr();
  for (;;) {
    if (consume(TK_EQUAL))
      n = mkbinary(A_ASSIGN, n, assign_expr());
    else if (consume(TK_STAR_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_MUL, n, assign_expr()));
    else if (consume(TK_SLASH_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_DIV, n, assign_expr()));
    else if (consume(TK_PLUS_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_ADD, n, assign_expr()));
    else if (consume(TK_MINUS_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_SUB, n, assign_expr()));
    else if (consume(TK_LEFT_SHIFT_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_LEFT_SHIFT, n, assign_expr()));
    else if (consume(TK_RIGHT_SHIFT_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_RIGHT_SHIFT, n, assign_expr()));
    else if (consume(TK_AND_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_B_AND, n, assign_expr()));
    else if (consume(TK_CARET_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_B_EXCLUSIVEOR, n, assign_expr()));
    else if (consume(TK_BAR_EQUAL))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_B_INCLUSIVEOR, n, assign_expr()));
    else
      return n;
  }
}

static Node conditional_expr() {
  Node n = logical_or_expr();
  if (consume(TK_QUESTIONMARK)) {
    Node e = expression();
    expect(TK_COLON);
    n = mkternary(n, e, conditional_expr());
  }
  return n;
}

static Node logical_or_expr() {
  Node n = logical_and_expr();
  for (;;) {
    if (consume(TK_BAR_BAR))
      n = mkbinary(A_L_OR, n, logical_and_expr());
    else
      return n;
  }
}

static Node logical_and_expr() {
  Node n = inclusive_or_expr();
  for (;;) {
    if (consume(TK_AND_AND))
      n = mkbinary(A_L_AND, n, inclusive_or_expr());
    else
      return n;
  }
}

static Node inclusive_or_expr() {
  Node n = exclusive_or_expr();
  for (;;) {
    if (consume(TK_BAR))
      n = mkbinary(A_B_INCLUSIVEOR, n, exclusive_or_expr());
    else
      return n;
  }
}

static Node exclusive_or_expr() {
  Node n = bitwise_and_expr();
  for (;;) {
    if (consume(TK_CARET))
      n = mkbinary(A_B_EXCLUSIVEOR, n, bitwise_and_expr());
    else
      return n;
  }
}

static Node bitwise_and_expr() {
  Node n = eq_expr();
  for (;;) {
    if (consume(TK_AND))
      n = mkbinary(A_B_AND, n, eq_expr());
    else
      return n;
  }
}

static Node eq_expr() {
  Node n = rel_expr();
  for (;;) {
    if (consume(TK_EQUAL_EQUAL))
      n = mkbinary(A_EQ, n, rel_expr());
    else if (consume(TK_NOT_EQUAL))
      n = mkbinary(A_NE, n, rel_expr());
    else
      return n;
  }
}

static Node rel_expr() {
  Node n = shift_expr();
  for (;;) {
    if (consume(TK_GREATER))
      n = mkbinary(A_GT, n, shift_expr());
    else if (consume(TK_LESS))
      n = mkbinary(A_LT, n, shift_expr());
    else if (consume(TK_GREATER_EQUAL))
      n = mkbinary(A_GE, n, shift_expr());
    else if (consume(TK_LESS_EQUAL))
      n = mkbinary(A_LE, n, shift_expr());
    else
      return n;
  }
}

static Node shift_expr() {
  Node n = sum_expr();
  for (;;) {
    if (consume(TK_LEFT_SHIFT))
      n = mkbinary(A_LEFT_SHIFT, n, sum_expr());
    else if (consume(TK_RIGHT_SHIFT))
      n = mkbinary(A_RIGHT_SHIFT, n, sum_expr());
    else
      return n;
  }
}

static Node sum_expr() {
  Node n = mul_expr();
  for (;;) {
    if (consume(TK_PLUS))
      n = mkbinary(A_ADD, n, mul_expr());
    else if (consume(TK_MINUS))
      n = mkbinary(A_SUB, n, mul_expr());
    else
      return n;
  }
}

static Node mul_expr() {
  Node n = unary_expr();
  for (;;) {
    if (consume(TK_STAR))
      n = mkbinary(A_MUL, n, unary_expr());
    else if (consume(TK_SLASH))
      n = mkbinary(A_DIV, n, unary_expr());
    else if (consume(TK_PERCENT))
      n = mkbinary(A_MOD, n, unary_expr());
    else
      return n;
  }
}

static Node unary_expr() {
  if (consume(TK_AND))
    return mkunary(A_ADDRESS_OF, unary_expr());
  if (consume(TK_STAR))
    return mkunary(A_DEFERENCE, unary_expr());
  if (consume(TK_PLUS))
    return mkunary(A_PLUS, unary_expr());
  if (consume(TK_MINUS))
    return mkunary(A_MINUS, unary_expr());
  if (consume(TK_TILDE))
    return mkunary(A_B_NOT, unary_expr());
  if (consume(TK_EXCLAMATION))
    return mkunary(A_L_NOT, unary_expr());
  if (consume(TK_PLUS_PLUS)) {
    Node u = unary_expr();
    return mkbinary(A_ASSIGN, u, mkbinary(A_ADD, u, mkicons(1)));
  }
  if (consume(TK_MINUS_MINUS)) {
    Node u = unary_expr();
    return mkbinary(A_ASSIGN, u, mkbinary(A_SUB, u, mkicons(1)));
  }
  if (consume(TK_SIZEOF)) {
    Token back = t;
    if (consume(TK_OPENING_PARENTHESES) && match_type_spec()) {
      Node u = mknode(A_NOOP);
      u->type = type_name();
      expect(TK_CLOSING_PARENTHESES);
      return mkunary(A_SIZE_OF, u);
    }
    t = back;
    return mkunary(A_SIZE_OF, unary_expr());
  }

  return postfix_expr();
}

static Node postfix_expr() {
  Node n = primary_expr();
  for (;;) {
    if (match(TK_OPENING_PARENTHESES))
      n = func_call(n);
    else if (match(TK_OPENING_BRACKETS))
      n = array_subscripting(n);
    else if (consume(TK_PLUS_PLUS))
      n = mkunary(A_POSTFIX_INC, n->kind == A_IDENT ? variable(n) : n);
    else if (consume(TK_MINUS_MINUS))
      n = mkunary(A_POSTFIX_DEC, n->kind == A_IDENT ? variable(n) : n);
    else
      return n->kind == A_IDENT ? variable(n) : n;
  }
}

static Node primary_expr() {
  Token tok;

  // constant
  if ((tok = consume(TK_NUM))) {
    Node n = mknode(A_NUM);
    n->type = inttype;
    n->intvalue = tok->value;
    return n;
  }

  // string literal
  if ((tok = consume(TK_STRING)))
    return mkstrlit(tok->name);

  // identifier
  if ((tok = consume(TK_IDENT))) {
    Node n = mknode(A_IDENT);
    n->name = tok->name;
    return n;
  }

  // parentheses
  if (consume(TK_OPENING_PARENTHESES)) {
    Node n = expression();
    expect(TK_CLOSING_PARENTHESES);
    return n;
  }

  error("parse: primary get unexpected token \"%s\"", t->name);
  return NULL;
}

static Node func_call(Node n) {
  if (n->kind != A_IDENT)
    error("not implemented");
  const char* name = n->name;

  Node f = find_global(name);
  if (f && !(f->kind == A_FUNCTION))
    error("called object \"%s\" is not a function", name);
  if (!f) {  //  implicit function declaration
    warn("implicit declaration of function %s", name);
    f = mkfunc(name, function_type(inttype, NULL));
  }

  n = mknode(A_FUNC_CALL);
  n->name = name;
  n->type = f->type->base;
  // function argument lists
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.2p6
  n->args = mklist(NULL);
  Proto param = f->type->proto;
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    Node arg = assign_expr();
    if (is_array(arg->type))
      arg = mkcvs(array_to_ptr(arg->type), arg);
    if (!f->type->proto) {
      arg = mkcvs(default_argument_promoe(arg->type), arg);
    } else if (!param || param->type == voidtype) {
      error("too many arguments to function %s", name);
    } else if (param->type->kind == TY_VARARG) {
      arg = mkcvs(default_argument_promoe(arg->type), arg);
    } else {
      arg = mkcvs(unqual(param->type), arg);
      param = param->next;
    }
    list_insert(n->args, mklist(arg));
    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  };
  return n;
}

static Node array_subscripting(Node n) {
  if (n->kind == A_IDENT) {
    n = find_var(n->name);
    if (!n)
      error("undefined array %s", n->name);
  }

  while (consume(TK_OPENING_BRACKETS)) {
    if (!is_ptr(n->type))
      error("only array/pointer can be subscripted");
    Node s = mknode(A_ARRAY_SUBSCRIPTING);
    s->index = mkcvs(longtype, expression());
    s->array = n;
    s->type = n->type->base;
    n = s;
    expect(TK_CLOSING_BRACKETS);
  }
  return n;
}

static Node variable(Node n) {
  n = find_var(n->name);
  if (!n)
    error("undefined variable %s", n->name);
  return n;
}

Node parse(Token root) {
  t = root;
  return trans_unit();
}