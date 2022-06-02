#include "inc.h"

/*******************
 * create AST Node *
 *******************/

// http://port70.net/~nsz/c/c99/n1256.html#6.3.2.1p1
static int is_lvalue(Node node) {
  switch (node->kind) {
    case A_VAR:
    case A_DEFERENCE:
    case A_ARRAY_SUBSCRIPTING:
      return 1;
    case A_MEMBER_SELECTION:
      return is_lvalue(node->structure);
    default:
      return 0;
  }
}

static int is_modifiable_lvalue(Node node, int ignore_qual) {
  Type t = ignore_qual ? unqual(node->type) : node->type;
  return is_lvalue(node) && !is_const(t) && !is_array(t) &&
         !is_struct_with_const_member(t);
}

static Node mknode(int kind, Token token) {
  Node n = calloc(1, sizeof(struct node));
  n->kind = kind;
  n->token = token;
  return n;
}

static Node mkicons(int intvalue) {
  Node n = mknode(A_NUM, NULL);
  n->type = inttype;
  n->intvalue = intvalue;
  return n;
}

static Node mkiconsstr(Token token) {
  Node n = mknode(A_NUM, token);
  char* suffix;
  n->intvalue = strtoull(token->name, &suffix, 0);
  if (errno == ERANGE)
    errorat(token, "integer constant to large");

  int is_oct_or_hex = (*token->name == '0') &&
                      ((token->name[1] >= '0' && token->name[1] <= '7') ||
                       (token->name[1] == 'x' || token->name[1] == 'X'));

  int is_none = strlen(suffix) == 0;
  int is_u = !strcmp(suffix, "u") || !strcmp(suffix, "U");
  int is_l = !strcmp(suffix, "l") || !strcmp(suffix, "L");
  is_l |= !strcmp(suffix, "ll") || !strcmp(suffix, "LL");
  int is_ul = !strcmp(suffix, "ul") || !strcmp(suffix, "UL") ||
              !strcmp(suffix, "Ul") || !strcmp(suffix, "uL") ||
              !strcmp(suffix, "lu") || !strcmp(suffix, "LU") ||
              !strcmp(suffix, "Lu") || !strcmp(suffix, "lU");
  is_ul |= !strcmp(suffix, "ull") || !strcmp(suffix, "ULL") ||
           !strcmp(suffix, "Ull") || !strcmp(suffix, "uLL") ||
           !strcmp(suffix, "llu") || !strcmp(suffix, "LLU") ||
           !strcmp(suffix, "LLu") || !strcmp(suffix, "llU");

  if (is_none) {
    if (n->intvalue <= INT_MAX) {
      n->type = inttype;
      return n;
    }
    if (is_oct_or_hex && n->intvalue <= UINT_MAX) {
      n->type = uinttype;
      return n;
    }
    if (n->intvalue <= LONG_MAX) {
      n->type = longtype;
      return n;
    }
    if (is_oct_or_hex && n->intvalue <= ULONG_MAX) {
      n->type = ulongtype;
      return n;
    }
    errorat(token, "integer constant too large");
  } else if (is_ul) {
    if (n->intvalue <= ULONG_MAX) {
      n->type = ulongtype;
      return n;
    }
    assert(0);
  } else if (is_u) {
    if (n->intvalue <= UINT_MAX) {
      n->type = uinttype;
      return n;
    }
    if (n->intvalue <= ULONG_MAX) {
      n->type = ulongtype;
      return n;
    }
  } else if (is_l) {
    if (n->intvalue <= LONG_MAX) {
      n->type = longtype;
      return n;
    }
    if (is_oct_or_hex && n->intvalue <= ULONG_MAX) {
      n->type = ulongtype;
      return n;
    }
    errorat(token, "integer constant too large");
  }
  errorat(token, "invalid integer suffix %s", suffix);
  assert(0);
}

static Node mkaux(int kind, Node body) {
  Node n = mknode(kind, NULL);
  n->body = body;
  return n;
}

static Node mkcvs(Type t, Node body) {
  if (body->type == t)
    return body;

  Node n = mkaux(A_CONVERSION, body);
  n->type = t;

  return n;
}

static Node mkcast(Node n, Type ty) {
  if (ty == voidtype)
    return mknode(A_NOOP, NULL);
  if (!is_scalar(n->type) || !is_scalar(ty))
    errorat(n->token, "scalar type required for cast operator");

  // a cast does not yield an lvalue, always create a conversion node
  n = mkaux(A_CONVERSION, n);
  n->type = ty;
  return n;
}

// usual arithmetic conversions for binary expressions
static Type usual_arithmetic_conversions(Node node) {
  Type t = usual_arithmetic_type(node->left->type, node->right->type);
  node->left = mkcvs(t, node->left);
  node->right = mkcvs(t, node->right);
  return t;
}

static Node unqual_array_to_ptr(Node n) {
  n = mkcvs(unqual(n->type), n);
  if (is_array(n->type))
    n = mkcvs(array_to_ptr(n->type), n);
  return n;
}

static Node mkunary(int kind, Node left, Token token) {
  Node n = mknode(kind, token);
  n->left = (kind == A_ADDRESS_OF || kind == A_POSTFIX_INC ||
             kind == A_POSTFIX_DEC || kind == A_SIZE_OF)
                ? left
                : unqual_array_to_ptr(left);

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.3.2
  if (kind == A_ADDRESS_OF) {
    if (left->kind == A_DEFERENCE)
      return left->left;
    if (is_lvalue(left)) {
      n->type = ptr_type(left->type);
      return n;
    }
    errorat(n->token, "invalid operand of unary '&' (lvalue required)");
  }
  if (kind == A_DEFERENCE) {
    if (left->kind == A_ADDRESS_OF)
      return left->left;
    if (is_ptr(left->type)) {
      n->type = deref_type(left->type);
      return n;
    }
    errorat(n->token, "invalid operand of unary '*' (have '%s')",
            left->type->str);
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.3.3
  if (kind == A_MINUS || kind == A_PLUS) {
    if (is_arithmetic(n->left->type)) {
      n->left = mkcvs(integral_promote(n->left->type), n->left);
      n->type = n->left->type;
      return n;
    }
    errorat(n->token, "invalid operand of unary +/- (have '%s')",
            n->left->type->str);
  }
  if (kind == A_B_NOT) {
    if (is_integer(n->left->type)) {
      n->left = mkcvs(integral_promote(n->left->type), n->left);
      n->type = n->left->type;
      return n;
    }
    errorat(n->token, "invalid operand of unary '~' (have '%s')",
            n->left->type->str);
  }
  if (kind == A_L_NOT) {
    if (is_scalar(n->left->type)) {
      n->type = inttype;
      return n;
    }
    errorat(n->token, "invalid operand of unary '!' (have '%s')",
            n->left->type->str);
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.4
  if (kind == A_POSTFIX_INC || kind == A_POSTFIX_DEC) {
    if (!is_modifiable_lvalue(n->left, 0))
      errorat(n->token, "invalid operand of postfix ++/-- (lvalue required)");
    if (!(is_arithmetic(n->left->type) || is_ptr(n->left->type)))
      errorat(n->token, "invalid operand of postfix ++/-- (have '%s')",
              n->left->type->str);
    n->left = mkcvs(unqual(n->left->type), left);
    n->type = n->left->type;
    return mkcvs(integral_promote(n->type), n);
  }

  if (kind == A_SIZE_OF) {
    Node s = mkicons(unqual(n->left->type)->size);
    s->type = uinttype;
    return s;
  }

  assert(0);
}

static Node mkbinary(int kind, Node left, Node right, Token token) {
  Node n = mknode(kind, token);
  n->left =
      (kind == A_ASSIGN || kind == A_INIT) ? left : unqual_array_to_ptr(left);
  n->right = unqual_array_to_ptr(right);

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.16
  if (kind == A_ASSIGN || kind == A_INIT) {
    if (!is_modifiable_lvalue(n->left, kind == A_INIT))
      errorat(n->left->token,
              "invalid left operand of assignment(modifiable lvalue required)");
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
    } else if (is_ptr(n->left->type) && n->right->kind == A_NUM &&
               n->right->intvalue == 0) {
      // http://port70.net/~nsz/c/c99/n1256.html#6.3.2.3p3
    } else if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
      if (kind == A_ASSIGN) {
        if (!is_const(deref_type(n->left->type)) &&
            is_const(deref_type(n->right->type)))
          errorat(n->token, "assign discards const");
      }
      if (!is_compatible_type(unqual(n->left->type), n->right->type))
        errorat(n->token, "assign with incompatible type");
    } else if (is_struct_or_union(n->left->type) &&
               is_struct_or_union(n->right->type)) {
      if (!is_compatible_type(unqual(n->left->type), n->right->type))
        errorat(n->token, "assign with incompatible type");
    } else
      errorat(n->token, "assign with incompatible type");

    n->type = unqual(n->left->type);
    n->right = mkcvs(n->type, n->right);

    if (kind == A_INIT) {
      n->kind = A_ASSIGN;
      n = mkaux(A_EXPR_STAT, n);
    }
    return n;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.17
  if (kind == A_COMMA) {
    n->type = n->right->type;
    return n;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.14
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.13
  if (kind == A_L_OR || kind == A_L_AND) {
    if (is_scalar(n->left->type) && is_scalar(n->right->type)) {
      n->type = inttype;
      return n;
    }
    errorat(n->token, "invalid operands of logical or/and (scalar required)");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.10
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.11
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.12
  if (kind == A_B_AND || kind == A_B_INCLUSIVEOR || kind == A_B_EXCLUSIVEOR) {
    if (is_integer(n->left->type) && is_integer(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    errorat(n->token, "invalid operand of bitwise operator, scalar required");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.9
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.8
  if (kind >= A_EQ && kind <= A_GE) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      usual_arithmetic_conversions(n);
      n->type = inttype;
      return n;
    }
    if (kind >= A_EQ && kind <= A_NE) {
      if ((is_ptr(n->left->type) &&
           (n->right->kind == A_NUM && n->right->intvalue == 0)) ||
          (is_ptr(n->right->type) &&
           (n->left->kind == A_NUM && n->left->intvalue == 0))) {
        n->type = inttype;
        return n;
      }
    }
    if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
      if (!is_compatible_type(n->left->type, n->right->type))
        errorat(n->token, "operand have incompatible type");
      n->type = inttype;
      return n;
    }

    errorat(n->token, "invalid operand");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.7
  if (kind == A_LEFT_SHIFT || kind == A_RIGHT_SHIFT) {
    if (is_integer(n->left->type) && is_integer(n->right->type)) {
      n->type = integral_promote(n->left->type);
      n->left = mkcvs(n->type, n->left);
      n->right = mkcvs(n->right->type, n->right);
      return n;
    }
    errorat(n->token, "invalid operand (integer required)");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.6
  if (kind == A_ADD) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    if (is_integer(n->left->type) && is_ptr(n->right->type)) {
      n->left =
          mkbinary(A_MUL, n->left, mkicons(n->right->type->base->size), NULL);
      n->type = n->right->type;
      return n;
    }
    if (is_ptr(n->left->type) && is_integer(n->right->type)) {
      n->right =
          mkbinary(A_MUL, n->right, mkicons(n->left->type->base->size), NULL);
      n->type = n->left->type;
      return n;
    }
    errorat(n->token, "invalid operands to binary +");
  }
  if (kind == A_SUB) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    if (is_ptr(n->left->type) && is_integer(n->right->type)) {
      n->right =
          mkbinary(A_MUL, n->right, mkicons(n->left->type->base->size), NULL);
      n->type = n->left->type;
      return n;
    }
    if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
      if (!is_compatible_type(n->left->type, n->right->type))
        errorat(n->token,
                "invalid operands to binary -, incompatiable pointer");
      n->type = inttype;
      return mkbinary(A_DIV, n, mkicons(n->left->type->base->size), NULL);
    }

    errorat(n->token, "invalid operands to binary -");
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.5.5
  if (kind == A_MUL || kind == A_DIV) {
    if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    errorat(n->token, "invalid operands to binary (arighmetic required)");
  }
  if (kind == A_MOD) {
    if (is_integer(n->left->type) && is_integer(n->right->type)) {
      n->type = usual_arithmetic_conversions(n);
      return n;
    }
    errorat(n->token, "invalid operands to binary (integer required)");
  }

  assert(0);
}

static Node mkternary(Node cond, Node left, Node right, Token token) {
  Node n = mknode(A_TERNARY, token);
  n->cond = unqual_array_to_ptr(cond);
  n->left = unqual_array_to_ptr(left);
  n->right = unqual_array_to_ptr(right);

  if (is_arithmetic(n->left->type) && is_arithmetic(n->right->type)) {
    n->type = usual_arithmetic_conversions(n);
    return n;
  }

  if (is_ptr(n->left->type) && is_ptr(n->right->type)) {
    if (!is_compatible_type(n->left->type, n->right->type))
      errorat(n->token, "pointer type mismatch in ternary");
    n->type = n->left->type;
    if (is_array(n->type))
      n->type = array_to_ptr(n->type);
    return n;
  }

  assert(0);
}

static Node mkvar(Token token, Type ty) {
  Node n = mknode(A_VAR, token);
  n->name = token->name;
  n->type = ty;
  n->token = token;
  n->is_global = current_func ? 0 : 1;
  install_symbol(n, SCOPE_INNER);
  return n;
}

static Node mkfunc(Token token, Type ty) {
  Node n = find_symbol(token, A_FUNCTION, SCOPE_FILE);
  if (n) {
    if (!is_compatible_type(n->type, ty)) {
      infoat(token, "previous:");
      errorat(n->token, "conflicting types for function %s", token->name);
    }
    n->token = token;
    n->type =
        n->body ? composite_type(n->type, ty) : composite_type(ty, n->type);
    return n;
  }

  n = mknode(A_FUNCTION, token);
  n->name = token->name;
  n->type = ty;
  install_symbol(n, SCOPE_FILE);

  return n;
}

static Node mkstrlit(Token token) {
  Node n = mknode(A_STRING_LITERAL, token);
  n->string_value = token->name;
  n->type = array_type(chartype, strlen(n->string_value) + 1);
  install_symbol(n, SCOPE_FILE);
  return n;
}

static Node mktag(Token tok, Type ty) {
  Node n = find_symbol(tok, A_TAG, SCOPE_INNER);
  if (n) {
    if (is_enum(ty)) {
      infoat(n->token, "originally defined here:");
      errorat(tok, "redeclaration tag");
    } else if (!ty->member) {
    } else if (!n->type->member) {
      n->token = tok;
      update_struct_or_union_type(n->type, ty->member);
    } else {
      infoat(n->token, "originally defined here:");
      errorat(tok, "redeclaration tag");
    }
  } else {
    n = mknode(A_TAG, tok);
    n->name = tok->name;
    n->type = ty;
    install_symbol(n, SCOPE_INNER);
  }
  return n;
}

// tag for struct/union/enum share namesapce
// http://port70.net/~nsz/c/c99/n1256.html#6.2.3
static Node find_tag(Token tok, int type_kind) {
  Node n = find_symbol(tok, A_TAG, SCOPE_ALL);
  if (n && unqual(n->type)->kind != type_kind) {
    infoat(n->token, "originally defined as:");
    errorat(tok, "conflict tag");
  }

  return n;
}

static Node mkenumconst(Token tok, int value) {
  Node n = mknode(A_ENUM_CONST, tok);
  n->name = tok->name;
  n->type = inttype;
  n->intvalue = value;
  install_symbol(n, SCOPE_INNER);
  return n;
}

static Node mktypedef(Token tok, Type ty) {
  Node n = mknode(A_TYPEDEF, tok);
  n->name = tok->name;
  n->type = ty;
  install_symbol(n, SCOPE_INNER);
  return n;
}

static int match_specifier_typedef() {
  if (match_specifier())
    return 1;

  if (!match(TK_IDENT))
    return 0;

  Node n = find_symbol(token(), A_ANY, SCOPE_ALL);

  return n && n->kind == A_TYPEDEF;
}

/**********************
 * double linked list *
 **********************/
// make a circular doubly linked list node
// if body is NULL, make a dummy head node
static Node mklist(Node body) {
  Node n = mkaux(A_DLIST, body);
  n->next = n->prev = n;
  return n;
}

// insert node to the end of list and return head
static Node list_insert(Node head, Node node) {
  head->prev->next = node;
  node->prev = head->prev;
  node->next = head;
  head->prev = node;
  return head;
}

int list_length(Node head) {
  int l = 0;
  for (Node n = head->next; n != head; n = n->next)
    l++;
  return l;
}

/*****************************
 * recursive parse procedure *
 *****************************/

// trans_unit:              { func_def | declaration }*
// =======================   Function DEF   =======================
// func_def:                declaration_specifiers declarator comp_stat
// =======================   Declaration   =======================
// declaration:             declaration_specifiers
//                          init_declarator { ',' init_declarator }* ';'
// declaration_specifiers:  { type_specifier | type-qualifier }*
// type_qualifier:          { 'const' | 'volatile' | 'restrict' }
// type_specifier:          { 'void' | 'char' | 'int' | 'short' |
//                            'long' | 'unsigned'|'signed'
//                            struct_union_specifier | enum_specifier }
// struct_union_specifier:  {'struct' | 'union'} identifier?
//                          { '{' struct_declaration_list '}' }?
// struct_declaration_list: { {type_qualifier|type_specifier}+
//                           decalrator {',' declarator}      ';' }+
// enum_specifier:          'enum' identifier? '{' enumerator_list ','? '}'
// enumerator_list:         enumerator { ','  enumerator }*
// enumerator:              enumeration-constant { '=' expression }?
// enumeration-constant:    identifier
// init_declarator:         declarator { '=' expression }? -> func_def
// declarator:              pointer? direct_declarator suffix_declarator*
// pointer:                 { '*' { type_qualifier }* }*
// direct_declarator:       '(' declarator ')' |  identifier | Empty
// suffix_declarator:       array_declarator | function_declarator
// array_declarator:        '[' expression ']'
// function_declarator:     '(' { parameter_list }* ')'
// parameter_list:          parameter_declaration {',' parameter_declaration}*
// parameter_declaration:   declaration_specifiers declarator
// type_name:               declaration_specifiers declarator
// =======================   Statement   =======================
// statement:      expr_stat | comp_stat
//                 selection-stat | iteration-stat | jump_stat
// selection-stat: if_stat
// if_stat:        'if' '('  expr_stat ')' statement { 'else' statement }
// iteration-stat: while_stat | dowhile_stat | for_stat
// while_stat:     'while' '(' expr_stat ')' statement
// dowhile_stat:   'do' statement 'while' '(' expression ')' ';'
// for_stat:       'for' '(' {expression}; {expression}; {expression}')'
//                 statement
// jump_stat:      break ';' | continue ';' | 'return' expression? ';'
// comp_stat:      '{' {declaration}* {stat}* '}'
// expr_stat:      {expression}? ';'
// =======================   Expression   =======================
// expression:         comma_expr
// comma_expr:         assign_expr { ', ' assign_expr }*
// assign_expr:        conditional_expr { '=' assign_expr }
// conditional_expr:   logical_or_expr { '?' expression : conditional_expr }?
// logical_or_expr:    logical_and_expr { '||' logical_and_expr }*
// logical_and_expr:   equality_expr { '&&' inclusive_or_expr }*
// inclusive_or_expr:  exclusive_or_expr { '|' exclusive_or_expr }*
// exclusive_or_expr:  bitwise_and_expr { '^' bitwise_and_expr }*
// bitwise_and_expr:   equality_expr { '&' equality_expr}*
// equality_expr:      relational_expr { '==' | '!='  relational_expr }*
// relational_expr:    sum_expr { '>' | '<' | '>=' | '<=' shift_expr}*
// shift_expr:         sum_expr { '<<' | '>>' sum_expr }*
// sum_expr:           mul_expr { '+'|'-' mul_expr }*
// mul_expr:           cast_expr { '*'|'/' | '%' cast_expr }*
// cast_expr:          {'(' type-name ')'}* unary_expr
// unary_expr:         { {'*'|'&'|'+'|'-'|'~'|'!'|'} cast_expr |
//                       {'++' |  '--' } * postfix_expr |
//                       sizeof '(' type_name ')' |
//                       sizeof unary_expr }
// postfix_expr:       primary_expr {arg_list|array_subscripting|'++'|'--' }*
// primary_expr:       identifier | number | string-literal | '('expression ')'
// arg_list:           '(' expression { ',' expression } ')'
// array_subscripting: '[' expression ']'

static void trans_unit();
static Node declaration();
static Type declaration_specifiers(int* is_typedef);
static Type struct_union_specifier();
static Member struct_declaration_list();
static Type enum_specifier();
static Node init_declarator(Type ty, int is_typedef);
static Type declarator(Type ty, Token* tok);
static Type pointer(Type ty);
static Type direct_declarator(Type ty, Token* tok);
static Type array_declarator(Type base);
static Type function_declarator(Type ty);
static Type type_name();
static Node function(Node f);
static Node statement(int reuse_scope);
static Node comp_stat(int reuse_scope);
static Node if_stat();
static Node while_stat();
static Node dowhile_stat();
static Node for_stat();
static Node expr_stat();
static Node expression();
static Node comma_expr();
static Node assign_expr();
static Node conditional_expr();
static Node logical_or_expr();
static Node logical_and_expr();
static Node inclusive_or_expr();
static Node exclusive_or_expr();
static Node bitwise_and_expr();
static Node eq_expr();
static Node rel_expr();
static Node shift_expr();
static Node sum_expr();
static Node mul_expr();
static Node cast_expr();
static Node unary_expr();
static Node postfix_expr();
static Node primary_expr();
static Node func_call(Node n);
static Node array_subscripting(Node n);
static Node member_selection(Node n);
static Node ordinary(Node n);

static void trans_unit() {
  while (token())
    declaration();
}

// for struct type declaration without declarator
//    make tag/type(return NULL)
// for typedef
//    make type(return NULL)
// for local declaration(called by comp_stat/for_stat)
//    make local variable(return node list for init assign)
// for global declaration(called by trans_unit)
//    make global variable(set it init_value member, return NULL)
//    make function node(return the function node)
// for function definition(called by trans_unit)
//    make function node && fill body(return NULL)
static Node declaration() {
  Token tok;
  int is_typedef;
  Type ty = declaration_specifiers(&is_typedef);

  if ((tok = consume(TK_SIMI))) {
    if (!is_struct_or_union(ty) && !is_enum(ty))
      errorat(tok, "declare nothing");
    return NULL;
  }

  Node n = init_declarator(ty, is_typedef);
  if (n && n->kind == A_FUNCTION) {
    if (match(TK_OPENING_BRACES)) {
      function(n);
      return NULL;
    }
    n = NULL;
  }

  struct node head = {0};
  Node last = &head;
  last->next = n;
  while (last->next)
    last = last->next;
  while (!consume(TK_SIMI)) {
    expect(TK_COMMA);
    n = init_declarator(ty, is_typedef);
    last->next = (n && n->kind == A_FUNCTION) ? NULL : n;
    while (last->next)
      last = last->next;
  }
  return head.next;
}

#define get_specifier(specs, spec) (specs & spec)
#define set_specifier(specs, spec) (specs |= spec)
enum {
  // type specifiers
  SPEC_VOID = 1 << 0,
  SPEC_CHAR = 1 << 1,
  SPEC_SHORT = 1 << 2,
  SPEC_INT = 1 << 3,
  SPEC_LONG1 = 1 << 4,
  SPEC_LONG2 = 1 << 5,
  SPEC_SIGNED = 1 << 6,
  SPEC_UNSIGNED = 1 << 7,
  SPEC_VOLATILE = 1 << 8,
  SPEC_CONST = 1 << 9,
  SPEC_RESTRICT = 1 << 10
};

static Type declaration_specifiers(int* is_typedef) {
  Token tok;
  Type struct_or_union_type = NULL;
  Type enum_type = NULL;
  Type typedef_type = NULL;
  int specifiers = 0;
  if (is_typedef)
    *is_typedef = 0;
  for (;;) {
    if ((tok = consume(TK_VOID))) {
      if (get_specifier(specifiers, SPEC_VOID))
        errorat(tok, "two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_VOID);
    } else if ((tok = consume(TK_CHAR))) {
      if (get_specifier(specifiers, SPEC_CHAR))
        errorat(tok, "two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_CHAR);
    } else if ((tok = consume(TK_SHORT))) {
      if (get_specifier(specifiers, SPEC_SHORT))
        errorat(tok, "two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_SHORT);
    } else if ((tok = consume(TK_INT))) {
      if (get_specifier(specifiers, SPEC_INT))
        errorat(tok, "two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_INT);
    } else if ((tok = consume(TK_LONG))) {
      if (get_specifier(specifiers, SPEC_LONG1)) {
        if (get_specifier(specifiers, SPEC_LONG2))
          errorat(tok, "two or more data types in declaration specifiers");
        else
          set_specifier(specifiers, SPEC_LONG2);
      } else {
        set_specifier(specifiers, SPEC_LONG1);
      }
    } else if ((tok = consume(TK_UNSIGNED))) {
      if (get_specifier(specifiers, SPEC_UNSIGNED))
        errorat(tok, "two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_UNSIGNED);
    } else if ((tok = consume(TK_SIGNED))) {
      if (get_specifier(specifiers, SPEC_SIGNED))
        errorat(tok, "two or more data types in declaration specifiers");
      set_specifier(specifiers, SPEC_SIGNED);
    } else if ((tok = consume(TK_CONST))) {
      set_specifier(specifiers, SPEC_CONST);
    } else if ((tok = consume(TK_VOLATILE))) {
      set_specifier(specifiers, SPEC_VOLATILE);
    } else if ((tok = consume(TK_RESTRICT))) {
      set_specifier(specifiers, SPEC_RESTRICT);
    } else if ((tok = match(TK_STRUCT)) || (tok = match(TK_UNION))) {
      if (struct_or_union_type)
        errorat(tok, "two or more data types in declaration specifiers");
      struct_or_union_type = struct_union_specifier();
    } else if ((tok = match(TK_ENUM))) {
      if (enum_type)
        errorat(tok, "two or more data types in declaration specifiers");
      enum_type = enum_specifier();
    } else if ((tok = consume(TK_TYPEDEF))) {
      if (!is_typedef)
        errorat(tok, "unexpected typedef");
      if (*is_typedef)
        errorat(tok, "duplicate typedef");
      *is_typedef = 1;
    } else if (match(TK_IDENT)) {
      Node n = find_symbol(token(), A_ANY, SCOPE_ALL);
      if (typedef_type || !n || n->kind != A_TYPEDEF)
        break;
      expect(TK_IDENT);
      typedef_type = n->type;
    } else
      break;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.7.2
  Type ty;
  int type_spec = specifiers & ((1 << 8) - 1);
  switch (type_spec) {
    case SPEC_VOID:
      ty = voidtype;
      break;
    case SPEC_CHAR:
    case SPEC_CHAR | SPEC_SIGNED:
      ty = chartype;
      break;
    case SPEC_CHAR | SPEC_UNSIGNED:
      ty = uchartype;
      break;
    case SPEC_SHORT:
    case SPEC_SIGNED | SPEC_SHORT:
    case SPEC_SHORT | SPEC_INT:
    case SPEC_SIGNED | SPEC_SHORT | SPEC_INT:
      ty = shorttype;
      break;
    case SPEC_UNSIGNED | SPEC_SHORT:
    case SPEC_UNSIGNED | SPEC_SHORT | SPEC_INT:
      ty = ushorttype;
      break;
    case SPEC_INT:
    case SPEC_SIGNED:
    case SPEC_INT | SPEC_SIGNED:
      ty = inttype;
      break;
    case SPEC_UNSIGNED:
    case SPEC_UNSIGNED | SPEC_INT:
      ty = uinttype;
      break;
    case SPEC_LONG1:
    case SPEC_SIGNED | SPEC_LONG1:
    case SPEC_LONG1 | SPEC_INT:
    case SPEC_SIGNED | SPEC_LONG1 | SPEC_INT:
      ty = longtype;
      break;
    case SPEC_UNSIGNED | SPEC_LONG1:
    case SPEC_UNSIGNED | SPEC_LONG1 | SPEC_INT:
      ty = ulongtype;
      break;
    case SPEC_LONG1 | SPEC_LONG2:
    case SPEC_SIGNED | SPEC_LONG1 | SPEC_LONG2:
    case SPEC_LONG1 | SPEC_LONG2 | SPEC_INT:
    case SPEC_SIGNED | SPEC_LONG1 | SPEC_LONG2 | SPEC_INT:
      ty = longtype;
      break;
    case SPEC_UNSIGNED | SPEC_LONG1 | SPEC_LONG2:
    case SPEC_UNSIGNED | SPEC_LONG1 | SPEC_LONG2 | SPEC_INT:
      ty = ulongtype;
      break;
    default:
      ty = NULL;
  }

  if (ty && struct_or_union_type)
    errorat(tok, "two or more data types in declaration specifiers");
  ty = ty ? ty : struct_or_union_type;
  if (ty && enum_type)
    errorat(tok, "two or more data types in declaration specifiers");
  ty = ty ? ty : enum_type;
  if (ty && typedef_type)
    errorat(tok, "two or more data types in declaration specifiers");
  ty = ty ? ty : typedef_type;
  if (!ty)
    errorat(token(), "no data type in declaration specifiers");

  // cache nothing in register, so all variable is VOLATILE?
  if (get_specifier(specifiers, SPEC_CONST))
    ty = const_type(ty);
  if (get_specifier(specifiers, SPEC_RESTRICT))
    errorat(tok, "not implemented: restrict");
  return ty;
}

static Member mkmember(Type type, Token tok) {
  Member p = calloc(1, sizeof(struct member));
  p->type = type;
  p->name = tok->name;
  p->token = tok;
  return p;
}

static void link_member(Member head, Member m) {
  while (head->next) {
    if (m->name && head->next->name == m->name)
      errorat(token(), "redefinition of member %s", m->name);
    head = head->next;
  }
  head->next = m;
}

static Member struct_declaration_list() {
  Token tok;
  struct member head = {0};
  if (consume(TK_OPENING_BRACES)) {
    while (!(tok = consume(TK_CLOSING_BRACES))) {
      Type ty = declaration_specifiers(NULL);
      Token mem_name = NULL;
      Type mem_ty = declarator(ty, &mem_name);
      if (!mem_name)
        errorat(token(), "empty member name");
      link_member(&head, mkmember(mem_ty, mem_name));
      while (!consume(TK_SIMI)) {
        expect(TK_COMMA);
        mem_name = NULL;
        mem_ty = declarator(ty, &mem_name);
        if (!mem_name)
          errorat(token(), "empty member name");
        link_member(&head, mkmember(mem_ty, mem_name));
      }
    }

    if (!head.next)
      errorat(tok, "empty struct");
  }
  return head.next;
}

static Type struct_union_specifier() {
  int ty_kind = consume(TK_STRUCT) ? TY_STRUCT : (expect(TK_UNION), TY_UNION);
  Token tok = consume(TK_IDENT);
  if (tok) {
    char buff[4096];
    sprintf(buff, ".tag.%s", tok->name);
    tok->name = string(buff);
  }
  Member member = struct_declaration_list();
  if (member) {
    Type ty = struct_or_union_type(member, tok ? tok->name : NULL, ty_kind);
    if (tok)
      mktag(tok, ty);
    return ty;
  }

  Node tag = find_tag(tok, ty_kind);
  if (!tag)
    tag = mktag(tok, struct_or_union_type(NULL, tok->name, ty_kind));
  return tag->type;
}

static void enumerator_list() {
  Token tok;
  expect(TK_OPENING_BRACES);
  int value = -1;
  while (!consume(TK_CLOSING_BRACES)) {
    tok = expect(TK_IDENT);
    if (consume(TK_EQUAL))
      value = mkiconsstr(consume(TK_NUM))->intvalue;
    else
      value++;
    mkenumconst(tok, value);
    if (!match(TK_CLOSING_BRACES))
      expect(TK_COMMA);
  }
}

static Type enum_specifier() {
  expect(TK_ENUM);
  Token tok = consume(TK_IDENT);
  if (tok) {
    char buff[4096];
    sprintf(buff, ".tag.%s", tok->name);
    tok->name = string(buff);
  }
  if (match(TK_OPENING_BRACES)) {
    enumerator_list();
    Type ty = enum_type(tok->name);
    if (tok)
      mktag(tok, ty);
    return ty;
  }

  return find_tag(tok, TY_ENUM)->type;
}

// returned value for different kind of declarotor
//    function:        function node in global
//    local variable:  nodes for init assign
//    global variable: NULL
static Node init_declarator(Type ty, int is_typedef) {
  Token tok;
  ty = declarator(ty, &tok);
  if (!tok)
    errorat(token(), "identifier name required in declarator");

  if (is_typedef) {
    mktypedef(tok, ty);
    return NULL;
  }

  if (is_funcion(ty))
    return mkfunc(tok, ty);

  Node n = mkvar(tok, ty);
  if ((tok = consume(TK_EQUAL))) {
    Node e = assign_expr();
    if (is_array(ty))
      errorat(n->token, "not implemented: initialize array");
    if (current_func)
      return mkbinary(A_INIT, n, e, tok);
    if (e->kind != A_NUM && e->kind != A_STRING_LITERAL)
      errorat(n->token, "initializer element is not constant");
    n->init_value = e;
    return NULL;
  }
  return NULL;
}

static Type placeholdertype =
    &(struct type){TY_PLACEHOLDER, -1, NULL, "placeholder"};

// replace placeholder type by real base type
static Type construct_type(Type base, Type ty) {
  if (ty == placeholdertype)
    return base;
  if (is_array(ty))
    return array_type(construct_type(base, ty->base),
                      ty->size / ty->base->size);
  if (is_ptr(ty))
    return ptr_type(construct_type(base, ty->base));

  if (is_funcion(ty))
    return function_type(construct_type(base, ty->base), ty->proto);

  assert(0);
}

// TODO: FIX THIS
// We can't parse an abstract function type (function declaration without
// name). The parenthese is ambiguous(complicated direct_declarator or
// function declarator suffix). For example, in "int *(int);", the '(' may be
// parsed as a complicated direct_declarator, however it should be a suffix
// indicate a function declarator.  It not a problem for function pointer, for
// example: "int (*)(int,...)". This should be supported because:
//   (http://port70.net/~nsz/c/c99/n1256.html#6.3.2.1p4)
static Type declarator(Type base, Token* tok) {
  base = pointer(base);

  Type ty = direct_declarator(placeholdertype, tok);

  if (match(TK_OPENING_PARENTHESES))
    base = function_declarator(base);
  if (match(TK_OPENING_BRACKETS))
    base = array_declarator(base);

  return construct_type(base, ty);
}

static Type pointer(Type ty) {
  Token tok;
  while (consume(TK_STAR)) {
    ty = ptr_type(ty);
    int q_const = 0, q_restrict = 0;
    for (;;) {
      if ((tok = consume(TK_CONST)))
        q_const = 1;
      else if ((tok = consume(TK_VOLATILE)))
        ;
      else if ((tok = consume(TK_RESTRICT)))
        q_restrict = 1;
      else {
        if (q_restrict)
          errorat(tok, "not implemented: restrict pointer");
        if (q_const)
          ty = const_type(ty);
        break;
      }
    }
  }
  return ty;
}

static Type direct_declarator(Type ty, Token* tok) {
  if ((*tok = consume(TK_IDENT))) {
    return ty;
  } else if (consume(TK_OPENING_PARENTHESES)) {
    ty = declarator(ty, tok);
    expect(TK_CLOSING_PARENTHESES);
    return ty;
  }
  return ty;
}

static Type array_declarator(Type base) {
  if (consume(TK_OPENING_BRACKETS)) {
    if (consume(TK_CLOSING_BRACKETS))
      return array_type(array_declarator(base), 0);

    Node n = expression();
    expect(TK_CLOSING_BRACKETS);
    if (n->kind != A_NUM)
      errorat(n->token, "not implemented: variable length array");
    if (n->intvalue == 0)
      errorat(n->token, "array size shall greater than zero");
    return array_type(array_declarator(base), n->intvalue);
  }
  return base;
}

static Proto mkproto(Type type, Token token) {
  Proto p = calloc(1, sizeof(struct proto));
  p->type = type;
  p->name = token ? token->name : NULL;
  p->token = token;
  return p;
}

static void link_proto(Proto head, Proto p) {
  if ((head->next && head->next->type == voidtype) ||  // other follow void
      (p->type == voidtype && head->next))             // void is not first
    // TODO: keep type's token...
    errorat(p->token ? p->token : token(), "void must be the only parameter");

  while (head->next) {
    if (p->name && head->next->name == p->name) {
      infoat(head->next->token, "previous:");
      errorat(p->token, "redeclaration of parameter %s", p->name);
    }
    head = head->next;
  }
  head->next = p;
}

static Type function_declarator(Type ty) {
  Token tok;
  struct proto head = {0};

  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    if ((tok = consume(TK_ELLIPSIS))) {
      if (!head.next)
        errorat(tok, "a named paramenter is required before ...");
      link_proto(&head, mkproto(type(TY_VARARG, NULL, 0), NULL));
      expect(TK_CLOSING_PARENTHESES);
      break;
    }

    Token tok = NULL;
    Type type = declarator(declaration_specifiers(NULL), &tok);
    if (is_array(type))
      type = array_to_ptr(type);
    link_proto(&head, mkproto(type, tok));

    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  }

  if (match(TK_OPENING_BRACES) && !head.next)
    link_proto(&head, mkproto(voidtype, NULL));

  return function_type(ty, head.next);
}

static Type type_name() {
  Type ty = declaration_specifiers(NULL);
  Token name = NULL;
  ty = declarator(ty, &name);
  if (name)
    errorat(name, "type name required");
  return ty;
}

static Node function(Node f) {
  // TODO: how can we get previous definition
  if (f->body)
    errorat(f->token, "redefinition of function %s", f->name);

  enter_func(f);
  enter_scope();
  // paramenter
  f->params = mklist(NULL);
  if (f->type->proto->type != voidtype)
    for (Proto p = f->type->proto; p; p = p->next) {
      if (p->type->kind == TY_VARARG)
        continue;
      if (!p->token)
        // TODO: errorat that type
        errorat(f->token, "omitting parameter name in function definition");
      list_insert(f->params, mklist(mkvar(p->token, p->type)));
    }

  f->body = comp_stat(1);

  exit_func(f);
  exit_scope();
  return NULL;
}

static Node statement(int reuse_scope) {
  Token tok;

  // compound statement
  if (match(TK_OPENING_BRACES)) {
    return comp_stat(reuse_scope);
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
  if ((tok = consume(TK_BREAK))) {
    expect(TK_SIMI);
    return mknode(A_BREAK, tok);
  }
  if ((tok = consume(TK_CONTINUE))) {
    expect(TK_SIMI);
    return mknode(A_CONTINUE, tok);
  }
  if ((tok = consume(TK_RETURN))) {
    Node n = mknode(A_RETURN, tok);
    if (consume(TK_SIMI))
      return n;
    n->body = mkcvs(current_func->type->base, expression());
    expect(TK_SIMI);
    return n;
  }

  // expr statement
  return expr_stat();
}

static Node comp_stat(int reuse_scope) {
  struct node head = {0};
  Node last = &head;
  expect(TK_OPENING_BRACES);
  if (!reuse_scope)
    enter_scope();
  while (!match(TK_CLOSING_BRACES)) {
    if (match_specifier_typedef())
      last->next = declaration();
    else
      last->next = statement(0);
    while (last->next)
      last = last->next;
  }
  expect(TK_CLOSING_BRACES);
  if (!reuse_scope)
    exit_scope();
  return mkaux(A_BLOCK, head.next);
}

static Node if_stat() {
  Node n = mknode(A_IF, expect(TK_IF));
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  n->then = statement(0);
  if (consume(TK_ELSE)) {
    n->els = statement(0);
  }
  return n;
}

static Node while_stat() {
  Node n = mknode(A_FOR, expect(TK_WHILE));
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  n->body = statement(0);
  return n;
}

static Node dowhile_stat() {
  Node n = mknode(A_DOWHILE, expect(TK_DO));
  n->body = statement(0);
  expect(TK_WHILE);
  expect(TK_OPENING_PARENTHESES);
  n->cond = expression();
  expect(TK_CLOSING_PARENTHESES);
  expect(TK_SIMI);
  return n;
}

static Node for_stat() {
  Node n = mknode(A_FOR, expect(TK_FOR));
  expect(TK_OPENING_PARENTHESES);
  enter_scope();
  if (!consume(TK_SIMI))
    n->init =
        match_specifier_typedef() ? mkaux(A_BLOCK, declaration()) : expr_stat();

  if (!consume(TK_SIMI)) {
    n->cond = expression();
    expect(TK_SIMI);
  }
  if (!consume(TK_CLOSING_PARENTHESES)) {
    n->post = mkaux(A_EXPR_STAT, expression());
    expect(TK_CLOSING_PARENTHESES);
  }
  n->body = statement(1);
  exit_scope();
  return n;
}

static Node expr_stat() {
  if (consume(TK_SIMI)) {
    return NULL;
  }

  Node n = expression();
  expect(TK_SIMI);
  return mkaux(A_EXPR_STAT, n);
}

static Node expression() {
  return comma_expr();
}

static Node comma_expr() {
  Token tok;
  Node n = assign_expr();
  for (;;) {
    if ((tok = consume(TK_COMMA)))
      n = mkbinary(A_COMMA, n, assign_expr(), tok);
    else
      return n;
  }
}

static Node assign_expr() {
  Token tok;
  Node n = conditional_expr();
  for (;;) {
    if ((tok = consume(TK_EQUAL)))
      n = mkbinary(A_ASSIGN, n, assign_expr(), tok);
    // TODO: indenpended ast node for each coumpund assignment
    else if ((tok = consume(TK_STAR_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_MUL, n, assign_expr(), NULL), tok);
    else if ((tok = consume(TK_SLASH_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_DIV, n, assign_expr(), NULL), tok);
    else if ((tok = consume(TK_PLUS_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_ADD, n, assign_expr(), NULL), tok);
    else if ((tok = consume(TK_MINUS_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_SUB, n, assign_expr(), NULL), tok);
    else if ((tok = consume(TK_LEFT_SHIFT_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_LEFT_SHIFT, n, assign_expr(), NULL),
                   tok);
    else if ((tok = consume(TK_RIGHT_SHIFT_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_RIGHT_SHIFT, n, assign_expr(), NULL),
                   tok);
    else if ((tok = consume(TK_AND_EQUAL)))
      n = mkbinary(A_ASSIGN, n, mkbinary(A_B_AND, n, assign_expr(), NULL), tok);
    else if ((tok = consume(TK_CARET_EQUAL)))
      n = mkbinary(A_ASSIGN, n,
                   mkbinary(A_B_EXCLUSIVEOR, n, assign_expr(), NULL), tok);
    else if ((tok = consume(TK_BAR_EQUAL)))
      n = mkbinary(A_ASSIGN, n,
                   mkbinary(A_B_INCLUSIVEOR, n, assign_expr(), NULL), tok);
    else
      return n;
  }
}

static Node conditional_expr() {
  Token tok;
  Node n = logical_or_expr();
  if ((tok = consume(TK_QUESTIONMARK))) {
    Node e = expression();
    expect(TK_COLON);
    n = mkternary(n, e, conditional_expr(), tok);
  }
  return n;
}

static Node logical_or_expr() {
  Token tok;
  Node n = logical_and_expr();
  for (;;) {
    if ((tok = consume(TK_BAR_BAR)))
      n = mkbinary(A_L_OR, n, logical_and_expr(), tok);
    else
      return n;
  }
}

static Node logical_and_expr() {
  Token tok;
  Node n = inclusive_or_expr();
  for (;;) {
    if ((tok = consume(TK_AND_AND)))
      n = mkbinary(A_L_AND, n, inclusive_or_expr(), tok);
    else
      return n;
  }
}

static Node inclusive_or_expr() {
  Token tok;
  Node n = exclusive_or_expr();
  for (;;) {
    if ((tok = consume(TK_BAR)))
      n = mkbinary(A_B_INCLUSIVEOR, n, exclusive_or_expr(), tok);
    else
      return n;
  }
}

static Node exclusive_or_expr() {
  Token tok;
  Node n = bitwise_and_expr();
  for (;;) {
    if ((tok = consume(TK_CARET)))
      n = mkbinary(A_B_EXCLUSIVEOR, n, bitwise_and_expr(), tok);
    else
      return n;
  }
}

static Node bitwise_and_expr() {
  Token tok;
  Node n = eq_expr();
  for (;;) {
    if ((tok = consume(TK_AND)))
      n = mkbinary(A_B_AND, n, eq_expr(), tok);
    else
      return n;
  }
}

static Node eq_expr() {
  Token tok;
  Node n = rel_expr();
  for (;;) {
    if ((tok = consume(TK_EQUAL_EQUAL)))
      n = mkbinary(A_EQ, n, rel_expr(), tok);
    else if ((tok = consume(TK_NOT_EQUAL)))
      n = mkbinary(A_NE, n, rel_expr(), tok);
    else
      return n;
  }
}

static Node rel_expr() {
  Token tok;
  Node n = shift_expr();
  for (;;) {
    if ((tok = consume(TK_GREATER)))
      n = mkbinary(A_GT, n, shift_expr(), tok);
    else if ((tok = consume(TK_LESS)))
      n = mkbinary(A_LT, n, shift_expr(), tok);
    else if ((tok = consume(TK_GREATER_EQUAL)))
      n = mkbinary(A_GE, n, shift_expr(), tok);
    else if ((tok = consume(TK_LESS_EQUAL)))
      n = mkbinary(A_LE, n, shift_expr(), tok);
    else
      return n;
  }
}

static Node shift_expr() {
  Token tok;
  Node n = sum_expr();
  for (;;) {
    if ((tok = consume(TK_LEFT_SHIFT)))
      n = mkbinary(A_LEFT_SHIFT, n, sum_expr(), tok);
    else if ((tok = consume(TK_RIGHT_SHIFT)))
      n = mkbinary(A_RIGHT_SHIFT, n, sum_expr(), tok);
    else
      return n;
  }
}

static Node sum_expr() {
  Token tok;
  Node n = mul_expr();
  for (;;) {
    if ((tok = consume(TK_PLUS)))
      n = mkbinary(A_ADD, n, mul_expr(), tok);
    else if ((tok = consume(TK_MINUS)))
      n = mkbinary(A_SUB, n, mul_expr(), tok);
    else
      return n;
  }
}

static Node mul_expr() {
  Token tok;
  Node n = cast_expr();
  for (;;) {
    if ((tok = consume(TK_STAR)))
      n = mkbinary(A_MUL, n, cast_expr(), tok);
    else if ((tok = consume(TK_SLASH)))
      n = mkbinary(A_DIV, n, cast_expr(), tok);
    else if ((tok = consume(TK_PERCENT)))
      n = mkbinary(A_MOD, n, cast_expr(), tok);
    else
      return n;
  }
}

static Node cast_expr() {
  if (match(TK_OPENING_PARENTHESES)) {
    Token back = token();
    consume(TK_OPENING_PARENTHESES);
    if (match_specifier_typedef()) {
      Type ty = type_name();
      expect(TK_CLOSING_PARENTHESES);
      return mkcast(cast_expr(), ty);
    }
    set_token(back);
  }
  return unary_expr();
}

static Node unary_expr() {
  Token tok;
  if ((tok = consume(TK_AND)))
    return mkunary(A_ADDRESS_OF, cast_expr(), tok);
  if ((tok = consume(TK_STAR)))
    return mkunary(A_DEFERENCE, cast_expr(), tok);
  if ((tok = consume(TK_PLUS)))
    return mkunary(A_PLUS, cast_expr(), tok);
  if ((tok = consume(TK_MINUS)))
    return mkunary(A_MINUS, cast_expr(), tok);
  if ((tok = consume(TK_TILDE)))
    return mkunary(A_B_NOT, cast_expr(), tok);
  if ((tok = consume(TK_EXCLAMATION)))
    return mkunary(A_L_NOT, cast_expr(), tok);
  if ((tok = consume(TK_PLUS_PLUS))) {
    Node u = unary_expr();
    return mkbinary(A_ASSIGN, u, mkbinary(A_ADD, u, mkicons(1), NULL), tok);
  }
  if ((tok = consume(TK_MINUS_MINUS))) {
    Node u = unary_expr();
    return mkbinary(A_ASSIGN, u, mkbinary(A_SUB, u, mkicons(1), NULL), tok);
  }
  if ((tok = consume(TK_SIZEOF))) {
    Token back = token();
    if (consume(TK_OPENING_PARENTHESES) && match_specifier_typedef()) {
      Node u = mknode(A_NOOP, NULL);
      u->type = type_name();
      expect(TK_CLOSING_PARENTHESES);
      return mkunary(A_SIZE_OF, u, tok);
    }
    set_token(back);
    return mkunary(A_SIZE_OF, unary_expr(), tok);
  }

  return postfix_expr();
}

static Node postfix_expr() {
  Token tok;
  Node n = primary_expr();
  for (;;) {
    if (match(TK_OPENING_PARENTHESES))
      n = func_call(n);
    else if (match(TK_OPENING_BRACKETS))
      n = array_subscripting(n);
    else if (match(TK_DOT) || match(TK_ARROW))
      n = member_selection(n);
    else if ((tok = consume(TK_PLUS_PLUS)))
      n = mkunary(A_POSTFIX_INC, n->kind == A_IDENT ? ordinary(n) : n, tok);
    else if ((tok = consume(TK_MINUS_MINUS)))
      n = mkunary(A_POSTFIX_DEC, n->kind == A_IDENT ? ordinary(n) : n, tok);
    else
      return n->kind == A_IDENT ? ordinary(n) : n;
  }
}

static Node primary_expr() {
  Token tok;

  // constant
  if ((tok = consume(TK_NUM))) {
    return mkiconsstr(tok);
  }

  // string literal
  if ((tok = consume(TK_STRING)))
    return mkstrlit(tok);

  // identifier
  if ((tok = consume(TK_IDENT))) {
    Node n = mknode(A_IDENT, tok);
    n->name = tok->name;
    return n;
  }

  // parentheses
  if (consume(TK_OPENING_PARENTHESES)) {
    Node n = expression();
    expect(TK_CLOSING_PARENTHESES);
    return n;
  }

  errorat(token(), "parse: primary get unexpected token");
  assert(0);
}

static Node func_call(Node n) {
  if (n->kind != A_IDENT)
    errorat(n->token, "not implemented");
  const char* name = n->name;

  Node f = find_symbol(n->token, A_FUNCTION, SCOPE_FILE);
  if (!f) {  //  implicit function declaration
    warn("implicit declaration of function %s", name);
    f = mkfunc(n->token, function_type(inttype, NULL));
  }

  n = mknode(A_FUNC_CALL, n->token);
  n->name = name;
  n->type = f->type->base;
  // function argument lists
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.2p6
  n->args = mklist(NULL);
  Proto param = f->type->proto;
  expect(TK_OPENING_PARENTHESES);
  while (!consume(TK_CLOSING_PARENTHESES)) {
    Node arg = unqual_array_to_ptr(assign_expr());
    if (!f->type->proto) {
      arg = mkcvs(default_argument_promoe(arg->type), arg);
    } else if (!param || param->type == voidtype) {
      infoat(f->token, "function declared:");
      errorat(n->token, "too many arguments to function %s", name);
    } else if (param->type->kind == TY_VARARG) {
      arg = mkcvs(default_argument_promoe(arg->type), arg);
    } else {
      // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.2p7
      if (is_arithmetic(param->type) && is_arithmetic(arg->type)) {
      } else if (is_ptr(param->type) && arg->kind == A_NUM &&
                 arg->intvalue == 0) {
      } else if (is_ptr(param->type) && is_ptr(arg->type)) {
        if (!is_const(deref_type(param->type)) &&
            is_const(deref_type(arg->type))) {
          infoat(param->token, "paramenter declared:");
          errorat(arg->token, "function call discards const");
        }
        if (!is_compatible_type(unqual(param->type), arg->type)) {
          infoat(param->token, "paramenter declared:");
          errorat(arg->token, "argument has incompatible type");
        }
      } else if (is_struct_or_union(param->type) &&
                 is_struct_or_union(arg->type)) {
        if (!is_compatible_type(unqual(param->type), arg->type)) {
          infoat(param->token, "paramenter declared:");
          errorat(arg->token, "argument has incompatible type");
        }
      } else {
        infoat(param->token, "paramenter declared:");
        errorat(arg->token, "argument has incompatible type");
      }
      arg = mkcvs(unqual(param->type), arg);

      param = param->next;
    }
    list_insert(n->args, mklist(arg));
    if (!match(TK_CLOSING_PARENTHESES))
      expect(TK_COMMA);
  };
  if (param && param->type != voidtype) {
    infoat(f->token, "function declared:");
    errorat(n->token, "too few arguments to function call");
  }
  return n;
}

static Node array_subscripting(Node n) {
  Token tok;
  Node a = n;
  if (n->kind == A_IDENT) {
    if (!(a = find_symbol(n->token, A_VAR, SCOPE_ALL)))
      errorat(n->token, "undeclared variable");
  }

  while ((tok = consume(TK_OPENING_BRACKETS))) {
    if (!is_ptr(a->type))
      errorat(tok, "only array/pointer can be subscripted");
    Node s = mknode(A_ARRAY_SUBSCRIPTING, tok);
    s->index = mkcvs(longtype, expression());
    s->array = a;
    s->type = a->type->base;
    a = s;
    expect(TK_CLOSING_BRACKETS);
  }
  return a;
}

static Node member_selection(Node n) {
  Token tok;
  Node a = n;
  if (n->kind == A_IDENT) {
    if (!(a = find_symbol(n->token, A_VAR, SCOPE_ALL)))
      errorat(n->token, "undefined variable");
  }

  if ((tok = consume(TK_ARROW))) {
    if (!is_ptr(a->type))
      errorat(a->token, "pointer expected for -> operator");
    a = mkunary(A_DEFERENCE, a, tok);
  } else
    tok = expect(TK_DOT);

  if (!is_struct_or_union(a->type))
    errorat(a->token, "select member in something not a structure/union");

  Node s = mknode(A_MEMBER_SELECTION, tok);
  s->structure = a;
  tok = expect(TK_IDENT);
  s->member = get_struct_or_union_member(a->type, tok->name);
  if (!s->member)
    errorat(tok, "struct/union has no wanted member");
  s->type = is_const(a->type) ? const_type(s->member->type) : s->member->type;

  return s;
}

static Node ordinary(Node n) {
  // http://port70.net/~nsz/c/c99/n1256.html#6.2.3
  Node a = find_symbol(n->token, A_ANY, SCOPE_ALL);
  if (!a)
    errorat(n->token, "undefined variable");
  if (a->kind != A_VAR && a->kind != A_ENUM_CONST)
    errorat(n->token, "unknown identifer kind");
  return a;
}

void parse() {
  trans_unit();
}