/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2002
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) James J Myers 2003
   Copyright (C) Jelmer Vernooij 2005-2007
   
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
#include "dynconfig/dynconfig.h"
#include "system/network.h"
#include "system/filesys.h"
#include "system/dir.h"
#include "param/param.h"

/**
 * @file
 * @brief Misc utility functions
 */


bool lpcfg_is_mydomain(struct loadparm_context *lp_ctx,
			     const char *domain)
{
	return strequal(lpcfg_workgroup(lp_ctx), domain);
}

bool lpcfg_is_my_domain_or_realm(struct loadparm_context *lp_ctx,
			     const char *domain)
{
	return strequal(lpcfg_workgroup(lp_ctx), domain) ||
		strequal(lpcfg_realm(lp_ctx), domain);
}

/**
  see if a string matches either our primary or one of our secondary 
  netbios aliases. do a case insensitive match
*/
bool lpcfg_is_myname(struct loadparm_context *lp_ctx, const char *name)
{
	const char **aliases;
	int i;

	if (strcasecmp(name, lpcfg_netbios_name(lp_ctx)) == 0) {
		return true;
	}

	aliases = lpcfg_netbios_aliases(lp_ctx);
	for (i=0; aliases && aliases[i]; i++) {
		if (strcasecmp(name, aliases[i]) == 0) {
			return true;
		}
	}

	return false;
}


/**
 A useful function for returning a path in the Samba lock directory.
**/
char *lock_path(TALLOC_CTX* mem_ctx, struct loadparm_context *lp_ctx,
			 const char *name)
{
	char *fname, *dname;
	if (name == NULL) {
		return NULL;
	}
	if (name[0] == 0 || name[0] == '/' || strstr(name, ":/")) {
		return talloc_strdup(mem_ctx, name);
	}

	dname = talloc_strdup(mem_ctx, lpcfg_lockdir(lp_ctx));
	trim_string(dname,"","/");
	
	if (!directory_exist(dname)) {
		mkdir(dname,0755);
	}
	
	fname = talloc_asprintf(mem_ctx, "%s/%s", dname, name);

	talloc_free(dname);

	return fname;
}

/**
 * @brief Returns an absolute path to a file in the directory containing the current config file
 *
 * @param name File to find, relative to the config file directory.
 *
 * @retval Pointer to a talloc'ed string containing the full path.
 **/

char *config_path(TALLOC_CTX* mem_ctx, struct loadparm_context *lp_ctx,
			   const char *name)
{
	char *fname, *config_dir, *p;
	config_dir = talloc_strdup(mem_ctx, lpcfg_configfile(lp_ctx));
	if (config_dir == NULL) {
		config_dir = talloc_strdup(mem_ctx, lp_default_path());
	}
	p = strrchr(config_dir, '/');
	if (p == NULL) {
		talloc_free(config_dir);
		config_dir = talloc_strdup(mem_ctx, ".");
		if (config_dir == NULL) {
			return NULL;
		}
	} else {
		p[0] = '\0';
	}
	fname = talloc_asprintf(mem_ctx, "%s/%s", config_dir, name);
	talloc_free(config_dir);
	return fname;
}

/**
 * @brief Returns an absolute path to a file in the Samba private directory.
 *
 * @param name File to find, relative to PRIVATEDIR.
 * if name is not relative, then use it as-is
 *
 * @retval Pointer to a talloc'ed string containing the full path.
 **/
char *private_path(TALLOC_CTX* mem_ctx, 
			    struct loadparm_context *lp_ctx,
			    const char *name)
{
	char *fname;
	if (name == NULL) {
		return NULL;
	}
	if (name[0] == 0 || name[0] == '/' || strstr(name, ":/")) {
		return talloc_strdup(mem_ctx, name);
	}
	fname = talloc_asprintf(mem_ctx, "%s/%s", lpcfg_private_dir(lp_ctx), name);
	return fname;
}

/**
  return a path in the smbd.tmp directory, where all temporary file
  for smbd go. If NULL is passed for name then return the directory 
  path itself
*/
char *smbd_tmp_path(TALLOC_CTX *mem_ctx, 
			     struct loadparm_context *lp_ctx,
			     const char *name)
{
	char *fname, *dname;

	dname = private_path(mem_ctx, lp_ctx, "smbd.tmp");
	if (!directory_exist(dname)) {
		mkdir(dname,0755);
	}

	if (name == NULL) {
		return dname;
	}

	fname = talloc_asprintf(mem_ctx, "%s/%s", dname, name);
	talloc_free(dname);

	return fname;
}

/**
 * Obtain the init function from a shared library file
 */
init_module_fn load_module(TALLOC_CTX *mem_ctx, const char *path)
{
	void *handle;
	void *init_fn;

	handle = dlopen(path, RTLD_NOW);
	if (handle == NULL) {
		DEBUG(0, ("Unable to open %s: %s\n", path, dlerror()));
		return NULL;
	}

	init_fn = dlsym(handle, SAMBA_INIT_MODULE);

	if (init_fn == NULL) {
		DEBUG(0, ("Unable to find %s() in %s: %s\n", 
			  SAMBA_INIT_MODULE, path, dlerror()));
		DEBUG(1, ("Loading module '%s' failed\n", path));
		dlclose(handle);
		return NULL;
	}

	return (init_module_fn)init_fn;
}

/**
 * Obtain list of init functions from the modules in the specified
 * directory
 */
init_module_fn *load_modules(TALLOC_CTX *mem_ctx, const char *path)
{
	DIR *dir;
	struct dirent *entry;
	char *filename;
	int success = 0;
	init_module_fn *ret = talloc_array(mem_ctx, init_module_fn, 2);

	ret[0] = NULL;
	
	dir = opendir(path);
	if (dir == NULL) {
		talloc_free(ret);
		return NULL;
	}

	while((entry = readdir(dir))) {
		if (ISDOT(entry->d_name) || ISDOTDOT(entry->d_name))
			continue;

		filename = talloc_asprintf(mem_ctx, "%s/%s", path, entry->d_name);

		ret[success] = load_module(mem_ctx, filename);
		if (ret[success]) {
			ret = talloc_realloc(mem_ctx, ret, init_module_fn, success+2);
			success++;
			ret[success] = NULL;
		}

		talloc_free(filename);
	}

	closedir(dir);

	return ret;
}

/**
 * Run the specified init functions.
 *
 * @return true if all functions ran successfully, false otherwise
 */
bool run_init_functions(init_module_fn *fns)
{
	int i;
	bool ret = true;
	
	if (fns == NULL)
		return true;
	
	for (i = 0; fns[i]; i++) { ret &= (bool)NT_STATUS_IS_OK(fns[i]()); }

	return ret;
}

static char *modules_path(TALLOC_CTX* mem_ctx, struct loadparm_context *lp_ctx,
			  const char *name)
{
	return talloc_asprintf(mem_ctx, "%s/%s", 
			       lpcfg_modulesdir(lp_ctx),
			       name);
}

/**
 * Load the initialization functions from DSO files for a specific subsystem.
 *
 * Will return an array of function pointers to initialization functions
 */

init_module_fn *load_samba_modules(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, const char *subsystem)
{
	char *path = modules_path(mem_ctx, lp_ctx, subsystem);
	init_module_fn *ret;

	ret = load_modules(mem_ctx, path);

	talloc_free(path);

	return ret;
}

const char *lpcfg_messaging_path(TALLOC_CTX *mem_ctx,
				       struct loadparm_context *lp_ctx)
{
	return smbd_tmp_path(mem_ctx, lp_ctx, "msg");
}

struct smb_iconv_convenience *smb_iconv_convenience_reinit_lp(TALLOC_CTX *mem_ctx,
							      struct loadparm_context *lp_ctx,
							      struct smb_iconv_convenience *old_ic)
{
	return smb_iconv_convenience_reinit(mem_ctx, lpcfg_dos_charset(lp_ctx),
					    lpcfg_unix_charset(lp_ctx),
					    lpcfg_display_charset(lp_ctx),
					    lpcfg_parm_bool(lp_ctx, NULL, "iconv", "native", true),
					    old_ic);
}


const char *lpcfg_sam_name(struct loadparm_context *lp_ctx)
{
	switch (lpcfg_server_role(lp_ctx)) {
	case ROLE_DOMAIN_CONTROLLER:
		return lpcfg_workgroup(lp_ctx);
	default:
		return lpcfg_netbios_name(lp_ctx);
	}
}

