/* User visible, per-frame registers, for GDB, the GNU debugger.

   Copyright (C) 2002, 2003, 2004, 2007 Free Software Foundation, Inc.

   Contributed by Red Hat.

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
#include "user-regs.h"
#include "gdbtypes.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "frame.h"

/* A table of user registers.

   User registers have regnum's that live above of the range [0
   .. gdbarch_num_regs + gdbarch_num_pseudo_regs)
   (which is controlled by the target).
   The target should never see a user register's regnum value.

   Always append, never delete.  By doing this, the relative regnum
   (offset from gdbarch_num_regs + gdbarch_num_pseudo_regs)
    assigned to each user  register never changes.  */

struct user_reg
{
  const char *name;
  struct value *(*read) (struct frame_info * frame, const void *baton);
  const void *baton;
  struct user_reg *next;
};

/* This structure is named gdb_user_regs instead of user_regs to avoid
   conflicts with any "struct user_regs" in system headers.  For instance,
   on ARM GNU/Linux native builds, nm-linux.h includes <signal.h> includes
   <sys/ucontext.h> includes <sys/procfs.h> includes <sys/user.h>, which
   declares "struct user_regs".  */

struct gdb_user_regs
{
  struct user_reg *first;
  struct user_reg **last;
};

static void
append_user_reg (struct gdb_user_regs *regs, const char *name,
		 user_reg_read_ftype *read, const void *baton,
		 struct user_reg *reg)
{
  /* The caller is responsible for allocating memory needed to store
     the register.  By doing this, the function can operate on a
     register list stored in the common heap or a specific obstack.  */
  gdb_assert (reg != NULL);
  reg->name = name;
  reg->read = read;
  reg->baton = baton;
  reg->next = NULL;
  (*regs->last) = reg;
  regs->last = &(*regs->last)->next;
}

/* An array of the builtin user registers.  */

static struct gdb_user_regs builtin_user_regs = { NULL, &builtin_user_regs.first };

void
user_reg_add_builtin (const char *name, user_reg_read_ftype *read,
		      const void *baton)
{
  append_user_reg (&builtin_user_regs, name, read, baton,
		   XMALLOC (struct user_reg));
}

/* Per-architecture user registers.  Start with the builtin user
   registers and then, again, append.  */

static struct gdbarch_data *user_regs_data;

static void *
user_regs_init (struct gdbarch *gdbarch)
{
  struct user_reg *reg;
  struct gdb_user_regs *regs = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct gdb_user_regs);
  regs->last = &regs->first;
  for (reg = builtin_user_regs.first; reg != NULL; reg = reg->next)
    append_user_reg (regs, reg->name, reg->read, reg->baton,
		     GDBARCH_OBSTACK_ZALLOC (gdbarch, struct user_reg));
  return regs;
}

void
user_reg_add (struct gdbarch *gdbarch, const char *name,
	      user_reg_read_ftype *read, const void *baton)
{
  struct gdb_user_regs *regs = gdbarch_data (gdbarch, user_regs_data);
  if (regs == NULL)
    {
      /* ULGH, called during architecture initialization.  Patch
         things up.  */
      regs = user_regs_init (gdbarch);
      deprecated_set_gdbarch_data (gdbarch, user_regs_data, regs);
    }
  append_user_reg (regs, name, read, baton,
		   GDBARCH_OBSTACK_ZALLOC (gdbarch, struct user_reg));
}

int
user_reg_map_name_to_regnum (struct gdbarch *gdbarch, const char *name,
			     int len)
{
  /* Make life easy, set the len to something reasonable.  */
  if (len < 0)
    len = strlen (name);

  /* Search register name space first - always let an architecture
     specific register override the user registers.  */
  {
    int i;
    int maxregs = (gdbarch_num_regs (gdbarch)
		   + gdbarch_num_pseudo_regs (gdbarch));
    for (i = 0; i < maxregs; i++)
      {
	const char *regname = gdbarch_register_name (gdbarch, i);
	if (regname != NULL && len == strlen (regname)
	    && strncmp (regname, name, len) == 0)
	  {
	    return i;
	  }
      }
  }

  /* Search the user name space.  */
  {
    struct gdb_user_regs *regs = gdbarch_data (gdbarch, user_regs_data);
    struct user_reg *reg;
    int nr;
    for (nr = 0, reg = regs->first; reg != NULL; reg = reg->next, nr++)
      {
	if ((len < 0 && strcmp (reg->name, name))
	    || (len == strlen (reg->name)
		&& strncmp (reg->name, name, len) == 0))
	  return gdbarch_num_regs (current_gdbarch)
		 + gdbarch_num_pseudo_regs (current_gdbarch) + nr;
      }
  }

  return -1;
}

static struct user_reg *
usernum_to_user_reg (struct gdbarch *gdbarch, int usernum)
{
  struct gdb_user_regs *regs = gdbarch_data (gdbarch, user_regs_data);
  struct user_reg *reg;
  for (reg = regs->first; reg != NULL; reg = reg->next)
    {
      if (usernum == 0)
	return reg;
      usernum--;
    }
  return NULL;
}

const char *
user_reg_map_regnum_to_name (struct gdbarch *gdbarch, int regnum)
{
  int maxregs = (gdbarch_num_regs (gdbarch)
		 + gdbarch_num_pseudo_regs (gdbarch));
  if (regnum < 0)
    return NULL;
  else if (regnum < maxregs)
    return gdbarch_register_name (gdbarch, regnum);
  else
    {
      struct user_reg *reg = usernum_to_user_reg (gdbarch, regnum - maxregs);
      if (reg == NULL)
	return NULL;
      else
	return reg->name;
    }
}

struct value *
value_of_user_reg (int regnum, struct frame_info *frame)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  int maxregs = (gdbarch_num_regs (gdbarch)
		 + gdbarch_num_pseudo_regs (gdbarch));
  struct user_reg *reg = usernum_to_user_reg (gdbarch, regnum - maxregs);
  gdb_assert (reg != NULL);
  return reg->read (frame, reg->baton);
}

extern initialize_file_ftype _initialize_user_regs; /* -Wmissing-prototypes */

void
_initialize_user_regs (void)
{
  user_regs_data = gdbarch_data_register_post_init (user_regs_init);
}
