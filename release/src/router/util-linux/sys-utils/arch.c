/* arch -- print machine architecture information
 * Created: Mon Dec 20 12:27:15 1993 by faith@cs.unc.edu
 * Revised: Mon Dec 20 12:29:23 1993 by faith@cs.unc.edu
 * Copyright 1993 Rickard E. Faith (faith@cs.unc.edu)

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <sys/utsname.h>

int main (void)
{
  struct utsname utsbuf;

  if (uname( &utsbuf )) {
     perror( "arch" );
     return 1;
  }

  printf( "%s\n", utsbuf.machine );

  return 0;
}
