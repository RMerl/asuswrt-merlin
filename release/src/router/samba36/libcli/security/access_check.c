/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Gerald Carter 2005
   Copyright (C) Volker Lendecke 2007
   Copyright (C) Jeremy Allison 2008
   Copyright (C) Andrew Bartlett 2010

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

/* Map generic access rights to object specific rights.  This technique is
   used to give meaning to assigning read, write, execute and all access to
   objects.  Each type of object has its own mapping of generic to object
   specific access rights. */

void se_map_generic(uint32_t *access_mask, const struct generic_mapping *mapping)
{
	uint32_t old_mask = *access_mask;

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

void se_map_standard(uint32_t *access_mask, const struct standard_mapping *mapping)
{
	uint32_t old_mask = *access_mask;

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
					const struct security_token *token)
{
	uint32_t denied = 0, granted = 0;
	unsigned i;

	if (security_token_has_sid(token, sd->owner_sid)) {
		granted |= SEC_STD_WRITE_DAC | SEC_STD_READ_CONTROL;
	}

	if (sd->dacl == NULL) {
		return granted & ~denied;
	}

	for (i = 0;i<sd->dacl->num_aces; i++) {
		struct security_ace *ace = &sd->dacl->aces[i];

		if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
			continue;
		}

		if (!security_token_has_sid(token, &ace->trustee)) {
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
			  const struct security_token *token,
			  uint32_t access_desired,
			  uint32_t *access_granted)
{
	uint32_t i;
	uint32_t bits_remaining;
	uint32_t explicitly_denied_bits = 0;

	*access_granted = access_desired;
	bits_remaining = access_desired;

	/* handle the maximum allowed flag */
	if (access_desired & SEC_FLAG_MAXIMUM_ALLOWED) {
		uint32_t orig_access_desired = access_desired;

		access_desired |= access_check_max_allowed(sd, token);
		access_desired &= ~SEC_FLAG_MAXIMUM_ALLOWED;
		*access_granted = access_desired;
		bits_remaining = access_desired;

		DEBUG(10,("se_access_check: MAX desired = 0x%x, granted = 0x%x, remaining = 0x%x\n",
			orig_access_desired,
			*access_granted,
			bits_remaining));
	}

	/* the owner always gets SEC_STD_WRITE_DAC and SEC_STD_READ_CONTROL */
	if ((bits_remaining & (SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL)) &&
	    security_token_has_sid(token, sd->owner_sid)) {
		bits_remaining &= ~(SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL);
	}

	/* a NULL dacl allows access */
	if ((sd->type & SEC_DESC_DACL_PRESENT) && sd->dacl == NULL) {
		*access_granted = access_desired;
		return NT_STATUS_OK;
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

		if (!security_token_has_sid(token, &ace->trustee)) {
			continue;
		}

		switch (ace->type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED:
			bits_remaining &= ~ace->access_mask;
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED:
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			explicitly_denied_bits |= (bits_remaining & ace->access_mask);
			break;
		default:	/* Other ACE types not handled/supported */
			break;
		}
	}

	bits_remaining |= explicitly_denied_bits;

	/*
	 * We check privileges here because they override even DENY entries.
	 */

	/* Does the user have the privilege to gain SEC_PRIV_SECURITY? */
	if (bits_remaining & SEC_FLAG_SYSTEM_SECURITY) {
		if (security_token_has_privilege(token, SEC_PRIV_SECURITY)) {
			bits_remaining &= ~SEC_FLAG_SYSTEM_SECURITY;
		} else {
			return NT_STATUS_PRIVILEGE_NOT_HELD;
		}
	}

	/* TODO: remove this, as it is file server specific */
	if ((bits_remaining & SEC_RIGHTS_PRIV_RESTORE) &&
	    security_token_has_privilege(token, SEC_PRIV_RESTORE)) {
		bits_remaining &= ~(SEC_RIGHTS_PRIV_RESTORE);
	}
	if ((bits_remaining & SEC_RIGHTS_PRIV_BACKUP) &&
	    security_token_has_privilege(token, SEC_PRIV_BACKUP)) {
		bits_remaining &= ~(SEC_RIGHTS_PRIV_BACKUP);
	}

	if ((bits_remaining & SEC_STD_WRITE_OWNER) &&
	     security_token_has_privilege(token, SEC_PRIV_TAKE_OWNERSHIP)) {
		bits_remaining &= ~(SEC_STD_WRITE_OWNER);
	}

done:
	if (bits_remaining != 0) {
		*access_granted = bits_remaining;
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}


static const struct GUID *get_ace_object_type(struct security_ace *ace)
{
        struct GUID *type;

        if (ace->object.object.flags & SEC_ACE_OBJECT_TYPE_PRESENT)
                type = &ace->object.object.type.type;
        else if (ace->object.object.flags & SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT)
                type = &ace->object.object.inherited_type.inherited_type; /* This doesn't look right. Is something wrong with the IDL? */
        else
                type = NULL;

        return type;

}

/* modified access check for the purposes of DS security
 * Lots of code duplication, it will ve united in just one
 * function eventually */

NTSTATUS sec_access_check_ds(const struct security_descriptor *sd,
			     const struct security_token *token,
			     uint32_t access_desired,
			     uint32_t *access_granted,
			     struct object_tree *tree,
			     struct dom_sid *replace_sid)
{
        uint32_t i;
        uint32_t bits_remaining;
        struct object_tree *node;
        const struct GUID *type;
        struct dom_sid *ps_sid = dom_sid_parse_talloc(NULL, SID_NT_SELF);

        *access_granted = access_desired;
        bits_remaining = access_desired;

        /* handle the maximum allowed flag */
        if (access_desired & SEC_FLAG_MAXIMUM_ALLOWED) {
                access_desired |= access_check_max_allowed(sd, token);
                access_desired &= ~SEC_FLAG_MAXIMUM_ALLOWED;
                *access_granted = access_desired;
		bits_remaining = access_desired;
        }

        if (access_desired & SEC_FLAG_SYSTEM_SECURITY) {
                if (security_token_has_privilege(token, SEC_PRIV_SECURITY)) {
                        bits_remaining &= ~SEC_FLAG_SYSTEM_SECURITY;
                } else {
                        talloc_free(ps_sid);
                        return NT_STATUS_PRIVILEGE_NOT_HELD;
                }
        }

	/* the owner always gets SEC_STD_WRITE_DAC and SEC_STD_READ_CONTROL */
	if ((bits_remaining & (SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL)) &&
	    security_token_has_sid(token, sd->owner_sid)) {
		bits_remaining &= ~(SEC_STD_WRITE_DAC|SEC_STD_READ_CONTROL);
	}

	/* TODO: remove this, as it is file server specific */
	if ((bits_remaining & SEC_RIGHTS_PRIV_RESTORE) &&
	    security_token_has_privilege(token, SEC_PRIV_RESTORE)) {
		bits_remaining &= ~(SEC_RIGHTS_PRIV_RESTORE);
	}
	if ((bits_remaining & SEC_RIGHTS_PRIV_BACKUP) &&
	    security_token_has_privilege(token, SEC_PRIV_BACKUP)) {
		bits_remaining &= ~(SEC_RIGHTS_PRIV_BACKUP);
	}

        /* a NULL dacl allows access */
        if ((sd->type & SEC_DESC_DACL_PRESENT) && sd->dacl == NULL) {
                *access_granted = access_desired;
                talloc_free(ps_sid);
                return NT_STATUS_OK;
        }

        if (sd->dacl == NULL) {
                goto done;
        }

        /* check each ace in turn. */
        for (i=0; bits_remaining && i < sd->dacl->num_aces; i++) {
		struct dom_sid *trustee;
		struct security_ace *ace = &sd->dacl->aces[i];

                if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
                        continue;
                }
		if (dom_sid_equal(&ace->trustee, ps_sid) && replace_sid) {
			trustee = replace_sid;
		}
		else
		{
			trustee = &ace->trustee;
		}
                if (!security_token_has_sid(token, trustee)) {
                        continue;
                }

                switch (ace->type) {
                case SEC_ACE_TYPE_ACCESS_ALLOWED:
                        if (tree)
                                object_tree_modify_access(tree, ace->access_mask);

                        bits_remaining &= ~ace->access_mask;
                        break;
                case SEC_ACE_TYPE_ACCESS_DENIED:
                        if (bits_remaining & ace->access_mask) {
                                talloc_free(ps_sid);
                                return NT_STATUS_ACCESS_DENIED;
                        }
                        break;
                case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
                case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
                        /* check only in case we have provided a tree,
                         * the ACE has an object type and that type
                         * is in the tree                           */
                        type = get_ace_object_type(ace);

                        if (!tree)
                                continue;

                        if (!type)
                                node = tree;
                        else
                                if (!(node = get_object_tree_by_GUID(tree, type)))
                                        continue;

                        if (ace->type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT) {
                                object_tree_modify_access(node, ace->access_mask);
                                if (node->remaining_access == 0) {
                                        talloc_free(ps_sid);
                                        return NT_STATUS_OK;
                                }
                        } else {
                                if (node->remaining_access & ace->access_mask){
                                        talloc_free(ps_sid);
                                        return NT_STATUS_ACCESS_DENIED;
                                }
                        }
                        break;
                default:        /* Other ACE types not handled/supported */
                        break;
                }
        }

done:
        talloc_free(ps_sid);
        if (bits_remaining != 0) {
                return NT_STATUS_ACCESS_DENIED;
        }

        return NT_STATUS_OK;
}
