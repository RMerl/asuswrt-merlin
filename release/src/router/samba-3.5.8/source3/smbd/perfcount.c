/*
   Unix SMB/Netbios implementation.
   Perfcounter initialization and support functions
   Copyright (C) Todd Stecher 2009

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

*/

#include "includes.h"

struct smb_perfcount_handlers *g_smb_perfcount_handlers = NULL;

struct smb_perfcount_module {
	char *name;
	struct smb_perfcount_handlers *handlers;
	struct smb_perfcount_module *prev, *next;
};

struct smb_perfcount_module *modules = NULL;


/*
 * a module is registered before it is actually loaded - keep a list
 *
 * @todo - currently perfcount modules are globally configured, so
 * building the list is not strictly required.
 * However, its a proven concept in VFS, and is here to allow a
 * move to eventual per-service perfcount configuration.
 *
 * Note many pre-connection statistics are interesting
 * (e.g. before binding to an individual share).
 *
 */
static struct smb_perfcount_module *smb_perfcount_find_module(const char *name)
{
	struct smb_perfcount_module *entry = modules;

	while (entry) {
		if (strcmp(entry->name, name)==0)
			return entry;

		entry = entry->next;
	}

	return NULL;
}
NTSTATUS smb_register_perfcounter(int interface_version, const char *name,
				  const struct smb_perfcount_handlers *handlers)
{
	struct smb_perfcount_module *entry = modules;

	if ((interface_version != SMB_PERFCOUNTER_INTERFACE_VERSION)) {
		DEBUG(0, ("Failed to register perfcount module.\n"
		          "The module was compiled against "
			  "SMB_PERFCOUNTER_INTERFACE_VERSION %d,\n"
		          "current SMB_PERFCOUNTER_INTERFACE_VERSION is %d.\n"
		          "Please recompile against the current Samba Version!\n",
			  interface_version, SMB_PERFCOUNTER_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (!name || !name[0] || !handlers) {
		DEBUG(0,("smb_register_perfcounter() called with NULL pointer "
			"or empty name!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (smb_perfcount_find_module(name)) {
		DEBUG(3,("Perfcount Module %s already loaded!\n", name));
		return NT_STATUS_OK;
	}

	entry = SMB_XMALLOC_P(struct smb_perfcount_module);
	entry->name = smb_xstrdup(name);
	entry->handlers = (struct smb_perfcount_handlers*) handlers;

	DLIST_ADD(modules, entry);
	DEBUG(3, ("Successfully added perfcounter module '%s'\n", name));
	return NT_STATUS_OK;
}

/****************************************************************************
  initialise smb perf counters
 ****************************************************************************/
static bool smb_load_perfcount_module(const char *name)
{
	char *module_path = NULL;
	char *module_name = NULL;
	char *module_param = NULL, *p;

	const struct smb_perfcount_module *entry;

	DEBUG(3, ("Initialising perfcounter module [%s]\n", name));

	if (g_smb_perfcount_handlers) {
		DEBUG(3,("Only 1 perfcount handler may be registered in "
			"smb.conf\n"));
		return true;
	}

	module_path = smb_xstrdup(name);

	p = strchr_m(module_path, ':');

	if (p) {
		*p = 0;
		module_param = p+1;
		trim_char(module_param, ' ', ' ');
	}

	trim_char(module_path, ' ', ' ');

	module_name = smb_xstrdup(module_path);

	if (module_name[0] == '/') {

		/*
		 * Extract the module name from the path. Just use the base
		 * name of the last path component.
		 */

		SAFE_FREE(module_name);
		module_name = smb_xstrdup(strrchr_m(module_path, '/')+1);

		p = strchr_m(module_name, '.');

		if (p != NULL) {
			*p = '\0';
		}
	}

	/* load the perfcounter module */
	if((entry = smb_perfcount_find_module(module_name)) ||
	   (NT_STATUS_IS_OK(smb_probe_module("perfcount", module_path)) &&
		(entry = smb_perfcount_find_module(module_name)))) {

		DEBUG(3,("Successfully loaded perfcounter module [%s] \n", name));
	} else {
		DEBUG(0,("Can't find a perfcounter module [%s]\n",name));
		goto fail;
	}

	g_smb_perfcount_handlers = entry->handlers;

	SAFE_FREE(module_path);
	SAFE_FREE(module_name);
	return True;

 fail:
	SAFE_FREE(module_path);
	SAFE_FREE(module_name);
	return False;
}

void smb_init_perfcount_data(struct smb_perfcount_data *pcd)
{

	ZERO_STRUCTP(pcd);
	pcd->handlers = g_smb_perfcount_handlers;
}

bool smb_perfcount_init(void)
{
	char *perfcount_object;

	perfcount_object = lp_perfcount_module();

	/* don't init */
	if (!perfcount_object || !perfcount_object[0])
		return True;

	if (!smb_load_perfcount_module(perfcount_object)) {
		DEBUG(0, ("smbd_load_percount_module failed for %s\n",
			perfcount_object));
		return False;
	}


	return True;
}
