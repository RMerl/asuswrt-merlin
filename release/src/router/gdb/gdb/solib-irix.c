/* Shared library support for IRIX.
   Copyright (C) 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2004,
   2007 Free Software Foundation, Inc.

   This file was created using portions of irix5-nat.c originally
   contributed to GDB by Ian Lance Taylor.

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

#include "symtab.h"
#include "bfd.h"
/* FIXME: ezannoni/2004-02-13 Verify that the include below is
   really needed.  */
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "target.h"
#include "inferior.h"

#include "solist.h"

/* Link map info to include in an allocate so_list entry.  Unlike some
   of the other solib backends, this (Irix) backend chooses to decode
   the link map info obtained from the target and store it as (mostly)
   CORE_ADDRs which need no further decoding.  This is more convenient
   because there are three different link map formats to worry about.
   We use a single routine (fetch_lm_info) to read (and decode) the target
   specific link map data.  */

struct lm_info
{
  CORE_ADDR addr;		/* address of obj_info or obj_list
				   struct on target (from which the
				   following information is obtained).  */
  CORE_ADDR next;		/* address of next item in list.  */
  CORE_ADDR reloc_offset;	/* amount to relocate by  */
  CORE_ADDR pathname_addr;	/* address of pathname  */
  int pathname_len;		/* length of pathname */
};

/* It's not desirable to use the system header files to obtain the
   structure of the obj_list or obj_info structs.  Therefore, we use a
   platform neutral representation which has been derived from the IRIX
   header files.  */

typedef struct
{
  gdb_byte b[4];
}
gdb_int32_bytes;
typedef struct
{
  gdb_byte b[8];
}
gdb_int64_bytes;

/* The "old" obj_list struct.  This is used with old (o32) binaries.
   The ``data'' member points at a much larger and more complicated
   struct which we will only refer to by offsets.  See
   fetch_lm_info().  */

struct irix_obj_list
{
  gdb_int32_bytes data;
  gdb_int32_bytes next;
  gdb_int32_bytes prev;
};

/* The ELF32 and ELF64 versions of the above struct.  The oi_magic value
   corresponds to the ``data'' value in the "old" struct.  When this value
   is 0xffffffff, the data will be in one of the following formats.  The
   ``oi_size'' field is used to decide which one we actually have.  */

struct irix_elf32_obj_info
{
  gdb_int32_bytes oi_magic;
  gdb_int32_bytes oi_size;
  gdb_int32_bytes oi_next;
  gdb_int32_bytes oi_prev;
  gdb_int32_bytes oi_ehdr;
  gdb_int32_bytes oi_orig_ehdr;
  gdb_int32_bytes oi_pathname;
  gdb_int32_bytes oi_pathname_len;
};

struct irix_elf64_obj_info
{
  gdb_int32_bytes oi_magic;
  gdb_int32_bytes oi_size;
  gdb_int64_bytes oi_next;
  gdb_int64_bytes oi_prev;
  gdb_int64_bytes oi_ehdr;
  gdb_int64_bytes oi_orig_ehdr;
  gdb_int64_bytes oi_pathname;
  gdb_int32_bytes oi_pathname_len;
  gdb_int32_bytes padding;
};

/* Union of all of the above (plus a split out magic field).  */

union irix_obj_info
{
  gdb_int32_bytes magic;
  struct irix_obj_list ol32;
  struct irix_elf32_obj_info oi32;
  struct irix_elf64_obj_info oi64;
};

/* MIPS sign extends its 32 bit addresses.  We could conceivably use
   extract_typed_address here, but to do so, we'd have to construct an
   appropriate type.  Calling extract_signed_integer seems simpler.  */

static CORE_ADDR
extract_mips_address (void *addr, int len)
{
  return extract_signed_integer (addr, len);
}

/* Fetch and return the link map data associated with ADDR.  Note that
   this routine automatically determines which (of three) link map
   formats is in use by the target.  */

struct lm_info
fetch_lm_info (CORE_ADDR addr)
{
  struct lm_info li;
  union irix_obj_info buf;

  li.addr = addr;

  /* The smallest region that we'll need is for buf.ol32.  We'll read
     that first.  We'll read more of the buffer later if we have to deal
     with one of the other cases.  (We don't want to incur a memory error
     if we were to read a larger region that generates an error due to
     being at the end of a page or the like.)  */
  read_memory (addr, (char *) &buf, sizeof (buf.ol32));

  if (extract_unsigned_integer (buf.magic.b, sizeof (buf.magic)) != 0xffffffff)
    {
      /* Use buf.ol32... */
      char obj_buf[432];
      CORE_ADDR obj_addr = extract_mips_address (&buf.ol32.data,
						 sizeof (buf.ol32.data));
      li.next = extract_mips_address (&buf.ol32.next, sizeof (buf.ol32.next));

      read_memory (obj_addr, obj_buf, sizeof (obj_buf));

      li.pathname_addr = extract_mips_address (&obj_buf[236], 4);
      li.pathname_len = 0;	/* unknown */
      li.reloc_offset = extract_mips_address (&obj_buf[196], 4)
	- extract_mips_address (&obj_buf[248], 4);

    }
  else if (extract_unsigned_integer (buf.oi32.oi_size.b,
				     sizeof (buf.oi32.oi_size))
	   == sizeof (buf.oi32))
    {
      /* Use buf.oi32...  */

      /* Read rest of buffer.  */
      read_memory (addr + sizeof (buf.ol32),
		   ((char *) &buf) + sizeof (buf.ol32),
		   sizeof (buf.oi32) - sizeof (buf.ol32));

      /* Fill in fields using buffer contents.  */
      li.next = extract_mips_address (&buf.oi32.oi_next,
				      sizeof (buf.oi32.oi_next));
      li.reloc_offset = extract_mips_address (&buf.oi32.oi_ehdr,
					      sizeof (buf.oi32.oi_ehdr))
	- extract_mips_address (&buf.oi32.oi_orig_ehdr,
				sizeof (buf.oi32.oi_orig_ehdr));
      li.pathname_addr = extract_mips_address (&buf.oi32.oi_pathname,
					       sizeof (buf.oi32.oi_pathname));
      li.pathname_len = extract_unsigned_integer (buf.oi32.oi_pathname_len.b,
						  sizeof (buf.oi32.
							  oi_pathname_len));
    }
  else if (extract_unsigned_integer (buf.oi64.oi_size.b,
				     sizeof (buf.oi64.oi_size))
	   == sizeof (buf.oi64))
    {
      /* Use buf.oi64...  */

      /* Read rest of buffer.  */
      read_memory (addr + sizeof (buf.ol32),
		   ((char *) &buf) + sizeof (buf.ol32),
		   sizeof (buf.oi64) - sizeof (buf.ol32));

      /* Fill in fields using buffer contents.  */
      li.next = extract_mips_address (&buf.oi64.oi_next,
				      sizeof (buf.oi64.oi_next));
      li.reloc_offset = extract_mips_address (&buf.oi64.oi_ehdr,
					      sizeof (buf.oi64.oi_ehdr))
	- extract_mips_address (&buf.oi64.oi_orig_ehdr,
				sizeof (buf.oi64.oi_orig_ehdr));
      li.pathname_addr = extract_mips_address (&buf.oi64.oi_pathname,
					       sizeof (buf.oi64.oi_pathname));
      li.pathname_len = extract_unsigned_integer (buf.oi64.oi_pathname_len.b,
						  sizeof (buf.oi64.
							  oi_pathname_len));
    }
  else
    {
      error (_("Unable to fetch shared library obj_info or obj_list info."));
    }

  return li;
}

/* The symbol which starts off the list of shared libraries.  */
#define DEBUG_BASE "__rld_obj_head"

static void *base_breakpoint;

static CORE_ADDR debug_base;	/* Base of dynamic linker structures */

/*

   LOCAL FUNCTION

   locate_base -- locate the base address of dynamic linker structs

   SYNOPSIS

   CORE_ADDR locate_base (void)

   DESCRIPTION

   For both the SunOS and SVR4 shared library implementations, if the
   inferior executable has been linked dynamically, there is a single
   address somewhere in the inferior's data space which is the key to
   locating all of the dynamic linker's runtime structures.  This
   address is the value of the symbol defined by the macro DEBUG_BASE.
   The job of this function is to find and return that address, or to
   return 0 if there is no such address (the executable is statically
   linked for example).

   For SunOS, the job is almost trivial, since the dynamic linker and
   all of it's structures are statically linked to the executable at
   link time.  Thus the symbol for the address we are looking for has
   already been added to the minimal symbol table for the executable's
   objfile at the time the symbol file's symbols were read, and all we
   have to do is look it up there.  Note that we explicitly do NOT want
   to find the copies in the shared library.

   The SVR4 version is much more complicated because the dynamic linker
   and it's structures are located in the shared C library, which gets
   run as the executable's "interpreter" by the kernel.  We have to go
   to a lot more work to discover the address of DEBUG_BASE.  Because
   of this complexity, we cache the value we find and return that value
   on subsequent invocations.  Note there is no copy in the executable
   symbol tables.

   Irix 5 is basically like SunOS.

   Note that we can assume nothing about the process state at the time
   we need to find this address.  We may be stopped on the first instruc-
   tion of the interpreter (C shared library), the first instruction of
   the executable itself, or somewhere else entirely (if we attached
   to the process for example).

 */

static CORE_ADDR
locate_base (void)
{
  struct minimal_symbol *msymbol;
  CORE_ADDR address = 0;

  msymbol = lookup_minimal_symbol (DEBUG_BASE, NULL, symfile_objfile);
  if ((msymbol != NULL) && (SYMBOL_VALUE_ADDRESS (msymbol) != 0))
    {
      address = SYMBOL_VALUE_ADDRESS (msymbol);
    }
  return (address);
}

/*

   LOCAL FUNCTION

   disable_break -- remove the "mapping changed" breakpoint

   SYNOPSIS

   static int disable_break ()

   DESCRIPTION

   Removes the breakpoint that gets hit when the dynamic linker
   completes a mapping change.

 */

static int
disable_break (void)
{
  int status = 1;


  /* Note that breakpoint address and original contents are in our address
     space, so we just need to write the original contents back. */

  if (deprecated_remove_raw_breakpoint (base_breakpoint) != 0)
    {
      status = 0;
    }

  base_breakpoint = NULL;

  /* Note that it is possible that we have stopped at a location that
     is different from the location where we inserted our breakpoint.
     On mips-irix, we can actually land in __dbx_init(), so we should
     not check the PC against our breakpoint address here.  See procfs.c
     for more details.  */

  return (status);
}

/*

   LOCAL FUNCTION

   enable_break -- arrange for dynamic linker to hit breakpoint

   SYNOPSIS

   int enable_break (void)

   DESCRIPTION

   This functions inserts a breakpoint at the entry point of the
   main executable, where all shared libraries are mapped in.
 */

static int
enable_break (void)
{
  if (symfile_objfile != NULL)
    {
      base_breakpoint
	= deprecated_insert_raw_breakpoint (entry_point_address ());

      if (base_breakpoint != NULL)
	return 1;
    }

  return 0;
}

/*

   LOCAL FUNCTION

   irix_solib_create_inferior_hook -- shared library startup support

   SYNOPSIS

   void solib_create_inferior_hook ()

   DESCRIPTION

   When gdb starts up the inferior, it nurses it along (through the
   shell) until it is ready to execute it's first instruction.  At this
   point, this function gets called via expansion of the macro
   SOLIB_CREATE_INFERIOR_HOOK.

   For SunOS executables, this first instruction is typically the
   one at "_start", or a similar text label, regardless of whether
   the executable is statically or dynamically linked.  The runtime
   startup code takes care of dynamically linking in any shared
   libraries, once gdb allows the inferior to continue.

   For SVR4 executables, this first instruction is either the first
   instruction in the dynamic linker (for dynamically linked
   executables) or the instruction at "start" for statically linked
   executables.  For dynamically linked executables, the system
   first exec's /lib/libc.so.N, which contains the dynamic linker,
   and starts it running.  The dynamic linker maps in any needed
   shared libraries, maps in the actual user executable, and then
   jumps to "start" in the user executable.

   For both SunOS shared libraries, and SVR4 shared libraries, we
   can arrange to cooperate with the dynamic linker to discover the
   names of shared libraries that are dynamically linked, and the
   base addresses to which they are linked.

   This function is responsible for discovering those names and
   addresses, and saving sufficient information about them to allow
   their symbols to be read at a later time.

   FIXME

   Between enable_break() and disable_break(), this code does not
   properly handle hitting breakpoints which the user might have
   set in the startup code or in the dynamic linker itself.  Proper
   handling will probably have to wait until the implementation is
   changed to use the "breakpoint handler function" method.

   Also, what if child has exit()ed?  Must exit loop somehow.
 */

static void
irix_solib_create_inferior_hook (void)
{
  if (!enable_break ())
    {
      warning (_("shared library handler failed to enable breakpoint"));
      return;
    }

  /* Now run the target.  It will eventually hit the breakpoint, at
     which point all of the libraries will have been mapped in and we
     can go groveling around in the dynamic linker structures to find
     out what we need to know about them. */

  clear_proceed_status ();
  stop_soon = STOP_QUIETLY;
  stop_signal = TARGET_SIGNAL_0;
  do
    {
      target_resume (pid_to_ptid (-1), 0, stop_signal);
      wait_for_inferior ();
    }
  while (stop_signal != TARGET_SIGNAL_TRAP);

  /* We are now either at the "mapping complete" breakpoint (or somewhere
     else, a condition we aren't prepared to deal with anyway), so adjust
     the PC as necessary after a breakpoint, disable the breakpoint, and
     add any shared libraries that were mapped in. */

  if (!disable_break ())
    {
      warning (_("shared library handler failed to disable breakpoint"));
    }

  /* solib_add will call reinit_frame_cache.
     But we are stopped in the startup code and we might not have symbols
     for the startup code, so heuristic_proc_start could be called
     and will put out an annoying warning.
     Delaying the resetting of stop_soon until after symbol loading
     suppresses the warning.  */
  solib_add ((char *) 0, 0, (struct target_ops *) 0, auto_solib_add);
  stop_soon = NO_STOP_QUIETLY;
  re_enable_breakpoints_in_shlibs ();
}

/* LOCAL FUNCTION

   current_sos -- build a list of currently loaded shared objects

   SYNOPSIS

   struct so_list *current_sos ()

   DESCRIPTION

   Build a list of `struct so_list' objects describing the shared
   objects currently loaded in the inferior.  This list does not
   include an entry for the main executable file.

   Note that we only gather information directly available from the
   inferior --- we don't examine any of the shared library files
   themselves.  The declaration of `struct so_list' says which fields
   we provide values for.  */

static struct so_list *
irix_current_sos (void)
{
  CORE_ADDR lma;
  char addr_buf[8];
  struct so_list *head = 0;
  struct so_list **link_ptr = &head;
  int is_first = 1;
  struct lm_info lm;

  /* Make sure we've looked up the inferior's dynamic linker's base
     structure.  */
  if (!debug_base)
    {
      debug_base = locate_base ();

      /* If we can't find the dynamic linker's base structure, this
         must not be a dynamically linked executable.  Hmm.  */
      if (!debug_base)
	return 0;
    }

  read_memory (debug_base,
	       addr_buf,
	       gdbarch_addr_bit (current_gdbarch) / TARGET_CHAR_BIT);
  lma = extract_mips_address (addr_buf,
			      gdbarch_addr_bit (current_gdbarch)
				/ TARGET_CHAR_BIT);

  while (lma)
    {
      lm = fetch_lm_info (lma);
      if (!is_first)
	{
	  int errcode;
	  char *name_buf;
	  int name_size;
	  struct so_list *new
	    = (struct so_list *) xmalloc (sizeof (struct so_list));
	  struct cleanup *old_chain = make_cleanup (xfree, new);

	  memset (new, 0, sizeof (*new));

	  new->lm_info = xmalloc (sizeof (struct lm_info));
	  make_cleanup (xfree, new->lm_info);

	  *new->lm_info = lm;

	  /* Extract this shared object's name.  */
	  name_size = lm.pathname_len;
	  if (name_size == 0)
	    name_size = SO_NAME_MAX_PATH_SIZE - 1;

	  if (name_size >= SO_NAME_MAX_PATH_SIZE)
	    {
	      name_size = SO_NAME_MAX_PATH_SIZE - 1;
	      warning
		("current_sos: truncating name of %d characters to only %d characters",
		 lm.pathname_len, name_size);
	    }

	  target_read_string (lm.pathname_addr, &name_buf,
			      name_size, &errcode);
	  if (errcode != 0)
	    warning (_("Can't read pathname for load map: %s."),
		       safe_strerror (errcode));
	  else
	    {
	      strncpy (new->so_name, name_buf, name_size);
	      new->so_name[name_size] = '\0';
	      xfree (name_buf);
	      strcpy (new->so_original_name, new->so_name);
	    }

	  new->next = 0;
	  *link_ptr = new;
	  link_ptr = &new->next;

	  discard_cleanups (old_chain);
	}
      is_first = 0;
      lma = lm.next;
    }

  return head;
}

/*

  LOCAL FUNCTION

  irix_open_symbol_file_object

  SYNOPSIS

  void irix_open_symbol_file_object (void *from_tty)

  DESCRIPTION

  If no open symbol file, attempt to locate and open the main symbol
  file.  On IRIX, this is the first link map entry.  If its name is
  here, we can open it.  Useful when attaching to a process without
  first loading its symbol file.

  If FROM_TTYP dereferences to a non-zero integer, allow messages to
  be printed.  This parameter is a pointer rather than an int because
  open_symbol_file_object() is called via catch_errors() and
  catch_errors() requires a pointer argument. */

static int
irix_open_symbol_file_object (void *from_ttyp)
{
  CORE_ADDR lma;
  char addr_buf[8];
  struct lm_info lm;
  struct cleanup *cleanups;
  int errcode;
  int from_tty = *(int *) from_ttyp;
  char *filename;

  if (symfile_objfile)
    if (!query ("Attempt to reload symbols from process? "))
      return 0;

  if ((debug_base = locate_base ()) == 0)
    return 0;			/* failed somehow...  */

  /* First link map member should be the executable.  */
  read_memory (debug_base,
	       addr_buf,
	       gdbarch_addr_bit (current_gdbarch) / TARGET_CHAR_BIT);
  lma = extract_mips_address (addr_buf,
			      gdbarch_addr_bit (current_gdbarch)
				/ TARGET_CHAR_BIT);
  if (lma == 0)
    return 0;			/* failed somehow...  */

  lm = fetch_lm_info (lma);

  if (lm.pathname_addr == 0)
    return 0;			/* No filename.  */

  /* Now fetch the filename from target memory.  */
  target_read_string (lm.pathname_addr, &filename, SO_NAME_MAX_PATH_SIZE - 1,
		      &errcode);

  if (errcode)
    {
      warning (_("failed to read exec filename from attached file: %s"),
	       safe_strerror (errcode));
      return 0;
    }

  cleanups = make_cleanup (xfree, filename);
  /* Have a pathname: read the symbol file.  */
  symbol_file_add_main (filename, from_tty);

  do_cleanups (cleanups);

  return 1;
}


/*

   LOCAL FUNCTION

   irix_special_symbol_handling -- additional shared library symbol handling

   SYNOPSIS

   void irix_special_symbol_handling ()

   DESCRIPTION

   Once the symbols from a shared object have been loaded in the usual
   way, we are called to do any system specific symbol handling that 
   is needed.

   For SunOS4, this consisted of grunging around in the dynamic
   linkers structures to find symbol definitions for "common" symbols
   and adding them to the minimal symbol table for the runtime common
   objfile.

   However, for IRIX, there's nothing to do.

 */

static void
irix_special_symbol_handling (void)
{
}

/* Using the solist entry SO, relocate the addresses in SEC.  */

static void
irix_relocate_section_addresses (struct so_list *so,
				 struct section_table *sec)
{
  sec->addr += so->lm_info->reloc_offset;
  sec->endaddr += so->lm_info->reloc_offset;
}

/* Free the lm_info struct.  */

static void
irix_free_so (struct so_list *so)
{
  xfree (so->lm_info);
}

/* Clear backend specific state.  */

static void
irix_clear_solib (void)
{
  debug_base = 0;
}

/* Return 1 if PC lies in the dynamic symbol resolution code of the
   run time loader.  */
static int
irix_in_dynsym_resolve_code (CORE_ADDR pc)
{
  return 0;
}

static struct target_so_ops irix_so_ops;

void
_initialize_irix_solib (void)
{
  irix_so_ops.relocate_section_addresses = irix_relocate_section_addresses;
  irix_so_ops.free_so = irix_free_so;
  irix_so_ops.clear_solib = irix_clear_solib;
  irix_so_ops.solib_create_inferior_hook = irix_solib_create_inferior_hook;
  irix_so_ops.special_symbol_handling = irix_special_symbol_handling;
  irix_so_ops.current_sos = irix_current_sos;
  irix_so_ops.open_symbol_file_object = irix_open_symbol_file_object;
  irix_so_ops.in_dynsym_resolve_code = irix_in_dynsym_resolve_code;

  /* FIXME: Don't do this here.  *_gdbarch_init() should set so_ops. */
  current_target_so_ops = &irix_so_ops;
}
