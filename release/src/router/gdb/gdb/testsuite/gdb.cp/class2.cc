/* This testcase is part of GDB, the GNU debugger.

   Copyright 2003, 2004, 2007 Free Software Foundation, Inc.

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

struct A
{
  virtual ~A ();
  int a1;
};

A::~A()
{
  a1 = 800;
}

struct B : public A
{
  virtual ~B ();
  int b1;
  int b2;
};

B::~B()
{
  a1 = 900;
  b1 = 901;
  b2 = 902;
}

// Stop the compiler from optimizing away data.
void refer (A *)
{
  ;
}

struct empty {};

// Stop the compiler from optimizing away data.
void refer (empty *)
{
  ;
}

int main (void)
{
  A alpha, *aap, *abp;
  B beta, *bbp;
  empty e;

  alpha.a1 = 100;
  beta.a1 = 200; beta.b1 = 201; beta.b2 = 202;

  aap = &alpha; refer (aap);
  abp = &beta;  refer (abp);
  bbp = &beta;  refer (bbp);
  refer (&e);

  return 0;  // marker return 0
} // marker close brace
