/* This testcase is part of GDB, the GNU debugger.

   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2007
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   */

// Test various -*- C++ -*- things.

// ====================== basic C++ types  =======================
bool            v_bool;
bool            v_bool_array[2];

typedef struct fleep fleep;
struct fleep { int a; } s;

// ====================== simple class structures  =======================

struct default_public_struct {
 // defaults to public:
  int a;
  int b;
};

struct explicit_public_struct {
 public:
  int a;
  int b;
};

struct protected_struct {
 protected:
  int a;
  int b;
};

struct private_struct {
 private:
  int a;
  int b;
};

struct mixed_protection_struct {
 public:
  int a;
  int b;
 private:
  int c;
  int d;
 protected:
  int e;
  int f;
 public:
  int g;
 private:
  int h;
 protected:
  int i;
};

class public_class {
 public:
  int a;
  int b;
};

class protected_class {
 protected:
  int a;
  int b;
};

class default_private_class {
 // defaults to private:
  int a;
  int b;
};

class explicit_private_class {
 private:
  int a;
  int b;
};

class mixed_protection_class {
 public:
  int a;
  int b;
 private:
  int c;
  int d;
 protected:
  int e;
  int f;
 public:
  int g;
 private:
  int h;
 protected:
  int i;
};

class const_vol_method_class {
public:
  int a;
  int b;
  int foo (int &) const;
  int bar (int &) volatile;
  int baz (int &) const volatile;
};

int const_vol_method_class::foo (int & ir) const
{
  return ir + 3;
}
int const_vol_method_class::bar (int & ir) volatile
{
  return ir + 4;
}
int const_vol_method_class::baz (int & ir) const volatile
{
  return ir + 5;
}

// ========================= simple inheritance ==========================

class A {
 public:
  int a;
  int x;
};

A g_A;

class B : public A {
 public:
  int b;
  int x;
};

B g_B;

class C : public A {
 public:
  int c;
  int x;
};

C g_C;

class D : public B, public C {
 public:
  int d;
  int x;
};

D g_D;

class E : public D {
 public:
  int e;
  int x;
};

E g_E;

class class_with_anon_union
{
 public:
  int one;
  union
  {
    int a;
    long b;
  };
};

class_with_anon_union g_anon_union;

void inheritance2 (void)
{
}

void inheritance1 (void)
{
  int ival;
  int *intp;

  // {A::a, A::x}

  g_A.A::a = 1;
  g_A.A::x = 2;

  // {{A::a,A::x},B::b,B::x}

  g_B.A::a = 3;
  g_B.A::x = 4;
  g_B.B::b = 5;
  g_B.B::x = 6;

  // {{A::a,A::x},C::c,C::x}

  g_C.A::a = 7;
  g_C.A::x = 8;
  g_C.C::c = 9;
  g_C.C::x = 10;

  // {{{A::a,A::x},B::b,B::x},{{A::a,A::x},C::c,C::x},D::d,D::x}

  // The following initialization code is non-portable, but allows us
  // to initialize all members of g_D until we can fill in the missing
  // initialization code with legal C++ code.

  for (intp = (int *) &g_D, ival = 11;
       intp < ((int *) &g_D + sizeof (g_D) / sizeof (int));
       intp++, ival++)
    {
      *intp = ival;
    }

  // Overlay the nonportable initialization with legal initialization.

  // ????? = 11;  (g_D.A::a = 11; is ambiguous)
  // ????? = 12;  (g_D.A::x = 12; is ambiguous)
/* djb 6-3-2000

	This should take care of it. Rather than try to initialize using an ambiguous
	construct, use 2 unambiguous ones for each. Since the ambiguous a/x member is
	coming from C, and B, initialize D's C::a, and B::a, and D's C::x and B::x.
 */
  g_D.C::a = 15;
  g_D.C::x = 12;
  g_D.B::a = 11;
  g_D.B::x = 12;
  g_D.B::b = 13;
  g_D.B::x = 14;
  // ????? = 15;
  // ????? = 16;
  g_D.C::c = 17;
  g_D.C::x = 18;
  g_D.D::d = 19;
  g_D.D::x = 20;


  // {{{{A::a,A::x},B::b,B::x},{{A::a,A::x},C::c,C::x},D::d,D::x}},E::e,E::x}

  // The following initialization code is non-portable, but allows us
  // to initialize all members of g_D until we can fill in the missing
  // initialization code with legal C++ code.

  for (intp = (int *) &g_E, ival = 21;
       intp < ((int *) &g_E + sizeof (g_E) / sizeof (int));
       intp++, ival++)
  {
    *intp = ival;
  }

  // Overlay the nonportable initialization with legal initialization.

  // ????? = 21;  (g_E.A::a = 21; is ambiguous)
  // ????? = 22;  (g_E.A::x = 22; is ambiguous)
  g_E.B::b = 23;
  g_E.B::x = 24;
  // ????? = 25;
  // ????? = 26;
  g_E.C::c = 27;
  g_E.C::x = 28;
  g_E.D::d = 29;
  g_E.D::x = 30;
  g_E.E::e = 31;
  g_E.E::x = 32;

  g_anon_union.one = 1;
  g_anon_union.a = 2;

  inheritance2 ();	
}

// ======================== static member functions =====================

class Static {
public:
  static void ii(int, int);
};
void Static::ii (int, int) { }

// ======================== virtual base classes=========================

class vA {
 public:
  int va;
  int vx;
};

vA g_vA;

class vB : public virtual vA {
 public:
  int vb;
  int vx;
};

vB g_vB;

class vC : public virtual vA {
 public:
  int vc;
  int vx;
};

vC g_vC;

class vD : public virtual vB, public virtual vC {
 public:
  int vd;
  int vx;
};

vD g_vD;

class vE : public virtual vD {
 public:
  int ve;
  int vx;
};

vE g_vE;

void inheritance4 (void)
{
}

void inheritance3 (void)
{
  int ival;
  int *intp;

  // {vA::va, vA::vx}

  g_vA.vA::va = 1;
  g_vA.vA::vx = 2;

  // {{vA::va, vA::vx}, vB::vb, vB::vx}

  g_vB.vA::va = 3;
  g_vB.vA::vx = 4;
  g_vB.vB::vb = 5;
  g_vB.vB::vx = 6;

  // {{vA::va, vA::vx}, vC::vc, vC::vx}

  g_vC.vA::va = 7;
  g_vC.vA::vx = 8;
  g_vC.vC::vc = 9;
  g_vC.vC::vx = 10;

  // {{{{vA::va, vA::vx}, vB::vb, vB::vx}, vC::vc, vC::vx}, vD::vd,vD::vx}

  g_vD.vA::va = 11;
  g_vD.vA::vx = 12;
  g_vD.vB::vb = 13;
  g_vD.vB::vx = 14;
  g_vD.vC::vc = 15;
  g_vD.vC::vx = 16;
  g_vD.vD::vd = 17;
  g_vD.vD::vx = 18;


  // {{{{{vA::va,vA::vx},vB::vb,vB::vx},vC::vc,vC::vx},vD::vd,vD::vx},vE::ve,vE::vx}

  g_vD.vA::va = 19;
  g_vD.vA::vx = 20;
  g_vD.vB::vb = 21;
  g_vD.vB::vx = 22;
  g_vD.vC::vc = 23;
  g_vD.vC::vx = 24;
  g_vD.vD::vd = 25;
  g_vD.vD::vx = 26;
  g_vE.vE::ve = 27;
  g_vE.vE::vx = 28;

  inheritance4 ();	
}

// ======================================================================

class Base1 {
 public:
  int x;
  Base1(int i) { x = i; }
};

class Foo
{
 public:
  int x;
  int y;
  static int st;
  Foo (int i, int j) { x = i; y = j; }
  int operator! ();
  operator int ();
  int times (int y);
};

class Bar : public Base1, public Foo {
 public:
  int z;
  Bar (int i, int j, int k) : Base1 (10*k), Foo (i, j) { z = k; }
};

int Foo::operator! () { return !x; }

int Foo::times (int y) { return x * y; }

int Foo::st = 100;

Foo::operator int() { return x; }

Foo foo(10, 11);
Bar bar(20, 21, 22);

class ClassWithEnum {
public:
  enum PrivEnum { red, green, blue, yellow = 42 };
  PrivEnum priv_enum;
  int x;
};

void enums2 (void)
{
}

/* classes.exp relies on statement order in this function for testing
   enumeration fields.  */

void enums1 ()
{
  ClassWithEnum obj_with_enum;
  obj_with_enum.priv_enum = ClassWithEnum::red;
  obj_with_enum.x = 0;
  enums2 ();
  obj_with_enum.priv_enum = ClassWithEnum::green;
}

class ClassParam {
public:
  int Aptr_a (A *a) { return a->a; }
  int Aptr_x (A *a) { return a->x; }
  int Aref_a (A &a) { return a.a; }
  int Aref_x (A &a) { return a.x; }
  int Aval_a (A a) { return a.a; }
  int Aval_x (A a) { return a.x; }
};

ClassParam class_param;

class Contains_static_instance
{
 public:
  int x;
  int y;
  Contains_static_instance (int i, int j) { x = i; y = j; }
  static Contains_static_instance null;
};

Contains_static_instance Contains_static_instance::null(0,0);
Contains_static_instance csi(10,20);

class Contains_nested_static_instance
{
 public:
  class Nested
  {
   public:
    Nested(int i) : z(i) {}
    int z;
    static Contains_nested_static_instance xx;
  };

  Contains_nested_static_instance(int i, int j) : x(i), y(j) {}

  int x;
  int y;

  static Contains_nested_static_instance null;
  static Nested yy;
};

Contains_nested_static_instance Contains_nested_static_instance::null(0, 0);
Contains_nested_static_instance::Nested Contains_nested_static_instance::yy(5);
Contains_nested_static_instance
  Contains_nested_static_instance::Nested::xx(1,2);
Contains_nested_static_instance cnsi(30,40);

typedef struct {
  int one;
  int two;
} tagless_struct;
tagless_struct v_tagless;

/* Try to get the compiler to allocate a class in a register.  */
class small {
 public:
  int x;
  int method ();
};

int
small::method ()
{
  return x + 5;
}

void marker_reg1 () {}

int
register_class ()
{
  /* We don't call any methods for v, so gcc version cygnus-2.3.3-930220
     might put this variable in a register.  This is a lose, though, because
     it means that GDB can't call any methods for that variable.  */
  register small v;

  int i;

  /* Perform a computation sufficiently complicated that optimizing compilers
     won't optimized out the variable.  If some compiler constant-folds this
     whole loop, maybe using a parameter to this function here would help.  */
  v.x = 0;
  for (i = 0; i < 13; ++i)
    v.x += i;
  --v.x; /* v.x is now 77 */
  marker_reg1 ();
  return v.x + 5;
}

void dummy()
{
  v_bool = true;
  v_bool_array[0] = false;
  v_bool_array[1] = v_bool;
}

void use_methods ()
{
  /* Refer to methods so that they don't get optimized away. */
  int i;
  i = class_param.Aptr_a (&g_A);
  i = class_param.Aptr_x (&g_A);
  i = class_param.Aref_a (g_A);
  i = class_param.Aref_x (g_A);
  i = class_param.Aval_a (g_A);
  i = class_param.Aval_x (g_A);
}


int
main()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  dummy();
  inheritance1 ();
  inheritance3 ();
  enums1 ();
  register_class ();

  /* FIXME: pmi gets optimized out.  Need to do some more computation with
     it or something.  (No one notices, because the test is xfail'd anyway,
     but that probably won't always be true...).  */
  int Foo::* pmi = &Foo::y;

  /* Make sure the AIX linker doesn't remove the variable.  */
  v_tagless.one = 5;

  use_methods ();

  return foo.*pmi;
}

/* Create an instance for some classes, otherwise they get optimized away.  */

default_public_struct default_public_s;
explicit_public_struct explicit_public_s;
protected_struct protected_s;
private_struct private_s;
mixed_protection_struct mixed_protection_s;
public_class public_c;
protected_class protected_c;
default_private_class default_private_c;
explicit_private_class explicit_private_c;
mixed_protection_class mixed_protection_c;
