/*  bag.c -- ARMulator support code:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. */

/********************************************************************/
/* bag.c:                                                           */
/* Offers a data structure for storing and getting pairs of number. */
/* The numbers are stored together, put one can be looked up by     */
/* quoting the other.  If a new pair is entered and one of the      */
/* numbers is a repeat of a previous pair, then the previos pair    */
/* is deleted.                                                      */
/********************************************************************/

#include "bag.h"
#include <stdlib.h>

#define HASH_TABLE_SIZE 256
#define hash(x) (((x)&0xff)^(((x)>>8)&0xff)^(((x)>>16)&0xff)^(((x)>>24)&0xff))

typedef struct hashentry
{
  struct hashentry *next;
  int first;
  int second;
}
Hashentry;

Hashentry *lookupbyfirst[HASH_TABLE_SIZE];
Hashentry *lookupbysecond[HASH_TABLE_SIZE];

void
addtolist (Hashentry ** add, long first, long second)
{
  while (*add)
    add = &((*add)->next);
  /* Malloc will never fail? :o( */
  (*add) = (Hashentry *) malloc (sizeof (Hashentry));
  (*add)->next = (Hashentry *) 0;
  (*add)->first = first;
  (*add)->second = second;
}

void
killwholelist (Hashentry * p)
{
  Hashentry *q;

  while (p)
    {
      q = p;
      p = p->next;
      free (q);
    }
}

static void
removefromlist (Hashentry ** p, long first)
{
  Hashentry *q;

  while (*p)
    {
      if ((*p)->first == first)
	{
	  q = (*p)->next;
	  free (*p);
	  *p = q;
	  return;
	}
      p = &((*p)->next);
    }
}

void
BAG_putpair (long first, long second)
{
  long junk;

  if (BAG_getfirst (&junk, second) != NO_SUCH_PAIR)
    BAG_killpair_bysecond (second);
  addtolist (&lookupbyfirst[hash (first)], first, second);
  addtolist (&lookupbysecond[hash (second)], first, second);
}

Bag_error
BAG_getfirst (long *first, long second)
{
  Hashentry *look;

  look = lookupbysecond[hash (second)];
  while (look)
    if (look->second == second)
      {
	*first = look->first;
	return NO_ERROR;
      }
  return NO_SUCH_PAIR;
}

Bag_error
BAG_getsecond (long first, long *second)
{
  Hashentry *look;

  look = lookupbyfirst[hash (first)];
  while (look)
    {
      if (look->first == first)
	{
	  *second = look->second;
	  return NO_ERROR;
	}
      look = look->next;
    }
  return NO_SUCH_PAIR;
}

Bag_error
BAG_killpair_byfirst (long first)
{
  long second;

  if (BAG_getsecond (first, &second) == NO_SUCH_PAIR)
    return NO_SUCH_PAIR;
  removefromlist (&lookupbyfirst[hash (first)], first);
  removefromlist (&lookupbysecond[hash (second)], first);
  return NO_ERROR;
}

Bag_error
BAG_killpair_bysecond (long second)
{
  long first;

  if (BAG_getfirst (&first, second) == NO_SUCH_PAIR)
    return NO_SUCH_PAIR;
  removefromlist (&lookupbyfirst[hash (first)], first);
  removefromlist (&lookupbysecond[hash (second)], first);
  return NO_ERROR;
}

void
BAG_newbag ()
{
  int i;

  for (i = 0; i < 256; i++)
    {
      killwholelist (lookupbyfirst[i]);
      killwholelist (lookupbysecond[i]);
      lookupbyfirst[i] = lookupbysecond[i] = (Hashentry *) 0;
    }
}
