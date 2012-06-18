/* update.c - The routines for updating the file to a consistent state. */

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

static void write_header __P((gdbm_file_info *));

/* This procedure writes the header back to the file described by DBF. */

static void
write_header (dbf)
     gdbm_file_info *dbf;
{
  int  num_bytes;	/* Return value for write. */
  off_t file_pos;	/* Return value for lseek. */

  file_pos = lseek (dbf->desc, 0L, L_SET);
  if (file_pos != 0) _gdbm_fatal (dbf, "lseek error");
  num_bytes = write (dbf->desc, dbf->header, dbf->header->block_size);
  if (num_bytes != dbf->header->block_size)
    _gdbm_fatal (dbf, "write error");

  /* Wait for all output to be done. */
  if (!dbf->fast_write)
    fsync (dbf->desc);
}


/* After all changes have been made in memory, we now write them
   all to disk. */
void
_gdbm_end_update (dbf)
     gdbm_file_info *dbf;
{
  int  num_bytes;	/* Return value for write. */
  off_t file_pos;	/* Return value for lseek. */
  
  
  /* Write the current bucket. */
  if (dbf->bucket_changed && (dbf->cache_entry != NULL))
    {
      _gdbm_write_bucket (dbf, dbf->cache_entry);
      dbf->bucket_changed = FALSE;
    }

  /* Write the other changed buckets if there are any. */
  if (dbf->second_changed)
    {
      if(dbf->bucket_cache != NULL)
        {
          register int index;

          for (index = 0; index < dbf->cache_size; index++)
	    {
	      if (dbf->bucket_cache[index].ca_changed)
	        _gdbm_write_bucket (dbf, &dbf->bucket_cache[index]);
            }
        }
      dbf->second_changed = FALSE;
    }
  
  /* Write the directory. */
  if (dbf->directory_changed)
    {
      file_pos = lseek (dbf->desc, dbf->header->dir, L_SET);
      if (file_pos != dbf->header->dir) _gdbm_fatal (dbf, "lseek error");
      num_bytes = write (dbf->desc, dbf->dir, dbf->header->dir_size);
      if (num_bytes != dbf->header->dir_size)
	_gdbm_fatal (dbf, "write error");
      dbf->directory_changed = FALSE;
      if (!dbf->header_changed && !dbf->fast_write)
	fsync (dbf->desc);
    }

  /* Final write of the header. */
  if (dbf->header_changed)
    {
      write_header (dbf);
      dbf->header_changed = FALSE;
    }
}


/* If a fatal error is detected, come here and exit. VAL tells which fatal
   error occured. */

void
_gdbm_fatal (dbf, val)
     gdbm_file_info *dbf;
     char *val;
{
  if ((dbf != NULL) && (dbf->fatal_err != NULL))
    (*dbf->fatal_err) (val);
  else
    {
      write (STDERR_FILENO, "gdbm fatal: ", 12);
      if (val != NULL)
	write (STDERR_FILENO, val, strlen(val));
      write (STDERR_FILENO, "\n", 1);
    }
  exit (1);
  /* NOTREACHED */
}
