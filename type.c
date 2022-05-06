#include "inc.h"
#include "stddef.h"

Type voidtype = &(struct type){TY_VOID, 0};
Type chartype = &(struct type){TY_CHAR, 1};
Type shorttype = &(struct type){TY_SHRT, 2};
Type inttype = &(struct type){TY_INT, 4};
Type longtype = &(struct type){TY_LONG, 8};
Type uchartype = &(struct type){TY_UCHAR, 1};
Type ushorttype = &(struct type){TY_USHRT, 2};
Type uinttype = &(struct type){TY_UINT, 4};
Type ulongtype = &(struct type){TY_ULONG, 8};

int is_signed(Type t) {
  return t == chartype || t == shorttype || t == inttype || t == longtype;
}

int is_unsigned(Type t) {
  return t == uchartype || t == ushorttype || t == uinttype || t == ulongtype;
}

Type integral_promote(Type t) {
  if (t->size < inttype->size)
    return inttype;
  return t;
}

Type usual_arithmetic_conversion(Type t1, Type t2) {
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