/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Gerald Carter 2005
   Copyright (C) Volker Lendecke 2007
   Copyright (C) Jeremy Allison 2008

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

extern NT_USER_TOKEN anonymous_token;

/* Map generic access rights to object specific rights.  This technique is
   used to give meaning to assigning read, write, execute and all access to
   objects.  Each type of object has its own mapping of generic to object
   specific access rights. */

void se_map_generic(uint32 *access_mask, const struct generic_mapping *mapping)
{
	uint32 old_mask = *access_mask;

	if (*access_mask & GENERIC_READ_ACCESS) {
		*access_mask &= ~GENERIC_READ_ACCESS;
		*access_mask |= mapping->generic_read;
	}

	if (*access_mask & GENERIC_WRITE_ACCESS) {
		*access_mask &= ~GENERIC_WRITE_ACCESS;
		*access_mask |= mapping->generic_write;
	}

	if (*access_mask & GENERIC_EXECUTE_ACCESS) {
		*access_mask &= ~GENERIC_EXECUTE_ACCESS;
		*access_mask |= mapping->generic_execute;
	}

	if (*access_mask & GENERIC_ALL_ACCESS) {
		*access_mask &= ~GENERIC_ALL_ACCESS;
		*access_mask |= mapping->generic_all;
	}

	if (old_mask != *access_mask) {
		DEBUG(10, ("se_map_generic(): mapped mask 0x%08x to 0x%08x\n",
			   old_mask, *access_mask));
	}
}

/* Map generic access rights to object specific rights for all the ACE's
 * in a security_acl.
 */

void security_acl_map_generic(struct security_acl *sa,
				const struct generic_mapping *mapping)
{
	unsigned int i;

	if (!sa) {
		return;
	}

	for (i = 0; i < sa->num_aces; i++) {
		se_map_generic(&sa->aces[i].access_mask, mapping);
	}
}

/* Map standard access rights to object specific rights.  This technique is
   used to give meaning to assigning read, write, execute and all access to
   objects.  Each type of object has its own mapping of standard to object
   specific access rights. */

void se_map_standard(uint32 *access_mask, struct standard_mapping *mapping)
{
	uint32 old_mask = *access_mask;

	if (*access_mask & SEC_STD_READ_CONTROL) {
		*access_mask &= ~SEC_STD_READ_CONTROL;
		*access_mask |= mapping->std_read;
	}

	if (*access_mask & (SEC_STD_DELETE|SEC_STD_WRITE_DAC|SEC_STD_WRITE_OWNER|SEC_STD_SYNCHRONIZE)) {
		*access_mask &= ~(SEC_STD_DELETE|SEC_STD_WRITE_DAC|SEC_STD_WRITE_OWNER|SEC_STD_SYNCHRONIZE);
		*access_mask |= mapping->std_all;
	}

	if (old_mask != *access_mask) {
		DEBUG(10, ("se_map_standard(): mapped mask 0x%08x to 0x%08x\n",
			   old_mask, *access_mask));
	}
}

/*
  perform a SEC_FLAG_MAXIMUM_ALLOWED access check
*/
static uint32_t access_check_max_allowed(const struct security_descriptor *sd, 
			  		const NT_USER_TOKEN *token)
{
	uint32_t denied = 0, granted = 0;
	unsigned i;

	if (is_sid_in_token(token, sd->owner_sid)) {
		granted |= SEC_STD_WRITE_DAC | SEC_STD_READ_CONTROL | SEC_STD_DELETE;
	} else if (user_has_privileges(token, &se_restore)) {
		granted |= SEC_STD_DELETE;
	}

	if (sd->dacl == NULL) {
		return granted & ~denied;
	}

	for (i = 0;i<sd->dacl->num_aces; i++) {
		struct security_ace *ace = &sd->dacl->aces[i];

		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
			continue;
		}

		if (!is_sid_in_token(token, &ace->trustee)) {
			continue;
		}

		switch (ace->type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED:
			granted |= ace->access_mask;
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED:
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			denied |= ace->access_mask;
			break;
		default:	/* Other ACE types not handled/supported */
			break;
		}
	}

	return granted & ~denied;
}

/*
  The main entry point for access checking. If returning ACCESS_DENIED
  this function returns the denied bits in the uint32_t pointed
  to by the access_granted pointer.
*/
NTSTATUS se_access_check(const struct security_descriptor *sd, 
			  const NT_USER_TOKEN *token,
			  uint32_t access_desired,
			  uint32_t *access_granted)
{
	int i;
	uint32_t bits_remaining;

	*access_granted = access_desired;
	bits_remaining = access_desired;

	/* handle the maximum allowed flag */
	if (access_desired & SEC_FLAG_MAXIMUM_ALLOWED) {
		uint32_t orig_access_desired = access_desired;

		access_desired |= access_check_max_allowed(sd, token);
		access_desired &= ~SEC_FLAG_MAXIMUM_ALLOWED;
		*access_granted = access_desired;
		bits_remaining = access_desired & ~SEC_STD_DELETE;

		DEBUG(10,("se_access_check: MAX desired = 0x%x, granted = 0x%x, remaining = 0x%x\n",
			orig_access_desired,
			*access_granted,
			bits_remaining));
	}

	if (access_desired & SEC_FLAG_SYSTEM_SECURITY) {
		if (user_has_privileges(token, &se_security)) {
			bits_remaining &= ~SEC_FLAG_SYSTEM_SECURITY;
		} else {
			return NT_STATUS_PRIVILEGE_NOT_HELD;
		}
	}

	/* a NULL dacl allows access */
	if ((sd->type & SEC_DESC_DACL_PRESENT) && sd->dacl == NULL) {
		*access_granted = access_desired;
		return NT_STATUS_OK;
	}

	/* the owner always gets SEC_STD_WRITE_DAC, SEC_STD_READ_CONTROL and SEC_STD_DELETE */
	if ((bits_remaining & (SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL|SEC_STD_DELETE)) &&
	    is_sid_in_token(token, sd->owner_sid)) {
		bits_remaining &= ~(SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL|SEC_STD_DELETE);
	}
	if ((bits_remaining & SEC_STD_DELETE) &&
	    user_has_privileges(token, &se_restore)) {
		bits_remaining &= ~SEC_STD_DELETE;
	}

	if (sd->dacl == NULL) {
		goto done;
	}

	/* check each ace in turn. */
	for (i=0; bits_remaining && i < sd->dacl->num_aces; i++) {
		struct security_ace *ace = &sd->dacl->aces[i];

		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
			continue;
		}

		if (!is_sid_in_token(token, &ace->trustee)) {
			continue;
		}

		switch (ace->type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED:
			bits_remaining &= ~ace->access_mask;
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED:
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			if (bits_remaining & ace->access_mask) {
				return NT_STATUS_ACCESS_DENIED;
			}
			break;
		default:	/* Other ACE types not handled/supported */
			break;
		}
	}

done:
	if (bits_remaining != 0) {
		*access_granted = bits_remaining;
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 samr_make_sam_obj_sd
 ********************************************************************/

NTSTATUS samr_make_sam_obj_sd(TALLOC_CTX *ctx, SEC_DESC **psd, size_t *sd_size)
{
	DOM_SID adm_sid;
	DOM_SID act_sid;

	SEC_ACE ace[3];

	SEC_ACL *psa = NULL;

	sid_copy(&adm_sid, &global_sid_Builtin);
	sid_append_rid(&adm_sid, BUILTIN_ALIAS_RID_ADMINS);

	sid_copy(&act_sid, &global_sid_Builtin);
	sid_append_rid(&act_sid, BUILTIN_ALIAS_RID_ACCOUNT_OPS);

	/*basic access for every one*/
	init_sec_ace(&ace[0], &global_sid_World, SEC_ACE_TYPE_ACCESS_ALLOWED,
		GENERIC_RIGHTS_SAM_EXECUTE | GENERIC_RIGHTS_SAM_READ, 0);

	/*full access for builtin aliases Administrators and Account Operators*/
	init_sec_ace(&ace[1], &adm_sid,
		SEC_ACE_TYPE_ACCESS_ALLOWED, GENERIC_RIGHTS_SAM_ALL_ACCESS, 0);
	init_sec_ace(&ace[2], &act_sid,
		SEC_ACE_TYPE_ACCESS_ALLOWED, GENERIC_RIGHTS_SAM_ALL_ACCESS, 0);

	if ((psa = make_sec_acl(ctx, NT4_ACL_REVISION, 3, ace)) == NULL)
		return NT_STATUS_NO_MEMORY;

	if ((*psd = make_sec_desc(ctx, SECURITY_DESCRIPTOR_REVISION_1,
				  SEC_DESC_SELF_RELATIVE, NULL, NULL, NULL,
				  psa, sd_size)) == NULL)
		return NT_STATUS_NO_MEMORY;

	return NT_STATUS_OK;
}
