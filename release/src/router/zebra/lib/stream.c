/*
 * Packet interface
 * Copyright (C) 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "stream.h"
#include "memory.h"
#include "network.h"
#include "prefix.h"


/*A macro to check pointers in order to not
  go behind the allocated mem block 
  S -- stream reference
  Z -- size of data to be written 
*/

#define CHECK_SIZE(S, Z) \
	if (((S)->putp + (Z)) > (S)->size) \
           (Z) = (S)->size - (S)->putp;

/* Stream is fixed length buffer for network output/input. */

/* Make stream buffer. */
struct stream *
stream_new (size_t size)
{
  struct stream *s;

  s = XCALLOC (MTYPE_STREAM, sizeof (struct stream));

  s->data = XCALLOC (MTYPE_STREAM_DATA, size);
  s->size = size;
  return s;
}

/* Free it now. */
void
stream_free (struct stream *s)
{
  XFREE (MTYPE_STREAM_DATA, s->data);
  XFREE (MTYPE_STREAM, s);
}

unsigned long
stream_get_getp (struct stream *s)
{
  return s->getp;
}

unsigned long
stream_get_putp (struct stream *s)
{
  return s->putp;
}

unsigned long
stream_get_endp (struct stream *s)
{
  return s->endp;
}

unsigned long
stream_get_size (struct stream *s)
{
  return s->size;
}

/* Stream structre' stream pointer related functions.  */
void
stream_set_getp (struct stream *s, unsigned long pos)
{
  s->getp = pos;
}

void
stream_set_putp (struct stream *s, unsigned long pos)
{
  s->putp = pos;
}

/* Forward pointer. */
void
stream_forward (struct stream *s, int size)
{
  s->getp += size;
}

/* Copy from stream to destination. */
void
stream_get (void *dst, struct stream *s, size_t size)
{
  memcpy (dst, s->data + s->getp, size);
  s->getp += size;
}

/* Get next character from the stream. */
u_char
stream_getc (struct stream *s)
{
  u_char c;

  c = s->data[s->getp];
  s->getp++;
  return c;
}

/* Get next character from the stream. */
u_char
stream_getc_from (struct stream *s, unsigned long from)
{
  u_char c;

  c = s->data[from];
  return c;
}

/* Get next word from the stream. */
u_int16_t
stream_getw (struct stream *s)
{
  u_int16_t w;

  w = s->data[s->getp++] << 8;
  w |= s->data[s->getp++];
  return w;
}

/* Get next word from the stream. */
u_int16_t
stream_getw_from (struct stream *s, unsigned long from)
{
  u_int16_t w;

  w = s->data[from++] << 8;
  w |= s->data[from];
  return w;
}

/* Get next long word from the stream. */
u_int32_t
stream_getl (struct stream *s)
{
  u_int32_t l;

  l  = s->data[s->getp++] << 24;
  l |= s->data[s->getp++] << 16;
  l |= s->data[s->getp++] << 8;
  l |= s->data[s->getp++];
  return l;
}

/* Get next long word from the stream. */
u_int32_t
stream_get_ipv4 (struct stream *s)
{
  u_int32_t l;

  memcpy (&l, s->data + s->getp, 4);
  s->getp += 4;

  return l;
}

/* Copy to source to stream. */
void
stream_put (struct stream *s, void *src, size_t size)
{

  CHECK_SIZE(s, size);

  if (src)
    memcpy (s->data + s->putp, src, size);
  else
    memset (s->data + s->putp, 0, size);

  s->putp += size;
  if (s->putp > s->endp)
    s->endp = s->putp;
}

/* Put character to the stream. */
int
stream_putc (struct stream *s, u_char c)
{
  if (s->putp >= s->size) return 0;

  s->data[s->putp] = c;
  s->putp++;
  if (s->putp > s->endp)
    s->endp = s->putp;
  return 1;
}

/* Put word to the stream. */
int
stream_putw (struct stream *s, u_int16_t w)
{
  if ((s->size - s->putp) < 2) return 0;

  s->data[s->putp++] = (u_char)(w >>  8);
  s->data[s->putp++] = (u_char) w;

  if (s->putp > s->endp)
    s->endp = s->putp;
  return 2;
}

/* Put long word to the stream. */
int
stream_putl (struct stream *s, u_int32_t l)
{
  if ((s->size - s->putp) < 4) return 0;

  s->data[s->putp++] = (u_char)(l >> 24);
  s->data[s->putp++] = (u_char)(l >> 16);
  s->data[s->putp++] = (u_char)(l >>  8);
  s->data[s->putp++] = (u_char)l;

  if (s->putp > s->endp)
    s->endp = s->putp;
  return 4;
}

int
stream_putc_at (struct stream *s, unsigned long putp, u_char c)
{
  s->data[putp] = c;
  return 1;
}

int
stream_putw_at (struct stream *s, unsigned long putp, u_int16_t w)
{
  s->data[putp] = (u_char)(w >>  8);
  s->data[putp + 1] = (u_char) w;
  return 2;
}

int
stream_putl_at (struct stream *s, unsigned long putp, u_int32_t l)
{
  s->data[putp] = (u_char)(l >> 24);
  s->data[putp + 1] = (u_char)(l >> 16);
  s->data[putp + 2] = (u_char)(l >>  8);
  s->data[putp + 3] = (u_char)l;
  return 4;
}

/* Put long word to the stream. */
int
stream_put_ipv4 (struct stream *s, u_int32_t l)
{
  if ((s->size - s->putp) < 4)
    return 0;

  memcpy (s->data + s->putp, &l, 4);
  s->putp += 4;

  if (s->putp > s->endp)
    s->endp = s->putp;
  return 4;
}

/* Put long word to the stream. */
int
stream_put_in_addr (struct stream *s, struct in_addr *addr)
{
  if ((s->size - s->putp) < 4)
    return 0;

  memcpy (s->data + s->putp, addr, 4);
  s->putp += 4;

  if (s->putp > s->endp)
    s->endp = s->putp;
  return 4;
}

/* Put prefix by nlri type format. */
int
stream_put_prefix (struct stream *s, struct prefix *p)
{
  u_char psize;

  psize = PSIZE (p->prefixlen);

  if ((s->size - s->putp) < psize) return 0;

  stream_putc (s, p->prefixlen);
  memcpy (s->data + s->putp, &p->u.prefix, psize);
  s->putp += psize;
  
  if (s->putp > s->endp)
    s->endp = s->putp;

  return psize;
}

/* Read size from fd. */
int
stream_read (struct stream *s, int fd, size_t size)
{
  int nbytes;

  nbytes = readn (fd, s->data + s->putp, size);

  if (nbytes > 0)
    {
      s->putp += nbytes;
      s->endp += nbytes;
    }
  return nbytes;
}

/* Read size from fd. */
int
stream_read_unblock (struct stream *s, int fd, size_t size)
{
  int nbytes;
  int val;

  val = fcntl (fd, F_GETFL, 0);
  fcntl (fd, F_SETFL, val|O_NONBLOCK);
  nbytes = read (fd, s->data + s->putp, size);
  fcntl (fd, F_SETFL, val);

  if (nbytes > 0)
    {
      s->putp += nbytes;
      s->endp += nbytes;
    }
  return nbytes;
}

/* Write data to buffer. */
int
stream_write (struct stream *s, u_char *ptr, size_t size)
{

  CHECK_SIZE(s, size);

  memcpy (s->data + s->putp, ptr, size);
  s->putp += size;
  if (s->putp > s->endp)
    s->endp = s->putp;
  return size;
}

/* Return current read pointer. */
u_char *
stream_pnt (struct stream *s)
{
  return s->data + s->getp;
}

/* Check does this stream empty? */
int
stream_empty (struct stream *s)
{
  if (s->putp == 0 && s->endp == 0 && s->getp == 0)
    return 1;
  else
    return 0;
}

/* Reset stream. */
void
stream_reset (struct stream *s)
{
  s->putp = 0;
  s->endp = 0;
  s->getp = 0;
}

/* Write stream contens to the file discriptor. */
int
stream_flush (struct stream *s, int fd)
{
  int nbytes;

  nbytes = write (fd, s->data + s->getp, s->endp - s->getp);

  return nbytes;
}

/* Stream first in first out queue. */

struct stream_fifo *
stream_fifo_new ()
{
  struct stream_fifo *new;
 
  new = XCALLOC (MTYPE_STREAM_FIFO, sizeof (struct stream_fifo));
  return new;
}

/* Add new stream to fifo. */
void
stream_fifo_push (struct stream_fifo *fifo, struct stream *s)
{
  if (fifo->tail)
    fifo->tail->next = s;
  else
    fifo->head = s;
     
  fifo->tail = s;

  fifo->count++;
}

/* Delete first stream from fifo. */
struct stream *
stream_fifo_pop (struct stream_fifo *fifo)
{
  struct stream *s;
  
  s = fifo->head; 

  if (s)
    { 
      fifo->head = s->next;

      if (fifo->head == NULL)
	fifo->tail = NULL;
    }

  fifo->count--;

  return s; 
}

/* Return first fifo entry. */
struct stream *
stream_fifo_head (struct stream_fifo *fifo)
{
  return fifo->head;
}

void
stream_fifo_clean (struct stream_fifo *fifo)
{
  struct stream *s;
  struct stream *next;

  for (s = fifo->head; s; s = next)
    {
      next = s->next;
      stream_free (s);
    }
  fifo->head = fifo->tail = NULL;
  fifo->count = 0;
}

void
stream_fifo_free (struct stream_fifo *fifo)
{
  stream_fifo_clean (fifo);
  XFREE (MTYPE_STREAM_FIFO, fifo);
}
