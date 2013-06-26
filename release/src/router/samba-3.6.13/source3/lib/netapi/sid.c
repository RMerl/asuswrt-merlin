/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
 *  Copyright (C) Guenther Deschner 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#include "lib/netapi/netapi.h"
#include "../libcli/security/security.h"

/****************************************************************
****************************************************************/

int ConvertSidToStringSid(const struct domsid *sid,
			  char **sid_string)
{
	char *ret;

	if (!sid || !sid_string) {
		return false;
	}

	ret = sid_string_talloc(NULL, (const struct dom_sid *)sid);
	if (!ret) {
		return false;
	}

	*sid_string = SMB_STRDUP(ret);

	TALLOC_FREE(ret);

	if (!*sid_string) {
		return false;
	}

	return true;
}

/****************************************************************
****************************************************************/

int ConvertStringSidToSid(const char *sid_string,
			  struct domsid **sid)
{
	struct dom_sid _sid;

	if (!sid_string || !sid) {
		return false;
	}

	if (!string_to_sid(&_sid, sid_string)) {
		return false;
	}

	*sid = (struct domsid *)SMB_MALLOC(sizeof(struct domsid));
	if (!*sid) {
		return false;
	}

	sid_copy((struct dom_sid*)*sid, &_sid);

	return true;
}
