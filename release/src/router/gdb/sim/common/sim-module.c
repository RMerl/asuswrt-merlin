/* Module support.

   Copyright 1996, 1997, 1998, 2003, 2007 Free Software Foundation, Inc.

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

#include "sim-main.h"
#include "sim-io.h"
#include "sim-options.h"
#include "sim-assert.h"

#if WITH_HW
#include "sim-hw.h"
#endif

#include "libiberty.h"

/* List of all modules.  */
static MODULE_INSTALL_FN * const modules[] = {
  standard_install,
  sim_events_install,
#ifdef SIM_HAVE_MODEL
  sim_model_install,
#endif
#if WITH_ENGINE
  sim_engine_install,
#endif
#if WITH_TRACE
  trace_install,
#endif
#if WITH_PROFILE
  profile_install,
#endif
  sim_core_install,
#ifndef SIM_HAVE_FLATMEM
  /* FIXME: should handle flatmem as well FLATMEM */
  sim_memopt_install,
#endif
#if WITH_WATCHPOINTS
  sim_watchpoint_install,
#endif
#if WITH_SCACHE
  scache_install,
#endif
#if WITH_HW
  sim_hw_install,
#endif
  /* Configured in [simulator specific] additional modules.  */
#ifdef MODULE_LIST
  MODULE_LIST
#endif
  0
};

/* Functions called from sim_open.  */

/* Initialize common parts before argument processing.  */

SIM_RC
sim_pre_argv_init (SIM_DESC sd, const char *myname)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) == NULL);

  STATE_MY_NAME (sd) = myname + strlen (myname);
  while (STATE_MY_NAME (sd) > myname && STATE_MY_NAME (sd)[-1] != '/')
    --STATE_MY_NAME (sd);

  /* Set the cpu names to default values.  */
  {
    int i;
    for (i = 0; i < MAX_NR_PROCESSORS; ++i)
      {
	char *name;
	asprintf (&name, "cpu%d", i);
	CPU_NAME (STATE_CPU (sd, i)) = name;
      }
  }

  sim_config_default (sd);

  /* Install all configured in modules.  */
  if (sim_module_install (sd) != SIM_RC_OK)
    return SIM_RC_FAIL;

  return SIM_RC_OK;
}

/* Initialize common parts after argument processing.  */

SIM_RC
sim_post_argv_init (SIM_DESC sd)
{
  int i;
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  /* Set the cpu->state backlinks for each cpu.  */
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      CPU_STATE (STATE_CPU (sd, i)) = sd;
      CPU_INDEX (STATE_CPU (sd, i)) = i;
    }

  if (sim_module_init (sd) != SIM_RC_OK)
    return SIM_RC_FAIL;

  return SIM_RC_OK;
}

/* Install all modules.
   If this fails, no modules are left installed.  */

SIM_RC
sim_module_install (SIM_DESC sd)
{
  MODULE_INSTALL_FN * const *modp;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) == NULL);

  STATE_MODULES (sd) = ZALLOC (struct module_list);
  for (modp = modules; *modp != NULL; ++modp)
    {
      if ((*modp) (sd) != SIM_RC_OK)
	{
	  sim_module_uninstall (sd);
	  SIM_ASSERT (STATE_MODULES (sd) == NULL);
	  return SIM_RC_FAIL;
	}
    }
  return SIM_RC_OK;
}

/* Called after all modules have been installed and after argv
   has been processed.  */

SIM_RC
sim_module_init (SIM_DESC sd)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_INIT_LIST *modp;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  for (modp = modules->init_list; modp != NULL; modp = modp->next)
    {
      if ((*modp->fn) (sd) != SIM_RC_OK)
	return SIM_RC_FAIL;
    }
  return SIM_RC_OK;
}

/* Called when ever the simulator is resumed */

SIM_RC
sim_module_resume (SIM_DESC sd)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_RESUME_LIST *modp;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  for (modp = modules->resume_list; modp != NULL; modp = modp->next)
    {
      if ((*modp->fn) (sd) != SIM_RC_OK)
	return SIM_RC_FAIL;
    }
  return SIM_RC_OK;
}

/* Called when ever the simulator is suspended */

SIM_RC
sim_module_suspend (SIM_DESC sd)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_SUSPEND_LIST *modp;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  for (modp = modules->suspend_list; modp != NULL; modp = modp->next)
    {
      if ((*modp->fn) (sd) != SIM_RC_OK)
	return SIM_RC_FAIL;
    }
  return SIM_RC_OK;
}

/* Uninstall installed modules, called by sim_close.  */

void
sim_module_uninstall (SIM_DESC sd)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_UNINSTALL_LIST *modp;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  /* Uninstall the modules.  */
  for (modp = modules->uninstall_list; modp != NULL; modp = modp->next)
    (*modp->fn) (sd);

  /* clean-up init list */
  {
    MODULE_INIT_LIST *n, *d;
    for (d = modules->init_list; d != NULL; d = n)
      {
	n = d->next;
	zfree (d);
      }
  }

  /* clean-up resume list */
  {
    MODULE_RESUME_LIST *n, *d;
    for (d = modules->resume_list; d != NULL; d = n)
      {
	n = d->next;
	zfree (d);
      }
  }

  /* clean-up suspend list */
  {
    MODULE_SUSPEND_LIST *n, *d;
    for (d = modules->suspend_list; d != NULL; d = n)
      {
	n = d->next;
	zfree (d);
      }
  }

  /* clean-up uninstall list */
  {
    MODULE_UNINSTALL_LIST *n, *d;
    for (d = modules->uninstall_list; d != NULL; d = n)
      {
	n = d->next;
	zfree (d);
      }
  }

  /* clean-up info list */
  {
    MODULE_INFO_LIST *n, *d;
    for (d = modules->info_list; d != NULL; d = n)
      {
	n = d->next;
	zfree (d);
      }
  }

  zfree (modules);
  STATE_MODULES (sd) = NULL;
}

/* Called when ever simulator info is needed */

void
sim_module_info (SIM_DESC sd, int verbose)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_INFO_LIST *modp;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  for (modp = modules->info_list; modp != NULL; modp = modp->next)
    {
      (*modp->fn) (sd, verbose);
    }
}

/* Add FN to the init handler list.
   init in the same order as the install. */

void
sim_module_add_init_fn (SIM_DESC sd, MODULE_INIT_FN fn)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_INIT_LIST *l = ZALLOC (MODULE_INIT_LIST);
  MODULE_INIT_LIST **last;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  last = &modules->init_list;
  while (*last != NULL)
    last = &((*last)->next);

  l->fn = fn;
  l->next = NULL;
  *last = l;
}

/* Add FN to the resume handler list.
   resume in the same order as the install. */

void
sim_module_add_resume_fn (SIM_DESC sd, MODULE_RESUME_FN fn)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_RESUME_LIST *l = ZALLOC (MODULE_RESUME_LIST);
  MODULE_RESUME_LIST **last;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  last = &modules->resume_list;
  while (*last != NULL)
    last = &((*last)->next);

  l->fn = fn;
  l->next = NULL;
  *last = l;
}

/* Add FN to the init handler list.
   suspend in the reverse order to install. */

void
sim_module_add_suspend_fn (SIM_DESC sd, MODULE_SUSPEND_FN fn)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_SUSPEND_LIST *l = ZALLOC (MODULE_SUSPEND_LIST);
  MODULE_SUSPEND_LIST **last;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  last = &modules->suspend_list;
  while (*last != NULL)
    last = &((*last)->next);

  l->fn = fn;
  l->next = modules->suspend_list;
  modules->suspend_list = l;
}

/* Add FN to the uninstall handler list.
   Uninstall in reverse order to install.  */

void
sim_module_add_uninstall_fn (SIM_DESC sd, MODULE_UNINSTALL_FN fn)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_UNINSTALL_LIST *l = ZALLOC (MODULE_UNINSTALL_LIST);

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  l->fn = fn;
  l->next = modules->uninstall_list;
  modules->uninstall_list = l;
}

/* Add FN to the info handler list.
   Report info in the same order as the install. */

void
sim_module_add_info_fn (SIM_DESC sd, MODULE_INFO_FN fn)
{
  struct module_list *modules = STATE_MODULES (sd);
  MODULE_INFO_LIST *l = ZALLOC (MODULE_INFO_LIST);
  MODULE_INFO_LIST **last;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (STATE_MODULES (sd) != NULL);

  last = &modules->info_list;
  while (*last != NULL)
    last = &((*last)->next);

  l->fn = fn;
  l->next = NULL;
  *last = l;
}
