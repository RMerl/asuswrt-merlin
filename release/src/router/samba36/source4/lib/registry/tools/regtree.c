/*
   Unix SMB/CIFS implementation.
   simple registry frontend

   Copyright (C) Jelmer Vernooij 2004-2007

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
#include "lib/registry/registry.h"
#include "lib/registry/tools/common.h"
#include "lib/events/events.h"
#include "lib/cmdline/popt_common.h"
#include "param/param.h"

/**
 * Print a registry key recursively
 *
 * @param level Level at which to print
 * @param p Key to print
 * @param fullpath Whether the full pat hshould be printed or just the last bit
 * @param novals Whether values should not be printed
 */
static void print_tree(unsigned int level, struct registry_key *p,
		       const char *name,
		       bool fullpath, bool novals)
{
	struct registry_key *subkey;
	const char *valuename, *keyname;
	uint32_t valuetype;
	DATA_BLOB valuedata;
	struct security_descriptor *sec_desc;
	WERROR error;
	unsigned int i;
	TALLOC_CTX *mem_ctx;

	for(i = 0; i < level; i++) putchar(' '); puts(name);

	mem_ctx = talloc_init("print_tree");
	for (i = 0; W_ERROR_IS_OK(error = reg_key_get_subkey_by_index(mem_ctx,
								      p,
								      i,
								      &keyname,
								      NULL,
								      NULL)); i++) {

	        SMB_ASSERT(strlen(keyname) > 0);
		if (!W_ERROR_IS_OK(reg_open_key(mem_ctx, p, keyname, &subkey)))
		        continue;

		print_tree(level+1, subkey, (fullpath && strlen(name))?
                                               talloc_asprintf(mem_ctx, "%s\\%s",
                                                               name, keyname):
                                               keyname, fullpath, novals);
		talloc_free(subkey);
	}
	talloc_free(mem_ctx);

	if(!W_ERROR_EQUAL(error, WERR_NO_MORE_ITEMS)) {
		DEBUG(0, ("Error occurred while fetching subkeys for '%s': %s\n",
				  name, win_errstr(error)));
	}

	if (!novals) {
		mem_ctx = talloc_init("print_tree");
		for(i = 0; W_ERROR_IS_OK(error = reg_key_get_value_by_index(
			mem_ctx, p, i, &valuename, &valuetype, &valuedata));
			i++) {
			unsigned int j;
			for(j = 0; j < level+1; j++) putchar(' ');
			printf("%s\n",  reg_val_description(mem_ctx,
				valuename, valuetype, valuedata));
		}
		talloc_free(mem_ctx);

		if(!W_ERROR_EQUAL(error, WERR_NO_MORE_ITEMS)) {
			DEBUG(0, ("Error occurred while fetching values for '%s': %s\n",
				name, win_errstr(error)));
		}
	}

	mem_ctx = talloc_init("sec_desc");
	if (NT_STATUS_IS_ERR(reg_get_sec_desc(mem_ctx, p, &sec_desc))) {
		DEBUG(0, ("Error getting security descriptor\n"));
	}
	talloc_free(mem_ctx);
}

int main(int argc, char **argv)
{
	int opt;
	unsigned int i;
	const char *file = NULL;
	const char *remote = NULL;
	poptContext pc;
	struct registry_context *h = NULL;
	struct registry_key *start_key = NULL;
	struct tevent_context *ev_ctx;
	WERROR error;
	bool fullpath = false, no_values = false;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"file", 'F', POPT_ARG_STRING, &file, 0, "file path", NULL },
		{"remote", 'R', POPT_ARG_STRING, &remote, 0, "connect to specified remote server", NULL },
		{"fullpath", 'f', POPT_ARG_NONE, &fullpath, 0, "show full paths", NULL},
		{"no-values", 'V', POPT_ARG_NONE, &no_values, 0, "don't show values", NULL},
		POPT_COMMON_SAMBA
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	pc = poptGetContext(argv[0], argc, (const char **) argv, long_options,0);

	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	ev_ctx = s4_event_context_init(NULL);

	if (remote != NULL) {
		h = reg_common_open_remote(remote, ev_ctx, cmdline_lp_ctx, cmdline_credentials);
	} else if (file != NULL) {
		start_key = reg_common_open_file(file, ev_ctx, cmdline_lp_ctx, cmdline_credentials);
	} else {
		h = reg_common_open_local(cmdline_credentials, ev_ctx, cmdline_lp_ctx);
	}

	if (h == NULL && start_key == NULL)
		return 1;

	poptFreeContext(pc);

	error = WERR_OK;

	if (start_key != NULL) {
		print_tree(0, start_key, "", fullpath, no_values);
	} else {
		for(i = 0; reg_predefined_keys[i].handle; i++) {
			error = reg_get_predefined_key(h,
						       reg_predefined_keys[i].handle,
						       &start_key);
			if (!W_ERROR_IS_OK(error)) {
				fprintf(stderr, "Skipping %s: %s\n",
					reg_predefined_keys[i].name,
					win_errstr(error));
				continue;
			}
			SMB_ASSERT(start_key != NULL);
			print_tree(0, start_key, reg_predefined_keys[i].name,
				   fullpath, no_values);
		}
	}

	return 0;
}
