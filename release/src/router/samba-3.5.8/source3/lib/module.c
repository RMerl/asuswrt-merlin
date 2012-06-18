/* 
   Unix SMB/CIFS implementation.
   module loading system

   Copyright (C) Jelmer Vernooij 2002-2003
   Copyright (C) Stefan (metze) Metzmacher 2003
   
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

#ifdef HAVE_DLOPEN

/* Load a dynamic module.  Only log a level 0 error if we are not checking 
   for the existence of a module (probling). */

static NTSTATUS do_smb_load_module(const char *module_name, bool is_probe)
{
	void *handle;
	init_module_function *init;
	NTSTATUS status;
	const char *error;

	/* Always try to use LAZY symbol resolving; if the plugin has 
	 * backwards compatibility, there might be symbols in the 
	 * plugin referencing to old (removed) functions
	 */
	handle = dlopen(module_name, RTLD_LAZY);

	/* This call should reset any possible non-fatal errors that 
	   occured since last call to dl* functions */
	error = dlerror();

	if(!handle) {
		int level = is_probe ? 3 : 0;
		DEBUG(level, ("Error loading module '%s': %s\n", module_name, error ? error : ""));
		return NT_STATUS_UNSUCCESSFUL;
	}

	init = (init_module_function *)dlsym(handle, "init_samba_module");

	/* we must check dlerror() to determine if it worked, because
           dlsym() can validly return NULL */
	error = dlerror();
	if (error) {
		DEBUG(0, ("Error trying to resolve symbol 'init_samba_module' "
			  "in %s: %s\n", module_name, error));
		dlclose(handle);
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(2, ("Module '%s' loaded\n", module_name));

	status = init();
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Module '%s' initialization failed: %s\n",
			    module_name, get_friendly_nt_error_msg(status)));
		dlclose(handle);
	}

	return status;
}

NTSTATUS smb_load_module(const char *module_name)
{
	return do_smb_load_module(module_name, False);
}

/* Load all modules in list and return number of
 * modules that has been successfully loaded */
int smb_load_modules(const char **modules)
{
	int i;
	int success = 0;

	for(i = 0; modules[i]; i++){
		if(NT_STATUS_IS_OK(smb_load_module(modules[i]))) {
			success++;
		}
	}

	DEBUG(2, ("%d modules successfully loaded\n", success));

	return success;
}

NTSTATUS smb_probe_module(const char *subsystem, const char *module)
{
	char *full_path = NULL;
	TALLOC_CTX *ctx = talloc_stackframe();
	NTSTATUS status;

	/* Check for absolute path */

	/* if we make any 'samba multibyte string'
	   calls here, we break
	   for loading string modules */

	DEBUG(5, ("Probing module '%s'\n", module));

	if (module[0] == '/') {
		status = do_smb_load_module(module, True);
		TALLOC_FREE(ctx);
		return status;
	}

	full_path = talloc_asprintf(ctx,
			"%s/%s.%s",
			modules_path(subsystem),
			module,
			shlib_ext());
	if (!full_path) {
		TALLOC_FREE(ctx);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5, ("Probing module '%s': Trying to load from %s\n",
		module, full_path));

	status = do_smb_load_module(full_path, True);

	TALLOC_FREE(ctx);
	return status;
}

#else /* HAVE_DLOPEN */

NTSTATUS smb_load_module(const char *module_name)
{
	DEBUG(0,("This samba executable has not been built with plugin support\n"));
	return NT_STATUS_NOT_SUPPORTED;
}

int smb_load_modules(const char **modules)
{
	DEBUG(0,("This samba executable has not been built with plugin support\n"));
	return -1;
}

NTSTATUS smb_probe_module(const char *subsystem, const char *module)
{
	DEBUG(0,("This samba executable has not been built with plugin support, not probing\n")); 
	return NT_STATUS_NOT_SUPPORTED;
}

#endif /* HAVE_DLOPEN */

void init_modules(void)
{
	/* FIXME: This can cause undefined symbol errors :
	 *  smb_register_vfs() isn't available in nmbd, for example */
	if(lp_preload_modules()) 
		smb_load_modules(lp_preload_modules());
}
