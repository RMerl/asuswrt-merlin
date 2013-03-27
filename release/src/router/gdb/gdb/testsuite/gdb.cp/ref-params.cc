/* This test script is part of GDB, the GNU debugger.

   Copyright 2006, 2007 Free Software Foundation, Inc.

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

/* Author: Paul N. Hilfinger, AdaCore Inc. */

struct Parent {
  Parent (int id0) : id(id0) { }
  int id;
};

struct Child : public Parent {
  Child (int id0) : Parent(id0) { }
};

int f1(Parent& R)
{
  return R.id;			/* Set breakpoint marker3 here.  */
}

int f2(Child& C)
{
  return f1(C);			/* Set breakpoint marker2 here.  */
}

struct OtherParent {
  OtherParent (int other_id0) : other_id(other_id0) { }
  int other_id;
};

struct MultiChild : public Parent, OtherParent {
  MultiChild (int id0) : Parent(id0), OtherParent(id0 * 2) { }
};

int mf1(OtherParent& R)
{
  return R.other_id;
}

int mf2(MultiChild& C)
{
  return mf1(C);
}

int main(void) 
{
  Child Q(42);
  Child& QR = Q;

  #ifdef usestubs
     set_debug_traps();
     breakpoint();
  #endif

  /* Set breakpoint marker1 here.  */

  f2(Q);
  f2(QR);

  MultiChild MQ(53);
  MultiChild& MQR = MQ;

  mf2(MQ);			/* Set breakpoint MQ here.  */
}
