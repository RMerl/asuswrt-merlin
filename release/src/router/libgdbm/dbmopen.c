/* dbmopen.c - Open the file for dbm operations. This looks like the
   NDBM interface for dbm use. */

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
#include "gdbmerrno.h"
#include "extern.h"

/* Initialize ndbm system.  FILE is a pointer to the file name.  In
   standard dbm, the database is found in files called FILE.pag and
   FILE.dir.  To make gdbm compatable with dbm using the dbminit call,
   the same file names are used.  Specifically, dbminit will use the file
   name FILE.pag in its call to gdbm open.  If the file (FILE.pag) has a
   size of zero bytes, a file initialization procedure is performed,
   setting up the initial structure in the file.  Any error detected will
   cause a return value of -1.  No errors cause the return value of 0.
   NOTE: file.dir will be ignored and will always have a size of zero.
   FLAGS and MODE are the same as the open (2) call.  This call will
   look at the FLAGS and decide what call to make to gdbm_open.  For
   FLAGS == O_RDONLY, it will be a GDBM_READER, if FLAGS == O_RDWR|O_CREAT,
   it will be a GDBM_WRCREAT (creater and writer) and if the FLAGS == O_RDWR,
   it will be a GDBM_WRITER and if FLAGS contain O_TRUNC then it will be
   a GDBM_NEWDB.  All other values of FLAGS in the flags are
   ignored. */

gdbm_file_info *
dbm_open (file, flags, mode)
     char *file;
     int flags;
     int mode;
{
  char* pag_file;	    /* Used to construct "file.pag". */
  char* dir_file;	    /* Used to construct "file.dir". */
  struct stat dir_stat;	    /* Stat information for "file.dir". */
  gdbm_file_info *temp_dbf;  /* Temporary file pointer storage. */


  /* Prepare the correct names of "file.pag" and "file.dir". */
  pag_file = (char *) malloc (strlen (file)+5);
  dir_file = (char *) malloc (strlen (file)+5);
  if ((pag_file == NULL) || (dir_file == NULL))
    {
      gdbm_errno = GDBM_MALLOC_ERROR;	/* For the hell of it. */
      return NULL;
    }

  strcpy (pag_file, file);
  strcat (pag_file, ".pag");
  strcpy (dir_file, file);
  strcat (dir_file, ".dir");
  

  /* Call the actual routine, saving the pointer to the file information. */
  flags &= O_RDONLY | O_RDWR | O_CREAT | O_TRUNC;
  if (flags == O_RDONLY)
    {
      temp_dbf = gdbm_open (pag_file, 0, GDBM_READER, 0, NULL);
    }
  else if (flags == (O_RDWR | O_CREAT))
    {
      temp_dbf = gdbm_open (pag_file, 0, GDBM_WRCREAT, mode, NULL);
    }
  else if ( (flags & O_TRUNC) == O_TRUNC)
    {
      temp_dbf = gdbm_open (pag_file, 0, GDBM_NEWDB, mode, NULL);
    }
  else
    {
      temp_dbf = gdbm_open (pag_file, 0, GDBM_WRITER, 0, NULL);
    }

  /* Did we successfully open the file? */
  if (temp_dbf == NULL)
    {
      gdbm_errno = GDBM_FILE_OPEN_ERROR;
      goto done;
    }

  /* If the database is new, link "file.dir" to "file.pag". This is done
     so the time stamp on both files is the same. */
  if (stat (dir_file, &dir_stat) == 0)
    {
      if (dir_stat.st_size == 0)
	if (unlink (dir_file) != 0 || link (pag_file, dir_file) != 0)
	  {
	    gdbm_errno = GDBM_FILE_OPEN_ERROR;
	    gdbm_close (temp_dbf);
	    temp_dbf = NULL;
	    goto done;
	  }
    }
  else
    {
      /* Since we can't stat it, we assume it is not there and try
         to link the dir_file to the pag_file. */
      if (link (pag_file, dir_file) != 0)
	{
	  gdbm_errno = GDBM_FILE_OPEN_ERROR;
	  gdbm_close (temp_dbf);
	  temp_dbf = NULL;
	  goto done;
	}
    }

done:
  free (pag_file);
  free (dir_file);
  return temp_dbf;
}
