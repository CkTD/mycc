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
static Node mknode(int kind) {
  Node n = calloc(1, sizeof(struct node));
  n->kind = kind;
  return n;
}

static Node mkuniary(int op, Node left) {
  Node n = mknode(op);
  n->left = left;
  return n;
}

static Node mkbinary(int kind, Node left, Node right) {
  Node n = mkuniary(kind, left);
  n->right = right;
  return n;
}

/*************
 * variables *
 *************/
static Var locals;
Var globals;

static Var findvarintable(char* name, Var table) {
  for (Var v = table; v; v = v->next) {
    if (strlen(name) == strlen(v->name) && !strcmp(name, v->name))
      return v;
  }
  return NULL;
}

static Var findvar(char* name) {
  Var v = findvarintable(name, locals);
  return v ? v : findvarintable(name, globals);
}

static Var mkvar(char* name, Type ty) {
  Var v = calloc(1, sizeof(struct var));
  v->name = name;
  v->type = ty;
  return v;
}

static Var mklvar(char* name, Type ty) {
  if (findvarintable(name, locals))
    error("redefine local variable \"%s\"", name);

  Var v = mkvar(name, ty);
  v->is_global = 0;
  v->next = locals;
  locals = v;
  return v;
}

static Var mkgvar(char* name, Type ty) {
  if (findvarintable(name, locals))
    error("redefine global variable \"%s\"", name);

  Var v = mkvar(name, ty);
  v->is_global = 1;
  v->next = globals;
  globals = v;
  return v;
}

/*****************************
 * recursive parse procedure *
 *****************************/
// trans_unit:     { func_def | declaration }*
// declaration:    type_spec identifier;
// type_spec:      'int'
// func_def:       type_spec identifier '(' param_list ')' comp_stat
// param_list:     param_declaration { ',' param_declaration }
// param_declaration: { type_spec }+ identifier
// statement:      expr_stat | print_stat | comp_stat
//                 selection-stat | iteration-stat | jump_stat
// selection-stat: if_stat
// if_stat:        'if' '('  expr_stat ')' statement { 'else' statement }
// iteration-stat: while_stat | dowhile_stat | for_stat
// while_stat:     'while' '(' expr_stat ')' statement
// dowhile_stat:   'do' statement 'while' '(' expression ')';
// for_stat:       'for' '(' {expression}; {expression}; {expression}')'
//                 statement
// jump_stat:      break ';' | continue ';' | 'return' expression? ';'
// comp_stat:      '{' {declaration}* {stat}* '}'
// print_stat:     'print' expr;
// expr_stat:      {expression}? ;
// expression:     assign_expr
// assign_expr:    equality_expr { '=' equality_expr }
// equality_expr:  relational_expr { '==' | '!='  relational_expr }
// relational_expr:  sum_expr { '>' | '<' | '>=' | '<=' sum_expr}
// sum_expr        :mul_expr { '+'|'-' mul_exp }
// mul_exp:        primary { '*'|'/' primary }
// primary:        identifier func_args? | number  | '(' expression ')'
// func_args:      '(' expression { ',' expression } ')'

static Node trans_unit();
static Node declaration();
Type type_spec();
static Node function();
Var param_list();
static Node statement();
static Node comp_stat();
static Node if_stat();
static Node while_stat();
static Node dowhile_stat();
static Node for_stat();
static Node expr_stat();
static Node expression();
static Node assign_expr();
static Node eq_expr();
static Node rel_expr();
static Node sum_expr();
static Node mul_expr();
static Node primary();
static Node func_arg();
static Node variable();

static int is_function;
static int check_next_top_level_item() {
  Token back = t;
  type_spec();
  is_function = consume(TK_IDENT) && consume(TK_OPENING_PARENTHESES);
  t = back;
  return is_function;
}

static Node trans_unit() {
  struct node head;
  Node last = &head;
  last->next = NULL;
  while (t->kind != TK_EOI) {
    check_next_top_level_item();
    if (is_function)  // function
      last = last->next = function();
    else  // global variable
      declaration();
  }
  return head.next;
}

static Node declaration() {
  Type ty = type_spec();
  if (is_function)
    mklvar(expect(TK_IDENT)->name, ty);
  else
    mkgvar(expect(TK_IDENT)->name, ty);
  expect(TK_SIMI);
  return NULL;
}

Type type_spec() {
  if (consume(TK_INT))
    return inttype;
  if (consume(TK_VOID))
    return inttype;

  error("unknown type %s", t->name);
  return NULL;
}

static Node function() {
  Node n = mknode(A_FUNCDEF);
  n->type = type_spec();
  n->funcname = expect(TK_IDENT)->name;

  locals = param_list();
  n->params = locals;
  n->body = comp_stat();
  n->locals = locals;
  return n;
}

Var param_list() {
  Var head = NULL;
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    Type ty = type_spec();
    Var v = mklvar(expect(TK_IDENT)->name, ty);
    v->next = head;
    head = v;
    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  }
  return head;
}

static Node statement() {
  // compound statement
  if (match(TK_OPENING_BRACES)) {
    return comp_stat();
  }

  // print statement
  if (consume(TK_PRINT)) {
    Node n = mknode(A_FUNCCALL);
    n->args = expression();
    n->callee = "print";
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
    n->left = expression();
    expect(TK_SIMI);
    return n;
  }

  // expr statement
  return expr_stat();
}

static Node comp_stat() {
  struct node head;
  Node last = &head;
  last->next = NULL;
  expect(TK_OPENING_BRACES);
  while (t->kind != TK_CLOSING_BRACES) {
    if (match(TK_INT)) {
      last->next = declaration();
    } else {
      last->next = statement();
    }
    while (last->next)
      last = last->next;
  }
  expect(TK_CLOSING_BRACES);
  Node n = mknode(A_BLOCK);
  n->body = head.next;
  return n;
}

static Node if_stat() {
  Node n = mknode(A_IF);
  expect(TK_IF);
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  n->then = statement();
  if (consume(TK_ELSE)) {
    n->els = statement();
  }
  return n;
}

static Node while_stat() {
  Node n = mknode(A_FOR);
  expect(TK_WHILE);
  expect(TK_OPENING_PARENTHESES);
  n->cond = eq_expr();
  expect(TK_CLOSING_PARENTHESES);
  n->body = statement();
  return n;
}

static Node dowhile_stat() {
  Node n = mknode(A_DOWHILE);
  expect(TK_DO);
  n->body = statement();
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
  if (!consume(TK_SIMI))
    n->init = expr_stat();
  if (!consume(TK_SIMI)) {
    n->cond = expression();
    expect(TK_SIMI);
  }
  if (!consume(TK_CLOSING_PARENTHESES)) {
    n->post = mkuniary(A_EXPR_STAT, expression());
    expect(TK_CLOSING_PARENTHESES);
  }
  n->body = statement();
  return n;
}

static Node expr_stat() {
  if (consume(TK_SIMI)) {
    return NULL;
  }

  Node n = expression();
  expect(TK_SIMI);
  return mkuniary(A_EXPR_STAT, n);
}

static Node expression() {
  return assign_expr();
}

static Node assign_expr() {
  Node n = eq_expr();
  if (!match(TK_EQUAL)) {
    return n;
  }

  if (n->kind != A_VAR) {
    error("lvalue expected!");
  }
  n->kind = A_VAR;
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
    if (consume(TK_GREATER)) {
      n = mkbinary(A_GT, n, sum_expr());
    } else if (consume(TK_LESS)) {
      n = mkbinary(A_LT, n, sum_expr());
    } else if (consume(TK_GREATEREQUAL)) {
      n = mkbinary(A_GE, n, sum_expr());
    } else if (consume(TK_LESSEQUAL)) {
      n = mkbinary(A_LE, n, sum_expr());
    } else {
      break;
    }
  }
  return n;
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
  Node n = primary();

  for (;;) {
    if (consume(TK_STAR)) {
      n = mkbinary(A_MUL, n, primary());
    } else if (consume(TK_SLASH)) {
      n = mkbinary(A_DIV, n, primary());
    } else {
      return n;
    }
  }
}

static Node primary() {
  Token tok;

  if ((tok = consume(TK_NUM))) {
    Node n = mknode(A_NUM);
    n->intvalue = tok->value;
    return n;
  }

  if ((tok = consume(TK_IDENT))) {
    if (match(TK_OPENING_PARENTHESES)) {
      Node n = mknode(A_FUNCCALL);
      n->callee = tok->name;
      n->args = func_arg();
      return n;
    }

    Var v = findvar(tok->name);
    if (!v)
      error("undefined variable");
    return variable(v);
  }

  if (consume(TK_OPENING_PARENTHESES)) {
    Node n = eq_expr();
    expect(TK_CLOSING_PARENTHESES);
    return n;
  }

  error("parse: primary get unexpected token");
  return NULL;
}

static Node func_arg() {
  struct node head = {};
  Node cur = &head;

  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    cur->next = expression();
    cur = cur->next;
    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  };
  return head.next;
}

static Node variable(Var v) {
  Node n = mknode(A_VAR);
  n->var = v;
  return n;
}

Node parse(Token root) {
  t = root;
  Node n = trans_unit();
  if (t->kind != TK_EOI) {
    error("parse: unexpected EOI");
  }
  return n;
}