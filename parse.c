#include "inc.h"
#include "stdio.h"
#include "stdlib.h"

char *ast_str[] = {
    [A_ASSIGN] = "assign",   [A_EQ] = "eq",
    [A_NE] = "noteq",        [A_LT] = "lt",
    [A_GT] = "gt",           [A_LE] = "le",
    [A_GE] = "ge",           [A_ADD] = "add",
    [A_SUB] = "sub",         [A_MUL] = "mul",
    [A_DIV] = "div",         [A_NUM] = "num",
    [A_PRINT] = "print",     [A_IDENT] = "identifier",
    [A_LVIDENT] = "lvalue",  [A_IF] = "if",
    [A_DOWHILE] = "dowhile", [A_FOR] = "for",
    [A_BREAK] = "break",     [A_CONTINUE] = "continue",
    [A_FUNCDEF] = "funcdef",
};

static Token t;  // token to be processed

static int match(int kind) { return t->kind == kind; }
static void expect(int kind) {
  if (!match(kind)) {
    error("parse: token of %s expected but got %s", token_str[kind],
          token_str[t->kind]);
  }
}
static void advance() { t = t->next; }
static void advancet(int kind) {
  expect(kind);
  advance();
}
static int advanceif(int kind) {
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
// expression:     assign_expr { '='  expression }
// assign_expr:    equality_expr { '==' | '!='  equality_expr }
// equality_expr:  relational_expr { '>' | '<' | '>=' | '<=' relational_expr}
// relational_expr:sum_exp { '+'|'-' sum_exp }
// sum_exp:        mul_exp { '*'|'/' mul_exp }
// mul_exp:        identifier | number  | '(' assignexpr ')'
// selection-stat: if_stat
// if_stat:        'if' '('  expr_stat ')' statement { 'else' statement }
// iteration-stat: while_stat | dowhile_stat | for_stat
// while_stat:     'while' '(' expr_stat ')' statement
// dowhile_stat:   'do' statement 'while' '(' expression ')';
// for_stat:       'for' '(' {expression}; {expression}; {expression}')'
//                 statement
// jump_stat:      break ';' | continue ';'

static Node trans_unit();
static Node function();
static Node statement();
static Node comp_stat();
static Node decl();
static Node assign_expr();
static Node expr_stat();
static Node expression();
static Node eq_expr();
static Node rel_expr();
static Node mul_expr();
static Node sum_expr();
static Node identifier();
static Node if_stat();
static Node while_stat();
static Node dowhile_stat();
static Node for_stat();

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

  advancet(TK_VOID);
  expect(TK_IDENT);
  n->funcname = t->name;
  advance(TK_IDENT);
  advancet(TK_OPENING_PARENTHESES);
  n->proto = NULL;
  advancet(TK_CLOSING_PARENTHESES);
  n->body = comp_stat();

  return n;
}

static Node statement() {
  // compound statement
  if (match(TK_OPENING_BRACES)) {
    return comp_stat();
  }

  // print statement
  if (match(TK_PRINT)) {
    advance();
    Node n = mkuniary(A_PRINT, expression());
    advancet(TK_SIMI);
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
    advancet(TK_BREAK);
    advancet(TK_SIMI);
    return mknode(A_BREAK);
  }
  if (match(TK_CONTINUE)) {
    advancet(TK_CONTINUE);
    advancet(TK_SIMI);
    return mknode(A_CONTINUE);
  }

  // expr statement
  return expr_stat();
}

static Node comp_stat() {
  struct node head;
  Node last = &head;
  last->next = NULL;
  advancet(TK_OPENING_BRACES);
  while (t->kind != TK_CLOSING_BRACES) {
    if (match(TK_INT)) {
      last->next = decl();
    } else {
      last->next = statement();
    }
    while (last->next) last = last->next;
  }
  advancet(TK_CLOSING_BRACES);
  Node n = mknode(A_BLOCK);
  n->body = head.next;
  return n;
}

static Node decl() {
  advancet(TK_INT);
  expect(TK_IDENT);
  mksym(t->name);
  advance();
  advance(TK_SIMI);
  return NULL;
}

static Node expr_stat() {
  if (advanceif(TK_SIMI)) {
    return NULL;
  }

  Node n = expression();
  advancet(TK_SIMI);
  return n;
}

static Node expression() {
  Node n = assign_expr();
  if (t->kind != TK_EQUAL) {
    return n;
  }

  if (n->kind != A_IDENT) {
    error("lvalue expected!");
  }
  n->kind = A_LVIDENT;
  advancet(TK_EQUAL);

  return mkbinary(A_ASSIGN, n, expression());
}

static Node assign_expr() {
  Node n = eq_expr();
  for (;;) {
    if (advanceif(TK_EQUALEQUAL)) {
      n = mkbinary(A_EQ, n, eq_expr());
    } else if (advanceif(TK_NOTEQUAL)) {
      n = mkbinary(A_NE, n, eq_expr());
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
  if (match(TK_NUM)) {
    Node n = mknode(A_NUM);
    n->intvalue = t->value;
    advance();
    return n;
  }

  if (match(TK_IDENT)) {
    return identifier();
  }

  if (match(TK_OPENING_PARENTHESES)) {
    advance();
    Node n = assign_expr();
    advancet(TK_CLOSING_PARENTHESES);
    return n;
  }

  error("parse: mulexpr got unexpected token %s", token_str[t->kind]);
  return NULL;
  // return expr();
}

static Node identifier() {
  Node n = mknode(A_IDENT);
  n->sym = t->name;
  advancet(TK_IDENT);
  return n;
}

Node if_stat() {
  Node n = mknode(A_IF);
  advancet(TK_IF);
  advancet(TK_OPENING_PARENTHESES);
  n->cond = expression();
  advancet(TK_CLOSING_PARENTHESES);
  n->then = statement();
  if (advanceif(TK_ELSE)) {
    n->els = statement();
  }

  return n;
}

Node while_stat() {
  Node n = mknode(A_FOR);

  advancet(TK_WHILE);
  advancet(TK_OPENING_PARENTHESES);
  n->cond = assign_expr();
  advancet(TK_CLOSING_PARENTHESES);
  n->body = statement();

  return n;
}

Node dowhile_stat() {
  Node n = mknode(A_DOWHILE);
  advancet(TK_DO);
  n->body = statement();
  advancet(TK_WHILE);
  advancet(TK_OPENING_PARENTHESES);
  n->cond = expression();
  advancet(TK_CLOSING_PARENTHESES);
  advancet(TK_SIMI);
  return n;
}

static Node for_stat() {
  Node n = mknode(A_FOR);
  advancet(TK_FOR);
  advancet(TK_OPENING_PARENTHESES);
  if (advanceif(TK_SIMI)) {
    n->init = NULL;
  } else {
    n->init = expression();
    advancet(TK_SIMI);
  }
  if (advanceif(TK_SIMI)) {
    n->cond = NULL;
  } else {
    n->cond = expression();
    advancet(TK_SIMI);
  }

  if (advanceif(TK_CLOSING_PARENTHESES)) {
    n->post = NULL;
  } else {
    n->post = expression();
    advancet(TK_CLOSING_PARENTHESES);
  }

  n->body = statement();
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

void print_ast(Node n, int indent) {
  for (; n; n = n->next) {
    fprintf(stderr, "%*s [%s] %d, %s\n", indent, "", ast_str[n->kind],
            n->intvalue, n->sym);
    print_ast(n->left, indent + 2);
    print_ast(n->right, indent + 2);
    if (n->next) fprintf(stderr, "%*s----------\n", indent, "");
  }
}