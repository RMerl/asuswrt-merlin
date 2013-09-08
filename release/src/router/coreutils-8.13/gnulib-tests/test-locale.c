/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test of <locale.h> substitute.
   Copyright (C) 2007, 2009-2011 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>, 2007.  */

#include <config.h>

#include <locale.h>

#include "verify.h"

int a[] =
  {
    LC_ALL,
    LC_COLLATE,
    LC_CTYPE,
    LC_MESSAGES,
    LC_MONETARY,
    LC_NUMERIC,
    LC_TIME
  };

#if HAVE_NEWLOCALE
/* Check that the locale_t type and the LC_GLOBAL_LOCALE macro are defined.  */
locale_t b = LC_GLOBAL_LOCALE;
#endif

/* Check that NULL can be passed through varargs as a pointer type,
   per POSIX 2008.  */
verify (sizeof NULL == sizeof (void *));

int
main ()
{
  return 0;
}
