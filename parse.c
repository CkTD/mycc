#include "inc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

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

static Node mkaux(int kind, Node body) {
  Node n = mknode(kind);
  n->body = body;
  return n;
}

static Node mkuniary(int kind, Node left) {
  Node n = mknode(kind);
  n->left = left;
  if (kind == A_ADDRESS_OF) {
    if (left->kind == A_DEFERENCE)
      return left->left;
    if (!is_lvalue(left))
      error("lvalue required as unary '&' operand");
    n->type = ptr_type(left->type);
  } else if (kind == A_DEFERENCE) {
    if (left->kind == A_ADDRESS_OF)
      return left->left;
    if (left->type->kind != TY_POINTER)
      error("pointer required as unary '*' operand");
    n->type = deref_type(left->type);
  } else
    error("unknown uniary operator");
  return n;
}

static Node mkcvs(Type t, Node body) {
  if (body->type == t)
    return body;

  Node n = mkaux(A_CONVERSION, body);
  // body->type -> type
  if (is_pointer(t) && is_pointer(body->type))
    warn("convert between different pointer types");
  else if (is_pointer(t) || is_pointer(body->type))
    warn("convert between pointer and integeral types");
  n->type = t;
  return n;
}

static Node mkbinary(int kind, Node left, Node right) {
  Node n = mknode(kind);
  if (kind == A_ASSIGN) {
    right = mkcvs(left->type, right);
    n->type = left->type;
  } else {
    Type t = usual_arithmetic_conversion(left->type, right->type);
    left = mkcvs(t, left);
    right = mkcvs(t, right);
    // http://port70.net/~nsz/c/c99/n1256.html#6.5.8p6
    // http://port70.net/~nsz/c/c99/n1256.html#6.5.9p3
    n->type = (kind >= A_EQ && kind <= A_GE) ? inttype : t;
  }
  n->left = left;
  n->right = right;
  return n;
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
// expression:     assign_expr
// assign_expr:    equality_expr { '=' expression }
// equality_expr:  relational_expr { '==' | '!='  relational_expr }
// relational_expr:  sum_expr { '>' | '<' | '>=' | '<=' sum_expr}
// sum_expr        :mul_expr { '+'|'-' mul_exp }
// mul_exp:        uniary_expr { '*'|'/' uniary_expr }
// uniary_expr:    { '*' | '&' }? primary
// primary:        identifier { arg_list | array_subscripting }? |
//                 number  | '('expression ')'
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
static Node assign_expr();
static Node uniary_expr();
static Node eq_expr();
static Node rel_expr();
static Node sum_expr();
static Node mul_expr();
static Node primary();
static Node func_call(const char* name);
static Node array_subscripting(const char* name);

static int is_function;
static int check_next_top_level_item() {
  Token back = t;
  type_spec();
  is_function = consume(TK_IDENT) && consume(TK_OPENING_PARENTHESES);
  t = back;
  return is_function;
}

static Node trans_unit() {
  while (t->kind != TK_EOI) {
    check_next_top_level_item();
    if (is_function)  // function
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
    Node e = expression();
    if (is_pointer(ty))
      error("not implemented: initialize array");
    if (is_function)
      return mkbinary(A_ASSIGN, n, e);
    if (e->kind != A_NUM)
      error("initialize global value should be integer constant");
    n->init_value = e;
    return NULL;
  }
  return NULL;
}

static Node declarator(Type ty) {
  // ptr
  while (ty && consume(TK_STAR))
    ty = ptr_type(ty);

  const char* name = expect(TK_IDENT)->name;

  // array
  Node head = NULL;
  while (consume(TK_OPENING_BRACKETS)) {
    if (consume(TK_CLOSING_BRACKETS)) {
      Node n = mknode(A_NOOP);
      n->next = head;
      head = n;
    } else {
      Node n = expression();
      n->next = head;
      head = n;
      expect(TK_CLOSING_BRACKETS);
    }
  }
  for (; head; head = head->next) {
    if (head->kind == A_NUM) {
      if (head->intvalue == 0)
        error("array size shall greater than zeror");
      ty = array_type(ty, head->intvalue);
    } else if (head->kind == A_NOOP)
      ty = array_type(ty, 0);
    else
      error("not implemented: non integer constant array length");
  }

  if (is_function)
    return mklvar(name, ty);
  return mkgvar(name, ty);
}

static Node function() {
  locals = NULL;
  Node n = mknode(A_FUNC_DEF);
  n->type = type_spec();
  n->name = expect(TK_IDENT)->name;
  n->is_function = 1;

  enter_scope();
  n->protos = param_list();
  n->next = globals;
  globals = n;
  n->body = comp_stat(1);
  n->locals = locals;
  exit_scope();

  return NULL;
}

Node param_list() {
  Node head = mklist(NULL);
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
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

  // print statement
  if (consume(TK_PRINT)) {
    Node n = mknode(A_FUNC_CALL);
    n->args = list_insert(mklist(NULL), mklist(expression()));
    n->callee_name = "print";
    n->type = inttype;
    expect(TK_SIMI);
    return n;
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
    n->body = mkcvs(globals->type, expression());
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
  return assign_expr();
}

static Node assign_expr() {
  Node n = eq_expr();
  if (!match(TK_EQUAL)) {
    return n;
  }

  if (!is_lvalue(n))
    error("lvalue required as left operand of assignment");

  expect(TK_EQUAL);

  return mkbinary(A_ASSIGN, n, expression());
}

static Node eq_expr() {
  Node n = rel_expr();
  for (;;) {
    if (consume(TK_EQUALEQUAL))
      n = mkbinary(A_EQ, n, rel_expr());
    else if (consume(TK_NOTEQUAL))
      n = mkbinary(A_NE, n, rel_expr());
    else
      return n;
  }
}

static Node rel_expr() {
  Node n = sum_expr();
  for (;;) {
    if (consume(TK_GREATER))
      n = mkbinary(A_GT, n, sum_expr());
    else if (consume(TK_LESS))
      n = mkbinary(A_LT, n, sum_expr());
    else if (consume(TK_GREATEREQUAL))
      n = mkbinary(A_GE, n, sum_expr());
    else if (consume(TK_LESSEQUAL))
      n = mkbinary(A_LE, n, sum_expr());
    else
      return n;
  }
}

static Node sum_expr() {
  Node n = mul_expr();
  for (;;) {
    if (consume(TK_ADD))
      n = mkbinary(A_ADD, n, mul_expr());
    else if (consume(TK_SUB))
      n = mkbinary(A_SUB, n, mul_expr());
    else
      return n;
  }
}

static Node mul_expr() {
  Node n = uniary_expr();
  for (;;) {
    if (consume(TK_STAR))
      n = mkbinary(A_MUL, n, uniary_expr());
    else if (consume(TK_SLASH))
      n = mkbinary(A_DIV, n, uniary_expr());
    else
      return n;
  }
}

static Node uniary_expr() {
  if (consume(TK_AND))
    return mkuniary(A_ADDRESS_OF, uniary_expr());
  if (consume(TK_STAR))
    return mkuniary(A_DEFERENCE, uniary_expr());
  return primary();
}

static Node primary() {
  Token tok;

  // number
  if ((tok = consume(TK_NUM))) {
    Node n = mknode(A_NUM);
    n->type = inttype;
    n->intvalue = tok->value;
    return n;
  }

  if ((tok = consume(TK_IDENT))) {
    // function call
    if (match(TK_OPENING_PARENTHESES))
      return func_call(tok->name);

    // array subscripting
    if (match(TK_OPENING_BRACKETS))
      return array_subscripting(tok->name);

    // variable
    Node v = find_var(tok->name);
    if (!v)
      error("undefined variable");
    return v;
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

static Node func_call(const char* name) {
  Node n = mknode(A_FUNC_CALL);
  Node f = find_global(name);

  if (!f)
    error("function %s not defined", name);
  if (!f->is_function)
    error("%s is a variable, function expected", name);

  n->callee_name = name;
  n->type = f->type;

  // function argument lists
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.2p6
  Node args = mklist(NULL);
  Node cur_param = f->protos ? f->protos->next : NULL;
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    Node arg = expression();
    if (f->protos) {
      if (cur_param != f->protos) {
        arg = mkcvs(cur_param->body->type, arg);
        cur_param = cur_param->next;
      } else
        warn("too many arguments to function %s", f->name);
    } else
      arg = mkcvs(integral_promote(arg->type), arg);

    list_insert(args, mklist(arg));
    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  };
  n->args = args;
  return n;
}

static Node array_subscripting(const char* name) {
  expect(TK_OPENING_BRACKETS);
  Node a = find_var(name);
  if (!a)
    error("array \"%s\" not defined", name);
  if (!is_array(a->type))
    error("only array can be subscripted");
  Node n = mknode(A_ARRAY_SUBSCRIPTING);
  n->array = a;
  n->index = mkcvs(ulongtype, expression());
  n->type = a->type->base;
  expect(TK_CLOSING_BRACKETS);

  return n;
}

Node parse(Token root) {
  t = root;
  return trans_unit();
}