/*
   Copyright (C) Andrew Tridgell 2009
 
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

#ifdef _SAMBA_BUILD_

#define UID_WRAPPER_NOT_REPLACE
#include "../replace/replace.h"
#include <talloc.h>
#include "system/passwd.h"

#else /* _SAMBA_BUILD_ */

#error uid_wrapper_only_supported_in_samba_yet

#endif

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

/*
  we keep the virtualised euid/egid/groups information here
 */
static struct {
	bool initialised;
	bool enabled;
	uid_t euid;
	gid_t egid;
	unsigned ngroups;
	gid_t *groups;
} uwrap;

static void uwrap_init(void)
{
	if (uwrap.initialised) return;
	uwrap.initialised = true;
	if (getenv("UID_WRAPPER")) {
		uwrap.enabled = true;
		/* put us in one group */
		uwrap.ngroups = 1;
		uwrap.groups = talloc_array(NULL, gid_t, 1);
		uwrap.groups[0] = 0;
	}
}

#undef uwrap_enabled
_PUBLIC_ int uwrap_enabled(void)
{
	uwrap_init();
	return uwrap.enabled?1:0;
}

_PUBLIC_ int uwrap_seteuid(uid_t euid)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return seteuid(euid);
	}
	/* assume for now that the ruid stays as root */
	uwrap.euid = euid;
	return 0;
}

_PUBLIC_ uid_t uwrap_geteuid(void)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return geteuid();
	}
	return uwrap.euid;
}

_PUBLIC_ int uwrap_setegid(gid_t egid)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return setegid(egid);
	}
	/* assume for now that the ruid stays as root */
	uwrap.egid = egid;
	return 0;
}

_PUBLIC_ uid_t uwrap_getegid(void)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return getegid();
	}
	return uwrap.egid;
}

_PUBLIC_ int uwrap_setgroups(size_t size, const gid_t *list)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return setgroups(size, list);
	}

	talloc_free(uwrap.groups);
	uwrap.ngroups = 0;
	uwrap.groups = NULL;

	if (size != 0) {
		uwrap.groups = talloc_array(NULL, gid_t, size);
		if (uwrap.groups == NULL) {
			errno = ENOMEM;
			return -1;
		}
		memcpy(uwrap.groups, list, size*sizeof(gid_t));
		uwrap.ngroups = size;
	}
	return 0;
}

_PUBLIC_ int uwrap_getgroups(int size, gid_t *list)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return getgroups(size, list);
	}

	if (size > uwrap.ngroups) {
		size = uwrap.ngroups;
	}
	if (size == 0) {
		return uwrap.ngroups;
	}
	if (size < uwrap.ngroups) {
		errno = EINVAL;
		return -1;
	}
	memcpy(list, uwrap.groups, size*sizeof(gid_t));
	return uwrap.ngroups;
}

_PUBLIC_ uid_t uwrap_getuid(void)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return getuid();
	}
	/* we don't simulate ruid changing */
	return 0;
}

_PUBLIC_ gid_t uwrap_getgid(void)
{
	uwrap_init();
	if (!uwrap.enabled) {
		return getgid();
	}
	/* we don't simulate rgid changing */
	return 0;
}
