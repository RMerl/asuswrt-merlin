/* parse.h

   Copyright (C) 2002, 2003 Niels Möller

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

#ifndef NETTLE_TOOLS_PARSE_H_INCLUDED
#define NETTLE_TOOLS_PARSE_H_INCLUDED

#include "misc.h"
#include "buffer.h"

struct sexp_compound_token
{
  enum sexp_token type;
  struct nettle_buffer display;
  struct nettle_buffer string;  
};

void
sexp_compound_token_init(struct sexp_compound_token *token);

void
sexp_compound_token_clear(struct sexp_compound_token *token);

struct sexp_parser
{
  struct sexp_input *input;
  enum sexp_mode mode;

  /* Nesting level of lists. Transport encoding counts as one
   * level of nesting. */
  unsigned level;

  /* The nesting level where the transport encoding occured.
   * Zero if we're not currently using tranport encoding. */
  unsigned transport;
};

void
sexp_parse_init(struct sexp_parser *parser,
		struct sexp_input *input,
		enum sexp_mode mode);

void
sexp_parse(struct sexp_parser *parser,
	   struct sexp_compound_token *token);

#endif /* NETTLE_TOOLS_PARSE_H_INCLUDED */
