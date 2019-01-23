/* Lowercase mapping for UTF-8 strings (locale dependent).
   Copyright (C) 2009-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2009.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "unicase.h"

#include <stddef.h>

#include "unicasemap.h"
#include "special-casing.h"

uint8_t *
u8_tolower (const uint8_t *s, size_t n, const char *iso639_language,
            uninorm_t nf,
            uint8_t *resultbuf, size_t *lengthp)
{
  return u8_casemap (s, n,
                     unicase_empty_prefix_context, unicase_empty_suffix_context,
                     iso639_language,
                     uc_tolower, offsetof (struct special_casing_rule, lower[0]),
                     nf,
                     resultbuf, lengthp);
}


#ifdef TEST

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read the contents of an input stream, and return it, terminated with a NUL
   byte. */
char *
read_file (FILE *stream)
{
#define BUFSIZE 4096
  char *buf = NULL;
  int alloc = 0;
  int size = 0;
  int count;

  while (! feof (stream))
    {
      if (size + BUFSIZE > alloc)
        {
          alloc = alloc + alloc / 2;
          if (alloc < size + BUFSIZE)
            alloc = size + BUFSIZE;
          buf = realloc (buf, alloc);
          if (buf == NULL)
            {
              fprintf (stderr, "out of memory\n");
              exit (1);
            }
        }
      count = fread (buf + size, 1, BUFSIZE, stream);
      if (count == 0)
        {
          if (ferror (stream))
            {
              perror ("fread");
              exit (1);
            }
        }
      else
        size += count;
    }
  buf = realloc (buf, size + 1);
  if (buf == NULL)
    {
      fprintf (stderr, "out of memory\n");
      exit (1);
    }
  buf[size] = '\0';
  return buf;
#undef BUFSIZE
}

int
main (int argc, char * argv[])
{
  setlocale (LC_ALL, "");
  if (argc == 1)
    {
      /* Display the lower case of the input string.  */
      char *input = read_file (stdin);
      int length = strlen (input);
      size_t output_length;
      uint8_t *output =
        u8_tolower ((uint8_t *) input, length, uc_locale_language (),
                    NULL,
                    NULL, &output_length);

      fwrite (output, 1, output_length, stdout);

      return 0;
    }
  else
    return 1;
}

#endif /* TEST */
