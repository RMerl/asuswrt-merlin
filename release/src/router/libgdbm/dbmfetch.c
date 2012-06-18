/* dbmfetch.c - Find a key and return the associated data.  */

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


/* NDBM Look up a given KEY and return the information associated with that
   KEY. The pointer in the structure that is  returned is a pointer to
   dynamically allocated memory block.  */

datum
dbm_fetch (dbf, key)
     gdbm_file_info *dbf;
     datum key;
{
  datum  ret_val;		/* The return value. */

  /* Free previous dynamic memory, do actual call, and save pointer to new
     memory. */
  ret_val = gdbm_fetch (dbf, key);
  if (_gdbm_fetch_val != NULL) free (_gdbm_fetch_val);
  _gdbm_fetch_val = ret_val.dptr;

  /* Return the new value. */
  return ret_val;
}
