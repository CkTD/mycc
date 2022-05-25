#include <stdio.h>
#include <stdlib.h>
#include "inc.h"

Type voidtype = &(struct type){TY_VOID, 0, NULL, "void"};
Type chartype = &(struct type){TY_CHAR, 1, NULL, "char"};
Type shorttype = &(struct type){TY_SHRT, 2, NULL, "short"};
Type inttype = &(struct type){TY_INT, 4, NULL, "int"};
Type longtype = &(struct type){TY_LONG, 8, NULL, "long"};
Type uchartype = &(struct type){TY_UCHAR, 1, NULL, "unsigned char"};
Type ushorttype = &(struct type){TY_USHRT, 2, NULL, "unsigned short"};
Type uinttype = &(struct type){TY_UINT, 4, NULL, "unsigned int"};
Type ulongtype = &(struct type){TY_ULONG, 8, NULL, "unsigned long"};
Type voidptrtype = &(struct type){TY_POINTER, 8, NULL, "void *"};

#define TTSIZE 128
static struct type_entry {
  Type t;
  struct type_entry* next;
} * type_table[TTSIZE];

const char* type_str(Type t) {
  static char buffer[4096];
  if (t->kind <= 8)
    return t->str;

  if (t->kind == TY_POINTER) {
    sprintf(buffer, "ptr{%s}", t->base->str);
  } else if (t->kind == TY_ARRAY) {
    sprintf(buffer, "array[%d]{%s}", t->size / t->base->size, t->base->str);
  } else if (t->kind == TY_CONST) {
    sprintf(buffer, "const{%s}", t->base->str);
  } else if (t->kind == TY_VARARG) {
    sprintf(buffer, "...");
  } else if (t->kind == TY_FUNCTION) {
    int i = 0;
    Proto p = t->proto;
    i += sprintf(buffer, "function{%s}{", t->base->str);
    if (p) {
      i += sprintf(buffer + i, "%s %s", p->type->str, p->name ? p->name : "");
      p = p->next;
    }
    while (p) {
      i += sprintf(buffer + i, ",%s %s", p->type->str, p->name ? p->name : "");
      p = p->next;
    }
    sprintf(buffer + i, "}");
  } else if (t->kind == TY_STRUCT || t->kind == TY_UNION) {
    int i = 0;
    Member m = t->member;
    i += sprintf(buffer, "%s %s{", t->kind == TY_STRUCT ? "struct" : "union",
                 t->tag ? t->tag : "");
    while (m) {
      i += sprintf(buffer + i, "%s %s;", m->type->str, m->name);
      m = m->next;
    }
    sprintf(buffer + i, "}");
  } else
    error("what kind of type?");

  return string(buffer);
}

Type type(int kind, Type base, int size) {
  unsigned h;
  if (kind != TY_FUNCTION && kind != TY_STRUCT && kind != TY_UNION &&
      kind != TY_ENUM) {
    h = (kind ^ (unsigned long)base) % TTSIZE;
    for (struct type_entry* i = type_table[h]; i; i = i->next) {
      if (i->t->kind == kind && i->t->base == base && i->t->size == size)
        return i->t;
    }
  }

  Type t = calloc(1, sizeof(struct type));
  t->kind = kind;
  t->base = base;
  t->size = size;
  if (kind != TY_FUNCTION && kind != TY_STRUCT && kind != TY_UNION &&
      kind != TY_ENUM) {
    t->str = type_str(t);
    struct type_entry* e = calloc(1, sizeof(struct type_entry));
    e->t = t;
    e->next = type_table[h];
    type_table[h] = e;
  }

  return t;
}

Type ptr_type(Type base) {
  if (base == voidtype)
    return voidptrtype;
  return type(TY_POINTER, base, 8);
}

Type deref_type(Type ptr) {
  if (!is_ptr(ptr))
    error("can only dereference a pointer");
  if (unqual(ptr) == voidptrtype)
    error("dereferencing void * pointer");
  return unqual(ptr)->base;
}

Type array_type(Type base, int n) {
  if (!unqual(base)->size)
    error("array of incomplete type");
  return type(TY_ARRAY, base, n * unqual(base)->size);
}

Type array_to_ptr(Type a) {
  if (!is_array(a))
    error("not an array type");
  return ptr_type(a->base);
}

Type function_type(Type t, Proto p) {
  if (is_array(t) || is_funcion(t))
    error("function return what?");
  t = type(TY_FUNCTION, t, 0);
  t->proto = p;
  t->str = type_str(t);  // add the proto
  return t;
}

Type const_type(Type t) {
  if (is_const(t))
    return t;
  return type(TY_CONST, t, t->size);
}

Type unqual(Type t) {
  return is_qual(t) ? t->base : t;
}

Type struct_or_union_type(Member member, const char* tag, int kind) {
  Type ty = type(kind, NULL, 0);
  ty->tag = tag;
  update_struct_or_union_type(ty, member);
  return ty;
}

Member get_struct_or_union_member(Type t, const char* name) {
  for (Member m = unqual(t)->member; m; m = m->next) {
    if (m->name == name)
      return m;
  }
  return NULL;
}

void update_struct_or_union_type(Type ty, Member member) {
  ty->member = member;
  ty->size = 0;
  for (Member m = ty->member; m; m = m->next) {
    if (m->type->size == 0)
      error("not implemented");

    m->offset = is_struct(ty) ? ty->size : 0;
    ty->size =
        is_struct(ty) ? ty->size + m->type->size : max(ty->size, m->type->size);
  }
  ty->str = type_str(ty);
}

Type enum_type(const char* tag) {
  Type t = type(TY_ENUM, NULL, 4);
  t->tag = tag;
  return t;
}

int is_ptr(Type t) {
  return unqual(t)->kind == TY_POINTER || is_array(t);
}

int is_array(Type t) {
  return t->kind == TY_ARRAY;
}

int is_funcion(Type t) {
  return t->kind == TY_FUNCTION;
}

int is_signed(Type t) {
  return unqual(t) == chartype || unqual(t) == shorttype ||
         unqual(t) == inttype || unqual(t) == longtype || is_enum(t);
}

int is_unsigned(Type t) {
  return unqual(t) == uchartype || unqual(t) == ushorttype ||
         unqual(t) == uinttype || unqual(t) == ulongtype;
}

int is_integer(Type t) {
  return is_signed(t) || is_unsigned(t) || is_enum(t);
}

int is_arithmetic(Type t) {
  return is_integer(t);
}

int is_scalar(Type t) {
  return is_arithmetic(t) || is_ptr(t);
}

int is_qual(Type t) {
  return t->kind == TY_CONST;
}

int is_const(Type t) {
  return t->kind == TY_CONST;
}

int is_struct(Type t) {
  return unqual(t)->kind == TY_STRUCT;
}

int is_union(Type t) {
  return unqual(t)->kind == TY_UNION;
}

int is_struct_or_union(Type t) {
  return unqual(t)->kind == TY_STRUCT || unqual(t)->kind == TY_UNION;
}

int is_enum(Type t) {
  return unqual(t)->kind == TY_ENUM;
}

int is_struct_with_const_member(Type t) {
  if (!is_struct(t))
    return 0;
  for (Member m = unqual(t)->member; m; m = m->next) {
    if (is_struct_with_const_member(m->type))
      return 1;
  }
  return 0;
}

int is_compatible_type(Type t1, Type t2) {
  if (t1 == t2)
    return 1;

  if (t1->kind != t2->kind)
    return 0;

  // http://port70.net/~nsz/c/c99/n1256.html#6.7.3p9
  if (is_qual(t1))
    return (is_const(t1) ^ is_const(t2))
               ? 0
               : is_compatible_type(unqual(t1), unqual(t2));

  // http://port70.net/~nsz/c/c99/n1256.html#6.7.5.2p6
  if (is_array(t1)) {
    if (!is_compatible_type(t1->base, t2->base))
      return 0;
    if (t1->size || t2->size)
      return t1->size == t2->size;
    return 1;
  }

  // http://port70.net/~nsz/c/c99/n1256.html#6.7.5.1p2
  if (is_ptr(t1))
    return t1 == voidptrtype || t2 == voidptrtype ||
           is_compatible_type(t1->base, t2->base);

  // http://port70.net/~nsz/c/c99/n1256.html#6.7.5.3p15
  if (is_funcion(t1)) {
    if (!is_compatible_type(t1->base, t2->base))
      return 0;

    if (!t1->proto && !t2->proto)
      return 1;

    if (t1->proto && t2->proto) {
      Proto p1 = t1->proto, p2 = t2->proto;
      for (; p1 && p2; p1 = p1->next, p2 = p2->next) {
        if (!is_compatible_type(p1->type, p2->type))
          return 0;
      }
      return !p1 && !p2;
    }

    for (Proto p = t1->proto ? t1->proto : t2->proto; p; p = p->next) {
      // a parameter list with an ellipsis cannot match an empty parameter name
      // list declaration
      if (p->type->kind == TY_VARARG)
        return 0;
      // an argument type that has a default promotion cannot match an empty
      // parameter name list declaration
      if (!is_compatible_type(default_argument_promoe(p->type), p->type))
        return 0;
    }
    return 1;
  }

  if (is_struct_or_union(t1)) {
    if (t1->tag != t2->tag)
      return 0;
    Member m1 = t1->member, m2 = t2->member;
    for (; m1 && m2; m1 = m1->next, m2 = m2->next) {
      if (m1->name != m2->name)
        return 0;
      if (!is_compatible_type(m1->type, m2->type))
        return 0;
    }
    if (m1 || m2)
      return 0;
    return 1;
  }

  error("what kind of type?");
  return 0;
}

Type composite_type(Type t1, Type t2) {
  if (!is_compatible_type(t1, t2))
    error("compose what?");
  if (is_array(t1)) {
    if (t1->size)
      return array_type(composite_type(t1->base, t2->base),
                        t1->size / t1->base->size);
    return array_type(composite_type(t1->base, t2->base),
                      t2->size / t2->base->size);
  }

  if (is_funcion(t1)) {
    if (!t1->proto)
      return function_type(composite_type(t1->base, t2->base), t2->proto);
    if (!t2->proto)
      return function_type(composite_type(t1->base, t2->base), t1->proto);
    struct proto head = {0};
    Proto last = &head;
    for (Proto p1 = t1->proto, p2 = t2->proto; p1 && p2;
         p1 = p1->next, p2 = p2->next) {
      // a parameter list with an ellipsis cannot match an empty parameter name
      // list declaration
      last = last->next = calloc(1, sizeof(struct proto));
      last->name = p1->name;
      last->type = composite_type(p1->type, p2->type);
    }
    return function_type(composite_type(t1->base, t2->base), head.next);
  }

  return t1;
}

Type integral_promote(Type t) {
  t = unqual(t);

  if (!is_integer(t))
    return t;

  if (is_enum(t) || t->size < inttype->size)
    return inttype;

  return t;
}

Type usual_arithmetic_type(Type t1, Type t2) {
  t1 = integral_promote(unqual(t1));
  t2 = integral_promote(unqual(t2));
  if (t1 == t2)
    return t1;

  if (!(is_signed(t1) ^ is_signed(t2)))
    return t1->size > t2->size ? t1 : t2;

  if (is_unsigned(t1) && t1->size >= t2->size)
    return t1;
  if (is_unsigned(t2) && t2->size >= t1->size)
    return t2;

  if (is_signed(t1) && t1->size > t2->size)
    return t1;
  if (is_signed(t2) && t2->size > t1->size)
    return t2;

  error("usual arithemtic conversion failed");
  return NULL;
}

Type default_argument_promoe(Type t) {
  // http://port70.net/~nsz/c/c99/n1256.html#6.5.2.2p6
  return integral_promote(t);
}