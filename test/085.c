
int main() {
  struct s {
    int a;
  };

  // redefine symbol
  // int a;
  // int a;

  // invalid operand &
  // int a = &12;

  // invalid operatnd *
  // int a;
  // int b = *a;

  // integer constant too large
  // int a = 111111111111111111111;

  // integer constant too large
  // int a = 18446744073709551615;

  // integer suffix
  // int a = 1844674407370lul;

  // cast type

  // struct s s = (struct s)32;

  // unary +/-
  // int* p;
  // +p;

  // unary ~
  // int* p;
  // ~p;

  // unary !
  // struct s s;
  // !s;

  // postfix ++/--
  // 32 ++;
  // struct s s;
  // s++;

  // =
  // // 3 = 2;

  // =
  // const int a;
  // a = 32;

  // =
  // struct s s;
  // s = 12;

  // &&
  // struct s s;
  // s && 1;

  // bitwise | & ^
  // 3 ^ (int*)0;

  // comparation
  // 3 > (int*)2;

  // shift
  // 3 >> (int*)2;

  // binary +
  // (int*)3 + (int*)2;

  // binary -
  // (int*)3 - (char*)2;

  // binary * / %
  // (int*)3 / 2;

  // ?:
  // 1 ? (int*)1 : (char*)1;

  return 0;
}

// conflict function declaration
// int func(int*);
// int func(void);

// redeclaration enum
// enum e { A = 1 };
// enum e { B = 2 };

// redeclaration struct / union
// union u;
// union u {
//   int i;
// };
// union u {
//   int k;
// };

// conflict tag
// union x;
// struct x;

// declare nothing
// int;

// declaration sepcifier none
// const const i;

// declaration sepcifier dup
// int int i;

// empty struct/union member name
// struct s {
//   int;
// };

// empty struct/union
// struct s {};

// declarator name?
// int f, ;

// duplicate member name
// struct s {
//   int a;
//   int a;
// };

// non constant initializer
// int y;
// int x = y;

// array of size 0
// int a[0];

// function paramenter: declaration void
// void f(int, void);

// function paramenter: redeclaration
// void f(int a, int a);

// function paramenter: ...
// void f(int a, ..., int b);

// function redefinition
// void f() {}
// void f() {
//   int a;
// }

// type name
// void f(){sizeof(int a)};

// function paramenter name
// void f(int) {}

// unexpected token
// void f() {
//   int a;
//   a < ;
// }

// function call: too many args
// int f(int a) {
//   return f(1, 2 + 3 + 3);
// }

// function call: too few args
// int f(int a, int b, ...) {
//   return f(1);
// }

// function call: discard const (***)
// int f(int* a) {
//   const int* b;
//   return f(b);
// }

// function call: incompatible arg type (***)
// int f(int* i) {
//   return f(1 + 1);
// }

// undefined array
// int f() {
//   return a[1];
// }

// undefined aggregrate
// int f() {
//   return x.y;
// }

// subscript bad value
// int f() {
//   int a;
//   return a[1];
// }

// ->  (***)
// int f() {
//   int a;
//   return a->x;
// }

// member not exist
// int f() {
//   struct s {
//     int a;
//   } s;
//   return s.b;
// }

// undefined var/enumconst
// int f() {
//   return xxx;
// }