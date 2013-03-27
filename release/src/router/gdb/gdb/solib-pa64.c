/* Handle PA64 shared libraries for GDB, the GNU Debugger.

   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   
   HP in their infinite stupidity choose not to use standard ELF dynamic
   linker interfaces.  They also choose not to make their ELF dymamic
   linker interfaces compatible with the SOM dynamic linker.  The
   net result is we can not use either of the existing somsolib.c or
   solib.c.  What a crock.

   Even more disgusting.  This file depends on functions provided only
   in certain PA64 libraries.  Thus this file is supposed to only be
   used native.  When will HP ever learn that they need to provide the
   same functionality in all their libraries!  */

#include "defs.h"
#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "target.h"
#include "inferior.h"

#include "hppa-tdep.h"
#include "solist.h"
#include "solib-pa64.h"

#undef SOLIB_PA64_DBG

/* If we are building for a SOM-only target, then we don't need this.  */
#ifndef PA_SOM_ONLY

/* FIXME: kettenis/20041213: These includes should be eliminated.  */
#include <dlfcn.h>
#include <elf.h>
#include <elf_hp.h>

struct lm_info {
  struct load_module_desc desc;
  CORE_ADDR desc_addr;
};

/* When adding fields, be sure to clear them in _initialize_pa64_solib. */
typedef struct
  {
    CORE_ADDR dld_flags_addr;
    LONGEST dld_flags;
    struct bfd_section *dyninfo_sect;
    int have_read_dld_descriptor;
    int is_valid;
    CORE_ADDR load_map;
    CORE_ADDR load_map_addr;
    struct load_module_desc dld_desc;
  }
dld_cache_t;

static dld_cache_t dld_cache;

static int
read_dynamic_info (asection *dyninfo_sect, dld_cache_t *dld_cache_p);

static void
pa64_relocate_section_addresses (struct so_list *so,
				 struct section_table *sec)
{
  asection *asec = sec->the_bfd_section;
  CORE_ADDR load_offset;

  /* Relocate all the sections based on where they got loaded.  */

  load_offset = bfd_section_vma (so->abfd, asec) - asec->filepos;

  if (asec->flags & SEC_CODE)
    {
      sec->addr += so->lm_info->desc.text_base - load_offset;
      sec->endaddr += so->lm_info->desc.text_base - load_offset;
    }
  else if (asec->flags & SEC_DATA)
    {
      sec->addr += so->lm_info->desc.data_base - load_offset;
      sec->endaddr += so->lm_info->desc.data_base - load_offset;
    }
}

static void
pa64_free_so (struct so_list *so)
{
  xfree (so->lm_info);
}

static void
pa64_clear_solib (void)
{
}

/* Wrapper for target_read_memory for dlgetmodinfo.  */

static void *
pa64_target_read_memory (void *buffer, CORE_ADDR ptr, size_t bufsiz, int ident)
{
  if (target_read_memory (ptr, buffer, bufsiz) != 0)
    return 0;
  return buffer;
}

/* Read the dynamic linker's internal shared library descriptor.

   This must happen after dld starts running, so we can't do it in
   read_dynamic_info.  Record the fact that we have loaded the
   descriptor.  If the library is archive bound, then return zero, else
   return nonzero.  */

static int
read_dld_descriptor (void)
{
  char *dll_path;
  asection *dyninfo_sect;

  /* If necessary call read_dynamic_info to extract the contents of the
     .dynamic section from the shared library.  */
  if (!dld_cache.is_valid) 
    {
      if (symfile_objfile == NULL)
	error (_("No object file symbols."));

      dyninfo_sect = bfd_get_section_by_name (symfile_objfile->obfd, 
					      ".dynamic");
      if (!dyninfo_sect) 
	{
	  return 0;
	}

      if (!read_dynamic_info (dyninfo_sect, &dld_cache))
	error (_("Unable to read in .dynamic section information."));
    }

  /* Read the load map pointer.  */
  if (target_read_memory (dld_cache.load_map_addr,
			  (char *) &dld_cache.load_map,
			  sizeof (dld_cache.load_map))
      != 0)
    {
      error (_("Error while reading in load map pointer."));
    }

  /* Read in the dld load module descriptor */
  if (dlgetmodinfo (-1, 
		    &dld_cache.dld_desc,
		    sizeof (dld_cache.dld_desc), 
		    pa64_target_read_memory, 
		    0, 
		    dld_cache.load_map)
      == 0)
    {
      error (_("Error trying to get information about dynamic linker."));
    }

  /* Indicate that we have loaded the dld descriptor.  */
  dld_cache.have_read_dld_descriptor = 1;

  return 1;
}


/* Read the .dynamic section and extract the information of interest,
   which is stored in dld_cache.  The routine elf_locate_base in solib.c 
   was used as a model for this.  */

static int
read_dynamic_info (asection *dyninfo_sect, dld_cache_t *dld_cache_p)
{
  char *buf;
  char *bufend;
  CORE_ADDR dyninfo_addr;
  int dyninfo_sect_size;
  CORE_ADDR entry_addr;

  /* Read in .dynamic section, silently ignore errors.  */
  dyninfo_addr = bfd_section_vma (symfile_objfile->obfd, dyninfo_sect);
  dyninfo_sect_size = bfd_section_size (exec_bfd, dyninfo_sect);
  buf = alloca (dyninfo_sect_size);
  if (target_read_memory (dyninfo_addr, buf, dyninfo_sect_size))
    return 0;

  /* Scan the .dynamic section and record the items of interest. 
     In particular, DT_HP_DLD_FLAGS */
  for (bufend = buf + dyninfo_sect_size, entry_addr = dyninfo_addr;
       buf < bufend;
       buf += sizeof (Elf64_Dyn), entry_addr += sizeof (Elf64_Dyn))
    {
      Elf64_Dyn *x_dynp = (Elf64_Dyn*)buf;
      Elf64_Sxword dyn_tag;
      CORE_ADDR	dyn_ptr;
      char *pbuf;

      pbuf = alloca (gdbarch_ptr_bit (current_gdbarch) / HOST_CHAR_BIT);
      dyn_tag = bfd_h_get_64 (symfile_objfile->obfd, 
			      (bfd_byte*) &x_dynp->d_tag);

      /* We can't use a switch here because dyn_tag is 64 bits and HP's
	 lame comiler does not handle 64bit items in switch statements.  */
      if (dyn_tag == DT_NULL)
	break;
      else if (dyn_tag == DT_HP_DLD_FLAGS)
	{
	  /* Set dld_flags_addr and dld_flags in *dld_cache_p */
	  dld_cache_p->dld_flags_addr = entry_addr + offsetof(Elf64_Dyn, d_un);
	  if (target_read_memory (dld_cache_p->dld_flags_addr,
	  			  (char*) &dld_cache_p->dld_flags, 
				  sizeof (dld_cache_p->dld_flags))
	      != 0)
	    {
	      error (_("Error while reading in .dynamic section of the program."));
	    }
	}
      else if (dyn_tag == DT_HP_LOAD_MAP)
	{
	  /* Dld will place the address of the load map at load_map_addr
	     after it starts running.  */
	  if (target_read_memory (entry_addr + offsetof(Elf64_Dyn, 
							d_un.d_ptr),
				  (char*) &dld_cache_p->load_map_addr,
				  sizeof (dld_cache_p->load_map_addr))
	      != 0)
	    {
	      error (_("Error while reading in .dynamic section of the program."));
	    }
	}
      else 
	{
	  /* tag is not of interest */
	}
    }

  /* Record other information and set is_valid to 1. */
  dld_cache_p->dyninfo_sect = dyninfo_sect;

  /* Verify that we read in required info.  These fields are re-set to zero
     in pa64_solib_restart.  */

  if (dld_cache_p->dld_flags_addr != 0 && dld_cache_p->load_map_addr != 0) 
    dld_cache_p->is_valid = 1;
  else 
    return 0;

  return 1;
}

/*
   bfd_lookup_symbol -- lookup the value for a specific symbol

   An expensive way to lookup the value of a single symbol for
   bfd's that are only temporary anyway.  This is used by the
   shared library support to find the address of the debugger
   interface structures in the shared library.

   Note that 0 is specifically allowed as an error return (no
   such symbol).
 */

static CORE_ADDR
bfd_lookup_symbol (bfd *abfd, char *symname)
{
  unsigned int storage_needed;
  asymbol *sym;
  asymbol **symbol_table;
  unsigned int number_of_symbols;
  unsigned int i;
  struct cleanup *back_to;
  CORE_ADDR symaddr = 0;

  storage_needed = bfd_get_symtab_upper_bound (abfd);

  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (xfree, symbol_table);
      number_of_symbols = bfd_canonicalize_symtab (abfd, symbol_table);

      for (i = 0; i < number_of_symbols; i++)
	{
	  sym = *symbol_table++;
	  if (strcmp (sym->name, symname) == 0)
	    {
	      /* Bfd symbols are section relative. */
	      symaddr = sym->value + sym->section->vma;
	      break;
	    }
	}
      do_cleanups (back_to);
    }
  return (symaddr);
}


/* This hook gets called just before the first instruction in the
   inferior process is executed.

   This is our opportunity to set magic flags in the inferior so
   that GDB can be notified when a shared library is mapped in and
   to tell the dynamic linker that a private copy of the library is
   needed (so GDB can set breakpoints in the library).

   We need to set two flag bits in this routine.

     DT_HP_DEBUG_PRIVATE to indicate that shared libraries should be
     mapped private.

     DT_HP_DEBUG_CALLBACK to indicate that we want the dynamic linker to
     call the breakpoint routine for significant events.  */

static void
pa64_solib_create_inferior_hook (void)
{
  struct minimal_symbol *msymbol;
  unsigned int dld_flags, status;
  asection *shlib_info, *interp_sect;
  char buf[4];
  struct objfile *objfile;
  CORE_ADDR anaddr;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  if (symfile_objfile == NULL)
    return;

  /* First see if the objfile was dynamically linked.  */
  shlib_info = bfd_get_section_by_name (symfile_objfile->obfd, ".dynamic");
  if (!shlib_info)
    return;

  /* It's got a .dynamic section, make sure it's not empty.  */
  if (bfd_section_size (symfile_objfile->obfd, shlib_info) == 0)
    return;

  /* Read in the .dynamic section.  */
  if (! read_dynamic_info (shlib_info, &dld_cache))
    error (_("Unable to read the .dynamic section."));

  /* Turn on the flags we care about.  */
  dld_cache.dld_flags |= DT_HP_DEBUG_PRIVATE;
  dld_cache.dld_flags |= DT_HP_DEBUG_CALLBACK;
  status = target_write_memory (dld_cache.dld_flags_addr,
				(char *) &dld_cache.dld_flags,
				sizeof (dld_cache.dld_flags));
  if (status != 0)
    error (_("Unable to modify dynamic linker flags."));

  /* Now we have to create a shared library breakpoint in the dynamic
     linker.  This can be somewhat tricky since the symbol is inside
     the dynamic linker (for which we do not have symbols or a base
     load address!   Luckily I wrote this code for solib.c years ago.  */
  interp_sect = bfd_get_section_by_name (exec_bfd, ".interp");
  if (interp_sect)
    {
      unsigned int interp_sect_size;
      char *buf;
      CORE_ADDR load_addr;
      bfd *tmp_bfd;
      CORE_ADDR sym_addr = 0;

      /* Read the contents of the .interp section into a local buffer;
	 the contents specify the dynamic linker this program uses.  */
      interp_sect_size = bfd_section_size (exec_bfd, interp_sect);
      buf = alloca (interp_sect_size);
      bfd_get_section_contents (exec_bfd, interp_sect,
				buf, 0, interp_sect_size);

      /* Now we need to figure out where the dynamic linker was
	 loaded so that we can load its symbols and place a breakpoint
	 in the dynamic linker itself.

	 This address is stored on the stack.  However, I've been unable
	 to find any magic formula to find it for Solaris (appears to
	 be trivial on GNU/Linux).  Therefore, we have to try an alternate
	 mechanism to find the dynamic linker's base address.  */
      tmp_bfd = bfd_openr (buf, gnutarget);
      if (tmp_bfd == NULL)
	return;

      /* Make sure the dynamic linker's really a useful object.  */
      if (!bfd_check_format (tmp_bfd, bfd_object))
	{
	  warning (_("Unable to grok dynamic linker %s as an object file"), buf);
	  bfd_close (tmp_bfd);
	  return;
	}

      /* We find the dynamic linker's base address by examining the
	 current pc (which point at the entry point for the dynamic
	 linker) and subtracting the offset of the entry point. 

	 Also note the breakpoint is the second instruction in the
	 routine.  */
      load_addr = read_pc () - tmp_bfd->start_address;
      sym_addr = bfd_lookup_symbol (tmp_bfd, "__dld_break");
      sym_addr = load_addr + sym_addr + 4;
      
      /* Create the shared library breakpoint.  */
      {
	struct breakpoint *b
	  = create_solib_event_breakpoint (sym_addr);

	/* The breakpoint is actually hard-coded into the dynamic linker,
	   so we don't need to actually insert a breakpoint instruction
	   there.  In fact, the dynamic linker's code is immutable, even to
	   ttrace, so we shouldn't even try to do that.  For cases like
	   this, we have "permanent" breakpoints.  */
	make_breakpoint_permanent (b);
      }

      /* We're done with the temporary bfd.  */
      bfd_close (tmp_bfd);
    }
}

static void
pa64_special_symbol_handling (void)
{
}

static struct so_list *
pa64_current_sos (void)
{
  struct so_list *head = 0;
  struct so_list **link_ptr = &head;
  int dll_index;

  /* Read in the load map pointer if we have not done so already.  */
  if (! dld_cache.have_read_dld_descriptor)
    if (! read_dld_descriptor ())
      return NULL;

  /* If the libraries were not mapped private, warn the user.  */
  if ((dld_cache.dld_flags & DT_HP_DEBUG_PRIVATE) == 0)
    warning (_("The shared libraries were not privately mapped; setting a\n"
    	     "breakpoint in a shared library will not work until you rerun "
	     "the program.\n"));

  for (dll_index = -1; ; dll_index++)
    {
      struct load_module_desc dll_desc;
      char *dll_path;
      struct so_list *new;
      struct cleanup *old_chain;

      if (dll_index == 0)
        continue;

      /* Read in the load module descriptor.  */
      if (dlgetmodinfo (dll_index, &dll_desc, sizeof (dll_desc),
			pa64_target_read_memory, 0, dld_cache.load_map)
	  == 0)
	break;

      /* Get the name of the shared library.  */
      dll_path = (char *)dlgetname (&dll_desc, sizeof (dll_desc),
			    pa64_target_read_memory,
			    0, dld_cache.load_map);

      new = (struct so_list *) xmalloc (sizeof (struct so_list));
      memset (new, 0, sizeof (struct so_list));
      new->lm_info = (struct lm_info *) xmalloc (sizeof (struct lm_info));
      memset (new->lm_info, 0, sizeof (struct lm_info));

      strncpy (new->so_name, dll_path, SO_NAME_MAX_PATH_SIZE - 1);
      new->so_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
      strcpy (new->so_original_name, new->so_name);

      memcpy (&new->lm_info->desc, &dll_desc, sizeof (dll_desc));

#ifdef SOLIB_PA64_DBG
      {
        struct load_module_desc *d = &new->lm_info->desc;
	printf ("\n+ library \"%s\" is described at index %d\n", new->so_name, 
		dll_index);
	printf ("    text_base = 0x%llx\n", d->text_base); 
	printf ("    text_size = 0x%llx\n", d->text_size); 
	printf ("    data_base = 0x%llx\n", d->data_base); 
	printf ("    data_size = 0x%llx\n", d->data_size); 
	printf ("    unwind_base = 0x%llx\n", d->unwind_base); 
	printf ("    linkage_ptr = 0x%llx\n", d->linkage_ptr); 
	printf ("    phdr_base = 0x%llx\n", d->phdr_base); 
	printf ("    tls_size = 0x%llx\n", d->tls_size); 
	printf ("    tls_start_addr = 0x%llx\n", d->tls_start_addr); 
	printf ("    unwind_size = 0x%llx\n", d->unwind_size); 
	printf ("    tls_index = 0x%llx\n", d->tls_index); 
      }
#endif

      /* Link the new object onto the list.  */
      new->next = NULL;
      *link_ptr = new;
      link_ptr = &new->next;
    }

  return head;
}

static int
pa64_open_symbol_file_object (void *from_ttyp)
{
  int from_tty = *(int *)from_ttyp;
  char buf[4];
  struct load_module_desc dll_desc;
  char *dll_path;

  if (symfile_objfile)
    if (!query ("Attempt to reload symbols from process? "))
      return 0;

  /* Read in the load map pointer if we have not done so already.  */
  if (! dld_cache.have_read_dld_descriptor)
    if (! read_dld_descriptor ())
      return 0;

  /* Read in the load module descriptor.  */
  if (dlgetmodinfo (0, &dll_desc, sizeof (dll_desc),
		    pa64_target_read_memory, 0, dld_cache.load_map) == 0)
    return 0;

  /* Get the name of the shared library.  */
  dll_path = (char *)dlgetname (&dll_desc, sizeof (dll_desc),
				pa64_target_read_memory,
				0, dld_cache.load_map);

  /* Have a pathname: read the symbol file.  */
  symbol_file_add_main (dll_path, from_tty);

  return 1;
}

/* Return nonzero if PC is an address inside the dynamic linker.  */
static int
pa64_in_dynsym_resolve_code (CORE_ADDR pc)
{
  asection *shlib_info;

  if (symfile_objfile == NULL)
    return 0;

  if (!dld_cache.have_read_dld_descriptor)
    if (!read_dld_descriptor ())
      return 0;

  return (pc >= dld_cache.dld_desc.text_base
	  && pc < dld_cache.dld_desc.text_base + dld_cache.dld_desc.text_size);
}


/* Return the GOT value for the shared library in which ADDR belongs.  If
   ADDR isn't in any known shared library, return zero.  */

static CORE_ADDR
pa64_solib_get_got_by_pc (CORE_ADDR addr)
{
  struct so_list *so_list = master_so_list ();
  CORE_ADDR got_value = 0;

  while (so_list)
    {
      if (so_list->lm_info->desc.text_base <= addr
	  && ((so_list->lm_info->desc.text_base
	       + so_list->lm_info->desc.text_size)
	      > addr))
        {
	  got_value = so_list->lm_info->desc.linkage_ptr;
 	  break;
	}
      so_list = so_list->next;
    }
  return got_value;
}

/* Get some HPUX-specific data from a shared lib.  */
static CORE_ADDR
pa64_solib_thread_start_addr (struct so_list *so)
{
  return so->lm_info->desc.tls_start_addr;
}


/* Return the address of the handle of the shared library in which ADDR
   belongs.  If ADDR isn't in any known shared library, return zero.  */

static CORE_ADDR
pa64_solib_get_solib_by_pc (CORE_ADDR addr)
{
  struct so_list *so_list = master_so_list ();
  CORE_ADDR retval = 0;

  while (so_list)
    {
      if (so_list->lm_info->desc.text_base <= addr
	  && ((so_list->lm_info->desc.text_base
	       + so_list->lm_info->desc.text_size)
	      > addr))
	{
	  retval = so_list->lm_info->desc_addr;
	  break;
	}
      so_list = so_list->next;
    }
  return retval;
}

/* pa64 libraries do not seem to set the section offsets in a standard (i.e. 
   SVr4) way; the text section offset stored in the file doesn't correspond
   to the place where the library is actually loaded into memory.  Instead,
   we rely on the dll descriptor to tell us where things were loaded.  */
static CORE_ADDR
pa64_solib_get_text_base (struct objfile *objfile)
{
  struct so_list *so;

  for (so = master_so_list (); so; so = so->next)
    if (so->objfile == objfile)
      return so->lm_info->desc.text_base;
  
  return 0;
}

static struct target_so_ops pa64_so_ops;

extern initialize_file_ftype _initialize_pa64_solib; /* -Wmissing-prototypes */

void
_initialize_pa64_solib (void)
{
  pa64_so_ops.relocate_section_addresses = pa64_relocate_section_addresses;
  pa64_so_ops.free_so = pa64_free_so;
  pa64_so_ops.clear_solib = pa64_clear_solib;
  pa64_so_ops.solib_create_inferior_hook = pa64_solib_create_inferior_hook;
  pa64_so_ops.special_symbol_handling = pa64_special_symbol_handling;
  pa64_so_ops.current_sos = pa64_current_sos;
  pa64_so_ops.open_symbol_file_object = pa64_open_symbol_file_object;
  pa64_so_ops.in_dynsym_resolve_code = pa64_in_dynsym_resolve_code;

  memset (&dld_cache, 0, sizeof (dld_cache));
}

void pa64_solib_select (struct gdbarch_tdep *tdep)
{
  current_target_so_ops = &pa64_so_ops;

  tdep->solib_thread_start_addr = pa64_solib_thread_start_addr;
  tdep->solib_get_got_by_pc = pa64_solib_get_got_by_pc;
  tdep->solib_get_solib_by_pc = pa64_solib_get_solib_by_pc;
  tdep->solib_get_text_base = pa64_solib_get_text_base;
}

#else /* PA_SOM_ONLY */

extern initialize_file_ftype _initialize_pa64_solib; /* -Wmissing-prototypes */

void
_initialize_pa64_solib (void)
{
}

void pa64_solib_select (struct gdbarch_tdep *tdep)
{
  /* For a SOM-only target, there is no pa64 solib support.  This is needed
     for hppa-hpux-tdep.c to build.  */
  error (_("Cannot select pa64 solib support for this configuration."));
}
#endif
