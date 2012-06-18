/* AS path management routines.
   Copyright (C) 1996, 97, 98, 99 Kunihiro Ishiguro

   This file is part of GNU Zebra.

   GNU Zebra is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   GNU Zebra is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Zebra; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <zebra.h>

#include "hash.h"
#include "memory.h"
#include "vector.h"
#include "vty.h"
#include "str.h"
#include "log.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_aspath.h"

/* Attr. Flags and Attr. Type Code. */
#define AS_HEADER_SIZE        2	 

/* Two octet is used for AS value. */
#define AS_VALUE_SIZE         sizeof (as_t)

/* AS segment octet length. */
#define ASSEGMENT_LEN(X)  ((X)->length * AS_VALUE_SIZE + AS_HEADER_SIZE)

/* To fetch and store as segment value. */
struct assegment
{
  u_char type;
  u_char length;
  as_t asval[1];
};

/* Hash for aspath.  This is the top level structure of AS path. */
struct hash *ashash;

static struct aspath *
aspath_new ()
{
  struct aspath *aspath;

  aspath = XMALLOC (MTYPE_AS_PATH, sizeof (struct aspath));
  memset (aspath, 0, sizeof (struct aspath));
  return aspath;
}

/* Free AS path structure. */
void
aspath_free (struct aspath *aspath)
{
  if (!aspath)
    return;
  if (aspath->data)
    XFREE (MTYPE_AS_SEG, aspath->data);
  if (aspath->str)
    XFREE (MTYPE_AS_STR, aspath->str);
  XFREE (MTYPE_AS_PATH, aspath);
}

/* Unintern aspath from AS path bucket. */
void
aspath_unintern (struct aspath *aspath)
{
  struct aspath *ret;

  if (aspath->refcnt)
    aspath->refcnt--;

  if (aspath->refcnt == 0)
    {
      /* This aspath must exist in aspath hash table. */
      ret = hash_release (ashash, aspath);
      assert (ret != NULL);
      aspath_free (aspath);
    }
}

/* Return the start or end delimiters for a particular Segment type */
#define AS_SEG_START 0
#define AS_SEG_END 1
static char
aspath_delimiter_char (u_char type, u_char which)
{
  int i;
  struct
  {
    int type;
    char start;
    char end;
  } aspath_delim_char [] =
    {
      { AS_SET,             '{', '}' },
      { AS_SEQUENCE,        ' ', ' ' },
      { AS_CONFED_SET,      '[', ']' },
      { AS_CONFED_SEQUENCE, '(', ')' },
      { 0 }
    };

  for (i = 0; aspath_delim_char[i].type != 0; i++)
    {
      if (aspath_delim_char[i].type == type)
	{
	  if (which == AS_SEG_START)
	    return aspath_delim_char[i].start;
	  else if (which == AS_SEG_END)
	    return aspath_delim_char[i].end;
	}
    }
  return ' ';
}

/* Convert aspath structure to string expression. */
char *
aspath_make_str_count (struct aspath *as)
{
  int space;
  u_char type;
  caddr_t pnt;
  caddr_t end;
  struct assegment *assegment;
  int str_size = ASPATH_STR_DEFAULT_LEN;
  int str_pnt;
  u_char *str_buf;
  int count = 0;

  /* Empty aspath. */
  if (as->length == 0)
    {
      str_buf = XMALLOC (MTYPE_AS_STR, 1);
      str_buf[0] = '\0';
      as->count = count;
      return str_buf;
    }

  /* Set default value. */
  space = 0;
  type = AS_SEQUENCE;

  /* Set initial pointer. */
  pnt = as->data;
  end = pnt + as->length;

  str_buf = XMALLOC (MTYPE_AS_STR, str_size);
  str_pnt = 0;

  assegment = (struct assegment *) pnt;

  while (pnt < end)
    {
      int i;
      int estimate_len;

      /* For fetch value. */
      assegment = (struct assegment *) pnt;

      /* Check AS type validity. */
      if ((assegment->type != AS_SET) && 
	  (assegment->type != AS_SEQUENCE) &&
	  (assegment->type != AS_CONFED_SET) && 
	  (assegment->type != AS_CONFED_SEQUENCE))
	{
	  XFREE (MTYPE_AS_STR, str_buf);
	  return NULL;
	}

      /* Check AS length. */
      if ((pnt + (assegment->length * AS_VALUE_SIZE) + AS_HEADER_SIZE) > end)
	{
	  XFREE (MTYPE_AS_STR, str_buf);
	  return NULL;
	}

      /* Buffer length check. */
      estimate_len = ((assegment->length * 6) + 4);
      
      /* String length check. */
      while (str_pnt + estimate_len >= str_size)
	{
	  str_size *= 2;
	  str_buf = XREALLOC (MTYPE_AS_STR, str_buf, str_size);
	}

      /* If assegment type is changed, print previous type's end
         character. */
      if (type != AS_SEQUENCE)
	str_buf[str_pnt++] = aspath_delimiter_char (type, AS_SEG_END);
      if (space)
	str_buf[str_pnt++] = ' ';

      if (assegment->type != AS_SEQUENCE)
	str_buf[str_pnt++] = aspath_delimiter_char (assegment->type, AS_SEG_START);

      space = 0;

      /* Increment count - ignoring CONFED SETS/SEQUENCES */
      if (assegment->type != AS_CONFED_SEQUENCE
	  && assegment->type != AS_CONFED_SET)
	{
	  if (assegment->type == AS_SEQUENCE)
	    count += assegment->length;
	  else if (assegment->type == AS_SET)
	    count++;
	}

      for (i = 0; i < assegment->length; i++)
	{
	  int len;

	  if (space)
	    {
	      if (assegment->type == AS_SET
		  || assegment->type == AS_CONFED_SET)
		str_buf[str_pnt++] = ',';
	      else
		str_buf[str_pnt++] = ' ';
	    }
	  else
	    space = 1;

	  len = sprintf (str_buf + str_pnt, "%d", ntohs (assegment->asval[i]));
	  str_pnt += len;
	}

      type = assegment->type;
      pnt += (assegment->length * AS_VALUE_SIZE) + AS_HEADER_SIZE;
    }

  if (assegment->type != AS_SEQUENCE)
    str_buf[str_pnt++] = aspath_delimiter_char (assegment->type, AS_SEG_END);

  str_buf[str_pnt] = '\0';

  as->count = count;

  return str_buf;
}

/* Intern allocated AS path. */
struct aspath *
aspath_intern (struct aspath *aspath)
{
  struct aspath *find;
  
  /* Assert this AS path structure is not interned. */
  assert (aspath->refcnt == 0);

  /* Check AS path hash. */
  find = hash_get (ashash, aspath, hash_alloc_intern);

  if (find != aspath)
    aspath_free (aspath);

  find->refcnt++;

  if (! find->str)
    find->str = aspath_make_str_count (find);

  return find;
}

/* Duplicate aspath structure.  Created same aspath structure but
   reference count and AS path string is cleared. */
struct aspath *
aspath_dup (struct aspath *aspath)
{
  struct aspath *new;

  new = XMALLOC (MTYPE_AS_PATH, sizeof (struct aspath));
  memset (new, 0, sizeof (struct aspath));

  new->length = aspath->length;

  if (new->length)
    {
      new->data = XMALLOC (MTYPE_AS_SEG, aspath->length);
      memcpy (new->data, aspath->data, aspath->length);
    }
  else
    new->data = NULL;

  /* new->str = aspath_make_str_count (aspath); */

  return new;
}

void *
aspath_hash_alloc (struct aspath *arg)
{
  struct aspath *aspath;

  /* New aspath strucutre is needed. */
  aspath = XMALLOC (MTYPE_AS_PATH, sizeof (struct aspath));
  memset ((void *) aspath, 0, sizeof (struct aspath));
  aspath->length = arg->length;

  /* In case of IBGP connection aspath's length can be zero. */
  if (arg->length)
    {
      aspath->data = XMALLOC (MTYPE_AS_SEG, arg->length);
      memcpy (aspath->data, arg->data, arg->length);
    }
  else
    aspath->data = NULL;

  /* Make AS path string. */
  aspath->str = aspath_make_str_count (aspath);

  /* Malformed AS path value. */
  if (! aspath->str)
    {
      aspath_free (aspath);
      return NULL;
    }

  return aspath;
}

/* AS path parse function.  pnt is a pointer to byte stream and length
   is length of byte stream.  If there is same AS path in the the AS
   path hash then return it else make new AS path structure. */
struct aspath *
aspath_parse (caddr_t pnt, int length)
{
  struct aspath as;
  struct aspath *find;

  /* If length is odd it's malformed AS path. */
  if (length % 2)
    return NULL;

  /* Looking up aspath hash entry. */
  as.data = pnt;
  as.length = length;

  /* If already same aspath exist then return it. */
  find = hash_get (ashash, &as, aspath_hash_alloc);
  if (! find)
    return NULL;
  find->refcnt++;

  return find;
}

#define min(A,B) ((A) < (B) ? (A) : (B))

#define ASSEGMENT_SIZE(N)  (AS_HEADER_SIZE + ((N) * AS_VALUE_SIZE))

struct aspath *
aspath_aggregate_segment_copy (struct aspath *aspath, struct assegment *seg,
			       int i)
{
  struct assegment *newseg;

  if (! aspath->data)
    {
      aspath->data = XMALLOC (MTYPE_AS_SEG, ASSEGMENT_SIZE (i));
      newseg = (struct assegment *) aspath->data;
      aspath->length = ASSEGMENT_SIZE (i);
    }
  else
    {
      aspath->data = XREALLOC (MTYPE_AS_SEG, aspath->data,
			       aspath->length + ASSEGMENT_SIZE (i));
      newseg = (struct assegment *) (aspath->data + aspath->length);
      aspath->length += ASSEGMENT_SIZE (i);
    }

  newseg->type = seg->type;
  newseg->length = i;
  memcpy (newseg->asval, seg->asval, (i * AS_VALUE_SIZE));

  return aspath;
}

struct assegment *
aspath_aggregate_as_set_add (struct aspath *aspath, struct assegment *asset,
			     as_t as)
{
  int i;

  /* If this is first AS set member, create new as-set segment. */
  if (asset == NULL)
    {
      if (! aspath->data)
	{
	  aspath->data = XMALLOC (MTYPE_AS_SEG, ASSEGMENT_SIZE (1));
	  asset = (struct assegment *) aspath->data;
	  aspath->length = ASSEGMENT_SIZE (1);
	}
      else
	{
	  aspath->data = XREALLOC (MTYPE_AS_SEG, aspath->data,
				   aspath->length + ASSEGMENT_SIZE (1));
	  asset = (struct assegment *) (aspath->data + aspath->length);
	  aspath->length += ASSEGMENT_SIZE (1);
	}
      asset->type = AS_SET;
      asset->length = 1;
      asset->asval[0] = as;
    }
  else
    {
      size_t offset;

      /* Check this AS value already exists or not. */
      for (i = 0; i < asset->length; i++)
	if (asset->asval[i] == as)
	  return asset;

      offset = (caddr_t) asset - (caddr_t) aspath->data;
      aspath->data = XREALLOC (MTYPE_AS_SEG, aspath->data,
			       aspath->length + AS_VALUE_SIZE);

      asset = (struct assegment *) (aspath->data + offset);
      aspath->length += AS_VALUE_SIZE;
      asset->asval[asset->length] = as;
      asset->length++;
    }

  return asset;
}

/* Modify as1 using as2 for aggregation. */
struct aspath *
aspath_aggregate (struct aspath *as1, struct aspath *as2)
{
  int i;
  int minlen;
  int match;
  int match1;
  int match2;
  caddr_t cp1;
  caddr_t cp2;
  caddr_t end1;
  caddr_t end2;
  struct assegment *seg1;
  struct assegment *seg2;
  struct aspath *aspath;
  struct assegment *asset;

  match = 0;
  minlen = 0;
  aspath = NULL;
  asset = NULL;
  cp1 = as1->data;
  end1 = as1->data + as1->length;
  cp2 = as2->data;
  end2 = as2->data + as2->length;

  seg1 = (struct assegment *) cp1;
  seg2 = (struct assegment *) cp2;

  /* First of all check common leading sequence. */
  while ((cp1 < end1) && (cp2 < end2))
    {
      /* Check segment type. */
      if (seg1->type != seg2->type)
	break;

      /* Minimum segment length. */
      minlen = min (seg1->length, seg2->length);

      for (match = 0; match < minlen; match++)
	if (seg1->asval[match] != seg2->asval[match])
	  break;

      if (match)
	{
	  if (! aspath)
	    aspath = aspath_new();
	  aspath = aspath_aggregate_segment_copy (aspath, seg1, match);
	}

      if (match != minlen || match != seg1->length 
	  || seg1->length != seg2->length)
	break;

      cp1 += ((seg1->length * AS_VALUE_SIZE) + AS_HEADER_SIZE);
      cp2 += ((seg2->length * AS_VALUE_SIZE) + AS_HEADER_SIZE);

      seg1 = (struct assegment *) cp1;
      seg2 = (struct assegment *) cp2;
    }

  if (! aspath)
    aspath = aspath_new();

  /* Make as-set using rest of all information. */
  match1 = match;
  while (cp1 < end1)
    {
      seg1 = (struct assegment *) cp1;

      for (i = match1; i < seg1->length; i++)
	asset = aspath_aggregate_as_set_add (aspath, asset, seg1->asval[i]);

      match1 = 0;
      cp1 += ((seg1->length * AS_VALUE_SIZE) + AS_HEADER_SIZE);
    }

  match2 = match;
  while (cp2 < end2)
    {
      seg2 = (struct assegment *) cp2;

      for (i = match2; i < seg2->length; i++)
	asset = aspath_aggregate_as_set_add (aspath, asset, seg2->asval[i]);

      match2 = 0;
      cp2 += ((seg2->length * AS_VALUE_SIZE) + AS_HEADER_SIZE);
    }

  return aspath;
}

/* When a BGP router receives an UPDATE with an MP_REACH_NLRI
   attribute, check the leftmost AS number in the AS_PATH attribute is
   or not the peer's AS number. */ 
int
aspath_firstas_check (struct aspath *aspath, as_t asno)
{
  caddr_t pnt;
  struct assegment *assegment;

  if (aspath == NULL)
    return 0;

  pnt = aspath->data;
  assegment = (struct assegment *) pnt;

  if (assegment
      && assegment->type == AS_SEQUENCE
      && assegment->asval[0] == htons (asno))
    return 1;

  return 0;
}

/* AS path loop check.  If aspath contains asno then return 1. */
int
aspath_loop_check (struct aspath *aspath, as_t asno)
{
  caddr_t pnt;
  caddr_t end;
  struct assegment *assegment;
  int count = 0;

  if (aspath == NULL)
    return 0;

  pnt = aspath->data;
  end = aspath->data + aspath->length;

  while (pnt < end)
    {
      int i;
      assegment = (struct assegment *) pnt;
      
      for (i = 0; i < assegment->length; i++)
	if (assegment->asval[i] == htons (asno))
	  count++;

      pnt += (assegment->length * AS_VALUE_SIZE) + AS_HEADER_SIZE;
    }
  return count;
}

/* When all of AS path is private AS return 1.  */
int
aspath_private_as_check (struct aspath *aspath)
{
  caddr_t pnt;
  caddr_t end;
  struct assegment *assegment;

  if (aspath == NULL)
    return 0;

  if (aspath->length == 0)
    return 0;

  pnt = aspath->data;
  end = aspath->data + aspath->length;

  while (pnt < end)
    {
      int i;
      assegment = (struct assegment *) pnt;
      
      for (i = 0; i < assegment->length; i++)
	{
	  if (ntohs (assegment->asval[i]) < BGP_PRIVATE_AS_MIN
	      || ntohs (assegment->asval[i]) > BGP_PRIVATE_AS_MAX)
	    return 0;
	}
      pnt += (assegment->length * AS_VALUE_SIZE) + AS_HEADER_SIZE;
    }
  return 1;
}

/* Merge as1 to as2.  as2 should be uninterned aspath. */
struct aspath *
aspath_merge (struct aspath *as1, struct aspath *as2)
{
  caddr_t data;

  if (! as1 || ! as2)
    return NULL;

  data = XMALLOC (MTYPE_AS_SEG, as1->length + as2->length);
  memcpy (data, as1->data, as1->length);
  memcpy (data + as1->length, as2->data, as2->length);

  XFREE (MTYPE_AS_SEG, as2->data);
  as2->data = data;
  as2->length += as1->length;
  as2->count += as1->count;
  return as2;
}

/* Prepend as1 to as2.  as2 should be uninterned aspath. */
struct aspath *
aspath_prepend (struct aspath *as1, struct aspath *as2)
{
  caddr_t pnt;
  caddr_t end;
  struct assegment *seg1 = NULL;
  struct assegment *seg2 = NULL;

  if (! as1 || ! as2)
    return NULL;

  seg2 = (struct assegment *) as2->data;

  /* In case of as2 is empty AS. */
  if (seg2 == NULL)
    {
      as2->length = as1->length;
      as2->data = XMALLOC (MTYPE_AS_SEG, as1->length);
      as2->count = as1->count;
      memcpy (as2->data, as1->data, as1->length);
      return as2;
    }

  /* assegment points last segment of as1. */
  pnt = as1->data;
  end = as1->data + as1->length;
  while (pnt < end)
    {
      seg1 = (struct assegment *) pnt;
      pnt += (seg1->length * AS_VALUE_SIZE) + AS_HEADER_SIZE;
    }

  /* In case of as1 is empty AS. */
  if (seg1 == NULL)
    return as2;

  /* Compare last segment type of as1 and first segment type of as2. */
  if (seg1->type != seg2->type)
    return aspath_merge (as1, as2);

  if (seg1->type == AS_SEQUENCE)
    {
      caddr_t newdata;
      struct assegment *seg = NULL;
      
      newdata = XMALLOC (MTYPE_AS_SEG, 
			 as1->length + as2->length - AS_HEADER_SIZE);
      memcpy (newdata, as1->data, as1->length);
      seg = (struct assegment *) (newdata + ((caddr_t)seg1 - as1->data));
      seg->length += seg2->length;
      memcpy (newdata + as1->length, as2->data + AS_HEADER_SIZE,
	      as2->length - AS_HEADER_SIZE);

      XFREE (MTYPE_AS_SEG, as2->data);
      as2->data = newdata;
      as2->length += (as1->length - AS_HEADER_SIZE);
      as2->count += as1->count;

      return as2;
    }
  else
    {
      /* AS_SET merge code is needed at here. */
      return aspath_merge (as1, as2);
    }

  /* Not reached */
}

/* Add specified AS to the leftmost of aspath. */
static struct aspath *
aspath_add_one_as (struct aspath *aspath, as_t asno, u_char type)
{
  struct assegment *assegment;

  assegment = (struct assegment *) aspath->data;

  /* In case of empty aspath. */
  if (assegment == NULL || assegment->length == 0)
    {
      aspath->length = AS_HEADER_SIZE + AS_VALUE_SIZE;

      if (assegment)
	aspath->data = XREALLOC (MTYPE_AS_SEG, aspath->data, aspath->length);
      else
	aspath->data = XMALLOC (MTYPE_AS_SEG, aspath->length);

      assegment = (struct assegment *) aspath->data;
      assegment->type = type;
      assegment->length = 1;
      assegment->asval[0] = htons (asno);

      return aspath;
    }

  if (assegment->type == type)
    {
      caddr_t newdata;
      struct assegment *newsegment;

      newdata = XMALLOC (MTYPE_AS_SEG, aspath->length + AS_VALUE_SIZE);
      newsegment = (struct assegment *) newdata;

      newsegment->type = type;
      newsegment->length = assegment->length + 1;
      newsegment->asval[0] = htons (asno);

      memcpy (newdata + AS_HEADER_SIZE + AS_VALUE_SIZE,
	      aspath->data + AS_HEADER_SIZE, 
	      aspath->length - AS_HEADER_SIZE);

      XFREE (MTYPE_AS_SEG, aspath->data);

      aspath->data = newdata;
      aspath->length += AS_VALUE_SIZE;
    } else {
      caddr_t newdata;
      struct assegment *newsegment;

      newdata = XMALLOC (MTYPE_AS_SEG, aspath->length + AS_VALUE_SIZE + AS_HEADER_SIZE);
      newsegment = (struct assegment *) newdata;

      newsegment->type = type;
      newsegment->length = 1;
      newsegment->asval[0] = htons (asno);

      memcpy (newdata + AS_HEADER_SIZE + AS_VALUE_SIZE,
	      aspath->data,
	      aspath->length);

      XFREE (MTYPE_AS_SEG, aspath->data);

      aspath->data = newdata;
      aspath->length += AS_HEADER_SIZE + AS_VALUE_SIZE;
    }

  return aspath;
}

/* Add specified AS to the leftmost of aspath. */
struct aspath *
aspath_add_seq (struct aspath *aspath, as_t asno)
{
  return aspath_add_one_as (aspath, asno, AS_SEQUENCE);
}

/* Compare leftmost AS value for MED check.  If as1's leftmost AS and
   as2's leftmost AS is same return 1. */
int
aspath_cmp_left (struct aspath *aspath1, struct aspath *aspath2)
{
  struct assegment *seg1;
  struct assegment *seg2;
  as_t as1;
  as_t as2;

  seg1 = (struct assegment *) aspath1->data;
  seg2 = (struct assegment *) aspath2->data;

  while (seg1 && seg1->length 
	 && (seg1->type == AS_CONFED_SEQUENCE || seg1->type == AS_CONFED_SET))
    seg1 = (struct assegment *) ((caddr_t) seg1 + ASSEGMENT_LEN (seg1));
  while (seg2 && seg2->length 
	 && (seg2->type == AS_CONFED_SEQUENCE || seg2->type == AS_CONFED_SET))
    seg2 = (struct assegment *) ((caddr_t) seg2 + ASSEGMENT_LEN (seg2));

  /* Check as1's */
  if (seg1 == NULL || seg1->length == 0 || seg1->type != AS_SEQUENCE)
    return 0;
  as1 = seg1->asval[0];

  if (seg2 == NULL || seg2->length == 0 || seg2->type != AS_SEQUENCE)
    return 0;
  as2 = seg2->asval[0];

  if (as1 == as2)
    return 1;

  return 0;
}

/* Compare leftmost AS value for MED check.  If as1's leftmost AS and
   as2's leftmost AS is same return 1. (confederation as-path
   only).  */
int
aspath_cmp_left_confed (struct aspath *aspath1, struct aspath *aspath2)
{
  struct assegment *seg1;
  struct assegment *seg2;

  as_t as1;
  as_t as2;

  if (aspath1->count || aspath2->count) 
    return 0;

  seg1 = (struct assegment *) aspath1->data;
  seg2 = (struct assegment *) aspath2->data;

  /* Check as1's */
  if (seg1 == NULL || seg1->length == 0 || seg1->type != AS_CONFED_SEQUENCE)
    return 0;
  as1 = seg1->asval[0];

  /* Check as2's */
  if (seg2 == NULL || seg2->length == 0 || seg2->type != AS_CONFED_SEQUENCE)
    return 0;
  as2 = seg2->asval[0];

  if (as1 == as2)
    return 1;

  return 0;
}

/* Delete first sequential AS_CONFED_SEQUENCE from aspath.  */
struct aspath *
aspath_delete_confed_seq (struct aspath *aspath)
{
  int seglen;
  struct assegment *assegment;

  if (! aspath)
    return aspath;

  assegment = (struct assegment *) aspath->data;

  while (assegment)
    {
      if (assegment->type != AS_CONFED_SEQUENCE)
	return aspath;

      seglen = ASSEGMENT_LEN (assegment);

      if (seglen == aspath->length)
	{
	  XFREE (MTYPE_AS_SEG, aspath->data);
	  aspath->data = NULL;
	  aspath->length = 0;
	}
      else
	{
	  memcpy (aspath->data, aspath->data + seglen,
		  aspath->length - seglen);
	  aspath->data = XREALLOC (MTYPE_AS_SEG, aspath->data,
				   aspath->length - seglen);
	  aspath->length -= seglen;
	}

      assegment = (struct assegment *) aspath->data;
    }
  return aspath;
}

/* Add new AS number to the leftmost part of the aspath as
   AS_CONFED_SEQUENCE.  */
struct aspath*
aspath_add_confed_seq (struct aspath *aspath, as_t asno)
{
  return aspath_add_one_as (aspath, asno, AS_CONFED_SEQUENCE);
}

/* Add new as value to as path structure. */
void
aspath_as_add (struct aspath *as, as_t asno)
{
  caddr_t pnt;
  caddr_t end;
  struct assegment *assegment;

  /* Increase as->data for new as value. */
  as->data = XREALLOC (MTYPE_AS_SEG, as->data, as->length + 2);
  as->length += 2;

  pnt = as->data;
  end = as->data + as->length;
  assegment = (struct assegment *) pnt;

  /* Last segment search procedure. */
  while (pnt + 2 < end)
    {
      assegment = (struct assegment *) pnt;

      /* We add 2 for segment_type and segment_length and segment
         value assegment->length * 2. */
      pnt += (AS_HEADER_SIZE + (assegment->length * AS_VALUE_SIZE));
    }

  assegment->asval[assegment->length] = htons (asno);
  assegment->length++;
}

/* Add new as segment to the as path. */
void
aspath_segment_add (struct aspath *as, int type)
{
  struct assegment *assegment;

  if (as->data == NULL)
    {
      as->data = XMALLOC (MTYPE_AS_SEG, 2);
      assegment = (struct assegment *) as->data;
      as->length = 2;
    }
  else
    {
      as->data = XREALLOC (MTYPE_AS_SEG, as->data, as->length + 2);
      assegment = (struct assegment *) (as->data + as->length);
      as->length += 2;
    }

  assegment->type = type;
  assegment->length = 0;
}

struct aspath *
aspath_empty ()
{
  return aspath_parse (NULL, 0);
}

struct aspath *
aspath_empty_get ()
{
  struct aspath *aspath;

  aspath = aspath_new ();
  aspath->str = aspath_make_str_count (aspath);
  return aspath;
}

unsigned long
aspath_count ()
{
  return ashash->count;
}     

/* 
   Theoretically, one as path can have:

   One BGP packet size should be less than 4096.
   One BGP attribute size should be less than 4096 - BGP header size.
   One BGP aspath size should be less than 4096 - BGP header size -
   BGP mandantry attribute size.
*/

/* AS path string lexical token enum. */
enum as_token
  {
    as_token_asval,
    as_token_set_start,
    as_token_set_end,
    as_token_confed_start,
    as_token_confed_end,
    as_token_unknown
  };

/* Return next token and point for string parse. */
char *
aspath_gettoken (char *buf, enum as_token *token, u_short *asno)
{
  char *p = buf;

  /* Skip space. */
  while (isspace ((int) *p))
    p++;

  /* Check the end of the string and type specify characters
     (e.g. {}()). */
  switch (*p)
    {
    case '\0':
      return NULL;
      break;
    case '{':
      *token = as_token_set_start;
      p++;
      return p;
      break;
    case '}':
      *token = as_token_set_end;
      p++;
      return p;
      break;
    case '(':
      *token = as_token_confed_start;
      p++;
      return p;
      break;
    case ')':
      *token = as_token_confed_end;
      p++;
      return p;
      break;
    }

  /* Check actual AS value. */
  if (isdigit ((int) *p)) 
    {
      u_short asval;

      *token = as_token_asval;
      asval = (*p - '0');
      p++;
      while (isdigit ((int) *p)) 
	{
	  asval *= 10;
	  asval += (*p - '0');
	  p++;
	}
      *asno = asval;
      return p;
    }
  
  /* There is no match then return unknown token. */
  *token = as_token_unknown;
  return  p++;
}

struct aspath *
aspath_str2aspath (char *str)
{
  enum as_token token;
  u_short as_type;
  u_short asno;
  struct aspath *aspath;
  int needtype;

  aspath = aspath_new ();

  /* We start default type as AS_SEQUENCE. */
  as_type = AS_SEQUENCE;
  needtype = 1;

  while ((str = aspath_gettoken (str, &token, &asno)) != NULL)
    {
      switch (token)
	{
	case as_token_asval:
	  if (needtype)
	    {
	      aspath_segment_add (aspath, as_type);
	      needtype = 0;
	    }
	  aspath_as_add (aspath, asno);
	  break;
	case as_token_set_start:
	  as_type = AS_SET;
	  aspath_segment_add (aspath, as_type);
	  needtype = 0;
	  break;
	case as_token_set_end:
	  as_type = AS_SEQUENCE;
	  needtype = 1;
	  break;
	case as_token_confed_start:
	  as_type = AS_CONFED_SEQUENCE;
	  aspath_segment_add (aspath, as_type);
	  needtype = 0;
	  break;
	case as_token_confed_end:
	  as_type = AS_SEQUENCE;
	  needtype = 1;
	  break;
	case as_token_unknown:
	default:
	  return NULL;
	  break;
	}
    }

  aspath->str = aspath_make_str_count (aspath);

  return aspath;
}

/* Make hash value by raw aspath data. */
unsigned int
aspath_key_make (struct aspath *aspath)
{
  unsigned int key = 0;
  int length;
  unsigned short *pnt;

  length = aspath->length / 2;
  pnt = (unsigned short *) aspath->data;

  while (length)
    {
      key += *pnt++;
      length--;
    }

  return key;
}

/* If two aspath have same value then return 1 else return 0 */
int
aspath_cmp (struct aspath *as1, struct aspath *as2)
{
  if (as1->length == as2->length 
      && !memcmp (as1->data, as2->data, as1->length))
    return 1;
  else
    return 0;
}

/* AS path hash initialize. */
void
aspath_init ()
{
  ashash = hash_create_size (32767, aspath_key_make, aspath_cmp);
}

/* return and as path value */
const char *
aspath_print (struct aspath *as)
{
  return as->str;
}

/* Printing functions */
void
aspath_print_vty (struct vty *vty, struct aspath *as)
{
  vty_out (vty, "%s", as->str);
}

void
aspath_show_all_iterator (struct hash_backet *backet, struct vty *vty)
{
  struct aspath *as;

  as = (struct aspath *) backet->data;

  vty_out (vty, "[%p:%d] (%ld) ", backet, backet->key, as->refcnt);
  vty_out (vty, "%s%s", as->str, VTY_NEWLINE);
}

/* Print all aspath and hash information.  This function is used from
   `show ip bgp paths' command. */
void
aspath_print_all_vty (struct vty *vty)
{
  hash_iterate (ashash, 
		(void (*) (struct hash_backet *, void *))
		aspath_show_all_iterator,
		vty);
}
