/* 
   Unix SMB/CIFS implementation.

   security descriptror utility functions

   Copyright (C) Andrew Tridgell 		2004
      
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

/*
  return a blank security descriptor (no owners, dacl or sacl)
*/
struct security_descriptor *security_descriptor_initialise(TALLOC_CTX *mem_ctx)
{
	struct security_descriptor *sd;

	sd = talloc(mem_ctx, struct security_descriptor);
	if (!sd) {
		return NULL;
	}

	sd->revision = SD_REVISION;
	/* we mark as self relative, even though it isn't while it remains
	   a pointer in memory because this simplifies the ndr code later.
	   All SDs that we store/emit are in fact SELF_RELATIVE
	*/
	sd->type = SEC_DESC_SELF_RELATIVE;

	sd->owner_sid = NULL;
	sd->group_sid = NULL;
	sd->sacl = NULL;
	sd->dacl = NULL;

	return sd;
}

struct security_acl *security_acl_dup(TALLOC_CTX *mem_ctx,
					     const struct security_acl *oacl)
{
	struct security_acl *nacl;

	if (oacl == NULL) {
		return NULL;
	}

	nacl = talloc (mem_ctx, struct security_acl);
	if (nacl == NULL) {
		return NULL;
	}

	nacl->aces = (struct security_ace *)talloc_memdup (nacl, oacl->aces, sizeof(struct security_ace) * oacl->num_aces);
	if ((nacl->aces == NULL) && (oacl->num_aces > 0)) {
		goto failed;
	}

	nacl->revision = oacl->revision;
	nacl->size = oacl->size;
	nacl->num_aces = oacl->num_aces;
	
	return nacl;

 failed:
	talloc_free (nacl);
	return NULL;
	
}

struct security_acl *security_acl_concatenate(TALLOC_CTX *mem_ctx,
                                              const struct security_acl *acl1,
                                              const struct security_acl *acl2)
{
        struct security_acl *nacl;
        uint32_t i;

        if (!acl1 && !acl2)
                return NULL;

        if (!acl1){
                nacl = security_acl_dup(mem_ctx, acl2);
                return nacl;
        }

        if (!acl2){
                nacl = security_acl_dup(mem_ctx, acl1);
                return nacl;
        }

        nacl = talloc (mem_ctx, struct security_acl);
        if (nacl == NULL) {
                return NULL;
        }

        nacl->revision = acl1->revision;
        nacl->size = acl1->size + acl2->size;
        nacl->num_aces = acl1->num_aces + acl2->num_aces;

        if (nacl->num_aces == 0)
                return nacl;

        nacl->aces = (struct security_ace *)talloc_array (mem_ctx, struct security_ace, acl1->num_aces+acl2->num_aces);
        if ((nacl->aces == NULL) && (nacl->num_aces > 0)) {
                goto failed;
        }

        for (i = 0; i < acl1->num_aces; i++)
                nacl->aces[i] = acl1->aces[i];
        for (i = 0; i < acl2->num_aces; i++)
                nacl->aces[i + acl1->num_aces] = acl2->aces[i];

        return nacl;

 failed:
        talloc_free (nacl);
        return NULL;

}

/* 
   talloc and copy a security descriptor
 */
struct security_descriptor *security_descriptor_copy(TALLOC_CTX *mem_ctx, 
						     const struct security_descriptor *osd)
{
	struct security_descriptor *nsd;

	nsd = talloc_zero(mem_ctx, struct security_descriptor);
	if (!nsd) {
		return NULL;
	}

	if (osd->owner_sid) {
		nsd->owner_sid = dom_sid_dup(nsd, osd->owner_sid);
		if (nsd->owner_sid == NULL) {
			goto failed;
		}
	}
	
	if (osd->group_sid) {
		nsd->group_sid = dom_sid_dup(nsd, osd->group_sid);
		if (nsd->group_sid == NULL) {
			goto failed;
		}
	}

	if (osd->sacl) {
		nsd->sacl = security_acl_dup(nsd, osd->sacl);
		if (nsd->sacl == NULL) {
			goto failed;
		}
	}

	if (osd->dacl) {
		nsd->dacl = security_acl_dup(nsd, osd->dacl);
		if (nsd->dacl == NULL) {
			goto failed;
		}
	}

	nsd->revision = osd->revision;
	nsd->type = osd->type;

	return nsd;

 failed:
	talloc_free(nsd);

	return NULL;
}

/*
  add an ACE to an ACL of a security_descriptor
*/

static NTSTATUS security_descriptor_acl_add(struct security_descriptor *sd,
					    bool add_to_sacl,
					    const struct security_ace *ace)
{
	struct security_acl *acl = NULL;

	if (add_to_sacl) {
		acl = sd->sacl;
	} else {
		acl = sd->dacl;
	}

	if (acl == NULL) {
		acl = talloc(sd, struct security_acl);
		if (acl == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		acl->revision = SECURITY_ACL_REVISION_NT4;
		acl->size     = 0;
		acl->num_aces = 0;
		acl->aces     = NULL;
	}

	acl->aces = talloc_realloc(acl, acl->aces,
				   struct security_ace, acl->num_aces+1);
	if (acl->aces == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	acl->aces[acl->num_aces] = *ace;

	switch (acl->aces[acl->num_aces].type) {
	case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
	case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
	case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
	case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
		acl->revision = SECURITY_ACL_REVISION_ADS;
		break;
	default:
		break;
	}

	acl->num_aces++;

	if (add_to_sacl) {
		sd->sacl = acl;
		sd->type |= SEC_DESC_SACL_PRESENT;
	} else {
		sd->dacl = acl;
		sd->type |= SEC_DESC_DACL_PRESENT;
	}

	return NT_STATUS_OK;
}

/*
  add an ACE to the SACL of a security_descriptor
*/

NTSTATUS security_descriptor_sacl_add(struct security_descriptor *sd,
				      const struct security_ace *ace)
{
	return security_descriptor_acl_add(sd, true, ace);
}

/*
  add an ACE to the DACL of a security_descriptor
*/

NTSTATUS security_descriptor_dacl_add(struct security_descriptor *sd,
				      const struct security_ace *ace)
{
	return security_descriptor_acl_add(sd, false, ace);
}

/*
  delete the ACE corresponding to the given trustee in an ACL of a
  security_descriptor
*/

static NTSTATUS security_descriptor_acl_del(struct security_descriptor *sd,
					    bool sacl_del,
					    const struct dom_sid *trustee)
{
	uint32_t i;
	bool found = false;
	struct security_acl *acl = NULL;

	if (sacl_del) {
		acl = sd->sacl;
	} else {
		acl = sd->dacl;
	}

	if (acl == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* there can be multiple ace's for one trustee */
	for (i=0;i<acl->num_aces;i++) {
		if (dom_sid_equal(trustee, &acl->aces[i].trustee)) {
			memmove(&acl->aces[i], &acl->aces[i+1],
				sizeof(acl->aces[i]) * (acl->num_aces - (i+1)));
			acl->num_aces--;
			if (acl->num_aces == 0) {
				acl->aces = NULL;
			}
			found = true;
		}
	}

	if (!found) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	acl->revision = SECURITY_ACL_REVISION_NT4;

	for (i=0;i<acl->num_aces;i++) {
		switch (acl->aces[i].type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
		case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
		case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
			acl->revision = SECURITY_ACL_REVISION_ADS;
			return NT_STATUS_OK;
		default:
			break; /* only for the switch statement */
		}
	}

	return NT_STATUS_OK;
}

/*
  delete the ACE corresponding to the given trustee in the DACL of a
  security_descriptor
*/

NTSTATUS security_descriptor_dacl_del(struct security_descriptor *sd,
				      const struct dom_sid *trustee)
{
	return security_descriptor_acl_del(sd, false, trustee);
}

/*
  delete the ACE corresponding to the given trustee in the SACL of a
  security_descriptor
*/

NTSTATUS security_descriptor_sacl_del(struct security_descriptor *sd,
				      const struct dom_sid *trustee)
{
	return security_descriptor_acl_del(sd, true, trustee);
}

/*
  compare two security ace structures
*/
bool security_ace_equal(const struct security_ace *ace1, 
			const struct security_ace *ace2)
{
	if (ace1 == ace2) return true;
	if (!ace1 || !ace2) return false;
	if (ace1->type != ace2->type) return false;
	if (ace1->flags != ace2->flags) return false;
	if (ace1->access_mask != ace2->access_mask) return false;
	if (!dom_sid_equal(&ace1->trustee, &ace2->trustee)) return false;

	return true;	
}


/*
  compare two security acl structures
*/
bool security_acl_equal(const struct security_acl *acl1, 
			const struct security_acl *acl2)
{
	uint32_t i;

	if (acl1 == acl2) return true;
	if (!acl1 || !acl2) return false;
	if (acl1->revision != acl2->revision) return false;
	if (acl1->num_aces != acl2->num_aces) return false;

	for (i=0;i<acl1->num_aces;i++) {
		if (!security_ace_equal(&acl1->aces[i], &acl2->aces[i])) return false;
	}
	return true;	
}

/*
  compare two security descriptors.
*/
bool security_descriptor_equal(const struct security_descriptor *sd1, 
			       const struct security_descriptor *sd2)
{
	if (sd1 == sd2) return true;
	if (!sd1 || !sd2) return false;
	if (sd1->revision != sd2->revision) return false;
	if (sd1->type != sd2->type) return false;

	if (!dom_sid_equal(sd1->owner_sid, sd2->owner_sid)) return false;
	if (!dom_sid_equal(sd1->group_sid, sd2->group_sid)) return false;
	if (!security_acl_equal(sd1->sacl, sd2->sacl))      return false;
	if (!security_acl_equal(sd1->dacl, sd2->dacl))      return false;

	return true;	
}

/*
  compare two security descriptors, but allow certain (missing) parts
  to be masked out of the comparison
*/
bool security_descriptor_mask_equal(const struct security_descriptor *sd1, 
				    const struct security_descriptor *sd2, 
				    uint32_t mask)
{
	if (sd1 == sd2) return true;
	if (!sd1 || !sd2) return false;
	if (sd1->revision != sd2->revision) return false;
	if ((sd1->type & mask) != (sd2->type & mask)) return false;

	if (!dom_sid_equal(sd1->owner_sid, sd2->owner_sid)) return false;
	if (!dom_sid_equal(sd1->group_sid, sd2->group_sid)) return false;
	if ((mask & SEC_DESC_DACL_PRESENT) && !security_acl_equal(sd1->dacl, sd2->dacl))      return false;
	if ((mask & SEC_DESC_SACL_PRESENT) && !security_acl_equal(sd1->sacl, sd2->sacl))      return false;

	return true;	
}


static struct security_descriptor *security_descriptor_appendv(struct security_descriptor *sd,
							       bool add_ace_to_sacl,
							       va_list ap)
{
	const char *sidstr;

	while ((sidstr = va_arg(ap, const char *))) {
		struct dom_sid *sid;
		struct security_ace *ace = talloc_zero(sd, struct security_ace);
		NTSTATUS status;

		if (ace == NULL) {
			talloc_free(sd);
			return NULL;
		}
		ace->type = va_arg(ap, unsigned int);
		ace->access_mask = va_arg(ap, unsigned int);
		ace->flags = va_arg(ap, unsigned int);
		sid = dom_sid_parse_talloc(ace, sidstr);
		if (sid == NULL) {
			talloc_free(sd);
			return NULL;
		}
		ace->trustee = *sid;
		if (add_ace_to_sacl) {
			status = security_descriptor_sacl_add(sd, ace);
		} else {
			status = security_descriptor_dacl_add(sd, ace);
		}
		/* TODO: check: would talloc_free(ace) here be correct? */
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(sd);
			return NULL;
		}
	}

	return sd;
}

struct security_descriptor *security_descriptor_append(struct security_descriptor *sd,
						       ...)
{
	va_list ap;

	va_start(ap, sd);
	sd = security_descriptor_appendv(sd, false, ap);
	va_end(ap);

	return sd;
}

static struct security_descriptor *security_descriptor_createv(TALLOC_CTX *mem_ctx,
							       uint16_t sd_type,
							       const char *owner_sid,
							       const char *group_sid,
							       bool add_ace_to_sacl,
							       va_list ap)
{
	struct security_descriptor *sd;

	sd = security_descriptor_initialise(mem_ctx);
	if (sd == NULL) {
		return NULL;
	}

	sd->type |= sd_type;

	if (owner_sid) {
		sd->owner_sid = dom_sid_parse_talloc(sd, owner_sid);
		if (sd->owner_sid == NULL) {
			talloc_free(sd);
			return NULL;
		}
	}
	if (group_sid) {
		sd->group_sid = dom_sid_parse_talloc(sd, group_sid);
		if (sd->group_sid == NULL) {
			talloc_free(sd);
			return NULL;
		}
	}

	return security_descriptor_appendv(sd, add_ace_to_sacl, ap);
}

/*
  create a security descriptor using string SIDs. This is used by the
  torture code to allow the easy creation of complex ACLs
  This is a varargs function. The list of DACL ACEs ends with a NULL sid.

  Each ACE contains a set of 4 parameters:
  SID, ACCESS_TYPE, MASK, FLAGS

  a typical call would be:

    sd = security_descriptor_dacl_create(mem_ctx,
                                         sd_type_flags,
                                         mysid,
                                         mygroup,
                                         SID_NT_AUTHENTICATED_USERS,
                                         SEC_ACE_TYPE_ACCESS_ALLOWED,
                                         SEC_FILE_ALL,
                                         SEC_ACE_FLAG_OBJECT_INHERIT,
                                         NULL);
  that would create a sd with one DACL ACE
*/

struct security_descriptor *security_descriptor_dacl_create(TALLOC_CTX *mem_ctx,
							    uint16_t sd_type,
							    const char *owner_sid,
							    const char *group_sid,
							    ...)
{
	struct security_descriptor *sd = NULL;
	va_list ap;
	va_start(ap, group_sid);
	sd = security_descriptor_createv(mem_ctx, sd_type, owner_sid,
					 group_sid, false, ap);
	va_end(ap);

	return sd;
}

struct security_descriptor *security_descriptor_sacl_create(TALLOC_CTX *mem_ctx,
							    uint16_t sd_type,
							    const char *owner_sid,
							    const char *group_sid,
							    ...)
{
	struct security_descriptor *sd = NULL;
	va_list ap;
	va_start(ap, group_sid);
	sd = security_descriptor_createv(mem_ctx, sd_type, owner_sid,
					 group_sid, true, ap);
	va_end(ap);

	return sd;
}

struct security_ace *security_ace_create(TALLOC_CTX *mem_ctx,
					 const char *sid_str,
					 enum security_ace_type type,
					 uint32_t access_mask,
					 uint8_t flags)

{
	struct dom_sid *sid;
	struct security_ace *ace;

	ace = talloc_zero(mem_ctx, struct security_ace);
	if (ace == NULL) {
		return NULL;
	}

	sid = dom_sid_parse_talloc(ace, sid_str);
	if (sid == NULL) {
		talloc_free(ace);
		return NULL;
	}

	ace->trustee = *sid;
	ace->type = type;
	ace->access_mask = access_mask;
	ace->flags = flags;

	return ace;
}
