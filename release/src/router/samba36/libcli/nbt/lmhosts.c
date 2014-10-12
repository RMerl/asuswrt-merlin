/*
   Unix SMB/CIFS implementation.

   manipulate nbt name structures

   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Jeremy Allison 2007
   Copyright (C) Andrew Bartlett 2009.

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
#include "lib/util/xfile.h"
#include "lib/util/util_net.h"
#include "system/filesys.h"
#include "system/network.h"
#include "../libcli/nbt/libnbt.h"

/********************************************************
 Start parsing the lmhosts file.
*********************************************************/

XFILE *startlmhosts(const char *fname)
{
	XFILE *fp = x_fopen(fname,O_RDONLY, 0);
	if (!fp) {
		DEBUG(4,("startlmhosts: Can't open lmhosts file %s. "
			"Error was %s\n",
			fname, strerror(errno)));
		return NULL;
	}
	return fp;
}

/********************************************************
 Parse the next line in the lmhosts file.
*********************************************************/

bool getlmhostsent(TALLOC_CTX *ctx, XFILE *fp, char **pp_name, int *name_type,
		struct sockaddr_storage *pss)
{
	char line[1024];

	*pp_name = NULL;

	while(!x_feof(fp) && !x_ferror(fp)) {
		char *ip = NULL;
		char *flags = NULL;
		char *extra = NULL;
		char *name = NULL;
		const char *ptr;
		char *ptr1 = NULL;
		int count = 0;

		*name_type = -1;

		if (!fgets_slash(line,sizeof(line),fp)) {
			continue;
		}

		if (*line == '#') {
			continue;
		}

		ptr = line;

		if (next_token_talloc(ctx, &ptr, &ip, NULL))
			++count;
		if (next_token_talloc(ctx, &ptr, &name, NULL))
			++count;
		if (next_token_talloc(ctx, &ptr, &flags, NULL))
			++count;
		if (next_token_talloc(ctx, &ptr, &extra, NULL))
			++count;

		if (count <= 0)
			continue;

		if (count > 0 && count < 2) {
			DEBUG(0,("getlmhostsent: Ill formed hosts line [%s]\n",
						line));
			continue;
		}

		if (count >= 4) {
			DEBUG(0,("getlmhostsent: too many columns "
				"in lmhosts file (obsolete syntax)\n"));
			continue;
		}

		if (!flags) {
			flags = talloc_strdup(ctx, "");
			if (!flags) {
				continue;
			}
		}

		DEBUG(4, ("getlmhostsent: lmhost entry: %s %s %s\n",
					ip, name, flags));

		if (strchr_m(flags,'G') || strchr_m(flags,'S')) {
			DEBUG(0,("getlmhostsent: group flag "
				"in lmhosts ignored (obsolete)\n"));
			continue;
		}

		if (!interpret_string_addr(pss, ip, AI_NUMERICHOST)) {
			DEBUG(0,("getlmhostsent: invalid address "
				"%s.\n", ip));
		}

		/* Extra feature. If the name ends in '#XX',
		 * where XX is a hex number, then only add that name type. */
		if((ptr1 = strchr_m(name, '#')) != NULL) {
			char *endptr;
      			ptr1++;

			*name_type = (int)strtol(ptr1, &endptr, 16);
			if(!*ptr1 || (endptr == ptr1)) {
				DEBUG(0,("getlmhostsent: invalid name "
					"%s containing '#'.\n", name));
				continue;
			}

			*(--ptr1) = '\0'; /* Truncate at the '#' */
		}

		*pp_name = talloc_strdup(ctx, name);
		if (!*pp_name) {
			return false;
		}
		return true;
	}

	return false;
}

/********************************************************
 Finish parsing the lmhosts file.
*********************************************************/

void endlmhosts(XFILE *fp)
{
	x_fclose(fp);
}

/********************************************************
 Resolve via "lmhosts" method.
*********************************************************/

NTSTATUS resolve_lmhosts_file_as_sockaddr(const char *lmhosts_file, 
					  const char *name, int name_type,
					  TALLOC_CTX *mem_ctx, 
					  struct sockaddr_storage **return_iplist,
					  int *return_count)
{
	/*
	 * "lmhosts" means parse the local lmhosts file.
	 */

	XFILE *fp;
	char *lmhost_name = NULL;
	int name_type2;
	struct sockaddr_storage return_ss;
	NTSTATUS status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	TALLOC_CTX *ctx = NULL;

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(3,("resolve_lmhosts: "
		"Attempting lmhosts lookup for name %s<0x%x>\n",
		name, name_type));

	fp = startlmhosts(lmhosts_file);

	if ( fp == NULL )
		return NT_STATUS_NO_SUCH_FILE;

	ctx = talloc_new(mem_ctx);
	if (!ctx) {
		endlmhosts(fp);
		return NT_STATUS_NO_MEMORY;
	}

	while (getlmhostsent(ctx, fp, &lmhost_name, &name_type2, &return_ss)) {

		if (!strequal(name, lmhost_name)) {
			TALLOC_FREE(lmhost_name);
			continue;
		}

		if ((name_type2 != -1) && (name_type != name_type2)) {
			TALLOC_FREE(lmhost_name);
			continue;
		}
		
		*return_iplist = talloc_realloc(ctx, (*return_iplist), 
						struct sockaddr_storage,
						(*return_count)+1);

		if ((*return_iplist) == NULL) {
			TALLOC_FREE(ctx);
			endlmhosts(fp);
			DEBUG(3,("resolve_lmhosts: talloc_realloc fail !\n"));
			return NT_STATUS_NO_MEMORY;
		}

		(*return_iplist)[*return_count] = return_ss;
		*return_count += 1;

		/* we found something */
		status = NT_STATUS_OK;

		/* Multiple names only for DC lookup */
		if (name_type != 0x1c)
			break;
	}

	talloc_steal(mem_ctx, *return_iplist);
	TALLOC_FREE(ctx);
	endlmhosts(fp);
	return status;
}

