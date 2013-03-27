/* Reading symbol files from memory.

   Copyright (C) 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000, 2001,
   2002, 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

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

/* This file defines functions (and commands to exercise those
   functions) for reading debugging information from object files
   whose images are mapped directly into the inferior's memory.  For
   example, the Linux kernel maps a "syscall DSO" into each process's
   address space; this DSO provides kernel-specific code for some
   system calls.

   At the moment, BFD only has functions for parsing object files from
   memory for the ELF format, even though the general idea isn't
   ELF-specific.  This means that BFD only provides the functions GDB
   needs when configured for ELF-based targets.  So these functions
   may only be compiled on ELF-based targets.

   GDB has no idea whether it has been configured for an ELF-based
   target or not: it just tries to handle whatever files it is given.
   But this means there are no preprocessor symbols on which we could
   make these functions' compilation conditional.

   So, for the time being, we put these functions alone in this file,
   and have .mt files reference them as appropriate.  In the future, I
   hope BFD will provide a format-independent bfd_from_remote_memory
   entry point.  */


#include "defs.h"
#include "symtab.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "exceptions.h"
#include "gdbcmd.h"
#include "target.h"
#include "value.h"
#include "symfile.h"
#include "observer.h"
#include "auxv.h"
#include "elf/common.h"


/* Read inferior memory at ADDR to find the header of a loaded object file
   and read its in-core symbols out of inferior memory.  TEMPL is a bfd
   representing the target's format.  NAME is the name to use for this
   symbol file in messages; it can be NULL or a malloc-allocated string
   which will be attached to the BFD.  */
static struct objfile *
symbol_file_add_from_memory (struct bfd *templ, CORE_ADDR addr, char *name,
			     int from_tty)
{
  struct objfile *objf;
  struct bfd *nbfd;
  struct bfd_section *sec;
  bfd_vma loadbase;
  struct section_addr_info *sai;
  unsigned int i;

  if (bfd_get_flavour (templ) != bfd_target_elf_flavour)
    error (_("add-symbol-file-from-memory not supported for this target"));

  nbfd = bfd_elf_bfd_from_remote_memory (templ, addr, &loadbase,
					 target_read_memory);
  if (nbfd == NULL)
    error (_("Failed to read a valid object file image from memory."));

  if (name == NULL)
    nbfd->filename = xstrdup ("shared object read from target memory");
  else
    nbfd->filename = name;

  if (!bfd_check_format (nbfd, bfd_object))
    {
      /* FIXME: should be checking for errors from bfd_close (for one thing,
         on error it does not free all the storage associated with the
         bfd).  */
      bfd_close (nbfd);
      error (_("Got object file from memory but can't read symbols: %s."),
	     bfd_errmsg (bfd_get_error ()));
    }

  sai = alloc_section_addr_info (bfd_count_sections (nbfd));
  make_cleanup (xfree, sai);
  i = 0;
  for (sec = nbfd->sections; sec != NULL; sec = sec->next)
    if ((bfd_get_section_flags (nbfd, sec) & (SEC_ALLOC|SEC_LOAD)) != 0)
      {
	sai->other[i].addr = bfd_get_section_vma (nbfd, sec) + loadbase;
	sai->other[i].name = (char *) bfd_get_section_name (nbfd, sec);
	sai->other[i].sectindex = sec->index;
	++i;
      }

  objf = symbol_file_add_from_bfd (nbfd, from_tty,
                                   sai, 0, OBJF_SHARED);

  /* This might change our ideas about frames already looked at.  */
  reinit_frame_cache ();

  return objf;
}


static void
add_symbol_file_from_memory_command (char *args, int from_tty)
{
  CORE_ADDR addr;
  struct bfd *templ;

  if (args == NULL)
    error (_("add-symbol-file-from-memory requires an expression argument"));

  addr = parse_and_eval_address (args);

  /* We need some representative bfd to know the target we are looking at.  */
  if (symfile_objfile != NULL)
    templ = symfile_objfile->obfd;
  else
    templ = exec_bfd;
  if (templ == NULL)
    error (_("\
Must use symbol-file or exec-file before add-symbol-file-from-memory."));

  symbol_file_add_from_memory (templ, addr, NULL, from_tty);
}

/* Arguments for symbol_file_add_from_memory_wrapper.  */

struct symbol_file_add_from_memory_args
{
  struct bfd *bfd;
  CORE_ADDR sysinfo_ehdr;
  char *name;
  int from_tty;
};

/* Wrapper function for symbol_file_add_from_memory, for
   catch_exceptions.  */

static int
symbol_file_add_from_memory_wrapper (struct ui_out *uiout, void *data)
{
  struct symbol_file_add_from_memory_args *args = data;

  symbol_file_add_from_memory (args->bfd, args->sysinfo_ehdr, args->name,
			       args->from_tty);
  return 0;
}

/* Try to add the symbols for the vsyscall page, if there is one.  This function
   is called via the inferior_created observer.  */

static void
add_vsyscall_page (struct target_ops *target, int from_tty)
{
  CORE_ADDR sysinfo_ehdr;

  if (target_auxv_search (target, AT_SYSINFO_EHDR, &sysinfo_ehdr) > 0
      && sysinfo_ehdr != (CORE_ADDR) 0)
    {
      struct bfd *bfd;
      struct symbol_file_add_from_memory_args args;

      if (core_bfd != NULL)
	bfd = core_bfd;
      else if (exec_bfd != NULL)
	bfd = exec_bfd;
      else
       /* FIXME: cagney/2004-05-06: Should not require an existing
	  BFD when trying to create a run-time BFD of the VSYSCALL
	  page in the inferior.  Unfortunately that's the current
	  interface so for the moment bail.  Introducing a
	  ``bfd_runtime'' (a BFD created using the loaded image) file
	  format should fix this.  */
	{
	  warning (_("\
Could not load vsyscall page because no executable was specified\n\
try using the \"file\" command first."));
	  return;
	}
      args.bfd = bfd;
      args.sysinfo_ehdr = sysinfo_ehdr;
      xasprintf (&args.name, "system-supplied DSO at 0x%s",
		 paddr_nz (sysinfo_ehdr));
      /* Pass zero for FROM_TTY, because the action of loading the
	 vsyscall DSO was not triggered by the user, even if the user
	 typed "run" at the TTY.  */
      args.from_tty = 0;
      catch_exceptions (uiout, symbol_file_add_from_memory_wrapper,
			&args, RETURN_MASK_ALL);
    }
}


void
_initialize_symfile_mem (void)
{
  add_cmd ("add-symbol-file-from-memory", class_files,
           add_symbol_file_from_memory_command, _("\
Load the symbols out of memory from a dynamically loaded object file.\n\
Give an expression for the address of the file's shared object file header."),
           &cmdlist);

  /* Want to know of each new inferior so that its vsyscall info can
     be extracted.  */
  observer_attach_inferior_created (add_vsyscall_page);
}
