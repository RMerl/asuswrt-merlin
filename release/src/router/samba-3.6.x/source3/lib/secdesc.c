/* 
 *  Unix SMB/Netbios implementation.
 *  SEC_DESC handling functions
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Jeremy R. Allison            1995-2003.
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
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
#include "../librpc/gen_ndr/ndr_security.h"
#include "../libcli/security/security.h"

#define ALL_SECURITY_INFORMATION (SECINFO_OWNER|SECINFO_GROUP|\
					SECINFO_DACL|SECINFO_SACL|\
					SECINFO_UNPROTECTED_SACL|\
					SECINFO_UNPROTECTED_DACL|\
					SECINFO_PROTECTED_SACL|\
					SECINFO_PROTECTED_DACL)

/* Map generic permissions to file object specific permissions */

const struct generic_mapping file_generic_mapping = {
	FILE_GENERIC_READ,
	FILE_GENERIC_WRITE,
	FILE_GENERIC_EXECUTE,
	FILE_GENERIC_ALL
};

/*******************************************************************
 Given a security_descriptor return the sec_info.
********************************************************************/

uint32_t get_sec_info(const struct security_descriptor *sd)
{
	uint32_t sec_info = ALL_SECURITY_INFORMATION;

	SMB_ASSERT(sd);

	if (sd->owner_sid == NULL) {
		sec_info &= ~SECINFO_OWNER;
	}
	if (sd->group_sid == NULL) {
		sec_info &= ~SECINFO_GROUP;
	}
	if (sd->sacl == NULL) {
		sec_info &= ~SECINFO_SACL;
	}
	if (sd->dacl == NULL) {
		sec_info &= ~SECINFO_DACL;
	}

	return sec_info;
}


/*******************************************************************
 Merge part of security descriptor old_sec in to the empty sections of 
 security descriptor new_sec.
********************************************************************/

struct sec_desc_buf *sec_desc_merge_buf(TALLOC_CTX *ctx, struct sec_desc_buf *new_sdb, struct sec_desc_buf *old_sdb)
{
	struct dom_sid *owner_sid, *group_sid;
	struct sec_desc_buf *return_sdb;
	struct security_acl *dacl, *sacl;
	struct security_descriptor *psd = NULL;
	uint16 secdesc_type;
	size_t secdesc_size;

	/* Copy over owner and group sids.  There seems to be no flag for
	   this so just check the pointer values. */

	owner_sid = new_sdb->sd->owner_sid ? new_sdb->sd->owner_sid :
		old_sdb->sd->owner_sid;

	group_sid = new_sdb->sd->group_sid ? new_sdb->sd->group_sid :
		old_sdb->sd->group_sid;
	
	secdesc_type = new_sdb->sd->type;

	/* Ignore changes to the system ACL.  This has the effect of making
	   changes through the security tab audit button not sticking. 
	   Perhaps in future Samba could implement these settings somehow. */

	sacl = NULL;
	secdesc_type &= ~SEC_DESC_SACL_PRESENT;

	/* Copy across discretionary ACL */

	if (secdesc_type & SEC_DESC_DACL_PRESENT) {
		dacl = new_sdb->sd->dacl;
	} else {
		dacl = old_sdb->sd->dacl;
	}

	/* Create new security descriptor from bits */

	psd = make_sec_desc(ctx, new_sdb->sd->revision, secdesc_type,
			    owner_sid, group_sid, sacl, dacl, &secdesc_size);

	return_sdb = make_sec_desc_buf(ctx, secdesc_size, psd);

	return(return_sdb);
}

struct security_descriptor *sec_desc_merge(TALLOC_CTX *ctx, struct security_descriptor *new_sdb, struct security_descriptor *old_sdb)
{
	struct dom_sid *owner_sid, *group_sid;
	struct security_acl *dacl, *sacl;
	struct security_descriptor *psd = NULL;
	uint16 secdesc_type;
	size_t secdesc_size;

	/* Copy over owner and group sids.  There seems to be no flag for
	   this so just check the pointer values. */

	owner_sid = new_sdb->owner_sid ? new_sdb->owner_sid :
		old_sdb->owner_sid;

	group_sid = new_sdb->group_sid ? new_sdb->group_sid :
		old_sdb->group_sid;

	secdesc_type = new_sdb->type;

	/* Ignore changes to the system ACL.  This has the effect of making
	   changes through the security tab audit button not sticking.
	   Perhaps in future Samba could implement these settings somehow. */

	sacl = NULL;
	secdesc_type &= ~SEC_DESC_SACL_PRESENT;

	/* Copy across discretionary ACL */

	if (secdesc_type & SEC_DESC_DACL_PRESENT) {
		dacl = new_sdb->dacl;
	} else {
		dacl = old_sdb->dacl;
	}

	/* Create new security descriptor from bits */
	psd = make_sec_desc(ctx, new_sdb->revision, secdesc_type,
			    owner_sid, group_sid, sacl, dacl, &secdesc_size);

	return psd;
}

/*******************************************************************
 Creates a struct security_descriptor structure
********************************************************************/

#define  SEC_DESC_HEADER_SIZE (2 * sizeof(uint16) + 4 * sizeof(uint32))

struct security_descriptor *make_sec_desc(TALLOC_CTX *ctx,
			enum security_descriptor_revision revision,
			uint16 type,
			const struct dom_sid *owner_sid, const struct dom_sid *grp_sid,
			struct security_acl *sacl, struct security_acl *dacl, size_t *sd_size)
{
	struct security_descriptor *dst;
	uint32 offset     = 0;

	*sd_size = 0;

	if(( dst = TALLOC_ZERO_P(ctx, struct security_descriptor)) == NULL)
		return NULL;

	dst->revision = revision;
	dst->type = type;

	if (sacl)
		dst->type |= SEC_DESC_SACL_PRESENT;
	if (dacl)
		dst->type |= SEC_DESC_DACL_PRESENT;

	dst->owner_sid = NULL;
	dst->group_sid   = NULL;
	dst->sacl      = NULL;
	dst->dacl      = NULL;

	if(owner_sid && ((dst->owner_sid = dom_sid_dup(dst,owner_sid)) == NULL))
		goto error_exit;

	if(grp_sid && ((dst->group_sid = dom_sid_dup(dst,grp_sid)) == NULL))
		goto error_exit;

	if(sacl && ((dst->sacl = dup_sec_acl(dst, sacl)) == NULL))
		goto error_exit;

	if(dacl && ((dst->dacl = dup_sec_acl(dst, dacl)) == NULL))
		goto error_exit;

	offset = SEC_DESC_HEADER_SIZE;

	/*
	 * Work out the linearization sizes.
	 */

	if (dst->sacl != NULL) {
		offset += dst->sacl->size;
	}
	if (dst->dacl != NULL) {
		offset += dst->dacl->size;
	}

	if (dst->owner_sid != NULL) {
		offset += ndr_size_dom_sid(dst->owner_sid, 0);
	}

	if (dst->group_sid != NULL) {
		offset += ndr_size_dom_sid(dst->group_sid, 0);
	}

	*sd_size = (size_t)offset;
	return dst;

error_exit:

	*sd_size = 0;
	return NULL;
}

/*******************************************************************
 Duplicate a struct security_descriptor structure.
********************************************************************/

struct security_descriptor *dup_sec_desc(TALLOC_CTX *ctx, const struct security_descriptor *src)
{
	size_t dummy;

	if(src == NULL)
		return NULL;

	return make_sec_desc( ctx, src->revision, src->type,
				src->owner_sid, src->group_sid, src->sacl,
				src->dacl, &dummy);
}

/*******************************************************************
 Convert a secdesc into a byte stream
********************************************************************/
NTSTATUS marshall_sec_desc(TALLOC_CTX *mem_ctx,
			   struct security_descriptor *secdesc,
			   uint8 **data, size_t *len)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(
		&blob, mem_ctx, secdesc,
		(ndr_push_flags_fn_t)ndr_push_security_descriptor);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_push_security_descriptor failed: %s\n",
			  ndr_errstr(ndr_err)));
		return ndr_map_error2ntstatus(ndr_err);
	}

	*data = blob.data;
	*len = blob.length;
	return NT_STATUS_OK;
}

/*******************************************************************
 Convert a secdesc_buf into a byte stream
********************************************************************/

NTSTATUS marshall_sec_desc_buf(TALLOC_CTX *mem_ctx,
			       struct sec_desc_buf *secdesc_buf,
			       uint8_t **data, size_t *len)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(
		&blob, mem_ctx, secdesc_buf,
		(ndr_push_flags_fn_t)ndr_push_sec_desc_buf);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_push_sec_desc_buf failed: %s\n",
			  ndr_errstr(ndr_err)));
		return ndr_map_error2ntstatus(ndr_err);
	}

	*data = blob.data;
	*len = blob.length;
	return NT_STATUS_OK;
}

/*******************************************************************
 Parse a byte stream into a secdesc
********************************************************************/
NTSTATUS unmarshall_sec_desc(TALLOC_CTX *mem_ctx, uint8 *data, size_t len,
			     struct security_descriptor **psecdesc)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	struct security_descriptor *result;

	if ((data == NULL) || (len == 0)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	result = TALLOC_ZERO_P(mem_ctx, struct security_descriptor);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	blob = data_blob_const(data, len);

	ndr_err = ndr_pull_struct_blob(&blob, result, result,
		(ndr_pull_flags_fn_t)ndr_pull_security_descriptor);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_pull_security_descriptor failed: %s\n",
			  ndr_errstr(ndr_err)));
		TALLOC_FREE(result);
		return ndr_map_error2ntstatus(ndr_err);
	}

	*psecdesc = result;
	return NT_STATUS_OK;
}

/*******************************************************************
 Parse a byte stream into a sec_desc_buf
********************************************************************/

NTSTATUS unmarshall_sec_desc_buf(TALLOC_CTX *mem_ctx, uint8_t *data, size_t len,
				 struct sec_desc_buf **psecdesc_buf)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	struct sec_desc_buf *result;

	if ((data == NULL) || (len == 0)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	result = TALLOC_ZERO_P(mem_ctx, struct sec_desc_buf);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	blob = data_blob_const(data, len);

	ndr_err = ndr_pull_struct_blob(&blob, result, result,
		(ndr_pull_flags_fn_t)ndr_pull_sec_desc_buf);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_pull_sec_desc_buf failed: %s\n",
			  ndr_errstr(ndr_err)));
		TALLOC_FREE(result);
		return ndr_map_error2ntstatus(ndr_err);
	}

	*psecdesc_buf = result;
	return NT_STATUS_OK;
}

/*******************************************************************
 Creates a struct security_descriptor structure with typical defaults.
********************************************************************/

struct security_descriptor *make_standard_sec_desc(TALLOC_CTX *ctx, const struct dom_sid *owner_sid, const struct dom_sid *grp_sid,
				 struct security_acl *dacl, size_t *sd_size)
{
	return make_sec_desc(ctx, SECURITY_DESCRIPTOR_REVISION_1,
			     SEC_DESC_SELF_RELATIVE, owner_sid, grp_sid, NULL,
			     dacl, sd_size);
}

/*******************************************************************
 Creates a struct sec_desc_buf structure.
********************************************************************/

struct sec_desc_buf *make_sec_desc_buf(TALLOC_CTX *ctx, size_t len, struct security_descriptor *sec_desc)
{
	struct sec_desc_buf *dst;

	if((dst = TALLOC_ZERO_P(ctx, struct sec_desc_buf)) == NULL)
		return NULL;

	/* max buffer size (allocated size) */
	dst->sd_size = (uint32)len;
	
	if(sec_desc && ((dst->sd = dup_sec_desc(ctx, sec_desc)) == NULL)) {
		return NULL;
	}

	return dst;
}

/*******************************************************************
 Duplicates a struct sec_desc_buf structure.
********************************************************************/

struct sec_desc_buf *dup_sec_desc_buf(TALLOC_CTX *ctx, struct sec_desc_buf *src)
{
	if(src == NULL)
		return NULL;

	return make_sec_desc_buf( ctx, src->sd_size, src->sd);
}

/*******************************************************************
 Add a new SID with its permissions to struct security_descriptor.
********************************************************************/

NTSTATUS sec_desc_add_sid(TALLOC_CTX *ctx, struct security_descriptor **psd, const struct dom_sid *sid, uint32 mask, size_t *sd_size)
{
	struct security_descriptor *sd   = 0;
	struct security_acl  *dacl = 0;
	struct security_ace  *ace  = 0;
	NTSTATUS  status;

	if (!ctx || !psd || !sid || !sd_size)
		return NT_STATUS_INVALID_PARAMETER;

	*sd_size = 0;

	status = sec_ace_add_sid(ctx, &ace, psd[0]->dacl->aces, &psd[0]->dacl->num_aces, sid, mask);
	
	if (!NT_STATUS_IS_OK(status))
		return status;

	if (!(dacl = make_sec_acl(ctx, psd[0]->dacl->revision, psd[0]->dacl->num_aces, ace)))
		return NT_STATUS_UNSUCCESSFUL;
	
	if (!(sd = make_sec_desc(ctx, psd[0]->revision, psd[0]->type, psd[0]->owner_sid, 
		psd[0]->group_sid, psd[0]->sacl, dacl, sd_size)))
		return NT_STATUS_UNSUCCESSFUL;

	*psd = sd;
	 sd  = 0;
	return NT_STATUS_OK;
}

/*******************************************************************
 Modify a SID's permissions in a struct security_descriptor.
********************************************************************/

NTSTATUS sec_desc_mod_sid(struct security_descriptor *sd, struct dom_sid *sid, uint32 mask)
{
	NTSTATUS status;

	if (!sd || !sid)
		return NT_STATUS_INVALID_PARAMETER;

	status = sec_ace_mod_sid(sd->dacl->aces, sd->dacl->num_aces, sid, mask);

	if (!NT_STATUS_IS_OK(status))
		return status;
	
	return NT_STATUS_OK;
}

/*******************************************************************
 Delete a SID from a struct security_descriptor.
********************************************************************/

NTSTATUS sec_desc_del_sid(TALLOC_CTX *ctx, struct security_descriptor **psd, struct dom_sid *sid, size_t *sd_size)
{
	struct security_descriptor *sd   = 0;
	struct security_acl  *dacl = 0;
	struct security_ace  *ace  = 0;
	NTSTATUS  status;

	if (!ctx || !psd[0] || !sid || !sd_size)
		return NT_STATUS_INVALID_PARAMETER;

	*sd_size = 0;
	
	status = sec_ace_del_sid(ctx, &ace, psd[0]->dacl->aces, &psd[0]->dacl->num_aces, sid);

	if (!NT_STATUS_IS_OK(status))
		return status;

	if (!(dacl = make_sec_acl(ctx, psd[0]->dacl->revision, psd[0]->dacl->num_aces, ace)))
		return NT_STATUS_UNSUCCESSFUL;
	
	if (!(sd = make_sec_desc(ctx, psd[0]->revision, psd[0]->type, psd[0]->owner_sid, 
		psd[0]->group_sid, psd[0]->sacl, dacl, sd_size)))
		return NT_STATUS_UNSUCCESSFUL;

	*psd = sd;
	 sd  = 0;
	return NT_STATUS_OK;
}

/*
 * Determine if an struct security_ace is inheritable
 */

static bool is_inheritable_ace(const struct security_ace *ace,
				bool container)
{
	if (!container) {
		return ((ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT) != 0);
	}

	if (ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT) {
		return true;
	}

	if ((ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT) &&
			!(ace->flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT)) {
		return true;
	}

	return false;
}

/*
 * Does a security descriptor have any inheritable components for
 * the newly created type ?
 */

bool sd_has_inheritable_components(const struct security_descriptor *parent_ctr, bool container)
{
	unsigned int i;
	const struct security_acl *the_acl = parent_ctr->dacl;

	if (the_acl == NULL) {
		return false;
	}

	for (i = 0; i < the_acl->num_aces; i++) {
		const struct security_ace *ace = &the_acl->aces[i];

		if (is_inheritable_ace(ace, container)) {
			return true;
		}
	}
	return false;
}

/* Create a child security descriptor using another security descriptor as
   the parent container.  This child object can either be a container or
   non-container object. */

NTSTATUS se_create_child_secdesc(TALLOC_CTX *ctx,
					struct security_descriptor **ppsd,
					size_t *psize,
					const struct security_descriptor *parent_ctr,
					const struct dom_sid *owner_sid,
					const struct dom_sid *group_sid,
					bool container)
{
	struct security_acl *new_dacl = NULL, *the_acl = NULL;
	struct security_ace *new_ace_list = NULL;
	unsigned int new_ace_list_ndx = 0, i;
	bool set_inherited_flags = (parent_ctr->type & SEC_DESC_DACL_AUTO_INHERITED);

	*ppsd = NULL;
	*psize = 0;

	/* Currently we only process the dacl when creating the child.  The
	   sacl should also be processed but this is left out as sacls are
	   not implemented in Samba at the moment.*/

	the_acl = parent_ctr->dacl;

	if (the_acl->num_aces) {
		if (2*the_acl->num_aces < the_acl->num_aces) {
			return NT_STATUS_NO_MEMORY;
		}

		if (!(new_ace_list = TALLOC_ARRAY(ctx, struct security_ace,
						2*the_acl->num_aces))) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		new_ace_list = NULL;
	}

	for (i = 0; i < the_acl->num_aces; i++) {
		const struct security_ace *ace = &the_acl->aces[i];
		struct security_ace *new_ace = &new_ace_list[new_ace_list_ndx];
		const struct dom_sid *ptrustee = &ace->trustee;
		const struct dom_sid *creator = NULL;
		uint8 new_flags = ace->flags;

		if (!is_inheritable_ace(ace, container)) {
			continue;
		}

		/* see the RAW-ACLS inheritance test for details on these rules */
		if (!container) {
			new_flags = 0;
		} else {
			new_flags &= ~(SEC_ACE_FLAG_INHERIT_ONLY|
					SEC_ACE_FLAG_INHERITED_ACE);

			if (!(new_flags & SEC_ACE_FLAG_CONTAINER_INHERIT)) {
				new_flags |= SEC_ACE_FLAG_INHERIT_ONLY;
			}
			if (new_flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT) {
				new_flags = 0;
			}
		}

		/* The CREATOR sids are special when inherited */
		if (dom_sid_equal(ptrustee, &global_sid_Creator_Owner)) {
			creator = &global_sid_Creator_Owner;
			ptrustee = owner_sid;
		} else if (dom_sid_equal(ptrustee, &global_sid_Creator_Group)) {
			creator = &global_sid_Creator_Group;
			ptrustee = group_sid;
		}

		if (creator && container &&
				(new_flags & SEC_ACE_FLAG_CONTAINER_INHERIT)) {

			/* First add the regular ACE entry. */
			init_sec_ace(new_ace, ptrustee, ace->type,
				ace->access_mask,
				set_inherited_flags ? SEC_ACE_FLAG_INHERITED_ACE : 0);

			DEBUG(5,("se_create_child_secdesc(): %s:%d/0x%02x/0x%08x"
				" inherited as %s:%d/0x%02x/0x%08x\n",
				sid_string_dbg(&ace->trustee),
				ace->type, ace->flags, ace->access_mask,
				sid_string_dbg(&new_ace->trustee),
				new_ace->type, new_ace->flags,
				new_ace->access_mask));

			new_ace_list_ndx++;

			/* Now add the extra creator ACE. */
			new_ace = &new_ace_list[new_ace_list_ndx];

			ptrustee = creator;
			new_flags |= SEC_ACE_FLAG_INHERIT_ONLY;
		} else if (container &&
				!(ace->flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT)) {
			ptrustee = &ace->trustee;
		}

		init_sec_ace(new_ace, ptrustee, ace->type,
				ace->access_mask, new_flags |
				(set_inherited_flags ? SEC_ACE_FLAG_INHERITED_ACE : 0));

		DEBUG(5, ("se_create_child_secdesc(): %s:%d/0x%02x/0x%08x "
			  " inherited as %s:%d/0x%02x/0x%08x\n",
			  sid_string_dbg(&ace->trustee),
			  ace->type, ace->flags, ace->access_mask,
			  sid_string_dbg(&ace->trustee),
			  new_ace->type, new_ace->flags,
			  new_ace->access_mask));

		new_ace_list_ndx++;
	}

	/* Create child security descriptor to return */
	if (new_ace_list_ndx) {
		new_dacl = make_sec_acl(ctx,
				NT4_ACL_REVISION,
				new_ace_list_ndx,
				new_ace_list);

		if (!new_dacl) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	*ppsd = make_sec_desc(ctx,
			SECURITY_DESCRIPTOR_REVISION_1,
			SEC_DESC_SELF_RELATIVE|SEC_DESC_DACL_PRESENT|
			(set_inherited_flags ? SEC_DESC_DACL_AUTO_INHERITED : 0),
			owner_sid,
			group_sid,
			NULL,
			new_dacl,
			psize);
	if (!*ppsd) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

NTSTATUS se_create_child_secdesc_buf(TALLOC_CTX *ctx,
					struct sec_desc_buf **ppsdb,
					const struct security_descriptor *parent_ctr,
					bool container)
{
	NTSTATUS status;
	size_t size = 0;
	struct security_descriptor *sd = NULL;

	*ppsdb = NULL;
	status = se_create_child_secdesc(ctx,
					&sd,
					&size,
					parent_ctr,
					parent_ctr->owner_sid,
					parent_ctr->group_sid,
					container);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*ppsdb = make_sec_desc_buf(ctx, size, sd);
	if (!*ppsdb) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}
