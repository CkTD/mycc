#include "inc.h"
#include "stdio.h"
#include "stdlib.h"

char *ast_str[] = {
    [A_ASSIGN] = "assign",  [A_EQ] = "eq",
    [A_NOTEQ] = "noteq",    [A_LT] = "lt",
    [A_GT] = "gt",          [A_LE] = "le",
    [A_GE] = "ge",          [A_ADD] = "add",
    [A_SUB] = "sub",        [A_MUL] = "mul",
    [A_DIV] = "div",        [A_NUM] = "num",
    [A_PRINT] = "print",    [A_IDENT] = "identifier",
    [A_LVIDENT] = "lvalue",
};

static Token t;  // token to be processed
static void match(int kind) {
  if (t->kind != kind) {
    error("parse: token of %s expected but got %s", token_str[kind],
          token_str[t->kind]);
  }
}
static void advance() { t = t->next; }
static void advancet(int kind) {
  match(kind);
  advance();
}
static int advanceif(int kind) {
  if (t->kind == kind) {
    advance();
    return 1;
  }
  return 0;
}

static Node mknode(int op, Node left, Node right) {
  Node n = malloc(sizeof(struct node));
  n->op = op;
  n->left = left;
  n->right = right;
  n->intvalue = 0;
  n->next = NULL;
  n->sym = NULL;
  return n;
}

static Node mkbinary(int op, Node left, Node right) {
  return mknode(op, left, right);
}

static Node mkuniary(int op, Node left) { return mknode(op, left, NULL); }

static Node mkleaf(int op) { return mknode(op, NULL, NULL); }

// C operator precedence
// https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence
// C syntax
// https://cs.wmich.edu/~gupta/teaching/cs4850/sumII06/The%20syntax%20of%20C%20in%20Backus-Naur%20form.htm

// statement:      expr_stat | print_stat | comp_stat | ;
// comp_stat:      '{' {decl}* {stat}* '}'
// decl:           'int' identifier;
// print_stat:     'print' expr;
// expr_stat:      expression;
// expression:     assign_expr { '='  expression }
// assign_expr:    equality_expr { '==' | '!='  equality_expr }
// equality_expr:  relational_expr { '>' | '<' | '>=' | '<=' relational_expr}
// relational_expr:sum_exp { '+'|'-' sum_exp }
// sum_exp:        mul_exp { '*'|'/' mul_exp }
// mul_exp:        identifier | number

static Node statement();
static Node comp_stat();
static Node decl();
static Node assign_expr();
static Node expression();
static Node eq_expr();
static Node rel_expr();
static Node mul_expr();
static Node sum_expr();

static Node statement() {
  // empyt statement
  if (t->kind == TK_SIMI) {
    advance();
    return NULL;
  }

  // compound statement
  if (t->kind == TK_OPENING_BRACES) {
    return comp_stat();
  }

  // print statement
  if (t->kind == TK_PRINT) {
    advance();
    Node n = mkuniary(A_PRINT, assign_expr());
    advancet(TK_SIMI);
    return n;
  }

  // expr statement
  Node n = expression();
  advancet(TK_SIMI);
  return n;
}

static Node comp_stat() {
  struct node head;
  Node last = &head;
  last->next = NULL;
  advancet(TK_OPENING_BRACES);
  while (t->kind != TK_CLOSING_BRACES) {
    if (t->kind == TK_INT) {
      last->next = decl();
    } else {
      last->next = statement();
    }

    if (last->next) last = last->next;
  }
  advancet(TK_CLOSING_BRACES);
  return head.next;
}

static Node decl() {
  advancet(TK_INT);
  match(TK_IDENT);
  mksym(t->name);
  advance();
  advance(TK_SIMI);
  return NULL;
}

static Node expression() {
  Node n = assign_expr();
  if (t->kind != TK_EQUAL) {
    return n;
  }

  if (n->op != A_IDENT) {
    error("lvalue expected!");
  }
  n->op = A_LVIDENT;
  advancet(TK_EQUAL);

  return mkbinary(A_ASSIGN, expression(), n);
}

static Node assign_expr() {
  Node n = eq_expr();
  for (;;) {
    if (advanceif(TK_EQUALEQUAL)) {
      n = mkbinary(A_EQ, n, eq_expr());
    } else if (advanceif(TK_NOTEQUAL)) {
      n = mkbinary(A_NOTEQ, n, eq_expr());
    } else {
      break;
    }
  }
  return n;
}

static Node eq_expr() {
  Node n = rel_expr();
  for (;;) {
    if (advanceif(TK_GREATER)) {
      n = mkbinary(A_GT, n, rel_expr());
    } else if (advanceif(TK_LESS)) {
      n = mkbinary(A_LT, n, rel_expr());
    } else if (advanceif(TK_GREATEREQUAL)) {
      n = mkbinary(A_GE, n, rel_expr());
    } else if (advanceif(TK_LESSEQUAL)) {
      n = mkbinary(A_LE, n, rel_expr());
    } else {
      break;
    }
  }
  return n;
}

static Node rel_expr() {
  Node n = sum_expr();
  for (;;) {
    if (advanceif(TK_ADD)) {
      n = mkbinary(A_ADD, n, sum_expr());
    } else if (advanceif(TK_SUB)) {
      n = mkbinary(A_SUB, n, sum_expr());
    } else {
      break;
    }
  }
  return n;
}

static Node sum_expr() {
  Node n = mul_expr();

  for (;;) {
    if (advanceif(TK_STAR)) {
      n = mkbinary(A_MUL, n, mul_expr());
    } else if (advanceif(TK_SLASH)) {
      n = mkbinary(A_DIV, n, mul_expr());
    } else {
      break;
    }
  }
  return n;
}

static Node mul_expr() {
  if (t->kind == TK_NUM) {
    Node n = mkleaf(A_NUM);
    n->intvalue = t->value;
    advance();
    return n;
  }

  if (t->kind == TK_IDENT) {
    Node n = mkleaf(A_IDENT);
    n->sym = t->name;
    advance();
    return n;
  }
  error("parse: mulexpr got unexpected token %s", token_str[t->kind]);
  return NULL;
  // return expr();
}

Node parse(Token root) {
  t = root;
  Node n = statement();
  if (t->kind != TK_EOI) {
    error("parse: unexpected EOI");
  }
  return n;
}

void print_ast(Node n, int indent) {
  for (; n; n = n->next) {
    fprintf(stderr, "%*s [%s] %d, %s\n", indent, "", ast_str[n->op],
            n->intvalue, n->sym);
    print_ast(n->left, indent + 2);
    print_ast(n->right, indent + 2);
  }
}