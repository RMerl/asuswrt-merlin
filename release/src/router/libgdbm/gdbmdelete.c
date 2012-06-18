/* gdbmdelete.c - Remove the key and its associated data from the database. */

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

/* Remove the KEYed item and the KEY from the database DBF.  The file on disk
   is updated to reflect the structure of the new database before returning
   from this procedure.  */

int
gdbm_delete (dbf, key)
     gdbm_file_info *dbf;
     datum key;
{
  int elem_loc;		/* The location in the current hash bucket. */
  int last_loc;		/* Last location emptied by the delete.  */
  int home;		/* Home position of an item. */
  bucket_element elem;  /* The element to be deleted. */
  char *find_data;	/* Return pointer from findkey. */
  int   hash_val;	/* Returned by findkey. */
  off_t free_adr;       /* Temporary stroage for address and size. */
  int   free_size;

  /* First check to make sure this guy is a writer. */
  if (dbf->read_write == GDBM_READER)
    {
      gdbm_errno = GDBM_READER_CANT_DELETE;
      return -1;
    }

  /* Initialize the gdbm_errno variable. */
  gdbm_errno = GDBM_NO_ERROR;

  /* Find the item. */
  elem_loc = _gdbm_findkey (dbf, key, &find_data, &hash_val);
  if (elem_loc == -1)
    {
      gdbm_errno = GDBM_ITEM_NOT_FOUND;
      return -1;
    }

  /* Save the element.  */
  elem = dbf->bucket->h_table[elem_loc];

  /* Delete the element.  */
  dbf->bucket->h_table[elem_loc].hash_value = -1;
  dbf->bucket->count -= 1;

  /* Move other elements to guarantee that they can be found. */
  last_loc = elem_loc;
  elem_loc = (elem_loc + 1) % dbf->header->bucket_elems;
  while (elem_loc != last_loc
	 && dbf->bucket->h_table[elem_loc].hash_value != -1)
    {
      home = dbf->bucket->h_table[elem_loc].hash_value
	     % dbf->header->bucket_elems;
      if ( (last_loc < elem_loc && (home <= last_loc || home > elem_loc))
	  || (last_loc > elem_loc && home <= last_loc && home > elem_loc))
	
	{
	  dbf->bucket->h_table[last_loc] = dbf->bucket->h_table[elem_loc];
	  dbf->bucket->h_table[elem_loc].hash_value = -1;
	  last_loc = elem_loc;
	}
      elem_loc = (elem_loc + 1) % dbf->header->bucket_elems;
    }

  /* Free the file space. */
  free_adr = elem.data_pointer;
  free_size = elem.key_size + elem.data_size;
  _gdbm_free (dbf, free_adr, free_size);

  /* Set the flags. */
  dbf->bucket_changed = TRUE;

  /* Clear out the data cache for the current bucket. */
  if (dbf->cache_entry->ca_data.dptr != NULL)
    {
      free (dbf->cache_entry->ca_data.dptr);
      dbf->cache_entry->ca_data.dptr = NULL;
    }
  dbf->cache_entry->ca_data.hash_val = -1;
  dbf->cache_entry->ca_data.key_size = 0;
  dbf->cache_entry->ca_data.elem_loc = -1;

  /* Do the writes. */
  _gdbm_end_update (dbf);
  return 0;  
}
