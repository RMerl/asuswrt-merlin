/* This test script is part of GDB, the GNU debugger.

   Copyright 2002, 2004,
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

/* Do not move this definition into a header file!  See the comments
   in psmang.exp.  */
struct s
{
  int value;
  void method1 (void);
  void method2 (void);
};

void
s::method1 ()
{
  value = 42;
}

int
main (int argc, char **argv)
{
  s si;

  si.method1 ();
  si.method2 ();
}


/* The presence of these variables ensures there will be so many
   symbols in psmang1.cc's symtab's global block that it will have a
   non-trivial hash table.  When there are only a very few symbols,
   the block only has one hash bucket, so even if we compute the hash
   value for the wrong symbol name, we'll still find a symbol that
   matches.  */
int ax;
int bx;
int a1x;
int b1x;
int a2x;
int b2x;
int a12x;
int b12x;
int a3x;
int b3x;
int a13x;
int b13x;
int a23x;
int b23x;
int a123x;
int b123x;
int a4x;
int b4x;
int a14x;
int b14x;
int a24x;
int b24x;
int a124x;
int b124x;
int a34x;
int b34x;
int a134x;
int b134x;
int a234x;
int b234x;
int a1234x;
int b1234x;
int a5x;
int b5x;
int a15x;
int b15x;
int a25x;
int b25x;
int a125x;
int b125x;
int a35x;
int b35x;
int a135x;
int b135x;
int a235x;
int b235x;
int a1235x;
int b1235x;
int a45x;
int b45x;
int a145x;
int b145x;
int a245x;
int b245x;
int a1245x;
int b1245x;
int a345x;
int b345x;
int a1345x;
int b1345x;
int a2345x;
int b2345x;
int a12345x;
int b12345x;
int a6x;
int b6x;
int a16x;
int b16x;
int a26x;
int b26x;
int a126x;
int b126x;
int a36x;
int b36x;
int a136x;
int b136x;
int a236x;
int b236x;
int a1236x;
int b1236x;
int a46x;
int b46x;
int a146x;
int b146x;
int a246x;
int b246x;
int a1246x;
int b1246x;
int a346x;
int b346x;
int a1346x;
int b1346x;
int a2346x;
int b2346x;
int a12346x;
int b12346x;
int a56x;
int b56x;
int a156x;
int b156x;
int a256x;
int b256x;
int a1256x;
int b1256x;
int a356x;
int b356x;
int a1356x;
int b1356x;
int a2356x;
int b2356x;
int a12356x;
int b12356x;
int a456x;
int b456x;
int a1456x;
int b1456x;
int a2456x;
int b2456x;
int a12456x;
int b12456x;
int a3456x;
int b3456x;
int a13456x;
int b13456x;
int a23456x;
int b23456x;
int a123456x;
int b123456x;
