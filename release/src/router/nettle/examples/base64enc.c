/* base64enc -- an encoder for base64

   Copyright (C) 2006, 2012 Jeronimo Pellegrini, Niels Möller

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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <fcntl.h>
#endif

#include "base64.h"

#include "io.h"

/* The number of bytes read in each iteration, we do one line at a time: */
#define CHUNK_SIZE 54

/* The *maximum* size of an encoded chunk: */
#define ENCODED_SIZE BASE64_ENCODE_LENGTH(CHUNK_SIZE)

/*
 * Reads bytes from standard input and writes base64-encoded
 * on standard output.
 */
int
main(int argc UNUSED, char **argv UNUSED)
{
  struct base64_encode_ctx b64_ctx;

  /* Init the context: */
  base64_encode_init(&b64_ctx);

#ifdef WIN32
  _setmode(0, O_BINARY);
#endif

  for (;;)
    {
      /* "buffer" will hold the bytes from disk: */
      uint8_t buffer[CHUNK_SIZE];
      /* "result" is the result vector: */
      uint8_t result[ENCODED_SIZE + BASE64_ENCODE_FINAL_LENGTH + 1];
      unsigned nbytes; /* Number of bytes read from stdin */
      int encoded_bytes; /* total number of bytes encoded per iteration */
      nbytes = fread(buffer,1,CHUNK_SIZE,stdin);

      /* We overwrite result with more data */
      encoded_bytes = base64_encode_update(&b64_ctx, result, nbytes, buffer);

      if (nbytes < CHUNK_SIZE)
	{
	  if (ferror(stdin))
	    {
	      werror ("Error reading file: %s\n", strerror(errno));
	      return EXIT_FAILURE;
	    }
	  encoded_bytes += base64_encode_final(&b64_ctx,result + encoded_bytes);

	  result[encoded_bytes++] = '\n';
	  if (!write_string (stdout, encoded_bytes, result)
	      || fflush (stdout) != 0)
	    {
	      werror ("Error writing file: %s\n", strerror(errno));
	      return EXIT_FAILURE;
	    }
	  return EXIT_SUCCESS;
	}

      /* The result vector is written */
      result[encoded_bytes++] = '\n';
      if (!write_string (stdout, encoded_bytes, result))
	{
	  werror ("Error writing file: %s\n", strerror(errno));
	  return EXIT_FAILURE;
	}
    }
}
