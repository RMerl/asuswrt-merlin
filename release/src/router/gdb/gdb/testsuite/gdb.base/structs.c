/* This testcase is part of GDB, the GNU debugger.

   Copyright 1996, 1999, 2003, 2007 Free Software Foundation, Inc.

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

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

/* Useful abreviations.  */
typedef void t;
typedef char tc;
typedef short ts;
typedef int ti;
typedef long tl;
typedef long long tll;
typedef float tf;
typedef double td;
typedef long double tld;

/* Force the type of each field.  */
#ifndef tA
typedef t tA;
#endif
#ifndef tB
typedef tA tB;
#endif
#ifndef tC
typedef tB tC;
#endif
#ifndef tD
typedef tC tD;
#endif
#ifndef tE
typedef tD tE;
#endif
#ifndef tF
typedef tE tF;
#endif
#ifndef tG
typedef tF tG;
#endif
#ifndef tH
typedef tG tH;
#endif
#ifndef tI
typedef tH tI;
#endif
#ifndef tJ
typedef tI tJ;
#endif
#ifndef tK
typedef tJ tK;
#endif
#ifndef tL
typedef tK tL;
#endif
#ifndef tM
typedef tL tM;
#endif
#ifndef tN
typedef tM tN;
#endif
#ifndef tO
typedef tN tO;
#endif
#ifndef tP
typedef tO tP;
#endif
#ifndef tQ
typedef tP tQ;
#endif
#ifndef tR
typedef tQ tR;
#endif

struct  struct1 {tA a;};
struct  struct2 {tA a; tB b;};
struct  struct3 {tA a; tB b; tC c; };
struct  struct4 {tA a; tB b; tC c; tD d; };
struct  struct5 {tA a; tB b; tC c; tD d; tE e; };
struct  struct6 {tA a; tB b; tC c; tD d; tE e; tF f; };
struct  struct7 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; };
struct  struct8 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; };
struct  struct9 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; };
struct struct10 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; };
struct struct11 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; };
struct struct12 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; };
struct struct13 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; tM m; };
struct struct14 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; tM m; tN n; };
struct struct15 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; tM m; tN n; tO o; };
struct struct16 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; tM m; tN n; tO o; tP p; };
struct struct17 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; tM m; tN n; tO o; tP p; tQ q; };
struct struct18 {tA a; tB b; tC c; tD d; tE e; tF f; tG g; tH h; tI i; tJ j; tK k; tL l; tM m; tN n; tO o; tP p; tQ q; tR r; };

struct  struct1  foo1 = {'1'}, L1;
struct  struct2  foo2 = {'a','2'}, L2;
struct  struct3  foo3 = {'1','b','3'}, L3;
struct  struct4  foo4 = {'a','2','c','4'}, L4;
struct  struct5  foo5 = {'1','b','3','d','5'}, L5;
struct  struct6  foo6 = {'a','2','c','4','e','6'}, L6;
struct  struct7  foo7 = {'1','b','3','d','5','f','7'}, L7;
struct  struct8  foo8 = {'a','2','c','4','e','6','g','8'}, L8;
struct  struct9  foo9 = {'1','b','3','d','5','f','7','h','9'}, L9;
struct struct10 foo10 = {'a','2','c','4','e','6','g','8','i','A'}, L10;
struct struct11 foo11 = {'1','b','3','d','5','f','7','h','9','j','B'}, L11;
struct struct12 foo12 = {'a','2','c','4','e','6','g','8','i','A','k','C'}, L12;
struct struct13 foo13 = {'1','b','3','d','5','f','7','h','9','j','B','l','D'}, L13;
struct struct14 foo14 = {'a','2','c','4','e','6','g','8','i','A','k','C','m','E'}, L14;
struct struct15 foo15 = {'1','b','3','d','5','f','7','h','9','j','B','l','D','n','F'}, L15;
struct struct16 foo16 = {'a','2','c','4','e','6','g','8','i','A','k','C','m','E','o','G'}, L16;
struct struct17 foo17 = {'1','b','3','d','5','f','7','h','9','j','B','l','D','n','F','p','H'}, L17;
struct struct18 foo18 = {'a','2','c','4','e','6','g','8','i','A','k','C','m','E','o','G','q','I'}, L18;

struct struct1  fun1()
{
  return foo1;  
}
struct struct2  fun2()
{
  return foo2;
}
struct struct3  fun3()
{
  return foo3;
}
struct struct4  fun4()
{
  return foo4;
}
struct struct5  fun5()
{
  return foo5;
}
struct struct6  fun6()
{
  return foo6;
}
struct struct7  fun7()
{
  return foo7;
}
struct struct8  fun8()
{
  return foo8;
}
struct struct9  fun9()
{
  return foo9;
}
struct struct10 fun10()
{
  return foo10; 
}
struct struct11 fun11()
{
  return foo11; 
}
struct struct12 fun12()
{
  return foo12; 
}
struct struct13 fun13()
{
  return foo13; 
}
struct struct14 fun14()
{
  return foo14; 
}
struct struct15 fun15()
{
  return foo15; 
}
struct struct16 fun16()
{
  return foo16; 
}
struct struct17 fun17()
{
  return foo17; 
}
struct struct18 fun18()
{
  return foo18; 
}

#ifdef PROTOTYPES
void Fun1(struct struct1 foo1)
#else
void Fun1(foo1)
     struct struct1 foo1;
#endif
{
  L1 = foo1;
}
#ifdef PROTOTYPES
void Fun2(struct struct2 foo2)
#else
void Fun2(foo2)
     struct struct2 foo2;
#endif
{
  L2 = foo2;
}
#ifdef PROTOTYPES
void Fun3(struct struct3 foo3)
#else
void Fun3(foo3)
     struct struct3 foo3;
#endif
{
  L3 = foo3;
}
#ifdef PROTOTYPES
void Fun4(struct struct4 foo4)
#else
void Fun4(foo4)
     struct struct4 foo4;
#endif
{
  L4 = foo4;
}
#ifdef PROTOTYPES
void Fun5(struct struct5 foo5)
#else
void Fun5(foo5)
     struct struct5 foo5;
#endif
{
  L5 = foo5;
}
#ifdef PROTOTYPES
void Fun6(struct struct6 foo6)
#else
void Fun6(foo6)
     struct struct6 foo6;
#endif
{
  L6 = foo6;
}
#ifdef PROTOTYPES
void Fun7(struct struct7 foo7)
#else
void Fun7(foo7)
     struct struct7 foo7;
#endif
{
  L7 = foo7;
}
#ifdef PROTOTYPES
void Fun8(struct struct8 foo8)
#else
void Fun8(foo8)
     struct struct8 foo8;
#endif
{
  L8 = foo8;
}
#ifdef PROTOTYPES
void Fun9(struct struct9 foo9)
#else
void Fun9(foo9)
     struct struct9 foo9;
#endif
{
  L9 = foo9;
}
#ifdef PROTOTYPES
void Fun10(struct struct10 foo10)
#else
void Fun10(foo10)
     struct struct10 foo10;
#endif
{
  L10 = foo10; 
}
#ifdef PROTOTYPES
void Fun11(struct struct11 foo11)
#else
void Fun11(foo11)
     struct struct11 foo11;
#endif
{
  L11 = foo11; 
}
#ifdef PROTOTYPES
void Fun12(struct struct12 foo12)
#else
void Fun12(foo12)
     struct struct12 foo12;
#endif
{
  L12 = foo12; 
}
#ifdef PROTOTYPES
void Fun13(struct struct13 foo13)
#else
void Fun13(foo13)
     struct struct13 foo13;
#endif
{
  L13 = foo13; 
}
#ifdef PROTOTYPES
void Fun14(struct struct14 foo14)
#else
void Fun14(foo14)
     struct struct14 foo14;
#endif
{
  L14 = foo14; 
}
#ifdef PROTOTYPES
void Fun15(struct struct15 foo15)
#else
void Fun15(foo15)
     struct struct15 foo15;
#endif
{
  L15 = foo15; 
}
#ifdef PROTOTYPES
void Fun16(struct struct16 foo16)
#else
void Fun16(foo16)
     struct struct16 foo16;
#endif
{
  L16 = foo16; 
}
#ifdef PROTOTYPES
void Fun17(struct struct17 foo17)
#else
void Fun17(foo17)
     struct struct17 foo17;
#endif
{
  L17 = foo17; 
}
#ifdef PROTOTYPES
void Fun18(struct struct18 foo18)
#else
void Fun18(foo18)
     struct struct18 foo18;
#endif
{
  L18 = foo18; 
}

zed ()
{

  L1.a = L2.a = L3.a = L4.a = L5.a = L6.a = L7.a = L8.a = L9.a = L10.a = L11.a = L12.a = L13.a = L14.a = L15.a = L16.a = L17.a = L18.a = 'Z';

  L2.b = L3.b = L4.b = L5.b = L6.b = L7.b = L8.b = L9.b = L10.b = L11.b = L12.b = L13.b = L14.b = L15.b = L16.b = L17.b = L18.b = 'Z';

  L3.c = L4.c = L5.c = L6.c = L7.c = L8.c = L9.c = L10.c = L11.c = L12.c = L13.c = L14.c = L15.c = L16.c = L17.c = L18.c = 'Z';

  L4.d = L5.d = L6.d = L7.d = L8.d = L9.d = L10.d = L11.d = L12.d = L13.d = L14.d = L15.d = L16.d = L17.d = L18.d = 'Z';

  L5.e = L6.e = L7.e = L8.e = L9.e = L10.e = L11.e = L12.e = L13.e = L14.e = L15.e = L16.e = L17.e = L18.e = 'Z';

  L6.f = L7.f = L8.f = L9.f = L10.f = L11.f = L12.f = L13.f = L14.f = L15.f = L16.f = L17.f = L18.f = 'Z';

  L7.g = L8.g = L9.g = L10.g = L11.g = L12.g = L13.g = L14.g = L15.g = L16.g = L17.g = L18.g = 'Z';

  L8.h = L9.h = L10.h = L11.h = L12.h = L13.h = L14.h = L15.h = L16.h = L17.h = L18.h = 'Z';

  L9.i = L10.i = L11.i = L12.i = L13.i = L14.i = L15.i = L16.i = L17.i = L18.i = 'Z';

  L10.j = L11.j = L12.j = L13.j = L14.j = L15.j = L16.j = L17.j = L18.j = 'Z';

  L11.k = L12.k = L13.k = L14.k = L15.k = L16.k = L17.k = L18.k = 'Z';

  L12.l = L13.l = L14.l = L15.l = L16.l = L17.l = L18.l = 'Z';

  L13.m = L14.m = L15.m = L16.m = L17.m = L18.m = 'Z';

  L14.n = L15.n = L16.n = L17.n = L18.n = 'Z';

  L15.o = L16.o = L17.o = L18.o = 'Z';

  L16.p = L17.p = L18.p = 'Z';

  L17.q = L18.q = 'Z';

  L18.r = 'Z';
}

int main()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  int i;

  Fun1(foo1);	
  Fun2(foo2);	
  Fun3(foo3);	
  Fun4(foo4);	
  Fun5(foo5);	
  Fun6(foo6);	
  Fun7(foo7);	
  Fun8(foo8);	
  Fun9(foo9);	
  Fun10(foo10);
  Fun11(foo11);
  Fun12(foo12);
  Fun13(foo13);
  Fun14(foo14);
  Fun15(foo15);
  Fun16(foo16);
  Fun17(foo17);
  Fun18(foo18);

  /* An infinite loop that first clears all the variables and then
     calls each function.  This "hack" is to make testing random
     functions easier - "advance funN" is guaranteed to have always
     been preceeded by a global variable clearing zed call.  */

  while (1)
    {
      zed ();
      L1  = fun1();	
      L2  = fun2();	
      L3  = fun3();	
      L4  = fun4();	
      L5  = fun5();	
      L6  = fun6();	
      L7  = fun7();	
      L8  = fun8();	
      L9  = fun9();	
      L10 = fun10();
      L11 = fun11();
      L12 = fun12();
      L13 = fun13();
      L14 = fun14();
      L15 = fun15();
      L16 = fun16();
      L17 = fun17();
      L18 = fun18();
    }

  return 0;
}
