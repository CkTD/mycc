#include <stdlib.h>
#include "inc.h"

Type voidtype = &(struct type){TY_VOID, 0};
Type chartype = &(struct type){TY_CHAR, 1};
Type shorttype = &(struct type){TY_SHRT, 2};
Type inttype = &(struct type){TY_INT, 4};
Type longtype = &(struct type){TY_LONG, 8};
Type uchartype = &(struct type){TY_UCHAR, 1};
Type ushorttype = &(struct type){TY_USHRT, 2};
Type uinttype = &(struct type){TY_UINT, 4};
Type ulongtype = &(struct type){TY_ULONG, 8};

#define TTSIZE 128
static struct type_entry {
  Type t;
  struct type_entry* next;
} * type_table[TTSIZE];

Type type(int kind, Type base, int size) {
  unsigned h = (kind ^ (unsigned long)base) % TTSIZE;
  for (struct type_entry* i = type_table[h]; i; i = i->next) {
    if (i->t->kind == kind && i->t->base == base && i->t->size == size)
      return i->t;
  }

  struct type_entry* e = calloc(1, sizeof(struct type_entry));
  e->t = calloc(1, sizeof(struct type));
  e->t->kind = kind;
  e->t->base = base;
  e->t->size = size;
  e->next = type_table[h];
  type_table[h] = e;

  return e->t;
}

Type ptr_type(Type base) {
  return type(TY_POINTER, base, 8);
}

Type deref_type(Type ptr) {
  if (ptr->kind != TY_POINTER)
    error("can only dereference a pointer");
  return ptr->base;
}

Type array_type(Type base, int n) {
  if (!base->size)
    error("array of incomplete type");
  return type(TY_ARRAY, base, n * base->size);
}

int is_ptr(Type t) {
  return t->kind == TY_POINTER || is_array(t);
}

int is_array(Type t) {
  return t->kind == TY_ARRAY;
}

Type array_to_ptr(Type a) {
  if (!is_array(a))
    error("not an array type");
  return ptr_type(a->base);
}

int is_ptr_compatiable(Type a, Type b) {
  if (!is_ptr(a) || !is_ptr(b))
    error("not a pointer type");
  if (is_array(a))
    a = array_to_ptr(a);
  if (is_array(b))
    b = array_to_ptr(b);
  return a == b;
}

int is_signed(Type t) {
  return t == chartype || t == shorttype || t == inttype || t == longtype;
}

int is_unsigned(Type t) {
  return t == uchartype || t == ushorttype || t == uinttype || t == ulongtype;
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

Type integral_promote(Type t) {
  if (!is_integer(t))
    return t;

  if (t->size < inttype->size)
    return inttype;
  return t;
}

Type usual_arithmetic_type(Type t1, Type t2) {
  t1 = integral_promote(t1);
  t2 = integral_promote(t2);
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