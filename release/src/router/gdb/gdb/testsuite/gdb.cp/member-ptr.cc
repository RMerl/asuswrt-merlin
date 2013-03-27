/* This testcase is part of GDB, the GNU debugger.

   Copyright 1998, 1999, 2004, 2006, 2007 Free Software Foundation, Inc.

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



extern "C" {
#include <stdio.h>
}


class A {
public:
  A();
  int foo (int x);
  int bar (int y);
  virtual int baz (int z);
  char c;
  int  j;
  int  jj;
  static int s;
};

class B {
public:
  static int s;
};

int A::s = 10;
int B::s = 20;

A::A()
{
  c = 'x';
  j = 5;
}

int A::foo (int dummy)
{
  j += 3;
  return j + dummy;
}

int A::bar (int dummy)
{
  int r;
  j += 13;
  r = this->foo(15);
  return r + j + 2 * dummy;
}

int A::baz (int dummy)
{
  int r;
  j += 15;
  r = this->foo(15);
  return r + j + 12 * dummy;
}

int fum (int dummy)
{
  return 2 + 13 * dummy;
}

typedef int (A::*PMF)(int);

typedef int A::*PMI;

/* This class is in front of the other base classes of Diamond, so
   that we can detect if the offset for Left or the first Base is
   added twice - otherwise it would be 2 * 0 == 0.  */
class Padding
{
public:
  int spacer;
  virtual int vspacer();
};

int Padding::vspacer()
{
  return this->spacer;
}

class Base
{
public:
  int x;
  int get_x();
  virtual int vget_base ();
};

int Base::get_x ()
{
  return this->x;
}

int Base::vget_base ()
{
  return this->x + 1000;
}

class Left : public Base {
public:
  virtual int vget ();
};

int Left::vget ()
{
  return this->x + 100;
}

class Right : public Base {
public:
  virtual int vget ();
};

int Right::vget ()
{
  return this->x + 200;
}

class Diamond : public Padding, public Left, public Right
{
public:
  virtual int vget_base ();
};

int Diamond::vget_base ()
{
  return this->Left::x + 2000;
}

int main ()
{
  A a;
  A * a_p;
  PMF pmf;

  PMF * pmf_p;
  PMI pmi;

  Diamond diamond;
  int (Diamond::*left_pmf) ();
  int (Diamond::*right_pmf) ();
  int (Diamond::*left_vpmf) ();
  int (Diamond::*left_base_vpmf) ();
  int (Diamond::*right_vpmf) ();
  int (Base::*base_vpmf) ();
  int Diamond::*diamond_pmi;

  PMI null_pmi;
  PMF null_pmf;

  a.j = 121;
  a.jj = 1331;
  
  int k;

  a_p = &a;

  pmi = &A::j;
  pmf = &A::bar;
  pmf_p = &pmf;

  diamond.Left::x = 77;
  diamond.Right::x = 88;

  /* Some valid pointer to members from a base class.  */
  left_pmf = (int (Diamond::*) ()) (int (Left::*) ()) (&Base::get_x);
  right_pmf = (int (Diamond::*) ()) (int (Right::*) ()) (&Base::get_x);
  left_vpmf = &Left::vget;
  left_base_vpmf = (int (Diamond::*) ()) (int (Left::*) ()) (&Base::vget_base);
  right_vpmf = &Right::vget;

  /* An unspecified, value preserving pointer to member cast.  */
  base_vpmf = (int (Base::*) ()) (int (Left::*) ()) &Diamond::vget_base;

  /* A pointer to data member from a base class.  */
  diamond_pmi = (int Diamond::*) (int Left::*) &Base::x;

  null_pmi = NULL;
  null_pmf = NULL;

  pmi = NULL; /* Breakpoint 1 here.  */

  k = (a.*pmf)(3);

  pmi = &A::jj;
  pmf = &A::foo;
  pmf_p = &pmf;

  k = (a.*pmf)(4);

  k = (a.**pmf_p)(5);

  k = a.*pmi;
  

  k = a.bar(2);

  k += fum (4);

  B b;

  k += b.s;
  
}
