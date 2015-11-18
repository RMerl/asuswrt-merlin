/* buffer.c

   A bare-bones string stream.

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
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

int
nettle_buffer_grow(struct nettle_buffer *buffer,
		   size_t length)
{
  assert(buffer->size <= buffer->alloc);
  
  if (buffer->size + length > buffer->alloc)
    {
      size_t alloc;
      uint8_t *p;
      
      if (!buffer->realloc)
	return 0;
      
      alloc = buffer->alloc * 2 + length + 100;
      p = buffer->realloc(buffer->realloc_ctx, buffer->contents, alloc);
      if (!p)
	return 0;
      
      buffer->contents = p;
      buffer->alloc = alloc;
    }
  return 1;
}

void
nettle_buffer_init_realloc(struct nettle_buffer *buffer,
			   void *realloc_ctx,
			   nettle_realloc_func *realloc)
{
  buffer->contents = NULL;
  buffer->alloc = 0;
  buffer->realloc = realloc;
  buffer->realloc_ctx = realloc_ctx;
  buffer->size = 0;
}

void
nettle_buffer_init_size(struct nettle_buffer *buffer,
			size_t length, uint8_t *space)
{
  buffer->contents = space;
  buffer->alloc = length;
  buffer->realloc = NULL;
  buffer->realloc_ctx = NULL;
  buffer->size = 0;
}

void
nettle_buffer_clear(struct nettle_buffer *buffer)
{
  if (buffer->realloc)
    buffer->realloc(buffer->realloc_ctx, buffer->contents, 0);

  buffer->contents = NULL;
  buffer->alloc = 0;
  buffer->size = 0;
}

void
nettle_buffer_reset(struct nettle_buffer *buffer)
{
  buffer->size = 0;
}

uint8_t *
nettle_buffer_space(struct nettle_buffer *buffer,
		    size_t length)
{
  uint8_t *p;

  if (!nettle_buffer_grow(buffer, length))
    return NULL;

  p = buffer->contents + buffer->size;
  buffer->size += length;
  return p;
}
     
int
nettle_buffer_write(struct nettle_buffer *buffer,
		    size_t length, const uint8_t *data)
{
  uint8_t *p = nettle_buffer_space(buffer, length);
  if (p)
    {
      memcpy(p, data, length);
      return 1;
    }
  else
    return 0;
}

int
nettle_buffer_copy(struct nettle_buffer *dst,
		   const struct nettle_buffer *src)
{
  return nettle_buffer_write(dst, src->size, src->contents);
}
