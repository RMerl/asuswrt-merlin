/* testgdbm.c - Driver program to test the database routines and to
   help debug gdbm.  Uses inside information to show "system" information */

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

#include "getopt.h"

extern const char * gdbm_version;

extern const char *gdbm_strerror __P((gdbm_error));

gdbm_file_info *gdbm_file;

/* Debug procedure to print the contents of the current hash bucket. */
void
print_bucket (bucket, mesg)
     hash_bucket *bucket;
     char *mesg;
{
  int  index;

  printf ("******* %s **********\n\nbits = %d\ncount= %d\nHash Table:\n",
	 mesg, bucket->bucket_bits, bucket->count);
  printf ("     #    hash value     key size    data size     data adr  home\n");
  for (index = 0; index < gdbm_file->header->bucket_elems; index++)
    printf ("  %4d  %12x  %11d  %11d  %11d %5d\n", index,
	   bucket->h_table[index].hash_value,
	   bucket->h_table[index].key_size,
	   bucket->h_table[index].data_size,
	   bucket->h_table[index].data_pointer,
	   bucket->h_table[index].hash_value % gdbm_file->header->bucket_elems);

  printf ("\nAvail count = %1d\n", bucket->av_count);
  printf ("Avail  adr     size\n");
  for (index = 0; index < bucket->av_count; index++)
    printf ("%9d%9d\n", bucket->bucket_avail[index].av_adr,
	                bucket->bucket_avail[index].av_size);
}


void
_gdbm_print_avail_list (dbf)
     gdbm_file_info *dbf;
{
  int temp;
  int size;
  avail_block *av_stk;
 
  /* Print the the header avail block.  */
  printf ("\nheader block\nsize  = %d\ncount = %d\n",
	  dbf->header->avail.size, dbf->header->avail.count);
  for (temp = 0; temp < dbf->header->avail.count; temp++)
    {
      printf ("  %15d   %10d \n", dbf->header->avail.av_table[temp].av_size,
	      dbf->header->avail.av_table[temp].av_adr);
    }

  /* Initialize the variables for a pass throught the avail stack. */
  temp = dbf->header->avail.next_block;
  size = ( ( (dbf->header->avail.size * sizeof (avail_elem)) >> 1)
	  + sizeof (avail_block));
  av_stk = (avail_block *) malloc (size);
  if (av_stk == NULL)
    {
      printf("Out of memory\n");
      exit (2);
    }

  /* Print the stack. */
  while (FALSE)
    {
      lseek (dbf->desc, temp, L_SET);
      read  (dbf->desc, av_stk, size);

      /* Print the block! */
      printf ("\nblock = %d\nsize  = %d\ncount = %d\n", temp,
	      av_stk->size, av_stk->count);
      for (temp = 0; temp < av_stk->count; temp++)
	{
	  printf ("  %15d   %10d \n", av_stk->av_table[temp].av_size,
	    av_stk->av_table[temp].av_adr);
	}
      temp = av_stk->next_block;
    }
}

void
_gdbm_print_bucket_cache (dbf)
     gdbm_file_info *dbf;
{
  register int index;
  char changed;
 
  if (dbf->bucket_cache != NULL) {
    printf(
      "Bucket Cache (size %d):\n  Index:  Address  Changed  Data_Hash \n",
      dbf->cache_size);
    for (index=0; index < dbf->cache_size; index++) {
      changed = dbf->bucket_cache[index].ca_changed;
      printf ("  %5d:  %7d  %7s  %x\n",
	      index,
	      dbf->bucket_cache[index].ca_adr,
	      (changed ? "True" : "False"),
	      dbf->bucket_cache[index].ca_data.hash_val);
    }
  } else
    printf("Bucket cache has not been initialized.\n");
}

void
usage (s)
     char *s;
{
  printf(
      "Usage: %s [-r or -ns] [-b block-size] [-c cache-size] [-g gdbm-file]\n",
      s);
  exit (2);
}


/* The test program allows one to call all the routines plus the hash function.
   The commands are single letter commands.  The user is prompted for all other
   information.  See the help command (?) for a list of all commands. */

int
main (argc, argv)
     int argc;
     char *argv[];

{

  char  cmd_ch;

  datum key_data;
  datum data_data;
  datum return_data;

  char key_line[500];
  char data_line[1000];

  char done = FALSE;
  int  opt;
  char reader = FALSE;
  char newdb = FALSE;
  int  fast  = 0;

  int  cache_size = DEFAULT_CACHESIZE;
  int  block_size = 0;

  char *file_name = NULL;


  /* Argument checking. */
  opterr = 0;
  while ((opt = getopt (argc, argv, "srnc:b:g:")) != -1)
    switch (opt) {
    case 's':  fast = GDBM_SYNC;
               if (reader) usage (argv[0]);
               break;
    case 'r':  reader = TRUE;
               if (newdb) usage (argv[0]);
               break;
    case 'n':  newdb = TRUE;
               if (reader) usage (argv[0]);
               break;
    case 'c':  cache_size = atoi(optarg);
    	       break;
    case 'b':  block_size = atoi(optarg);
               break;
    case 'g':  file_name = optarg;
               break;
    default:  usage(argv[0]);
    }

  if(file_name == NULL) 
    file_name = "junk.gdbm";

  /* Initialize variables. */
  key_data.dptr = NULL;
  data_data.dptr = data_line;

  if (reader)
    {
      gdbm_file = gdbm_open (file_name, block_size, GDBM_READER, 00664, NULL);
    }
  else if (newdb)
    {
      gdbm_file =
    	gdbm_open (file_name, block_size, GDBM_NEWDB | fast, 00664, NULL);
    }
  else
    {
      gdbm_file =
    	gdbm_open (file_name, block_size, GDBM_WRCREAT | fast, 00664, NULL);
    }
  if (gdbm_file == NULL)
    {
      printf("gdbm_open failed, %s\n", gdbm_strerror(gdbm_errno));
      exit (2);
    }

  if (gdbm_setopt(gdbm_file, GDBM_CACHESIZE, &cache_size, sizeof(int)) == -1)
    {
      printf("gdbm_setopt failed, %s\n", gdbm_strerror(gdbm_errno));
      exit(2);
    }

  /* Welcome message. */
  printf ("\nWelcome to the gdbm test program.  Type ? for help.\n\n");
  
  while (!done)
    {
      printf ("com -> ");
      cmd_ch = getchar ();
      if (cmd_ch != '\n')
	{
	  char temp;
	  do
	      temp = getchar ();
	  while (temp != '\n' && temp != EOF);
	}
      if (cmd_ch == EOF) cmd_ch = 'q';
      switch (cmd_ch)
	{
	
	/* Standard cases found in all test{dbm,ndbm,gdbm} programs. */
	case '\n':
	  printf ("\n");
	  break;

	case 'c':
	  {
	    int temp;
	    temp = 0;
	    if (key_data.dptr != NULL) free (key_data.dptr);
	    return_data = gdbm_firstkey (gdbm_file);
	    while (return_data.dptr != NULL)
	      {
		temp++;
		key_data = return_data;
		return_data = gdbm_nextkey (gdbm_file, key_data);
		free (key_data.dptr);
	      }
	    printf ("There are %d items in the database.\n\n", temp);
	    key_data.dptr = NULL;
	  }
	  break;

	case 'd':
	  if (key_data.dptr != NULL) free (key_data.dptr);
	  printf ("key -> ");
	  gets (key_line);
	  key_data.dptr = key_line;
	  key_data.dsize = strlen (key_line)+1;
	  if (gdbm_delete (gdbm_file, key_data) != 0)
	    printf ("Item not found or deleted\n");
	  printf ("\n");
	  key_data.dptr = NULL;
	  break;

	case 'f':
	  if (key_data.dptr != NULL) free (key_data.dptr);
	  printf ("key -> ");
	  gets (key_line);
	  key_data.dptr = key_line;
	  key_data.dsize = strlen (key_line)+1;
	  return_data = gdbm_fetch (gdbm_file, key_data);
	  if (return_data.dptr != NULL)
	    {
	      printf ("data is ->%s\n\n", return_data.dptr);
	      free (return_data.dptr);
	    }
	  else
	    printf ("No such item found.\n\n");
	  key_data.dptr = NULL;
	  break;

	case 'n':
	  if (key_data.dptr != NULL) free (key_data.dptr);
	  printf ("key -> ");
	  gets (key_line);
	  key_data.dptr = key_line;
	  key_data.dsize = strlen (key_line)+1;
	  return_data = gdbm_nextkey (gdbm_file, key_data);
	  if (return_data.dptr != NULL)
	    {
	      key_data = return_data;
	      printf ("key is  ->%s\n", key_data.dptr);
	      return_data = gdbm_fetch (gdbm_file, key_data);
	      printf ("data is ->%s\n\n", return_data.dptr);
	      free (return_data.dptr);
	    }
	  else
	    {
	      printf ("No such item found.\n\n");
	      key_data.dptr = NULL;
	    }
	  break;

	case 'q':
	  done = TRUE;
	  break;

	case 's':
	  if (key_data.dptr != NULL) free (key_data.dptr);
	  printf ("key -> ");
	  gets (key_line);
	  key_data.dptr = key_line;
	  key_data.dsize = strlen (key_line)+1;
	  printf ("data -> ");
	  gets (data_line);
	  data_data.dsize = strlen (data_line)+1;
	  if (gdbm_store (gdbm_file, key_data, data_data, GDBM_REPLACE) != 0)
	    printf ("Item not inserted. \n");
	  printf ("\n");
	  key_data.dptr = NULL;
	  break;

	case '1':
	  if (key_data.dptr != NULL) free (key_data.dptr);
	  key_data = gdbm_firstkey (gdbm_file);
	  if (key_data.dptr != NULL)
	    {
	      printf ("key is  ->%s\n", key_data.dptr);
	      return_data = gdbm_fetch (gdbm_file, key_data);
	      printf ("data is ->%s\n\n", return_data.dptr);
	      free (return_data.dptr);
	    }
	  else
	    printf ("No such item found.\n\n");
	  break;

	case '2':
	  return_data = gdbm_nextkey (gdbm_file, key_data);
	  if (return_data.dptr != NULL)
	    {
	      free (key_data.dptr);
	      key_data = return_data;
	      printf ("key is  ->%s\n", key_data.dptr);
	      return_data = gdbm_fetch (gdbm_file, key_data);
	      printf ("data is ->%s\n\n", return_data.dptr);
	      free (return_data.dptr);
	    }
	  else
	    printf ("No such item found.\n\n");
	  break;


	/* Special cases for the testgdbm program. */
	case 'r':
	  {
	    if (gdbm_reorganize (gdbm_file))
	      printf ("Reorganization failed. \n\n");
	    else
	      printf ("Reorganization succeeded. \n\n");
	  }
	  break;

	case 'A':
	  _gdbm_print_avail_list (gdbm_file);
	  printf ("\n");
	  break;

	case 'B':
	  {
	    int temp;
	    char number[80];

	    printf ("bucket? ");
	    gets (number);
	    sscanf (number,"%d",&temp);

	    if (temp >= gdbm_file->header->dir_size /4)
	      {
		printf ("Not a bucket. \n\n");
		break;
	      }
	    _gdbm_get_bucket (gdbm_file, temp);
	  }
	  printf ("Your bucket is now ");

	case 'C':
	  print_bucket (gdbm_file->bucket, "Current bucket");
	  printf ("\n current directory entry = %d.\n", gdbm_file->bucket_dir);
	  printf (" current bucket address  = %d.\n\n",
		  gdbm_file->cache_entry->ca_adr);
	  break;

	case 'D':
	  printf ("Hash table directory.\n");
	  printf ("  Size =  %d.  Bits = %d. \n\n",gdbm_file->header->dir_size,
		  gdbm_file->header->dir_bits);
	  {
	    int temp;

	    for (temp = 0; temp < gdbm_file->header->dir_size / 4; temp++)
	      {
		printf ("  %10d:  %12d\n", temp, gdbm_file->dir[temp]);
		if ( (temp+1) % 20 == 0 && isatty (0))
		  {
		    printf ("*** CR to continue: ");
		    while (getchar () != '\n') /* Do nothing. */;
		  }
	      }
	  }
	  printf ("\n");
	  break;

	case 'F':
	  {
	    printf ("\nFile Header: \n\n");
	    printf ("  table        = %d\n", gdbm_file->header->dir);
	    printf ("  table size   = %d\n", gdbm_file->header->dir_size);
	    printf ("  table bits   = %d\n", gdbm_file->header->dir_bits);
	    printf ("  block size   = %d\n", gdbm_file->header->block_size);
	    printf ("  bucket elems = %d\n", gdbm_file->header->bucket_elems);
	    printf ("  bucket size  = %d\n", gdbm_file->header->bucket_size);
	    printf ("  header magic = %x\n", gdbm_file->header->header_magic);
	    printf ("  next block   = %d\n", gdbm_file->header->next_block);
	    printf ("  avail size   = %d\n", gdbm_file->header->avail.size);
	    printf ("  avail count  = %d\n", gdbm_file->header->avail.count);
	    printf ("  avail nx blk = %d\n", gdbm_file->header->avail.next_block);
	    printf ("\n");
	  }
	  break;

        case 'H':
	  if (key_data.dptr != NULL) free (key_data.dptr);
	  printf ("key -> ");
	  gets (key_line);
	  key_data.dptr = key_line;
	  key_data.dsize = strlen (key_line)+1;
	  printf ("hash value = %x. \n\n", _gdbm_hash (key_data));
	  key_data.dptr = NULL;
	  break;

	case 'K':
	  _gdbm_print_bucket_cache (gdbm_file);
	  break;

	case 'V':
	  printf ("%s\n\n", gdbm_version);
	  break;

	case '?':
	  printf ("c - count (number of entries)\n");
	  printf ("d - delete\n");
	  printf ("f - fetch\n");
	  printf ("n - nextkey\n");
	  printf ("q - quit\n");
	  printf ("s - store\n");
	  printf ("1 - firstkey\n");
	  printf ("2 - nextkey on last key (from n, 1 or 2)\n\n");

	  printf ("r - reorganize\n");
	  printf ("A - print avail list\n");
	  printf ("B - get and print current bucket n\n");
	  printf ("C - print current bucket\n");
	  printf ("D - print hash directory\n");
	  printf ("F - print file header\n");
	  printf ("H - hash value of key\n");
	  printf ("K - print the bucket cache\n");
	  printf ("V - print version of gdbm\n");
	  break;

	default:
	  printf ("What? \n\n");
	  break;

	}
    }

  /* Quit normally. */
  exit (0);

}
