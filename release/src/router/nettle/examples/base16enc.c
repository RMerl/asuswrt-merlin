/* base16enc -- an encoder for base16

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

#include "base16.h"

#include "io.h"

/* The number of bytes read in each iteration, we do one line at a time: */
#define CHUNK_SIZE 36

/* The *maximum* size of an encoded chunk: */
#define ENCODED_SIZE BASE16_ENCODE_LENGTH(CHUNK_SIZE)

/*
 * Reads bytes from standard input and writes base64-encoded
 * on standard output.
 */
int
main(int argc UNUSED, char **argv UNUSED)
{

#ifdef WIN32
  _setmode(0, O_BINARY);
#endif
  
  /* There is no context to initialize. */

  for (;;)
    {
      /* "buffer" will hold the bytes from disk: */
      uint8_t buffer[CHUNK_SIZE];
      /* "result" will hold bytes before output: */
      uint8_t result[ENCODED_SIZE + 1];
      unsigned nbytes; /* Number of bytes read from stdin */
      int encoded_bytes; /* Total number of bytes encoded per iteration */
      
      nbytes = fread(buffer,1,CHUNK_SIZE,stdin);

      /* We overwrite result with more data */
      base16_encode_update(result, nbytes, buffer);
      encoded_bytes = BASE16_ENCODE_LENGTH(nbytes);
      result[encoded_bytes++] = '\n';
      
      if (nbytes < CHUNK_SIZE)
	{
	  if (ferror(stdin))
	    {
	      werror ("Error reading file: %s\n", strerror(errno));
	      return EXIT_FAILURE;
	    }
	  if (!write_string (stdout, encoded_bytes, result)
	      || fflush (stdout) != 0)
	    {
	      werror ("Error writing file: %s\n", strerror(errno));
	      return EXIT_FAILURE;
	    }
	  return EXIT_SUCCESS;
	}
      /* The result vector is printed */
      if (!write_string(stdout,encoded_bytes, result))
	{
	  werror ("Error writing file: %s\n", strerror(errno));
	  return EXIT_FAILURE;
	}
    }
}

