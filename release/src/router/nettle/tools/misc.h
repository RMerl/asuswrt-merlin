/* misc.h

   Copyright (C) 2002, 2003, 2008, 2011 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#ifndef NETTLE_TOOLS_MISC_H_INCLUDED
#define NETTLE_TOOLS_MISC_H_INCLUDED

#if HAVE_CONFIG_H
# include "config.h"
#endif

void
die(const char *format, ...) PRINTF_STYLE (1, 2) NORETURN;

void
werror(const char *format, ...) PRINTF_STYLE (1, 2);

void *
xalloc(size_t size);

enum sexp_mode
  {
    SEXP_CANONICAL = 0,
    SEXP_ADVANCED = 1,
    SEXP_TRANSPORT = 2,
  };

enum sexp_token
  {
    SEXP_STRING,
    SEXP_DISPLAY, /* Constructed by sexp_parse */
    SEXP_COMMENT,
    SEXP_LIST_START,
    SEXP_LIST_END,
    SEXP_EOF,

    /* The below types are internal to the input parsing. sexp_parse
     * should never return a token of this type. */
    SEXP_DISPLAY_START,
    SEXP_DISPLAY_END,
    SEXP_TRANSPORT_START,
    SEXP_CODING_END,
  };

extern const char
sexp_token_chars[0x80];

#define TOKEN_CHAR(c) ((c) < 0x80 && sexp_token_chars[(c)])

#endif /* NETTLE_TOOLS_MISC_H_INCLUDED */
