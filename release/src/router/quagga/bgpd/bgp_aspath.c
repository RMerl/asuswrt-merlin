/* AS path management routines.
   Copyright (C) 1996, 97, 98, 99 Kunihiro Ishiguro
   Copyright (C) 2005 Sun Microsystems, Inc.

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
#include "stream.h"
#include "jhash.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_attr.h"

/* Attr. Flags and Attr. Type Code. */
#define AS_HEADER_SIZE        2	 

/* Now FOUR octets are used for AS value. */
#define AS_VALUE_SIZE         sizeof (as_t)
/* This is the old one */
#define AS16_VALUE_SIZE	      sizeof (as16_t)

/* Maximum protocol segment length value */
#define AS_SEGMENT_MAX		255

/* The following length and size macros relate specifically to Quagga's
 * internal representation of AS-Segments, not per se to the on-wire
 * sizes and lengths.  At present (200508) they sort of match, however
 * the ONLY functions which should now about the on-wire syntax are
 * aspath_put, assegment_put and assegment_parse.
 *
 * aspath_put returns bytes written, the only definitive record of
 * size of wire-format attribute..
 */

/* Calculated size in bytes of ASN segment data to hold N ASN's */
#define ASSEGMENT_DATA_SIZE(N,S) \
	((N) * ( (S) ? AS_VALUE_SIZE : AS16_VALUE_SIZE) )

/* Calculated size of segment struct to hold N ASN's */
#define ASSEGMENT_SIZE(N,S)  (AS_HEADER_SIZE + ASSEGMENT_DATA_SIZE (N,S))

/* AS segment octet length. */
#define ASSEGMENT_LEN(X,S) ASSEGMENT_SIZE((X)->length,S)

/* AS_SEQUENCE segments can be packed together */
/* Can the types of X and Y be considered for packing? */
#define ASSEGMENT_TYPES_PACKABLE(X,Y) \
  ( ((X)->type == (Y)->type) \
   && ((X)->type == AS_SEQUENCE))
/* Types and length of X,Y suitable for packing? */
#define ASSEGMENTS_PACKABLE(X,Y) \
  ( ASSEGMENT_TYPES_PACKABLE( (X), (Y)) \
   && ( ((X)->length + (Y)->length) <= AS_SEGMENT_MAX ) )

/* As segment header - the on-wire representation 
 * NOT the internal representation!
 */
struct assegment_header
{
  u_char type;
  u_char length;
};

/* Hash for aspath.  This is the top level structure of AS path. */
static struct hash *ashash;

/* Stream for SNMP. See aspath_snmp_pathseg */
static struct stream *snmp_stream;

/* Callers are required to initialize the memory */
static as_t *
assegment_data_new (int num)
{
  return (XMALLOC (MTYPE_AS_SEG_DATA, ASSEGMENT_DATA_SIZE (num, 1)));
}

/* Get a new segment. Note that 0 is an allowed length,
 * and will result in a segment with no allocated data segment.
 * the caller should immediately assign data to the segment, as the segment
 * otherwise is not generally valid
 */
static struct assegment *
assegment_new (u_char type, u_short length)
{
  struct assegment *new;
  
  new = XCALLOC (MTYPE_AS_SEG, sizeof (struct assegment));
  
  if (length)
    new->as = assegment_data_new (length);
  
  new->length = length;
  new->type = type;
  
  return new;
}

static void
assegment_free (struct assegment *seg)
{
  if (!seg)
    return;
  
  if (seg->as)
    XFREE (MTYPE_AS_SEG_DATA, seg->as);
  memset (seg, 0xfe, sizeof(struct assegment));
  XFREE (MTYPE_AS_SEG, seg);
  
  return;
}

/* free entire chain of segments */
static void
assegment_free_all (struct assegment *seg)
{
  struct assegment *prev;
  
  while (seg)
    {
      prev = seg;
      seg = seg->next;
      assegment_free (prev);
    }
}

/* Duplicate just the given assegment and its data */
static struct assegment *
assegment_dup (struct assegment *seg)
{
  struct assegment *new;
  
  new = assegment_new (seg->type, seg->length);
  memcpy (new->as, seg->as, ASSEGMENT_DATA_SIZE (new->length, 1) );
    
  return new;
}

/* Duplicate entire chain of assegments, return the head */
static struct assegment *
assegment_dup_all (struct assegment *seg)
{
  struct assegment *new = NULL;
  struct assegment *head = NULL;
  
  while (seg)
    {
      if (head)
        {
          new->next = assegment_dup (seg);
          new = new->next;
        }
      else
        head = new = assegment_dup (seg);
      
      seg = seg->next;
    }
  return head;
}

/* prepend the as number to given segment, given num of times */
static struct assegment *
assegment_prepend_asns (struct assegment *seg, as_t asnum, int num)
{
  as_t *newas;
  int i;
  
  if (!num)
    return seg;
  
  if (num >= AS_SEGMENT_MAX)
    return seg; /* we don't do huge prepends */
  
  newas = assegment_data_new (seg->length + num);

  for (i = 0; i < num; i++)
    newas[i] = asnum;

  memcpy (newas + num, seg->as, ASSEGMENT_DATA_SIZE (seg->length, 1));
  XFREE (MTYPE_AS_SEG_DATA, seg->as);
  seg->as = newas;
  seg->length += num;

  return seg;
}

/* append given array of as numbers to the segment */
static struct assegment *
assegment_append_asns (struct assegment *seg, as_t *asnos, int num)
{
  as_t *newas;
  
  newas = XREALLOC (MTYPE_AS_SEG_DATA, seg->as,
		      ASSEGMENT_DATA_SIZE (seg->length + num, 1));

  if (newas)
    {
      seg->as = newas;
      memcpy (seg->as + seg->length, asnos, ASSEGMENT_DATA_SIZE(num, 1));
      seg->length += num;
      return seg;
    }

  assegment_free_all (seg);
  return NULL;
}

static int
int_cmp (const void *p1, const void *p2)
{
  const as_t *as1 = p1;
  const as_t *as2 = p2;
  
  return (*as1 == *as2) 
          ? 0 : ( (*as1 > *as2) ? 1 : -1);
}

/* normalise the segment.
 * In particular, merge runs of AS_SEQUENCEs into one segment
 * Internally, we do not care about the wire segment length limit, and
 * we want each distinct AS_PATHs to have the exact same internal
 * representation - eg, so that our hashing actually works..
 */
static struct assegment *
assegment_normalise (struct assegment *head)
{
  struct assegment *seg = head, *pin;
  struct assegment *tmp;
  
  if (!head)
    return head;
  
  while (seg)
    {
      pin = seg;
      
      /* Sort values SET segments, for determinism in paths to aid
       * creation of hash values / path comparisons
       * and because it helps other lesser implementations ;)
       */
      if (seg->type == AS_SET || seg->type == AS_CONFED_SET)
      	{
	  int tail = 0;
	  int i;
	  
	  qsort (seg->as, seg->length, sizeof(as_t), int_cmp);
	  
	  /* weed out dupes */
	  for (i=1; i < seg->length; i++)
	    {
	      if (seg->as[tail] == seg->as[i])
	      	continue;
	      
	      tail++;
	      if (tail < i)
	      	seg->as[tail] = seg->as[i];
	    }
	  /* seg->length can be 0.. */
	  if (seg->length)
	    seg->length = tail + 1;
	}

      /* read ahead from the current, pinned segment while the segments
       * are packable/mergeable. Append all following packable segments
       * to the segment we have pinned and remove these appended
       * segments.
       */      
      while (pin->next && ASSEGMENT_TYPES_PACKABLE(pin, pin->next))
        {
          tmp = pin->next;
          seg = pin->next;
          
          /* append the next sequence to the pinned sequence */
          pin = assegment_append_asns (pin, seg->as, seg->length);
          
          /* bypass the next sequence */
          pin->next = seg->next;
          
          /* get rid of the now referenceless segment */
          assegment_free (tmp);
          
        }

      seg = pin->next;
    }
  return head;
}

static struct aspath *
aspath_new (void)
{
  return XCALLOC (MTYPE_AS_PATH, sizeof (struct aspath));
}

/* Free AS path structure. */
void
aspath_free (struct aspath *aspath)
{
  if (!aspath)
    return;
  if (aspath->segments)
    assegment_free_all (aspath->segments);
  if (aspath->str)
    XFREE (MTYPE_AS_STR, aspath->str);
  XFREE (MTYPE_AS_PATH, aspath);
}

/* Unintern aspath from AS path bucket. */
void
aspath_unintern (struct aspath **aspath)
{
  struct aspath *ret;
  struct aspath *asp = *aspath;
  
  if (asp->refcnt)
    asp->refcnt--;

  if (asp->refcnt == 0)
    {
      /* This aspath must exist in aspath hash table. */
      ret = hash_release (ashash, asp);
      assert (ret != NULL);
      aspath_free (asp);
      *aspath = NULL;
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

/* countup asns from this segment and index onward */
static int
assegment_count_asns (struct assegment *seg, int from)
{
  int count = 0;
  while (seg)
    {
      if (!from)
        count += seg->length;
      else
        {
          count += (seg->length - from);
          from = 0;
        }
      seg = seg->next;
    }
  return count;
}

unsigned int
aspath_count_confeds (struct aspath *aspath)
{
  int count = 0;
  struct assegment *seg = aspath->segments;
  
  while (seg)
    {
      if (seg->type == AS_CONFED_SEQUENCE)
        count += seg->length;
      else if (seg->type == AS_CONFED_SET)
        count++;
      
      seg = seg->next;
    }
  return count;
}

unsigned int
aspath_count_hops (struct aspath *aspath)
{
  int count = 0;
  struct assegment *seg = aspath->segments;
  
  while (seg)
    {
      if (seg->type == AS_SEQUENCE)
        count += seg->length;
      else if (seg->type == AS_SET)
        count++;
      
      seg = seg->next;
    }
  return count;
}

/* Estimate size aspath /might/ take if encoded into an
 * ASPATH attribute.
 *
 * This is a quick estimate, not definitive! aspath_put()
 * may return a different number!!
 */
unsigned int
aspath_size (struct aspath *aspath)
{
  int size = 0;
  struct assegment *seg = aspath->segments;
  
  while (seg)
    {
      size += ASSEGMENT_SIZE(seg->length, 1);
      seg = seg->next;
    }
  return size;
}

/* Return highest public ASN in path */
as_t
aspath_highest (struct aspath *aspath)
{
  struct assegment *seg = aspath->segments;
  as_t highest = 0;
  unsigned int i;
  
  while (seg)
    {
      for (i = 0; i < seg->length; i++)
        if (seg->as[i] > highest
            && (seg->as[i] < BGP_PRIVATE_AS_MIN
                || seg->as[i] > BGP_PRIVATE_AS_MAX))
	  highest = seg->as[i];
      seg = seg->next;
    }
  return highest;
}

/* Return the left-most ASN in path */
as_t
aspath_leftmost (struct aspath *aspath)
{
  struct assegment *seg = aspath->segments;
  as_t leftmost = 0;

  if (seg && seg->length && seg->type == AS_SEQUENCE)
    leftmost = seg->as[0];

  return leftmost;
}

/* Return 1 if there are any 4-byte ASes in the path */
unsigned int
aspath_has_as4 (struct aspath *aspath)
{
  struct assegment *seg = aspath->segments;
  unsigned int i;
  
  while (seg)
    {
      for (i = 0; i < seg->length; i++)
        if (seg->as[i] > BGP_AS_MAX)
	  return 1;
      seg = seg->next;
    }
  return 0;
}

/* Convert aspath structure to string expression. */
static void
aspath_make_str_count (struct aspath *as)
{
  struct assegment *seg;
  int str_size;
  int len = 0;
  char *str_buf;

  /* Empty aspath. */
  if (!as->segments)
    {
      as->str = XMALLOC (MTYPE_AS_STR, 1);
      as->str[0] = '\0';
      as->str_len = 0;
      return;
    }
  
  seg = as->segments;
  
  /* ASN takes 5 to 10 chars plus seperator, see below.
   * If there is one differing segment type, we need an additional
   * 2 chars for segment delimiters, and the final '\0'.
   * Hopefully this is large enough to avoid hitting the realloc
   * code below for most common sequences.
   *
   * This was changed to 10 after the well-known BGP assertion, which
   * had hit some parts of the Internet in May of 2009.
   */
#define ASN_STR_LEN (10 + 1)
  str_size = MAX (assegment_count_asns (seg, 0) * ASN_STR_LEN + 2 + 1,
                  ASPATH_STR_DEFAULT_LEN);
  str_buf = XMALLOC (MTYPE_AS_STR, str_size);

  while (seg)
    {
      int i;
      char seperator;
      
      /* Check AS type validity. Set seperator for segment */
      switch (seg->type)
        {
          case AS_SET:
          case AS_CONFED_SET:
            seperator = ',';
            break;
          case AS_SEQUENCE:
          case AS_CONFED_SEQUENCE:
            seperator = ' ';
            break;
          default:
            XFREE (MTYPE_AS_STR, str_buf);
            as->str = NULL;
            as->str_len = 0;
            return;
        }
      
      /* We might need to increase str_buf, particularly if path has
       * differing segments types, our initial guesstimate above will
       * have been wrong. Need 10 chars for ASN, a seperator each and
       * potentially two segment delimiters, plus a space between each
       * segment and trailing zero.
       *
       * This definitely didn't work with the value of 5 bytes and
       * 32-bit ASNs.
       */
#define SEGMENT_STR_LEN(X) (((X)->length * ASN_STR_LEN) + 2 + 1 + 1)
      if ( (len + SEGMENT_STR_LEN(seg)) > str_size)
        {
          str_size = len + SEGMENT_STR_LEN(seg);
          str_buf = XREALLOC (MTYPE_AS_STR, str_buf, str_size);
        }
#undef ASN_STR_LEN
#undef SEGMENT_STR_LEN
      
      if (seg->type != AS_SEQUENCE)
        len += snprintf (str_buf + len, str_size - len, 
			 "%c", 
                         aspath_delimiter_char (seg->type, AS_SEG_START));
      
      /* write out the ASNs, with their seperators, bar the last one*/
      for (i = 0; i < seg->length; i++)
        {
          len += snprintf (str_buf + len, str_size - len, "%u", seg->as[i]);
          
          if (i < (seg->length - 1))
            len += snprintf (str_buf + len, str_size - len, "%c", seperator);
        }
      
      if (seg->type != AS_SEQUENCE)
        len += snprintf (str_buf + len, str_size - len, "%c", 
                        aspath_delimiter_char (seg->type, AS_SEG_END));
      if (seg->next)
        len += snprintf (str_buf + len, str_size - len, " ");
      
      seg = seg->next;
    }
  
  assert (len < str_size);
  
  str_buf[len] = '\0';
  as->str = str_buf;
  as->str_len = len;

  return;
}

static void
aspath_str_update (struct aspath *as)
{
  if (as->str)
    XFREE (MTYPE_AS_STR, as->str);
  aspath_make_str_count (as);
}

/* Intern allocated AS path. */
struct aspath *
aspath_intern (struct aspath *aspath)
{
  struct aspath *find;

  /* Assert this AS path structure is not interned and has the string
     representation built. */
  assert (aspath->refcnt == 0);
  assert (aspath->str);

  /* Check AS path hash. */
  find = hash_get (ashash, aspath, hash_alloc_intern);
  if (find != aspath)
    aspath_free (aspath);

  find->refcnt++;

  return find;
}

/* Duplicate aspath structure.  Created same aspath structure but
   reference count and AS path string is cleared. */
struct aspath *
aspath_dup (struct aspath *aspath)
{
  unsigned short buflen = aspath->str_len + 1;
  struct aspath *new;

  new = XCALLOC (MTYPE_AS_PATH, sizeof (struct aspath));

  if (aspath->segments)
    new->segments = assegment_dup_all (aspath->segments);

  if (!aspath->str)
    return new;

  new->str = XMALLOC (MTYPE_AS_STR, buflen);
  new->str_len = aspath->str_len;

  /* copy the string data */
  if (aspath->str_len > 0)
    memcpy (new->str, aspath->str, buflen);
  else
    new->str[0] = '\0';

  return new;
}

static void *
aspath_hash_alloc (void *arg)
{
  const struct aspath *aspath = arg;
  struct aspath *new;

  /* Malformed AS path value. */
  assert (aspath->str);
  if (! aspath->str)
    return NULL;

  /* New aspath structure is needed. */
  new = XMALLOC (MTYPE_AS_PATH, sizeof (struct aspath));

  /* Reuse segments and string representation */
  new->refcnt = 0;
  new->segments = aspath->segments;
  new->str = aspath->str;
  new->str_len = aspath->str_len;

  return new;
}

/* parse as-segment byte stream in struct assegment */
static int
assegments_parse (struct stream *s, size_t length, 
                  struct assegment **result, int use32bit)
{
  struct assegment_header segh;
  struct assegment *seg, *prev = NULL, *head = NULL;
  size_t bytes = 0;
  
  /* empty aspath (ie iBGP or somesuch) */
  if (length == 0)
    return 0;
  
  if (BGP_DEBUG (as4, AS4_SEGMENT))
    zlog_debug ("[AS4SEG] Parse aspath segment: got total byte length %lu",
		(unsigned long) length);
  /* basic checks */
  if ((STREAM_READABLE(s) < length)
      || (STREAM_READABLE(s) < AS_HEADER_SIZE) 
      || (length % AS16_VALUE_SIZE ))
    return -1;
  
  while (bytes < length)
    {
      int i;
      size_t seg_size;
      
      if ((length - bytes) <= AS_HEADER_SIZE)
        {
          if (head)
            assegment_free_all (head);
          return -1;
        }
      
      /* softly softly, get the header first on its own */
      segh.type = stream_getc (s);
      segh.length = stream_getc (s);
      
      seg_size = ASSEGMENT_SIZE(segh.length, use32bit);

      if (BGP_DEBUG (as4, AS4_SEGMENT))
	zlog_debug ("[AS4SEG] Parse aspath segment: got type %d, length %d",
                    segh.type, segh.length);
      
      /* check it.. */
      if ( ((bytes + seg_size) > length)
          /* 1771bis 4.3b: seg length contains one or more */
          || (segh.length == 0) 
          /* Paranoia in case someone changes type of segment length.
           * Shift both values by 0x10 to make the comparison operate
           * on more, than 8 bits (otherwise it's a warning, bug #564).
           */
          || ((sizeof segh.length > 1) 
              && (0x10 + segh.length > 0x10 + AS_SEGMENT_MAX)))
        {
          if (head)
            assegment_free_all (head);
          return -1;
        }
      
      switch (segh.type)
        {
          case AS_SEQUENCE:
          case AS_SET:
          case AS_CONFED_SEQUENCE:
          case AS_CONFED_SET:
            break;
          default:
            if (head)
              assegment_free_all (head);
            return -1;
        }
      
      /* now its safe to trust lengths */
      seg = assegment_new (segh.type, segh.length);
      
      if (head)
        prev->next = seg;
      else /* it's the first segment */
        head = prev = seg;
      
      for (i = 0; i < segh.length; i++)
	seg->as[i] = (use32bit) ? stream_getl (s) : stream_getw (s);

      bytes += seg_size;
      
      if (BGP_DEBUG (as4, AS4_SEGMENT))
	zlog_debug ("[AS4SEG] Parse aspath segment: Bytes now: %lu",
	            (unsigned long) bytes);
      
      prev = seg;
    }
 
  *result = assegment_normalise (head);
  return 0;
}

/* AS path parse function.  pnt is a pointer to byte stream and length
   is length of byte stream.  If there is same AS path in the the AS
   path hash then return it else make new AS path structure. 
   
   On error NULL is returned.
 */
struct aspath *
aspath_parse (struct stream *s, size_t length, int use32bit)
{
  struct aspath as;
  struct aspath *find;

  /* If length is odd it's malformed AS path. */
  /* Nit-picking: if (use32bit == 0) it is malformed if odd,
   * otherwise its malformed when length is larger than 2 and (length-2) 
   * is not dividable by 4.
   * But... this time we're lazy
   */
  if (length % AS16_VALUE_SIZE )
    return NULL;

  memset (&as, 0, sizeof (struct aspath));
  if (assegments_parse (s, length, &as.segments, use32bit) < 0)
    return NULL;

  /* If already same aspath exist then return it. */
  find = hash_get (ashash, &as, aspath_hash_alloc);

  /* bug! should not happen, let the daemon crash below */
  assert (find);

  /* if the aspath was already hashed free temporary memory. */
  if (find->refcnt)
    {
      assegment_free_all (as.segments);
      /* aspath_key_make() always updates the string */
      XFREE (MTYPE_AS_STR, as.str);
    }

  find->refcnt++;

  return find;
}

static void
assegment_data_put (struct stream *s, as_t *as, int num, int use32bit)
{
  int i;
  assert (num <= AS_SEGMENT_MAX);
  
  for (i = 0; i < num; i++)
    if ( use32bit )
      stream_putl (s, as[i]);
    else
      {
        if ( as[i] <= BGP_AS_MAX )
	  stream_putw(s, as[i]);
	else
	  stream_putw(s, BGP_AS_TRANS);
      }
}

static size_t
assegment_header_put (struct stream *s, u_char type, int length)
{
  size_t lenp;
  assert (length <= AS_SEGMENT_MAX);
  stream_putc (s, type);
  lenp = stream_get_endp (s);
  stream_putc (s, length);
  return lenp;
}

/* write aspath data to stream */
size_t
aspath_put (struct stream *s, struct aspath *as, int use32bit )
{
  struct assegment *seg = as->segments;
  size_t bytes = 0;
  
  if (!seg || seg->length == 0)
    return 0;
  
  if (seg)
    {
      /*
       * Hey, what do we do when we have > STREAM_WRITABLE(s) here?
       * At the moment, we would write out a partial aspath, and our peer
       * will complain and drop the session :-/
       *
       * The general assumption here is that many things tested will
       * never happen.  And, in real live, up to now, they have not.
       */
      while (seg && (ASSEGMENT_LEN(seg, use32bit) <= STREAM_WRITEABLE(s)))
        {
          struct assegment *next = seg->next;
          int written = 0;
          int asns_packed = 0;
          size_t lenp;
          
          /* Overlength segments have to be split up */
          while ( (seg->length - written) > AS_SEGMENT_MAX)
            {
              assegment_header_put (s, seg->type, AS_SEGMENT_MAX);
              assegment_data_put (s, seg->as, AS_SEGMENT_MAX, use32bit);
              written += AS_SEGMENT_MAX;
              bytes += ASSEGMENT_SIZE (written, use32bit);
            }
          
          /* write the final segment, probably is also the first */
          lenp = assegment_header_put (s, seg->type, seg->length - written);
          assegment_data_put (s, (seg->as + written), seg->length - written, 
                              use32bit);
          
          /* Sequence-type segments can be 'packed' together
           * Case of a segment which was overlength and split up
           * will be missed here, but that doesn't matter.
           */
          while (next && ASSEGMENTS_PACKABLE (seg, next))
            {
              /* NB: We should never normally get here given we
               * normalise aspath data when parse them. However, better
               * safe than sorry. We potentially could call
               * assegment_normalise here instead, but it's cheaper and
               * easier to do it on the fly here rather than go through
               * the segment list twice every time we write out
               * aspath's.
               */
              
              /* Next segment's data can fit in this one */
              assegment_data_put (s, next->as, next->length, use32bit);
              
              /* update the length of the segment header */
	      stream_putc_at (s, lenp, seg->length - written + next->length);
              asns_packed += next->length;
               
	      next = next->next;
	    }
          
          bytes += ASSEGMENT_SIZE (seg->length - written + asns_packed, 
				   use32bit);
          seg = next;
        }
    }
  return bytes;
}

/* This is for SNMP BGP4PATHATTRASPATHSEGMENT
 * We have no way to manage the storage, so we use a static stream
 * wrapper around aspath_put.
 */
u_char *
aspath_snmp_pathseg (struct aspath *as, size_t *varlen)
{
#define SNMP_PATHSEG_MAX 1024

  if (!snmp_stream)
    snmp_stream = stream_new (SNMP_PATHSEG_MAX);
  else
    stream_reset (snmp_stream);
  
  if (!as)
    {
      *varlen = 0;
      return NULL;
    }
  aspath_put (snmp_stream, as, 0); /* use 16 bit for now here */
  
  *varlen = stream_get_endp (snmp_stream);
  return stream_pnt(snmp_stream);
}
      
#define min(A,B) ((A) < (B) ? (A) : (B))

static struct assegment *
aspath_aggregate_as_set_add (struct aspath *aspath, struct assegment *asset,
			     as_t as)
{
  int i;

  /* If this is first AS set member, create new as-set segment. */
  if (asset == NULL)
    {
      asset = assegment_new (AS_SET, 1);
      if (! aspath->segments)
	aspath->segments = asset;
      else
        {
          struct assegment *seg = aspath->segments;
          while (seg->next)
            seg = seg->next;
          seg->next = asset;
        }
      asset->type = AS_SET;
      asset->length = 1;
      asset->as[0] = as;
    }
  else
    {
      /* Check this AS value already exists or not. */
      for (i = 0; i < asset->length; i++)
	if (asset->as[i] == as)
	  return asset;
      
      asset->length++;
      asset->as = XREALLOC (MTYPE_AS_SEG_DATA, asset->as, 
                            asset->length * AS_VALUE_SIZE);
      asset->as[asset->length - 1] = as;
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
  int from;
  struct assegment *seg1 = as1->segments;
  struct assegment *seg2 = as2->segments;
  struct aspath *aspath = NULL;
  struct assegment *asset;
  struct assegment *prevseg = NULL;

  match = 0;
  minlen = 0;
  aspath = NULL;
  asset = NULL;

  /* First of all check common leading sequence. */
  while (seg1 && seg2)
    {      
      /* Check segment type. */
      if (seg1->type != seg2->type)
	break;

      /* Minimum segment length. */
      minlen = min (seg1->length, seg2->length);

      for (match = 0; match < minlen; match++)
	if (seg1->as[match] != seg2->as[match])
	  break;

      if (match)
	{
	  struct assegment *seg = assegment_new (seg1->type, 0);
	  
	  seg = assegment_append_asns (seg, seg1->as, match);

	  if (! aspath)
	    {
	      aspath = aspath_new ();
	      aspath->segments = seg;
	     }
	  else
	    prevseg->next = seg;
	  
	  prevseg = seg;
	}

      if (match != minlen || match != seg1->length 
	  || seg1->length != seg2->length)
	break;
      
      seg1 = seg1->next;
      seg2 = seg2->next;
    }

  if (! aspath)
    aspath = aspath_new();

  /* Make as-set using rest of all information. */
  from = match;
  while (seg1)
    {
      for (i = from; i < seg1->length; i++)
	asset = aspath_aggregate_as_set_add (aspath, asset, seg1->as[i]);
      
      from = 0;
      seg1 = seg1->next;
    }

  from = match;
  while (seg2)
    {
      for (i = from; i < seg2->length; i++)
	asset = aspath_aggregate_as_set_add (aspath, asset, seg2->as[i]);

      from = 0;
      seg2 = seg2->next;
    }
  
  assegment_normalise (aspath->segments);
  aspath_str_update (aspath);
  return aspath;
}

/* When a BGP router receives an UPDATE with an MP_REACH_NLRI
   attribute, check the leftmost AS number in the AS_PATH attribute is
   or not the peer's AS number. */ 
int
aspath_firstas_check (struct aspath *aspath, as_t asno)
{
  if ( (aspath == NULL) || (aspath->segments == NULL) )
    return 0;
  
  if (aspath->segments
      && (aspath->segments->type == AS_SEQUENCE)
      && (aspath->segments->as[0] == asno ))
    return 1;

  return 0;
}

/* AS path loop check.  If aspath contains asno then return >= 1. */
int
aspath_loop_check (struct aspath *aspath, as_t asno)
{
  struct assegment *seg;
  int count = 0;

  if ( (aspath == NULL) || (aspath->segments == NULL) )
    return 0;
  
  seg = aspath->segments;
  
  while (seg)
    {
      int i;
      
      for (i = 0; i < seg->length; i++)
	if (seg->as[i] == asno)
	  count++;
      
      seg = seg->next;
    }
  return count;
}

/* When all of AS path is private AS return 1.  */
int
aspath_private_as_check (struct aspath *aspath)
{
  struct assegment *seg;
  
  if ( !(aspath && aspath->segments) )
    return 0;
    
  seg = aspath->segments;

  while (seg)
    {
      int i;
      
      for (i = 0; i < seg->length; i++)
	{
	  if ( (seg->as[i] < BGP_PRIVATE_AS_MIN)
	      || (seg->as[i] > BGP_PRIVATE_AS_MAX) )
	    return 0;
	}
      seg = seg->next;
    }
  return 1;
}

/* AS path confed check.  If aspath contains confed set or sequence then return 1. */
int
aspath_confed_check (struct aspath *aspath)
{
  struct assegment *seg;

  if ( !(aspath && aspath->segments) )
    return 0;

  seg = aspath->segments;

  while (seg)
    {
      if (seg->type == AS_CONFED_SET || seg->type == AS_CONFED_SEQUENCE)
	  return 1;
      seg = seg->next;
    }
  return 0;
}

/* Leftmost AS path segment confed check.  If leftmost AS segment is of type
  AS_CONFED_SEQUENCE or AS_CONFED_SET then return 1.  */
int
aspath_left_confed_check (struct aspath *aspath)
{

  if ( !(aspath && aspath->segments) )
    return 0;

  if ( (aspath->segments->type == AS_CONFED_SEQUENCE)
      || (aspath->segments->type == AS_CONFED_SET) )
    return 1;

  return 0;
}

/* Merge as1 to as2.  as2 should be uninterned aspath. */
static struct aspath *
aspath_merge (struct aspath *as1, struct aspath *as2)
{
  struct assegment *last, *new;

  if (! as1 || ! as2)
    return NULL;

  last = new = assegment_dup_all (as1->segments);
  
  /* find the last valid segment */
  while (last && last->next)
    last = last->next;
  
  last->next = as2->segments;
  as2->segments = new;
  aspath_str_update (as2);
  return as2;
}

/* Prepend as1 to as2.  as2 should be uninterned aspath. */
struct aspath *
aspath_prepend (struct aspath *as1, struct aspath *as2)
{
  struct assegment *seg1;
  struct assegment *seg2;

  if (! as1 || ! as2)
    return NULL;
  
  seg1 = as1->segments;
  seg2 = as2->segments;
  
  /* If as2 is empty, only need to dupe as1's chain onto as2 */
  if (seg2 == NULL)
    {
      as2->segments = assegment_dup_all (as1->segments);
      aspath_str_update (as2);
      return as2;
    }
  
  /* If as1 is empty AS, no prepending to do. */
  if (seg1 == NULL)
    return as2;
  
  /* find the tail as1's segment chain. */
  while (seg1 && seg1->next)
    seg1 = seg1->next;

  /* Delete any AS_CONFED_SEQUENCE segment from as2. */
  if (seg1->type == AS_SEQUENCE && seg2->type == AS_CONFED_SEQUENCE)
    as2 = aspath_delete_confed_seq (as2);

  /* Compare last segment type of as1 and first segment type of as2. */
  if (seg1->type != seg2->type)
    return aspath_merge (as1, as2);

  if (seg1->type == AS_SEQUENCE)
    {
      /* We have two chains of segments, as1->segments and seg2, 
       * and we have to attach them together, merging the attaching
       * segments together into one.
       * 
       * 1. dupe as1->segments onto head of as2
       * 2. merge seg2's asns onto last segment of this new chain
       * 3. attach chain after seg2
       */
      
      /* dupe as1 onto as2's head */
      seg1 = as2->segments = assegment_dup_all (as1->segments);
      
      /* refind the tail of as2, reusing seg1 */
      while (seg1 && seg1->next)
        seg1 = seg1->next;
      
      /* merge the old head, seg2, into tail, seg1 */
      seg1 = assegment_append_asns (seg1, seg2->as, seg2->length);
      
      /* bypass the merged seg2, and attach any chain after it to
       * chain descending from as2's head
       */
      seg1->next = seg2->next;
      
      /* seg2 is now referenceless and useless*/
      assegment_free (seg2);
      
      /* we've now prepended as1's segment chain to as2, merging
       * the inbetween AS_SEQUENCE of seg2 in the process 
       */
      aspath_str_update (as2);
      return as2;
    }
  else
    {
      /* AS_SET merge code is needed at here. */
      return aspath_merge (as1, as2);
    }
  /* XXX: Ermmm, what if as1 has multiple segments?? */
  
  /* Not reached */
}

/* Iterate over AS_PATH segments and wipe all occurences of the
 * listed AS numbers. Hence some segments may lose some or even
 * all data on the way, the operation is implemented as a smarter
 * version of aspath_dup(), which allocates memory to hold the new
 * data, not the original. The new AS path is returned.
 */
struct aspath *
aspath_filter_exclude (struct aspath * source, struct aspath * exclude_list)
{
  struct assegment * srcseg, * exclseg, * lastseg;
  struct aspath * newpath;

  newpath = aspath_new();
  lastseg = NULL;

  for (srcseg = source->segments; srcseg; srcseg = srcseg->next)
  {
    unsigned i, y, newlen = 0, done = 0, skip_as;
    struct assegment * newseg;

    /* Find out, how much ASns are we going to pick from this segment.
     * We can't perform filtering right inline, because the size of
     * the new segment isn't known at the moment yet.
     */
    for (i = 0; i < srcseg->length; i++)
    {
      skip_as = 0;
      for (exclseg = exclude_list->segments; exclseg && !skip_as; exclseg = exclseg->next)
        for (y = 0; y < exclseg->length; y++)
          if (srcseg->as[i] == exclseg->as[y])
          {
            skip_as = 1;
            // There's no sense in testing the rest of exclusion list, bail out.
            break;
          }
      if (!skip_as)
        newlen++;
    }
    /* newlen is now the number of ASns to copy */
    if (!newlen)
      continue;

    /* Actual copying. Allocate memory and iterate once more, performing filtering. */
    newseg = assegment_new (srcseg->type, newlen);
    for (i = 0; i < srcseg->length; i++)
    {
      skip_as = 0;
      for (exclseg = exclude_list->segments; exclseg && !skip_as; exclseg = exclseg->next)
        for (y = 0; y < exclseg->length; y++)
          if (srcseg->as[i] == exclseg->as[y])
          {
            skip_as = 1;
            break;
          }
      if (skip_as)
        continue;
      newseg->as[done++] = srcseg->as[i];
    }
    /* At his point newlen must be equal to done, and both must be positive. Append
     * the filtered segment to the gross result. */
    if (!lastseg)
      newpath->segments = newseg;
    else
      lastseg->next = newseg;
    lastseg = newseg;
  }
  aspath_str_update (newpath);
  /* We are happy returning even an empty AS_PATH, because the administrator
   * might expect this very behaviour. There's a mean to avoid this, if necessary,
   * by having a match rule against certain AS_PATH regexps in the route-map index.
   */
  aspath_free (source);
  return newpath;
}

/* Add specified AS to the leftmost of aspath. */
static struct aspath *
aspath_add_asns (struct aspath *aspath, as_t asno, u_char type, unsigned num)
{
  struct assegment *assegment = aspath->segments;
  int i;

  if (assegment && assegment->type == type)
    {
      /* extend existing segment */
      aspath->segments = assegment_prepend_asns (aspath->segments, asno, num);
    }
  else 
    {
      /* prepend with new segment */
      struct assegment *newsegment = assegment_new (type, num);
      for (i = 0; i < num; i++)
	newsegment->as[i] = asno;

      /* insert potentially replacing empty segment */
      if (assegment && assegment->length == 0)
	{
	  newsegment->next = assegment->next;
	  assegment_free (assegment);
	}
       else
	  newsegment->next = assegment;
      aspath->segments = newsegment;
    }

  aspath_str_update (aspath);
  return aspath;
}

/* Add specified AS to the leftmost of aspath num times. */
struct aspath *
aspath_add_seq_n (struct aspath *aspath, as_t asno, unsigned num)
{
  return aspath_add_asns (aspath, asno, AS_SEQUENCE, num);
}

/* Add specified AS to the leftmost of aspath. */
struct aspath *
aspath_add_seq (struct aspath *aspath, as_t asno)
{
  return aspath_add_asns (aspath, asno, AS_SEQUENCE, 1);
}

/* Compare leftmost AS value for MED check.  If as1's leftmost AS and
   as2's leftmost AS is same return 1. */
int
aspath_cmp_left (const struct aspath *aspath1, const struct aspath *aspath2)
{
  const struct assegment *seg1;
  const struct assegment *seg2;

  if (!(aspath1 && aspath2))
    return 0;

  seg1 = aspath1->segments;
  seg2 = aspath2->segments;

  /* find first non-confed segments for each */
  while (seg1 && ((seg1->type == AS_CONFED_SEQUENCE)
		  || (seg1->type == AS_CONFED_SET)))
    seg1 = seg1->next;

  while (seg2 && ((seg2->type == AS_CONFED_SEQUENCE)
		  || (seg2->type == AS_CONFED_SET)))
    seg2 = seg2->next;

  /* Check as1's */
  if (!(seg1 && seg2
	&& (seg1->type == AS_SEQUENCE) && (seg2->type == AS_SEQUENCE)))
    return 0;
  
  if (seg1->as[0] == seg2->as[0])
    return 1;

  return 0;
}

/* Truncate an aspath after a number of hops, and put the hops remaining
 * at the front of another aspath.  Needed for AS4 compat.
 *
 * Returned aspath is a /new/ aspath, which should either by free'd or
 * interned by the caller, as desired.
 */
struct aspath *
aspath_reconcile_as4 ( struct aspath *aspath, struct aspath *as4path)
{
  struct assegment *seg, *newseg, *prevseg = NULL;
  struct aspath *newpath = NULL, *mergedpath;
  int hops, cpasns = 0;
  
  if (!aspath)
    return NULL;
  
  seg = aspath->segments;
  
  /* CONFEDs should get reconciled too.. */
  hops = (aspath_count_hops (aspath) + aspath_count_confeds (aspath))
         - aspath_count_hops (as4path);
  
  if (hops < 0)
    {
      if (BGP_DEBUG (as4, AS4))
        zlog_warn ("[AS4] Fewer hops in AS_PATH than NEW_AS_PATH");
      /* Something's gone wrong. The RFC says we should now ignore AS4_PATH,
       * which is daft behaviour - it contains vital loop-detection
       * information which must have been removed from AS_PATH.
       */
       hops = aspath_count_hops (aspath);
    }
  
  if (!hops)
   return aspath_dup (as4path);
  
  if ( BGP_DEBUG(as4, AS4))
    zlog_debug("[AS4] got AS_PATH %s and AS4_PATH %s synthesizing now",
               aspath->str, as4path->str);

  while (seg && hops > 0)
    {
      switch (seg->type)
        {
          case AS_SET:
          case AS_CONFED_SET:
            hops--;
            cpasns = seg->length;
            break;
          case AS_CONFED_SEQUENCE:
	    /* Should never split a confed-sequence, if hop-count
	     * suggests we must then something's gone wrong somewhere.
	     *
	     * Most important goal is to preserve AS_PATHs prime function
	     * as loop-detector, so we fudge the numbers so that the entire
	     * confed-sequence is merged in.
	     */
	    if (hops < seg->length)
	      {
	        if (BGP_DEBUG (as4, AS4))
	          zlog_debug ("[AS4] AS4PATHmangle: AS_CONFED_SEQUENCE falls"
	                      " across 2/4 ASN boundary somewhere, broken..");
	        hops = seg->length;
	      }
	  case AS_SEQUENCE:
	    cpasns = MIN(seg->length, hops);
	    hops -= seg->length;
	}
      
      assert (cpasns <= seg->length);
      
      newseg = assegment_new (seg->type, 0);
      newseg = assegment_append_asns (newseg, seg->as, cpasns);

      if (!newpath)
        {
          newpath = aspath_new ();
          newpath->segments = newseg;
        }
      else
        prevseg->next = newseg;

      prevseg = newseg;
      seg = seg->next;
    }
    
  /* We may be able to join some segments here, and we must
   * do this because... we want normalised aspaths in out hash
   * and we do not want to stumble in aspath_put.
   */
  mergedpath = aspath_merge (newpath, aspath_dup(as4path));
  aspath_free (newpath);
  mergedpath->segments = assegment_normalise (mergedpath->segments);
  aspath_str_update (mergedpath);
  
  if ( BGP_DEBUG(as4, AS4))
    zlog_debug ("[AS4] result of synthesizing is %s",
                mergedpath->str);
  
  return mergedpath;
}

/* Compare leftmost AS value for MED check.  If as1's leftmost AS and
   as2's leftmost AS is same return 1. (confederation as-path
   only).  */
int
aspath_cmp_left_confed (const struct aspath *aspath1, const struct aspath *aspath2)
{
  if (! (aspath1 && aspath2) )
    return 0;
  
  if ( !(aspath1->segments && aspath2->segments) )
    return 0;
  
  if ( (aspath1->segments->type != AS_CONFED_SEQUENCE)
      || (aspath2->segments->type != AS_CONFED_SEQUENCE) )
    return 0;
  
  if (aspath1->segments->as[0] == aspath2->segments->as[0])
    return 1;

  return 0;
}

/* Delete all leading AS_CONFED_SEQUENCE/SET segments from aspath.
 * See RFC3065, 6.1 c1 */
struct aspath *
aspath_delete_confed_seq (struct aspath *aspath)
{
  struct assegment *seg;

  if (!(aspath && aspath->segments))
    return aspath;

  seg = aspath->segments;
  
  /* "if the first path segment of the AS_PATH is 
   *  of type AS_CONFED_SEQUENCE,"
   */
  if (aspath->segments->type != AS_CONFED_SEQUENCE)
    return aspath;

  /* "... that segment and any immediately following segments 
   *  of the type AS_CONFED_SET or AS_CONFED_SEQUENCE are removed 
   *  from the AS_PATH attribute,"
   */
  while (seg && 
         (seg->type == AS_CONFED_SEQUENCE || seg->type == AS_CONFED_SET))
    {
      aspath->segments = seg->next;
      assegment_free (seg);
      seg = aspath->segments;
    }
  aspath_str_update (aspath);
  return aspath;
}

/* Add new AS number to the leftmost part of the aspath as
   AS_CONFED_SEQUENCE.  */
struct aspath*
aspath_add_confed_seq (struct aspath *aspath, as_t asno)
{
  return aspath_add_asns (aspath, asno, AS_CONFED_SEQUENCE, 1);
}

/* Add new as value to as path structure. */
static void
aspath_as_add (struct aspath *as, as_t asno)
{
  struct assegment *seg = as->segments;

  if (!seg)
    return;
  
  /* Last segment search procedure. */
  while (seg->next)
    seg = seg->next;

  assegment_append_asns (seg, &asno, 1);
}

/* Add new as segment to the as path. */
static void
aspath_segment_add (struct aspath *as, int type)
{
  struct assegment *seg = as->segments;
  struct assegment *new = assegment_new (type, 0);

  if (seg)
    {
      while (seg->next)
	seg = seg->next;
      seg->next = new;
    }
  else
    as->segments = new;
}

struct aspath *
aspath_empty (void)
{
  return aspath_parse (NULL, 0, 1); /* 32Bit ;-) */
}

struct aspath *
aspath_empty_get (void)
{
  struct aspath *aspath;

  aspath = aspath_new ();
  aspath_make_str_count (aspath);
  return aspath;
}

unsigned long
aspath_count (void)
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
  as_token_confed_seq_start,
  as_token_confed_seq_end,
  as_token_confed_set_start,
  as_token_confed_set_end,
  as_token_unknown
};

/* Return next token and point for string parse. */
static const char *
aspath_gettoken (const char *buf, enum as_token *token, u_long *asno)
{
  const char *p = buf;

  /* Skip seperators (space for sequences, ',' for sets). */
  while (isspace ((int) *p) || *p == ',')
    p++;

  /* Check the end of the string and type specify characters
     (e.g. {}()). */
  switch (*p)
    {
    case '\0':
      return NULL;
    case '{':
      *token = as_token_set_start;
      p++;
      return p;
    case '}':
      *token = as_token_set_end;
      p++;
      return p;
    case '(':
      *token = as_token_confed_seq_start;
      p++;
      return p;
    case ')':
      *token = as_token_confed_seq_end;
      p++;
      return p;
    case '[':
      *token = as_token_confed_set_start;
      p++;
      return p;
    case ']':
      *token = as_token_confed_set_end;
      p++;
      return p;
    }

  /* Check actual AS value. */
  if (isdigit ((int) *p)) 
    {
      as_t asval;
      
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
aspath_str2aspath (const char *str)
{
  enum as_token token = as_token_unknown;
  u_short as_type;
  u_long asno = 0;
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
	case as_token_confed_seq_start:
	  as_type = AS_CONFED_SEQUENCE;
	  aspath_segment_add (aspath, as_type);
	  needtype = 0;
	  break;
	case as_token_confed_seq_end:
	  as_type = AS_SEQUENCE;
	  needtype = 1;
	  break;
	case as_token_confed_set_start:
	  as_type = AS_CONFED_SET;
	  aspath_segment_add (aspath, as_type);
	  needtype = 0;
	  break;
	case as_token_confed_set_end:
	  as_type = AS_SEQUENCE;
	  needtype = 1;
	  break;
	case as_token_unknown:
	default:
	  aspath_free (aspath);
	  return NULL;
	}
    }

  aspath_make_str_count (aspath);

  return aspath;
}

/* Make hash value by raw aspath data. */
unsigned int
aspath_key_make (void *p)
{
  struct aspath *aspath = (struct aspath *) p;
  unsigned int key = 0;

  if (!aspath->str)
    aspath_str_update (aspath);

  key = jhash (aspath->str, aspath->str_len, 2334325);

  return key;
}

/* If two aspath have same value then return 1 else return 0 */
int
aspath_cmp (const void *arg1, const void *arg2)
{
  const struct assegment *seg1 = ((const struct aspath *)arg1)->segments;
  const struct assegment *seg2 = ((const struct aspath *)arg2)->segments;
  
  while (seg1 || seg2)
    {
      int i;
      if ((!seg1 && seg2) || (seg1 && !seg2))
	return 0;
      if (seg1->type != seg2->type)
        return 0;      
      if (seg1->length != seg2->length)
        return 0;
      for (i = 0; i < seg1->length; i++)
        if (seg1->as[i] != seg2->as[i])
          return 0;
      seg1 = seg1->next;
      seg2 = seg2->next;
    }
  return 1;
}

/* AS path hash initialize. */
void
aspath_init (void)
{
  ashash = hash_create_size (32768, aspath_key_make, aspath_cmp);
}

void
aspath_finish (void)
{
  hash_free (ashash);
  ashash = NULL;
  
  if (snmp_stream)
    stream_free (snmp_stream);
}

/* return and as path value */
const char *
aspath_print (struct aspath *as)
{
  return (as ? as->str : NULL);
}

/* Printing functions */
/* Feed the AS_PATH to the vty; the suffix string follows it only in case
 * AS_PATH wasn't empty.
 */
void
aspath_print_vty (struct vty *vty, const char *format, struct aspath *as, const char * suffix)
{
  assert (format);
  vty_out (vty, format, as->str);
  if (as->str_len && strlen (suffix))
    vty_out (vty, "%s", suffix);
}

static void
aspath_show_all_iterator (struct hash_backet *backet, struct vty *vty)
{
  struct aspath *as;

  as = (struct aspath *) backet->data;

  vty_out (vty, "[%p:%u] (%ld) ", backet, backet->key, as->refcnt);
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
