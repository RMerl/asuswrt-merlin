/* Simple stream test.
 * 
 * Copyright (C) 2006 Sun Microsystems, Inc.
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>
#include <stream.h>
#include <thread.h>

static long int ham = 0xdeadbeefdeadbeef;
struct thread_master *master;

static void
print_stream (struct stream *s)
{
  size_t getp = stream_get_getp (s);
  
  printf ("endp: %ld, readable: %ld, writeable: %ld\n",
          stream_get_endp (s),
          STREAM_READABLE (s),
          STREAM_WRITEABLE (s));
  
  while (STREAM_READABLE (s))
    {
      printf ("0x%x ", *stream_pnt (s));
      stream_forward_getp (s, 1);
    }
  
  printf ("\n");
  
  /* put getp back to where it was */
  stream_set_getp (s, getp);
}

int
main (void)
{
  struct stream *s;
  
  s = stream_new (1024);
  
  stream_putc (s, ham);
  stream_putw (s, ham);
  stream_putl (s, ham);
  stream_putq (s, ham);
  
  print_stream (s);
  
  stream_resize (s, stream_get_endp (s));
  
  print_stream (s);
  
  printf ("c: 0x%hhx\n", stream_getc (s));
  printf ("w: 0x%hx\n", stream_getw (s));
  printf ("l: 0x%x\n", stream_getl (s));
  printf ("q: 0x%lx\n", stream_getq (s));
  
  return 0;
}
