/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - ACL support

   Copyright (C) Andrew Tridgell 2004

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
#include "auth/auth.h"
#include "vfs_posix.h"
#include "librpc/gen_ndr/xattr.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "../lib/util/unix_privs.h"

#if defined(UID_WRAPPER)
#if !defined(UID_WRAPPER_REPLACE) && !defined(UID_WRAPPER_NOT_REPLACE)
#define UID_WRAPPER_REPLACE
#include "../uid_wrapper/uid_wrapper.h"
#endif
#else
#define uwrap_enabled() 0
#endif

/* the list of currently registered ACL backends */
static struct pvfs_acl_backend {
	const struct pvfs_acl_ops *ops;
} *backends = NULL;
static int num_backends;

/*
  register a pvfs acl backend. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.  
*/
NTSTATUS pvfs_acl_register(const struct pvfs_acl_ops *ops)
{
	struct pvfs_acl_ops *new_ops;

	if (pvfs_acl_backend_byname(ops->name) != NULL) {
		DEBUG(0,("pvfs acl backend '%s' already registered\n", ops->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	backends = talloc_realloc(talloc_autofree_context(), backends, struct pvfs_acl_backend, num_backends+1);
	NT_STATUS_HAVE_NO_MEMORY(backends);

	new_ops = (struct pvfs_acl_ops *)talloc_memdup(backends, ops, sizeof(*ops));
	new_ops->name = talloc_strdup(new_ops, ops->name);

	backends[num_backends].ops = new_ops;

	num_backends++;

	DEBUG(3,("NTVFS backend '%s' registered\n", ops->name));

	return NT_STATUS_OK;
}


/*
  return the operations structure for a named backend
*/
const struct pvfs_acl_ops *pvfs_acl_backend_byname(const char *name)
{
	int i;

	for (i=0;i<num_backends;i++) {
		if (strcmp(backends[i].ops->name, name) == 0) {
			return backends[i].ops;
		}
	}

	return NULL;
}

NTSTATUS pvfs_acl_init(struct loadparm_context *lp_ctx)
{
	static bool initialized = false;
#define _MODULE_PROTO(init) extern NTSTATUS init(void);
	STATIC_pvfs_acl_MODULES_PROTO;
	init_module_fn static_init[] = { STATIC_pvfs_acl_MODULES };
	init_module_fn *shared_init;

	if (initialized) return NT_STATUS_OK;
	initialized = true;

	shared_init = load_samba_modules(NULL, lp_ctx, "pvfs_acl");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	return NT_STATUS_OK;
}


/*
  map a single access_mask from generic to specific bits for files/dirs
*/
static uint32_t pvfs_translate_mask(uint32_t access_mask)
{
	if (access_mask & SEC_MASK_GENERIC) {
		if (access_mask & SEC_GENERIC_READ)    access_mask |= SEC_RIGHTS_FILE_READ;
		if (access_mask & SEC_GENERIC_WRITE)   access_mask |= SEC_RIGHTS_FILE_WRITE;
		if (access_mask & SEC_GENERIC_EXECUTE) access_mask |= SEC_RIGHTS_FILE_EXECUTE;
		if (access_mask & SEC_GENERIC_ALL)     access_mask |= SEC_RIGHTS_FILE_ALL;
		access_mask &= ~SEC_MASK_GENERIC;
	}
	return access_mask;
}


/*
  map any generic access bits in the given acl
  this relies on the fact that the mappings for files and directories
  are the same
*/
static void pvfs_translate_generic_bits(struct security_acl *acl)
{
	unsigned i;

	if (!acl) return;

	for (i=0;i<acl->num_aces;i++) {
		struct security_ace *ace = &acl->aces[i];
		ace->access_mask = pvfs_translate_mask(ace->access_mask);
	}
}


/*
  setup a default ACL for a file
*/
static NTSTATUS pvfs_default_acl(struct pvfs_state *pvfs,
				 struct ntvfs_request *req,
				 struct pvfs_filename *name, int fd, 
				 struct security_descriptor **psd)
{
	struct security_descriptor *sd;
	NTSTATUS status;
	struct security_ace ace;
	mode_t mode;
	struct id_map *ids;
	struct composite_context *ctx;

	*psd = security_descriptor_initialise(req);
	if (*psd == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	sd = *psd;

	ids = talloc_zero_array(sd, struct id_map, 2);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	ids[0].xid.id = name->st.st_uid;
	ids[0].xid.type = ID_TYPE_UID;
	ids[0].sid = NULL;

	ids[1].xid.id = name->st.st_gid;
	ids[1].xid.type = ID_TYPE_GID;
	ids[1].sid = NULL;

	ctx = wbc_xids_to_sids_send(pvfs->wbc_ctx, ids, 2, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = wbc_xids_to_sids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	sd->owner_sid = talloc_steal(sd, ids[0].sid);
	sd->group_sid = talloc_steal(sd, ids[1].sid);

	talloc_free(ids);
	sd->type |= SEC_DESC_DACL_PRESENT;

	mode = name->st.st_mode;

	/*
	  we provide up to 4 ACEs
	    - Owner
	    - Group
	    - Everyone
	    - Administrator
	 */


	/* setup owner ACE */
	ace.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace.flags = 0;
	ace.trustee = *sd->owner_sid;
	ace.access_mask = 0;

	if (mode & S_IRUSR) {
		if (mode & S_IWUSR) {
			ace.access_mask |= SEC_RIGHTS_FILE_ALL;
		} else {
			ace.access_mask |= SEC_RIGHTS_FILE_READ | SEC_FILE_EXECUTE;
		}
	}
	if (mode & S_IWUSR) {
		ace.access_mask |= SEC_RIGHTS_FILE_WRITE | SEC_STD_DELETE;
	}
	if (ace.access_mask) {
		security_descriptor_dacl_add(sd, &ace);
	}


	/* setup group ACE */
	ace.trustee = *sd->group_sid;
	ace.access_mask = 0;
	if (mode & S_IRGRP) {
		ace.access_mask |= SEC_RIGHTS_FILE_READ | SEC_FILE_EXECUTE;
	}
	if (mode & S_IWGRP) {
		/* note that delete is not granted - this matches posix behaviour */
		ace.access_mask |= SEC_RIGHTS_FILE_WRITE;
	}
	if (ace.access_mask) {
		security_descriptor_dacl_add(sd, &ace);
	}

	/* setup other ACE */
	ace.trustee = *dom_sid_parse_talloc(req, SID_WORLD);
	ace.access_mask = 0;
	if (mode & S_IROTH) {
		ace.access_mask |= SEC_RIGHTS_FILE_READ | SEC_FILE_EXECUTE;
	}
	if (mode & S_IWOTH) {
		ace.access_mask |= SEC_RIGHTS_FILE_WRITE;
	}
	if (ace.access_mask) {
		security_descriptor_dacl_add(sd, &ace);
	}

	/* setup system ACE */
	ace.trustee = *dom_sid_parse_talloc(req, SID_NT_SYSTEM);
	ace.access_mask = SEC_RIGHTS_FILE_ALL;
	security_descriptor_dacl_add(sd, &ace);
	
	return NT_STATUS_OK;
}
				 

/*
  omit any security_descriptor elements not specified in the given
  secinfo flags
*/
static void normalise_sd_flags(struct security_descriptor *sd, uint32_t secinfo_flags)
{
	if (!(secinfo_flags & SECINFO_OWNER)) {
		sd->owner_sid = NULL;
	}
	if (!(secinfo_flags & SECINFO_GROUP)) {
		sd->group_sid = NULL;
	}
	if (!(secinfo_flags & SECINFO_DACL)) {
		sd->dacl = NULL;
	}
	if (!(secinfo_flags & SECINFO_SACL)) {
		sd->sacl = NULL;
	}
}

/*
  answer a setfileinfo for an ACL
*/
NTSTATUS pvfs_acl_set(struct pvfs_state *pvfs, 
		      struct ntvfs_request *req,
		      struct pvfs_filename *name, int fd, 
		      uint32_t access_mask,
		      union smb_setfileinfo *info)
{
	uint32_t secinfo_flags = info->set_secdesc.in.secinfo_flags;
	struct security_descriptor *new_sd, *sd, orig_sd;
	NTSTATUS status = NT_STATUS_NOT_FOUND;
	uid_t old_uid = -1;
	gid_t old_gid = -1;
	uid_t new_uid = -1;
	gid_t new_gid = -1;
	struct id_map *ids;
	struct composite_context *ctx;

	if (pvfs->acl_ops != NULL) {
		status = pvfs->acl_ops->acl_load(pvfs, name, fd, req, &sd);
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		status = pvfs_default_acl(pvfs, req, name, fd, &sd);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ids = talloc(req, struct id_map);
	NT_STATUS_HAVE_NO_MEMORY(ids);
	ZERO_STRUCT(ids->xid);
	ids->sid = NULL;
	ids->status = ID_UNKNOWN;

	new_sd = info->set_secdesc.in.sd;
	orig_sd = *sd;

	old_uid = name->st.st_uid;
	old_gid = name->st.st_gid;

	/* only set the elements that have been specified */
	if (secinfo_flags & SECINFO_OWNER) {
		if (!(access_mask & SEC_STD_WRITE_OWNER)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		if (!dom_sid_equal(sd->owner_sid, new_sd->owner_sid)) {
			ids->sid = new_sd->owner_sid;
			ctx = wbc_sids_to_xids_send(pvfs->wbc_ctx, ids, 1, ids);
			NT_STATUS_HAVE_NO_MEMORY(ctx);
			status = wbc_sids_to_xids_recv(ctx, &ids);
			NT_STATUS_NOT_OK_RETURN(status);

			if (ids->xid.type == ID_TYPE_BOTH ||
			    ids->xid.type == ID_TYPE_UID) {
				new_uid = ids->xid.id;
			}
		}
		sd->owner_sid = new_sd->owner_sid;
	}
	if (secinfo_flags & SECINFO_GROUP) {
		if (!(access_mask & SEC_STD_WRITE_OWNER)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		if (!dom_sid_equal(sd->group_sid, new_sd->group_sid)) {
			ids->sid = new_sd->group_sid;
			ctx = wbc_sids_to_xids_send(pvfs->wbc_ctx, ids, 1, ids);
			NT_STATUS_HAVE_NO_MEMORY(ctx);
			status = wbc_sids_to_xids_recv(ctx, &ids);
			NT_STATUS_NOT_OK_RETURN(status);

			if (ids->xid.type == ID_TYPE_BOTH ||
			    ids->xid.type == ID_TYPE_GID) {
				new_gid = ids->xid.id;
			}

		}
		sd->group_sid = new_sd->group_sid;
	}
	if (secinfo_flags & SECINFO_DACL) {
		if (!(access_mask & SEC_STD_WRITE_DAC)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		sd->dacl = new_sd->dacl;
		pvfs_translate_generic_bits(sd->dacl);
	}
	if (secinfo_flags & SECINFO_SACL) {
		if (!(access_mask & SEC_FLAG_SYSTEM_SECURITY)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		sd->sacl = new_sd->sacl;
		pvfs_translate_generic_bits(sd->sacl);
	}

	if (new_uid == old_uid) {
		new_uid = -1;
	}

	if (new_gid == old_gid) {
		new_gid = -1;
	}

	/* if there's something to change try it */
	if (new_uid != -1 || new_gid != -1) {
		int ret;
		if (fd == -1) {
			ret = chown(name->full_name, new_uid, new_gid);
		} else {
			ret = fchown(fd, new_uid, new_gid);
		}
		if (errno == EPERM) {
			if (uwrap_enabled()) {
				ret = 0;
			} else {
				/* try again as root if we have SEC_PRIV_RESTORE or
				   SEC_PRIV_TAKE_OWNERSHIP */
				if (security_token_has_privilege(req->session_info->security_token,
								 SEC_PRIV_RESTORE) ||
				    security_token_has_privilege(req->session_info->security_token,
								 SEC_PRIV_TAKE_OWNERSHIP)) {
					void *privs;
					privs = root_privileges();
					if (fd == -1) {
						ret = chown(name->full_name, new_uid, new_gid);
					} else {
						ret = fchown(fd, new_uid, new_gid);
					}
					talloc_free(privs);
				}
			}
		}
		if (ret == -1) {
			return pvfs_map_errno(pvfs, errno);
		}
	}

	/* we avoid saving if the sd is the same. This means when clients
	   copy files and end up copying the default sd that we don't
	   needlessly use xattrs */
	if (!security_descriptor_equal(sd, &orig_sd) && pvfs->acl_ops) {
		status = pvfs->acl_ops->acl_save(pvfs, name, fd, sd);
	}

	return status;
}


/*
  answer a fileinfo query for the ACL
*/
NTSTATUS pvfs_acl_query(struct pvfs_state *pvfs, 
			struct ntvfs_request *req,
			struct pvfs_filename *name, int fd, 
			union smb_fileinfo *info)
{
	NTSTATUS status = NT_STATUS_NOT_FOUND;
	struct security_descriptor *sd;

	if (pvfs->acl_ops) {
		status = pvfs->acl_ops->acl_load(pvfs, name, fd, req, &sd);
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		status = pvfs_default_acl(pvfs, req, name, fd, &sd);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	normalise_sd_flags(sd, info->query_secdesc.in.secinfo_flags);

	info->query_secdesc.out.sd = sd;

	return NT_STATUS_OK;
}


/*
  check the read only bit against any of the write access bits
*/
static bool pvfs_read_only(struct pvfs_state *pvfs, uint32_t access_mask)
{
	if ((pvfs->flags & PVFS_FLAG_READONLY) &&
	    (access_mask & (SEC_FILE_WRITE_DATA |
			    SEC_FILE_APPEND_DATA | 
			    SEC_FILE_WRITE_EA | 
			    SEC_FILE_WRITE_ATTRIBUTE | 
			    SEC_STD_DELETE | 
			    SEC_STD_WRITE_DAC | 
			    SEC_STD_WRITE_OWNER | 
			    SEC_DIR_DELETE_CHILD))) {
		return true;
	}
	return false;
}

/*
  see if we are a member of the appropriate unix group
 */
static bool pvfs_group_member(struct pvfs_state *pvfs, gid_t gid)
{
	int i, ngroups;
	gid_t *groups;
	if (getegid() == gid) {
		return true;
	}
	ngroups = getgroups(0, NULL);
	if (ngroups == 0) {
		return false;
	}
	groups = talloc_array(pvfs, gid_t, ngroups);
	if (groups == NULL) {
		return false;
	}
	if (getgroups(ngroups, groups) != ngroups) {
		talloc_free(groups);
		return false;
	}
	for (i=0; i<ngroups; i++) {
		if (groups[i] == gid) break;
	}
	talloc_free(groups);
	return i < ngroups;
}

/*
  default access check function based on unix permissions
  doing this saves on building a full security descriptor
  for the common case of access check on files with no 
  specific NT ACL

  If name is NULL then treat as a new file creation
*/
static NTSTATUS pvfs_access_check_unix(struct pvfs_state *pvfs,
				       struct ntvfs_request *req,
				       struct pvfs_filename *name,
				       uint32_t *access_mask)
{
	uid_t uid = geteuid();
	uint32_t max_bits = SEC_RIGHTS_FILE_READ | SEC_FILE_ALL;
	struct security_token *token = req->session_info->security_token;

	if (pvfs_read_only(pvfs, *access_mask)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (name == NULL || uid == name->st.st_uid) {
		max_bits |= SEC_STD_ALL;
	} else if (security_token_has_privilege(token, SEC_PRIV_RESTORE)) {
		max_bits |= SEC_STD_DELETE;
	}

	if (name == NULL ||
	    (name->st.st_mode & S_IWOTH) ||
	    ((name->st.st_mode & S_IWGRP) && 
	     pvfs_group_member(pvfs, name->st.st_gid))) {
		max_bits |= SEC_STD_ALL;
	}

	if (uwrap_enabled()) {
		/* when running with the uid wrapper, files will be created
		   owned by the ruid, but we may have a different simulated 
		   euid. We need to force the permission bits as though the 
		   files owner matches the euid */
		max_bits |= SEC_STD_ALL;
	}

	if (*access_mask & SEC_FLAG_MAXIMUM_ALLOWED) {
		*access_mask |= max_bits;
		*access_mask &= ~SEC_FLAG_MAXIMUM_ALLOWED;
	}

	if ((*access_mask & SEC_FLAG_SYSTEM_SECURITY) &&
	    security_token_has_privilege(token, SEC_PRIV_SECURITY)) {
		max_bits |= SEC_FLAG_SYSTEM_SECURITY;
	}
	
	if (((*access_mask & ~max_bits) & SEC_RIGHTS_PRIV_RESTORE) &&
	    security_token_has_privilege(token, SEC_PRIV_RESTORE)) {
		max_bits |= ~(SEC_RIGHTS_PRIV_RESTORE);
	}
	if (((*access_mask & ~max_bits) & SEC_RIGHTS_PRIV_BACKUP) &&
	    security_token_has_privilege(token, SEC_PRIV_BACKUP)) {
		max_bits |= ~(SEC_RIGHTS_PRIV_BACKUP);
	}

	if (*access_mask & ~max_bits) {
		DEBUG(0,(__location__ " denied access to '%s' - wanted 0x%08x but got 0x%08x (missing 0x%08x)\n",
			 name?name->full_name:"(new file)", *access_mask, max_bits, *access_mask & ~max_bits));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (pvfs->ntvfs->ctx->protocol != PROTOCOL_SMB2) {
		/* on SMB, this bit is always granted, even if not
		   asked for */
		*access_mask |= SEC_FILE_READ_ATTRIBUTE;
	}

	return NT_STATUS_OK;
}


/*
  check the security descriptor on a file, if any
  
  *access_mask is modified with the access actually granted
*/
NTSTATUS pvfs_access_check(struct pvfs_state *pvfs, 
			   struct ntvfs_request *req,
			   struct pvfs_filename *name,
			   uint32_t *access_mask)
{
	struct security_token *token = req->session_info->security_token;
	struct xattr_NTACL *acl;
	NTSTATUS status;
	struct security_descriptor *sd;
	bool allow_delete = false;

	/* on SMB2 a blank access mask is always denied */
	if (pvfs->ntvfs->ctx->protocol == PROTOCOL_SMB2 &&
	    *access_mask == 0) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (pvfs_read_only(pvfs, *access_mask)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (*access_mask & SEC_FLAG_MAXIMUM_ALLOWED ||
	    *access_mask & SEC_STD_DELETE) {
		status = pvfs_access_check_parent(pvfs, req,
						  name, SEC_DIR_DELETE_CHILD);
		if (NT_STATUS_IS_OK(status)) {
			allow_delete = true;
			*access_mask &= ~SEC_STD_DELETE;
		}
	}

	acl = talloc(req, struct xattr_NTACL);
	if (acl == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* expand the generic access bits to file specific bits */
	*access_mask = pvfs_translate_mask(*access_mask);
	if (pvfs->ntvfs->ctx->protocol != PROTOCOL_SMB2) {
		*access_mask &= ~SEC_FILE_READ_ATTRIBUTE;
	}

	status = pvfs_acl_load(pvfs, name, -1, acl);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		talloc_free(acl);
		status = pvfs_access_check_unix(pvfs, req, name, access_mask);
		goto done;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	switch (acl->version) {
	case 1:
		sd = acl->info.sd;
		break;
	default:
		return NT_STATUS_INVALID_ACL;
	}

	/* check the acl against the required access mask */
	status = se_access_check(sd, token, *access_mask, access_mask);
	talloc_free(acl);
done:
	if (pvfs->ntvfs->ctx->protocol != PROTOCOL_SMB2) {
		/* on SMB, this bit is always granted, even if not
		   asked for */
		*access_mask |= SEC_FILE_READ_ATTRIBUTE;
	}

	if (allow_delete) {
		*access_mask |= SEC_STD_DELETE;
	}

	return status;
}


/*
  a simplified interface to access check, designed for calls that
  do not take or return an access check mask
*/
NTSTATUS pvfs_access_check_simple(struct pvfs_state *pvfs, 
				  struct ntvfs_request *req,
				  struct pvfs_filename *name,
				  uint32_t access_needed)
{
	if (access_needed == 0) {
		return NT_STATUS_OK;
	}
	return pvfs_access_check(pvfs, req, name, &access_needed);
}

/*
  access check for creating a new file/directory
*/
NTSTATUS pvfs_access_check_create(struct pvfs_state *pvfs, 
				  struct ntvfs_request *req,
				  struct pvfs_filename *name,
				  uint32_t *access_mask,
				  bool container,
				  struct security_descriptor **sd)
{
	struct pvfs_filename *parent;
	NTSTATUS status;
	uint32_t parent_mask;
	bool allow_delete = false;

	if (pvfs_read_only(pvfs, *access_mask)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = pvfs_resolve_parent(pvfs, req, name, &parent);
	NT_STATUS_NOT_OK_RETURN(status);

	if (container) {
		parent_mask = SEC_DIR_ADD_SUBDIR;
	} else {
		parent_mask = SEC_DIR_ADD_FILE;
	}
	if (*access_mask & SEC_FLAG_MAXIMUM_ALLOWED ||
	    *access_mask & SEC_STD_DELETE) {
		parent_mask |= SEC_DIR_DELETE_CHILD;
	}

	status = pvfs_access_check(pvfs, req, parent, &parent_mask);
	if (NT_STATUS_IS_OK(status)) {
		if (parent_mask & SEC_DIR_DELETE_CHILD) {
			allow_delete = true;
		}
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		/*
		 * on ACCESS_DENIED we get the rejected bits
		 * remove the non critical SEC_DIR_DELETE_CHILD
		 * and check if something else was rejected.
		 */
		parent_mask &= ~SEC_DIR_DELETE_CHILD;
		if (parent_mask != 0) {
			return NT_STATUS_ACCESS_DENIED;
		}
		status = NT_STATUS_OK;
	} else {
		return status;
	}

	if (*sd == NULL) {
		status = pvfs_acl_inherited_sd(pvfs, req, req, parent, container, sd);
	}

	talloc_free(parent);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* expand the generic access bits to file specific bits */
	*access_mask = pvfs_translate_mask(*access_mask);

	if (*access_mask & SEC_FLAG_MAXIMUM_ALLOWED) {
		*access_mask |= SEC_RIGHTS_FILE_ALL;
		*access_mask &= ~SEC_FLAG_MAXIMUM_ALLOWED;
	}

	if (pvfs->ntvfs->ctx->protocol != PROTOCOL_SMB2) {
		/* on SMB, this bit is always granted, even if not
		   asked for */
		*access_mask |= SEC_FILE_READ_ATTRIBUTE;
	}

	if (allow_delete) {
		*access_mask |= SEC_STD_DELETE;
	}

	return NT_STATUS_OK;
}

/*
  access check for creating a new file/directory - no access mask supplied
*/
NTSTATUS pvfs_access_check_parent(struct pvfs_state *pvfs, 
				  struct ntvfs_request *req,
				  struct pvfs_filename *name,
				  uint32_t access_mask)
{
	struct pvfs_filename *parent;
	NTSTATUS status;

	status = pvfs_resolve_parent(pvfs, req, name, &parent);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return pvfs_access_check_simple(pvfs, req, parent, access_mask);
}


/*
  determine if an ACE is inheritable
*/
static bool pvfs_inheritable_ace(struct pvfs_state *pvfs,
				 const struct security_ace *ace,
				 bool container)
{
	if (!container) {
		return (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT) != 0;
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
  this is the core of ACL inheritance. It copies any inheritable
  aces from the parent SD to the child SD. Note that the algorithm 
  depends on whether the child is a container or not
*/
static NTSTATUS pvfs_acl_inherit_aces(struct pvfs_state *pvfs, 
				      struct security_descriptor *parent_sd,
				      struct security_descriptor *sd,
				      bool container)
{
	int i;
	
	for (i=0;i<parent_sd->dacl->num_aces;i++) {
		struct security_ace ace = parent_sd->dacl->aces[i];
		NTSTATUS status;
		const struct dom_sid *creator = NULL, *new_id = NULL;
		uint32_t orig_flags;

		if (!pvfs_inheritable_ace(pvfs, &ace, container)) {
			continue;
		}

		orig_flags = ace.flags;

		/* see the RAW-ACLS inheritance test for details on these rules */
		if (!container) {
			ace.flags = 0;
		} else {
			ace.flags &= ~SEC_ACE_FLAG_INHERIT_ONLY;

			if (!(ace.flags & SEC_ACE_FLAG_CONTAINER_INHERIT)) {
				ace.flags |= SEC_ACE_FLAG_INHERIT_ONLY;
			}
			if (ace.flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT) {
				ace.flags = 0;
			}
		}

		/* the CREATOR sids are special when inherited */
		if (dom_sid_equal(&ace.trustee, pvfs->sid_cache.creator_owner)) {
			creator = pvfs->sid_cache.creator_owner;
			new_id = sd->owner_sid;
		} else if (dom_sid_equal(&ace.trustee, pvfs->sid_cache.creator_group)) {
			creator = pvfs->sid_cache.creator_group;
			new_id = sd->group_sid;
		} else {
			new_id = &ace.trustee;
		}

		if (creator && container && 
		    (ace.flags & SEC_ACE_FLAG_CONTAINER_INHERIT)) {
			uint32_t flags = ace.flags;

			ace.trustee = *new_id;
			ace.flags = 0;
			status = security_descriptor_dacl_add(sd, &ace);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}

			ace.trustee = *creator;
			ace.flags = flags | SEC_ACE_FLAG_INHERIT_ONLY;
			status = security_descriptor_dacl_add(sd, &ace);
		} else if (container && 
			   !(orig_flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT)) {
			status = security_descriptor_dacl_add(sd, &ace);
		} else {
			ace.trustee = *new_id;
			status = security_descriptor_dacl_add(sd, &ace);
		}

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	return NT_STATUS_OK;
}



/*
  calculate the ACL on a new file/directory based on the inherited ACL
  from the parent. If there is no inherited ACL then return a NULL
  ACL, which means the default ACL should be used
*/
NTSTATUS pvfs_acl_inherited_sd(struct pvfs_state *pvfs, 
			       TALLOC_CTX *mem_ctx,
			       struct ntvfs_request *req,
			       struct pvfs_filename *parent,
			       bool container,
			       struct security_descriptor **ret_sd)
{
	struct xattr_NTACL *acl;
	NTSTATUS status;
	struct security_descriptor *parent_sd, *sd;
	struct id_map *ids;
	struct composite_context *ctx;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	*ret_sd = NULL;

	acl = talloc(req, struct xattr_NTACL);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(acl, tmp_ctx);

	status = pvfs_acl_load(pvfs, parent, -1, acl);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}
	NT_STATUS_NOT_OK_RETURN_AND_FREE(status, tmp_ctx);

	switch (acl->version) {
	case 1:
		parent_sd = acl->info.sd;
		break;
	default:
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_ACL;
	}

	if (parent_sd == NULL ||
	    parent_sd->dacl == NULL ||
	    parent_sd->dacl->num_aces == 0) {
		/* go with the default ACL */
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	/* create the new sd */
	sd = security_descriptor_initialise(req);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sd, tmp_ctx);

	ids = talloc_array(sd, struct id_map, 2);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(ids, tmp_ctx);

	ids[0].xid.id = geteuid();
	ids[0].xid.type = ID_TYPE_UID;
	ids[0].sid = NULL;
	ids[0].status = ID_UNKNOWN;

	ids[1].xid.id = getegid();
	ids[1].xid.type = ID_TYPE_GID;
	ids[1].sid = NULL;
	ids[1].status = ID_UNKNOWN;

	ctx = wbc_xids_to_sids_send(pvfs->wbc_ctx, ids, 2, ids);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(ctx, tmp_ctx);

	status = wbc_xids_to_sids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN_AND_FREE(status, tmp_ctx);

	sd->owner_sid = talloc_steal(sd, ids[0].sid);
	sd->group_sid = talloc_steal(sd, ids[1].sid);

	sd->type |= SEC_DESC_DACL_PRESENT;

	/* fill in the aces from the parent */
	status = pvfs_acl_inherit_aces(pvfs, parent_sd, sd, container);
	NT_STATUS_NOT_OK_RETURN_AND_FREE(status, tmp_ctx);

	/* if there is nothing to inherit then we fallback to the
	   default acl */
	if (sd->dacl == NULL || sd->dacl->num_aces == 0) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	*ret_sd = talloc_steal(mem_ctx, sd);

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}


/*
  setup an ACL on a new file/directory based on the inherited ACL from
  the parent. If there is no inherited ACL then we don't set anything,
  as the default ACL applies anyway
*/
NTSTATUS pvfs_acl_inherit(struct pvfs_state *pvfs, 
			  struct ntvfs_request *req,
			  struct pvfs_filename *name,
			  int fd)
{
	struct xattr_NTACL acl;
	NTSTATUS status;
	struct security_descriptor *sd;
	struct pvfs_filename *parent;
	bool container;

	/* form the parents path */
	status = pvfs_resolve_parent(pvfs, req, name, &parent);
	NT_STATUS_NOT_OK_RETURN(status);

	container = (name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY) ? true:false;

	status = pvfs_acl_inherited_sd(pvfs, req, req, parent, container, &sd);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(parent);
		return status;
	}

	if (sd == NULL) {
		return NT_STATUS_OK;
	}

	acl.version = 1;
	acl.info.sd = sd;

	status = pvfs_acl_save(pvfs, name, fd, &acl);
	talloc_free(sd);
	talloc_free(parent);

	return status;
}

/*
  return the maximum allowed access mask
*/
NTSTATUS pvfs_access_maximal_allowed(struct pvfs_state *pvfs, 
				     struct ntvfs_request *req,
				     struct pvfs_filename *name,
				     uint32_t *maximal_access)
{
	*maximal_access = SEC_FLAG_MAXIMUM_ALLOWED;
	return pvfs_access_check(pvfs, req, name, maximal_access);
}
