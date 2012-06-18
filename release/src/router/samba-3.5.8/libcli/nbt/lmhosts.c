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

