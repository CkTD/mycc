#include "inc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

static Var locals;

static Var mkvar(char* name, Type ty) {
  Var v = calloc(1, sizeof(struct var));
  v->name = name;
  v->type = ty;
  return v;
}

static Var mklvar(char* name, Type ty) {
  Var v = mkvar(name, ty);
  v->next = locals;
  locals = v;
  return v;
}

static Var findvar(char* name) {
  for (Var v = locals; v; v = v->next) {
    if (strlen(name) == strlen(v->name) && !strcmp(name, v->name))
      return v;
  }
  return NULL;
}

// C operator precedence
// https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence
// C syntax
// https://cs.wmich.edu/~gupta/teaching/cs4850/sumII06/The%20syntax%20of%20C%20in%20Backus-Naur%20form.htm

// trans_unit:     { func_def }
// func_def:       'void' identifier '(' ')' comp_stat
// statement:      expr_stat | print_stat | comp_stat
//                 selection-stat | iteration-stat | jump_stat
// comp_stat:      '{' {decl}* {stat}* '}'
// decl:           'int' identifier;
// print_stat:     'print' expr;
// expr_stat:      {expression}? ;
// expression:     assign_expr
// assign_expr:    equality_expr { '=' equality_expr }
// equality_expr:  relational_expr { '==' | '!='  relational_expr }
// relational_expr:  sum_expr { '>' | '<' | '>=' | '<=' sum_expr}
// sum_expr        :mul_expr { '+'|'-' mul_exp }
// mul_exp:        primary { '*'|'/' primary }
// primary:        identifier | number  | '(' expression ')'
// selection-stat: if_stat
// if_stat:        'if' '('  expr_stat ')' statement { 'else' statement }
// iteration-stat: while_stat | dowhile_stat | for_stat
// while_stat:     'while' '(' expr_stat ')' statement
// dowhile_stat:   'do' statement 'while' '(' expression ')';
// for_stat:       'for' '(' {expression}; {expression}; {expression}')'
//                 statement
// jump_stat:      break ';' | continue ';'

static Token t;  // token to be processed
static int match(int kind) {
  return t->kind == kind;
}

static void expect(int kind) {
  if (!match(kind)) {
    error("parse: token of %s expected but got %s", token_str[kind],
          token_str[t->kind]);
  }
}

static void advance() {
  t = t->next;
}

static void skip(int kind) {
  expect(kind);
  advance();
}

static int consume(int kind) {
  if (match(kind)) {
    advance();
    return 1;
  }
  return 0;
}

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

static Node trans_unit();
static Node function();
static Node statement();
static Node comp_stat();
static Node if_stat();
static Node while_stat();
static Node dowhile_stat();
static Node for_stat();
static Node expr_stat();
static Node decl();
static Node expression();
static Node assign_expr();
static Node eq_expr();
static Node rel_expr();
static Node sum_expr();
static Node mul_expr();
static Node primary();
static Node variable();

static Node trans_unit() {
  struct node head;
  Node last = &head;
  last->next = NULL;
  while (t->kind != TK_EOI) {
    last = last->next = function();
  }
  return head.next;
}

static Node function() {
  Node n = mknode(A_FUNCDEF);
  locals = NULL;
  skip(TK_VOID);
  expect(TK_IDENT);
  n->funcname = t->name;
  skip(TK_IDENT);
  skip(TK_OPENING_PARENTHESES);
  n->proto = NULL;
  skip(TK_CLOSING_PARENTHESES);
  n->body = comp_stat();
  n->locals = locals;
  return n;
}

static Node statement() {
  // compound statement
  if (match(TK_OPENING_BRACES)) {
    return comp_stat();
  }

  // print statement
  if (consume(TK_PRINT)) {
    Node n = mkuniary(A_PRINT, expression());
    skip(TK_SIMI);
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
  if (match(TK_BREAK)) {
    skip(TK_BREAK);
    skip(TK_SIMI);
    return mknode(A_BREAK);
  }
  if (match(TK_CONTINUE)) {
    skip(TK_CONTINUE);
    skip(TK_SIMI);
    return mknode(A_CONTINUE);
  }

  // expr statement
  return expr_stat();
}

static Node comp_stat() {
  struct node head;
  Node last = &head;
  last->next = NULL;
  skip(TK_OPENING_BRACES);
  while (t->kind != TK_CLOSING_BRACES) {
    if (match(TK_INT)) {
      last->next = decl();
    } else {
      last->next = statement();
    }
    while (last->next)
      last = last->next;
  }
  skip(TK_CLOSING_BRACES);
  Node n = mknode(A_BLOCK);
  n->body = head.next;
  return n;
}

static Node if_stat() {
  Node n = mknode(A_IF);
  skip(TK_IF);
  skip(TK_OPENING_PARENTHESES);
  n->cond = expression();
  skip(TK_CLOSING_PARENTHESES);
  n->then = statement();
  if (consume(TK_ELSE)) {
    n->els = statement();
  }
  return n;
}

static Node while_stat() {
  Node n = mknode(A_FOR);
  skip(TK_WHILE);
  skip(TK_OPENING_PARENTHESES);
  n->cond = eq_expr();
  skip(TK_CLOSING_PARENTHESES);
  n->body = statement();
  return n;
}

static Node dowhile_stat() {
  Node n = mknode(A_DOWHILE);
  skip(TK_DO);
  n->body = statement();
  skip(TK_WHILE);
  skip(TK_OPENING_PARENTHESES);
  n->cond = expression();
  skip(TK_CLOSING_PARENTHESES);
  skip(TK_SIMI);
  return n;
}

static Node for_stat() {
  Node n = mknode(A_FOR);
  skip(TK_FOR);
  skip(TK_OPENING_PARENTHESES);
  if (!consume(TK_SIMI))
    n->init = expr_stat();
  if (!consume(TK_SIMI)) {
    n->cond = expression();
    skip(TK_SIMI);
  }
  if (!consume(TK_CLOSING_PARENTHESES)) {
    n->post = mkuniary(A_EXPR_STAT, expression());
    skip(TK_CLOSING_PARENTHESES);
  }
  n->body = statement();
  return n;
}

static Node decl() {
  skip(TK_INT);
  expect(TK_IDENT);
  mklvar(t->name, inttype);
  advance();
  skip(TK_SIMI);
  return NULL;
}

static Node expr_stat() {
  if (consume(TK_SIMI)) {
    return NULL;
  }

  Node n = expression();
  skip(TK_SIMI);
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
  skip(TK_EQUAL);

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
  if (match(TK_NUM)) {
    Node n = mknode(A_NUM);
    n->intvalue = t->value;
    advance();
    return n;
  }

  if (match(TK_IDENT)) {
    Var v = findvar(t->name);
    if (!v)
      error("undefined variable");
    return variable(v);
  }

  if (consume(TK_OPENING_PARENTHESES)) {
    Node n = eq_expr();
    skip(TK_CLOSING_PARENTHESES);
    return n;
  }

  error("parse: primary get unexpected token");
  return NULL;
}

static Node variable(Var v) {
  Node n = mknode(A_VAR);
  n->var = v;
  skip(TK_IDENT);
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