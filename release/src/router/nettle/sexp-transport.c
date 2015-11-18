/* sexp-transport.c

   Parsing s-expressions in transport format.

   Copyright (C) 2002 Niels Möller

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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "sexp.h"

#include "base64.h"

/* NOTE: Decodes the input string in place */
int
sexp_transport_iterator_first(struct sexp_iterator *iterator,
			      size_t length, uint8_t *input)
{
  /* We first base64 decode any transport encoded sexp at the start of
   * the input. */

  size_t in = 0;
  size_t out = 0;

  while (in < length)
    switch(input[in])
      {
      case ' ':  /* SPC, TAB, LF, CR */
      case '\t':
      case '\n':
      case '\r':
	in++;
	break;
	  
      case ';':  /* Comments */
	while (++in < length && input[in] != '\n')
	  ;
	break;
	  
      case '{':
	{
	  /* Found transport encoding */
	  struct base64_decode_ctx ctx;
	  size_t coded_length;
	  size_t end;

	  for (end = ++in; end < length && input[end] != '}'; end++)
	    ;

	  if (end == length)
	    return 0;
	    
	  base64_decode_init(&ctx);
	  
	  if (base64_decode_update(&ctx, &coded_length, input + out,
				   end - in, input + in)
	      && base64_decode_final(&ctx))
	    {	  
	      out += coded_length;
	      in = end + 1;
	    }
	  else
	    return 0;
	  
	  break;
	}
      default:
	/* Expression isn't in transport encoding. Rest of the input
	 * should be in canonical encoding. */
	goto transport_done;
      }
  
 transport_done:

  /* Here, we have two, possibly empty, input parts in canonical
   * encoding:
   *
   * 0...out-1,  in...length -1
   *
   * If the input was already in canonical encoding, out = 0 and in =
   * amount of leading space.
   *
   * If all input was in transport encoding, in == length.
   */
  if (!out)
    {
      input += in;
      length -= in;
    }
  else if (in == length)
    length = out;
  else if (out == in)
    /* Unusual case, nothing happens */
    ;
  else
    {
      /* Both parts non-empty */
      assert(out < in);
      memmove(input + out, input + in, length - in);
      length -= (in - out);
    }

  return sexp_iterator_first(iterator, length, input);
}
