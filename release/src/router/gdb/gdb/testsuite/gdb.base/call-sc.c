/* This testcase is part of GDB, the GNU debugger.

   Copyright 2004, 2007 Free Software Foundation, Inc.

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
typedef enum { e = '1' } te;

/* Force the type of each field.  */
#ifndef T
typedef t T;
#endif

T foo = '1', L;

T fun()
{
  return foo;  
}

#ifdef PROTOTYPES
void Fun(T foo)
#else
void Fun(foo)
     T foo;
#endif
{
  L = foo;
}

zed ()
{
  L = 'Z';
}

int main()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  int i;

  Fun(foo);	

  /* An infinite loop that first clears all the variables and then
     calls the function.  This "hack" is to make re-testing easier -
     "advance fun" is guaranteed to have always been preceeded by a
     global variable clearing zed call.  */

  zed ();
  while (1)
    {
      L = fun ();	
      zed ();
    }

  return 0;
}
