/* This testcase is part of GDB, the GNU debugger.

   Copyright 2001, 2004, 2007 Free Software Foundation, Inc.

   Contributed by Red Hat, originally written by Jim Blandy.

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
   bug-gdb@gnu.org  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* X_string is a null-terminated string in the X charset whose
   elements are as follows.  X should be the name the `set charset'
   command uses for the character set, in lower-case, with any
   non-identifier characters replaced with underscores.  Where a
   character set doesn't have the given character, the string should
   contain the character 'x'.

   [0] --- the `alert' character, '\a'
   [1] --- the `backspace' character, '\b'
   [2] --- the `form feed' character, '\f'
   [3] --- the `line feed' character, '\n'
   [4] --- the `carriage return' character, '\r'
   [5] --- the `horizontal tab' character, '\t'
   [6] --- the `vertical tab' character, '\v'
   [7  .. 32] --- the uppercase letters A-Z
   [33 .. 58] --- the lowercase letters a-z
   [59 .. 68] --- the digits 0-9
   [69] --- the `cent' character
   [70] --- a control character with no defined backslash escape

   Feel free to extend these as you like.  */

#define NUM_CHARS (71)

char ascii_string[NUM_CHARS];
char iso_8859_1_string[NUM_CHARS];
char ebcdic_us_string[NUM_CHARS];
char ibm1047_string[NUM_CHARS];


void
init_string (char string[],
             char x,
             char alert, char backspace, char form_feed,
             char line_feed, char carriage_return, char horizontal_tab,
             char vertical_tab, char cent, char misc_ctrl)
{
  memset (string, x, NUM_CHARS);
  string[0] = alert;
  string[1] = backspace;
  string[2] = form_feed;
  string[3] = line_feed;
  string[4] = carriage_return;
  string[5] = horizontal_tab;
  string[6] = vertical_tab;
  string[69] = cent;
  string[70] = misc_ctrl;
}


void
fill_run (char string[], int start, int len, int first)
{
  int i;

  for (i = 0; i < len; i++)
    string[start + i] = first + i;
}


int main ()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  (void) malloc (1);
  /* Initialize ascii_string.  */
  init_string (ascii_string,
               120,
               7, 8, 12,
               10, 13, 9,
               11, 120, 17);
  fill_run (ascii_string, 7, 26, 65);
  fill_run (ascii_string, 33, 26, 97);
  fill_run (ascii_string, 59, 10, 48);

  /* Initialize iso_8859_1_string.  */
  init_string (iso_8859_1_string,
               120,
               7, 8, 12,
               10, 13, 9,
               11, 162, 17);
  fill_run (iso_8859_1_string, 7, 26, 65);
  fill_run (iso_8859_1_string, 33, 26, 97);
  fill_run (iso_8859_1_string, 59, 10, 48);

  /* Initialize ebcdic_us_string.  */
  init_string (ebcdic_us_string,
               167,
               47, 22, 12,
               37, 13, 5,
               11, 74, 17);
  /* In EBCDIC, the upper-case letters are broken into three separate runs.  */
  fill_run (ebcdic_us_string, 7, 9, 193);
  fill_run (ebcdic_us_string, 16, 9, 209);
  fill_run (ebcdic_us_string, 25, 8, 226);
  /* The lower-case letters are, too.  */
  fill_run (ebcdic_us_string, 33, 9, 129);
  fill_run (ebcdic_us_string, 42, 9, 145);
  fill_run (ebcdic_us_string, 51, 8, 162);
  /* The digits, at least, are contiguous.  */
  fill_run (ebcdic_us_string, 59, 10, 240);

  /* Initialize ibm1047_string.  */
  init_string (ibm1047_string,
               167,
               47, 22, 12,
               37, 13, 5,
               11, 74, 17);
  /* In EBCDIC, the upper-case letters are broken into three separate runs.  */
  fill_run (ibm1047_string, 7, 9, 193);
  fill_run (ibm1047_string, 16, 9, 209);
  fill_run (ibm1047_string, 25, 8, 226);
  /* The lower-case letters are, too.  */
  fill_run (ibm1047_string, 33, 9, 129);
  fill_run (ibm1047_string, 42, 9, 145);
  fill_run (ibm1047_string, 51, 8, 162);
  /* The digits, at least, are contiguous.  */
  fill_run (ibm1047_string, 59, 10, 240);

  puts ("All set!");            /* all strings initialized */
}
