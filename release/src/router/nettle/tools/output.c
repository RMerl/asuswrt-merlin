/* output.c

   Copyright (C) 2002, 2003, 2009 Niels Möller

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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "output.h"

/* For TMP_ALLOC */ 
#include "nettle-internal.h"

void
sexp_output_init(struct sexp_output *output, FILE *f,
		 unsigned width, int prefer_hex)
{
  output->f = f;
  output->line_width = width;
  output->coding = NULL;
  output->prefer_hex = prefer_hex;
  output->hash = NULL;
  output->ctx = NULL;
  
  output->pos = 0;
  output->soft_newline = 0;
}

void
sexp_output_hash_init(struct sexp_output *output,
		      const struct nettle_hash *hash, void *ctx)
{
  output->hash = hash;
  output->ctx = ctx;
  hash->init(ctx);
}

static void
sexp_put_raw_char(struct sexp_output *output, uint8_t c)
{
  if (putc(c, output->f) < 0)
    die("Write failed: %s\n", strerror(errno));

  output->pos++;
  output->soft_newline = 0;
}

void
sexp_put_newline(struct sexp_output *output,
		 unsigned indent)
{
  if (output->soft_newline)
    output->soft_newline = 0;
  else
    {
      unsigned i;

      sexp_put_raw_char(output, '\n');
      output->pos = 0;
  
      for(i = 0; i < indent; i++)
	sexp_put_raw_char(output, ' ');
  
      output->pos = indent;
    }
}

/* Put a newline, but only if it is followed by another newline,
   collaps to one newline only. */
void
sexp_put_soft_newline(struct sexp_output *output,
		      unsigned indent)
{
  sexp_put_newline(output, indent);
  output->soft_newline = 1;
}

void
sexp_put_char(struct sexp_output *output, uint8_t c)
{
  if (output->coding)
    {
      /* Two is enough for both base16 and base64. */
      uint8_t encoded[2];
      unsigned done;

      unsigned i;

      done = output->coding->encode_update(&output->base64, encoded,
					   1, &c);
      assert(done <= sizeof(encoded));
      
      for (i = 0; i<done; i++)
	{
	  if (output->line_width
	      && output->pos >= output->line_width
	      && output->pos >= (output->coding_indent + 10))
	    sexp_put_newline(output, output->coding_indent);
	  
	  sexp_put_raw_char(output, encoded[i]);
	}
    }
  else if (output->hash)
    output->hash->update(output->ctx, 1, &c);
  else
    sexp_put_raw_char(output, c);
}

void
sexp_put_data(struct sexp_output *output,
	      unsigned length, const uint8_t *data)
{
  unsigned i;

  for (i = 0; i<length; i++)
    sexp_put_char(output, data[i]);
}

static void
sexp_put_length(struct sexp_output *output, 
		unsigned length)
{
  unsigned digit = 1;

  for (;;)
    {
      unsigned next = digit * 10;
      if (next > length)
	break;
      digit = next;
    }

  for (; digit; length %= digit, digit /= 10)
    sexp_put_char(output, '0' + length / digit);
}

void
sexp_put_code_start(struct sexp_output *output,
		    const struct nettle_armor *coding)
{
  assert(!output->coding);
  
  output->coding_indent = output->pos;
  
  output->coding = coding;
  output->coding->encode_init(&output->base64);
}

void
sexp_put_code_end(struct sexp_output *output)
{
  /* Enough for both hex and base64 */
  uint8_t encoded[BASE64_ENCODE_FINAL_LENGTH];
  unsigned done;

  assert(output->coding);

  done = output->coding->encode_final(&output->base64, encoded);

  assert(done <= sizeof(encoded));
  
  output->coding = NULL;

  sexp_put_data(output, done, encoded);
}

void
sexp_put_string(struct sexp_output *output, enum sexp_mode mode,
		struct nettle_buffer *string)
{
  if (!string->size)
    sexp_put_data(output, 2,
		  (mode == SEXP_ADVANCED) ? "\"\"": "0:");

  else if (mode == SEXP_ADVANCED)
    {
      unsigned i;
      int token = (string->contents[0] < '0' || string->contents[0] > '9');
      int quote_friendly = 1;
#define CONTROL_SIZE 0x20
      static const char escape_names[CONTROL_SIZE] =
	{
	  0,0,0,0,0,0,0,0, 'b','t','n',0,'f','r',0,0,
	  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
	};

      for (i = 0; i<string->size; i++)
	{
	  uint8_t c = string->contents[i];
	  
	  if (token & !TOKEN_CHAR(c))
	    token = 0;
	  
	  if (quote_friendly)
	    {
	      if (c >= 0x7f)
		quote_friendly = 0;
	      else if (c < CONTROL_SIZE && !escape_names[c])
		quote_friendly = 0;
	    }
	}
      
      if (token)
	sexp_put_data(output, string->size, string->contents);

      else if (quote_friendly)
	{
	  sexp_put_char(output, '"');

	  for (i = 0; i<string->size; i++)
	    {
	      int escape = 0;
	      uint8_t c = string->contents[i];

	      assert(c < 0x7f);
	      
	      if (c == '\\' || c == '"')
		escape = 1;
	      else if (c < CONTROL_SIZE)
		{
		  escape = 1;
		  c = escape_names[c];
		  assert(c);
		}
	      if (escape)
		sexp_put_char(output, '\\');

	      sexp_put_char(output, c);
	    }
	  
	  sexp_put_char(output, '"');
	}
      else 
	{
	  uint8_t delimiter;
	  const struct nettle_armor *coding;
	  
	  if (output->prefer_hex)
	    {
	      delimiter = '#';
	      coding = &nettle_base16;
	    }
	  else
	    {
	      delimiter = '|';
	      coding = &nettle_base64;
	    }
	  
	  sexp_put_char(output, delimiter);
	  sexp_put_code_start(output, coding);
	  sexp_put_data(output, string->size, string->contents);
	  sexp_put_code_end(output);
	  sexp_put_char(output, delimiter);
	}
#undef CONTROL_SIZE
    }
  else
    {
      sexp_put_length(output, string->size);
      sexp_put_char(output, ':');
      sexp_put_data(output, string->size, string->contents);
    }
}

void
sexp_put_digest(struct sexp_output *output)
{
  TMP_DECL(digest, uint8_t, NETTLE_MAX_HASH_DIGEST_SIZE);
  TMP_ALLOC(digest, output->hash->digest_size);
  
  assert(output->hash);

  output->hash->digest(output->ctx, output->hash->digest_size, digest);

  sexp_put_code_start(output, &nettle_base16);
  sexp_put_data(output, output->hash->digest_size, digest);
  sexp_put_code_end(output);
}

