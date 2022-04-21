#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inc.h"

// https://web.stanford.edu/class/archive/cs/cs107/cs107.1222/guide/x86-64.html

// gas manual: https://sourceware.org/binutils/docs-2.38/as.html

// x86_64 instruction reference: https://www.felixcloutier.com/x86/

static char *reg_list[4] = {
    "r8",
    "r9",
    "r10",
    "r11",
};

static char *reg_list_b[4] = {
    "r8b",
    "r9b",
    "r10b",
    "r11b",
};

static int reg_stat[4];

static void free_all_reg() { memset(reg_stat, 0, sizeof(int) * 4); }

static int alloc_reg() {
  for (int i = 0; i < 4; i++) {
    if (!reg_stat[i]) {
      reg_stat[i] = 1;
      return i;
    }
  }
  error("out of registers");
  return 0;
}

static void free_reg(int r) {
  if (r < 0) return;
  if (!reg_stat[r]) {
    error("free an unused register");
  }
  reg_stat[r] = 0;
}

static int label_id = 0;
char *new_label() {
  char buf[32];
  sprintf(buf, "L%d", label_id++);
  return string(buf);
}

static int load(int value) {
  int r = alloc_reg();
  fprintf(stdout, "\tmov\t$%d, %%%s\n", value, reg_list[r]);
  return r;
}

static int loadglobal(char *id) {
  if (!findsym(id)) {
    error("use undefined global variable %s", id);
  }
  int r = alloc_reg();
  fprintf(stdout, "\tmov\t%s(%%rip), %%%s\n", id, reg_list[r]);
  return r;
}

static int storglobal(char *id, int r) {
  if (!findsym(id)) {
    error("use undefined global variable %s", id);
  }
  fprintf(stdout, "\tmov\t%%%s, %s(%%rip)\n", reg_list[r], id);
  return r;
}

// https://stackoverflow.com/questions/38335212/calling-printf-in-x86-64-using-gnu-assembler#answer-38335743
static int print(int r1) {
  fprintf(stdout, "\tmov\t%%%s, %%rdi\n", reg_list[r1]);
  fprintf(stdout, "\tpush\t%%rbx\n");
  fprintf(stdout, "\tcall\tprint\n");
  fprintf(stdout, "\tpop\t%%rbx\n");
  fprintf(stdout, "\tmov\t%%rax, %%%s\n", reg_list[r1]);
  return r1;
}

static int add(int r1, int r2) {
  fprintf(stdout, "\tadd\t%%%s, %%%s\n", reg_list[r1], reg_list[r2]);
  free_reg(r1);
  return r2;
}

static int sub(int r1, int r2) {
  fprintf(stdout, "\tsub\t%%%s, %%%s\n", reg_list[r2], reg_list[r1]);
  free_reg(r2);
  return r1;
}

static int mul(int r1, int r2) {
  fprintf(stdout, "\timul\t%%%s, %%%s\n", reg_list[r1], reg_list[r2]);
  free_reg(r1);
  return r2;
}

static int divide(int r1, int r2) {
  fprintf(stdout, "\tmov\t%%%s, %%rax\n", reg_list[r1]);
  fprintf(stdout, "\tcqo\n");
  fprintf(stdout, "\tdiv\t%%%s\n", reg_list[r2]);
  fprintf(stdout, "\tmov\t%%rax, %%%s\n", reg_list[r1]);
  free_reg(r2);
  return r1;
}

// e, ne, g, ge, l, le
static int compare(int r1, int r2, char *cd) {
  fprintf(stdout, "\tcmp\t%%%s, %%%s\n", reg_list[r2], reg_list[r1]);
  fprintf(stdout, "\tset%s\t%%%s\n", cd, reg_list_b[r1]);
  fprintf(stdout, "\tand\t$255, %%%s\n", reg_list[r1]);
  free_reg(r2);
  return r1;
}

static void globalsym(char *s) { fprintf(stdout, "\t.comm\t%s, 8, 8\n", s); }

void genglobalsym(char *s) { globalsym(s); }

static int gen_expr(Node n, int storreg);
static void gen_stat(Node n);

static void gen_if(Node n) {
  char *lend = new_label();
  char *lfalse = n->els ? new_label() : lend;
  int reg;

  // condition
  reg = gen_expr(n->cond, -1);
  fprintf(stdout, "\tcmp\t$%d, %%%s\n", 0, reg_list[reg]);
  free_reg(reg);
  fprintf(stdout, "\tjz\t%s\n", lfalse);

  // true statement
  gen_stat(n->then);

  // false statement
  if (n->els) {
    fprintf(stdout, "\tjmp\t%s\n", lend);
    fprintf(stdout, "%s:\n", lfalse);
    gen_stat(n->els);
  }

  fprintf(stdout, "%s:\n", lend);
}

typedef struct jumploc *JumpLoc;
struct jumploc {
  char **lcontinue;
  char **lbreak;
  JumpLoc next;
};
JumpLoc iterjumploc;

static void iter_enter(char **lcontinue, char **lbreak) {
  JumpLoc j = malloc(sizeof(struct jumploc));
  j->lbreak = lbreak;
  j->lcontinue = lcontinue;
  j->next = iterjumploc;
  iterjumploc = j;
}

static void iter_exit() { iterjumploc = iterjumploc->next; }

static void gen_dowhile(Node n) {
  int reg;
  char *lstat = new_label();
  char *lend = NULL;

  iter_enter(&lstat, &lend);

  // statement
  fprintf(stdout, "%s:\n", lstat);
  gen_stat(n->body);

  // condition
  reg = gen_expr(n->cond, -1);
  fprintf(stdout, "\tcmp\t$%d, %%%s\n", 0, reg_list[reg]);
  free_reg(reg);
  fprintf(stdout, "\tjnz\t%s\n", lstat);

  iter_exit();
  if (lend) {
    fprintf(stdout, "%s:\n", lend);
  }
}

static void gen_break(Node n) {
  if (!(*iterjumploc->lbreak)) {
    *iterjumploc->lbreak = new_label();
  }
  fprintf(stdout, "\tjmp\t%s\n", *iterjumploc->lbreak);
}

static void gen_continue(Node n) {
  if (!(*iterjumploc->lcontinue)) {
    *iterjumploc->lcontinue = new_label();
  }
  fprintf(stdout, "\tjmp\t%s\n", *iterjumploc->lcontinue);
}

static void gen_for(Node n) {
  int reg;
  char *lcond = new_label();
  char *lend = new_label();
  char *lcontinue = (n->post) ? NULL : lcond;

  // cond_expr
  fprintf(stdout, "%s:\n", lcond);
  if (n->cond) {
    reg = gen_expr(n->cond, -1);
    fprintf(stdout, "\tcmp\t$%d, %%%s\n", 0, reg_list[reg]);
    free_reg(reg);
    fprintf(stdout, "\tje\t%s\n", lend);
  }

  // stat
  iter_enter(&lcontinue, &lend);
  gen_stat(n->body);
  iter_exit();

  // post_expr
  if (n->post) {
    if (lcontinue) {
      fprintf(stdout, "%s:\n", lcontinue);
    }
    free_reg(gen_expr(n->post, -1));
  }
  fprintf(stdout, "\tjmp\t%s\n", lcond);
  fprintf(stdout, "%s:\n", lend);
}

static int gen_expr(Node n, int storreg) {
  int lreg, rreg;
  if (n->left) lreg = gen_expr(n->left, -1);
  if (n->right) rreg = gen_expr(n->right, lreg);

  switch (n->kind) {
    case A_PRINT:
      return print(lreg);
    case A_ADD:
      return add(lreg, rreg);
    case A_SUB:
      return sub(lreg, rreg);
    case A_DIV:
      return divide(lreg, rreg);
    case A_MUL:
      return mul(lreg, rreg);
    case A_NUM:
      return load(n->intvalue);
    case A_IDENT:
      return loadglobal(n->sym);
    case A_LVIDENT:
      return storglobal(n->sym, storreg);
    case A_ASSIGN:
      return rreg;
    case A_EQ:
      return compare(lreg, rreg, "e");
    case A_NOTEQ:
      return compare(lreg, rreg, "ne");
    case A_GT:
      return compare(lreg, rreg, "g");
    case A_LT:
      return compare(lreg, rreg, "l");
    case A_GE:
      return compare(lreg, rreg, "ge");
    case A_LE:
      return compare(lreg, rreg, "le");
    default:
      error("unknown ast type %s", ast_str[n->kind]);
  }
  return -1;
}

static void gen_stat(Node n) {
  switch (n->kind) {
    case A_BLOCK:
      for (Node s = n->body; s; s = s->next) gen_stat(s);
      return;
    case A_IF:
      return gen_if(n);
    case A_FOR:
      return gen_for(n);
    case A_DOWHILE:
      return gen_dowhile(n);
    case A_BREAK:
      return gen_break(n);
    case A_CONTINUE:
      return gen_continue(n);
    default:
      free_reg(gen_expr(n, -1));
  }
}

static void gen_func(Node n) {
  if (n->proto) {
    error("func proto is not void");
  }

  fprintf(stdout, ".text\n");
  fprintf(stdout, ".global %s\n", n->funcname);
  fprintf(stdout, "%s:\n", n->funcname);
  gen_stat(n->body);
  fprintf(stdout, "\tret\n");
}

void codegen(Node n) {
  // print function
  fprintf(stdout, ".data\n");
  fprintf(stdout, "format: .asciz \"%%d\\n\"\n");
  fprintf(stdout, ".text\n");
  fprintf(stdout, "print: \n");
  fprintf(stdout, "\tpush\t%%rbx\n");
  fprintf(stdout, "\tmov\t%%rdi, %%rsi\n");
  fprintf(stdout, "\tlea\tformat(%%rip), %%rdi\n");
  fprintf(stdout, "\txor\t%%rax, %%rax\n");
  fprintf(stdout, "\tcall\tprintf\n");
  fprintf(stdout, "\tpop\t%%rbx\n");
  fprintf(stdout, "\tret\n");

  // translate ast to code
  for (; n; n = n->next) {
    gen_func(n);
  }
}