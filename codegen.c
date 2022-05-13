#include <stdio.h>
#include <stdlib.h>

#include "inc.h"

// X86_64 Registers
//
// 64-bit | 32-bits | 16-bits | 8-bits | Conventional use
// ==============================================================
// rax   | eax      | ax      | al     | Return value, callee-owned
// rdi   | edi      | di      | dil    | 1-6 argument, callee-owned
// rsi   | esi      | si      | sil
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

// register name maps
static const char size_suffix[] = {[1] = 'b', [2] = 'w', [4] = 'l', [8] = 'q'};
static const char* a_regs[] =
    {[1] = "al", [2] = "ax", [4] = "eax", [8] = "rax"};
static const char* arg_regs[][6] = {
    [1] = {"dil", "sil", "dl", "cl", "r8b", "r9b"},
    [2] = {"di", "si", "dx", "cx", "r8w", "r9w"},
    [4] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"},
    [8] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"}};

// unique label
static int label_id = 0;
static const char* new_label() {
  char buf[32];
  sprintf(buf, ".L%d", label_id++);
  return string(buf);
}

static Node current_func;

static void gen_expr(Node n);
static void gen_stat(Node n);

/******************************
 *    generate expressions    *
 ******************************/
// binary expressions
// get operand from: %rax(left) and %rdi(right)
// store result to : %rax
static void add(Node n) {
  if (n->type == inttype || n->type == uinttype)
    fprintf(stdout, "\taddl\t%%edi, %%eax\n");
  else if (n->type == longtype || n->type == ulongtype || is_ptr(n->type))
    fprintf(stdout, "\taddq\t%%rdi, %%rax\n");
  else
    error("unkonwn type");
}

static void sub(Node n) {
  if (n->type == inttype || n->type == uinttype)
    fprintf(stdout, "\tsubl\t%%edi, %%eax\n");
  else if (n->type == longtype || n->type == ulongtype || is_ptr(n->type))
    fprintf(stdout, "\tsubq\t%%rdi, %%rax\n");
  else
    error("unkonwn type");
}

static void mul(Node n) {
  if (n->type == inttype || n->type == uinttype)
    fprintf(stdout, "\timull\t%%edi, %%eax\n");
  else if (n->type == longtype || n->type == ulongtype)
    fprintf(stdout, "\timulq\t%%rdi, %%rax\n");
  else
    error("unkonwn type");
}

static void divide(Node n) {
  if (n->type == inttype) {
    fprintf(stdout, "\tcdq\n");
    fprintf(stdout, "\tidivl\t%%edi\n");
  } else if (n->type == uinttype) {
    fprintf(stdout, "\tcdq\n");
    fprintf(stdout, "\tdivl\t%%edi\n");
  } else if (n->type == longtype) {
    fprintf(stdout, "\tcqo\n");
    fprintf(stdout, "\tidivq\t%%rdi\n");
  } else if (n->type == ulongtype) {
    fprintf(stdout, "\tcqo\n");
    fprintf(stdout, "\tdivq\t%%rdi\n");
  } else
    error("unkonwn type");
}

static void compare(Node n) {
  if (n->type == inttype || n->type == uinttype)
    fprintf(stdout, "\tcmpl\t%%edi, %%eax\n");
  else if (n->type == longtype || n->type == ulongtype)
    fprintf(stdout, "\tcmpq\t%%rdi, %%rax\n");
  char* cd;
  if (is_signed(n->left->type)) {
    if (n->kind == A_EQ)
      cd = "e";
    else if (n->kind == A_NE)
      cd = "ne";
    else if (n->kind == A_GT)
      cd = "g";
    else if (n->kind == A_GE)
      cd = "ge";
    else if (n->kind == A_LT)
      cd = "l";
    else if (n->kind == A_LE)
      cd = "le";
  } else if (is_unsigned(n->left->type) || is_ptr(n->left->type)) {
    if (n->kind == A_EQ)
      cd = "e";
    else if (n->kind == A_NE)
      cd = "ne";
    else if (n->kind == A_GT)
      cd = "a";
    else if (n->kind == A_GE)
      cd = "ae";
    else if (n->kind == A_LT)
      cd = "b";
    else if (n->kind == A_LE)
      cd = "be";
  } else
    error("compare what?");
  fprintf(stdout, "\tset%s\t%%al\n", cd);
  fprintf(stdout, "\tmovzbl\t%%al, %%eax\n");
}

static void gen_iconst(Node n) {
  if (n->type != inttype)
    error("a number constant must have type int");
  fprintf(stdout, "\tmovl\t$%d, %%eax\n", n->intvalue);
  fprintf(stdout, "\tpushq\t%%rax\n");
}

static void gen_load(Type ty) {
  if (is_array(ty))  // for array, the address is it's value
    return;

  fprintf(stdout, "\tpopq\t%%rax\n");
  if (ty == chartype || ty == uchartype)
    fprintf(stdout, "\tmovb\t(%%rax), %%al\n");
  else if (ty == shorttype || ty == ushorttype)
    fprintf(stdout, "\tmovw\t(%%rax), %%ax\n");
  else if (ty == inttype || ty == uinttype)
    fprintf(stdout, "\tmovl\t(%%rax), %%eax\n");
  else if (ty == longtype || ty == ulongtype || is_ptr(ty))
    fprintf(stdout, "\tmovq\t(%%rax), %%rax\n");
  else
    error("load unknown type");

  fprintf(stdout, "\tpushq\t%%rax\n");
}

static void gen_store(Type ty) {
  fprintf(stdout, "\tpopq\t%%rax\n");
  fprintf(stdout, "\tpopq\t%%rdi\n");
  if (ty == chartype || ty == uchartype)
    fprintf(stdout, "\tmovb\t%%al, (%%rdi)\n");
  else if (ty == shorttype || ty == ushorttype)
    fprintf(stdout, "\tmovw\t%%ax, (%%rdi)\n");
  else if (ty == inttype || ty == uinttype)
    fprintf(stdout, "\tmovl\t%%eax, (%%rdi)\n");
  else if (ty == longtype || ty == ulongtype || is_ptr(ty))
    fprintf(stdout, "\tmovq\t%%rax, (%%rdi)\n");
  else if (is_array(ty))
    error("assignment to expression with array type");
  else
    error("store unknown type");

  fprintf(stdout, "\tpushq\t%%rax\n");
}

static void gen_addr(Node n) {
  if (n->kind == A_VAR) {
    if (n->is_global)
      fprintf(stdout, "\tleaq\t%s(%%rip), %%rax\n", n->name);
    else
      fprintf(stdout, "\tleaq\t-%d(%%rbp), %%rax\n", n->offset);
    fprintf(stdout, "\tpushq\t%%rax\n");
    return;
  }

  if (n->kind == A_DEFERENCE) {
    if (!is_ptr(n->left->type))
      error("can only dereference pointer");

    if (n->left->kind == A_VAR) {
      gen_addr(n->left);
      return gen_load(n->left->type);
    }
    return gen_expr(n->left);
  }

  if (n->kind == A_ARRAY_SUBSCRIPTING) {
    if (is_array(n->array->type))
      gen_addr(n->array);
    else  // pointer
      gen_expr(n->array);

    gen_expr(n->index);
    fprintf(stdout, "\tpopq\t%%rax\n");
    fprintf(stdout, "\tpopq\t%%rdi\n");
    int size = n->array->type->base->size;
    if (size <= 8) {
      fprintf(stdout, "\tleaq\t(%%rdi, %%rax, %d), %%rax\n", size);
    } else {
      fprintf(stdout, "\timulq\t$%d, %%rax\n", size);
      fprintf(stdout, "\tleaq\t(%%rdi, %%rax), %%rax\n");
    }
    fprintf(stdout, "\tpushq\t%%rax\n");
    return;
  }

  if (n->kind == A_STRING_LITERAL) {
    fprintf(stdout, "\tleaq\t%s(%%rip), %%rax\n", n->name);
    fprintf(stdout, "\tpushq\t%%rax\n");
    return;
  }

  error("generate address for unknown kind");
}

static void gen_funccall(Node n) {
  fprintf(stdout, "// call function \"%s\"\n", n->callee_name);
  Node node;
  int nargs = 0;
  list_for_each(n->args, node) {
    gen_expr(node->body);
    fprintf(stdout, "\tpopq\t%%%s\n", arg_regs[8][nargs]);
    if (++nargs >= 6 || node->next == n->args)
      break;  // after break, node point to the last register args
  }
  Node last_regarg = node;
  list_for_each_reverse(n->args, node) {
    if (node == last_regarg)
      break;
    gen_expr(node->body);
  }

  int l = label_id++;
  fprintf(stdout, "\tmovq\t%%rsp, %%rax\n");
  fprintf(stdout, "\tandq\t$0xf, %%rax\n");
  fprintf(stdout, "\tcmp\t$0, %%rax\n");
  fprintf(stdout, "\tjnz\t.L.asc.%d\n", l);
  fprintf(stdout, "\tmovq\t$0, %%rax\n");
  fprintf(stdout, "\tcall\t%s\n", n->callee_name);
  fprintf(stdout, "\tjmp\t.L.asc.end.%d\n", l);
  fprintf(stdout, ".L.asc.%d:\n", l);
  fprintf(stdout, "\tsubq\t$8, %%rsp\n");
  fprintf(stdout, "\tmovq\t$0, %%rax\n");
  fprintf(stdout, "\tcall\t%s\n", n->callee_name);
  fprintf(stdout, "\taddq\t$8, %%rsp\n");
  fprintf(stdout, ".L.asc.end.%d:\n", l);
  fprintf(stdout, "\tpushq\t%%rax\n");

  fprintf(stdout, "// ---- call function \"%s\"\n", n->callee_name);
}

static void gen_conversion(Node n) {
  gen_expr(n->body);

  // for now,  we only have integral type
  if (n->body->type->size < n->type->size) {
    fprintf(stdout, "\tpopq\t%%rax\n");
    fprintf(stdout, "\tmov%c%c%c\t%%%s, %%%s\n",
            is_signed(n->body->type) ? 's' : 'z',
            size_suffix[n->body->type->size], size_suffix[n->type->size],
            a_regs[n->body->type->size], a_regs[n->type->size]);
    fprintf(stdout, "\tpushq\t%%rax\n");
  }
}

static void gen_expr(Node n) {
  switch (n->kind) {
    case A_NUM:
      gen_iconst(n);
      return;
    case A_STRING_LITERAL:
      gen_addr(n);
      return;
    case A_VAR:
    case A_ARRAY_SUBSCRIPTING:
      gen_addr(n);
      gen_load(n->type);
      return;
    case A_DEFERENCE:
      gen_expr(n->left);
      gen_load(deref_type(n->left->type));
      return;
    case A_ADDRESS_OF:
      gen_addr(n->left);
      return;
    case A_ASSIGN:
      fprintf(stdout, "\t// assignment\n");
      gen_addr(n->left);
      gen_expr(n->right);
      gen_store(n->left->type);
      return;
    case A_FUNC_CALL:
      gen_funccall(n);
      return;
    case A_CONVERSION:
      gen_conversion(n);
      return;
  }

  // The order of evaluation is unspecified
  // https:// en.cppreference.com/w/cpp/language/eval_order
  gen_expr(n->left);
  gen_expr(n->right);
  fprintf(stdout, "\tpopq\t%%rdi\n");
  fprintf(stdout, "\tpopq\t%%rax\n");
  switch (n->kind) {
    case A_ADD:
      add(n);
      break;
    case A_SUB:
      sub(n);
      break;
    case A_DIV:
      divide(n);
      break;
    case A_MUL:
      mul(n);
      break;
    case A_EQ:
    case A_NE:
    case A_GT:
    case A_LT:
    case A_GE:
    case A_LE:
      compare(n);
      break;
    default:
      error("unknown ast node");
  }
  fprintf(stdout, "\tpushq\t%%rax\n");
}

/******************************
 *    generate statements    *
 ******************************/
// jump locations for iteration statements (for, while, do-while)
typedef struct jumploc* JumpLoc;
struct jumploc {
  const char** lcontinue;
  const char** lbreak;
  JumpLoc next;
};
static JumpLoc iterjumploc;

static void iter_enter(const char** lcontinue, const char** lbreak) {
  JumpLoc j = malloc(sizeof(struct jumploc));
  j->lbreak = lbreak;
  j->lcontinue = lcontinue;
  j->next = iterjumploc;
  iterjumploc = j;
}

static void iter_exit() {
  iterjumploc = iterjumploc->next;
}

static void gen_if(Node n) {
  const char* lend = new_label();
  const char* lfalse = n->els ? new_label() : lend;

  // condition
  gen_expr(n->cond);
  fprintf(stdout, "\tpopq\t%%rax\n");
  fprintf(stdout, "\tcmpl\t$0, %%eax\n");
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

static void gen_dowhile(Node n) {
  const char* lstat = new_label();
  const char* lend = NULL;

  iter_enter(&lstat, &lend);

  // statement
  fprintf(stdout, "%s:\n", lstat);
  gen_stat(n->body);

  // condition
  gen_expr(n->cond);
  fprintf(stdout, "\tpopq\t%%rax\n");
  fprintf(stdout, "\tcmpl\t$0, %%eax\n");
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
  const char* lcond = new_label();
  const char* lend = new_label();
  const char* lcontinue = (n->post) ? NULL : lcond;

  // init
  if (n->init) {
    gen_stat(n->init);
  }

  // condition
  fprintf(stdout, "%s:\n", lcond);
  if (n->cond) {
    gen_expr(n->cond);
    fprintf(stdout, "\tpopq\t%%rax\n");
    fprintf(stdout, "\tcmpl\t$0, %%eax\n");
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
  if (n->body) {
    if (current_func->type == voidtype)
      warn("return with a value, in function returning void");
    gen_expr(n->body);
    fprintf(stdout, "\tpopq\t%%rax\n");
  }
  fprintf(stdout, "\tjmp\t.L.return.%s\n", current_func->name);
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
      gen_expr(n->body);
      fprintf(stdout, "\taddq\t$8, %%rsp\n");
      return;
    default:
      return gen_expr(n);
  }
}

/********************************************************
 *  generate global objs ( function, global variable )  *
 ********************************************************/

static void handle_lvars(Node n) {
  int offset = 0;
  for (Node v = n->locals; v; v = v->next) {
    offset += v->type->size;
    v->offset = offset;
  }
  if (offset % 8) {
    offset += 8 - (offset % 8);
  }
  n->stack_size = offset;
}

static void gen_func(Node globals) {
  for (Node n = globals; n; n = n->next) {
    if (n->kind != A_FUNC_DEF)
      continue;

    current_func = n;
    handle_lvars(n);

    fprintf(stdout, "\t.text\n");
    fprintf(stdout, "\t.global %s\n", n->name);
    fprintf(stdout, "%s:\n", n->name);

    // Prologue
    fprintf(stdout, "\tpushq\t%%rbp\n");
    fprintf(stdout, "\tmovq\t%%rsp, %%rbp\n");
    fprintf(stdout, "\tsubq\t$%d, %%rsp\n", n->stack_size);

    // Load arguments to local variables
    int i = 0;
    Node node;
    list_for_each(n->protos, node) {
      Node v = node->body;
      if (i < 6)
        fprintf(stdout, "\tmov%c\t%%%s, -%d(%%rbp)\n",
                size_suffix[v->type->size], arg_regs[v->type->size][i],
                v->offset);
      else {
        fprintf(stdout, "\tmovq\t%d(%%rbp), %%rax\n", 8 * (i - 6) + 16);
        fprintf(stdout, "\tmov%c\t%%%s, -%d(%%rbp)\n",
                size_suffix[v->type->size], a_regs[v->type->size], v->offset);
      }
      i++;
    }

    // Function body
    gen_stat(n->body);

    // Epilogue
    fprintf(stdout, ".L.return.%s:\n", n->name);
    fprintf(stdout, "\tmovq\t%%rbp, %%rsp\n");
    fprintf(stdout, "\tpopq\t%%rbp\n");
    fprintf(stdout, "\tret\n");
  }
}

// generate the print function: void print(int value);
static void gen_print() {
  fprintf(stdout, "\t.data\n");
  fprintf(stdout, "format: .asciz \"%%d\\n\"\n");
  fprintf(stdout, "\t.text\n");
  fprintf(stdout, "print: \n");
  fprintf(stdout, "\tmovq\t%%rdi, %%rsi\n");
  fprintf(stdout, "\tleaq\tformat(%%rip), %%rdi\n");
  fprintf(stdout, "\tmovq\t$0, %%rax\n");
  fprintf(stdout, "\tpushq\t%%rbx\n");
  fprintf(stdout, "\tcall\tprintf\n");
  fprintf(stdout, "\tpopq\t%%rbx\n");
  fprintf(stdout, "\tret\n");
}

static void gen_data(Node globals) {
  int n_rodata = 0, n_data = 0, n_bss = 0;

  for (Node n = globals; n; n = n->next) {
    if (n->kind == A_STRING_LITERAL) {
      if (n_rodata++ == 0)
        fprintf(stdout, "\t.section .rodata\n");
      n->name = new_label();
      fprintf(stdout, "%s:\n", n->name);
      fprintf(stdout, "\t.string\t\"%s\"\n", n->string_value);
    }
  }

  for (Node n = globals; n; n = n->next) {
    if (n->kind == A_VAR && !n->init_value) {
      if (n_bss++ == 0)
        fprintf(stdout, "\t.bss\n");
      fprintf(stdout, "\t.global %s\n", n->name);
      fprintf(stdout, "%s:\n", n->name);
      fprintf(stdout, "\t.zero %d\n", n->type->size);
    }
  }

  for (Node n = globals; n; n = n->next) {
    if (n->kind == A_VAR && n->init_value) {
      if (n_data++ == 0)
        fprintf(stdout, "\t.data\n");
      fprintf(stdout, "\t.global %s\n", n->name);
      fprintf(stdout, "%s:\n", n->name);
      if (n->init_value->kind == A_NUM) {
        if (n->type->size == 1)
          fprintf(stdout, "\t.byte\t%d\n", n->init_value->intvalue);
        else
          fprintf(stdout, "\t.%dbyte\t%d\n", n->type->size,
                  n->init_value->intvalue);
      } else if (n->init_value->kind == A_STRING_LITERAL) {
        fprintf(stdout, "\t.8byte\t%s\n", n->init_value->name);
      }
    }
  }
}

void codegen(Node globals) {
  gen_data(globals);
  gen_print();
  gen_func(globals);
}