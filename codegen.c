#include <stdio.h>
#include <string.h>

#include "inc.h"

// https://web.stanford.edu/class/archive/cs/cs107/cs107.1222/guide/x86-64.html

// https://sourceware.org/binutils/docs-2.38/as.html
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

static int astgen(Node n, int storreg) {
  int lreg, rreg, reg = -1;

  for (; n; n = n->next) {
    reg = lreg = rreg = -1;
    if (n->left) lreg = astgen(n->left, -1);
    if (n->right) rreg = astgen(n->right, lreg);

    switch (n->op) {
      case A_PRINT:
        reg = print(lreg);
        break;
      case A_ADD:
        reg = add(lreg, rreg);
        break;
      case A_SUB:
        reg = sub(lreg, rreg);
        break;
      case A_DIV:
        reg = divide(lreg, rreg);
        break;
      case A_MUL:
        reg = mul(lreg, rreg);
        break;
      case A_NUM:
        reg = load(n->intvalue);
        break;
      case A_IDENT:
        reg = loadglobal(n->sym);
        break;
      case A_LVIDENT:
        reg = storglobal(n->sym, storreg);
        break;
      case A_ASSIGN:
        reg = rreg;
        break;
      case A_EQ:
        reg = compare(lreg, rreg, "e");
        break;
      case A_NOTEQ:
        reg = compare(lreg, rreg, "ne");
        break;
      case A_GT:
        reg = compare(lreg, rreg, "g");
        break;
      case A_LT:
        reg = compare(lreg, rreg, "l");
        break;
      case A_GE:
        reg = compare(lreg, rreg, "ge");
        break;
      case A_LE:
        reg = compare(lreg, rreg, "le");
        break;
      default:
        error("unknown ast type %s", ast_str[n->op]);
    }
    if (n->next) free_reg(reg);
  }
  return reg;
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

  // main
  fprintf(stdout, ".text\n");
  fprintf(stdout, ".globl main\n");
  fprintf(stdout, "main:\n");
  astgen(n, 0);
  fprintf(stdout, "\tmov\t$0, %%rax\n");
  fprintf(stdout, "\tret\n");
}