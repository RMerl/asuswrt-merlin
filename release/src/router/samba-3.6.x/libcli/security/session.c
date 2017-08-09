/*
   Unix SMB/CIFS implementation.

   session_info utility functions

   Copyright (C) Andrew Bartlett 2008-2010

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
#include "libcli/security/security.h"
#include "librpc/gen_ndr/auth.h"

enum security_user_level security_session_user_level(struct auth_session_info *session_info,
						     const struct dom_sid *domain_sid)
{
	if (!session_info) {
		return SECURITY_ANONYMOUS;
	}

	if (security_token_is_system(session_info->security_token)) {
		return SECURITY_SYSTEM;
	}

	if (security_token_is_anonymous(session_info->security_token)) {
		return SECURITY_ANONYMOUS;
	}

	if (security_token_has_builtin_administrators(session_info->security_token)) {
		return SECURITY_ADMINISTRATOR;
	}

	if (domain_sid) {
		struct dom_sid *rodc_dcs;
		rodc_dcs = dom_sid_add_rid(session_info, domain_sid, DOMAIN_RID_READONLY_DCS);
		if (security_token_has_sid(session_info->security_token, rodc_dcs)) {
			talloc_free(rodc_dcs);
			return SECURITY_RO_DOMAIN_CONTROLLER;
		}
		talloc_free(rodc_dcs);
	}

	if (security_token_has_enterprise_dcs(session_info->security_token)) {
		return SECURITY_DOMAIN_CONTROLLER;
	}

	if (security_token_has_nt_authenticated_users(session_info->security_token)) {
		return SECURITY_USER;
	}

	return SECURITY_ANONYMOUS;
}
