/* sexp.c

   Parsing s-expressions.

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

#include <stdlib.h>
#include <string.h>

#include "sexp.h"

#include "macros.h"
#include "nettle-internal.h"

/* Initializes the iterator, but one has to call next to get to the
 * first element. */
static void
sexp_iterator_init(struct sexp_iterator *iterator,
		   unsigned length, const uint8_t *input)
{
  iterator->length = length;
  iterator->buffer = input;
  iterator->pos = 0;
  iterator->level = 0;
  iterator->type = SEXP_END; /* Value doesn't matter */
  iterator->display_length = 0;
  iterator->display = NULL;
  iterator->atom_length = 0;
  iterator->atom = NULL;
}

#define EMPTY(i) ((i)->pos == (i)->length)
#define NEXT(i) ((i)->buffer[(i)->pos++])

static int
sexp_iterator_simple(struct sexp_iterator *iterator,
		     size_t *size,
		     const uint8_t **string)
{
  unsigned length = 0;
  uint8_t c;
  
  if (EMPTY(iterator)) return 0;
  c = NEXT(iterator);
  if (EMPTY(iterator)) return 0;

  if (c >= '1' && c <= '9')
    do
      {
	length = length * 10 + (c - '0');
	if (length > (iterator->length - iterator->pos))
	  return 0;

	if (EMPTY(iterator)) return 0;
	c = NEXT(iterator);
      }
    while (c >= '0' && c <= '9');

  else if (c == '0')
    /* There can be only one */
    c = NEXT(iterator);
  else 
    return 0;

  if (c != ':')
    return 0;

  *size = length;
  *string = iterator->buffer + iterator->pos;
  iterator->pos += length;

  return 1;
}

/* All these functions return 1 on success, 0 on failure */

/* Look at the current position in the data. Sets iterator->type, and
 * ignores the old value. */

static int
sexp_iterator_parse(struct sexp_iterator *iterator)
{
  iterator->start = iterator->pos;
  
  if (EMPTY(iterator))
    {
      if (iterator->level)
	return 0;
      
      iterator->type = SEXP_END;
      return 1;
    }
  switch (iterator->buffer[iterator->pos])
    {
    case '(': /* A list */
      iterator->type = SEXP_LIST;
      return 1;

    case ')':
      if (!iterator->level)
	return 0;
      
      iterator->pos++;
      iterator->type = SEXP_END;      
      return 1;
      
    case '[': /* Atom with display type */
      iterator->pos++;
      if (!sexp_iterator_simple(iterator,
				&iterator->display_length,
				&iterator->display))
	return 0;
      if (EMPTY(iterator) || NEXT(iterator) != ']')
	return 0;

      break;

    default:
      /* Must be either a decimal digit or a syntax error.
       * Errors are detected by sexp_iterator_simple. */
      iterator->display_length = 0;
      iterator->display = NULL;

      break;
    }

  iterator->type = SEXP_ATOM;
      
  return sexp_iterator_simple(iterator,
			      &iterator->atom_length,
			      &iterator->atom);
}

int
sexp_iterator_first(struct sexp_iterator *iterator,
		    size_t length, const uint8_t *input)
{
  sexp_iterator_init(iterator, length, input);
  return sexp_iterator_parse(iterator);
}

int
sexp_iterator_next(struct sexp_iterator *iterator)
{
  switch (iterator->type)
    {
    case SEXP_END:
      return 1;
    case SEXP_LIST:
      /* Skip this list */
      return sexp_iterator_enter_list(iterator)
	&& sexp_iterator_exit_list(iterator);
    case SEXP_ATOM:
      /* iterator->pos should already point at the start of the next
       * element. */
      return sexp_iterator_parse(iterator);
    }
  /* If we get here, we have a bug. */
  abort();
}

/* Current element must be a list. */
int
sexp_iterator_enter_list(struct sexp_iterator *iterator)
{
  if (iterator->type != SEXP_LIST)
    return 0;

  if (EMPTY(iterator) || NEXT(iterator) != '(')
    /* Internal error */
    abort();

  iterator->level++;

  return sexp_iterator_parse(iterator);
}

/* Skips the rest of the current list */
int
sexp_iterator_exit_list(struct sexp_iterator *iterator)
{
  if (!iterator->level)
    return 0;

  while(iterator->type != SEXP_END)
    if (!sexp_iterator_next(iterator))
      return 0;
      
  iterator->level--;

  return sexp_iterator_parse(iterator);
}

#if 0
/* What's a reasonable interface for this? */
int
sexp_iterator_exit_lists(struct sexp_iterator *iterator,
			 unsigned level)
{
  assert(iterator->level >= level);

  while (iterator->level > level)
    if (!sexp_iterator_exit_list(iterator))
      return 0;

  return 1;
}
#endif

const uint8_t *
sexp_iterator_subexpr(struct sexp_iterator *iterator,
		      size_t *length)
{
  size_t start = iterator->start;
  if (!sexp_iterator_next(iterator))
    return 0;

  *length = iterator->start - start;
  return iterator->buffer + start;
}

int
sexp_iterator_get_uint32(struct sexp_iterator *iterator,
			 uint32_t *x)
{
  if (iterator->type == SEXP_ATOM
      && !iterator->display
      && iterator->atom_length
      && iterator->atom[0] < 0x80)
    {
      size_t length = iterator->atom_length;
      const uint8_t *p = iterator->atom;

      /* Skip leading zeros. */
      while(length && !*p)
	{
	  length--; p++;
	}

      switch(length)
	{
	case 0:
	  *x = 0;
	  break;
	case 1:
	  *x = p[0];
	  break;
	case 2:
	  *x = READ_UINT16(p);
	  break;
	case 3:
	  *x = READ_UINT24(p);
	  break;
	case 4:
	  *x = READ_UINT32(p);
	  break;
	default:
	  return 0;
	}
      return sexp_iterator_next(iterator);
    }
  return 0;
}

int
sexp_iterator_check_type(struct sexp_iterator *iterator,
			 const uint8_t *type)
{
  return (sexp_iterator_enter_list(iterator)
	  && iterator->type == SEXP_ATOM
	  && !iterator->display
	  && strlen(type) == iterator->atom_length
	  && !memcmp(type, iterator->atom, iterator->atom_length)
	  && sexp_iterator_next(iterator));
}

const uint8_t *
sexp_iterator_check_types(struct sexp_iterator *iterator,
			  unsigned ntypes,
			  const uint8_t * const *types)
{
  if (sexp_iterator_enter_list(iterator)
      && iterator->type == SEXP_ATOM
      && !iterator->display)
    {
      unsigned i;
      for (i = 0; i<ntypes; i++)
	if (strlen(types[i]) == iterator->atom_length
	    && !memcmp(types[i], iterator->atom,
		       iterator->atom_length))
	  return sexp_iterator_next(iterator) ? types[i] : NULL;
    }
  return NULL;
}

int
sexp_iterator_assoc(struct sexp_iterator *iterator,
		    unsigned nkeys,
		    const uint8_t * const *keys,
		    struct sexp_iterator *values)
{
  TMP_DECL(found, int, NETTLE_MAX_SEXP_ASSOC);
  unsigned nfound;
  unsigned i;

  TMP_ALLOC(found, nkeys);
  for (i = 0; i<nkeys; i++)
    found[i] = 0;

  nfound = 0;
  
  for (;;)
    {
      switch (iterator->type)
	{
	case SEXP_LIST:

	  if (!sexp_iterator_enter_list(iterator))
	    return 0;
	  
	  if (iterator->type == SEXP_ATOM
	      && !iterator->display)
	    {
	      /* Compare to the given keys */
	      for (i = 0; i<nkeys; i++)
		{
		  /* NOTE: The strlen could be put outside of the
		   * loop */
		  if (strlen(keys[i]) == iterator->atom_length
		      && !memcmp(keys[i], iterator->atom,
				 iterator->atom_length))
		    {
		      if (found[i])
			/* We don't allow duplicates */
			return 0;

		      /* Advance to point to value */
		      if (!sexp_iterator_next(iterator))
			return 0;

		      found[i] = 1;
		      nfound++;
		      
		      /* Record this position. */
		      values[i] = *iterator;
		      
		      break;
		    }
		}
	    }
	  if (!sexp_iterator_exit_list(iterator))
	    return 0;
	  break;
	case SEXP_ATOM:
	  /* Just ignore */
	  if (!sexp_iterator_next(iterator))
	    return 0;
	  break;
	  
	case SEXP_END:
	  return sexp_iterator_exit_list(iterator)
	    && (nfound == nkeys);

	default:
	  abort();
	}
    }
}
