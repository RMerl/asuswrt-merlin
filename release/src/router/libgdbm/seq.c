/* seq.c - This is the sequential visit of the database.  This defines two
   user-visable routines that are used together. This is the DBM interface. */

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
#include "extern.h"

/* Start the visit of all keys in the database.  This produces something in
   hash order, not in any sorted order.  */

datum
firstkey ()
{
  datum ret_val;

  /* Free previous dynamic memory, do actual call, and save pointer to new
     memory. */
  ret_val = gdbm_firstkey (_gdbm_file);
  if (_gdbm_memory.dptr != NULL) free (_gdbm_memory.dptr);
  _gdbm_memory = ret_val;

  /* Return the new value. */
  return ret_val;
}


/* Continue visiting all keys.  The next key following KEY is returned. */

datum
nextkey (key)
     datum key;
{
  datum ret_val;

  /* Make sure we have a valid key. */
  if (key.dptr == NULL)
    return key;

  /* Call gdbm nextkey with supplied value. After that, free the old value. */
  ret_val = gdbm_nextkey (_gdbm_file, key);
  if (_gdbm_memory.dptr != NULL) free (_gdbm_memory.dptr);
  _gdbm_memory = ret_val;

  /* Return the new value. */
  return ret_val;
}
