/* Module support.
   Copyright (C) 1996, 1997, 2007 Free Software Foundation, Inc.
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

/* This file is intended to be included by sim-base.h.  */

#ifndef SIM_MODULES_H
#define SIM_MODULES_H

/* Modules are addons to the simulator that perform a specific function
   (e.g. tracing, profiling, memory subsystem, etc.).  Some modules are
   builtin, and others are added at configure time.  The intent is to
   provide a uniform framework for all of the pieces that make up the
   simulator.

   TODO: Add facilities for saving/restoring state to/from a file.  */


/* Various function types.  */

typedef SIM_RC (MODULE_INSTALL_FN) (SIM_DESC);
typedef SIM_RC (MODULE_INIT_FN) (SIM_DESC);
typedef SIM_RC (MODULE_RESUME_FN) (SIM_DESC);
typedef SIM_RC (MODULE_SUSPEND_FN) (SIM_DESC);
typedef void   (MODULE_UNINSTALL_FN) (SIM_DESC);
typedef void   (MODULE_INFO_FN) (SIM_DESC, int);


/* Lists of installed handlers.  */

typedef struct module_init_list {
  struct module_init_list *next;
  MODULE_INIT_FN *fn;
} MODULE_INIT_LIST;

typedef struct module_resume_list {
  struct module_resume_list *next;
  MODULE_RESUME_FN *fn;
} MODULE_RESUME_LIST;

typedef struct module_suspend_list {
  struct module_suspend_list *next;
  MODULE_SUSPEND_FN *fn;
} MODULE_SUSPEND_LIST;

typedef struct module_uninstall_list {
  struct module_uninstall_list *next;
  MODULE_UNINSTALL_FN *fn;
} MODULE_UNINSTALL_LIST;

typedef struct module_info_list {
  struct module_info_list *next;
  MODULE_INFO_FN *fn;
} MODULE_INFO_LIST;


/* Functions to register module with various handler lists */

SIM_RC sim_module_install (SIM_DESC);
void sim_module_uninstall (SIM_DESC);
void sim_module_add_init_fn (SIM_DESC sd, MODULE_INIT_FN fn);
void sim_module_add_resume_fn (SIM_DESC sd, MODULE_RESUME_FN fn);
void sim_module_add_suspend_fn (SIM_DESC sd, MODULE_SUSPEND_FN fn);
void sim_module_add_uninstall_fn (SIM_DESC sd, MODULE_UNINSTALL_FN fn);
void sim_module_add_info_fn (SIM_DESC sd, MODULE_INFO_FN fn);


/* Initialize installed modules before argument processing.
   Called by sim_open.  */
SIM_RC sim_pre_argv_init (SIM_DESC sd, const char *myname);

/* Initialize installed modules after argument processing.
   Called by sim_open.  */
SIM_RC sim_post_argv_init (SIM_DESC sd);

/* Re-initialize the module.  Called by sim_create_inferior. */
SIM_RC sim_module_init (SIM_DESC sd);

/* Suspend/resume modules.  Called by sim_run or sim_resume */
SIM_RC sim_module_suspend (SIM_DESC sd);
SIM_RC sim_module_resume (SIM_DESC sd);

/* Report general information on module */
void sim_module_info (SIM_DESC sd, int verbose);


/* Module private data */

struct module_list {

  /* List of installed module `init' handlers */
  MODULE_INIT_LIST *init_list;

  /* List of installed module `uninstall' handlers.  */
  MODULE_UNINSTALL_LIST *uninstall_list;

  /* List of installed module `resume' handlers.  */
  MODULE_RESUME_LIST *resume_list;

  /* List of installed module `suspend' handlers.  */
  MODULE_SUSPEND_LIST *suspend_list;

  /* List of installed module `info' handlers.  */
  MODULE_INFO_LIST *info_list;

};


#endif /* SIM_MODULES_H */
