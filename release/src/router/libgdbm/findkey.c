/* findkey.c - The routine that finds a key entry in the file. */

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


/* Read the data found in bucket entry ELEM_LOC in file DBF and
   return a pointer to it.  Also, cache the read value. */

char *
_gdbm_read_entry (dbf, elem_loc)
     gdbm_file_info *dbf;
     int elem_loc;
{
  int num_bytes;		/* For seeking and reading. */
  int key_size;
  int data_size;
  off_t file_pos;
  data_cache_elem *data_ca;

  /* Is it already in the cache? */
  if (dbf->cache_entry->ca_data.elem_loc == elem_loc)
    return dbf->cache_entry->ca_data.dptr;

  /* Set sizes and pointers. */
  key_size = dbf->bucket->h_table[elem_loc].key_size;
  data_size = dbf->bucket->h_table[elem_loc].data_size;
  data_ca = &dbf->cache_entry->ca_data;
  
  /* Set up the cache. */
  if (data_ca->dptr != NULL) free (data_ca->dptr);
  data_ca->key_size = key_size;
  data_ca->data_size = data_size;
  data_ca->elem_loc = elem_loc;
  data_ca->hash_val = dbf->bucket->h_table[elem_loc].hash_value;
  if (key_size+data_size == 0)
    data_ca->dptr = (char *) malloc (1);
  else
    data_ca->dptr = (char *) malloc (key_size+data_size);
  if (data_ca->dptr == NULL) _gdbm_fatal (dbf, "malloc error");


  /* Read into the cache. */
  file_pos = lseek (dbf->desc,
		    dbf->bucket->h_table[elem_loc].data_pointer, L_SET);
  if (file_pos != dbf->bucket->h_table[elem_loc].data_pointer)
    _gdbm_fatal (dbf, "lseek error");
  num_bytes = read (dbf->desc, data_ca->dptr, key_size+data_size);
  if (num_bytes != key_size+data_size) _gdbm_fatal (dbf, "read error");
  
  return data_ca->dptr;
}



/* Find the KEY in the file and get ready to read the associated data.  The
   return value is the location in the current hash bucket of the KEY's
   entry.  If it is found, a pointer to the data and the key are returned
   in DPTR.  If it is not found, the value -1 is returned.  Since find
   key computes the hash value of key, that value */
int
_gdbm_findkey (dbf, key, dptr, new_hash_val)
     gdbm_file_info *dbf;
     datum key;
     char **dptr;
     int *new_hash_val;		/* The new hash value. */
{
  int    bucket_hash_val;	/* The hash value from the bucket. */
  char  *file_key;		/* The complete key as stored in the file. */
  int    elem_loc;		/* The location in the bucket. */
  int    home_loc;		/* The home location in the bucket. */
  int    key_size;		/* Size of the key on the file.  */

  /* Compute hash value and load proper bucket.  */
  *new_hash_val = _gdbm_hash (key);
  _gdbm_get_bucket (dbf, *new_hash_val>> (31-dbf->header->dir_bits));

  /* Is the element the last one found for this bucket? */
  if (dbf->cache_entry->ca_data.elem_loc != -1 
      && *new_hash_val == dbf->cache_entry->ca_data.hash_val
      && dbf->cache_entry->ca_data.key_size == key.dsize
      && dbf->cache_entry->ca_data.dptr != NULL
      && bcmp (dbf->cache_entry->ca_data.dptr, key.dptr, key.dsize) == 0)
    {
      /* This is it. Return the cache pointer. */
      *dptr = dbf->cache_entry->ca_data.dptr+key.dsize;
      return dbf->cache_entry->ca_data.elem_loc;
    }
      
  /* It is not the cached value, search for element in the bucket. */
  elem_loc = *new_hash_val % dbf->header->bucket_elems;
  home_loc = elem_loc;
  bucket_hash_val = dbf->bucket->h_table[elem_loc].hash_value;
  while (bucket_hash_val != -1)
    {
      key_size = dbf->bucket->h_table[elem_loc].key_size;
      if (bucket_hash_val != *new_hash_val
	 || key_size != key.dsize
	 || bcmp (dbf->bucket->h_table[elem_loc].key_start, key.dptr,
			(SMALL < key_size ? SMALL : key_size)) != 0) 
	{
	  /* Current elem_loc is not the item, go to next item. */
	  elem_loc = (elem_loc + 1) % dbf->header->bucket_elems;
	  if (elem_loc == home_loc) return -1;
	  bucket_hash_val = dbf->bucket->h_table[elem_loc].hash_value;
	}
      else
	{
	  /* This may be the one we want.
	     The only way to tell is to read it. */
	  file_key = _gdbm_read_entry (dbf, elem_loc);
	  if (bcmp (file_key, key.dptr, key_size) == 0)
	    {
	      /* This is the item. */
	      *dptr = file_key+key.dsize;
	      return elem_loc;
	    }
	  else
	    {
	      /* Not the item, try the next one.  Return if not found. */
	      elem_loc = (elem_loc + 1) % dbf->header->bucket_elems;
	      if (elem_loc == home_loc) return -1;
	      bucket_hash_val = dbf->bucket->h_table[elem_loc].hash_value;
	    }
	}
    }

  /* If we get here, we never found the key. */
  return -1;

}
