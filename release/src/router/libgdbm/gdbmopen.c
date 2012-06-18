/* gdbmopen.c - Open the dbm file and initialize data structures for use. */

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

/* Initialize dbm system.  FILE is a pointer to the file name.  If the file
   has a size of zero bytes, a file initialization procedure is performed,
   setting up the initial structure in the file.  BLOCK_SIZE is used during
   initialization to determine the size of various constructs.  If the value
   is less than 512, the file system blocksize is used, otherwise the value
   of BLOCK_SIZE is used.  BLOCK_SIZE is ignored if the file has previously
   initialized.  If FLAGS is set to GDBM_READ the user wants to just
   read the database and any call to dbm_store or dbm_delete will fail. Many
   readers can access the database at the same time.  If FLAGS is set to
   GDBM_WRITE, the user wants both read and write access to the database and
   requires exclusive access.  If FLAGS is GDBM_WRCREAT, the user wants
   both read and write access to the database and if the database does not
   exist, create a new one.  If FLAGS is GDBM_NEWDB, the user want a
   new database created, regardless of whether one existed, and wants read
   and write access to the new database.  Any error detected will cause a 
   return value of null and an approprate value will be in gdbm_errno.  If
   no errors occur, a pointer to the "gdbm file descriptor" will be
   returned. */
   

gdbm_file_info *
gdbm_open (file, block_size, flags, mode, fatal_func)
     char *file;
     int  block_size;
     int  flags;
     int  mode;
     void (*fatal_func) ();
{
  gdbm_file_info *dbf;		/* The record to return. */
  struct stat file_stat;	/* Space for the stat information. */
  int         len;		/* Length of the file name. */
  int         num_bytes;	/* Used in reading and writing. */
  off_t       file_pos;		/* Used with seeks. */
  int	      lock_val;         /* Returned by the flock call. */
  int	      file_block_size;	/* Block size to use for a new file. */
  int 	      index;		/* Used as a loop index. */
  char        need_trunc;	/* Used with GDBM_NEWDB and locking to avoid
				   truncating a file from under a reader. */

  /* Initialize the gdbm_errno variable. */
  gdbm_errno = GDBM_NO_ERROR;

  /* Allocate new info structure. */
  dbf = (gdbm_file_info *) malloc (sizeof (gdbm_file_info));
  if (dbf == NULL)
    {
      gdbm_errno = GDBM_MALLOC_ERROR;
      return NULL;
    }

  /* Initialize some fields for known values.  This is done so gdbm_close
     will work if called before allocating some structures. */
  dbf->dir  = NULL;
  dbf->bucket = NULL;
  dbf->header = NULL;
  dbf->bucket_cache = NULL;
  dbf->cache_size = 0;
  
  /* Save name of file. */
  len = strlen (file);
  dbf->name = (char *) malloc (len + 1);
  if (dbf->name == NULL)
    {
      free (dbf);
      gdbm_errno = GDBM_MALLOC_ERROR;
      return NULL;
    }
  strcpy (dbf->name, file);

  /* Initialize the fatal error routine. */
  dbf->fatal_err = fatal_func;

  dbf->fast_write = TRUE;	/* Default to setting fast_write. */
  dbf->file_locking = TRUE;	/* Default to doing file locking. */
  dbf->central_free = FALSE;	/* Default to not using central_free. */
  dbf->coalesce_blocks = FALSE; /* Default to not coalescing blocks. */

  /* GDBM_FAST used to determine whethere or not we set fast_write. */
  if (flags & GDBM_SYNC)
    {
      /* If GDBM_SYNC has been requested, don't do fast_write. */
      dbf->fast_write = FALSE;
    }
  if (flags & GDBM_NOLOCK)
    {
      dbf->file_locking = FALSE;
    }

  /* Open the file. */
  need_trunc = FALSE;
  switch (flags & GDBM_OPENMASK)
    {
      case GDBM_READER:
	dbf->desc = open (dbf->name, O_RDONLY, 0);
	break;

      case GDBM_WRITER:
	dbf->desc = open (dbf->name, O_RDWR, 0);
	break;

      case GDBM_NEWDB:
	dbf->desc = open (dbf->name, O_RDWR|O_CREAT, mode);
	need_trunc = TRUE;
	break;

      default:
	dbf->desc = open (dbf->name, O_RDWR|O_CREAT, mode);
	break;

    }
  if (dbf->desc < 0)
    {
      free (dbf->name);
      free (dbf);
      gdbm_errno = GDBM_FILE_OPEN_ERROR;
      return NULL;
    }

  /* Get the status of the file. */
  fstat (dbf->desc, &file_stat);

  /* Lock the file in the approprate way. */
  if ((flags & GDBM_OPENMASK) == GDBM_READER)
    {
      if (file_stat.st_size == 0)
	{
	  close (dbf->desc);
	  free (dbf->name);
	  free (dbf);
	  gdbm_errno = GDBM_EMPTY_DATABASE;
	  return NULL;
	}
      if (dbf->file_locking)
	{
          /* Sets lock_val to 0 for success.  See systems.h. */
          READLOCK_FILE(dbf);
	}
    }
  else if (dbf->file_locking)
    {
      /* Sets lock_val to 0 for success.  See systems.h. */
      WRITELOCK_FILE(dbf);
    }
  if (dbf->file_locking && (lock_val != 0))
    {
      close (dbf->desc);
      free (dbf->name);
      free (dbf);
      if ((flags & GDBM_OPENMASK) == GDBM_READER)
	gdbm_errno = GDBM_CANT_BE_READER;
      else
	gdbm_errno = GDBM_CANT_BE_WRITER;
      return NULL;
    }

  /* Record the kind of user. */
  dbf->read_write = (flags & GDBM_OPENMASK);

  /* If we do have a write lock and it was a GDBM_NEWDB, it is 
     now time to truncate the file. */
  if (need_trunc && file_stat.st_size != 0)
    {
      TRUNCATE (dbf);
      fstat (dbf->desc, &file_stat);
    }

  /* Decide if this is a new file or an old file. */
  if (file_stat.st_size == 0)
    {

      /* This is a new file.  Create an empty database.  */

      /* Start with the blocksize. */
      if (block_size < 512)
	file_block_size = STATBLKSIZE;
      else
	file_block_size = block_size;

      /* Get space for the file header. */
      dbf->header = (gdbm_file_header *) malloc (file_block_size);
      if (dbf->header == NULL)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_MALLOC_ERROR;
	  return NULL;
	}

      /* Set the magic number and the block_size. */
      dbf->header->header_magic = 0x13579ace;
      dbf->header->block_size = file_block_size;
     
      /* Create the initial hash table directory.  */
      dbf->header->dir_size = 8 * sizeof (off_t);
      dbf->header->dir_bits = 3;
      while (dbf->header->dir_size < dbf->header->block_size)
	{
	  dbf->header->dir_size <<= 1;
	  dbf->header->dir_bits += 1;
	}

      /* Check for correct block_size. */
      if (dbf->header->dir_size != dbf->header->block_size)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_BLOCK_SIZE_ERROR;
	  return NULL;
	}

      /* Allocate the space for the directory. */
      dbf->dir = (off_t *) malloc (dbf->header->dir_size);
      if (dbf->dir == NULL)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_MALLOC_ERROR;
	  return NULL;
	}
      dbf->header->dir = dbf->header->block_size;

      /* Create the first and only hash bucket. */
      dbf->header->bucket_elems =
	(dbf->header->block_size - sizeof (hash_bucket))
	/ sizeof (bucket_element) + 1;
      dbf->header->bucket_size  = dbf->header->block_size;
      dbf->bucket = (hash_bucket *) malloc (dbf->header->bucket_size);
      if (dbf->bucket == NULL)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_MALLOC_ERROR;
	  return NULL;
	}
      _gdbm_new_bucket (dbf, dbf->bucket, 0);
      dbf->bucket->av_count = 1;
      dbf->bucket->bucket_avail[0].av_adr = 3*dbf->header->block_size;
      dbf->bucket->bucket_avail[0].av_size = dbf->header->block_size;

      /* Set table entries to point to hash buckets. */
      for (index = 0; index < dbf->header->dir_size / sizeof (off_t); index++)
	dbf->dir[index] = 2*dbf->header->block_size;

      /* Initialize the active avail block. */
      dbf->header->avail.size
	= ( (dbf->header->block_size - sizeof (gdbm_file_header))
	 / sizeof (avail_elem)) + 1;
      dbf->header->avail.count = 0;
      dbf->header->avail.next_block = 0;
      dbf->header->next_block  = 4*dbf->header->block_size;

      /* Write initial configuration to the file. */
      /* Block 0 is the file header and active avail block. */
      num_bytes = write (dbf->desc, dbf->header, dbf->header->block_size);
      if (num_bytes != dbf->header->block_size)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_WRITE_ERROR;
	  return NULL;
	}

      /* Block 1 is the initial bucket directory. */
      num_bytes = write (dbf->desc, dbf->dir, dbf->header->dir_size);
      if (num_bytes != dbf->header->dir_size)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_WRITE_ERROR;
	  return NULL;
	}

      /* Block 2 is the only bucket. */
      num_bytes = write (dbf->desc, dbf->bucket, dbf->header->bucket_size);
      if (num_bytes != dbf->header->bucket_size)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_WRITE_ERROR;
	  return NULL;
	}

      /* Wait for initial configuration to be written to disk. */
      fsync (dbf->desc);

      free (dbf->bucket);
    }
  else
    {
      /* This is an old database.  Read in the information from the file
	 header and initialize the hash directory. */

      gdbm_file_header partial_header;  /* For the first part of it. */

      /* Read the partial file header. */
      num_bytes = read (dbf->desc, &partial_header, sizeof (gdbm_file_header));
      if (num_bytes != sizeof (gdbm_file_header))
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_READ_ERROR;
	  return NULL;
	}

      /* Is the magic number good? */
      if (partial_header.header_magic != 0x13579ace)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_BAD_MAGIC_NUMBER;
	  return NULL;
	}

      /* It is a good database, read the entire header. */
      dbf->header = (gdbm_file_header *) malloc (partial_header.block_size);
      if (dbf->header == NULL)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_MALLOC_ERROR;
	  return NULL;
	}
      bcopy (&partial_header, dbf->header, sizeof (gdbm_file_header));
      num_bytes = read (dbf->desc, &dbf->header->avail.av_table[1],
			dbf->header->block_size-sizeof (gdbm_file_header));
      if (num_bytes != dbf->header->block_size-sizeof (gdbm_file_header))
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_READ_ERROR;
	  return NULL;
	}
	
      /* Allocate space for the hash table directory.  */
      dbf->dir = (off_t *) malloc (dbf->header->dir_size);
      if (dbf->dir == NULL)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_MALLOC_ERROR;
	  return NULL;
	}

      /* Read the hash table directory. */
      file_pos = lseek (dbf->desc, dbf->header->dir, L_SET);
      if (file_pos != dbf->header->dir)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_SEEK_ERROR;
	  return NULL;
	}

      num_bytes = read (dbf->desc, dbf->dir, dbf->header->dir_size);
      if (num_bytes != dbf->header->dir_size)
	{
	  gdbm_close (dbf);
	  gdbm_errno = GDBM_FILE_READ_ERROR;
	  return NULL;
	}

    }

  /* Finish initializing dbf. */
  dbf->last_read = -1;
  dbf->bucket = NULL;
  dbf->bucket_dir = 0;
  dbf->cache_entry = NULL;
  dbf->header_changed = FALSE;
  dbf->directory_changed = FALSE;
  dbf->bucket_changed = FALSE;
  dbf->second_changed = FALSE;
  
  /* Everything is fine, return the pointer to the file
     information structure.  */
  return dbf;

}

/* initialize the bucket cache. */
int
_gdbm_init_cache(dbf, size)
    gdbm_file_info *dbf;
    int size;
{
register int index;

  if (dbf->bucket_cache == NULL)
    {
      dbf->bucket_cache = (cache_elem *) malloc(sizeof(cache_elem) * size);
      if(dbf->bucket_cache == NULL)
        {
          gdbm_errno = GDBM_MALLOC_ERROR;
          return(-1);
        }
      dbf->cache_size = size;

      for(index = 0; index < size; index++)
        {
          (dbf->bucket_cache[index]).ca_bucket
            = (hash_bucket *) malloc (dbf->header->bucket_size);
          if ((dbf->bucket_cache[index]).ca_bucket == NULL)
	    {
              gdbm_errno = GDBM_MALLOC_ERROR;
	      return(-1);
            }
          (dbf->bucket_cache[index]).ca_adr = 0;
          (dbf->bucket_cache[index]).ca_changed = FALSE;
          (dbf->bucket_cache[index]).ca_data.hash_val = -1;
          (dbf->bucket_cache[index]).ca_data.elem_loc = -1;
          (dbf->bucket_cache[index]).ca_data.dptr = NULL;
        }
      dbf->bucket = dbf->bucket_cache[0].ca_bucket;
      dbf->cache_entry = &dbf->bucket_cache[0];
    }
  return(0);
}
