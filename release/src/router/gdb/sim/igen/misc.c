/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */



#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "config.h"
#include "misc.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/* NB: Because warning and error can be interchanged, neither append a
   trailing '\n' */

void
error (const line_ref *line, char *msg, ...)
{
  va_list ap;
  if (line != NULL)
    fprintf (stderr, "%s:%d: ", line->file_name, line->line_nr);
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  exit (1);
}

void
warning (const line_ref *line, char *msg, ...)
{
  va_list ap;
  if (line != NULL)
    fprintf (stderr, "%s:%d: warning: ", line->file_name, line->line_nr);
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
}

void
notify (const line_ref *line, char *msg, ...)
{
  va_list ap;
  if (line != NULL)
    fprintf (stdout, "%s %d: info: ", line->file_name, line->line_nr);
  va_start (ap, msg);
  vfprintf (stdout, msg, ap);
  va_end (ap);
}

void *
zalloc (long size)
{
  void *memory = malloc (size);
  if (memory == NULL)
    ERROR ("zalloc failed");
  memset (memory, 0, size);
  return memory;
}


unsigned long long
a2i (const char *a)
{
  int neg = 0;
  int base = 10;
  unsigned long long num = 0;
  int looping;

  while (isspace (*a))
    a++;

  if (strcmp (a, "true") == 0 || strcmp (a, "TRUE") == 0)
    return 1;

  if (strcmp (a, "false") == 0 || strcmp (a, "false") == 0)
    return 0;

  if (*a == '-')
    {
      neg = 1;
      a++;
    }

  if (*a == '0')
    {
      if (a[1] == 'x' || a[1] == 'X')
	{
	  a += 2;
	  base = 16;
	}
      else if (a[1] == 'b' || a[1] == 'b')
	{
	  a += 2;
	  base = 2;
	}
      else
	base = 8;
    }

  looping = 1;
  while (looping)
    {
      int ch = *a++;

      switch (base)
	{
	default:
	  looping = 0;
	  break;

	case 2:
	  if (ch >= '0' && ch <= '1')
	    {
	      num = (num * 2) + (ch - '0');
	    }
	  else
	    {
	      looping = 0;
	    }
	  break;

	case 10:
	  if (ch >= '0' && ch <= '9')
	    {
	      num = (num * 10) + (ch - '0');
	    }
	  else
	    {
	      looping = 0;
	    }
	  break;

	case 8:
	  if (ch >= '0' && ch <= '7')
	    {
	      num = (num * 8) + (ch - '0');
	    }
	  else
	    {
	      looping = 0;
	    }
	  break;

	case 16:
	  if (ch >= '0' && ch <= '9')
	    {
	      num = (num * 16) + (ch - '0');
	    }
	  else if (ch >= 'a' && ch <= 'f')
	    {
	      num = (num * 16) + (ch - 'a' + 10);
	    }
	  else if (ch >= 'A' && ch <= 'F')
	    {
	      num = (num * 16) + (ch - 'A' + 10);
	    }
	  else
	    {
	      looping = 0;
	    }
	  break;
	}
    }

  if (neg)
    num = -num;

  return num;
}

unsigned
target_a2i (int ms_bit_nr, const char *a)
{
  if (ms_bit_nr)
    return (ms_bit_nr - a2i (a));
  else
    return a2i (a);
}

unsigned
i2target (int ms_bit_nr, unsigned bit)
{
  if (ms_bit_nr)
    return ms_bit_nr - bit;
  else
    return bit;
}


int
name2i (const char *names, const name_map * map)
{
  const name_map *curr;
  const char *name = names;
  while (*name != '\0')
    {
      /* find our name */
      char *end = strchr (name, ',');
      char *next;
      unsigned len;
      if (end == NULL)
	{
	  end = strchr (name, '\0');
	  next = end;
	}
      else
	{
	  next = end + 1;
	}
      len = end - name;
      /* look it up */
      curr = map;
      while (curr->name != NULL)
	{
	  if (strncmp (curr->name, name, len) == 0
	      && strlen (curr->name) == len)
	    return curr->i;
	  curr++;
	}
      name = next;
    }
  /* nothing found, possibly return a default */
  curr = map;
  while (curr->name != NULL)
    curr++;
  if (curr->i >= 0)
    return curr->i;
  else
    error (NULL, "%s contains no valid names", names);
  return 0;
}

const char *
i2name (const int i, const name_map * map)
{
  while (map->name != NULL)
    {
      if (map->i == i)
	return map->name;
      map++;
    }
  error (NULL, "map lookup failed for %d\n", i);
  return NULL;
}
