/* Copyright 1999, 2004, 2005, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stdlib.h>
#include <string.h>

struct _simple_struct {
  int integer;
  unsigned int unsigned_integer;
  char character;
  signed char signed_character;
  char *char_ptr;
  int array_of_10[10];
};

typedef struct _simple_struct simpleton;

simpleton global_simple;

enum foo {
  bar = 1,
  baz
};

typedef enum foo efoo;

union named_union
{
  int integer;
  char *char_ptr;
};

typedef struct _struct_decl {
  int   integer;
  char  character;
  char *char_ptr;
  long  long_int;
  int  **int_ptr_ptr;
  long  long_array[12];

  void (*func_ptr) (void);
  struct _struct_decl (*func_ptr_struct) (int, char *, long);
  struct _struct_decl *(*func_ptr_ptr) (int, char *, long);
  union {
    int   a;
    char *b;
    long  c;
    enum foo d;
  } u1;

  struct {
    union {
      struct {
        int d;
        char e[10];
        int *(*func) (void);
        efoo foo;
      } u1s1;

      long f;
      struct {
        char array_ptr[2];
        int (*func) (int, char *);
      } u1s2;
    } u2;

    int g;
    char h;
    long i[10];
  } s2;
} weird_struct;

struct _struct_n_pointer {
  char ****char_ptr;
  long ****long_ptr;
  struct _struct_n_pointer *ptrs[3];
  struct _struct_n_pointer *next;
};

void do_locals_tests (void);
void do_block_tests (void);
void subroutine1 (int, long *);
void nothing (void);
void do_children_tests (void);
void do_special_tests (void);
void incr_a (char);

void incr_a (char a)
{
  int b;
  b = a;
}

void
do_locals_tests ()
{
  int linteger;
  int *lpinteger;
  char lcharacter;
  char *lpcharacter;
  long llong;
  long *lplong;
  float lfloat;
  float *lpfloat;
  double ldouble;
  double *lpdouble;
  struct _simple_struct lsimple;
  struct _simple_struct *lpsimple;
  void (*func) (void);

  /* Simple assignments */
  linteger = 1234;
  lpinteger = &linteger;
  lcharacter = 'a';
  lpcharacter = &lcharacter;
  llong = 2121L;
  lplong = &llong;
  lfloat = 2.1;
  lpfloat = &lfloat;
  ldouble = 2.718281828459045;
  lpdouble = &ldouble;
  lsimple.integer = 1234;
  lsimple.unsigned_integer = 255;
  lsimple.character = 'a';
  lsimple.signed_character = 21;
  lsimple.char_ptr = &lcharacter;
  lpsimple = &lsimple;
  func = nothing;

  /* Check pointers */
  linteger = 4321;
  lcharacter = 'b';
  llong = 1212L;
  lfloat = 1.2;
  ldouble = 5.498548281828172;
  lsimple.integer = 255;
  lsimple.unsigned_integer = 4321;
  lsimple.character = 'b';
  lsimple.signed_character = 0;

  subroutine1 (linteger, &llong);
}

void
nothing ()
{
}

void
subroutine1 (int i, long *l)
{
  global_simple.integer = i + 3;
  i = 212;
  *l = 12;
}

void
do_block_tests ()
{
  int cb = 12;

  {
    int foo;
    foo = 123;
    {
      int foo2;
      foo2 = 123;
      {
        int foo;
        foo = 321;
      }
      foo2 = 0;
    }
    foo = 0;
  }

  cb = 21;
}

void
do_children_tests (void)
{
  weird_struct *weird;
  struct _struct_n_pointer *psnp;
  struct _struct_n_pointer snp0, snp1, snp2;
  char a0[2] = {}, *a1, **a2, ***a3;
  char b0[2] = {}, *b1, **b2, ***b3;
  char c0[2] = {}, *c1, **c2, ***c3;
  long z0, *z1, **z2, ***z3;
  long y0, *y1, **y2, ***y3;
  long x0, *x1, **x2, ***x3;
  int *foo;
  int bar;

  struct _struct_decl struct_declarations;
  memset (&struct_declarations, 0, sizeof (struct_declarations));
  weird = &struct_declarations;

  struct_declarations.integer = 123;
  weird->char_ptr = "hello";
  bar = 2121;
  foo = &bar;
  struct_declarations.int_ptr_ptr = &foo;
  weird->long_array[0] = 1234;
  struct_declarations.long_array[1] = 2345;
  weird->long_array[2] = 3456;
  struct_declarations.long_array[3] = 4567;
  weird->long_array[4] = 5678;
  struct_declarations.long_array[5] = 6789;
  weird->long_array[6] = 7890;
  struct_declarations.long_array[7] = 8901;
  weird->long_array[8] = 9012;
  struct_declarations.long_array[9] = 1234;

  weird->func_ptr = nothing;
  struct_declarations.long_array[10] = 3456;
  struct_declarations.long_array[11] = 5678;

  /* Struct/pointer/array tests */
  a0[0] = '0';  
  a1 = a0;
  a2 = &a1;
  a3 = &a2;
  b0[0] = '1';
  b1 = b0;
  b2 = &b1;
  b3 = &b2;
  c0[1] = '2';
  c1 = c0;
  c2 = &c1;
  c3 = &c2;
  z0 = 0xdead + 0;
  z1 = &z0;
  z2 = &z1;
  z3 = &z2;
  y0 = 0xdead + 1;
  y1 = &y0;
  y2 = &y1;
  y3 = &y2;
  x0 = 0xdead + 2;
  x1 = &x0;
  x2 = &x1;
  x3 = &x2;
  snp0.char_ptr = &a3;
  snp0.long_ptr = &z3;
  snp0.ptrs[0] = &snp0;
  snp0.ptrs[1] = &snp1;
  snp0.ptrs[2] = &snp2;
  snp0.next = &snp1;
  snp1.char_ptr = &b3;
  snp1.long_ptr = &y3;
  snp1.ptrs[0] = &snp0;
  snp1.ptrs[1] = &snp1;
  snp1.ptrs[2] = &snp2;
  snp1.next = &snp2;
  snp2.char_ptr = &c3;
  snp2.long_ptr = &x3;
  snp2.ptrs[0] = &snp0;
  snp2.ptrs[1] = &snp1;
  snp2.ptrs[2] = &snp2;
  snp2.next = 0x0;
  psnp = &snp0;
  snp0.char_ptr = &b3;
  snp1.char_ptr = &c3;
  snp2.char_ptr = &a3;
  snp0.long_ptr = &y3;
  snp1.long_ptr = &x3;
  snp2.long_ptr = &z3;
}

void
do_special_tests (void)
{
  union named_union u;
  union {
    int a;
    char b;
    long c;
  } anonu;
  struct _simple_struct s;
  struct {
    int a;
    char b;
    long c;
  } anons;
  enum foo e;
  enum { A, B, C } anone;
  int array[21];
  int a;

  a = 1;   
  incr_a(2);
}

int
main (int argc, char *argv [])
{
  do_locals_tests ();
  do_block_tests ();
  do_children_tests ();
  do_special_tests ();
  exit (0);
}

  
