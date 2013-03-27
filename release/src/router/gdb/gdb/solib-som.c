/* Handle SOM shared libraries.

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

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
#include "som.h"
#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "target.h"
#include "inferior.h"

#include "hppa-tdep.h"
#include "solist.h"

#undef SOLIB_SOM_DBG 

/* These ought to be defined in some public interface, but aren't.  They
   define the meaning of the various bits in the distinguished __dld_flags
   variable that is declared in every debuggable a.out on HP-UX, and that
   is shared between the debugger and the dynamic linker.
 */
#define DLD_FLAGS_MAPPRIVATE    0x1
#define DLD_FLAGS_HOOKVALID     0x2
#define DLD_FLAGS_LISTVALID     0x4
#define DLD_FLAGS_BOR_ENABLE    0x8

struct lm_info
  {
    /* Version of this structure (it is expected to change again in hpux10).  */
    unsigned char struct_version;

    /* Binding mode for this library.  */
    unsigned char bind_mode;

    /* Version of this library.  */
    short library_version;

    /* Start of text address,
       link-time text location (length of text area),
       end of text address.  */
    CORE_ADDR text_addr;
    CORE_ADDR text_link_addr;
    CORE_ADDR text_end;

    /* Start of data, start of bss and end of data.  */
    CORE_ADDR data_start;
    CORE_ADDR bss_start;
    CORE_ADDR data_end;

    /* Value of linkage pointer (%r19).  */
    CORE_ADDR got_value;

    /* Address in target of offset from thread-local register of
       start of this thread's data.  I.e., the first thread-local
       variable in this shared library starts at *(tsd_start_addr)
       from that area pointed to by cr27 (mpsfu_hi).
      
       We do the indirection as soon as we read it, so from then
       on it's the offset itself.  */
    CORE_ADDR tsd_start_addr;

    /* Address of the link map entry in the loader.  */
    CORE_ADDR lm_addr;
  };

/* These addresses should be filled in by som_solib_create_inferior_hook.
   They are also used elsewhere in this module.
 */
typedef struct
  {
    CORE_ADDR address;
    struct unwind_table_entry *unwind;
  }
addr_and_unwind_t;

/* When adding fields, be sure to clear them in _initialize_som_solib. */
static struct
  {
    int is_valid;
    addr_and_unwind_t hook;
    addr_and_unwind_t hook_stub;
    addr_and_unwind_t load;
    addr_and_unwind_t load_stub;
    addr_and_unwind_t unload;
    addr_and_unwind_t unload2;
    addr_and_unwind_t unload_stub;
  }
dld_cache;

static void
som_relocate_section_addresses (struct so_list *so,
				struct section_table *sec)
{
  flagword aflag = bfd_get_section_flags(so->abfd, sec->the_bfd_section);

  if (aflag & SEC_CODE)
    {
      sec->addr    += so->lm_info->text_addr - so->lm_info->text_link_addr; 
      sec->endaddr += so->lm_info->text_addr - so->lm_info->text_link_addr;
    }
  else if (aflag & SEC_DATA)
    {
      sec->addr    += so->lm_info->data_start; 
      sec->endaddr += so->lm_info->data_start;
    }
  else
    ;
}

/* This hook gets called just before the first instruction in the
   inferior process is executed.

   This is our opportunity to set magic flags in the inferior so
   that GDB can be notified when a shared library is mapped in and
   to tell the dynamic linker that a private copy of the library is
   needed (so GDB can set breakpoints in the library).

   __dld_flags is the location of the magic flags; as of this implementation
   there are 3 flags of interest:

   bit 0 when set indicates that private copies of the libraries are needed
   bit 1 when set indicates that the callback hook routine is valid
   bit 2 when set indicates that the dynamic linker should maintain the
   __dld_list structure when loading/unloading libraries.

   Note that shared libraries are not mapped in at this time, so we have
   run the inferior until the libraries are mapped in.  Typically this
   means running until the "_start" is called.  */

static void
som_solib_create_inferior_hook (void)
{
  struct minimal_symbol *msymbol;
  unsigned int dld_flags, status, have_endo;
  asection *shlib_info;
  char buf[4];
  CORE_ADDR anaddr;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  if (symfile_objfile == NULL)
    return;

  /* First see if the objfile was dynamically linked.  */
  shlib_info = bfd_get_section_by_name (symfile_objfile->obfd, "$SHLIB_INFO$");
  if (!shlib_info)
    return;

  /* It's got a $SHLIB_INFO$ section, make sure it's not empty.  */
  if (bfd_section_size (symfile_objfile->obfd, shlib_info) == 0)
    return;

  have_endo = 0;
  /* Slam the pid of the process into __d_pid.

     We used to warn when this failed, but that warning is only useful
     on very old HP systems (hpux9 and older).  The warnings are an
     annoyance to users of modern systems and foul up the testsuite as
     well.  As a result, the warnings have been disabled.  */
  msymbol = lookup_minimal_symbol ("__d_pid", NULL, symfile_objfile);
  if (msymbol == NULL)
    goto keep_going;

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  store_unsigned_integer (buf, 4, PIDGET (inferior_ptid));
  status = target_write_memory (anaddr, buf, 4);
  if (status != 0)
    {
      warning (_("\
Unable to write __d_pid.\n\
Suggest linking with /opt/langtools/lib/end.o.\n\
GDB will be unable to track shl_load/shl_unload calls"));
      goto keep_going;
    }

  /* Get the value of _DLD_HOOK (an export stub) and put it in __dld_hook;
     This will force the dynamic linker to call __d_trap when significant
     events occur.

     Note that the above is the pre-HP-UX 9.0 behaviour.  At 9.0 and above,
     the dld provides an export stub named "__d_trap" as well as the
     function named "__d_trap" itself, but doesn't provide "_DLD_HOOK".
     We'll look first for the old flavor and then the new.
   */
  msymbol = lookup_minimal_symbol ("_DLD_HOOK", NULL, symfile_objfile);
  if (msymbol == NULL)
    msymbol = lookup_minimal_symbol ("__d_trap", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning (_("\
Unable to find _DLD_HOOK symbol in object file.\n\
Suggest linking with /opt/langtools/lib/end.o.\n\
GDB will be unable to track shl_load/shl_unload calls"));
      goto keep_going;
    }
  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  dld_cache.hook.address = anaddr;

  /* Grrr, this might not be an export symbol!  We have to find the
     export stub.  */
  msymbol = hppa_lookup_stub_minimal_symbol (SYMBOL_LINKAGE_NAME (msymbol),
                                             EXPORT);
  if (msymbol != NULL)
    {
      anaddr = SYMBOL_VALUE (msymbol);
      dld_cache.hook_stub.address = anaddr;
    }
  store_unsigned_integer (buf, 4, anaddr);

  msymbol = lookup_minimal_symbol ("__dld_hook", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning (_("\
Unable to find __dld_hook symbol in object file.\n\
Suggest linking with /opt/langtools/lib/end.o.\n\
GDB will be unable to track shl_load/shl_unload calls"));
      goto keep_going;
    }
  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);
  status = target_write_memory (anaddr, buf, 4);

  /* Now set a shlib_event breakpoint at __d_trap so we can track
     significant shared library events.  */
  msymbol = lookup_minimal_symbol ("__d_trap", NULL, symfile_objfile);
  if (msymbol == NULL)
    {
      warning (_("\
Unable to find __dld_d_trap symbol in object file.\n\
Suggest linking with /opt/langtools/lib/end.o.\n\
GDB will be unable to track shl_load/shl_unload calls"));
      goto keep_going;
    }
  create_solib_event_breakpoint (SYMBOL_VALUE_ADDRESS (msymbol));

  /* We have all the support usually found in end.o, so we can track
     shl_load and shl_unload calls.  */
  have_endo = 1;

keep_going:

  /* Get the address of __dld_flags, if no such symbol exists, then we can
     not debug the shared code.  */
  msymbol = lookup_minimal_symbol ("__dld_flags", NULL, NULL);
  if (msymbol == NULL)
    {
      error (_("Unable to find __dld_flags symbol in object file."));
    }

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);

  /* Read the current contents.  */
  status = target_read_memory (anaddr, buf, 4);
  if (status != 0)
    error (_("Unable to read __dld_flags."));
  dld_flags = extract_unsigned_integer (buf, 4);

  /* Turn on the flags we care about.  */
  dld_flags |= DLD_FLAGS_MAPPRIVATE;
  if (have_endo)
    dld_flags |= DLD_FLAGS_HOOKVALID;
  store_unsigned_integer (buf, 4, dld_flags);
  status = target_write_memory (anaddr, buf, 4);
  if (status != 0)
    error (_("Unable to write __dld_flags."));

  /* Now find the address of _start and set a breakpoint there. 
     We still need this code for two reasons:

     * Not all sites have /opt/langtools/lib/end.o, so it's not always
     possible to track the dynamic linker's events.

     * At this time no events are triggered for shared libraries
     loaded at startup time (what a crock).  */

  msymbol = lookup_minimal_symbol ("_start", NULL, symfile_objfile);
  if (msymbol == NULL)
    error (_("Unable to find _start symbol in object file."));

  anaddr = SYMBOL_VALUE_ADDRESS (msymbol);

  /* Make the breakpoint at "_start" a shared library event breakpoint.  */
  create_solib_event_breakpoint (anaddr);

  clear_symtab_users ();
}

static void
som_special_symbol_handling (void)
{
}

static void
som_solib_desire_dynamic_linker_symbols (void)
{
  struct objfile *objfile;
  struct unwind_table_entry *u;
  struct minimal_symbol *dld_msymbol;

  /* Do we already know the value of these symbols?  If so, then
     we've no work to do.

     (If you add clauses to this test, be sure to likewise update the
     test within the loop.)
   */
  if (dld_cache.is_valid)
    return;

  ALL_OBJFILES (objfile)
  {
    dld_msymbol = lookup_minimal_symbol ("shl_load", NULL, objfile);
    if (dld_msymbol != NULL)
      {
	dld_cache.load.address = SYMBOL_VALUE (dld_msymbol);
	dld_cache.load.unwind = find_unwind_entry (dld_cache.load.address);
      }

    dld_msymbol = lookup_minimal_symbol_solib_trampoline ("shl_load",
							  objfile);
    if (dld_msymbol != NULL)
      {
	if (SYMBOL_TYPE (dld_msymbol) == mst_solib_trampoline)
	  {
	    u = find_unwind_entry (SYMBOL_VALUE (dld_msymbol));
	    if ((u != NULL) && (u->stub_unwind.stub_type == EXPORT))
	      {
		dld_cache.load_stub.address = SYMBOL_VALUE (dld_msymbol);
		dld_cache.load_stub.unwind = u;
	      }
	  }
      }

    dld_msymbol = lookup_minimal_symbol ("shl_unload", NULL, objfile);
    if (dld_msymbol != NULL)
      {
	dld_cache.unload.address = SYMBOL_VALUE (dld_msymbol);
	dld_cache.unload.unwind = find_unwind_entry (dld_cache.unload.address);

	/* ??rehrauer: I'm not sure exactly what this is, but it appears
	   that on some HPUX 10.x versions, there's two unwind regions to
	   cover the body of "shl_unload", the second being 4 bytes past
	   the end of the first.  This is a large hack to handle that
	   case, but since I don't seem to have any legitimate way to
	   look for this thing via the symbol table...
	 */
	if (dld_cache.unload.unwind != NULL)
	  {
	    u = find_unwind_entry (dld_cache.unload.unwind->region_end + 4);
	    if (u != NULL)
	      {
		dld_cache.unload2.address = u->region_start;
		dld_cache.unload2.unwind = u;
	      }
	  }
      }

    dld_msymbol = lookup_minimal_symbol_solib_trampoline ("shl_unload",
							  objfile);
    if (dld_msymbol != NULL)
      {
	if (SYMBOL_TYPE (dld_msymbol) == mst_solib_trampoline)
	  {
	    u = find_unwind_entry (SYMBOL_VALUE (dld_msymbol));
	    if ((u != NULL) && (u->stub_unwind.stub_type == EXPORT))
	      {
		dld_cache.unload_stub.address = SYMBOL_VALUE (dld_msymbol);
		dld_cache.unload_stub.unwind = u;
	      }
	  }
      }

    /* Did we find everything we were looking for?  If so, stop. */
    if ((dld_cache.load.address != 0)
	&& (dld_cache.load_stub.address != 0)
	&& (dld_cache.unload.address != 0)
	&& (dld_cache.unload_stub.address != 0))
      {
	dld_cache.is_valid = 1;
	break;
      }
  }

  dld_cache.hook.unwind = find_unwind_entry (dld_cache.hook.address);
  dld_cache.hook_stub.unwind = find_unwind_entry (dld_cache.hook_stub.address);

  /* We're prepared not to find some of these symbols, which is why
     this function is a "desire" operation, and not a "require".
   */
}

static int
som_in_dynsym_resolve_code (CORE_ADDR pc)
{
  struct unwind_table_entry *u_pc;

  /* Are we in the dld itself?

     ??rehrauer: Large hack -- We'll assume that any address in a
     shared text region is the dld's text.  This would obviously
     fall down if the user attached to a process, whose shlibs
     weren't mapped to a (writeable) private region.  However, in
     that case the debugger probably isn't able to set the fundamental
     breakpoint in the dld callback anyways, so this hack should be
     safe.
   */
  if ((pc & (CORE_ADDR) 0xc0000000) == (CORE_ADDR) 0xc0000000)
    return 1;

  /* Cache the address of some symbols that are part of the dynamic
     linker, if not already known.
   */
  som_solib_desire_dynamic_linker_symbols ();

  /* Are we in the dld callback?  Or its export stub? */
  u_pc = find_unwind_entry (pc);
  if (u_pc == NULL)
    return 0;

  if ((u_pc == dld_cache.hook.unwind) || (u_pc == dld_cache.hook_stub.unwind))
    return 1;

  /* Or the interface of the dld (i.e., "shl_load" or friends)? */
  if ((u_pc == dld_cache.load.unwind)
      || (u_pc == dld_cache.unload.unwind)
      || (u_pc == dld_cache.unload2.unwind)
      || (u_pc == dld_cache.load_stub.unwind)
      || (u_pc == dld_cache.unload_stub.unwind))
    return 1;

  /* Apparently this address isn't part of the dld's text. */
  return 0;
}

static void
som_clear_solib (void)
{
}

struct dld_list {
  char name[4];
  char info[4];
  char text_addr[4];
  char text_link_addr[4];
  char text_end[4];
  char data_start[4];
  char bss_start[4];
  char data_end[4];
  char got_value[4];
  char next[4];
  char tsd_start_addr_ptr[4];
};

static CORE_ADDR
link_map_start (void)
{
  struct minimal_symbol *sym;
  CORE_ADDR addr;
  char buf[4];
  unsigned int dld_flags;

  sym = lookup_minimal_symbol ("__dld_flags", NULL, NULL);
  if (!sym)
    error (_("Unable to find __dld_flags symbol in object file."));
  addr = SYMBOL_VALUE_ADDRESS (sym);
  read_memory (addr, buf, 4);
  dld_flags = extract_unsigned_integer (buf, 4);
  if ((dld_flags & DLD_FLAGS_LISTVALID) == 0)
    error (_("__dld_list is not valid according to __dld_flags."));

  /* If the libraries were not mapped private, warn the user.  */
  if ((dld_flags & DLD_FLAGS_MAPPRIVATE) == 0)
    warning (_("The shared libraries were not privately mapped; setting a\n"
	     "breakpoint in a shared library will not work until you rerun the "
	     "program.\n"));

  sym = lookup_minimal_symbol ("__dld_list", NULL, NULL);
  if (!sym)
    {
      /* Older crt0.o files (hpux8) don't have __dld_list as a symbol,
         but the data is still available if you know where to look.  */
      sym = lookup_minimal_symbol ("__dld_flags", NULL, NULL);
      if (!sym)
	{
	  error (_("Unable to find dynamic library list."));
	  return 0;
	}
      addr = SYMBOL_VALUE_ADDRESS (sym) - 8;
    }
  else
    addr = SYMBOL_VALUE_ADDRESS (sym);

  read_memory (addr, buf, 4);
  addr = extract_unsigned_integer (buf, 4);
  if (addr == 0)
    return 0;

  read_memory (addr, buf, 4);
  return extract_unsigned_integer (buf, 4);
}

/* Does this so's name match the main binary? */
static int
match_main (const char *name)
{
  return strcmp (name, symfile_objfile->name) == 0;
}

static struct so_list *
som_current_sos (void)
{
  CORE_ADDR lm;
  struct so_list *head = 0;
  struct so_list **link_ptr = &head;

  for (lm = link_map_start (); lm; )
    {
      char *namebuf;
      CORE_ADDR addr;
      struct so_list *new;
      struct cleanup *old_chain;
      int errcode;
      struct dld_list dbuf;
      char tsdbuf[4];

      new = (struct so_list *) xmalloc (sizeof (struct so_list));
      old_chain = make_cleanup (xfree, new);

      memset (new, 0, sizeof (*new));
      new->lm_info = xmalloc (sizeof (struct lm_info));
      make_cleanup (xfree, new->lm_info);

      read_memory (lm, (gdb_byte *)&dbuf, sizeof (struct dld_list));

      addr = extract_unsigned_integer ((gdb_byte *)&dbuf.name,
				       sizeof (dbuf.name));
      target_read_string (addr, &namebuf, SO_NAME_MAX_PATH_SIZE - 1, &errcode);
      if (errcode != 0)
	warning (_("Can't read pathname for load map: %s."),
		 safe_strerror (errcode));
      else
	{
	  strncpy (new->so_name, namebuf, SO_NAME_MAX_PATH_SIZE - 1);
	  new->so_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
	  xfree (namebuf);
	  strcpy (new->so_original_name, new->so_name);
	}

	if (new->so_name[0] && !match_main (new->so_name))
	  {
	    struct lm_info *lmi = new->lm_info;
	    unsigned int tmp;

	    lmi->lm_addr = lm;

#define EXTRACT(_fld) \
  extract_unsigned_integer ((gdb_byte *)&dbuf._fld, sizeof (dbuf._fld));

	    lmi->text_addr = EXTRACT (text_addr);
	    tmp = EXTRACT (info);
	    lmi->library_version = (tmp >> 16) & 0xffff;
	    lmi->bind_mode = (tmp >> 8) & 0xff;
	    lmi->struct_version = tmp & 0xff;
	    lmi->text_link_addr = EXTRACT (text_link_addr);
	    lmi->text_end = EXTRACT (text_end);
	    lmi->data_start = EXTRACT (data_start);
	    lmi->bss_start = EXTRACT (bss_start);
	    lmi->data_end = EXTRACT (data_end);
	    lmi->got_value = EXTRACT (got_value);
	    tmp = EXTRACT (tsd_start_addr_ptr);
	    read_memory (tmp, tsdbuf, 4);
	    lmi->tsd_start_addr = extract_unsigned_integer (tsdbuf, 4);

#ifdef SOLIB_SOM_DBG
	    printf ("\n+ library \"%s\" is described at 0x%s\n", new->so_name, 
	    	    paddr_nz (lm));
	    printf ("  'version' is %d\n", new->lm_info->struct_version);
	    printf ("  'bind_mode' is %d\n", new->lm_info->bind_mode);
	    printf ("  'library_version' is %d\n", 
	    	    new->lm_info->library_version);
	    printf ("  'text_addr' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->text_addr));
	    printf ("  'text_link_addr' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->text_link_addr));
	    printf ("  'text_end' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->text_end));
	    printf ("  'data_start' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->data_start));
	    printf ("  'bss_start' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->bss_start));
	    printf ("  'data_end' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->data_end));
	    printf ("  'got_value' is %s\n", 
	    	    paddr_nz (new->lm_info->got_value));
	    printf ("  'tsd_start_addr' is 0x%s\n", 
	    	    paddr_nz (new->lm_info->tsd_start_addr));
#endif

	    new->addr_low = lmi->text_addr;
	    new->addr_high = lmi->text_end;

	    /* Link the new object onto the list.  */
	    new->next = NULL;
	    *link_ptr = new;
	    link_ptr = &new->next;
	  }
 	else
	  {
	    free_so (new);
	  }

      lm = EXTRACT (next);
      discard_cleanups (old_chain);
#undef EXTRACT
    }

  /* TODO: The original somsolib code has logic to detect and eliminate 
     duplicate entries.  Do we need that?  */

  return head;
}

static int
som_open_symbol_file_object (void *from_ttyp)
{
  CORE_ADDR lm, l_name;
  char *filename;
  int errcode;
  int from_tty = *(int *)from_ttyp;
  char buf[4];

  if (symfile_objfile)
    if (!query ("Attempt to reload symbols from process? "))
      return 0;

  /* First link map member should be the executable.  */
  if ((lm = link_map_start ()) == 0)
    return 0;	/* failed somehow... */

  /* Read address of name from target memory to GDB.  */
  read_memory (lm + offsetof (struct dld_list, name), buf, 4);

  /* Convert the address to host format.  Assume that the address is
     unsigned.  */
  l_name = extract_unsigned_integer (buf, 4);

  if (l_name == 0)
    return 0;		/* No filename.  */

  /* Now fetch the filename from target memory.  */
  target_read_string (l_name, &filename, SO_NAME_MAX_PATH_SIZE - 1, &errcode);

  if (errcode)
    {
      warning (_("failed to read exec filename from attached file: %s"),
	       safe_strerror (errcode));
      return 0;
    }

  make_cleanup (xfree, filename);
  /* Have a pathname: read the symbol file.  */
  symbol_file_add_main (filename, from_tty);

  return 1;
}

static void
som_free_so (struct so_list *so)
{
  xfree (so->lm_info);
}

static CORE_ADDR
som_solib_thread_start_addr (struct so_list *so)
{
  return so->lm_info->tsd_start_addr;
}

/* Return the GOT value for the shared library in which ADDR belongs.  If
   ADDR isn't in any known shared library, return zero.  */

static CORE_ADDR
som_solib_get_got_by_pc (CORE_ADDR addr)
{
  struct so_list *so_list = master_so_list ();
  CORE_ADDR got_value = 0;

  while (so_list)
    {
      if (so_list->lm_info->text_addr <= addr
	  && so_list->lm_info->text_end > addr)
	{
	  got_value = so_list->lm_info->got_value;
	  break;
	}
      so_list = so_list->next;
    }
  return got_value;
}

/* Return the address of the handle of the shared library in which ADDR belongs.
   If ADDR isn't in any known shared library, return zero.  */
/* this function is used in initialize_hp_cxx_exception_support in 
   hppa-hpux-tdep.c  */

static CORE_ADDR
som_solib_get_solib_by_pc (CORE_ADDR addr)
{
  struct so_list *so_list = master_so_list ();

  while (so_list)
    {
      if (so_list->lm_info->text_addr <= addr
	  && so_list->lm_info->text_end > addr)
	{
	  break;
	}
      so_list = so_list->next;
    }
  if (so_list)
    return so_list->lm_info->lm_addr;
  else
    return 0;
}


static struct target_so_ops som_so_ops;

extern initialize_file_ftype _initialize_som_solib; /* -Wmissing-prototypes */

void
_initialize_som_solib (void)
{
  som_so_ops.relocate_section_addresses = som_relocate_section_addresses;
  som_so_ops.free_so = som_free_so;
  som_so_ops.clear_solib = som_clear_solib;
  som_so_ops.solib_create_inferior_hook = som_solib_create_inferior_hook;
  som_so_ops.special_symbol_handling = som_special_symbol_handling;
  som_so_ops.current_sos = som_current_sos;
  som_so_ops.open_symbol_file_object = som_open_symbol_file_object;
  som_so_ops.in_dynsym_resolve_code = som_in_dynsym_resolve_code;
}

void som_solib_select (struct gdbarch_tdep *tdep)
{
  current_target_so_ops = &som_so_ops;

  tdep->solib_thread_start_addr = som_solib_thread_start_addr;
  tdep->solib_get_got_by_pc = som_solib_get_got_by_pc;
  tdep->solib_get_solib_by_pc = som_solib_get_solib_by_pc;
}

/* The rest of these functions are not part of the solib interface; they 
   are used by somread.c or hppa-hpux-tdep.c */

int
som_solib_section_offsets (struct objfile *objfile,
			   struct section_offsets *offsets)
{
  struct so_list *so_list = master_so_list ();

  while (so_list)
    {
      /* Oh what a pain!  We need the offsets before so_list->objfile
         is valid.  The BFDs will never match.  Make a best guess.  */
      if (strstr (objfile->name, so_list->so_name))
	{
	  asection *private_section;

	  /* The text offset is easy.  */
	  offsets->offsets[SECT_OFF_TEXT (objfile)]
	    = (so_list->lm_info->text_addr
	       - so_list->lm_info->text_link_addr);
	  offsets->offsets[SECT_OFF_RODATA (objfile)]
	    = ANOFFSET (offsets, SECT_OFF_TEXT (objfile));

	  /* We should look at presumed_dp in the SOM header, but
	     that's not easily available.  This should be OK though.  */
	  private_section = bfd_get_section_by_name (objfile->obfd,
						     "$PRIVATE$");
	  if (!private_section)
	    {
	      warning (_("Unable to find $PRIVATE$ in shared library!"));
	      offsets->offsets[SECT_OFF_DATA (objfile)] = 0;
	      offsets->offsets[SECT_OFF_BSS (objfile)] = 0;
	      return 1;
	    }
	  offsets->offsets[SECT_OFF_DATA (objfile)]
	    = (so_list->lm_info->data_start - private_section->vma);
	  offsets->offsets[SECT_OFF_BSS (objfile)]
	    = ANOFFSET (offsets, SECT_OFF_DATA (objfile));
	  return 1;
	}
      so_list = so_list->next;
    }
  return 0;
}
