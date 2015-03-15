/* trunc.c: Set the size of an existing file, or create a file of a
 *          specified size.
 *
 * Copyright (C) 2008 Micah J. Cowan
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved. */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define PROGRAM_NAME  "trunc"

void
usage (FILE *f)
{
  fputs (
PROGRAM_NAME " [-c] file sz\n\
\n\
Set the filesize of FILE to SIZE.\n\
\n\
  -c: create FILE if it doesn't exist.\n\
\n\
  Multiplier suffixes for SIZE (case-insensitive):\n\
      k: SIZE * 1024\n\
      m: SIZE * 1024 * 1024\n", f);
}

off_t
get_size (const char str[])
{
  unsigned long val;
  int suffix;
  char *end;

  errno = 0;
  val = strtoul(str, &end, 10);
  if (end == str)
    {
      fputs (PROGRAM_NAME ": size is not a number.\n", stderr);
      usage (stderr);
      exit (EXIT_FAILURE);
    }
  else if (errno == ERANGE
           || (unsigned long)(off_t)val != val)
    {
      fputs (PROGRAM_NAME ": size is out of range.\n", stderr);
      exit (EXIT_FAILURE);
    }

  suffix = tolower ((unsigned char) end[0]);
  if (suffix == 'k')
    {
      val *= 1024;
    }
  else if (suffix == 'm')
    {
      val *= 1024 * 1024;
    }

  return val;
}

int
main (int argc, char *argv[])
{
  const char *fname;
  const char *szstr;
  off_t      sz;
  int        create = 0;
  int        option;
  int        fd;

#ifdef ENABLE_NLS
  /* Set the current locale.  */
  setlocale (LC_ALL, "");
  /* Set the text message domain.  */
  bindtextdomain ("wget", LOCALEDIR);
  textdomain ("wget");
#endif /* ENABLE_NLS */

  /* Parse options. */
  while ((option = getopt (argc, argv, "c")) != -1)
    {
      switch (option) {
        case 'c':
          create = 1;
          break;
        case '?':
          fprintf (stderr, PROGRAM_NAME ": Unrecognized option `%c'.\n\n",
                   optopt);
          usage (stderr);
          exit (EXIT_FAILURE);
        default:
          /* We shouldn't reach here. */
          abort();
      }
    }

  if (argv[optind] == NULL
      || argv[optind+1] == NULL
      || argv[optind+2] != NULL)
    {
      usage (stderr);
      exit (EXIT_FAILURE);
    }

  fname = argv[optind];
  szstr = argv[optind+1];

  sz = get_size(szstr);
  if (create)
    {
      mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
      fd = open(fname, O_WRONLY | O_CREAT, mode);
    }
  else
    {
      fd = open(fname, O_WRONLY);
    }

  if (fd == -1)
    {
      perror (PROGRAM_NAME ": open");
      exit (EXIT_FAILURE);
    }

  if (ftruncate(fd, sz) == -1)
    {
      perror (PROGRAM_NAME ": truncate");
      exit (EXIT_FAILURE);
    }

  if (close (fd) < 0)
    {
      perror (PROGRAM_NAME ": close");
      exit (EXIT_FAILURE);
    }

  return 0;
}
