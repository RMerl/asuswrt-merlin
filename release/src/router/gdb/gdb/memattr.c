/* Memory attributes support, for GDB.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   This file is part of GDB.

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

#include "defs.h"
#include "command.h"
#include "gdbcmd.h"
#include "memattr.h"
#include "target.h"
#include "value.h"
#include "language.h"
#include "vec.h"
#include "gdb_string.h"

const struct mem_attrib default_mem_attrib =
{
  MEM_RW,			/* mode */
  MEM_WIDTH_UNSPECIFIED,
  0,				/* hwbreak */
  0,				/* cache */
  0,				/* verify */
  -1 /* Flash blocksize not specified.  */
};

const struct mem_attrib unknown_mem_attrib =
{
  MEM_NONE,			/* mode */
  MEM_WIDTH_UNSPECIFIED,
  0,				/* hwbreak */
  0,				/* cache */
  0,				/* verify */
  -1 /* Flash blocksize not specified.  */
};


VEC(mem_region_s) *mem_region_list, *target_mem_region_list;
static int mem_number = 0;

/* If this flag is set, the memory region list should be automatically
   updated from the target.  If it is clear, the list is user-controlled
   and should be left alone.  */
static int mem_use_target = 1;

/* If this flag is set, we have tried to fetch the target memory regions
   since the last time it was invalidated.  If that list is still
   empty, then the target can't supply memory regions.  */
static int target_mem_regions_valid;

/* If this flag is set, gdb will assume that memory ranges not
   specified by the memory map have type MEM_NONE, and will
   emit errors on all accesses to that memory.  */
static int inaccessible_by_default = 0;

static void
show_inaccessible_by_default (struct ui_file *file, int from_tty,
			      struct cmd_list_element *c,
			      const char *value)
{
  if (inaccessible_by_default)
    fprintf_filtered (file, _("\
Unknown memory addresses will be treated as inaccessible.\n"));
  else
    fprintf_filtered (file, _("\
Unknown memory addresses will be treated as RAM.\n"));          
}


/* Predicate function which returns true if LHS should sort before RHS
   in a list of memory regions, useful for VEC_lower_bound.  */

static int
mem_region_lessthan (const struct mem_region *lhs,
		     const struct mem_region *rhs)
{
  return lhs->lo < rhs->lo;
}

/* A helper function suitable for qsort, used to sort a
   VEC(mem_region_s) by starting address.  */

int
mem_region_cmp (const void *untyped_lhs, const void *untyped_rhs)
{
  const struct mem_region *lhs = untyped_lhs;
  const struct mem_region *rhs = untyped_rhs;

  if (lhs->lo < rhs->lo)
    return -1;
  else if (lhs->lo == rhs->lo)
    return 0;
  else
    return 1;
}

/* Allocate a new memory region, with default settings.  */

void
mem_region_init (struct mem_region *new)
{
  memset (new, 0, sizeof (struct mem_region));
  new->enabled_p = 1;
  new->attrib = default_mem_attrib;
}

/* This function should be called before any command which would
   modify the memory region list.  It will handle switching from
   a target-provided list to a local list, if necessary.  */

static void
require_user_regions (int from_tty)
{
  struct mem_region *m;
  int ix, length;

  /* If we're already using a user-provided list, nothing to do.  */
  if (!mem_use_target)
    return;

  /* Switch to a user-provided list (possibly a copy of the current
     one).  */
  mem_use_target = 0;

  /* If we don't have a target-provided region list yet, then
     no need to warn.  */
  if (mem_region_list == NULL)
    return;

  /* Otherwise, let the user know how to get back.  */
  if (from_tty)
    warning (_("Switching to manual control of memory regions; use "
	       "\"mem auto\" to fetch regions from the target again."));

  /* And create a new list for the user to modify.  */
  length = VEC_length (mem_region_s, target_mem_region_list);
  mem_region_list = VEC_alloc (mem_region_s, length);
  for (ix = 0; VEC_iterate (mem_region_s, target_mem_region_list, ix, m); ix++)
    VEC_quick_push (mem_region_s, mem_region_list, m);
}

/* This function should be called before any command which would
   read the memory region list, other than those which call
   require_user_regions.  It will handle fetching the
   target-provided list, if necessary.  */

static void
require_target_regions (void)
{
  if (mem_use_target && !target_mem_regions_valid)
    {
      target_mem_regions_valid = 1;
      target_mem_region_list = target_memory_map ();
      mem_region_list = target_mem_region_list;
    }
}

static void
create_mem_region (CORE_ADDR lo, CORE_ADDR hi,
		   const struct mem_attrib *attrib)
{
  struct mem_region new;
  int i, ix;

  /* lo == hi is a useless empty region */
  if (lo >= hi && hi != 0)
    {
      printf_unfiltered (_("invalid memory region: low >= high\n"));
      return;
    }

  mem_region_init (&new);
  new.lo = lo;
  new.hi = hi;

  ix = VEC_lower_bound (mem_region_s, mem_region_list, &new,
			mem_region_lessthan);

  /* Check for an overlapping memory region.  We only need to check
     in the vicinity - at most one before and one after the
     insertion point.  */
  for (i = ix - 1; i < ix + 1; i++)
    {
      struct mem_region *n;

      if (i < 0)
	continue;
      if (i >= VEC_length (mem_region_s, mem_region_list))
	continue;

      n = VEC_index (mem_region_s, mem_region_list, i);

      if ((lo >= n->lo && (lo < n->hi || n->hi == 0)) 
	  || (hi > n->lo && (hi <= n->hi || n->hi == 0))
	  || (lo <= n->lo && (hi >= n->hi || hi == 0)))
	{
	  printf_unfiltered (_("overlapping memory region\n"));
	  return;
	}
    }

  new.number = ++mem_number;
  new.attrib = *attrib;
  VEC_safe_insert (mem_region_s, mem_region_list, ix, &new);
}

/*
 * Look up the memory region cooresponding to ADDR.
 */
struct mem_region *
lookup_mem_region (CORE_ADDR addr)
{
  static struct mem_region region;
  struct mem_region *m;
  CORE_ADDR lo;
  CORE_ADDR hi;
  int ix;

  require_target_regions ();

  /* First we initialize LO and HI so that they describe the entire
     memory space.  As we process the memory region chain, they are
     redefined to describe the minimal region containing ADDR.  LO
     and HI are used in the case where no memory region is defined
     that contains ADDR.  If a memory region is disabled, it is
     treated as if it does not exist.  The initial values for LO
     and HI represent the bottom and top of memory.  */

  lo = 0;
  hi = 0;

  /* Either find memory range containing ADDRESS, or set LO and HI
     to the nearest boundaries of an existing memory range.
     
     If we ever want to support a huge list of memory regions, this
     check should be replaced with a binary search (probably using
     VEC_lower_bound).  */
  for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
    {
      if (m->enabled_p == 1)
	{
	  /* If the address is in the memory region, return that memory range.  */
	  if (addr >= m->lo && (addr < m->hi || m->hi == 0))
	    return m;

	  /* This (correctly) won't match if m->hi == 0, representing
	     the top of the address space, because CORE_ADDR is unsigned;
	     no value of LO is less than zero.  */
	  if (addr >= m->hi && lo < m->hi)
	    lo = m->hi;

	  /* This will never set HI to zero; if we're here and ADDR
	     is at or below M, and the region starts at zero, then ADDR
	     would have been in the region.  */
	  if (addr <= m->lo && (hi == 0 || hi > m->lo))
	    hi = m->lo;
	}
    }

  /* Because no region was found, we must cons up one based on what
     was learned above.  */
  region.lo = lo;
  region.hi = hi;

  /* When no memory map is defined at all, we always return 
     'default_mem_attrib', so that we do not make all memory 
     inaccessible for targets that don't provide a memory map.  */
  if (inaccessible_by_default && !VEC_empty (mem_region_s, mem_region_list))
    region.attrib = unknown_mem_attrib;
  else
    region.attrib = default_mem_attrib;

  return &region;
}

/* Invalidate any memory regions fetched from the target.  */

void
invalidate_target_mem_regions (void)
{
  struct mem_region *m;
  int ix;

  if (!target_mem_regions_valid)
    return;

  target_mem_regions_valid = 0;
  VEC_free (mem_region_s, target_mem_region_list);
  if (mem_use_target)
    mem_region_list = NULL;
}

/* Clear memory region list */

static void
mem_clear (void)
{
  VEC_free (mem_region_s, mem_region_list);
}


static void
mem_command (char *args, int from_tty)
{
  CORE_ADDR lo, hi;
  char *tok;
  struct mem_attrib attrib;

  if (!args)
    error_no_arg (_("No mem"));

  /* For "mem auto", switch back to using a target provided list.  */
  if (strcmp (args, "auto") == 0)
    {
      if (mem_use_target)
	return;

      if (mem_region_list != target_mem_region_list)
	{
	  mem_clear ();
	  mem_region_list = target_mem_region_list;
	}

      mem_use_target = 1;
      return;
    }

  require_user_regions (from_tty);

  tok = strtok (args, " \t");
  if (!tok)
    error (_("no lo address"));
  lo = parse_and_eval_address (tok);

  tok = strtok (NULL, " \t");
  if (!tok)
    error (_("no hi address"));
  hi = parse_and_eval_address (tok);

  attrib = default_mem_attrib;
  while ((tok = strtok (NULL, " \t")) != NULL)
    {
      if (strcmp (tok, "rw") == 0)
	attrib.mode = MEM_RW;
      else if (strcmp (tok, "ro") == 0)
	attrib.mode = MEM_RO;
      else if (strcmp (tok, "wo") == 0)
	attrib.mode = MEM_WO;

      else if (strcmp (tok, "8") == 0)
	attrib.width = MEM_WIDTH_8;
      else if (strcmp (tok, "16") == 0)
	{
	  if ((lo % 2 != 0) || (hi % 2 != 0))
	    error (_("region bounds not 16 bit aligned"));
	  attrib.width = MEM_WIDTH_16;
	}
      else if (strcmp (tok, "32") == 0)
	{
	  if ((lo % 4 != 0) || (hi % 4 != 0))
	    error (_("region bounds not 32 bit aligned"));
	  attrib.width = MEM_WIDTH_32;
	}
      else if (strcmp (tok, "64") == 0)
	{
	  if ((lo % 8 != 0) || (hi % 8 != 0))
	    error (_("region bounds not 64 bit aligned"));
	  attrib.width = MEM_WIDTH_64;
	}

#if 0
      else if (strcmp (tok, "hwbreak") == 0)
	attrib.hwbreak = 1;
      else if (strcmp (tok, "swbreak") == 0)
	attrib.hwbreak = 0;
#endif

      else if (strcmp (tok, "cache") == 0)
	attrib.cache = 1;
      else if (strcmp (tok, "nocache") == 0)
	attrib.cache = 0;

#if 0
      else if (strcmp (tok, "verify") == 0)
	attrib.verify = 1;
      else if (strcmp (tok, "noverify") == 0)
	attrib.verify = 0;
#endif

      else
	error (_("unknown attribute: %s"), tok);
    }

  create_mem_region (lo, hi, &attrib);
}


static void
mem_info_command (char *args, int from_tty)
{
  struct mem_region *m;
  struct mem_attrib *attrib;
  int ix;

  if (mem_use_target)
    printf_filtered (_("Using memory regions provided by the target.\n"));
  else
    printf_filtered (_("Using user-defined memory regions.\n"));

  require_target_regions ();

  if (!mem_region_list)
    {
      printf_unfiltered (_("There are no memory regions defined.\n"));
      return;
    }

  printf_filtered ("Num ");
  printf_filtered ("Enb ");
  printf_filtered ("Low Addr   ");
  if (gdbarch_addr_bit (current_gdbarch) > 32)
    printf_filtered ("        ");
  printf_filtered ("High Addr  ");
  if (gdbarch_addr_bit (current_gdbarch) > 32)
    printf_filtered ("        ");
  printf_filtered ("Attrs ");
  printf_filtered ("\n");

  for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
    {
      char *tmp;
      printf_filtered ("%-3d %-3c\t",
		       m->number,
		       m->enabled_p ? 'y' : 'n');
      if (gdbarch_addr_bit (current_gdbarch) <= 32)
	tmp = hex_string_custom ((unsigned long) m->lo, 8);
      else
	tmp = hex_string_custom ((unsigned long) m->lo, 16);
      
      printf_filtered ("%s ", tmp);

      if (gdbarch_addr_bit (current_gdbarch) <= 32)
	{
	if (m->hi == 0)
	  tmp = "0x100000000";
	else
	  tmp = hex_string_custom ((unsigned long) m->hi, 8);
	}
      else
	{
	if (m->hi == 0)
	  tmp = "0x10000000000000000";
	else
	  tmp = hex_string_custom ((unsigned long) m->hi, 16);
	}

      printf_filtered ("%s ", tmp);

      /* Print a token for each attribute.

       * FIXME: Should we output a comma after each token?  It may
       * make it easier for users to read, but we'd lose the ability
       * to cut-and-paste the list of attributes when defining a new
       * region.  Perhaps that is not important.
       *
       * FIXME: If more attributes are added to GDB, the output may
       * become cluttered and difficult for users to read.  At that
       * time, we may want to consider printing tokens only if they
       * are different from the default attribute.  */

      attrib = &m->attrib;
      switch (attrib->mode)
	{
	case MEM_RW:
	  printf_filtered ("rw ");
	  break;
	case MEM_RO:
	  printf_filtered ("ro ");
	  break;
	case MEM_WO:
	  printf_filtered ("wo ");
	  break;
	case MEM_FLASH:
	  printf_filtered ("flash blocksize 0x%x ", attrib->blocksize);
	  break;
	}

      switch (attrib->width)
	{
	case MEM_WIDTH_8:
	  printf_filtered ("8 ");
	  break;
	case MEM_WIDTH_16:
	  printf_filtered ("16 ");
	  break;
	case MEM_WIDTH_32:
	  printf_filtered ("32 ");
	  break;
	case MEM_WIDTH_64:
	  printf_filtered ("64 ");
	  break;
	case MEM_WIDTH_UNSPECIFIED:
	  break;
	}

#if 0
      if (attrib->hwbreak)
	printf_filtered ("hwbreak");
      else
	printf_filtered ("swbreak");
#endif

      if (attrib->cache)
	printf_filtered ("cache ");
      else
	printf_filtered ("nocache ");

#if 0
      if (attrib->verify)
	printf_filtered ("verify ");
      else
	printf_filtered ("noverify ");
#endif

      printf_filtered ("\n");

      gdb_flush (gdb_stdout);
    }
}


/* Enable the memory region number NUM. */

static void
mem_enable (int num)
{
  struct mem_region *m;
  int ix;

  for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
    if (m->number == num)
      {
	m->enabled_p = 1;
	return;
      }
  printf_unfiltered (_("No memory region number %d.\n"), num);
}

static void
mem_enable_command (char *args, int from_tty)
{
  char *p = args;
  char *p1;
  int num;
  struct mem_region *m;
  int ix;

  require_user_regions (from_tty);

  dcache_invalidate (target_dcache);

  if (p == 0)
    {
      for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
	m->enabled_p = 1;
    }
  else
    while (*p)
      {
	p1 = p;
	while (*p1 >= '0' && *p1 <= '9')
	  p1++;
	if (*p1 && *p1 != ' ' && *p1 != '\t')
	  error (_("Arguments must be memory region numbers."));

	num = atoi (p);
	mem_enable (num);

	p = p1;
	while (*p == ' ' || *p == '\t')
	  p++;
      }
}


/* Disable the memory region number NUM. */

static void
mem_disable (int num)
{
  struct mem_region *m;
  int ix;

  for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
    if (m->number == num)
      {
	m->enabled_p = 0;
	return;
      }
  printf_unfiltered (_("No memory region number %d.\n"), num);
}

static void
mem_disable_command (char *args, int from_tty)
{
  char *p = args;
  char *p1;
  int num;
  struct mem_region *m;
  int ix;

  require_user_regions (from_tty);

  dcache_invalidate (target_dcache);

  if (p == 0)
    {
      for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
	m->enabled_p = 0;
    }
  else
    while (*p)
      {
	p1 = p;
	while (*p1 >= '0' && *p1 <= '9')
	  p1++;
	if (*p1 && *p1 != ' ' && *p1 != '\t')
	  error (_("Arguments must be memory region numbers."));

	num = atoi (p);
	mem_disable (num);

	p = p1;
	while (*p == ' ' || *p == '\t')
	  p++;
      }
}

/* Delete the memory region number NUM. */

static void
mem_delete (int num)
{
  struct mem_region *m1, *m;
  int ix;

  if (!mem_region_list)
    {
      printf_unfiltered (_("No memory region number %d.\n"), num);
      return;
    }

  for (ix = 0; VEC_iterate (mem_region_s, mem_region_list, ix, m); ix++)
    if (m->number == num)
      break;

  if (m == NULL)
    {
      printf_unfiltered (_("No memory region number %d.\n"), num);
      return;
    }

  VEC_ordered_remove (mem_region_s, mem_region_list, ix);
}

static void
mem_delete_command (char *args, int from_tty)
{
  char *p = args;
  char *p1;
  int num;

  require_user_regions (from_tty);

  dcache_invalidate (target_dcache);

  if (p == 0)
    {
      if (query ("Delete all memory regions? "))
	mem_clear ();
      dont_repeat ();
      return;
    }

  while (*p)
    {
      p1 = p;
      while (*p1 >= '0' && *p1 <= '9')
	p1++;
      if (*p1 && *p1 != ' ' && *p1 != '\t')
	error (_("Arguments must be memory region numbers."));

      num = atoi (p);
      mem_delete (num);

      p = p1;
      while (*p == ' ' || *p == '\t')
	p++;
    }

  dont_repeat ();
}

static void
dummy_cmd (char *args, int from_tty)
{
}

extern initialize_file_ftype _initialize_mem; /* -Wmissing-prototype */

static struct cmd_list_element *mem_set_cmdlist;
static struct cmd_list_element *mem_show_cmdlist;

void
_initialize_mem (void)
{
  add_com ("mem", class_vars, mem_command, _("\
Define attributes for memory region or reset memory region handling to\n\
target-based.\n\
Usage: mem auto\n\
       mem <lo addr> <hi addr> [<mode> <width> <cache>], \n\
where <mode>  may be rw (read/write), ro (read-only) or wo (write-only), \n\
      <width> may be 8, 16, 32, or 64, and \n\
      <cache> may be cache or nocache"));

  add_cmd ("mem", class_vars, mem_enable_command, _("\
Enable memory region.\n\
Arguments are the code numbers of the memory regions to enable.\n\
Usage: enable mem <code number>\n\
Do \"info mem\" to see current list of code numbers."), &enablelist);

  add_cmd ("mem", class_vars, mem_disable_command, _("\
Disable memory region.\n\
Arguments are the code numbers of the memory regions to disable.\n\
Usage: disable mem <code number>\n\
Do \"info mem\" to see current list of code numbers."), &disablelist);

  add_cmd ("mem", class_vars, mem_delete_command, _("\
Delete memory region.\n\
Arguments are the code numbers of the memory regions to delete.\n\
Usage: delete mem <code number>\n\
Do \"info mem\" to see current list of code numbers."), &deletelist);

  add_info ("mem", mem_info_command,
	    _("Memory region attributes"));

  add_prefix_cmd ("mem", class_vars, dummy_cmd, _("\
Memory regions settings"),
		  &mem_set_cmdlist, "set mem ",
		  0/* allow-unknown */, &setlist);
  add_prefix_cmd ("mem", class_vars, dummy_cmd, _("\
Memory regions settings"),
		  &mem_show_cmdlist, "show mem  ",
		  0/* allow-unknown */, &showlist);

  add_setshow_boolean_cmd ("inaccessible-by-default", no_class,
				  &inaccessible_by_default, _("\
Set handling of unknown memory regions."), _("\
Show handling of unknown memory regions."), _("\
If on, and some memory map is defined, debugger will emit errors on\n\
accesses to memory not defined in the memory map. If off, accesses to all\n\
memory addresses will be allowed."),
				NULL,
				show_inaccessible_by_default,
				&mem_set_cmdlist,
				&mem_show_cmdlist);
}
