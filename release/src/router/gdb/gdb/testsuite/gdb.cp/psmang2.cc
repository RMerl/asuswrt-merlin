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

#include <stdio.h>

/* Do not move this definition into a header file!  See the comments
   in psmang.exp.  */
struct s
{
  int value;
  void method1 (void);
  void method2 (void);
};

void
s::method2 (void)
{
  printf ("%d\n", value);
}


/* The presence of these variables ensures there will be so many
   symbols in psmang2.cc's symtab's global block that it will have a
   non-trivial hash table.  When there are only a very few symbols,
   the block only has one hash bucket, so even if we compute the hash
   value for the wrong symbol name, we'll still find a symbol that
   matches.  */
int a;
int b;
int a1;
int b1;
int a2;
int b2;
int a12;
int b12;
int a3;
int b3;
int a13;
int b13;
int a23;
int b23;
int a123;
int b123;
int a4;
int b4;
int a14;
int b14;
int a24;
int b24;
int a124;
int b124;
int a34;
int b34;
int a134;
int b134;
int a234;
int b234;
int a1234;
int b1234;
int a5;
int b5;
int a15;
int b15;
int a25;
int b25;
int a125;
int b125;
int a35;
int b35;
int a135;
int b135;
int a235;
int b235;
int a1235;
int b1235;
int a45;
int b45;
int a145;
int b145;
int a245;
int b245;
int a1245;
int b1245;
int a345;
int b345;
int a1345;
int b1345;
int a2345;
int b2345;
int a12345;
int b12345;
int a6;
int b6;
int a16;
int b16;
int a26;
int b26;
int a126;
int b126;
int a36;
int b36;
int a136;
int b136;
int a236;
int b236;
int a1236;
int b1236;
int a46;
int b46;
int a146;
int b146;
int a246;
int b246;
int a1246;
int b1246;
int a346;
int b346;
int a1346;
int b1346;
int a2346;
int b2346;
int a12346;
int b12346;
int a56;
int b56;
int a156;
int b156;
int a256;
int b256;
int a1256;
int b1256;
int a356;
int b356;
int a1356;
int b1356;
int a2356;
int b2356;
int a12356;
int b12356;
int a456;
int b456;
int a1456;
int b1456;
int a2456;
int b2456;
int a12456;
int b12456;
int a3456;
int b3456;
int a13456;
int b13456;
int a23456;
int b23456;
int a123456;
int b123456;
