/* Dynamic architecture support for GDB, the GNU debugger.

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
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

#include "arch-utils.h"
#include "buildsym.h"
#include "gdbcmd.h"
#include "inferior.h"		/* enum CALL_DUMMY_LOCATION et.al. */
#include "gdb_string.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "sim-regno.h"
#include "gdbcore.h"
#include "osabi.h"
#include "target-descriptions.h"

#include "version.h"

#include "floatformat.h"

int
always_use_struct_convention (int gcc_p, struct type *value_type)
{
  return 1;
}

enum return_value_convention
legacy_return_value (struct gdbarch *gdbarch, struct type *valtype,
		     struct regcache *regcache, gdb_byte *readbuf,
		     const gdb_byte *writebuf)
{
  /* NOTE: cagney/2004-06-13: The gcc_p parameter to
     USE_STRUCT_CONVENTION isn't used.  */
  int struct_return = ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
			|| TYPE_CODE (valtype) == TYPE_CODE_UNION
			|| TYPE_CODE (valtype) == TYPE_CODE_ARRAY)
		       && gdbarch_deprecated_use_struct_convention
			    (current_gdbarch, 0, valtype));

  if (writebuf != NULL)
    {
      gdb_assert (!struct_return);
      /* NOTE: cagney/2004-06-13: See stack.c:return_command.  Old
	 architectures don't expect store_return_value to handle small
	 structures.  Should not be called with such types.  */
      gdb_assert (TYPE_CODE (valtype) != TYPE_CODE_STRUCT
		  && TYPE_CODE (valtype) != TYPE_CODE_UNION);
      gdbarch_store_return_value (current_gdbarch, valtype, regcache, writebuf);
    }

  if (readbuf != NULL)
    {
      gdb_assert (!struct_return);
      gdbarch_extract_return_value (current_gdbarch,
				    valtype, regcache, readbuf);
    }

  if (struct_return)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    return RETURN_VALUE_REGISTER_CONVENTION;
}

int
legacy_register_sim_regno (int regnum)
{
  /* Only makes sense to supply raw registers.  */
  gdb_assert (regnum >= 0 && regnum < gdbarch_num_regs (current_gdbarch));
  /* NOTE: cagney/2002-05-13: The old code did it this way and it is
     suspected that some GDB/SIM combinations may rely on this
     behavour.  The default should be one2one_register_sim_regno
     (below).  */
  if (gdbarch_register_name (current_gdbarch, regnum) != NULL
      && gdbarch_register_name (current_gdbarch, regnum)[0] != '\0')
    return regnum;
  else
    return LEGACY_SIM_REGNO_IGNORE;
}

CORE_ADDR
generic_skip_trampoline_code (struct frame_info *frame, CORE_ADDR pc)
{
  return 0;
}

CORE_ADDR
generic_skip_solib_resolver (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  return 0;
}

int
generic_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return 0;
}

int
generic_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  return 0;
}

/* Helper functions for gdbarch_inner_than */

int
core_addr_lessthan (CORE_ADDR lhs, CORE_ADDR rhs)
{
  return (lhs < rhs);
}

int
core_addr_greaterthan (CORE_ADDR lhs, CORE_ADDR rhs)
{
  return (lhs > rhs);
}

/* Misc helper functions for targets. */

CORE_ADDR
core_addr_identity (CORE_ADDR addr)
{
  return addr;
}

CORE_ADDR
convert_from_func_ptr_addr_identity (struct gdbarch *gdbarch, CORE_ADDR addr,
				     struct target_ops *targ)
{
  return addr;
}

int
no_op_reg_to_regnum (int reg)
{
  return reg;
}

void
default_elf_make_msymbol_special (asymbol *sym, struct minimal_symbol *msym)
{
  return;
}

void
default_coff_make_msymbol_special (int val, struct minimal_symbol *msym)
{
  return;
}

int
cannot_register_not (int regnum)
{
  return 0;
}

/* Legacy version of target_virtual_frame_pointer().  Assumes that
   there is an gdbarch_deprecated_fp_regnum and that it is the same, cooked or
   raw.  */

void
legacy_virtual_frame_pointer (CORE_ADDR pc,
			      int *frame_regnum,
			      LONGEST *frame_offset)
{
  /* FIXME: cagney/2002-09-13: This code is used when identifying the
     frame pointer of the current PC.  It is assuming that a single
     register and an offset can determine this.  I think it should
     instead generate a byte code expression as that would work better
     with things like Dwarf2's CFI.  */
  if (gdbarch_deprecated_fp_regnum (current_gdbarch) >= 0
      && gdbarch_deprecated_fp_regnum (current_gdbarch)
	   < gdbarch_num_regs (current_gdbarch))
    *frame_regnum = gdbarch_deprecated_fp_regnum (current_gdbarch);
  else if (gdbarch_sp_regnum (current_gdbarch) >= 0
	   && gdbarch_sp_regnum (current_gdbarch)
	        < gdbarch_num_regs (current_gdbarch))
    *frame_regnum = gdbarch_sp_regnum (current_gdbarch);
  else
    /* Should this be an internal error?  I guess so, it is reflecting
       an architectural limitation in the current design.  */
    internal_error (__FILE__, __LINE__, _("No virtual frame pointer available"));
  *frame_offset = 0;
}


int
generic_convert_register_p (int regnum, struct type *type)
{
  return 0;
}

int
default_stabs_argument_has_addr (struct gdbarch *gdbarch, struct type *type)
{
  return 0;
}

int
generic_instruction_nullified (struct gdbarch *gdbarch,
			       struct regcache *regcache)
{
  return 0;
}

int
default_remote_register_number (struct gdbarch *gdbarch,
				int regno)
{
  return regno;
}


/* Functions to manipulate the endianness of the target.  */

static int target_byte_order_user = BFD_ENDIAN_UNKNOWN;

static const char endian_big[] = "big";
static const char endian_little[] = "little";
static const char endian_auto[] = "auto";
static const char *endian_enum[] =
{
  endian_big,
  endian_little,
  endian_auto,
  NULL,
};
static const char *set_endian_string;

enum bfd_endian
selected_byte_order (void)
{
  if (target_byte_order_user != BFD_ENDIAN_UNKNOWN)
    return gdbarch_byte_order (current_gdbarch);
  else
    return BFD_ENDIAN_UNKNOWN;
}

/* Called by ``show endian''.  */

static void
show_endian (struct ui_file *file, int from_tty, struct cmd_list_element *c,
	     const char *value)
{
  if (target_byte_order_user == BFD_ENDIAN_UNKNOWN)
    if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
      fprintf_unfiltered (file, _("The target endianness is set automatically "
				  "(currently big endian)\n"));
    else
      fprintf_unfiltered (file, _("The target endianness is set automatically "
			   "(currently little endian)\n"));
  else
    if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
      fprintf_unfiltered (file,
			  _("The target is assumed to be big endian\n"));
    else
      fprintf_unfiltered (file,
			  _("The target is assumed to be little endian\n"));
}

static void
set_endian (char *ignore_args, int from_tty, struct cmd_list_element *c)
{
  struct gdbarch_info info;

  gdbarch_info_init (&info);

  if (set_endian_string == endian_auto)
    {
      target_byte_order_user = BFD_ENDIAN_UNKNOWN;
      if (! gdbarch_update_p (info))
	internal_error (__FILE__, __LINE__,
			_("set_endian: architecture update failed"));
    }
  else if (set_endian_string == endian_little)
    {
      info.byte_order = BFD_ENDIAN_LITTLE;
      if (! gdbarch_update_p (info))
	printf_unfiltered (_("Little endian target not supported by GDB\n"));
      else
	target_byte_order_user = BFD_ENDIAN_LITTLE;
    }
  else if (set_endian_string == endian_big)
    {
      info.byte_order = BFD_ENDIAN_BIG;
      if (! gdbarch_update_p (info))
	printf_unfiltered (_("Big endian target not supported by GDB\n"));
      else
	target_byte_order_user = BFD_ENDIAN_BIG;
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("set_endian: bad value"));

  show_endian (gdb_stdout, from_tty, NULL, NULL);
}

/* Given SELECTED, a currently selected BFD architecture, and
   FROM_TARGET, a BFD architecture reported by the target description,
   return what architecture to use.  Either may be NULL; if both are
   specified, we use the more specific.  If the two are obviously
   incompatible, warn the user.  */

static const struct bfd_arch_info *
choose_architecture_for_target (const struct bfd_arch_info *selected,
				const struct bfd_arch_info *from_target)
{
  const struct bfd_arch_info *compat1, *compat2;

  if (selected == NULL)
    return from_target;

  if (from_target == NULL)
    return selected;

  /* struct bfd_arch_info objects are singletons: that is, there's
     supposed to be exactly one instance for a given machine.  So you
     can tell whether two are equivalent by comparing pointers.  */
  if (from_target == selected)
    return selected;

  /* BFD's 'A->compatible (A, B)' functions return zero if A and B are
     incompatible.  But if they are compatible, it returns the 'more
     featureful' of the two arches.  That is, if A can run code
     written for B, but B can't run code written for A, then it'll
     return A.

     Some targets (e.g. MIPS as of 2006-12-04) don't fully
     implement this, instead always returning NULL or the first
     argument.  We detect that case by checking both directions.  */

  compat1 = selected->compatible (selected, from_target);
  compat2 = from_target->compatible (from_target, selected);

  if (compat1 == NULL && compat2 == NULL)
    {
      warning (_("Selected architecture %s is not compatible "
		 "with reported target architecture %s"),
	       selected->printable_name, from_target->printable_name);
      return selected;
    }

  if (compat1 == NULL)
    return compat2;
  if (compat2 == NULL)
    return compat1;
  if (compat1 == compat2)
    return compat1;

  /* If the two didn't match, but one of them was a default architecture,
     assume the more specific one is correct.  This handles the case
     where an executable or target description just says "mips", but
     the other knows which MIPS variant.  */
  if (compat1->the_default)
    return compat2;
  if (compat2->the_default)
    return compat1;

  /* We have no idea which one is better.  This is a bug, but not
     a critical problem; warn the user.  */
  warning (_("Selected architecture %s is ambiguous with "
	     "reported target architecture %s"),
	   selected->printable_name, from_target->printable_name);
  return selected;
}

/* Functions to manipulate the architecture of the target */

enum set_arch { set_arch_auto, set_arch_manual };

static const struct bfd_arch_info *target_architecture_user;

static const char *set_architecture_string;

const char *
selected_architecture_name (void)
{
  if (target_architecture_user == NULL)
    return NULL;
  else
    return set_architecture_string;
}

/* Called if the user enters ``show architecture'' without an
   argument. */

static void
show_architecture (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c, const char *value)
{
  const char *arch;
  arch = gdbarch_bfd_arch_info (current_gdbarch)->printable_name;
  if (target_architecture_user == NULL)
    fprintf_filtered (file, _("\
The target architecture is set automatically (currently %s)\n"), arch);
  else
    fprintf_filtered (file, _("\
The target architecture is assumed to be %s\n"), arch);
}


/* Called if the user enters ``set architecture'' with or without an
   argument. */

static void
set_architecture (char *ignore_args, int from_tty, struct cmd_list_element *c)
{
  struct gdbarch_info info;

  gdbarch_info_init (&info);

  if (strcmp (set_architecture_string, "auto") == 0)
    {
      target_architecture_user = NULL;
      if (!gdbarch_update_p (info))
	internal_error (__FILE__, __LINE__,
			_("could not select an architecture automatically"));
    }
  else
    {
      info.bfd_arch_info = bfd_scan_arch (set_architecture_string);
      if (info.bfd_arch_info == NULL)
	internal_error (__FILE__, __LINE__,
			_("set_architecture: bfd_scan_arch failed"));
      if (gdbarch_update_p (info))
	target_architecture_user = info.bfd_arch_info;
      else
	printf_unfiltered (_("Architecture `%s' not recognized.\n"),
			   set_architecture_string);
    }
  show_architecture (gdb_stdout, from_tty, NULL, NULL);
}

/* Try to select a global architecture that matches "info".  Return
   non-zero if the attempt succeds.  */
int
gdbarch_update_p (struct gdbarch_info info)
{
  struct gdbarch *new_gdbarch = gdbarch_find_by_info (info);

  /* If there no architecture by that name, reject the request.  */
  if (new_gdbarch == NULL)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update_p: "
			    "Architecture not found\n");
      return 0;
    }

  /* If it is the same old architecture, accept the request (but don't
     swap anything).  */
  if (new_gdbarch == current_gdbarch)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update_p: "
			    "Architecture 0x%08lx (%s) unchanged\n",
			    (long) new_gdbarch,
			    gdbarch_bfd_arch_info (new_gdbarch)->printable_name);
      return 1;
    }

  /* It's a new architecture, swap it in.  */
  if (gdbarch_debug)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_update_p: "
			"New architecture 0x%08lx (%s) selected\n",
			(long) new_gdbarch,
			gdbarch_bfd_arch_info (new_gdbarch)->printable_name);
  deprecated_current_gdbarch_select_hack (new_gdbarch);

  return 1;
}

/* Return the architecture for ABFD.  If no suitable architecture
   could be find, return NULL.  */

struct gdbarch *
gdbarch_from_bfd (bfd *abfd)
{
  struct gdbarch *old_gdbarch = current_gdbarch;
  struct gdbarch *new_gdbarch;
  struct gdbarch_info info;

  /* If we call gdbarch_find_by_info without filling in info.abfd,
     then it will use the global exec_bfd.  That's fine if we don't
     have one of those either.  And that's the only time we should
     reach here with a NULL ABFD argument - when we are discarding
     the executable.  */
  gdb_assert (abfd != NULL || exec_bfd == NULL);

  gdbarch_info_init (&info);
  info.abfd = abfd;
  return gdbarch_find_by_info (info);
}

/* Set the dynamic target-system-dependent parameters (architecture,
   byte-order) using information found in the BFD */

void
set_gdbarch_from_file (bfd *abfd)
{
  struct gdbarch *gdbarch;

  gdbarch = gdbarch_from_bfd (abfd);
  if (gdbarch == NULL)
    error (_("Architecture of file not recognized."));
  deprecated_current_gdbarch_select_hack (gdbarch);
}

/* Initialize the current architecture.  Update the ``set
   architecture'' command so that it specifies a list of valid
   architectures.  */

#ifdef DEFAULT_BFD_ARCH
extern const bfd_arch_info_type DEFAULT_BFD_ARCH;
static const bfd_arch_info_type *default_bfd_arch = &DEFAULT_BFD_ARCH;
#else
static const bfd_arch_info_type *default_bfd_arch;
#endif

#ifdef DEFAULT_BFD_VEC
extern const bfd_target DEFAULT_BFD_VEC;
static const bfd_target *default_bfd_vec = &DEFAULT_BFD_VEC;
#else
static const bfd_target *default_bfd_vec;
#endif

static int default_byte_order = BFD_ENDIAN_UNKNOWN;

void
initialize_current_architecture (void)
{
  const char **arches = gdbarch_printable_names ();

  /* determine a default architecture and byte order. */
  struct gdbarch_info info;
  gdbarch_info_init (&info);
  
  /* Find a default architecture. */
  if (default_bfd_arch == NULL)
    {
      /* Choose the architecture by taking the first one
	 alphabetically. */
      const char *chosen = arches[0];
      const char **arch;
      for (arch = arches; *arch != NULL; arch++)
	{
	  if (strcmp (*arch, chosen) < 0)
	    chosen = *arch;
	}
      if (chosen == NULL)
	internal_error (__FILE__, __LINE__,
			_("initialize_current_architecture: No arch"));
      default_bfd_arch = bfd_scan_arch (chosen);
      if (default_bfd_arch == NULL)
	internal_error (__FILE__, __LINE__,
			_("initialize_current_architecture: Arch not found"));
    }

  info.bfd_arch_info = default_bfd_arch;

  /* Take several guesses at a byte order.  */
  if (default_byte_order == BFD_ENDIAN_UNKNOWN
      && default_bfd_vec != NULL)
    {
      /* Extract BFD's default vector's byte order. */
      switch (default_bfd_vec->byteorder)
	{
	case BFD_ENDIAN_BIG:
	  default_byte_order = BFD_ENDIAN_BIG;
	  break;
	case BFD_ENDIAN_LITTLE:
	  default_byte_order = BFD_ENDIAN_LITTLE;
	  break;
	default:
	  break;
	}
    }
  if (default_byte_order == BFD_ENDIAN_UNKNOWN)
    {
      /* look for ``*el-*'' in the target name. */
      const char *chp;
      chp = strchr (target_name, '-');
      if (chp != NULL
	  && chp - 2 >= target_name
	  && strncmp (chp - 2, "el", 2) == 0)
	default_byte_order = BFD_ENDIAN_LITTLE;
    }
  if (default_byte_order == BFD_ENDIAN_UNKNOWN)
    {
      /* Wire it to big-endian!!! */
      default_byte_order = BFD_ENDIAN_BIG;
    }

  info.byte_order = default_byte_order;

  if (! gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__,
		    _("initialize_current_architecture: Selection of "
		      "initial architecture failed"));

  /* Create the ``set architecture'' command appending ``auto'' to the
     list of architectures. */
  {
    struct cmd_list_element *c;
    /* Append ``auto''. */
    int nr;
    for (nr = 0; arches[nr] != NULL; nr++);
    arches = xrealloc (arches, sizeof (char*) * (nr + 2));
    arches[nr + 0] = "auto";
    arches[nr + 1] = NULL;
    add_setshow_enum_cmd ("architecture", class_support,
			  arches, &set_architecture_string, _("\
Set architecture of target."), _("\
Show architecture of target."), NULL,
			  set_architecture, show_architecture,
			  &setlist, &showlist);
    add_alias_cmd ("processor", "architecture", class_support, 1, &setlist);
  }
}


/* Initialize a gdbarch info to values that will be automatically
   overridden.  Note: Originally, this ``struct info'' was initialized
   using memset(0).  Unfortunately, that ran into problems, namely
   BFD_ENDIAN_BIG is zero.  An explicit initialization function that
   can explicitly set each field to a well defined value is used.  */

void
gdbarch_info_init (struct gdbarch_info *info)
{
  memset (info, 0, sizeof (struct gdbarch_info));
  info->byte_order = BFD_ENDIAN_UNKNOWN;
  info->osabi = GDB_OSABI_UNINITIALIZED;
}

/* Similar to init, but this time fill in the blanks.  Information is
   obtained from the global "set ..." options and explicitly
   initialized INFO fields.  */

void
gdbarch_info_fill (struct gdbarch_info *info)
{
  /* Check for the current file.  */
  if (info->abfd == NULL)
    info->abfd = exec_bfd;

  /* Check for the current target description.  */
  if (info->target_desc == NULL)
    info->target_desc = target_current_description ();

  /* "(gdb) set architecture ...".  */
  if (info->bfd_arch_info == NULL
      && target_architecture_user)
    info->bfd_arch_info = target_architecture_user;
  /* From the file.  */
  if (info->bfd_arch_info == NULL
      && info->abfd != NULL
      && bfd_get_arch (info->abfd) != bfd_arch_unknown
      && bfd_get_arch (info->abfd) != bfd_arch_obscure)
    info->bfd_arch_info = bfd_get_arch_info (info->abfd);
  /* From the target.  */
  if (info->target_desc != NULL)
    info->bfd_arch_info = choose_architecture_for_target
      (info->bfd_arch_info, tdesc_architecture (info->target_desc));
  /* From the default.  */
  if (info->bfd_arch_info == NULL)
    info->bfd_arch_info = default_bfd_arch;

  /* "(gdb) set byte-order ...".  */
  if (info->byte_order == BFD_ENDIAN_UNKNOWN
      && target_byte_order_user != BFD_ENDIAN_UNKNOWN)
    info->byte_order = target_byte_order_user;
  /* From the INFO struct.  */
  if (info->byte_order == BFD_ENDIAN_UNKNOWN
      && info->abfd != NULL)
    info->byte_order = (bfd_big_endian (info->abfd) ? BFD_ENDIAN_BIG
			: bfd_little_endian (info->abfd) ? BFD_ENDIAN_LITTLE
			: BFD_ENDIAN_UNKNOWN);
  /* From the default.  */
  if (info->byte_order == BFD_ENDIAN_UNKNOWN)
    info->byte_order = default_byte_order;

  /* "(gdb) set osabi ...".  Handled by gdbarch_lookup_osabi.  */
  if (info->osabi == GDB_OSABI_UNINITIALIZED)
    info->osabi = gdbarch_lookup_osabi (info->abfd);

  /* Must have at least filled in the architecture.  */
  gdb_assert (info->bfd_arch_info != NULL);
}

/* */

extern initialize_file_ftype _initialize_gdbarch_utils; /* -Wmissing-prototypes */

void
_initialize_gdbarch_utils (void)
{
  struct cmd_list_element *c;
  add_setshow_enum_cmd ("endian", class_support,
			endian_enum, &set_endian_string, _("\
Set endianness of target."), _("\
Show endianness of target."), NULL,
			set_endian, show_endian,
			&setlist, &showlist);
}
