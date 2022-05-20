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

#define TTSIZE 128
static struct type_entry {
  Type t;
  struct type_entry* next;
} * type_table[TTSIZE];

static const char* type_name(Type t) {
  static char buffer[4096];
  if (t->kind <= 8)
    return t->name;

  if (t->kind == TY_POINTER) {
    sprintf(buffer, "ptr{%s}", t->base->name);
  } else if (t->kind == TY_ARRAY) {
    sprintf(buffer, "array[%d]{%s}", t->size / t->base->size, t->base->name);
  } else if (t->kind == TY_CONST) {
    sprintf(buffer, "const{%s}", t->base->name);
  } else if (t->kind == TY_VARARG) {
    sprintf(buffer, "...");
  } else if (t->kind == TY_FUNCTION) {
    int i = 0;
    Proto p = t->proto;
    i += sprintf(buffer, "function{%s}{", t->base->name);
    if (p) {
      i += sprintf(buffer + i, "%s %s", p->type->name, p->name ? p->name : "");
      p = p->next;
    }
    while (p) {
      i += sprintf(buffer + i, ",%s %s", p->type->name, p->name ? p->name : "");
      p = p->next;
    }
    sprintf(buffer + i, "}");
  } else
    error("what kind of type?");

  return string(buffer);
}

Type type(int kind, Type base, int size) {
  unsigned h;
  if (kind != TY_FUNCTION) {
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
  t->name = type_name(t);

  if (kind != TY_FUNCTION) {
    struct type_entry* e = calloc(1, sizeof(struct type_entry));
    e->t = t;
    e->next = type_table[h];
    type_table[h] = e;
  }

  return t;
}

Type ptr_type(Type base) {
  return type(TY_POINTER, base, 8);
}

Type deref_type(Type ptr) {
  if (!is_ptr(ptr))
    error("can only dereference a pointer");
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
  t->name = type_name(t);  // add the proto
  return t;
}

Type const_type(Type t) {
  return type(TY_CONST, t, 0);
}

Type unqual(Type t) {
  return is_qual(t) ? t->base : t;
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
         unqual(t) == inttype || unqual(t) == longtype;
}

int is_unsigned(Type t) {
  return unqual(t) == uchartype || unqual(t) == ushorttype ||
         unqual(t) == uinttype || unqual(t) == ulongtype;
}

int is_integer(Type t) {
  return is_signed(t) || is_unsigned(t);
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

int is_ptr_compatiable(Type a, Type b) {
  if (!is_ptr(a) || !is_ptr(b))
    error("not a pointer type");
  if (is_array(a))
    a = array_to_ptr(a);
  if (is_array(b))
    b = array_to_ptr(b);
  return unqual(a) == unqual(b);
}

Type integral_promote(Type t) {
  t = unqual(t);

  if (!is_integer(t))
    return t;

  if (t->size < inttype->size)
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