/* Simulator memory option handling.
   Copyright (C) 1996-1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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

#include "cconfig.h"

#include "sim-main.h"
#include "sim-assert.h"
#include "sim-options.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Memory fill byte. */
static unsigned8 fill_byte_value;
static int fill_byte_flag = 0;

/* Memory mapping; see OPTION_MEMORY_MAPFILE. */
static int mmap_next_fd = -1;

/* Memory command line options. */

enum {
  OPTION_MEMORY_DELETE = OPTION_START,
  OPTION_MEMORY_REGION,
  OPTION_MEMORY_SIZE,
  OPTION_MEMORY_INFO,
  OPTION_MEMORY_ALIAS,
  OPTION_MEMORY_CLEAR,
  OPTION_MEMORY_FILL,
  OPTION_MEMORY_MAPFILE
};

static DECLARE_OPTION_HANDLER (memory_option_handler);

static const OPTION memory_options[] =
{
  { {"memory-delete", required_argument, NULL, OPTION_MEMORY_DELETE },
      '\0', "ADDRESS|all", "Delete memory at ADDRESS (all addresses)",
      memory_option_handler },
  { {"delete-memory", required_argument, NULL, OPTION_MEMORY_DELETE },
      '\0', "ADDRESS", NULL,
      memory_option_handler },

  { {"memory-region", required_argument, NULL, OPTION_MEMORY_REGION },
      '\0', "ADDRESS,SIZE[,MODULO]", "Add a memory region",
      memory_option_handler },

  { {"memory-alias", required_argument, NULL, OPTION_MEMORY_ALIAS },
      '\0', "ADDRESS,SIZE{,ADDRESS}", "Add memory shadow",
      memory_option_handler },

  { {"memory-size", required_argument, NULL, OPTION_MEMORY_SIZE },
      '\0', "<size>[in bytes, Kb (k suffix), Mb (m suffix) or Gb (g suffix)]",
     "Add memory at address zero", memory_option_handler },

  { {"memory-fill", required_argument, NULL, OPTION_MEMORY_FILL },
      '\0', "VALUE", "Fill subsequently added memory regions",
      memory_option_handler },

  { {"memory-clear", no_argument, NULL, OPTION_MEMORY_CLEAR },
      '\0', NULL, "Clear subsequently added memory regions",
      memory_option_handler },

#if defined(HAVE_MMAP) && defined(HAVE_MUNMAP)
  { {"memory-mapfile", required_argument, NULL, OPTION_MEMORY_MAPFILE },
      '\0', "FILE", "Memory-map next memory region from file",
      memory_option_handler },
#endif

  { {"memory-info", no_argument, NULL, OPTION_MEMORY_INFO },
      '\0', NULL, "List configurable memory regions",
      memory_option_handler },
  { {"info-memory", no_argument, NULL, OPTION_MEMORY_INFO },
      '\0', NULL, NULL,
      memory_option_handler },

  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};


static sim_memopt *
do_memopt_add (SIM_DESC sd,
	       int level,
	       int space,
	       address_word addr,
	       address_word nr_bytes,
	       unsigned modulo,
	       sim_memopt **entry,
	       void *buffer)
{
  void *fill_buffer;
  unsigned fill_length;
  void *free_buffer;
  unsigned long free_length;

  if (buffer != NULL)
    {
      /* Buffer already given.  sim_memory_uninstall will free it. */
      sim_core_attach (sd, NULL,
		       level, access_read_write_exec, space,
		       addr, nr_bytes, modulo, NULL, buffer);

      free_buffer = buffer;
      free_length = 0;
      fill_buffer = buffer;
      fill_length = (modulo == 0) ? nr_bytes : modulo;
    }
  else
    {
      /* Allocate new well-aligned buffer, just as sim_core_attach(). */
      void *aligned_buffer;
      int padding = (addr % sizeof (unsigned64));
      unsigned long bytes = (modulo == 0 ? nr_bytes : modulo) + padding;

      free_buffer = NULL;
      free_length = bytes;

#ifdef HAVE_MMAP
      /* Memory map or malloc(). */
      if (mmap_next_fd >= 0)
	{
	  /* Check that given file is big enough. */
	  struct stat s;
	  int rc;

	  /* Some kernels will SIGBUS the application if mmap'd file
	     is not large enough.  */ 
	  rc = fstat (mmap_next_fd, &s);
	  if (rc < 0 || s.st_size < bytes)
	    {
	      sim_io_error (sd,
			    "Error, cannot confirm that mmap file is large enough "
			    "(>= %ld bytes)\n", bytes);
	    }

	  free_buffer = mmap (0, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, mmap_next_fd, 0);
	  if (free_buffer == 0 || free_buffer == (char*)-1) /* MAP_FAILED */
	    {
	      sim_io_error (sd, "Error, cannot mmap file (%s).\n",
			    strerror(errno));
	    }
	}
#endif 

      /* Need heap allocation? */ 
      if (free_buffer == NULL)
	{
	  /* If filling with non-zero value, do not use clearing allocator. */
	  if (fill_byte_flag && fill_byte_value != 0)
	    free_buffer = xmalloc (bytes); /* don't clear */
	  else
	    free_buffer = zalloc (bytes); /* clear */
	}

      aligned_buffer = (char*) free_buffer + padding;

      sim_core_attach (sd, NULL,
		       level, access_read_write_exec, space,
		       addr, nr_bytes, modulo, NULL, aligned_buffer);

      fill_buffer = aligned_buffer;
      fill_length = (modulo == 0) ? nr_bytes : modulo;

      /* If we just used a clearing allocator, and are about to fill with
         zero, truncate the redundant fill operation. */

      if (fill_byte_flag && fill_byte_value == 0)
         fill_length = 1; /* avoid boundary length=0 case */
    }

  if (fill_byte_flag)
    {
      ASSERT (fill_buffer != 0);
      memset ((char*) fill_buffer, fill_byte_value, fill_length);
    }

  while ((*entry) != NULL)
    entry = &(*entry)->next;
  (*entry) = ZALLOC (sim_memopt);
  (*entry)->level = level;
  (*entry)->space = space;
  (*entry)->addr = addr;
  (*entry)->nr_bytes = nr_bytes;
  (*entry)->modulo = modulo;
  (*entry)->buffer = free_buffer;

  /* Record memory unmapping info.  */
  if (mmap_next_fd >= 0)
    {
      (*entry)->munmap_length = free_length;
      close (mmap_next_fd);
      mmap_next_fd = -1;
    }
  else
    (*entry)->munmap_length = 0;

  return (*entry);
}

static SIM_RC
do_memopt_delete (SIM_DESC sd,
		  int level,
		  int space,
		  address_word addr)
{
  sim_memopt **entry = &STATE_MEMOPT (sd);
  sim_memopt *alias;
  while ((*entry) != NULL
	 && ((*entry)->level != level
	      || (*entry)->space != space
	      || (*entry)->addr != addr))
    entry = &(*entry)->next;
  if ((*entry) == NULL)
    {
      sim_io_eprintf (sd, "Memory at 0x%lx not found, not deleted\n",
		      (long) addr);
      return SIM_RC_FAIL;
    }
  /* delete any buffer */
  if ((*entry)->buffer != NULL)
    {
#ifdef HAVE_MUNMAP
      if ((*entry)->munmap_length > 0)
	munmap ((*entry)->buffer, (*entry)->munmap_length);
      else
#endif
	zfree ((*entry)->buffer);
    }

  /* delete it and its aliases */
  alias = *entry;
  *entry = (*entry)->next;
  while (alias != NULL)
    {
      sim_memopt *dead = alias;
      alias = alias->alias;
      sim_core_detach (sd, NULL, dead->level, dead->space, dead->addr);
      zfree (dead);
    }
  return SIM_RC_OK;
}


static char *
parse_size (char *chp,
	    address_word *nr_bytes,
	    unsigned *modulo)
{
  /* <nr_bytes>[K|M|G] [ "%" <modulo> ] */
  *nr_bytes = strtoul (chp, &chp, 0);
  switch (*chp)
    {
    case '%':
      *modulo = strtoul (chp + 1, &chp, 0);
      break;
    case 'g': case 'G': /* Gigabyte suffix.  */
      *nr_bytes <<= 10;
      /* Fall through.  */
    case 'm': case 'M': /* Megabyte suffix.  */
      *nr_bytes <<= 10;
      /* Fall through.  */
    case 'k': case 'K': /* Kilobyte suffix.  */
      *nr_bytes <<= 10;
      /* Check for a modulo specifier after the suffix.  */
      ++ chp;
      if (* chp == 'b' || * chp == 'B')
	++ chp;
      if (* chp == '%')
	*modulo = strtoul (chp + 1, &chp, 0);
      break;
    }
  return chp;
}

static char *
parse_ulong_value (char *chp,
		     unsigned long *value)
{
  *value = strtoul (chp, &chp, 0);
  return chp;
}

static char *
parse_addr (char *chp,
	    int *level,
	    int *space,
	    address_word *addr)
{
  /* [ <space> ": " ] <addr> [ "@" <level> ] */
  *addr = (unsigned long) strtoul (chp, &chp, 0);
  if (*chp == ':')
    {
      *space = *addr;
      *addr = (unsigned long) strtoul (chp + 1, &chp, 0);
    }
  if (*chp == '@')
    {
      *level = strtoul (chp + 1, &chp, 0);
    }
  return chp;
}


static SIM_RC
memory_option_handler (SIM_DESC sd, sim_cpu *cpu, int opt,
		       char *arg, int is_command)
{
  switch (opt)
    {

    case OPTION_MEMORY_DELETE:
      if (strcasecmp (arg, "all") == 0)
	{
	  while (STATE_MEMOPT (sd) != NULL)
	    do_memopt_delete (sd,
			      STATE_MEMOPT (sd)->level,
			      STATE_MEMOPT (sd)->space,
			      STATE_MEMOPT (sd)->addr);
	  return SIM_RC_OK;
	}
      else
	{
	  int level = 0;
	  int space = 0;
	  address_word addr = 0;
	  parse_addr (arg, &level, &space, &addr);
	  return do_memopt_delete (sd, level, space, addr);
	}
    
    case OPTION_MEMORY_REGION:
      {
	char *chp = arg;
	int level = 0;
	int space = 0;
	address_word addr = 0;
	address_word nr_bytes = 0;
	unsigned modulo = 0;
	/* parse the arguments */
	chp = parse_addr (chp, &level, &space, &addr);
	if (*chp != ',')
	  {
	    sim_io_eprintf (sd, "Missing size for memory-region\n");
	    return SIM_RC_FAIL;
	  }
	chp = parse_size (chp + 1, &nr_bytes, &modulo);
	/* old style */
	if (*chp == ',')
	  modulo = strtoul (chp + 1, &chp, 0);
	/* try to attach/insert it */
	do_memopt_add (sd, level, space, addr, nr_bytes, modulo,
		       &STATE_MEMOPT (sd), NULL);
	return SIM_RC_OK;
      }

    case OPTION_MEMORY_ALIAS:
      {
	char *chp = arg;
	int level = 0;
	int space = 0;
	address_word addr = 0;
	address_word nr_bytes = 0;
	unsigned modulo = 0;
	sim_memopt *entry;
	/* parse the arguments */
	chp = parse_addr (chp, &level, &space, &addr);
	if (*chp != ',')
	  {
	    sim_io_eprintf (sd, "Missing size for memory-region\n");
	    return SIM_RC_FAIL;
	  }
	chp = parse_size (chp + 1, &nr_bytes, &modulo);
	/* try to attach/insert the main record */
	entry = do_memopt_add (sd, level, space, addr, nr_bytes, modulo,
			       &STATE_MEMOPT (sd),
			       NULL);
	/* now attach all the aliases */
	while (*chp == ',')
	  {
	    int a_level = level;
	    int a_space = space;
	    address_word a_addr = addr;
	    chp = parse_addr (chp + 1, &a_level, &a_space, &a_addr);
	    do_memopt_add (sd, a_level, a_space, a_addr, nr_bytes, modulo,
			   &entry->alias, entry->buffer);
	  }
	return SIM_RC_OK;
      }

    case OPTION_MEMORY_SIZE:
      {
	int level = 0;
	int space = 0;
	address_word addr = 0;
	address_word nr_bytes = 0;
	unsigned modulo = 0;
	/* parse the arguments */
	parse_size (arg, &nr_bytes, &modulo);
	/* try to attach/insert it */
	do_memopt_add (sd, level, space, addr, nr_bytes, modulo,
		       &STATE_MEMOPT (sd), NULL);
	return SIM_RC_OK;
      }

    case OPTION_MEMORY_CLEAR:
      {
	fill_byte_value = (unsigned8) 0;
	fill_byte_flag = 1;
	return SIM_RC_OK;
	break;
      }

    case OPTION_MEMORY_FILL:
      {
	unsigned long fill_value;
	parse_ulong_value (arg, &fill_value);
	if (fill_value > 255)
	  {
	    sim_io_eprintf (sd, "Missing fill value between 0 and 255\n");
	    return SIM_RC_FAIL;
	  }
	fill_byte_value = (unsigned8) fill_value;
	fill_byte_flag = 1;
	return SIM_RC_OK;
	break;
      }

    case OPTION_MEMORY_MAPFILE:
      {
	if (mmap_next_fd >= 0)
	  {
	    sim_io_eprintf (sd, "Duplicate memory-mapfile option\n");
	    return SIM_RC_FAIL;
	  }

	mmap_next_fd = open (arg, O_RDWR);
	if (mmap_next_fd < 0)
	  {
	    sim_io_eprintf (sd, "Cannot open file `%s': %s\n",
			    arg, strerror(errno));
	    return SIM_RC_FAIL;
	  }

	return SIM_RC_OK;
      }

    case OPTION_MEMORY_INFO:
      {
	sim_memopt *entry;
	sim_io_printf (sd, "Memory maps:\n");
	for (entry = STATE_MEMOPT (sd); entry != NULL; entry = entry->next)
	  {
	    sim_memopt *alias;
	    sim_io_printf (sd, " memory");
	    if (entry->alias == NULL)
	      sim_io_printf (sd, " region ");
	    else
	      sim_io_printf (sd, " alias ");
	    if (entry->space != 0)
	      sim_io_printf (sd, "0x%lx:", (long) entry->space);
	    sim_io_printf (sd, "0x%08lx", (long) entry->addr);
	    if (entry->level != 0)
	      sim_io_printf (sd, "@0x%lx", (long) entry->level);
	    sim_io_printf (sd, ",0x%lx",
			   (long) entry->nr_bytes);
	    if (entry->modulo != 0)
	      sim_io_printf (sd, "%%0x%lx", (long) entry->modulo);
	    for (alias = entry->alias;
		 alias != NULL;
		 alias = alias->next)
	      {
		if (alias->space != 0)
		  sim_io_printf (sd, "0x%lx:", (long) alias->space);
		sim_io_printf (sd, ",0x%08lx", (long) alias->addr);
		if (alias->level != 0)
		  sim_io_printf (sd, "@0x%lx", (long) alias->level);
	      }
	    sim_io_printf (sd, "\n");
	  }
	return SIM_RC_OK;
	break;
      }

    default:
      sim_io_eprintf (sd, "Unknown memory option %d\n", opt);
      return SIM_RC_FAIL;

    }

  return SIM_RC_FAIL;
}


/* "memory" module install handler.

   This is called via sim_module_install to install the "memory" subsystem
   into the simulator.  */

static MODULE_INIT_FN sim_memory_init;
static MODULE_UNINSTALL_FN sim_memory_uninstall;

SIM_RC
sim_memopt_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_add_option_table (sd, NULL, memory_options);
  sim_module_add_uninstall_fn (sd, sim_memory_uninstall);
  sim_module_add_init_fn (sd, sim_memory_init);
  return SIM_RC_OK;
}


/* Uninstall the "memory" subsystem from the simulator.  */

static void
sim_memory_uninstall (SIM_DESC sd)
{
  sim_memopt **entry = &STATE_MEMOPT (sd);
  sim_memopt *alias;

  while ((*entry) != NULL)
    {
      /* delete any buffer */
      if ((*entry)->buffer != NULL)
	{
#ifdef HAVE_MUNMAP
	  if ((*entry)->munmap_length > 0)
	    munmap ((*entry)->buffer, (*entry)->munmap_length);
	  else
#endif
	    zfree ((*entry)->buffer);
	}

      /* delete it and its aliases */
      alias = *entry;

      /* next victim */
      *entry = (*entry)->next;

      while (alias != NULL)
	{
	  sim_memopt *dead = alias;
	  alias = alias->alias;
	  sim_core_detach (sd, NULL, dead->level, dead->space, dead->addr);
	  zfree (dead);
	}
    }
}


static SIM_RC
sim_memory_init (SIM_DESC sd)
{
  /* Reinitialize option modifier flags, in case they were left
     over from a previous sim startup event.  */
  fill_byte_flag = 0;
  mmap_next_fd = -1;

  return SIM_RC_OK;
}
