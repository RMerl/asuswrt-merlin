/* close.c - Close the "original" style database. */

/*  This file is part of GDBM, the GNU data base manager, by Philip A. Nelson.
    Copyright (C) 1993  Free Software Foundation, Inc.

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

    You may contact the original author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department
                Western Washington University
                Bellingham, WA 98226
       
    The author of this file is:
       e-mail:  downsj@downsj.com

*************************************************************************/


/* include system configuration before all else. */
#include "autoconf.h"

#include "gdbmdefs.h"
#include "extern.h"


/* it's unclear whether dbmclose() is *always* a void function in old
   C libraries. we use int, here. */

int
dbmclose()
{
  if (_gdbm_file != NULL)
    {
      gdbm_close (_gdbm_file);
      _gdbm_file = NULL;
      if (_gdbm_memory.dptr != NULL) free(_gdbm_memory.dptr);
      _gdbm_memory.dptr = NULL;
      _gdbm_memory.dsize = 0;
    }
  return (0);
}
