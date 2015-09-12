/* 
 * Copyright (C) 2004 Paul Jakma
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
#include <memory.h>
#include <buffer.h>

struct thread_master *master;

int
main(int argc, char **argv)
{
  struct buffer *b1, *b2;
  int n;
  char junk[3];
  char c = 'a';

  memory_init();
  
  if ((argc != 2) || (sscanf(argv[1], "%d%1s", &n, junk) != 1))
    {
      fprintf(stderr, "Usage: %s <number of chars to simulate>\n", *argv);
      return 1;
    }

  b1 = buffer_new(0);
  b2 = buffer_new(1024);
  
  while (n-- > 0)
    {
      buffer_put(b1, &c, 1);
      buffer_put(b2, &c, 1);
      if (c++ == 'z')
        c = 'a';
      buffer_reset(b1);
      buffer_reset(b2);
    }
  buffer_free(b1);
  buffer_free(b2);
  return 0;
}
