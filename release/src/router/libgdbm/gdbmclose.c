/* gdbmclose.c - Close a previously opened dbm file. */

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

/* Close the dbm file and free all memory associated with the file DBF.
   Before freeing members of DBF, check and make sure that they were
   allocated.  */

void
gdbm_close (dbf)
     gdbm_file_info *dbf;
{
  register int index;	/* For freeing the bucket cache. */

  /* Make sure the database is all on disk. */
  if (dbf->read_write != GDBM_READER)
    fsync (dbf->desc);

  /* Close the file and free all malloced memory. */
  if (dbf->file_locking)
    {
      UNLOCK_FILE(dbf);
    }
  close (dbf->desc);
  free (dbf->name);
  if (dbf->dir != NULL) free (dbf->dir);

  if (dbf->bucket_cache != NULL) {
    for (index = 0; index < dbf->cache_size; index++) {
      if (dbf->bucket_cache[index].ca_bucket != NULL)
	free (dbf->bucket_cache[index].ca_bucket);
      if (dbf->bucket_cache[index].ca_data.dptr != NULL)
	free (dbf->bucket_cache[index].ca_data.dptr);
    }
    free (dbf->bucket_cache);
  }
  if ( dbf->header != NULL ) free (dbf->header);
  free (dbf);
}
