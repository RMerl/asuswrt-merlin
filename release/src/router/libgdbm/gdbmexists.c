/* gdbmexists.c - Check to see if a key exists */

/*  This file is part of GDBM, the GNU data base manager, by Philip A. Nelson.
    Copyright (C) 1993  Free Software Foundation, Inc.

    GDBM is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
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
#include "gdbmerrno.h"

/* this is nothing more than a wrapper around _gdbm_findkey(). the
   point? it doesn't alloate any memory. */

int
gdbm_exists (dbf, key)
    gdbm_file_info *dbf;
    datum key;
{
  char *find_data;		/* dummy */
  int hash_val;		/* dummy */

  return (_gdbm_findkey (dbf, key, &find_data, &hash_val) >= 0);
}
