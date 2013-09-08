/* -*- buffer-read-only: t -*- vi: set ro: */
/* DO NOT EDIT! GENERATED AUTOMATICALLY! */
/* Test of <stddef.h> substitute.
   Copyright (C) 2009-2011 Free Software Foundation, Inc.

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

/* Written by Eric Blake <ebb9@byu.net>, 2009.  */

#include <config.h>

#include <stddef.h>

#include "verify.h"

/* Check that appropriate types are defined.  */
wchar_t a = 'c';
ptrdiff_t b = 1;
size_t c = 2;

/* Check that NULL can be passed through varargs as a pointer type,
   per POSIX 2008.  */
verify (sizeof NULL == sizeof (void *));

/* Check that offsetof produces integer constants with correct type.  */
struct d
{
  char e;
  char f;
};
/* Solaris 10 has a bug where offsetof is under-parenthesized, and
   cannot be used as an arbitrary expression.  However, since it is
   unlikely to bite real code, we ignore that short-coming.  */
/* verify (sizeof offsetof (struct d, e) == sizeof (size_t)); */
verify (sizeof (offsetof (struct d, e)) == sizeof (size_t));
verify (offsetof (struct d, e) < -1); /* Must be unsigned.  */
verify (offsetof (struct d, f) == 1);

int
main (void)
{
  return 0;
}
