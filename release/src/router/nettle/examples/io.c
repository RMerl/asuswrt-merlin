/* io.c

   Miscellaneous functions used by the example programs.

   Copyright (C) 2002, 2012 Niels Möller

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

#include <stdarg.h>
#include <stdlib.h>

/* For errno and strerror */
#include <errno.h>
#include <string.h>

#include "io.h"

#define RANDOM_DEVICE "/dev/urandom"
#define BUFSIZE 1000

int quiet_flag = 0;

void *
xalloc(size_t size)
{
  void *p = malloc(size);
  if (!p)
    {
      fprintf(stderr, "Virtual memory exhausted.\n");
      abort();
    }

  return p;
}

void
werror(const char *format, ...)
{
  if (!quiet_flag)
    {
      va_list args;
      va_start(args, format);
      vfprintf(stderr, format, args);
      va_end(args);
    }
}

unsigned
read_file(const char *name, unsigned max_size, char **contents)
{
  unsigned size, done;
  char *buffer;
  FILE *f;
    
  f = fopen(name, "rb");
  if (!f)
    {
      werror("Opening `%s' failed: %s\n", name, strerror(errno));
      return 0;
    }

  size = 100;

  for (buffer = NULL, done = 0;; size *= 2)
    {
      char *p;

      if (max_size && size > max_size)
	size = max_size;

      /* Space for terminating NUL */
      p = realloc(buffer, size + 1);

      if (!p)
	{
	fail:
	  fclose(f);
	  free(buffer);
	  *contents = NULL;
	  return 0;
	}

      buffer = p;
      done += fread(buffer + done, 1, size - done, f);

      if (done < size)
	{
	  /* Short count means EOF or read error */
	  if (ferror(f))
	    {
	      fprintf (stderr, "Reading `%s' failed: %s\n",
		       name, strerror(errno));

	      goto fail;
	    }
	  if (done == 0)
	    /* Treat empty file as error */
	    goto fail;

	  break;
	}

      if (size == max_size)
	break;
    }
  
  fclose(f);

  /* NUL-terminate the data. */
  buffer[done] = '\0';
  *contents = buffer;
  
  return done;
}

int
write_string(FILE *f, unsigned size, const char *buffer)
{
  size_t res = fwrite(buffer, 1, size, f);

  return res == size;
}

int
write_file(const char *name, unsigned size, const char *buffer)
{
  FILE *f = fopen(name, "wb");
  int res;
  
  if (!f)
    return 0;

  res = write_string(f, size, buffer);
  return fclose(f) == 0 && res;
}

int
simple_random(struct yarrow256_ctx *ctx, const char *name)
{
  unsigned length;
  char *buffer;

  if (name)
    length = read_file(name, 0, &buffer);
  else
    length = read_file(RANDOM_DEVICE, 20, &buffer);
  
  if (!length)
    return 0;

  yarrow256_seed(ctx, length, buffer);

  free(buffer);

  return 1;
}

int
hash_file(const struct nettle_hash *hash, void *ctx, FILE *f)
{
  for (;;)
    {
      char buffer[BUFSIZE];
      size_t res = fread(buffer, 1, sizeof(buffer), f);
      if (ferror(f))
	return 0;
      
      hash->update(ctx, res, buffer);
      if (feof(f))
	return 1;
    }
}
