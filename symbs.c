#include "inc.h"

// file scope
// global variables/function/string-literal/tags
// are accumulated to this list during parsing
Node globals;

// nested block scope
typedef struct blockscope* BlockScope;
struct blockscope {
  Node list;
  BlockScope outer;
};
// variables/tags for current block
static BlockScope blockscope;

// current function being parsed/generated
Node current_func;
// local variables for current function
static Node locals;

void enter_func(Node f) {
  if (current_func)
    error("nested function");

  locals = f->locals;
  current_func = f;
}

void exit_func(Node f) {
  if (current_func != f)
    error("not in func");

  f->locals = locals;
  current_func = NULL;
}

void enter_scope(Node f) {
  BlockScope new_scope = calloc(1, sizeof(struct blockscope));
  new_scope->outer = blockscope;
  blockscope = new_scope;
}

void exit_scope(Node f) {
  blockscope = blockscope->outer;
}

// search symbol of 'kind' having 'name' in block scope s
//   if name not found in scope, return NULL
//   if a symbol matching the name but with different kind, error
//   kind = A_ANY(0) means any kind is ok
static Node find_block_scope(const char* name, BlockScope s, int kind) {
  for (Node n = s->list; n; n = n->scope_next) {
    if (name == n->name) {
      if (kind && n->kind != kind)
        error("conflict type for %s", name);
      return n;
    }
  }
  return NULL;
}

static Node find_file_scope(const char* name, int kind) {
  for (Node n = globals; n; n = n->next) {
    if (name == n->name) {
      if (kind && n->kind != kind)
        error("conflict type for %s", name);
      return n;
    }
  }
  return NULL;
}

// for scope=FILE search in file scope
// for scope=INNER search in file scope if not in a function
//                   else search in current block scope
// for scope=ALL search in nested block scope from inner to outer,
//                   then search in file scope
Node find_symbol(const char* name, int kind, int scope) {
  Node n = NULL;

  if (scope == SCOPE_FILE || (scope == SCOPE_INNER && !current_func)) {
    return find_file_scope(name, kind);
  } else if (scope == SCOPE_INNER || (scope == SCOPE_INNER && current_func)) {
    return find_block_scope(name, blockscope, kind);
  } else if (scope == SCOPE_ALL) {
    for (BlockScope s = blockscope; s; s = s->outer) {
      if ((n = find_block_scope(name, s, kind))) {
        return n;
      }
    }
    return find_file_scope(name, kind);
  }

  error("unknwon scope");
  return n;
}

// for scope=FILE install to file scope
// for scope=INNER install to file scope if not in a function
//                   else install to current block scope
void install_symbol(Node n, int scope) {
  if (n->name && find_symbol(n->name, n->kind, scope))
    error("redefine symbol \"%s\"", n->name);

  if (scope == SCOPE_INNER && current_func) {
    if (n->kind != A_TAG) {
      n->next = locals;
      locals = n;
    }
    n->scope_next = blockscope->list;
    blockscope->list = n;
    return;
  }

  if ((scope == SCOPE_FILE) || (scope == SCOPE_INNER && !current_func)) {
    n->next = globals;
    globals = n;
    return;
  }

  error("unknown scope");
}