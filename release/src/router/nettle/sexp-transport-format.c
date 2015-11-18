/* sexp-transport-format.c

   Writing s-expressions in transport format.

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

#include "sexp.h"

#include "base64.h"
#include "buffer.h"

size_t
sexp_transport_vformat(struct nettle_buffer *buffer,
		       const char *format, va_list args)
{
  size_t start = 0;
  size_t length;
  size_t base64_length;

  if (buffer)
    {
      if (!NETTLE_BUFFER_PUTC(buffer, '{'))
	return 0;

      start = buffer->size;
    }
  
  length = sexp_vformat(buffer, format, args);

  if (!length)
    return 0;

  base64_length = BASE64_ENCODE_RAW_LENGTH(length);

  if (buffer)
    {
      if (!nettle_buffer_space(buffer, base64_length - length))
	return 0;

      base64_encode_raw(buffer->contents + start,
			length, buffer->contents + start);
      
      if (!NETTLE_BUFFER_PUTC(buffer, '}'))
	return 0;
    }
  
  return base64_length + 2;
}

size_t
sexp_transport_format(struct nettle_buffer *buffer,
		      const char *format, ...)
{
  size_t done;
  va_list args;

  va_start(args, format);
  done = sexp_transport_vformat(buffer, format, args);
  va_end(args);
  
  return done;
}
