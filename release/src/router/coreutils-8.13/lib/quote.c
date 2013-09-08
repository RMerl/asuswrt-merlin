/* quote.c - quote arguments for output

   Copyright (C) 1998-2001, 2003, 2005-2006, 2009-2011 Free Software
   Foundation, Inc.

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

/* Written by Paul Eggert <eggert@twinsun.com> */

#include <config.h>

#include "quotearg.h"
#include "quote.h"

/* Return an unambiguous printable representation of NAME,
   allocated in slot N, suitable for diagnostics.  */
char const *
quote_n (int n, char const *name)
{
  return quotearg_n_style (n, locale_quoting_style, name);
}

/* Return an unambiguous printable representation of NAME,
   suitable for diagnostics.  */
char const *
quote (char const *name)
{
  return quote_n (0, name);
}
