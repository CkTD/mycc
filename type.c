#include "inc.h"

static struct type ty_int = {TY_INT, 8};

Type inttype = &ty_int;

Type ty(Node n) {
  if (n->type)
    return n->type;

  switch (n->kind) {
    case A_NUM:
      n->type = inttype;
      break;
    case A_VAR:
      n->type = n->var->type;
      break;
    case A_ADD:
    case A_SUB:
    case A_MUL:
    case A_DIV:
      n->type = ty(n->left);
      break;
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_GE:
    case A_LE:
      n->type = inttype;
      break;
  }
  return n->type;
}