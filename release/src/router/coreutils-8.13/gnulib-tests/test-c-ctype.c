/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test of character handling in C locale.
   Copyright (C) 2005, 2007-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2005.  */

#include <config.h>

#include "c-ctype.h"

#include <locale.h>

#include "macros.h"

static void
test_all (void)
{
  int c;

  for (c = -0x80; c < 0x100; c++)
    {
      ASSERT (c_isascii (c) == (c >= 0 && c < 0x80));

      switch (c)
        {
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
        case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
        case 's': case 't': case 'u': case 'v': case 'w': case 'x':
        case 'y': case 'z':
        case '0': case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
          ASSERT (c_isalnum (c) == 1);
          break;
        default:
          ASSERT (c_isalnum (c) == 0);
          break;
        }

      switch (c)
        {
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
        case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
        case 's': case 't': case 'u': case 'v': case 'w': case 'x':
        case 'y': case 'z':
          ASSERT (c_isalpha (c) == 1);
          break;
        default:
          ASSERT (c_isalpha (c) == 0);
          break;
        }

      switch (c)
        {
        case '\t': case ' ':
          ASSERT (c_isblank (c) == 1);
          break;
        default:
          ASSERT (c_isblank (c) == 0);
          break;
        }

      ASSERT (c_iscntrl (c) == ((c >= 0 && c < 0x20) || c == 0x7f));

      switch (c)
        {
        case '0': case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
          ASSERT (c_isdigit (c) == 1);
          break;
        default:
          ASSERT (c_isdigit (c) == 0);
          break;
        }

      switch (c)
        {
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
        case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
        case 's': case 't': case 'u': case 'v': case 'w': case 'x':
        case 'y': case 'z':
          ASSERT (c_islower (c) == 1);
          break;
        default:
          ASSERT (c_islower (c) == 0);
          break;
        }

      ASSERT (c_isgraph (c) == ((c >= 0x20 && c < 0x7f) && c != ' '));

      ASSERT (c_isprint (c) == (c >= 0x20 && c < 0x7f));

      ASSERT (c_ispunct (c) == (c_isgraph (c) && !c_isalnum (c)));

      switch (c)
        {
        case ' ': case '\t': case '\n': case '\v': case '\f': case '\r':
          ASSERT (c_isspace (c) == 1);
          break;
        default:
          ASSERT (c_isspace (c) == 0);
          break;
        }

      switch (c)
        {
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
          ASSERT (c_isupper (c) == 1);
          break;
        default:
          ASSERT (c_isupper (c) == 0);
          break;
        }

      switch (c)
        {
        case '0': case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
          ASSERT (c_isxdigit (c) == 1);
          break;
        default:
          ASSERT (c_isxdigit (c) == 0);
          break;
        }

      switch (c)
        {
        case 'A':
          ASSERT (c_tolower (c) == 'a');
          ASSERT (c_toupper (c) == c);
          break;
        case 'B':
          ASSERT (c_tolower (c) == 'b');
          ASSERT (c_toupper (c) == c);
          break;
        case 'C':
          ASSERT (c_tolower (c) == 'c');
          ASSERT (c_toupper (c) == c);
          break;
        case 'D':
          ASSERT (c_tolower (c) == 'd');
          ASSERT (c_toupper (c) == c);
          break;
        case 'E':
          ASSERT (c_tolower (c) == 'e');
          ASSERT (c_toupper (c) == c);
          break;
        case 'F':
          ASSERT (c_tolower (c) == 'f');
          ASSERT (c_toupper (c) == c);
          break;
        case 'G':
          ASSERT (c_tolower (c) == 'g');
          ASSERT (c_toupper (c) == c);
          break;
        case 'H':
          ASSERT (c_tolower (c) == 'h');
          ASSERT (c_toupper (c) == c);
          break;
        case 'I':
          ASSERT (c_tolower (c) == 'i');
          ASSERT (c_toupper (c) == c);
          break;
        case 'J':
          ASSERT (c_tolower (c) == 'j');
          ASSERT (c_toupper (c) == c);
          break;
        case 'K':
          ASSERT (c_tolower (c) == 'k');
          ASSERT (c_toupper (c) == c);
          break;
        case 'L':
          ASSERT (c_tolower (c) == 'l');
          ASSERT (c_toupper (c) == c);
          break;
        case 'M':
          ASSERT (c_tolower (c) == 'm');
          ASSERT (c_toupper (c) == c);
          break;
        case 'N':
          ASSERT (c_tolower (c) == 'n');
          ASSERT (c_toupper (c) == c);
          break;
        case 'O':
          ASSERT (c_tolower (c) == 'o');
          ASSERT (c_toupper (c) == c);
          break;
        case 'P':
          ASSERT (c_tolower (c) == 'p');
          ASSERT (c_toupper (c) == c);
          break;
        case 'Q':
          ASSERT (c_tolower (c) == 'q');
          ASSERT (c_toupper (c) == c);
          break;
        case 'R':
          ASSERT (c_tolower (c) == 'r');
          ASSERT (c_toupper (c) == c);
          break;
        case 'S':
          ASSERT (c_tolower (c) == 's');
          ASSERT (c_toupper (c) == c);
          break;
        case 'T':
          ASSERT (c_tolower (c) == 't');
          ASSERT (c_toupper (c) == c);
          break;
        case 'U':
          ASSERT (c_tolower (c) == 'u');
          ASSERT (c_toupper (c) == c);
          break;
        case 'V':
          ASSERT (c_tolower (c) == 'v');
          ASSERT (c_toupper (c) == c);
          break;
        case 'W':
          ASSERT (c_tolower (c) == 'w');
          ASSERT (c_toupper (c) == c);
          break;
        case 'X':
          ASSERT (c_tolower (c) == 'x');
          ASSERT (c_toupper (c) == c);
          break;
        case 'Y':
          ASSERT (c_tolower (c) == 'y');
          ASSERT (c_toupper (c) == c);
          break;
        case 'Z':
          ASSERT (c_tolower (c) == 'z');
          ASSERT (c_toupper (c) == c);
          break;
        case 'a':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'A');
          break;
        case 'b':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'B');
          break;
        case 'c':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'C');
          break;
        case 'd':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'D');
          break;
        case 'e':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'E');
          break;
        case 'f':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'F');
          break;
        case 'g':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'G');
          break;
        case 'h':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'H');
          break;
        case 'i':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'I');
          break;
        case 'j':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'J');
          break;
        case 'k':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'K');
          break;
        case 'l':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'L');
          break;
        case 'm':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'M');
          break;
        case 'n':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'N');
          break;
        case 'o':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'O');
          break;
        case 'p':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'P');
          break;
        case 'q':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'Q');
          break;
        case 'r':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'R');
          break;
        case 's':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'S');
          break;
        case 't':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'T');
          break;
        case 'u':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'U');
          break;
        case 'v':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'V');
          break;
        case 'w':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'W');
          break;
        case 'x':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'X');
          break;
        case 'y':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'Y');
          break;
        case 'z':
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == 'Z');
          break;
        default:
          ASSERT (c_tolower (c) == c);
          ASSERT (c_toupper (c) == c);
          break;
        }
    }
}

int
main ()
{
  test_all ();

  setlocale (LC_ALL, "de_DE");
  test_all ();

  setlocale (LC_ALL, "ja_JP.EUC-JP");
  test_all ();

  return 0;
}
