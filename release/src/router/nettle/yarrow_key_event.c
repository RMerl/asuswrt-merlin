/* yarrow_key_event.c

   Example entropy estimator for key-like input events.

   Copyright (C) 2001 Niels Möller

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

#include "yarrow.h"

void
yarrow_key_event_init(struct yarrow_key_event_ctx *ctx)
{
  unsigned i;
  
  ctx->index = 0;
  ctx->previous = 0;
  
  for (i = 0; i < YARROW_KEY_EVENT_BUFFER; i++)
    ctx->chars[i] = 0;  
}

unsigned
yarrow_key_event_estimate(struct yarrow_key_event_ctx *ctx,
			  unsigned key, unsigned time)
{
  unsigned entropy = 0;
  unsigned i;

  /* Look at timing first. */
  if (ctx->previous && (time > ctx->previous) )
    {
      if ( (time - ctx->previous) >= 256)
        entropy++;
    }
  ctx->previous = time;

  if (!key)
    return entropy;
  
  for (i = 0; i < YARROW_KEY_EVENT_BUFFER; i++)
    if (key == ctx->chars[i])
      /* This is a recent character. Ignore it. */
      return entropy;

  /* Count one bit of entropy, unless this was one of the initial 16
   * characters. */
  if (ctx->chars[ctx->index])
    entropy++;
  
  /* Remember the character. */
  
  ctx->chars[ctx->index] = key;
  ctx->index = (ctx->index + 1) % YARROW_KEY_EVENT_BUFFER;

  return entropy;
}

