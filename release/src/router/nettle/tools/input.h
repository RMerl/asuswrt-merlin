/* input.h

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

#ifndef NETTLE_TOOLS_INPUT_H_INCLUDED
#define NETTLE_TOOLS_INPUT_H_INCLUDED

#include "misc.h"

#include "base16.h"
#include "base64.h"
#include "buffer.h"
#include "nettle-meta.h"

#include <stdio.h>

/* Special marks in the input stream */
enum sexp_char_type
  {
    SEXP_NORMAL_CHAR = 0,
    SEXP_EOF_CHAR, SEXP_END_CHAR,
  };

struct sexp_input
{
  FILE *f;

  /* Character stream, consisting of ordinary characters,
   * SEXP_EOF_CHAR, and SEXP_END_CHAR. */
  enum sexp_char_type ctype;
  uint8_t c;
  
  const struct nettle_armor *coding;

  union {
    struct base64_decode_ctx base64;
    struct base16_decode_ctx hex;
  } state;

  /* Terminator for current coding */
  uint8_t terminator;
  
  /* Type of current token */
  enum sexp_token token;
};

void
sexp_input_init(struct sexp_input *input, FILE *f);

void
sexp_get_char(struct sexp_input *input);

void
sexp_get_token(struct sexp_input *input, enum sexp_mode mode,
	       struct nettle_buffer *string);


#endif /* NETTLE_TOOLS_INPUT_H_INCLUDED */
