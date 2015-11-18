/* base64dec -- an decoder for base64

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

#define CHUNK_SIZE 16392

/* The maximum number of bytes generated for each chunk: */
#define DECODED_SIZE BASE64_DECODE_LENGTH(CHUNK_SIZE)


/*
 * Reads base-64 encoded from stdin, writes decoded to stdout.
 */
int
main(int argc UNUSED, char **argv UNUSED)
{
  /* "buffer" will hold the bytes from disk: */
  uint8_t * buffer = xalloc (CHUNK_SIZE);

  /* "result" will hold bytes before output: */
  uint8_t * result = xalloc (DECODED_SIZE);

  /* We need a Base64 context for decoding: */
  struct base64_decode_ctx b64_ctx;

  /* Init the context: */
  base64_decode_init(&b64_ctx);

#ifdef WIN32
  _setmode(1, O_BINARY);
#endif

  for (;;)
    {
      int nbytes; /* Number of bytes read frmo disk at each iteration */
      size_t decoded_bytes; /* Bytes actually generated at each iteration */

      nbytes = fread(buffer, 1, CHUNK_SIZE, stdin);

      if (nbytes < CHUNK_SIZE && ferror(stdin))
	{
	  werror ("Error reading file: %s\n", strerror(errno));
	  return EXIT_FAILURE;
	}

      /* Decodes one chunk: */
      if (!base64_decode_update(&b64_ctx, &decoded_bytes, result, nbytes, buffer))
	{
	  werror ("Error decoding input (not base64?)\n");
	  return EXIT_FAILURE;
	}

      if (!write_string (stdout, decoded_bytes, result))
	{
	  werror ("Error writing file: %s\n", strerror(errno));
	  return EXIT_FAILURE;
	}

      if (nbytes < CHUNK_SIZE)
	{
	  /* Check if decoding finalized OK: */
	  if (!base64_decode_final(&b64_ctx))
	    {
	      werror ("Decoding did not finish properly.\n");
	      return EXIT_FAILURE;
	    }
	  break;
	}
    }

  if (fflush (stdout) != 0)
    {
      werror ("Error writing file: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }

  free (buffer);
  free (result);

  return EXIT_SUCCESS;
}
