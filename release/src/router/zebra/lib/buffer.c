/*
 * Buffering of output and input. 
 * Copyright (C) 1998 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 * 
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */

#include <zebra.h>

#include "memory.h"
#include "buffer.h"

/* Make buffer data. */
struct buffer_data *
buffer_data_new (size_t size)
{
  struct buffer_data *d;

  d = XMALLOC (MTYPE_BUFFER_DATA, sizeof (struct buffer_data));
  memset (d, 0, sizeof (struct buffer_data));
  d->data = XMALLOC (MTYPE_BUFFER_DATA, size);

  return d;
}

void
buffer_data_free (struct buffer_data *d)
{
  if (d->data)
    XFREE (MTYPE_BUFFER_DATA, d->data);
  XFREE (MTYPE_BUFFER_DATA, d);
}

/* Make new buffer. */
struct buffer *
buffer_new (size_t size)
{
  struct buffer *b;

  b = XMALLOC (MTYPE_BUFFER, sizeof (struct buffer));
  memset (b, 0, sizeof (struct buffer));

  b->size = size;

  return b;
}

/* Free buffer. */
void
buffer_free (struct buffer *b)
{
  struct buffer_data *d;
  struct buffer_data *next;

  d = b->head;
  while (d)
    {
      next = d->next;
      buffer_data_free (d);
      d = next;
    }

  d = b->unused_head;
  while (d)
    {
      next = d->next;
      buffer_data_free (d);
      d = next;
    }
  
  XFREE (MTYPE_BUFFER, b);
}

/* Make string clone. */
char *
buffer_getstr (struct buffer *b)
{
  return strdup ((char *)b->head->data);
}

/* Return 1 if buffer is empty. */
int
buffer_empty (struct buffer *b)
{
  if (b->tail == NULL || b->tail->cp == b->tail->sp)
    return 1;
  else
    return 0;
}

/* Clear and free all allocated data. */
void
buffer_reset (struct buffer *b)
{
  struct buffer_data *data;
  struct buffer_data *next;
  
  for (data = b->head; data; data = next)
    {
      next = data->next;
      buffer_data_free (data);
    }
  b->head = b->tail = NULL;
  b->alloc = 0;
  b->length = 0;
}

/* Add buffer_data to the end of buffer. */
void
buffer_add (struct buffer *b)
{
  struct buffer_data *d;

  d = buffer_data_new (b->size);

  if (b->tail == NULL)
    {
      d->prev = NULL;
      d->next = NULL;
      b->head = d;
      b->tail = d;
    }
  else
    {
      d->prev = b->tail;
      d->next = NULL;

      b->tail->next = d;
      b->tail = d;
    }

  b->alloc++;
}

/* Write data to buffer. */
int
buffer_write (struct buffer *b, u_char *ptr, size_t size)
{
  struct buffer_data *data;

  data = b->tail;
  b->length += size;

  /* We use even last one byte of data buffer. */
  while (size)    
    {
      /* If there is no data buffer add it. */
      if (data == NULL || data->cp == b->size)
	{
	  buffer_add (b);
	  data = b->tail;
	}

      /* Last data. */
      if (size <= (b->size - data->cp))
	{
	  memcpy ((data->data + data->cp), ptr, size);

	  data->cp += size;
	  size = 0;
	}
      else
	{
	  memcpy ((data->data + data->cp), ptr, (b->size - data->cp));

	  size -= (b->size - data->cp);
	  ptr += (b->size - data->cp);

	  data->cp = b->size;
	}
    }
  return 1;
}

/* Insert character into the buffer. */
int
buffer_putc (struct buffer *b, u_char c)
{
  buffer_write (b, &c, 1);
  return 1;
}

/* Insert word (2 octets) into ther buffer. */
int
buffer_putw (struct buffer *b, u_short c)
{
  buffer_write (b, (char *)&c, 2);
  return 1;
}

/* Put string to the buffer. */
int
buffer_putstr (struct buffer *b, u_char *c)
{
  size_t size;

  size = strlen ((char *)c);
  buffer_write (b, c, size);
  return 1;
}

/* Flush specified size to the fd. */
void
buffer_flush (struct buffer *b, int fd, size_t size)
{
  int iov_index;
  struct iovec *iovec;
  struct buffer_data *data;
  struct buffer_data *out;
  struct buffer_data *next;

  iovec = malloc (sizeof (struct iovec) * b->alloc);
  iov_index = 0;

  for (data = b->head; data; data = data->next)
    {
      iovec[iov_index].iov_base = (char *)(data->data + data->sp);

      if (size <= (data->cp - data->sp))
	{
	  iovec[iov_index++].iov_len = size;
	  data->sp += size;
	  if (data->sp == data->cp)
	    data = data->next;
	  break;
	}
      else
	{
	  iovec[iov_index++].iov_len = data->cp - data->sp;
	  size -= data->cp - data->sp;
	  data->sp = data->cp;
	}
    }

  /* Write buffer to the fd. */
  writev (fd, iovec, iov_index);

  /* Free printed buffer data. */
  for (out = b->head; out && out != data; out = next)
    {
      next = out->next;
      if (next)
	next->prev = NULL;
      else
	b->tail = next;
      b->head = next;

      buffer_data_free (out);
      b->alloc--;
    }

  free (iovec);
}

/* Flush all buffer to the fd. */
int
buffer_flush_all (struct buffer *b, int fd)
{
  int ret;
  struct buffer_data *d;
  int iov_index;
  struct iovec *iovec;

  if (buffer_empty (b))
    return 0;

  iovec = malloc (sizeof (struct iovec) * b->alloc);
  iov_index = 0;

  for (d = b->head; d; d = d->next)
    {
      iovec[iov_index].iov_base = (char *)(d->data + d->sp);
      iovec[iov_index].iov_len = d->cp - d->sp;
      iov_index++;
    }
  ret = writev (fd, iovec, iov_index);

  free (iovec);

  buffer_reset (b);

  return ret;
}

/* Flush all buffer to the fd. */
int
buffer_flush_vty_all (struct buffer *b, int fd, int erase_flag,
		      int no_more_flag)
{
  int nbytes;
  int iov_index;
  struct iovec *iov;
  struct iovec small_iov[3];
  char more[] = " --More-- ";
  char erase[] = { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
		   ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		   0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08};
  struct buffer_data *data;
  struct buffer_data *out;
  struct buffer_data *next;

  /* For erase and more data add two to b's buffer_data count.*/
  if (b->alloc == 1)
    iov = small_iov;
  else
    iov = XCALLOC (MTYPE_TMP, sizeof (struct iovec) * (b->alloc + 2));

  data = b->head;
  iov_index = 0;

  /* Previously print out is performed. */
  if (erase_flag)
    {
      iov[iov_index].iov_base = erase;
      iov[iov_index].iov_len = sizeof erase;
      iov_index++;
    }

  /* Output data. */
  for (data = b->head; data; data = data->next)
    {
      iov[iov_index].iov_base = (char *)(data->data + data->sp);
      iov[iov_index].iov_len = data->cp - data->sp;
      iov_index++;
    }

  /* In case of `more' display need. */
  if (! buffer_empty (b) && !no_more_flag)
    {
      iov[iov_index].iov_base = more;
      iov[iov_index].iov_len = sizeof more;
      iov_index++;
    }

  /* We use write or writev*/
  nbytes = writev (fd, iov, iov_index);

  /* Error treatment. */
  if (nbytes < 0)
    {
      if (errno == EINTR)
	;
      if (errno == EWOULDBLOCK)
	;
    }

  /* Free printed buffer data. */
  for (out = b->head; out && out != data; out = next)
    {
      next = out->next;
      if (next)
	next->prev = NULL;
      else
	b->tail = next;
      b->head = next;

      buffer_data_free (out);
      b->alloc--;
    }

  if (iov != small_iov)
    XFREE (MTYPE_TMP, iov);

  return nbytes;
}

/* Flush buffer to the file descriptor.  Mainly used from vty
   interface. */
int
buffer_flush_vty (struct buffer *b, int fd, int size, 
		  int erase_flag, int no_more_flag)
{
  int nbytes;
  int iov_index;
  struct iovec *iov;
  struct iovec small_iov[3];
  char more[] = " --More-- ";
  char erase[] = { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
		   ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		   0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08};
  struct buffer_data *data;
  struct buffer_data *out;
  struct buffer_data *next;

#ifdef  IOV_MAX
  int iov_size;
  int total_size;
  struct iovec *c_iov;
  int c_nbytes;
#endif /* IOV_MAX */

  /* For erase and more data add two to b's buffer_data count.*/
  if (b->alloc == 1)
    iov = small_iov;
  else
    iov = XCALLOC (MTYPE_TMP, sizeof (struct iovec) * (b->alloc + 2));

  data = b->head;
  iov_index = 0;

  /* Previously print out is performed. */
  if (erase_flag)
    {
      iov[iov_index].iov_base = erase;
      iov[iov_index].iov_len = sizeof erase;
      iov_index++;
    }

  /* Output data. */
  for (data = b->head; data; data = data->next)
    {
      iov[iov_index].iov_base = (char *)(data->data + data->sp);

      if (size <= (data->cp - data->sp))
	{
	  iov[iov_index++].iov_len = size;
	  data->sp += size;
	  if (data->sp == data->cp)
	    data = data->next;
	  break;
	}
      else
	{
	  iov[iov_index++].iov_len = data->cp - data->sp;
	  size -= (data->cp - data->sp);
	  data->sp = data->cp;
	}
    }

  /* In case of `more' display need. */
  if (!buffer_empty (b) && !no_more_flag)
    {
      iov[iov_index].iov_base = more;
      iov[iov_index].iov_len = sizeof more;
      iov_index++;
    }

  /* We use write or writev*/

#ifdef IOV_MAX
  /* IOV_MAX are normally defined in <sys/uio.h> , Posix.1g.
     example: Solaris2.6 are defined IOV_MAX size at 16.     */
  c_iov = iov;
  total_size = iov_index;
  nbytes = 0;

  while( total_size > 0 )
    {
       /* initialize write vector size at once */
       iov_size = ( total_size > IOV_MAX ) ? IOV_MAX : total_size;

       c_nbytes = writev (fd, c_iov, iov_size );

       if( c_nbytes < 0 )
         {
           if(errno == EINTR)
             ;
             ;
           if(errno == EWOULDBLOCK)
             ;
             ;
           nbytes = c_nbytes;
           break;

         }

        nbytes += c_nbytes;

       /* move pointer io-vector */
       c_iov += iov_size;
       total_size -= iov_size;
    }
#else  /* IOV_MAX */
   nbytes = writev (fd, iov, iov_index);

  /* Error treatment. */
  if (nbytes < 0)
    {
      if (errno == EINTR)
	;
      if (errno == EWOULDBLOCK)
	;
    }
#endif /* IOV_MAX */

  /* Free printed buffer data. */
  for (out = b->head; out && out != data; out = next)
    {
      next = out->next;
      if (next)
	next->prev = NULL;
      else
	b->tail = next;
      b->head = next;

      buffer_data_free (out);
      b->alloc--;
    }

  if (iov != small_iov)
    XFREE (MTYPE_TMP, iov);

  return nbytes;
}

/* Calculate size of outputs then flush buffer to the file
   descriptor. */
int
buffer_flush_window (struct buffer *b, int fd, int width, int height, 
		     int erase, int no_more)
{
  unsigned long cp;
  unsigned long size;
  int lp;
  int lineno;
  struct buffer_data *data;

  if (height >= 2)
    height--;

  /* We have to calculate how many bytes should be written. */
  lp = 0;
  lineno = 0;
  size = 0;
  
  for (data = b->head; data; data = data->next)
    {
      cp = data->sp;

      while (cp < data->cp)
	{
	  if (data->data[cp] == '\n' || lp == width)
	    {
	      lineno++;
	      if (lineno == height)
		{
		  cp++;
		  size++;
		  goto flush;
		}
	      lp = 0;
	    }
	  cp++;
	  lp++;
	  size++;
	}
    }

  /* Write data to the file descriptor. */
 flush:

  return buffer_flush_vty (b, fd, size, erase, no_more);
}
