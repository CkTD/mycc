#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inc.h"

// https://web.stanford.edu/class/archive/cs/cs107/cs107.1222/guide/x86-64.html

// gas manual: https://sourceware.org/binutils/docs-2.38/as.html

// x86_64 instruction reference: https://www.felixcloutier.com/x86/

// X86_64 Registers
//
// 64-bit | 32-bits | 16-bits | 8-bits | Conventional use
// ==============================================================
// rax   | eax      | ax      | al     | Return value, callee-owned
// rdi   | edi      | di      | dil    | 1-6 argument, callee-owned
// rsi   | esi      | si      | si
// rdx   | edx      | dx      | dl
// rcx   | ecx      | cx      | cl
// r8    | r8d      | r8w     | r8b
// r9    | r9d      | r9w     | r9b
// r10   | r10d     | r10w    | r10b   | Scratch/temporary, callee-owned
// r11   | r11d     | r11w    | r11b
// rsp   | esp      | sp      | spl    | Stack pointer, caller-owned
// rbx   | ebx      | bx      | bl     | Local Variable, caller-owned
// rbp   | ebp      | bp      | bpl
// r12   | r12d     | r12w    | r12b
// r13   | r13d     | r13w    | r13b
// r14   | r14d     | r14w    | r14b
// r15   | r15d     | r15w    | r15b
//
// rip
// eflags

Node current_func;

const char* arg_resg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static int label_id = 0;
char* new_label() {
  char buf[32];
  sprintf(buf, ".L%d", label_id++);
  return string(buf);
}

// binary expressions
// get operand from: %rax(left) and %rdi(right)
// store result to : %rax
static void add() {
  fprintf(stdout, "\tadd\t%%rdi, %%rax\n");
}
static void sub() {
  fprintf(stdout, "\tsub\t%%rdi, %%rax\n");
}
static void mul() {
  fprintf(stdout, "\timul\t%%rdi, %%rax\n");
}
static void divide() {
  fprintf(stdout, "\tcqo\n");
  fprintf(stdout, "\tdiv\t%%rdi\n");
}
// e, ne, g, ge, l, le
static void compare(char* cd) {
  fprintf(stdout, "\tcmp\t%%rdi, %%rax\n");
  fprintf(stdout, "\tset%s\t%%al\n", cd);
  fprintf(stdout, "\tand\t$255, %%rax\n");
}

static void gen_addr(Node n) {
  if (n->kind == A_VAR) {
    fprintf(stdout, "\tlea\t-%d(%%rbp), %%rax\n", n->var->offset);
    fprintf(stdout, "\tpush\t%%rax\n");
    return;
  }
  error("not a lvalue");
}

static void gen_load(Node n) {
  fprintf(stdout, "\tpop\t%%rax\n");
  if (ty(n) == inttype)
    fprintf(stdout, "\tmov\t(%%rax), %%rax\n");
  else
    error("not a lvalue");

  fprintf(stdout, "\tpush\t%%rax\n");
}

static void gen_store(Node n) {
  fprintf(stdout, "\tpop\t%%rdi\n");
  fprintf(stdout, "\tpop\t%%rax\n");
  if (ty(n) == inttype)
    fprintf(stdout, "\tmov\t%%rdi, (%%rax)\n");
  else
    error("store unknown type");

  fprintf(stdout, "\tpush\t%%rdi\n");
}

// load an int constant
static void gen_iconst(int value) {
  fprintf(stdout, "\tmov\t$%d, %%rax\n", value);
  fprintf(stdout, "\tpush\t%%rax\n");
}

// https://stackoverflow.com/questions/38335212/calling-printf-in-x86-64-using-gnu-assembler#answer-38335743

static void gen_expr(Node n);
static void gen_stat(Node n);

static void gen_if(Node n) {
  char* lend = new_label();
  char* lfalse = n->els ? new_label() : lend;

  // condition
  gen_expr(n->cond);
  fprintf(stdout, "\tpop\t%%rax\n");
  fprintf(stdout, "\tcmp\t$0, %%rax\n");
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

typedef struct jumploc* JumpLoc;
struct jumploc {
  char** lcontinue;
  char** lbreak;
  JumpLoc next;
};
JumpLoc iterjumploc;

static void iter_enter(char** lcontinue, char** lbreak) {
  JumpLoc j = malloc(sizeof(struct jumploc));
  j->lbreak = lbreak;
  j->lcontinue = lcontinue;
  j->next = iterjumploc;
  iterjumploc = j;
}

static void iter_exit() {
  iterjumploc = iterjumploc->next;
}

static void gen_dowhile(Node n) {
  char* lstat = new_label();
  char* lend = NULL;

  iter_enter(&lstat, &lend);

  // statement
  fprintf(stdout, "%s:\n", lstat);
  gen_stat(n->body);

  // condition
  gen_expr(n->cond);
  fprintf(stdout, "\tpop\t%%rax\n");
  fprintf(stdout, "\tcmp\t$0, %%rax\n");
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
  char* lcond = new_label();
  char* lend = new_label();
  char* lcontinue = (n->post) ? NULL : lcond;

  // init
  if (n->init) {
    gen_stat(n->init);
  }

  // condition
  fprintf(stdout, "%s:\n", lcond);
  if (n->cond) {
    gen_expr(n->cond);
    fprintf(stdout, "\tpop\t%%rax\n");
    fprintf(stdout, "\tcmp\t$0, %%rax\n");
    fprintf(stdout, "\tje\t%s\n", lend);
  }

  // stat
  iter_enter(&lcontinue, &lend);
  gen_stat(n->body);
  iter_exit();

  // post_expr
  if (n->post) {
    if (lcontinue)
      fprintf(stdout, "%s:\n", lcontinue);
    gen_stat(n->post);
  }
  fprintf(stdout, "\tjmp\t%s\n", lcond);
  fprintf(stdout, "%s:\n", lend);
}

static void gen_return(Node n) {
  if (n->left) {
    gen_expr(n->left);
    fprintf(stdout, "\tpop\t%%rax\n");
  }
  fprintf(stdout, "\tjmp\t .L.return.%s\n", current_func->funcname);
}

// https://refspecs.linuxbase.org/elf/x86_64-abi-0.21.pdf(section 3.2)
static void gen_funccall(Node n) {
  int nargs = 0;
  for (Node a = n->args; a; a = a->next) {
    gen_expr(a);
    nargs++;
  }

  int regargs = (nargs >= 6) ? 6 : nargs;
  for (int i = 0; i < regargs; i++) {
    fprintf(stdout, "\tpop\t%%%s\n", arg_resg[i]);
  }
  int lid = label_id++;
  fprintf(stdout, "\tmov\t%%rsp, %%rax\n");
  fprintf(stdout, "\tand\t$15, %%rax\n");
  fprintf(stdout, "\tjnz .L.asc.%d\n", lid);
  fprintf(stdout, "\tmov\t$0, %%rax\n");
  fprintf(stdout, "\tcall %s\n", n->callee);
  fprintf(stdout, "\tjmp .L.end.%d\n", lid);
  fprintf(stdout, ".L.asc.%d:\n", lid);
  fprintf(stdout, "\tsub\t$8, %%rsp\n");
  fprintf(stdout, "\tmov\t$0, %%rax\n");
  fprintf(stdout, "\tcall %s\n", n->callee);
  fprintf(stdout, "\tadd\t$8, %%rsp\n");
  fprintf(stdout, ".L.end.%d:\n", lid);
  fprintf(stdout, "\tpush\t%%rax\n");
}

static void gen_expr(Node n) {
  switch (n->kind) {
    case A_NUM:
      gen_iconst(n->intvalue);
      return;
    case A_VAR:
      gen_addr(n);
      gen_load(n);
      return;
    case A_ASSIGN:
      //   if (ty(n->left) != ty(n->right))
      //     error("type mismatch");
      gen_addr(n->left);
      gen_expr(n->right);
      gen_store(n->left);
      return;
    case A_FUNCCALL:
      gen_funccall(n);
      return;
  }

  // The order of evaluation is unspecified
  // https:// en.cppreference.com/w/cpp/language/eval_order
  gen_expr(n->left);
  gen_expr(n->right);
  fprintf(stdout, "\tpop\t%%rdi\n");
  fprintf(stdout, "\tpop\t%%rax\n");
  switch (n->kind) {
    case A_ADD:
      add();
      break;
    case A_SUB:
      sub();
      break;
    case A_DIV:
      divide();
      break;
    case A_MUL:
      mul();
      break;
    case A_EQ:
      compare("e");
      break;
    case A_NE:
      compare("ne");
      break;
    case A_GT:
      compare("g");
      break;
    case A_LT:
      compare("l");
      break;
    case A_GE:
      compare("ge");
      break;
    case A_LE:
      compare("le");
      break;
    default:
      error("unknown ast node");
  }
  fprintf(stdout, "\tpush\t%%rax\n");
}

static void gen_stat(Node n) {
  switch (n->kind) {
    case A_BLOCK:
      for (Node s = n->body; s; s = s->next)
        gen_stat(s);
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
    case A_RETURN:
      return gen_return(n);
    case A_EXPR_STAT:
      gen_expr(n->left);
      fprintf(stdout, "\tadd\t$8, %%rsp\n");
      return;
    default:
      return gen_expr(n);
  }
}

static void handle_lvars(Node n) {
  int offset = 0;
  for (Var v = n->locals; v; v = v->next) {
    offset += v->type->size;
    v->offset = offset;
  }
  n->stacksize = offset;
}

static void gen_func(Node n) {
  current_func = n;
  handle_lvars(n);

  fprintf(stdout, ".text\n");
  fprintf(stdout, ".global %s\n", n->funcname);
  fprintf(stdout, "%s:\n", n->funcname);

  // Prologue
  fprintf(stdout, "\tpush\t%%rbp\n");
  fprintf(stdout, "\tmov\t%%rsp, %%rbp\n");
  fprintf(stdout, "\tsub\t$%d, %%rsp\n", n->stacksize);

  // Load arguments
  int i = 0;
  for (Var a = n->params; a; a = a->next, i++) {
    if (a->type != inttype)
      error("non int arg");

    if (i < 6) {
      fprintf(stdout, "\tmov\t%%%s, -%d(%%rbp)\n", arg_resg[i], a->offset);
    } else {
      fprintf(stdout, "\tmov\t%d(%%rbp), %%rax\n", 8 * (i - 6) + 16);
      fprintf(stdout, "\tmov\t%%rax, -%d(%%rbp)\n", a->offset);
    }
  }

  // Function body
  gen_stat(n->body);

  // Epilogue
  fprintf(stdout, ".L.return.%s:\n", n->funcname);
  fprintf(stdout, "\tmov\t%%rbp, %%rsp\n");
  fprintf(stdout, "\tpop\t%%rbp\n");
  fprintf(stdout, "\tret\n");
}

void codegen(Node n) {
  // print function
  int lid = label_id++;
  fprintf(stdout, ".data\n");
  fprintf(stdout, "format: .asciz \"%%d\\n\"\n");
  fprintf(stdout, ".text\n");
  fprintf(stdout, "print: \n");
  fprintf(stdout, "\tmov\t%%rdi, %%rsi\n");
  fprintf(stdout, "\tlea\tformat(%%rip), %%rdi\n");
  // call printf
  fprintf(stdout, "\tmov\t%%rsp, %%rax\n");
  fprintf(stdout, "\tand\t$15, %%rax\n");
  fprintf(stdout, "\tjnz .L.asc.%d\n", lid);
  fprintf(stdout, "\tmov\t$0, %%rax\n");
  fprintf(stdout, "\tcall printf\n");
  fprintf(stdout, "\tjmp .L.end.%d\n", lid);
  fprintf(stdout, ".L.asc.%d:\n", lid);
  fprintf(stdout, "\tsub\t$8, %%rsp\n");
  fprintf(stdout, "\tmov\t$0, %%rax\n");
  fprintf(stdout, "\tcall printf\n");
  fprintf(stdout, "\tadd\t$8, %%rsp\n");
  fprintf(stdout, ".L.end.%d:\n", lid);
  fprintf(stdout, "\tret\n");

  // translate ast to code
  for (; n; n = n->next) {
    gen_func(n);
  }
}