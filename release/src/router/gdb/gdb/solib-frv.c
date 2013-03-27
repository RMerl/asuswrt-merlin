/* Handle FR-V (FDPIC) shared libraries for GDB, the GNU Debugger.
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include "defs.h"
#include "gdb_string.h"
#include "inferior.h"
#include "gdbcore.h"
#include "solib.h"
#include "solist.h"
#include "frv-tdep.h"
#include "objfiles.h"
#include "symtab.h"
#include "language.h"
#include "command.h"
#include "gdbcmd.h"
#include "elf/frv.h"

/* Flag which indicates whether internal debug messages should be printed.  */
static int solib_frv_debug;

/* FR-V pointers are four bytes wide.  */
enum { FRV_PTR_SIZE = 4 };

/* Representation of loadmap and related structs for the FR-V FDPIC ABI.  */

/* External versions; the size and alignment of the fields should be
   the same as those on the target.  When loaded, the placement of
   the bits in each field will be the same as on the target.  */
typedef gdb_byte ext_Elf32_Half[2];
typedef gdb_byte ext_Elf32_Addr[4];
typedef gdb_byte ext_Elf32_Word[4];

struct ext_elf32_fdpic_loadseg
{
  /* Core address to which the segment is mapped.  */
  ext_Elf32_Addr addr;
  /* VMA recorded in the program header.  */
  ext_Elf32_Addr p_vaddr;
  /* Size of this segment in memory.  */
  ext_Elf32_Word p_memsz;
};

struct ext_elf32_fdpic_loadmap {
  /* Protocol version number, must be zero.  */
  ext_Elf32_Half version;
  /* Number of segments in this map.  */
  ext_Elf32_Half nsegs;
  /* The actual memory map.  */
  struct ext_elf32_fdpic_loadseg segs[1 /* nsegs, actually */];
};

/* Internal versions; the types are GDB types and the data in each
   of the fields is (or will be) decoded from the external struct
   for ease of consumption.  */
struct int_elf32_fdpic_loadseg
{
  /* Core address to which the segment is mapped.  */
  CORE_ADDR addr;
  /* VMA recorded in the program header.  */
  CORE_ADDR p_vaddr;
  /* Size of this segment in memory.  */
  long p_memsz;
};

struct int_elf32_fdpic_loadmap {
  /* Protocol version number, must be zero.  */
  int version;
  /* Number of segments in this map.  */
  int nsegs;
  /* The actual memory map.  */
  struct int_elf32_fdpic_loadseg segs[1 /* nsegs, actually */];
};

/* Given address LDMADDR, fetch and decode the loadmap at that address.
   Return NULL if there is a problem reading the target memory or if
   there doesn't appear to be a loadmap at the given address.  The
   allocated space (representing the loadmap) returned by this
   function may be freed via a single call to xfree().  */

static struct int_elf32_fdpic_loadmap *
fetch_loadmap (CORE_ADDR ldmaddr)
{
  struct ext_elf32_fdpic_loadmap ext_ldmbuf_partial;
  struct ext_elf32_fdpic_loadmap *ext_ldmbuf;
  struct int_elf32_fdpic_loadmap *int_ldmbuf;
  int ext_ldmbuf_size, int_ldmbuf_size;
  int version, seg, nsegs;

  /* Fetch initial portion of the loadmap.  */
  if (target_read_memory (ldmaddr, (gdb_byte *) &ext_ldmbuf_partial,
                          sizeof ext_ldmbuf_partial))
    {
      /* Problem reading the target's memory.  */
      return NULL;
    }

  /* Extract the version.  */
  version = extract_unsigned_integer (ext_ldmbuf_partial.version,
                                      sizeof ext_ldmbuf_partial.version);
  if (version != 0)
    {
      /* We only handle version 0.  */
      return NULL;
    }

  /* Extract the number of segments.  */
  nsegs = extract_unsigned_integer (ext_ldmbuf_partial.nsegs,
                                    sizeof ext_ldmbuf_partial.nsegs);

  /* Allocate space for the complete (external) loadmap.  */
  ext_ldmbuf_size = sizeof (struct ext_elf32_fdpic_loadmap)
               + (nsegs - 1) * sizeof (struct ext_elf32_fdpic_loadseg);
  ext_ldmbuf = xmalloc (ext_ldmbuf_size);

  /* Copy over the portion of the loadmap that's already been read.  */
  memcpy (ext_ldmbuf, &ext_ldmbuf_partial, sizeof ext_ldmbuf_partial);

  /* Read the rest of the loadmap from the target.  */
  if (target_read_memory (ldmaddr + sizeof ext_ldmbuf_partial,
                          (gdb_byte *) ext_ldmbuf + sizeof ext_ldmbuf_partial,
                          ext_ldmbuf_size - sizeof ext_ldmbuf_partial))
    {
      /* Couldn't read rest of the loadmap.  */
      xfree (ext_ldmbuf);
      return NULL;
    }

  /* Allocate space into which to put information extract from the
     external loadsegs.  I.e, allocate the internal loadsegs.  */
  int_ldmbuf_size = sizeof (struct int_elf32_fdpic_loadmap)
               + (nsegs - 1) * sizeof (struct int_elf32_fdpic_loadseg);
  int_ldmbuf = xmalloc (int_ldmbuf_size);

  /* Place extracted information in internal structs.  */
  int_ldmbuf->version = version;
  int_ldmbuf->nsegs = nsegs;
  for (seg = 0; seg < nsegs; seg++)
    {
      int_ldmbuf->segs[seg].addr
	= extract_unsigned_integer (ext_ldmbuf->segs[seg].addr,
	                            sizeof (ext_ldmbuf->segs[seg].addr));
      int_ldmbuf->segs[seg].p_vaddr
	= extract_unsigned_integer (ext_ldmbuf->segs[seg].p_vaddr,
	                            sizeof (ext_ldmbuf->segs[seg].p_vaddr));
      int_ldmbuf->segs[seg].p_memsz
	= extract_unsigned_integer (ext_ldmbuf->segs[seg].p_memsz,
	                            sizeof (ext_ldmbuf->segs[seg].p_memsz));
    }

  xfree (ext_ldmbuf);
  return int_ldmbuf;
}

/* External link_map and elf32_fdpic_loadaddr struct definitions.  */

typedef gdb_byte ext_ptr[4];

struct ext_elf32_fdpic_loadaddr
{
  ext_ptr map;			/* struct elf32_fdpic_loadmap *map; */
  ext_ptr got_value;		/* void *got_value; */
};

struct ext_link_map
{
  struct ext_elf32_fdpic_loadaddr l_addr;

  /* Absolute file name object was found in.  */
  ext_ptr l_name;		/* char *l_name; */

  /* Dynamic section of the shared object.  */
  ext_ptr l_ld;			/* ElfW(Dyn) *l_ld; */

  /* Chain of loaded objects.  */
  ext_ptr l_next, l_prev;	/* struct link_map *l_next, *l_prev; */
};

/* Link map info to include in an allocated so_list entry */

struct lm_info
  {
    /* The loadmap, digested into an easier to use form.  */
    struct int_elf32_fdpic_loadmap *map;
    /* The GOT address for this link map entry.  */
    CORE_ADDR got_value;
    /* The link map address, needed for frv_fetch_objfile_link_map().  */
    CORE_ADDR lm_addr;

    /* Cached dynamic symbol table and dynamic relocs initialized and
       used only by find_canonical_descriptor_in_load_object().

       Note: kevinb/2004-02-26: It appears that calls to
       bfd_canonicalize_dynamic_reloc() will use the same symbols as
       those supplied to the first call to this function.  Therefore,
       it's important to NOT free the asymbol ** data structure
       supplied to the first call.  Thus the caching of the dynamic
       symbols (dyn_syms) is critical for correct operation.  The
       caching of the dynamic relocations could be dispensed with.  */
    asymbol **dyn_syms;
    arelent **dyn_relocs;
    int dyn_reloc_count;	/* number of dynamic relocs.  */

  };

/* The load map, got value, etc. are not available from the chain
   of loaded shared objects.  ``main_executable_lm_info'' provides
   a way to get at this information so that it doesn't need to be
   frequently recomputed.  Initialized by frv_relocate_main_executable().  */
static struct lm_info *main_executable_lm_info;

static void frv_relocate_main_executable (void);
static CORE_ADDR main_got (void);
static int enable_break2 (void);

/*

   LOCAL FUNCTION

   bfd_lookup_symbol -- lookup the value for a specific symbol

   SYNOPSIS

   CORE_ADDR bfd_lookup_symbol (bfd *abfd, char *symname)

   DESCRIPTION

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
  long storage_needed;
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

  if (symaddr)
    return symaddr;

  /* Look for the symbol in the dynamic string table too.  */

  storage_needed = bfd_get_dynamic_symtab_upper_bound (abfd);

  if (storage_needed > 0)
    {
      symbol_table = (asymbol **) xmalloc (storage_needed);
      back_to = make_cleanup (xfree, symbol_table);
      number_of_symbols = bfd_canonicalize_dynamic_symtab (abfd, symbol_table);

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

  return symaddr;
}


/*

  LOCAL FUNCTION

  open_symbol_file_object

  SYNOPSIS

  void open_symbol_file_object (void *from_tty)

  DESCRIPTION

  If no open symbol file, attempt to locate and open the main symbol
  file.

  If FROM_TTYP dereferences to a non-zero integer, allow messages to
  be printed.  This parameter is a pointer rather than an int because
  open_symbol_file_object() is called via catch_errors() and
  catch_errors() requires a pointer argument. */

static int
open_symbol_file_object (void *from_ttyp)
{
  /* Unimplemented.  */
  return 0;
}

/* Cached value for lm_base(), below.  */
static CORE_ADDR lm_base_cache = 0;

/* Link map address for main module.  */
static CORE_ADDR main_lm_addr = 0;

/* Return the address from which the link map chain may be found.  On
   the FR-V, this may be found in a number of ways.  Assuming that the
   main executable has already been relocated, the easiest way to find
   this value is to look up the address of _GLOBAL_OFFSET_TABLE_.  A
   pointer to the start of the link map will be located at the word found
   at _GLOBAL_OFFSET_TABLE_ + 8.  (This is part of the dynamic linker
   reserve area mandated by the ABI.)  */

static CORE_ADDR
lm_base (void)
{
  struct minimal_symbol *got_sym;
  CORE_ADDR addr;
  gdb_byte buf[FRV_PTR_SIZE];

  /* One of our assumptions is that the main executable has been relocated.
     Bail out if this has not happened.  (Note that post_create_inferior()
     in infcmd.c will call solib_add prior to solib_create_inferior_hook().
     If we allow this to happen, lm_base_cache will be initialized with
     a bogus value.  */
  if (main_executable_lm_info == 0)
    return 0;

  /* If we already have a cached value, return it.  */
  if (lm_base_cache)
    return lm_base_cache;

  got_sym = lookup_minimal_symbol ("_GLOBAL_OFFSET_TABLE_", NULL,
                                   symfile_objfile);
  if (got_sym == 0)
    {
      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
	                    "lm_base: _GLOBAL_OFFSET_TABLE_ not found.\n");
      return 0;
    }

  addr = SYMBOL_VALUE_ADDRESS (got_sym) + 8;

  if (solib_frv_debug)
    fprintf_unfiltered (gdb_stdlog,
			"lm_base: _GLOBAL_OFFSET_TABLE_ + 8 = %s\n",
			hex_string_custom (addr, 8));

  if (target_read_memory (addr, buf, sizeof buf) != 0)
    return 0;
  lm_base_cache = extract_unsigned_integer (buf, sizeof buf);

  if (solib_frv_debug)
    fprintf_unfiltered (gdb_stdlog,
			"lm_base: lm_base_cache = %s\n",
			hex_string_custom (lm_base_cache, 8));

  return lm_base_cache;
}


/* LOCAL FUNCTION

   frv_current_sos -- build a list of currently loaded shared objects

   SYNOPSIS

   struct so_list *frv_current_sos ()

   DESCRIPTION

   Build a list of `struct so_list' objects describing the shared
   objects currently loaded in the inferior.  This list does not
   include an entry for the main executable file.

   Note that we only gather information directly available from the
   inferior --- we don't examine any of the shared library files
   themselves.  The declaration of `struct so_list' says which fields
   we provide values for.  */

static struct so_list *
frv_current_sos (void)
{
  CORE_ADDR lm_addr, mgot;
  struct so_list *sos_head = NULL;
  struct so_list **sos_next_ptr = &sos_head;

  /* Make sure that the main executable has been relocated.  This is
     required in order to find the address of the global offset table,
     which in turn is used to find the link map info.  (See lm_base()
     for details.)

     Note that the relocation of the main executable is also performed
     by SOLIB_CREATE_INFERIOR_HOOK(), however, in the case of core
     files, this hook is called too late in order to be of benefit to
     SOLIB_ADD.  SOLIB_ADD eventually calls this this function,
     frv_current_sos, and also precedes the call to
     SOLIB_CREATE_INFERIOR_HOOK().   (See post_create_inferior() in
     infcmd.c.)  */
  if (main_executable_lm_info == 0 && core_bfd != NULL)
    frv_relocate_main_executable ();

  /* Fetch the GOT corresponding to the main executable.  */
  mgot = main_got ();

  /* Locate the address of the first link map struct.  */
  lm_addr = lm_base ();

  /* We have at least one link map entry.  Fetch the the lot of them,
     building the solist chain.  */
  while (lm_addr)
    {
      struct ext_link_map lm_buf;
      CORE_ADDR got_addr;

      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
			    "current_sos: reading link_map entry at %s\n",
			    hex_string_custom (lm_addr, 8));

      if (target_read_memory (lm_addr, (gdb_byte *) &lm_buf, sizeof (lm_buf)) != 0)
	{
	  warning (_("frv_current_sos: Unable to read link map entry.  Shared object chain may be incomplete."));
	  break;
	}

      got_addr
	= extract_unsigned_integer (lm_buf.l_addr.got_value,
				    sizeof (lm_buf.l_addr.got_value));
      /* If the got_addr is the same as mgotr, then we're looking at the
	 entry for the main executable.  By convention, we don't include
	 this in the list of shared objects.  */
      if (got_addr != mgot)
	{
	  int errcode;
	  char *name_buf;
	  struct int_elf32_fdpic_loadmap *loadmap;
	  struct so_list *sop;
	  CORE_ADDR addr;

	  /* Fetch the load map address.  */
	  addr = extract_unsigned_integer (lm_buf.l_addr.map,
					   sizeof lm_buf.l_addr.map);
	  loadmap = fetch_loadmap (addr);
	  if (loadmap == NULL)
	    {
	      warning (_("frv_current_sos: Unable to fetch load map.  Shared object chain may be incomplete."));
	      break;
	    }

	  sop = xcalloc (1, sizeof (struct so_list));
	  sop->lm_info = xcalloc (1, sizeof (struct lm_info));
	  sop->lm_info->map = loadmap;
	  sop->lm_info->got_value = got_addr;
	  sop->lm_info->lm_addr = lm_addr;
	  /* Fetch the name.  */
	  addr = extract_unsigned_integer (lm_buf.l_name,
					   sizeof (lm_buf.l_name));
	  target_read_string (addr, &name_buf, SO_NAME_MAX_PATH_SIZE - 1,
			      &errcode);

	  if (solib_frv_debug)
	    fprintf_unfiltered (gdb_stdlog, "current_sos: name = %s\n",
	                        name_buf);
	  
	  if (errcode != 0)
	    warning (_("Can't read pathname for link map entry: %s."),
		     safe_strerror (errcode));
	  else
	    {
	      strncpy (sop->so_name, name_buf, SO_NAME_MAX_PATH_SIZE - 1);
	      sop->so_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
	      xfree (name_buf);
	      strcpy (sop->so_original_name, sop->so_name);
	    }

	  *sos_next_ptr = sop;
	  sos_next_ptr = &sop->next;
	}
      else
	{
	  main_lm_addr = lm_addr;
	}

      lm_addr = extract_unsigned_integer (lm_buf.l_next, sizeof (lm_buf.l_next));
    }

  enable_break2 ();

  return sos_head;
}


/* Return 1 if PC lies in the dynamic symbol resolution code of the
   run time loader.  */

static CORE_ADDR interp_text_sect_low;
static CORE_ADDR interp_text_sect_high;
static CORE_ADDR interp_plt_sect_low;
static CORE_ADDR interp_plt_sect_high;

static int
frv_in_dynsym_resolve_code (CORE_ADDR pc)
{
  return ((pc >= interp_text_sect_low && pc < interp_text_sect_high)
	  || (pc >= interp_plt_sect_low && pc < interp_plt_sect_high)
	  || in_plt_section (pc, NULL));
}

/* Given a loadmap and an address, return the displacement needed
   to relocate the address.  */

CORE_ADDR
displacement_from_map (struct int_elf32_fdpic_loadmap *map,
                       CORE_ADDR addr)
{
  int seg;

  for (seg = 0; seg < map->nsegs; seg++)
    {
      if (map->segs[seg].p_vaddr <= addr
          && addr < map->segs[seg].p_vaddr + map->segs[seg].p_memsz)
	{
	  return map->segs[seg].addr - map->segs[seg].p_vaddr;
	}
    }

  return 0;
}

/* Print a warning about being unable to set the dynamic linker
   breakpoint.  */

static void
enable_break_failure_warning (void)
{
  warning (_("Unable to find dynamic linker breakpoint function.\n"
           "GDB will be unable to debug shared library initializers\n"
	   "and track explicitly loaded dynamic code."));
}

/*

   LOCAL FUNCTION

   enable_break -- arrange for dynamic linker to hit breakpoint

   SYNOPSIS

   int enable_break (void)

   DESCRIPTION

   The dynamic linkers has, as part of its debugger interface, support
   for arranging for the inferior to hit a breakpoint after mapping in
   the shared libraries.  This function enables that breakpoint.

   On the FR-V, using the shared library (FDPIC) ABI, the symbol
   _dl_debug_addr points to the r_debug struct which contains
   a field called r_brk.  r_brk is the address of the function
   descriptor upon which a breakpoint must be placed.  Being a
   function descriptor, we must extract the entry point in order
   to set the breakpoint.

   Our strategy will be to get the .interp section from the
   executable.  This section will provide us with the name of the
   interpreter.  We'll open the interpreter and then look up
   the address of _dl_debug_addr.  We then relocate this address
   using the interpreter's loadmap.  Once the relocated address
   is known, we fetch the value (address) corresponding to r_brk
   and then use that value to fetch the entry point of the function
   we're interested in.

 */

static int enable_break1_done = 0;
static int enable_break2_done = 0;

static int
enable_break2 (void)
{
  int success = 0;
  char **bkpt_namep;
  asection *interp_sect;

  if (!enable_break1_done || enable_break2_done)
    return 1;

  enable_break2_done = 1;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  interp_text_sect_low = interp_text_sect_high = 0;
  interp_plt_sect_low = interp_plt_sect_high = 0;

  /* Find the .interp section; if not found, warn the user and drop
     into the old breakpoint at symbol code.  */
  interp_sect = bfd_get_section_by_name (exec_bfd, ".interp");
  if (interp_sect)
    {
      unsigned int interp_sect_size;
      gdb_byte *buf;
      bfd *tmp_bfd = NULL;
      int tmp_fd = -1;
      char *tmp_pathname = NULL;
      int status;
      CORE_ADDR addr, interp_loadmap_addr;
      gdb_byte addr_buf[FRV_PTR_SIZE];
      struct int_elf32_fdpic_loadmap *ldm;

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

      tmp_fd  = solib_open (buf, &tmp_pathname);
      if (tmp_fd >= 0)
	tmp_bfd = bfd_fopen (tmp_pathname, gnutarget, FOPEN_RB, tmp_fd);

      if (tmp_bfd == NULL)
	{
	  enable_break_failure_warning ();
	  return 0;
	}

      /* Make sure the dynamic linker is really a useful object.  */
      if (!bfd_check_format (tmp_bfd, bfd_object))
	{
	  warning (_("Unable to grok dynamic linker %s as an object file"), buf);
	  enable_break_failure_warning ();
	  bfd_close (tmp_bfd);
	  return 0;
	}

      status = frv_fdpic_loadmap_addresses (current_gdbarch,
                                            &interp_loadmap_addr, 0);
      if (status < 0)
	{
	  warning (_("Unable to determine dynamic linker loadmap address."));
	  enable_break_failure_warning ();
	  bfd_close (tmp_bfd);
	  return 0;
	}

      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
	                    "enable_break: interp_loadmap_addr = %s\n",
			    hex_string_custom (interp_loadmap_addr, 8));

      ldm = fetch_loadmap (interp_loadmap_addr);
      if (ldm == NULL)
	{
	  warning (_("Unable to load dynamic linker loadmap at address %s."),
	           hex_string_custom (interp_loadmap_addr, 8));
	  enable_break_failure_warning ();
	  bfd_close (tmp_bfd);
	  return 0;
	}

      /* Record the relocated start and end address of the dynamic linker
         text and plt section for svr4_in_dynsym_resolve_code.  */
      interp_sect = bfd_get_section_by_name (tmp_bfd, ".text");
      if (interp_sect)
	{
	  interp_text_sect_low
	    = bfd_section_vma (tmp_bfd, interp_sect);
	  interp_text_sect_low
	    += displacement_from_map (ldm, interp_text_sect_low);
	  interp_text_sect_high
	    = interp_text_sect_low + bfd_section_size (tmp_bfd, interp_sect);
	}
      interp_sect = bfd_get_section_by_name (tmp_bfd, ".plt");
      if (interp_sect)
	{
	  interp_plt_sect_low =
	    bfd_section_vma (tmp_bfd, interp_sect);
	  interp_plt_sect_low
	    += displacement_from_map (ldm, interp_plt_sect_low);
	  interp_plt_sect_high =
	    interp_plt_sect_low + bfd_section_size (tmp_bfd, interp_sect);
	}

      addr = bfd_lookup_symbol (tmp_bfd, "_dl_debug_addr");
      if (addr == 0)
	{
	  warning (_("Could not find symbol _dl_debug_addr in dynamic linker"));
	  enable_break_failure_warning ();
	  bfd_close (tmp_bfd);
	  return 0;
	}

      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
	                    "enable_break: _dl_debug_addr (prior to relocation) = %s\n",
			    hex_string_custom (addr, 8));

      addr += displacement_from_map (ldm, addr);

      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
	                    "enable_break: _dl_debug_addr (after relocation) = %s\n",
			    hex_string_custom (addr, 8));

      /* Fetch the address of the r_debug struct.  */
      if (target_read_memory (addr, addr_buf, sizeof addr_buf) != 0)
	{
	  warning (_("Unable to fetch contents of _dl_debug_addr (at address %s) from dynamic linker"),
	           hex_string_custom (addr, 8));
	}
      addr = extract_unsigned_integer (addr_buf, sizeof addr_buf);

      /* Fetch the r_brk field.  It's 8 bytes from the start of
         _dl_debug_addr.  */
      if (target_read_memory (addr + 8, addr_buf, sizeof addr_buf) != 0)
	{
	  warning (_("Unable to fetch _dl_debug_addr->r_brk (at address %s) from dynamic linker"),
	           hex_string_custom (addr + 8, 8));
	  enable_break_failure_warning ();
	  bfd_close (tmp_bfd);
	  return 0;
	}
      addr = extract_unsigned_integer (addr_buf, sizeof addr_buf);

      /* Now fetch the function entry point.  */
      if (target_read_memory (addr, addr_buf, sizeof addr_buf) != 0)
	{
	  warning (_("Unable to fetch _dl_debug_addr->.r_brk entry point (at address %s) from dynamic linker"),
	           hex_string_custom (addr, 8));
	  enable_break_failure_warning ();
	  bfd_close (tmp_bfd);
	  return 0;
	}
      addr = extract_unsigned_integer (addr_buf, sizeof addr_buf);

      /* We're done with the temporary bfd.  */
      bfd_close (tmp_bfd);

      /* We're also done with the loadmap.  */
      xfree (ldm);

      /* Now (finally!) create the solib breakpoint.  */
      create_solib_event_breakpoint (addr);

      return 1;
    }

  /* Tell the user we couldn't set a dynamic linker breakpoint.  */
  enable_break_failure_warning ();

  /* Failure return.  */
  return 0;
}

static int
enable_break (void)
{
  asection *interp_sect;

  /* Remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  /* Check for the presence of a .interp section.  If there is no
     such section, the executable is statically linked.  */

  interp_sect = bfd_get_section_by_name (exec_bfd, ".interp");

  if (interp_sect)
    {
      enable_break1_done = 1;
      create_solib_event_breakpoint (symfile_objfile->ei.entry_point);

      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
			    "enable_break: solib event breakpoint placed at entry point: %s\n",
			    hex_string_custom
			      (symfile_objfile->ei.entry_point, 8));
    }
  else
    {
      if (solib_frv_debug)
	fprintf_unfiltered (gdb_stdlog,
	                    "enable_break: No .interp section found.\n");
    }

  return 1;
}

/*

   LOCAL FUNCTION

   special_symbol_handling -- additional shared library symbol handling

   SYNOPSIS

   void special_symbol_handling ()

   DESCRIPTION

   Once the symbols from a shared object have been loaded in the usual
   way, we are called to do any system specific symbol handling that 
   is needed.

 */

static void
frv_special_symbol_handling (void)
{
  /* Nothing needed (yet) for FRV. */
}

static void
frv_relocate_main_executable (void)
{
  int status;
  CORE_ADDR exec_addr;
  struct int_elf32_fdpic_loadmap *ldm;
  struct cleanup *old_chain;
  struct section_offsets *new_offsets;
  int changed;
  struct obj_section *osect;

  status = frv_fdpic_loadmap_addresses (current_gdbarch, 0, &exec_addr);

  if (status < 0)
    {
      /* Not using FDPIC ABI, so do nothing.  */
      return;
    }

  /* Fetch the loadmap located at ``exec_addr''.  */
  ldm = fetch_loadmap (exec_addr);
  if (ldm == NULL)
    error (_("Unable to load the executable's loadmap."));

  if (main_executable_lm_info)
    xfree (main_executable_lm_info);
  main_executable_lm_info = xcalloc (1, sizeof (struct lm_info));
  main_executable_lm_info->map = ldm;

  new_offsets = xcalloc (symfile_objfile->num_sections,
			 sizeof (struct section_offsets));
  old_chain = make_cleanup (xfree, new_offsets);
  changed = 0;

  ALL_OBJFILE_OSECTIONS (symfile_objfile, osect)
    {
      CORE_ADDR orig_addr, addr, offset;
      int osect_idx;
      int seg;
      
      osect_idx = osect->the_bfd_section->index;

      /* Current address of section.  */
      addr = osect->addr;
      /* Offset from where this section started.  */
      offset = ANOFFSET (symfile_objfile->section_offsets, osect_idx);
      /* Original address prior to any past relocations.  */
      orig_addr = addr - offset;

      for (seg = 0; seg < ldm->nsegs; seg++)
	{
	  if (ldm->segs[seg].p_vaddr <= orig_addr
	      && orig_addr < ldm->segs[seg].p_vaddr + ldm->segs[seg].p_memsz)
	    {
	      new_offsets->offsets[osect_idx]
		= ldm->segs[seg].addr - ldm->segs[seg].p_vaddr;

	      if (new_offsets->offsets[osect_idx] != offset)
		changed = 1;
	      break;
	    }
	}
    }

  if (changed)
    objfile_relocate (symfile_objfile, new_offsets);

  do_cleanups (old_chain);

  /* Now that symfile_objfile has been relocated, we can compute the
     GOT value and stash it away.  */
  main_executable_lm_info->got_value = main_got ();
}

/*

   GLOBAL FUNCTION

   frv_solib_create_inferior_hook -- shared library startup support

   SYNOPSIS

   void frv_solib_create_inferior_hook ()

   DESCRIPTION

   When gdb starts up the inferior, it nurses it along (through the
   shell) until it is ready to execute it's first instruction.  At this
   point, this function gets called via expansion of the macro
   SOLIB_CREATE_INFERIOR_HOOK.

   For the FR-V shared library ABI (FDPIC), the main executable
   needs to be relocated.  The shared library breakpoints also need
   to be enabled.
 */

static void
frv_solib_create_inferior_hook (void)
{
  /* Relocate main executable.  */
  frv_relocate_main_executable ();

  /* Enable shared library breakpoints.  */
  if (!enable_break ())
    {
      warning (_("shared library handler failed to enable breakpoint"));
      return;
    }
}

static void
frv_clear_solib (void)
{
  lm_base_cache = 0;
  enable_break1_done = 0;
  enable_break2_done = 0;
  main_lm_addr = 0;
  if (main_executable_lm_info != 0)
    {
      xfree (main_executable_lm_info->map);
      xfree (main_executable_lm_info->dyn_syms);
      xfree (main_executable_lm_info->dyn_relocs);
      xfree (main_executable_lm_info);
      main_executable_lm_info = 0;
    }
}

static void
frv_free_so (struct so_list *so)
{
  xfree (so->lm_info->map);
  xfree (so->lm_info->dyn_syms);
  xfree (so->lm_info->dyn_relocs);
  xfree (so->lm_info);
}

static void
frv_relocate_section_addresses (struct so_list *so,
                                 struct section_table *sec)
{
  int seg;
  struct int_elf32_fdpic_loadmap *map;

  map = so->lm_info->map;

  for (seg = 0; seg < map->nsegs; seg++)
    {
      if (map->segs[seg].p_vaddr <= sec->addr
          && sec->addr < map->segs[seg].p_vaddr + map->segs[seg].p_memsz)
	{
	  CORE_ADDR displ = map->segs[seg].addr - map->segs[seg].p_vaddr;
	  sec->addr += displ;
	  sec->endaddr += displ;
	  break;
	}
    }
}

/* Return the GOT address associated with the main executable.  Return
   0 if it can't be found.  */

static CORE_ADDR
main_got (void)
{
  struct minimal_symbol *got_sym;

  got_sym = lookup_minimal_symbol ("_GLOBAL_OFFSET_TABLE_", NULL, symfile_objfile);
  if (got_sym == 0)
    return 0;

  return SYMBOL_VALUE_ADDRESS (got_sym);
}

/* Find the global pointer for the given function address ADDR.  */

CORE_ADDR
frv_fdpic_find_global_pointer (CORE_ADDR addr)
{
  struct so_list *so;

  so = master_so_list ();
  while (so)
    {
      int seg;
      struct int_elf32_fdpic_loadmap *map;

      map = so->lm_info->map;

      for (seg = 0; seg < map->nsegs; seg++)
	{
	  if (map->segs[seg].addr <= addr
	      && addr < map->segs[seg].addr + map->segs[seg].p_memsz)
	    return so->lm_info->got_value;
	}

      so = so->next;
    }

  /* Didn't find it it any of the shared objects.  So assume it's in the
     main executable.  */
  return main_got ();
}

/* Forward declarations for frv_fdpic_find_canonical_descriptor().  */
static CORE_ADDR find_canonical_descriptor_in_load_object
  (CORE_ADDR, CORE_ADDR, char *, bfd *, struct lm_info *);

/* Given a function entry point, attempt to find the canonical descriptor
   associated with that entry point.  Return 0 if no canonical descriptor
   could be found.  */

CORE_ADDR
frv_fdpic_find_canonical_descriptor (CORE_ADDR entry_point)
{
  char *name;
  CORE_ADDR addr;
  CORE_ADDR got_value;
  struct int_elf32_fdpic_loadmap *ldm = 0;
  struct symbol *sym;
  int status;
  CORE_ADDR exec_loadmap_addr;

  /* Fetch the corresponding global pointer for the entry point.  */
  got_value = frv_fdpic_find_global_pointer (entry_point);

  /* Attempt to find the name of the function.  If the name is available,
     it'll be used as an aid in finding matching functions in the dynamic
     symbol table.  */
  sym = find_pc_function (entry_point);
  if (sym == 0)
    name = 0;
  else
    name = SYMBOL_LINKAGE_NAME (sym);

  /* Check the main executable.  */
  addr = find_canonical_descriptor_in_load_object
           (entry_point, got_value, name, symfile_objfile->obfd,
	    main_executable_lm_info);

  /* If descriptor not found via main executable, check each load object
     in list of shared objects.  */
  if (addr == 0)
    {
      struct so_list *so;

      so = master_so_list ();
      while (so)
	{
	  addr = find_canonical_descriptor_in_load_object
		   (entry_point, got_value, name, so->abfd, so->lm_info);

	  if (addr != 0)
	    break;

	  so = so->next;
	}
    }

  return addr;
}

static CORE_ADDR
find_canonical_descriptor_in_load_object
  (CORE_ADDR entry_point, CORE_ADDR got_value, char *name, bfd *abfd,
   struct lm_info *lm)
{
  arelent *rel;
  unsigned int i;
  CORE_ADDR addr = 0;

  /* Nothing to do if no bfd.  */
  if (abfd == 0)
    return 0;

  /* Nothing to do if no link map.  */
  if (lm == 0)
    return 0;

  /* We want to scan the dynamic relocs for R_FRV_FUNCDESC relocations.
     (More about this later.)  But in order to fetch the relocs, we
     need to first fetch the dynamic symbols.  These symbols need to
     be cached due to the way that bfd_canonicalize_dynamic_reloc()
     works.  (See the comments in the declaration of struct lm_info
     for more information.)  */
  if (lm->dyn_syms == NULL)
    {
      long storage_needed;
      unsigned int number_of_symbols;

      /* Determine amount of space needed to hold the dynamic symbol table.  */
      storage_needed = bfd_get_dynamic_symtab_upper_bound (abfd);

      /* If there are no dynamic symbols, there's nothing to do.  */
      if (storage_needed <= 0)
	return 0;

      /* Allocate space for the dynamic symbol table.  */
      lm->dyn_syms = (asymbol **) xmalloc (storage_needed);

      /* Fetch the dynamic symbol table.  */
      number_of_symbols = bfd_canonicalize_dynamic_symtab (abfd, lm->dyn_syms);

      if (number_of_symbols == 0)
	return 0;
    }

  /* Fetch the dynamic relocations if not already cached.  */
  if (lm->dyn_relocs == NULL)
    {
      long storage_needed;

      /* Determine amount of space needed to hold the dynamic relocs.  */
      storage_needed = bfd_get_dynamic_reloc_upper_bound (abfd);

      /* Bail out if there are no dynamic relocs.  */
      if (storage_needed <= 0)
	return 0;

      /* Allocate space for the relocs.  */
      lm->dyn_relocs = (arelent **) xmalloc (storage_needed);

      /* Fetch the dynamic relocs.  */
      lm->dyn_reloc_count 
	= bfd_canonicalize_dynamic_reloc (abfd, lm->dyn_relocs, lm->dyn_syms);
    }

  /* Search the dynamic relocs.  */
  for (i = 0; i < lm->dyn_reloc_count; i++)
    {
      rel = lm->dyn_relocs[i];

      /* Relocs of interest are those which meet the following
         criteria:

	   - the names match (assuming the caller could provide
	     a name which matches ``entry_point'').
	   - the relocation type must be R_FRV_FUNCDESC.  Relocs
	     of this type are used (by the dynamic linker) to
	     look up the address of a canonical descriptor (allocating
	     it if need be) and initializing the GOT entry referred
	     to by the offset to the address of the descriptor.

	 These relocs of interest may be used to obtain a
	 candidate descriptor by first adjusting the reloc's
	 address according to the link map and then dereferencing
	 this address (which is a GOT entry) to obtain a descriptor
	 address.  */
      if ((name == 0 || strcmp (name, (*rel->sym_ptr_ptr)->name) == 0)
          && rel->howto->type == R_FRV_FUNCDESC)
	{
	  gdb_byte buf [FRV_PTR_SIZE];

	  /* Compute address of address of candidate descriptor.  */
	  addr = rel->address + displacement_from_map (lm->map, rel->address);

	  /* Fetch address of candidate descriptor.  */
	  if (target_read_memory (addr, buf, sizeof buf) != 0)
	    continue;
	  addr = extract_unsigned_integer (buf, sizeof buf);

	  /* Check for matching entry point.  */
	  if (target_read_memory (addr, buf, sizeof buf) != 0)
	    continue;
	  if (extract_unsigned_integer (buf, sizeof buf) != entry_point)
	    continue;

	  /* Check for matching got value.  */
	  if (target_read_memory (addr + 4, buf, sizeof buf) != 0)
	    continue;
	  if (extract_unsigned_integer (buf, sizeof buf) != got_value)
	    continue;

	  /* Match was successful!  Exit loop.  */
	  break;
	}
    }

  return addr;
}

/* Given an objfile, return the address of its link map.  This value is
   needed for TLS support.  */
CORE_ADDR
frv_fetch_objfile_link_map (struct objfile *objfile)
{
  struct so_list *so;

  /* Cause frv_current_sos() to be run if it hasn't been already.  */
  if (main_lm_addr == 0)
    solib_add (0, 0, 0, 1);

  /* frv_current_sos() will set main_lm_addr for the main executable.  */
  if (objfile == symfile_objfile)
    return main_lm_addr;

  /* The other link map addresses may be found by examining the list
     of shared libraries.  */
  for (so = master_so_list (); so; so = so->next)
    {
      if (so->objfile == objfile)
	return so->lm_info->lm_addr;
    }

  /* Not found!  */
  return 0;
}

static struct target_so_ops frv_so_ops;

void
_initialize_frv_solib (void)
{
  frv_so_ops.relocate_section_addresses = frv_relocate_section_addresses;
  frv_so_ops.free_so = frv_free_so;
  frv_so_ops.clear_solib = frv_clear_solib;
  frv_so_ops.solib_create_inferior_hook = frv_solib_create_inferior_hook;
  frv_so_ops.special_symbol_handling = frv_special_symbol_handling;
  frv_so_ops.current_sos = frv_current_sos;
  frv_so_ops.open_symbol_file_object = open_symbol_file_object;
  frv_so_ops.in_dynsym_resolve_code = frv_in_dynsym_resolve_code;

  /* FIXME: Don't do this here.  *_gdbarch_init() should set so_ops. */
  current_target_so_ops = &frv_so_ops;

  /* Debug this file's internals.  */
  add_setshow_zinteger_cmd ("solib-frv", class_maintenance,
			    &solib_frv_debug, _("\
Set internal debugging of shared library code for FR-V."), _("\
Show internal debugging of shared library code for FR-V."), _("\
When non-zero, FR-V solib specific internal debugging is enabled."),
			    NULL,
			    NULL, /* FIXME: i18n: */
			    &setdebuglist, &showdebuglist);
}
