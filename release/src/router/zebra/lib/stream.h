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

#ifndef _ZEBRA_STREAM_H
#define _ZEBRA_STREAM_H

/* Stream buffer. */
struct stream
{
  struct stream *next;

  unsigned char *data;
  
  /* Put pointer. */
  unsigned long putp;

  /* Get pointer. */
  unsigned long getp;

  /* End of pointer. */
  unsigned long endp;

  /* Data size. */
  unsigned long size;
};

/* First in first out queue structure. */
struct stream_fifo
{
  unsigned long count;

  struct stream *head;
  struct stream *tail;
};

/* Utility macros. */
#define STREAM_PNT(S)   ((S)->data + (S)->getp)
#define STREAM_SIZE(S)  ((S)->size)
#define STREAM_REMAIN(S) ((S)->size - (S)->putp)
#define STREAM_DATA(S)  ((S)->data)

/* Stream prototypes. */
struct stream *stream_new (size_t);
void stream_free (struct stream *);

unsigned long stream_get_getp (struct stream *);
unsigned long stream_get_putp (struct stream *);
unsigned long stream_get_endp (struct stream *);
unsigned long stream_get_size (struct stream *);
u_char *stream_get_data (struct stream *);

void stream_set_getp (struct stream *, unsigned long);
void stream_set_putp (struct stream *, unsigned long);

void stream_forward (struct stream *, int);

void stream_put (struct stream *, void *, size_t);
int stream_putc (struct stream *, u_char);
int stream_putc_at (struct stream *, unsigned long, u_char);
int stream_putw (struct stream *, u_int16_t);
int stream_putw_at (struct stream *, unsigned long, u_int16_t);
int stream_putl (struct stream *, u_int32_t);
int stream_putl_at (struct stream *, unsigned long, u_int32_t);
int stream_put_ipv4 (struct stream *, u_int32_t);
int stream_put_in_addr (struct stream *, struct in_addr *);

void stream_get (void *, struct stream *, size_t);
u_char stream_getc (struct stream *);
u_char stream_getc_from (struct stream *, unsigned long);
u_int16_t stream_getw (struct stream *);
u_int16_t stream_getw_from (struct stream *, unsigned long);
u_int32_t stream_getl (struct stream *);
u_int32_t stream_get_ipv4 (struct stream *);

#undef stream_read
#undef stream_write
int stream_read (struct stream *, int, size_t);
int stream_read_unblock (struct stream *, int, size_t);
int stream_write (struct stream *, u_char *, size_t);

u_char *stream_pnt (struct stream *);
void stream_reset (struct stream *);
int stream_flush (struct stream *, int);
int stream_empty (struct stream *);

/* Stream fifo. */
struct stream_fifo *stream_fifo_new ();
void stream_fifo_push (struct stream_fifo *fifo, struct stream *s);
struct stream *stream_fifo_pop (struct stream_fifo *fifo);
struct stream *stream_fifo_head (struct stream_fifo *fifo);
void stream_fifo_clean (struct stream_fifo *fifo);
void stream_fifo_free (struct stream_fifo *fifo);

#endif /* _ZEBRA_STREAM_H */
