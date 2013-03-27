/* Frame unwinder for frames using the libunwind library.

   Copyright (C) 2003, 2004, 2006, 2007 Free Software Foundation, Inc.

   Written by Jeff Johnston, contributed by Red Hat Inc.

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

#include "inferior.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "symtab.h"
#include "objfiles.h"
#include "regcache.h"

#include <dlfcn.h>

#include "gdb_assert.h"
#include "gdb_string.h"

#include "libunwind-frame.h"

#include "complaints.h"

static int libunwind_initialized;
static struct gdbarch_data *libunwind_descr_handle;

/* Required function pointers from libunwind.  */
static int (*unw_get_reg_p) (unw_cursor_t *, unw_regnum_t, unw_word_t *);
static int (*unw_get_fpreg_p) (unw_cursor_t *, unw_regnum_t, unw_fpreg_t *);
static int (*unw_get_saveloc_p) (unw_cursor_t *, unw_regnum_t, unw_save_loc_t *);
static int (*unw_is_signal_frame_p) (unw_cursor_t *);
static int (*unw_step_p) (unw_cursor_t *);
static int (*unw_init_remote_p) (unw_cursor_t *, unw_addr_space_t, void *);
static unw_addr_space_t (*unw_create_addr_space_p) (unw_accessors_t *, int);
static void (*unw_destroy_addr_space_p) (unw_addr_space_t);
static int (*unw_search_unwind_table_p) (unw_addr_space_t, unw_word_t, unw_dyn_info_t *,
					 unw_proc_info_t *, int, void *);
static unw_word_t (*unw_find_dyn_list_p) (unw_addr_space_t, unw_dyn_info_t *,
					  void *);


struct libunwind_frame_cache
{
  CORE_ADDR base;
  CORE_ADDR func_addr;
  unw_cursor_t cursor;
  unw_addr_space_t as;
};

/* We need to qualify the function names with a platform-specific prefix to match 
   the names used by the libunwind library.  The UNW_OBJ macro is provided by the
   libunwind.h header file.  */
#define STRINGIFY2(name)	#name
#define STRINGIFY(name)		STRINGIFY2(name)

#ifndef LIBUNWIND_SO
/* Use the stable ABI major version number.  `libunwind-ia64.so' is a link time
   only library, not a runtime one.  */
#define LIBUNWIND_SO "libunwind-" STRINGIFY(UNW_TARGET) ".so.7"
#endif

static char *get_reg_name = STRINGIFY(UNW_OBJ(get_reg));
static char *get_fpreg_name = STRINGIFY(UNW_OBJ(get_fpreg));
static char *get_saveloc_name = STRINGIFY(UNW_OBJ(get_save_loc));
static char *is_signal_frame_name = STRINGIFY(UNW_OBJ(is_signal_frame));
static char *step_name = STRINGIFY(UNW_OBJ(step));
static char *init_remote_name = STRINGIFY(UNW_OBJ(init_remote));
static char *create_addr_space_name = STRINGIFY(UNW_OBJ(create_addr_space));
static char *destroy_addr_space_name = STRINGIFY(UNW_OBJ(destroy_addr_space));
static char *search_unwind_table_name = STRINGIFY(UNW_OBJ(search_unwind_table));
static char *find_dyn_list_name = STRINGIFY(UNW_OBJ(find_dyn_list));

static struct libunwind_descr *
libunwind_descr (struct gdbarch *gdbarch)
{
  return gdbarch_data (gdbarch, libunwind_descr_handle);
}

static void *
libunwind_descr_init (struct gdbarch *gdbarch)
{
  struct libunwind_descr *descr = GDBARCH_OBSTACK_ZALLOC (gdbarch,
							  struct libunwind_descr);
  return descr;
}

void
libunwind_frame_set_descr (struct gdbarch *gdbarch, struct libunwind_descr *descr)
{
  struct libunwind_descr *arch_descr;

  gdb_assert (gdbarch != NULL);

  arch_descr = gdbarch_data (gdbarch, libunwind_descr_handle);

  if (arch_descr == NULL)
    {
      /* First time here.  Must initialize data area.  */
      arch_descr = libunwind_descr_init (gdbarch);
      deprecated_set_gdbarch_data (gdbarch, libunwind_descr_handle, arch_descr);
    }

  /* Copy new descriptor info into arch descriptor.  */
  arch_descr->gdb2uw = descr->gdb2uw;
  arch_descr->uw2gdb = descr->uw2gdb;
  arch_descr->is_fpreg = descr->is_fpreg;
  arch_descr->accessors = descr->accessors;
  arch_descr->special_accessors = descr->special_accessors;
}

static struct libunwind_frame_cache *
libunwind_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  unw_accessors_t *acc;
  unw_addr_space_t as;
  unw_word_t fp;
  unw_regnum_t uw_sp_regnum;
  struct libunwind_frame_cache *cache;
  struct libunwind_descr *descr;
  int i, ret;

  if (*this_cache)
    return *this_cache;

  /* Allocate a new cache.  */
  cache = FRAME_OBSTACK_ZALLOC (struct libunwind_frame_cache);

  /* We can assume we are unwinding a normal frame.  Even if this is
     for a signal trampoline, ia64 signal "trampolines" use a normal
     subroutine call to start the signal handler.  */
  cache->func_addr = frame_func_unwind (next_frame, NORMAL_FRAME);
  if (cache->func_addr == 0
      && frame_relative_level (next_frame) > 0
      && get_frame_type (next_frame) != SIGTRAMP_FRAME)
    return NULL;

  /* Get a libunwind cursor to the previous frame.  We do this by initializing
     a cursor.  Libunwind treats a new cursor as the top of stack and will get
     the current register set via the libunwind register accessor.  Now, we
     provide the platform-specific accessors and we set up the register accessor to use
     the frame register unwinding interfaces so that we properly get the registers for
     the current frame rather than the top.  We then use the  unw_step function to 
     move the libunwind cursor back one frame.  We can later use this cursor to find previous 
     registers via the unw_get_reg interface which will invoke libunwind's special logic.  */
  descr = libunwind_descr (get_frame_arch (next_frame));
  acc = descr->accessors;
  as =  unw_create_addr_space_p (acc,
				 gdbarch_byte_order (current_gdbarch)
				 == BFD_ENDIAN_BIG
				 ? __BIG_ENDIAN
				 : __LITTLE_ENDIAN);

  unw_init_remote_p (&cache->cursor, as, next_frame);
  if (unw_step_p (&cache->cursor) < 0)
    {
      unw_destroy_addr_space_p (as);
      return NULL;
    }

  /* To get base address, get sp from previous frame.  */
  uw_sp_regnum = descr->gdb2uw (gdbarch_sp_regnum (current_gdbarch));
  ret = unw_get_reg_p (&cache->cursor, uw_sp_regnum, &fp);
  if (ret < 0)
    {
      unw_destroy_addr_space_p (as);
      error (_("Can't get libunwind sp register."));
    }

  cache->base = (CORE_ADDR)fp;
  cache->as = as;

  *this_cache = cache;
  return cache;
}

void
libunwind_frame_dealloc_cache (struct frame_info *self, void *this_cache)
{
  struct libunwind_frame_cache *cache = this_cache;
  if (cache->as)
    unw_destroy_addr_space_p (cache->as);
}

unw_word_t
libunwind_find_dyn_list (unw_addr_space_t as, unw_dyn_info_t *di, void *arg)
{
  return unw_find_dyn_list_p (as, di, arg);
}

static const struct frame_unwind libunwind_frame_unwind =
{
  NORMAL_FRAME,
  libunwind_frame_this_id,
  libunwind_frame_prev_register,
  NULL,
  NULL,
  NULL,
  libunwind_frame_dealloc_cache
};

/* Verify if there is sufficient libunwind information for the frame to use
   libunwind frame unwinding.  */
const struct frame_unwind *
libunwind_frame_sniffer (struct frame_info *next_frame)
{
  unw_cursor_t cursor;
  unw_accessors_t *acc;
  unw_addr_space_t as;
  struct libunwind_descr *descr;
  int i, ret;

  /* To test for libunwind unwind support, initialize a cursor to the current frame and try to back
     up.  We use this same method when setting up the frame cache (see libunwind_frame_cache()).
     If libunwind returns success for this operation, it means that it has found sufficient
     libunwind unwinding information to do so.  */

  descr = libunwind_descr (get_frame_arch (next_frame));
  acc = descr->accessors;
  as =  unw_create_addr_space_p (acc,
				 gdbarch_byte_order (current_gdbarch)
				 == BFD_ENDIAN_BIG
				 ? __BIG_ENDIAN
				 : __LITTLE_ENDIAN);

  ret = unw_init_remote_p (&cursor, as, next_frame);

  if (ret < 0)
    {
      unw_destroy_addr_space_p (as);
      return NULL;
    }

 
  /* Check to see if we have libunwind info by checking if we are in a 
     signal frame.  If it doesn't return an error, we have libunwind info
     and can use libunwind.  */
  ret = unw_is_signal_frame_p (&cursor);
  unw_destroy_addr_space_p (as);

  if (ret < 0)
    return NULL;

  return &libunwind_frame_unwind;
}

void
libunwind_frame_this_id (struct frame_info *next_frame, void **this_cache,
		      struct frame_id *this_id)
{
  struct libunwind_frame_cache *cache =
    libunwind_frame_cache (next_frame, this_cache);

  if (cache != NULL)
    (*this_id) = frame_id_build (cache->base, cache->func_addr);
  else
    (*this_id) = null_frame_id;
}

void
libunwind_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			       int regnum, int *optimizedp,
			       enum lval_type *lvalp, CORE_ADDR *addrp,
			       int *realnump, gdb_byte *valuep)
{
  struct libunwind_frame_cache *cache =
    libunwind_frame_cache (next_frame, this_cache);

  void *ptr;
  unw_cursor_t *c;
  unw_save_loc_t sl;
  int i, ret;
  unw_word_t intval;
  unw_fpreg_t fpval;
  unw_regnum_t uw_regnum;
  struct libunwind_descr *descr;

  if (cache == NULL)
    return;
  
  /* Convert from gdb register number to libunwind register number.  */
  descr = libunwind_descr (get_frame_arch (next_frame));
  uw_regnum = descr->gdb2uw (regnum);

  gdb_assert (regnum >= 0);

  if (!target_has_registers)
    error (_("No registers."));

  *optimizedp = 0;
  *addrp = 0;
  *lvalp = not_lval;
  *realnump = -1;

  if (valuep)
    memset (valuep, 0, register_size (current_gdbarch, regnum));

  if (uw_regnum < 0)
    return;

  /* To get the previous register, we use the libunwind register APIs with
     the cursor we have already pushed back to the previous frame.  */

  if (descr->is_fpreg (uw_regnum))
    {
      ret = unw_get_fpreg_p (&cache->cursor, uw_regnum, &fpval);
      ptr = &fpval;
    }
  else
    {
      ret = unw_get_reg_p (&cache->cursor, uw_regnum, &intval);
      ptr = &intval;
    }

  if (ret < 0)
    return;

  if (valuep)
    memcpy (valuep, ptr, register_size (current_gdbarch, regnum));

  if (unw_get_saveloc_p (&cache->cursor, uw_regnum, &sl) < 0)
    return;

  switch (sl.type)
    {
    case UNW_SLT_NONE:
      *optimizedp = 1;
      break;

    case UNW_SLT_MEMORY:
      *lvalp = lval_memory;
      *addrp = sl.u.addr;
      break;

    case UNW_SLT_REG:
      *lvalp = lval_register;
      *realnump = regnum;
      break;
    }
} 

CORE_ADDR
libunwind_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct libunwind_frame_cache *cache =
    libunwind_frame_cache (next_frame, this_cache);

  if (cache == NULL)
    return (CORE_ADDR)NULL;
  return cache->base;
}

/* The following is a glue routine to call the libunwind unwind table
   search function to get unwind information for a specified ip address.  */ 
int
libunwind_search_unwind_table (void *as, long ip, void *di,
			       void *pi, int need_unwind_info, void *args)
{
  return unw_search_unwind_table_p (*(unw_addr_space_t *)as, (unw_word_t )ip, 
				    di, pi, need_unwind_info, args);
}

/* Verify if we are in a sigtramp frame and we can use libunwind to unwind.  */
const struct frame_unwind *
libunwind_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  unw_cursor_t cursor;
  unw_accessors_t *acc;
  unw_addr_space_t as;
  struct libunwind_descr *descr;
  int i, ret;

  /* To test for libunwind unwind support, initialize a cursor to the
     current frame and try to back up.  We use this same method when
     setting up the frame cache (see libunwind_frame_cache()).  If
     libunwind returns success for this operation, it means that it
     has found sufficient libunwind unwinding information to do
     so.  */

  descr = libunwind_descr (get_frame_arch (next_frame));
  acc = descr->accessors;
  as =  unw_create_addr_space_p (acc,
				 gdbarch_byte_order (current_gdbarch)
				 == BFD_ENDIAN_BIG
				 ? __BIG_ENDIAN
				 : __LITTLE_ENDIAN);

  ret = unw_init_remote_p (&cursor, as, next_frame);

  if (ret < 0)
    {
      unw_destroy_addr_space_p (as);
      return NULL;
    }

  /* Check to see if we are in a signal frame.  */
  ret = unw_is_signal_frame_p (&cursor);
  unw_destroy_addr_space_p (as);
  if (ret > 0)
    return &libunwind_frame_unwind;

  return NULL;
}

/* The following routine is for accessing special registers of the top frame.
   A special set of accessors must be given that work without frame info.
   This is used by ia64 to access the rse registers r32-r127.  While they
   are usually located at BOF, this is not always true and only the libunwind
   info can decipher where they actually are.  */
int
libunwind_get_reg_special (struct gdbarch *gdbarch, struct regcache *regcache,
			   int regnum, void *buf)
{
  unw_cursor_t cursor;
  unw_accessors_t *acc;
  unw_addr_space_t as;
  struct libunwind_descr *descr;
  int ret;
  unw_regnum_t uw_regnum;
  unw_word_t intval;
  unw_fpreg_t fpval;
  void *ptr;


  descr = libunwind_descr (gdbarch);
  acc = descr->special_accessors;
  as =  unw_create_addr_space_p (acc,
				 gdbarch_byte_order (current_gdbarch)
				 == BFD_ENDIAN_BIG
				 ? __BIG_ENDIAN
				 : __LITTLE_ENDIAN);

  ret = unw_init_remote_p (&cursor, as, regcache);
  if (ret < 0)
    {
      unw_destroy_addr_space_p (as);
      return -1;
    }

  uw_regnum = descr->gdb2uw (regnum);

  if (descr->is_fpreg (uw_regnum))
    {
      ret = unw_get_fpreg_p (&cursor, uw_regnum, &fpval);
      ptr = &fpval;
    }
  else
    {
      ret = unw_get_reg_p (&cursor, uw_regnum, &intval);
      ptr = &intval;
    }

  unw_destroy_addr_space_p (as);

  if (ret < 0)
    return -1;

  if (buf)
    memcpy (buf, ptr, register_size (current_gdbarch, regnum));

  return 0;
}
  
static int
libunwind_load (void)
{
  void *handle;

  handle = dlopen (LIBUNWIND_SO, RTLD_NOW);
  if (handle == NULL)
    return 0;

  /* Initialize pointers to the dynamic library functions we will use.  */

  unw_get_reg_p = dlsym (handle, get_reg_name);
  if (unw_get_reg_p == NULL)
    return 0;

  unw_get_fpreg_p = dlsym (handle, get_fpreg_name);
  if (unw_get_fpreg_p == NULL)
    return 0;

  unw_get_saveloc_p = dlsym (handle, get_saveloc_name);
  if (unw_get_saveloc_p == NULL)
    return 0;

  unw_is_signal_frame_p = dlsym (handle, is_signal_frame_name);
  if (unw_is_signal_frame_p == NULL)
    return 0;

  unw_step_p = dlsym (handle, step_name);
  if (unw_step_p == NULL)
    return 0;

  unw_init_remote_p = dlsym (handle, init_remote_name);
  if (unw_init_remote_p == NULL)
    return 0;

  unw_create_addr_space_p = dlsym (handle, create_addr_space_name);
  if (unw_create_addr_space_p == NULL)
    return 0;

  unw_destroy_addr_space_p = dlsym (handle, destroy_addr_space_name);
  if (unw_destroy_addr_space_p == NULL)
    return 0;

  unw_search_unwind_table_p = dlsym (handle, search_unwind_table_name);
  if (unw_search_unwind_table_p == NULL)
    return 0;

  unw_find_dyn_list_p = dlsym (handle, find_dyn_list_name);
  if (unw_find_dyn_list_p == NULL)
    return 0;
   
  return 1;
}

int
libunwind_is_initialized (void)
{
  return libunwind_initialized;
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_libunwind_frame (void);

void
_initialize_libunwind_frame (void)
{
  libunwind_descr_handle = gdbarch_data_register_post_init (libunwind_descr_init);

  libunwind_initialized = libunwind_load ();
}
