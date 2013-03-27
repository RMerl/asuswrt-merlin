/* Utility to load a file into the simulator.
   Copyright (C) 1997, 1998, 2001, 2002, 2004, 2007
   Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* This is a standalone loader, independent of the sim-basic.h machinery,
   as it is used by simulators that don't use it [though that doesn't mean
   to suggest that they shouldn't :-)].  */

#ifdef HAVE_CONFIG_H
#include "cconfig.h"
#endif
#include "ansidecl.h"
#include <stdio.h> /* for NULL */
#include <stdarg.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <time.h>

#include "sim-basics.h"
#include "bfd.h"
#include "sim-utils.h"

#include "gdb/callback.h"
#include "gdb/remote-sim.h"

static void eprintf PARAMS ((host_callback *, const char *, ...));
static void xprintf PARAMS ((host_callback *, const char *, ...));
static void report_transfer_performance
  PARAMS ((host_callback *, unsigned long, time_t, time_t));
static void xprintf_bfd_vma PARAMS ((host_callback *, bfd_vma));

/* Load program PROG into the simulator using the function DO_LOAD.
   If PROG_BFD is non-NULL, the file has already been opened.
   If VERBOSE_P is non-zero statistics are printed of each loaded section
   and the transfer rate (for consistency with gdb).
   If LMA_P is non-zero the program sections are loaded at the LMA
   rather than the VMA
   If this fails an error message is printed and NULL is returned.
   If it succeeds the bfd is returned.
   NOTE: For historical reasons, older hardware simulators incorrectly
   write the program sections at LMA interpreted as a virtual address.
   This is still accommodated for backward compatibility reasons. */


bfd *
sim_load_file (sd, myname, callback, prog, prog_bfd, verbose_p, lma_p, do_write)
     SIM_DESC sd;
     const char *myname;
     host_callback *callback;
     char *prog;
     bfd *prog_bfd;
     int verbose_p;
     int lma_p;
     sim_write_fn do_write;
{
  asection *s;
  /* Record separately as we don't want to close PROG_BFD if it was passed.  */
  bfd *result_bfd;
  time_t start_time = 0;	/* Start and end times of download */
  time_t end_time = 0;
  unsigned long data_count = 0;	/* Number of bytes transferred to memory */
  int found_loadable_section;

  if (prog_bfd != NULL)
    result_bfd = prog_bfd;
  else
    {
      result_bfd = bfd_openr (prog, 0);
      if (result_bfd == NULL)
	{
	  eprintf (callback, "%s: can't open \"%s\": %s\n", 
		   myname, prog, bfd_errmsg (bfd_get_error ()));
	  return NULL;
	}
    }

  if (!bfd_check_format (result_bfd, bfd_object)) 
    {
      eprintf (callback, "%s: \"%s\" is not an object file: %s\n",
	       myname, prog, bfd_errmsg (bfd_get_error ()));
      /* Only close if we opened it.  */
      if (prog_bfd == NULL)
	bfd_close (result_bfd);
      return NULL;
    }

  if (verbose_p)
    start_time = time (NULL);

  found_loadable_section = 0;
  for (s = result_bfd->sections; s; s = s->next) 
    {
      if (s->flags & SEC_LOAD) 
	{
	  bfd_size_type size;

	  size = bfd_get_section_size (s);
	  if (size > 0)
	    {
	      char *buffer;
	      bfd_vma lma;

	      buffer = malloc (size);
	      if (buffer == NULL)
		{
		  eprintf (callback,
			   "%s: insufficient memory to load \"%s\"\n",
			   myname, prog);
		  /* Only close if we opened it.  */
		  if (prog_bfd == NULL)
		    bfd_close (result_bfd);
		  return NULL;
		}
	      if (lma_p)
		lma = bfd_section_lma (result_bfd, s);
	      else
		lma = bfd_section_vma (result_bfd, s);
	      if (verbose_p)
		{
		  xprintf (callback, "Loading section %s, size 0x%lx %s ",
			   bfd_get_section_name (result_bfd, s),
			   (unsigned long) size,
			   (lma_p ? "lma" : "vma"));
		  xprintf_bfd_vma (callback, lma);
		  xprintf (callback, "\n");
		}
	      data_count += size;
	      bfd_get_section_contents (result_bfd, s, buffer, 0, size);
	      do_write (sd, lma, buffer, size);
	      found_loadable_section = 1;
	      free (buffer);
	    }
	}
    }

  if (!found_loadable_section)
    {
      eprintf (callback,
	       "%s: no loadable sections \"%s\"\n",
	       myname, prog);
      return NULL;
    }

  if (verbose_p)
    {
      end_time = time (NULL);
      xprintf (callback, "Start address ");
      xprintf_bfd_vma (callback, bfd_get_start_address (result_bfd));
      xprintf (callback, "\n");
      report_transfer_performance (callback, data_count, start_time, end_time);
    }

  bfd_cache_close (result_bfd);

  return result_bfd;
}

static void
xprintf VPARAMS ((host_callback *callback, const char *fmt, ...))
{
  va_list ap;

  VA_START (ap, fmt);

  (*callback->vprintf_filtered) (callback, fmt, ap);

  va_end (ap);
}

static void
eprintf VPARAMS ((host_callback *callback, const char *fmt, ...))
{
  va_list ap;

  VA_START (ap, fmt);

  (*callback->evprintf_filtered) (callback, fmt, ap);

  va_end (ap);
}

/* Report how fast the transfer went. */

static void
report_transfer_performance (callback, data_count, start_time, end_time)
     host_callback *callback;
     unsigned long data_count;
     time_t start_time, end_time;
{
  xprintf (callback, "Transfer rate: ");
  if (end_time != start_time)
    xprintf (callback, "%ld bits/sec",
	     (data_count * 8) / (end_time - start_time));
  else
    xprintf (callback, "%ld bits in <1 sec", (data_count * 8));
  xprintf (callback, ".\n");
}

/* Print a bfd_vma.
   This is intended to handle the vagaries of 32 vs 64 bits, etc.  */

static void
xprintf_bfd_vma (callback, vma)
     host_callback *callback;
     bfd_vma vma;
{
  /* FIXME: for now */
  xprintf (callback, "0x%lx", (unsigned long) vma);
}
