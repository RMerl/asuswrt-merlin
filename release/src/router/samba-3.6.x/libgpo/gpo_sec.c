/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2007
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
#include "libcli/security/security.h"
#include "../libgpo/gpo.h"
#include "auth.h"
#include "../librpc/ndr/libndr.h"
#if _SAMBA_BUILD_ == 4
#include "libgpo/ads_convenience.h"
#include "librpc/gen_ndr/security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "../libcli/security/secace.h"
#endif

/****************************************************************
****************************************************************/

static bool gpo_sd_check_agp_object_guid(const struct security_ace_object *object)
{
	struct GUID ext_right_apg_guid;
	NTSTATUS status;

	if (!object) {
		return false;
	}

	status = GUID_from_string(ADS_EXTENDED_RIGHT_APPLY_GROUP_POLICY,
				  &ext_right_apg_guid);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	switch (object->flags) {
		case SEC_ACE_OBJECT_TYPE_PRESENT:
			if (GUID_equal(&object->type.type,
				       &ext_right_apg_guid)) {
				return true;
			}
		case SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT:
			if (GUID_equal(&object->inherited_type.inherited_type,
				       &ext_right_apg_guid)) {
				return true;
			}
		default:
			break;
	}

	return false;
}

/****************************************************************
****************************************************************/

static bool gpo_sd_check_agp_object(const struct security_ace *ace)
{
	if (!sec_ace_object(ace->type)) {
		return false;
	}

	return gpo_sd_check_agp_object_guid(&ace->object.object);
}

/****************************************************************
****************************************************************/

static bool gpo_sd_check_agp_access_bits(uint32_t access_mask)
{
	return (access_mask & SEC_ADS_CONTROL_ACCESS);
}

#if 0
/****************************************************************
****************************************************************/

static bool gpo_sd_check_read_access_bits(uint32_t access_mask)
{
	uint32_t read_bits = SEC_RIGHTS_LIST_CONTENTS |
			   SEC_RIGHTS_READ_ALL_PROP |
			   SEC_RIGHTS_READ_PERMS;

	return (read_bits == (access_mask & read_bits));
}
#endif

/****************************************************************
****************************************************************/

static NTSTATUS gpo_sd_check_ace_denied_object(const struct security_ace *ace,
					       const struct security_token *token)
{
	char *sid_str;

	if (gpo_sd_check_agp_object(ace) &&
	    gpo_sd_check_agp_access_bits(ace->access_mask) &&
	    nt_token_check_sid(&ace->trustee, token)) {
		sid_str = dom_sid_string(NULL, &ace->trustee);
		DEBUG(10,("gpo_sd_check_ace_denied_object: "
			"Access denied as of ace for %s\n",
			sid_str));
		talloc_free(sid_str);
		return NT_STATUS_ACCESS_DENIED;
	}

	return STATUS_MORE_ENTRIES;
}

/****************************************************************
****************************************************************/

static NTSTATUS gpo_sd_check_ace_allowed_object(const struct security_ace *ace,
						const struct security_token *token)
{
	char *sid_str;

	if (gpo_sd_check_agp_object(ace) &&
	    gpo_sd_check_agp_access_bits(ace->access_mask) &&
	    nt_token_check_sid(&ace->trustee, token)) {
		sid_str = dom_sid_string(NULL, &ace->trustee);
		DEBUG(10,("gpo_sd_check_ace_allowed_object: "
			"Access granted as of ace for %s\n",
			sid_str));
		talloc_free(sid_str);

		return NT_STATUS_OK;
	}

	return STATUS_MORE_ENTRIES;
}

/****************************************************************
****************************************************************/

static NTSTATUS gpo_sd_check_ace(const struct security_ace *ace,
				 const struct security_token *token)
{
	switch (ace->type) {
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			return gpo_sd_check_ace_denied_object(ace, token);
		case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
			return gpo_sd_check_ace_allowed_object(ace, token);
		default:
			return STATUS_MORE_ENTRIES;
	}
}

/****************************************************************
****************************************************************/

NTSTATUS gpo_apply_security_filtering(const struct GROUP_POLICY_OBJECT *gpo,
				      const struct security_token *token)
{
	struct security_descriptor *sd = gpo->security_descriptor;
	struct security_acl *dacl = NULL;
	NTSTATUS status = NT_STATUS_ACCESS_DENIED;
	int i;

	if (!token) {
		return NT_STATUS_INVALID_USER_BUFFER;
	}

	if (!sd) {
		return NT_STATUS_INVALID_SECURITY_DESCR;
	}

	dacl = sd->dacl;
	if (!dacl) {
		return NT_STATUS_INVALID_SECURITY_DESCR;
	}

	/* check all aces and only return NT_STATUS_OK (== Access granted) or
	 * NT_STATUS_ACCESS_DENIED ( == Access denied) - the default is to
	 * deny access */

	for (i = 0; i < dacl->num_aces; i ++) {

		status = gpo_sd_check_ace(&dacl->aces[i], token);

		if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
			return status;
		} else if (NT_STATUS_IS_OK(status)) {
			return status;
		}

		continue;
	}

	return NT_STATUS_ACCESS_DENIED;
}
