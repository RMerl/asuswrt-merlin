/* hash.c - The gdbm hash function. */

/*  This file is part of GDBM, the GNU data base manager, by Philip A. Nelson.
    Copyright (C) 1990, 1991, 1993  Free Software Foundation, Inc.

    GDBM is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    GDBM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GDBM; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department
                Western Washington University
                Bellingham, WA 98226
       
*************************************************************************/


/* include system configuration before all else. */
#include "autoconf.h"

#include "gdbmdefs.h"


/* This hash function computes a 31 bit value.  The value is used to index
   the hash directory using the top n bits.  It is also used in a hash bucket
   to find the home position of the element by taking the value modulo the
   bucket hash table size. */

int
_gdbm_hash (key)
     datum key;
{
  unsigned int value;	/* Used to compute the hash value.  */
  int   index;		/* Used to cycle through random values. */


  /* Set the initial value from key. */
  value = 0x238F13AF * key.dsize;
  for (index = 0; index < key.dsize; index++)
    value = (value + (key.dptr[index] << (index*5 % 24))) & 0x7FFFFFFF;

  value = (1103515243 * value + 12345) & 0x7FFFFFFF;  

  /* Return the value. */
  return((int) value);
}
