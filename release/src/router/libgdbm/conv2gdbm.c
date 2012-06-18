/* conv2gdbm.c - This is a program to convert dbm files to gdbm files. */

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

#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "gdbm.h"

#include "getopt.h"

extern int dbminit ();
extern datum fetch ();
extern datum firstkey ();
extern datum nextkey ();

static void usage();

/* Boolean Constants */
#define TRUE 1
#define FALSE 0

int
main (argc, argv)
     int argc;
     char *argv[];
{
  GDBM_FILE gfile;	/* The gdbm file. */
  datum key;		/* Key and data pairs retrieved. */
  datum data;
  int   errors;		/* error count. */
  int   num;		/* insert count. */
  int   block_size;     /* gdbm block size. */
  char  quiet;		/* Do work Quietly? */
  char  option_char;    /* The option character. */

  char *dbm_file, *gdbm_file;   /* pointers to the file names. */

  /* Initialize things. */
  quiet = FALSE;
  block_size = 0;

  /* Check for proper arguments. */

  if (argc < 2)
    usage (argv[0]);

  /* Check for the options.  */
  while  ( (option_char = getopt (argc, argv, "b:q")) != EOF)
    {
      switch (option_char)
	{
	case 'b':
	  block_size = atoi (optarg);
	  break;

	case 'q':
	  quiet = TRUE;
	  break;

	default:
	  usage (argv[0]);
	}
    }

  /* The required dbm file name. */
  if (argc <= optind)
    {
      usage (argv[0]);
    }
  else
    {
      dbm_file = argv[optind];
      gdbm_file = argv[optind];
      optind += 1;
    }

  /* The optional gdbm file name. */
  if (argc > optind)
    {
      gdbm_file = argv[optind];
      optind += 1;
    }

  /* No more arguments are legal. */
  if (argc > optind)  usage (argv[0]);
  
  /* Open the dbm file. */
  if (dbminit (dbm_file) != 0)
    {
      printf ("%s: dbm file not opened\n", argv[0]);
      exit (2);
    }

  /* Open the gdbm file.  Since the dbm files have .pag and .dir we
     will use the file name without any extension.  */
  gfile = gdbm_open (gdbm_file, block_size, GDBM_WRCREAT, 00664, NULL);
  if (gfile == NULL)
    {
      printf ("%s: gdbm file not opened\n", argv[0]);
      exit (2);
    }


  /* Do the conversion.  */
  errors = 0;
  num = 0;

  if (!quiet)
    printf ("%s: Converting %s.pag and %s.dir to %s.\n", argv[0], dbm_file,
	    dbm_file, gdbm_file);

  /* The convert loop - read a key/data pair from the dbm file and insert
     it into the gdbm file. */

  for (key = firstkey (); key.dptr != NULL; key = nextkey (key))
    {
      data = fetch (key);
      if (gdbm_store (gfile, key, data, GDBM_INSERT) != 0)
	{
	  errors++;
	}
      else
	{
	  num++;
	  if ( !quiet && ((num % 100) == 0))
	    {
	      printf (".");
	      if ( (num % 7000) == 0)
		printf ("\n");
	    }
	}
    }

  gdbm_close (gfile);

  if (!quiet)
    {
      /* Final reporting. */
      if (errors)
	printf ("%s: %d items not inserted into %s.\n", argv[0],
	        errors, gdbm_file);

      printf  ("%s: %d items inserted into %s.\n", argv[0], num, gdbm_file);
    }
  exit(0);
  /* NOTREACHED */
}

static void usage (name)
     char *name;
{
  printf ("usage: %s [-q] [-b block_size] dbmfile [gdbmfile]\n", name);
  exit (2);
}
