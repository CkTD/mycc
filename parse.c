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

/*******************
 * create AST Node *
 *******************/

static int is_lvalue(Node node) {
  switch (node->kind) {
    case A_VAR:
      return !is_array(node->type);
    case A_DEFERENCE:
    case A_ARRAY_SUBSCRIPTING:
      return 1;
    default:
      return 0;
  }
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
  // body->type -> type
  if (is_ptr(t) && is_ptr(body->type)) {
    if (!is_ptr_compatiable(t, body->type))
      warn("convert between incompatiable pointers");
    return body;
  } else if (is_ptr(t) || is_ptr(body->type))
    warn("convert between pointer and integeral types");
  else if (!is_integer(t) || !is_integer(body->type))
    error("not implemented: what type?");
  n->type = t;
  return n;
}

// kind:
//     A_ADDRESS_OF, A_DEREFERENCE
//     A_MINUS, A_PLUS, A_B_NOT, A_L_NOT
//     A_POSTFIX_INC, A_POSTFIX_DEC
//     A_SIZE_OF
static Node mkunary(int kind, Node left) {
  Node n = mknode(kind);
  n->left = left;

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
    if (!is_lvalue(n->left) ||
        !(is_arithmetic(n->left->type) || is_ptr(n->left->type)))
      error("invalid operand");
    n->type = n->left->type;
    return mkcvs(integral_promote(n->type), n);
  }

  if (kind == A_SIZE_OF) {
    Node s = mkicons(n->left->type->size);
    s->type = uinttype;
    return s;
  }

  error("invalid unary operator");
  return NULL;
}

// usual arithmetic conversions for binary expressions
static Type usual_arithmetic_conversions(Node node) {
  Type t = usual_arithmetic_type(node->left->type, node->right->type);
  node->left = mkcvs(t, node->left);
  node->right = mkcvs(t, node->right);
  return t;
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
  n->left = left;
  n->right = right;

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.17
  if (kind == A_COMMA) {
    n->type = n->right->type;
    return n;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.16
  if (kind == A_ASSIGN) {
    if (!is_lvalue(n->left))
      error("lvalue required as left operand of assignment");
    n->right = mkcvs(n->left->type, n->right);
    n->type = n->left->type;
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
      if (!is_ptr_compatiable(n->left->type, n->right->type))
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
  n->cond = cond;
  n->left = left;
  n->right = right;

  if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
    n->type = usual_arithmetic_conversions(n);
    return n;
  }

  if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
    if (!is_ptr_compatiable(n->left->type, n->right->type))
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
// global variables/function are accumulated to this list during parsing
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
  if (v && v->is_function)
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
// declaration:    type_spec init_declarator { ',' init_declarator }* ';'
// type_spec:      'unsigned'? { 'char' | 'int' | 'short' | 'long' }
// init_declarator:declarator { '=' expression }?
// declarator:     {'*'}* identifier { '[' expression ']' }?
// func_def:       type_spec identifier '(' param_list ')' comp_stat
// param_list:     param_declaration { ',' param_declaration }
// param_declaration: { type_spec }+ identifier
// statement:      expr_stat | print_stat | comp_stat
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
// print_stat:     'print' expr;
// expr_stat:      {expression}? ;
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
static Node init_declarator(Type ty);  // scope????
static Node declarator(Type ty);
static Type type_spec();
static Node function();
static Node param_list();
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

static Node current_func;
static int check_next_top_level_item() {
  Token back = t;
  type_spec();
  while (consume(TK_STAR))
    ;
  int is_function = consume(TK_IDENT) && consume(TK_OPENING_PARENTHESES);
  t = back;
  return is_function;
}

static Node trans_unit() {
  while (t) {
    if (check_next_top_level_item())  // function
      function();
    else  // global variable
      declaration();
  }
  return globals;
}

static Node declaration() {
  Type ty = type_spec();
  struct node head = {};
  Node last = &head;
  last->next = init_declarator(ty);
  while (last->next)
    last = last->next;
  while (!consume(TK_SIMI)) {
    expect(TK_COMMA);
    last->next = init_declarator(ty);
    while (last->next)
      last = last->next;
  }
  return head.next;
}

Type type_spec() {
  Type ty = NULL;
  if (consume(TK_VOID))
    ty = voidtype;
  else if (consume(TK_UNSIGNED)) {
    if (consume(TK_CHAR))
      ty = uchartype;
    else if (consume(TK_SHORT))
      ty = ushorttype;
    else if (consume(TK_INT))
      ty = uinttype;
    else if (consume(TK_LONG))
      ty = ulongtype;
  } else if (consume(TK_CHAR))
    ty = chartype;
  else if (consume(TK_SHORT))
    ty = shorttype;
  else if (consume(TK_INT))
    ty = inttype;
  else if (consume(TK_LONG))
    ty = longtype;

  return ty;
}

static Node init_declarator(Type ty) {
  Node n = declarator(ty);
  if (consume(TK_EQUAL)) {
    Node e = assign_expr();
    if (is_ptr(ty))
      error("not implemented: initialize array");
    if (current_func)
      return mkaux(A_EXPR_STAT, mkbinary(A_ASSIGN, n, e));
    if (e->kind != A_NUM && e->kind != A_STRING_LITERAL)
      error(
          "global variable can only be initializd by integer constant or "
          "string literal");
    n->init_value = e;
    return NULL;
  }
  return NULL;
}

static Type array_postfix(Type base) {
  if (consume(TK_OPENING_BRACKETS)) {
    if (consume(TK_CLOSING_BRACKETS))
      return array_type(array_postfix(base), 0);

    Node n = expression();
    expect(TK_CLOSING_BRACKETS);
    if (n->kind != A_NUM)
      error("not implemented: non integer constant array length");
    if (n->intvalue == 0)
      error("array size shall greater than zeror");
    return array_type(array_postfix(base), n->intvalue);
  }
  return base;
}

static Node declarator(Type ty) {
  // ptr
  while (ty && consume(TK_STAR))
    ty = ptr_type(ty);

  const char* name = expect(TK_IDENT)->name;

  // array
  ty = array_postfix(ty);

  return current_func ? mklvar(name, ty) : mkgvar(name, ty);
}

static int protos_compatitable(Node p1, Node p2) {
  Node n1 = p1->next, n2 = p2->next;
  for (; n1 != p1 && n2 != p2; n1 = n1->next, n2 = n2->next) {
    if (n1->body->type != n2->body->type)
      return 0;
  }

  return n1 == p1 && n2 == p2;
}

static Node function() {
  Type ty = type_spec();
  while (consume(TK_STAR))
    ty = ptr_type(ty);

  const char* name = expect(TK_IDENT)->name;

  enter_scope();
  locals = NULL;
  Node protos = param_list();

  Node n = find_global(name);
  if (n) {  // the function has been declared
    if (!n->is_function || n->body) {
      error("redefine symbol %s", name);
    } else {
      if (n->type != ty || !protos_compatitable(protos, n->protos))
        error("conflict types for %s", name);
      n->protos = protos;
    }
  } else {
    n = mknode(A_FUNCTION);
    n->name = name;
    n->type = ty;
    n->protos = protos;
    n->is_function = 1;
    n->next = globals;
    globals = n;
  }

  if (consume(TK_SIMI)) {  // function declaration whith out definition
  } else {
    current_func = n;
    n->body = comp_stat(1);
    n->locals = locals;
    current_func = NULL;
  }

  exit_scope();

  return NULL;
}

Node param_list() {
  Node head = mklist(NULL);
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    if (consume(TK_ELLIPSIS)) {
      if (list_empty(head))
        error("a named paramenter is required before...");
      list_insert(head, mklist(mknode(A_VARARG)));
      expect(TK_CLOSING_PARENTHESES);
      break;
    }

    Type ty = type_spec();
    while (consume(TK_STAR))
      ty = ptr_type(ty);
    const char* name = expect(TK_IDENT)->name;
    list_insert(head, mklist(mklvar(name, ty)));

    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  }
  return head;
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
    n->body = mkcvs(current_func->type, expression());
    expect(TK_SIMI);
    return n;
  }

  // expr statement
  return expr_stat();
}

static Node comp_stat(int reuse_scope) {
  struct node head = {};
  Node last = &head;
  expect(TK_OPENING_BRACES);
  if (!reuse_scope)
    enter_scope();
  while (t->kind != TK_CLOSING_BRACES) {
    if (t->kind >= TK_VOID && t->kind <= TK_LONG)
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
  if (consume(TK_SIZEOF))
    return mkunary(A_SIZE_OF, unary_expr());
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

  n = mknode(A_FUNC_CALL);
  Node f = find_global(name);

  if (f && !f->is_function)
    error("called object \"%s\" is not a function", name);

  n->callee_name = name;
  n->type = f ? f->type : inttype;  //  implicit function declaration

  // function argument lists
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.2p6
  Node args = mklist(NULL);
  Node cur_param = (f && f->protos) ? f->protos->next : NULL;
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    Node arg = assign_expr();

    if (!f || !f->protos) {  // function without prototype
      arg = mkcvs(integral_promote(arg->type), arg);
    } else {  // function with porotype
      if (cur_param == f->protos) {
        warn("too many arguments to function %s", f->name);
      } else {
        if (cur_param->body->kind == A_VARARG) {
          arg = mkcvs(integral_promote(arg->type), arg);
        } else {
          arg = mkcvs(cur_param->body->type, arg);
          cur_param = cur_param->next;
        }
      }
    }

    list_insert(args, mklist(arg));
    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  };
  n->args = args;
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