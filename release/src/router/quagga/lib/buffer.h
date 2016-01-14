/*
 * Buffering to output and input. 
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

#ifndef _ZEBRA_BUFFER_H
#define _ZEBRA_BUFFER_H


/* Create a new buffer.  Memory will be allocated in chunks of the given
   size.  If the argument is 0, the library will supply a reasonable
   default size suitable for buffering socket I/O. */
extern struct buffer *buffer_new (size_t);

/* Free all data in the buffer. */
extern void buffer_reset (struct buffer *);

/* This function first calls buffer_reset to release all buffered data.
   Then it frees the struct buffer itself. */
extern void buffer_free (struct buffer *);

/* Add the given data to the end of the buffer. */
extern void buffer_put (struct buffer *, const void *, size_t);
/* Add a single character to the end of the buffer. */
extern void buffer_putc (struct buffer *, u_char);
/* Add a NUL-terminated string to the end of the buffer. */
extern void buffer_putstr (struct buffer *, const char *);

/* Combine all accumulated (and unflushed) data inside the buffer into a
   single NUL-terminated string allocated using XMALLOC(MTYPE_TMP).  Note
   that this function does not alter the state of the buffer, so the data
   is still inside waiting to be flushed. */
char *buffer_getstr (struct buffer *);

/* Returns 1 if there is no pending data in the buffer.  Otherwise returns 0. */
int buffer_empty (struct buffer *);

typedef enum
  {
    /* An I/O error occurred.  The buffer should be destroyed and the
       file descriptor should be closed. */
    BUFFER_ERROR = -1,

    /* The data was written successfully, and the buffer is now empty
       (there is no pending data waiting to be flushed). */
    BUFFER_EMPTY = 0,

    /* There is pending data in the buffer waiting to be flushed.  Please
       try flushing the buffer when select indicates that the file descriptor
       is writeable. */
    BUFFER_PENDING = 1
  } buffer_status_t;

/* Try to write this data to the file descriptor.  Any data that cannot
   be written immediately is added to the buffer queue. */
extern buffer_status_t buffer_write(struct buffer *, int fd,
				    const void *, size_t);

/* This function attempts to flush some (but perhaps not all) of 
   the queued data to the given file descriptor. */
extern buffer_status_t buffer_flush_available(struct buffer *, int fd);

/* The following 2 functions (buffer_flush_all and buffer_flush_window)
   are for use in lib/vty.c only.  They should not be used elsewhere. */

/* Call buffer_flush_available repeatedly until either all data has been
   flushed, or an I/O error has been encountered, or the operation would
   block. */
extern buffer_status_t buffer_flush_all (struct buffer *, int fd);

/* Attempt to write enough data to the given fd to fill a window of the
   given width and height (and remove the data written from the buffer).

   If !no_more, then a message saying " --More-- " is appended. 
   If erase is true, then first overwrite the previous " --More-- " message
   with spaces.

   Any write error (including EAGAIN or EINTR) will cause this function
   to return -1 (because the logic for handling the erase and more features
   is too complicated to retry the write later).
*/
extern buffer_status_t buffer_flush_window (struct buffer *, int fd, int width,
					    int height, int erase, int no_more);

#endif /* _ZEBRA_BUFFER_H */
