#include <stdio.h>
#include <stdlib.h>

#include "inc.h"

// register name maps
enum {
  RDI,  // 1-6 argument, callee-owned
  RSI,
  RDX,
  RCX,
  R8,
  R9,
  RAX,  // Return value, callee-owned
  R10,  // Scratch/temporary, callee-owned
  R11,
  RSP,  // Stack pointer, caller-owned
  RBX,  // Local Variable, caller-owned
  RBP,
  R12,
  R13,
  R14,
  R15,
};

static const char* regs(int size, int name) {
  static const char* regs[][16] = {
      [1] = {"dil", "sil", "dl", "cl", "r8b", "r9b", "al", "r10b", "r11b",
             "spl", "bl", "bpl", "r12b", "r13b", "r14b", "r15b"},
      [2] = {"di", "si", "dx", "cx", "r8w", "r9w", "ax", "r10w", "r11w", "sp",
             "bx", "bp", "r12w", "r13w", "r14w", "r15w"},
      [4] = {"edi", "esi", "edx", "ecx", "r8d", "r9d", "eax", "r10d", "r11d",
             "esp", "ebx", "ebp", "r12d", "r13d", "r14d", "r15d"},
      [8] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9", "rax", "r10", "r11", "rsp",
             "rbx", "rbp", "r12", "r13", "r14", "r15"}};

  assert(name >= RDI && name <= R15);
  assert(size == 1 || size == 2 || size == 4 || size == 8);
  return regs[size][name];
}

static char size_suffix(int size) {
  switch (size) {
    case 1:
      return 'b';
    case 2:
      return 'w';
    case 4:
      return 'l';
    case 8:
      return 'q';
    case 16:
      return 'o';
  }
  assert(0);
}

// unique label
static int label_id = 0;
static const char* new_label() {
  char buf[32];
  sprintf(buf, ".L%d", label_id++);
  return string(buf);
}

static void gen_expr(Node n);
static void gen_stat(Node n);

/******************************
 *    generate expressions    *
 ******************************/

static void gen_iconst(Node n) {
  // TODO: to hold a unsigned int , maybe use long type for n->intvalue?
  if (n->type != inttype && n->type != uinttype)
    error("a number constant must have type int");
  output("\tmovl\t$%d, %%eax\n", n->intvalue);
  output("\tpushq\t%%rax\n");
}

static void gen_load(Type ty) {
  ty = unqual(ty);

  if (is_array(ty))  // for array, the address is it's value
    return;
  if (is_struct_or_union(ty))  // for struct, keep the address
    return;

  output("\tpopq\t%%rax\n");
  if (is_scalar(ty))
    output("\tmov%c\t(%%rax), %%%s\n", size_suffix(ty->size),
           regs(ty->size, RAX));
  else
    error("load unknown type");

  output("\tpushq\t%%rax\n");
}

static void gen_store(Type ty) {
  ty = unqual(ty);

  output("\tpopq\t%%rax\n");
  output("\tpopq\t%%rdi\n");
  if (is_scalar(ty))
    output("\tmov%c\t%%%s, (%%rdi)\n", size_suffix(ty->size),
           regs(ty->size, RAX));
  else if (is_array(ty))
    error("assignment to expression with array type");
  else if (is_struct_or_union(ty)) {
    int offset = 0;
    for (int s = 8; s; s >>= 1)
      for (; ty->size - offset >= s; offset += s) {
        output("\tmov%c\t%d(%%rax), %%%s\n", size_suffix(s), offset,
               regs(s, RSI));
        output("\tmov%c\t%%%s, %d(%%rdi)\n", size_suffix(s), regs(s, RSI),
               offset);
      }
  } else
    error("store unknown type");

  output("\tpushq\t%%rax\n");
}

static void gen_addr(Node n) {
  if (n->kind == A_VAR) {
    if (n->is_global)
      output("\tleaq\t%s(%%rip), %%rax\n", n->name);
    else
      output("\tleaq\t-%d(%%rbp), %%rax\n", n->offset);
    output("\tpushq\t%%rax\n");
    return;
  }

  if (n->kind == A_DEFERENCE) {
    if (!is_ptr(n->left->type))
      error("can only dereference pointer");

    if (n->left->kind == A_VAR) {
      gen_addr(n->left);
      gen_load(n->left->type);
      return;
    }
    gen_expr(n->left);
    return;
  }

  if (n->kind == A_ARRAY_SUBSCRIPTING) {
    if (is_array(n->array->type))
      gen_addr(n->array);
    else  // pointer
      gen_expr(n->array);

    gen_expr(n->index);
    output("\tpopq\t%%rax\n");
    output("\tpopq\t%%rdi\n");
    int size = n->array->type->base->size;
    if (size <= 8) {
      output("\tleaq\t(%%rdi, %%rax, %d), %%rax\n", size);
    } else {
      output("\timulq\t$%d, %%rax\n", size);
      output("\tleaq\t(%%rdi, %%rax), %%rax\n");
    }
    output("\tpushq\t%%rax\n");
    return;
  }

  if (n->kind == A_MEMBER_SELECTION) {
    gen_addr(n->structure);
    output("\tpopq\t%%rax\n");
    output("\taddq\t$%d, %%rax\n", n->member->offset);
    output("\tpushq\t%%rax\n");
    return;
  }

  if (n->kind == A_STRING_LITERAL) {
    output("\tleaq\t%s(%%rip), %%rax\n", n->name);
    output("\tpushq\t%%rax\n");
    return;
  }

  assert(0);  // generate address for unknown kind
}

static void gen_funccall(Node n) {
  output("// call function \"%s\"\n", n->name);
  Node node;

  int nargs = list_length(n->args);
  int nregargs = nargs > 6 ? 6 : nargs;
  int nmemargs = nargs - nregargs;

  if (nmemargs % 2)
    output("\tsubq\t$8, %%rsp\n");

  list_for_each_reverse(n->args, node) gen_expr(node->body);

  for (int i = 0; i < nregargs; i++) {
    output("\tpopq\t%%%s\n", regs(8, i));
  }

  output("\tcall\t%s\n", n->name);
  if (nmemargs % 2)
    output("\taddq\t$8, %%rsp\n");
  output("\tpushq\t%%rax\n");
  output("// ---- call function \"%s\"\n", n->name);
}

static void gen_conversion(Node n) {
  gen_expr(n->body);

  int src_size = is_array(n->body->type) ? 8 : unqual(n->body->type)->size;
  int dst_size = unqual(n->type)->size;
  if (src_size < dst_size) {
    output("\tpopq\t%%rax\n");
    output("\tmov%c%c%c\t%%%s, %%%s\n", is_signed(n->body->type) ? 's' : 'z',
           size_suffix(src_size), size_suffix(dst_size), regs(src_size, RAX),
           regs(dst_size, RAX));
    output("\tpushq\t%%rax\n");
  }
}

static void gen_ternary(Node n) {
  const char* rlabel = new_label();
  const char* elabel = new_label();
  gen_expr(n->cond);
  output("\tpopq\t%%rax\n");
  output("\tcmp%c\t$0, %%%s\n", size_suffix(n->cond->type->size),
         regs(n->cond->type->size, RAX));
  output("\tje\t%s\n", rlabel);
  gen_expr(n->left);
  output("\tjmp\t%s\n", elabel);
  output("%s:\n", rlabel);
  gen_expr(n->right);
  output("%s:\n", elabel);
}

static void gen_unary_arithmetic(Node n) {
  gen_expr(n->left);
  output("\tpopq\t%%rax\n");
  if (n->kind == A_B_NOT) {
    output("\tnot%c\t%%%s\n", size_suffix(n->type->size),
           regs(n->type->size, RAX));
  } else if (n->kind == A_MINUS) {
    output("\tneg%c\t%%%s\n", size_suffix(n->type->size),
           regs(n->type->size, RAX));
  } else if (n->kind == A_PLUS) {
  } else if (n->kind == A_L_NOT) {
    const char* olabel = new_label();
    const char* elabel = new_label();
    output("\tcmp%c\t$0, %%%s\n", size_suffix(n->left->type->size),
           regs(n->left->type->size, RAX));
    output("\tje\t%s\n", olabel);
    output("\tmovl\t$0, %%eax\n");
    output("\tjmp\t%s\n", elabel);
    output("%s:\n", olabel);
    output("\tmovl\t$1, %%eax\n");
    output("%s:\n", elabel);
  }

  output("\tpushq\t%%rax\n");
}

static void gen_postfix_incdec(Node n) {
  gen_addr(n->left);
  gen_load(n->type);

  gen_addr(n->left);
  output("\tpopq\t%%rax\n");
  if (is_arithmetic(n->type))
    output("\t%s%c\t(%%rax)\n", n->kind == A_POSTFIX_INC ? "inc" : "dec",
           size_suffix(n->type->size));
  else  // pointer
    output("\t%sq\t$%d, (%%rax)\n", n->kind == A_POSTFIX_INC ? "add" : "sub",
           n->type->base->size);
}

// binary expressions
// get operand from: %rax(left) and %rdi(right)
// store result to : %rax
static void gen_ementary_arithmetic(Node n) {
  const char* inst;
  if (n->kind == A_ADD)
    inst = "add";
  else if (n->kind == A_SUB)
    inst = "sub";
  else if (n->kind == A_MUL)
    inst = "imul";
  else {
    char from = size_suffix(n->type->size), to = size_suffix(n->type->size * 2);
    output("\tc%c%c\n", from == 'l' ? 'd' : from, to == 'l' ? 'd' : to);
    output("\t%sdiv%c\t%%%s\n", is_signed(n->type) ? "i" : "",
           size_suffix(n->type->size), regs(n->type->size, RDI));
    if (n->kind == A_MOD)
      output("\tmov%c\t%%%s, %%%s\n", size_suffix(n->type->size),
             regs(n->type->size, RDX), regs(n->type->size, RAX));
    return;
  }

  output("\t%s%c\t%%%s, %%%s\n", inst, size_suffix(n->type->size),
         regs(n->type->size, RDI), regs(n->type->size, RAX));
}

static void gen_compare(Node n) {
  if (n->type == inttype || n->type == uinttype)
    output("\tcmpl\t%%edi, %%eax\n");
  else if (n->type == longtype || n->type == ulongtype)
    output("\tcmpq\t%%rdi, %%rax\n");
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
    assert(0);  // compare what

  output("\tset%s\t%%al\n", cd);
  output("\tmovzbl\t%%al, %%eax\n");
}

static void gen_logical_and(Node n) {
  const char* flabel = new_label();
  const char* elabel = new_label();
  output("\tcmp%c\t$0, %%%s\n", size_suffix(n->left->type->size),
         regs(n->left->type->size, RAX));
  output("\tje\t%s\n", flabel);
  output("\tcmp%c\t$0, %%%s\n", size_suffix(n->right->type->size),
         regs(n->right->type->size, RDI));
  output("\tje\t%s\n", flabel);
  output("\tmovl\t$1, %%eax\n");
  output("\tjmp\t%s\n", elabel);
  output("%s:\n", flabel);
  output("\tmovl\t$0, %%eax\n");
  output("%s:\n", elabel);
}

static void gen_logical_or(Node n) {
  const char* tlabel = new_label();
  const char* elabel = new_label();
  output("\tcmp%c\t$0, %%%s\n", size_suffix(n->left->type->size),
         regs(n->left->type->size, RAX));
  output("\tjne\t%s\n", tlabel);
  output("\tcmp%c\t$0, %%%s\n", size_suffix(n->right->type->size),
         regs(n->right->type->size, RDI));
  output("\tjne\t%s\n", tlabel);
  output("\tmovl\t$0, %%eax\n");
  output("\tjmp\t%s\n", elabel);
  output("%s:\n", tlabel);
  output("\tmovl\t$1, %%eax\n");
  output("%s:\n", elabel);
}

static void gen_bitwise(Node n) {
  const char* inst;
  if (n->kind == A_B_AND)
    inst = "and";
  else if (n->kind == A_B_INCLUSIVEOR)
    inst = "or";
  else if (n->kind == A_B_EXCLUSIVEOR)
    inst = "xor";
  else
    assert(0);  // what bitwise operator;

  output("\t%s%c\t%%%s, %%%s\n", inst, size_suffix(n->type->size),
         regs(n->type->size, RDI), regs(n->type->size, RAX));
}

static void gen_shift(Node n) {
  const char* inst;
  if (n->kind == A_LEFT_SHIFT)
    inst = is_signed(n->left->type) ? "sal" : "shl";
  else
    inst = is_signed(n->left->type) ? "sar" : "shr";

  output("\tmovq\t%%rdi, %%rcx\n");
  output("\t%s%c\t%%cl, %%%s\n", inst, size_suffix(n->left->type->size),
         regs(n->left->type->size, RAX));
}

static void gen_expr(Node n) {
  switch (n->kind) {
    case A_NOOP:
      return;
    case A_NUM:
    case A_ENUM_CONST:
      gen_iconst(n);
      return;
    case A_STRING_LITERAL:
      gen_addr(n);
      return;
    case A_VAR:
    case A_ARRAY_SUBSCRIPTING:
    case A_MEMBER_SELECTION:
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
      output("\t// assignment\n");
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
    case A_TERNARY:
      gen_ternary(n);
      return;
    case A_COMMA:
      gen_expr(n->left);
      output("\tpopq\t%%rax\n");
      gen_expr(n->right);
      return;
    case A_MINUS:
    case A_PLUS:
    case A_L_NOT:
    case A_B_NOT:
      gen_unary_arithmetic(n);
      return;
    case A_POSTFIX_INC:
    case A_POSTFIX_DEC:
      gen_postfix_incdec(n);
      return;
  }

  // binary expr
  // The order of evaluation is unspecified
  // https://en.cppreference.com/w/cpp/language/eval_order
  gen_expr(n->left);
  gen_expr(n->right);
  output("\tpopq\t%%rdi\n");
  output("\tpopq\t%%rax\n");
  switch (n->kind) {
    case A_ADD:
    case A_SUB:
    case A_DIV:
    case A_MUL:
    case A_MOD:
      gen_ementary_arithmetic(n);
      break;
    case A_EQ:
    case A_NE:
    case A_GT:
    case A_LT:
    case A_GE:
    case A_LE:
      gen_compare(n);
      break;
    case A_L_AND:
      gen_logical_and(n);
      break;
    case A_L_OR:
      gen_logical_or(n);
      break;
    case A_B_AND:
    case A_B_EXCLUSIVEOR:
    case A_B_INCLUSIVEOR:
      gen_bitwise(n);
      break;
    case A_LEFT_SHIFT:
    case A_RIGHT_SHIFT:
      gen_shift(n);
      break;
    default:
      assert(0);  // unknown ast node
  }
  output("\tpushq\t%%rax\n");
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
  output("\tpopq\t%%rax\n");
  output("\tcmpl\t$0, %%eax\n");
  output("\tjz\t%s\n", lfalse);

  // true statement
  gen_stat(n->then);

  // false statement
  if (n->els) {
    output("\tjmp\t%s\n", lend);
    output("%s:\n", lfalse);
    gen_stat(n->els);
  }

  output("%s:\n", lend);
}

static void gen_dowhile(Node n) {
  const char* lstat = new_label();
  const char* lend = NULL;

  iter_enter(&lstat, &lend);

  // statement
  output("%s:\n", lstat);
  gen_stat(n->body);

  // condition
  gen_expr(n->cond);
  output("\tpopq\t%%rax\n");
  output("\tcmpl\t$0, %%eax\n");
  output("\tjnz\t%s\n", lstat);

  iter_exit();
  if (lend) {
    output("%s:\n", lend);
  }
}

static void gen_break(Node n) {
  if (!(*iterjumploc->lbreak)) {
    *iterjumploc->lbreak = new_label();
  }
  output("\tjmp\t%s\n", *iterjumploc->lbreak);
}

static void gen_continue(Node n) {
  if (!(*iterjumploc->lcontinue)) {
    *iterjumploc->lcontinue = new_label();
  }
  output("\tjmp\t%s\n", *iterjumploc->lcontinue);
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
  output("%s:\n", lcond);
  if (n->cond) {
    gen_expr(n->cond);
    output("\tpopq\t%%rax\n");
    output("\tcmpl\t$0, %%eax\n");
    output("\tje\t%s\n", lend);
  }

  // stat
  iter_enter(&lcontinue, &lend);
  gen_stat(n->body);
  iter_exit();

  // post_expr
  if (n->post) {
    if (lcontinue)
      output("%s:\n", lcontinue);
    gen_stat(n->post);
  }
  output("\tjmp\t%s\n", lcond);
  output("%s:\n", lend);
}

static void gen_return(Node n) {
  if (n->body) {
    if (current_func->type == voidtype)
      warn("return with a value, in function returning void");
    gen_expr(n->body);
    output("\tpopq\t%%rax\n");
  }
  output("\tjmp\t.L.return.%s\n", current_func->name);
}

static void gen_stat(Node n) {
  switch (n->kind) {
    case A_BLOCK:
      for (Node s = n->body; s; s = s->next)
        gen_stat(s);
      return;
    case A_IF:
      gen_if(n);
      return;
    case A_FOR:
      gen_for(n);
      return;
    case A_DOWHILE:
      gen_dowhile(n);
      return;
    case A_BREAK:
      gen_break(n);
      return;
    case A_CONTINUE:
      gen_continue(n);
      return;
    case A_RETURN:
      gen_return(n);
      return;
    case A_EXPR_STAT:
      gen_expr(n->body);
      output("\taddq\t$8, %%rsp\n");
      return;
    default:
      gen_expr(n);
      return;
  }
}

/********************************
 *  generate data and function  *
 ********************************/

static void handle_lvars(Node n) {
  int offset = 0;
  for (Node v = n->locals; v; v = v->next) {
    if (v->kind == A_VAR) {
      offset += unqual(v->type)->size;
      v->offset = offset;
    }
  }
  if (offset % 8) {
    offset += 8 - (offset % 8);
  }
  n->stack_size = (offset + 15) & -16;
}

static void gen_func() {
  for (Node n = globals; n; n = n->next) {
    if (n->kind != A_FUNCTION || !n->body)
      continue;

    enter_func(n);
    handle_lvars(n);

    output("\t.text\n");
    output("\t.global %s\n", n->name);
    output("%s:\n", n->name);

    // Prologue
    output("\tpushq\t%%rbp\n");
    output("\tmovq\t%%rsp, %%rbp\n");
    output("\tsubq\t$%d, %%rsp\n", n->stack_size);

    // Load arguments to local variables
    int i = 0;
    Node node;
    list_for_each(n->params, node) {
      Node v = node->body;
      if (i < 6)
        output("\tmov%c\t%%%s, -%d(%%rbp)\n",
               size_suffix(unqual(v->type)->size),
               regs(unqual(v->type)->size, i), v->offset);
      else {
        output("\tmovq\t%d(%%rbp), %%rax\n", 8 * (i - 6) + 16);
        output("\tmov%c\t%%%s, -%d(%%rbp)\n",
               size_suffix(unqual(v->type)->size),
               regs(unqual(v->type)->size, RAX), v->offset);
      }
      i++;
    }

    // Function body
    gen_stat(n->body);

    // Epilogue
    output(".L.return.%s:\n", n->name);
    output("\tmovq\t%%rbp, %%rsp\n");
    output("\tpopq\t%%rbp\n");
    output("\tret\n");

    exit_func(n);
  }
}

static void gen_data() {
  int n_rodata = 0, n_data = 0, n_bss = 0;

  for (Node n = globals; n; n = n->next) {
    if (n->kind == A_STRING_LITERAL) {
      if (n_rodata++ == 0)
        output("\t.section .rodata\n");
      n->name = new_label();
      output("%s:\n", n->name);
      output("\t.string\t\"%s\"\n", escape(n->string_value));
    }
  }

  for (Node n = globals; n; n = n->next) {
    if (n->kind == A_VAR && !n->init_value) {
      if (n_bss++ == 0)
        output("\t.bss\n");
      output("\t.global %s\n", n->name);
      output("%s:\n", n->name);
      output("\t.zero %d\n", n->type->size);
    }
  }

  for (Node n = globals; n; n = n->next) {
    if (n->kind == A_VAR && n->init_value) {
      if (n_data++ == 0)
        output("\t.data\n");
      output("\t.global %s\n", n->name);
      output("%s:\n", n->name);
      if (n->init_value->kind == A_NUM) {
        if (n->type->size == 1)
          output("\t.byte\t%d\n", n->init_value->intvalue);
        else
          output("\t.%dbyte\t%d\n", n->type->size, n->init_value->intvalue);
      } else if (n->init_value->kind == A_STRING_LITERAL) {
        output("\t.8byte\t%s\n", n->init_value->name);
      }
    }
  }
}

void codegen() {
  gen_data();
  gen_func();
}