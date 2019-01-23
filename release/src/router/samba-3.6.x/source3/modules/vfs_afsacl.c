/* 
 * Convert AFS acls to NT acls and vice versa.
 *
 * Copyright (C) Volker Lendecke, 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "../librpc/gen_ndr/lsa.h"
#include "../libcli/security/security.h"
#include "../libcli/security/dom_sid.h"
#include "passdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#include <afs/stds.h>
#include <afs/afs.h>
#include <afs/auth.h>
#include <afs/venus.h>
#include <afs/prs_fs.h>

#define MAXSIZE 2048

extern const struct dom_sid global_sid_World;
extern const struct dom_sid global_sid_Builtin_Administrators;
extern const struct dom_sid global_sid_Builtin_Backup_Operators;
extern const struct dom_sid global_sid_Authenticated_Users;
extern const struct dom_sid global_sid_NULL;

static char space_replacement = '%';

/* Do we expect SIDs as pts names? */
static bool sidpts;

extern int afs_syscall(int, char *, int, char *, int);

struct afs_ace {
	bool positive;
	char *name;
	struct dom_sid sid;
	enum lsa_SidType type;
	uint32 rights;
	struct afs_ace *next;
};

struct afs_acl {
	TALLOC_CTX *ctx;
	int type;
	int num_aces;
	struct afs_ace *acelist;
};

struct afs_iob {
	char *in, *out;
	uint16 in_size, out_size;
};


static bool init_afs_acl(struct afs_acl *acl)
{
	ZERO_STRUCT(*acl);
	acl->ctx = talloc_init("afs_acl");
	if (acl->ctx == NULL) {
		DEBUG(10, ("Could not init afs_acl"));
		return False;
	}
	return True;
}

static void free_afs_acl(struct afs_acl *acl)
{
	if (acl->ctx != NULL)
		talloc_destroy(acl->ctx);
	acl->ctx = NULL;
	acl->num_aces = 0;
	acl->acelist = NULL;
}

static struct afs_ace *clone_afs_ace(TALLOC_CTX *mem_ctx, struct afs_ace *ace)
{
	struct afs_ace *result = TALLOC_P(mem_ctx, struct afs_ace);

	if (result == NULL)
		return NULL;

	*result = *ace;

	result->next = NULL;
	result->name = talloc_strdup(mem_ctx, ace->name);

	if (result->name == NULL) {
		return NULL;
	}

	return result;
}
	
static struct afs_ace *new_afs_ace(TALLOC_CTX *mem_ctx,
				   bool positive,
				   const char *name, uint32 rights)
{
	struct dom_sid sid;
	enum lsa_SidType type;
	struct afs_ace *result;

	if (strcmp(name, "system:administrators") == 0) {

		sid_copy(&sid, &global_sid_Builtin_Administrators);
		type = SID_NAME_ALIAS;

	} else if (strcmp(name, "system:anyuser") == 0) {

		sid_copy(&sid, &global_sid_World);
		type = SID_NAME_ALIAS;

	} else if (strcmp(name, "system:authuser") == 0) {

		sid_copy(&sid, &global_sid_Authenticated_Users);
		type = SID_NAME_WKN_GRP;

	} else if (strcmp(name, "system:backup") == 0) {

		sid_copy(&sid, &global_sid_Builtin_Backup_Operators);
		type = SID_NAME_ALIAS;

	} else if (sidpts) {
		/* All PTS users/groups are expressed as SIDs */

		sid_copy(&sid, &global_sid_NULL);
		type = SID_NAME_UNKNOWN;

		if (string_to_sid(&sid, name)) {
			const char *user, *domain;
			/* We have to find the type, look up the SID */
			lookup_sid(talloc_tos(), &sid,
				   &domain, &user, &type);
		}

	} else {

		const char *domain, *uname;
		char *p;

		p = strchr_m(name, *lp_winbind_separator());
		if (p != NULL) {
			*p = '\\';
		}

		if (!lookup_name(talloc_tos(), name, LOOKUP_NAME_ALL,
				 &domain, &uname, &sid, &type)) {
			DEBUG(10, ("Could not find AFS user %s\n", name));

			sid_copy(&sid, &global_sid_NULL);
			type = SID_NAME_UNKNOWN;

		}
	}

	result = TALLOC_P(mem_ctx, struct afs_ace);

	if (result == NULL) {
		DEBUG(0, ("Could not talloc AFS ace\n"));
		return NULL;
	}

	result->name = talloc_strdup(mem_ctx, name);
	if (result->name == NULL) {
		DEBUG(0, ("Could not talloc AFS ace name\n"));
		return NULL;
	}

	result->sid = sid;
	result->type = type;

	result->positive = positive;
	result->rights = rights;

	return result;
}

static void add_afs_ace(struct afs_acl *acl,
			bool positive,
			const char *name, uint32 rights)
{
	struct afs_ace *ace;

	for (ace = acl->acelist; ace != NULL; ace = ace->next) {
		if ((ace->positive == positive) &&
		    (strequal(ace->name, name))) {
			ace->rights |= rights;
			return;
		}
	}

	ace = new_afs_ace(acl->ctx, positive, name, rights);

	ace->next = acl->acelist;
	acl->acelist = ace;

	acl->num_aces += 1;

	DEBUG(10, ("add_afs_ace: Added %s entry for %s with rights %d\n",
		   ace->positive?"positive":"negative",
		   ace->name, ace->rights));

	return;
}

/* AFS ACLs in string form are a long string of fields delimited with \n.
 *
 * First line:  Number of positive entries
 * Second line: Number of negative entries
 * Third and following lines: The entries themselves
 *
 * An ACE is a line of two fields, delimited by \t.
 *
 * First field:  Name
 * Second field: Rights
 */

static bool parse_afs_acl(struct afs_acl *acl, const char *acl_str)
{
	int nplus, nminus;
	int aces;

	char str[MAXSIZE+1];
	char *p = str;

	strncpy(str, acl_str, MAXSIZE);

	if (sscanf(p, "%d", &nplus) != 1)
		return False;

	DEBUG(10, ("Found %d positive entries\n", nplus));

	if ((p = strchr(p, '\n')) == NULL)
		return False;
	p += 1;

	if (sscanf(p, "%d", &nminus) != 1)
		return False;
	
	DEBUG(10, ("Found %d negative entries\n", nminus));

	if ((p = strchr(p, '\n')) == NULL)
		return False;
	p += 1;

	for (aces = nplus+nminus; aces > 0; aces--)
	{

		const char *namep;
		fstring name;
		uint32 rights;
		char *space;

		namep = p;

		if ((p = strchr(p, '\t')) == NULL)
			return False;
		*p = '\0';
		p += 1;

		if (sscanf(p, "%d", &rights) != 1)
			return False;

		if ((p = strchr(p, '\n')) == NULL)
			return False;
		p += 1;

		fstrcpy(name, namep);

		while ((space = strchr_m(name, space_replacement)) != NULL)
			*space = ' ';

		add_afs_ace(acl, nplus>0, name, rights);

		nplus -= 1;
	}

	return True;
}

static bool unparse_afs_acl(struct afs_acl *acl, char *acl_str)
{
	/* TODO: String length checks!!!! */

	int positives = 0;
	int negatives = 0;
	fstring line;

	*acl_str = 0;

	struct afs_ace *ace = acl->acelist;

	while (ace != NULL) {
		if (ace->positive)
			positives++;
		else
			negatives++;
		ace = ace->next;
	}

	fstr_sprintf(line, "%d\n", positives);
	safe_strcat(acl_str, line, MAXSIZE);

	fstr_sprintf(line, "%d\n", negatives);
	safe_strcat(acl_str, line, MAXSIZE);

	ace = acl->acelist;

	while (ace != NULL) {
		fstr_sprintf(line, "%s\t%d\n", ace->name, ace->rights);
		safe_strcat(acl_str, line, MAXSIZE);
		ace = ace->next;
	}
	return True;
}

static uint32 afs_to_nt_file_rights(uint32 rights)
{
	uint32 result = 0;

	if (rights & PRSFS_READ)
		result |= FILE_READ_DATA | FILE_READ_EA | 
			FILE_EXECUTE | FILE_READ_ATTRIBUTES |
			READ_CONTROL_ACCESS | SYNCHRONIZE_ACCESS;

	if (rights & PRSFS_WRITE)
		result |= FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES |
			FILE_WRITE_EA | FILE_APPEND_DATA;

	if (rights & PRSFS_LOCK)
		result |= WRITE_OWNER_ACCESS;

	if (rights & PRSFS_DELETE)
		result |= DELETE_ACCESS;

	return result;
}

static void afs_to_nt_dir_rights(uint32 afs_rights, uint32 *nt_rights,
				 uint8 *flag)
{
	*nt_rights = 0;
	*flag = SEC_ACE_FLAG_OBJECT_INHERIT |
		SEC_ACE_FLAG_CONTAINER_INHERIT;

	if (afs_rights & PRSFS_INSERT)
		*nt_rights |= FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY;

	if (afs_rights & PRSFS_LOOKUP)
		*nt_rights |= FILE_READ_DATA | FILE_READ_EA | 
			FILE_EXECUTE | FILE_READ_ATTRIBUTES |
			READ_CONTROL_ACCESS | SYNCHRONIZE_ACCESS;

	if (afs_rights & PRSFS_WRITE)
		*nt_rights |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA |
			FILE_APPEND_DATA | FILE_WRITE_EA;

	if ((afs_rights & (PRSFS_INSERT|PRSFS_LOOKUP|PRSFS_DELETE)) ==
	    (PRSFS_INSERT|PRSFS_LOOKUP|PRSFS_DELETE))
		*nt_rights |= FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA |
			GENERIC_WRITE_ACCESS;

	if (afs_rights & PRSFS_DELETE)
		*nt_rights |= DELETE_ACCESS;

	if (afs_rights & PRSFS_ADMINISTER)
		*nt_rights |= FILE_DELETE_CHILD | WRITE_DAC_ACCESS |
			WRITE_OWNER_ACCESS;

	if ( (afs_rights & PRSFS_LOOKUP) ==
	     (afs_rights & (PRSFS_LOOKUP|PRSFS_READ)) ) {
		/* Only lookup right */
		*flag = SEC_ACE_FLAG_CONTAINER_INHERIT;
	}

	return;
}

#define AFS_FILE_RIGHTS (PRSFS_READ|PRSFS_WRITE|PRSFS_LOCK)
#define AFS_DIR_RIGHTS  (PRSFS_INSERT|PRSFS_LOOKUP|PRSFS_DELETE|PRSFS_ADMINISTER)

static void split_afs_acl(struct afs_acl *acl,
			  struct afs_acl *dir_acl,
			  struct afs_acl *file_acl)
{
	struct afs_ace *ace;

	init_afs_acl(dir_acl);
	init_afs_acl(file_acl);

	for (ace = acl->acelist; ace != NULL; ace = ace->next) {
		if (ace->rights & AFS_FILE_RIGHTS) {
			add_afs_ace(file_acl, ace->positive, ace->name,
				    ace->rights & AFS_FILE_RIGHTS);
		}

		if (ace->rights & AFS_DIR_RIGHTS) {
			add_afs_ace(dir_acl, ace->positive, ace->name,
				    ace->rights & AFS_DIR_RIGHTS);
		}
	}
	return;
}

static bool same_principal(struct afs_ace *x, struct afs_ace *y)
{
	return ( (x->positive == y->positive) &&
		 (dom_sid_compare(&x->sid, &y->sid) == 0) );
}

static void merge_afs_acls(struct afs_acl *dir_acl,
			   struct afs_acl *file_acl,
			   struct afs_acl *target)
{
	struct afs_ace *ace;

	init_afs_acl(target);

	for (ace = dir_acl->acelist; ace != NULL; ace = ace->next) {
		struct afs_ace *file_ace;
		bool found = False;

		for (file_ace = file_acl->acelist;
		     file_ace != NULL;
		     file_ace = file_ace->next) {
			if (!same_principal(ace, file_ace))
				continue;

			add_afs_ace(target, ace->positive, ace->name,
				    ace->rights | file_ace->rights);
			found = True;
			break;
		}
		if (!found)
			add_afs_ace(target, ace->positive, ace->name,
				    ace->rights);
	}

	for (ace = file_acl->acelist; ace != NULL; ace = ace->next) {
		struct afs_ace *dir_ace;
		bool already_seen = False;

		for (dir_ace = dir_acl->acelist;
		     dir_ace != NULL;
		     dir_ace = dir_ace->next) {
			if (!same_principal(ace, dir_ace))
				continue;
			already_seen = True;
			break;
		}
		if (!already_seen)
			add_afs_ace(target, ace->positive, ace->name,
				    ace->rights);
	}
}

#define PERMS_READ   0x001200a9
#define PERMS_CHANGE 0x001301bf
#define PERMS_FULL   0x001f01ff

static struct static_dir_ace_mapping {
	uint8 type;
	uint8 flags;
	uint32 mask;
	uint32 afs_rights;
} ace_mappings[] = {

	/* Full control */
	{ 0, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PERMS_FULL, 127 /* rlidwka */ },

	/* Change (write) */
	{ 0, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PERMS_CHANGE, 63 /* rlidwk */ },

	/* Read (including list folder content) */
	{ 0, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PERMS_READ, 9 /* rl */ },

	/* Read without list folder content -- same as "l" */
	{ 0, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT,
	  0x00120089, 8 /* l */ },

	/* some stupid workaround for preventing fallbacks */ 	
	{ 0, 0x3, 0x0012019F, 9 /* rl */ },
	{ 0, 0x13, PERMS_FULL, 127 /* full */ },
	
	/* read, delete and execute access plus synchronize */
	{ 0, 0x3, 0x001300A9, 9 /* should be rdl, set to rl */},
	/* classical read list */
	{ 0, 0x13, 0x001200A9, 9 /* rl */},
	/* almost full control, no delete */
	{ 0, 0x13, PERMS_CHANGE, 63 /* rwidlk */},

	/* List folder */
	{ 0, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PERMS_READ, 8 /* l */ },

	/* FULL without inheritance -- in all cases here we also get
	   the corresponding INHERIT_ONLY ACE in the same ACL */
	{ 0, 0, PERMS_FULL, 127 /* rlidwka */ },

	/* FULL inherit only -- counterpart to previous one */
	{ 0, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_INHERIT_ONLY,
	  PERMS_FULL | SEC_GENERIC_WRITE, 127 /* rlidwka */ },

	/* CHANGE without inheritance -- in all cases here we also get
	   the corresponding INHERIT_ONLY ACE in the same ACL */
	{ 0, 0, PERMS_CHANGE, 63 /* rlidwk */ },

	/* CHANGE inherit only -- counterpart to previous one */
	{ 0, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_INHERIT_ONLY,
	  PERMS_CHANGE | SEC_GENERIC_WRITE, 63 /* rlidwk */ },

	/* End marker, hopefully there's no afs right 9999 :-) */
	{ 0, 0, 0, 9999 }
};

static uint32 nt_to_afs_dir_rights(const char *filename, const struct security_ace *ace)
{
	uint32 result = 0;
	uint32 rights = ace->access_mask;
	uint8 flags = ace->flags;

	struct static_dir_ace_mapping *m;

	for (m = &ace_mappings[0]; m->afs_rights != 9999; m++) {
		if ( (ace->type == m->type) &&
		     (ace->flags == m->flags) &&
		     (ace->access_mask == m->mask) )
			return m->afs_rights;
	}

	DEBUG(1, ("AFSACL FALLBACK: 0x%X 0x%X 0x%X %s %X\n",
		  ace->type, ace->flags, ace->access_mask, filename, rights));

	if (rights & (GENERIC_ALL_ACCESS|WRITE_DAC_ACCESS)) {
		result |= PRSFS_READ | PRSFS_WRITE | PRSFS_INSERT |
			PRSFS_LOOKUP | PRSFS_DELETE | PRSFS_LOCK |
			PRSFS_ADMINISTER;
	}

	if (rights & (GENERIC_READ_ACCESS|FILE_READ_DATA)) {
		result |= PRSFS_LOOKUP;
		if (flags & SEC_ACE_FLAG_OBJECT_INHERIT) {
			result |= PRSFS_READ;
		}
	}

	if (rights & (GENERIC_WRITE_ACCESS|FILE_WRITE_DATA)) {
		result |= PRSFS_INSERT | PRSFS_DELETE;
		if (flags & SEC_ACE_FLAG_OBJECT_INHERIT) {
			result |= PRSFS_WRITE | PRSFS_LOCK;
		}
	}

	return result;
}

static uint32 nt_to_afs_file_rights(const char *filename, const struct security_ace *ace)
{
	uint32 result = 0;
	uint32 rights = ace->access_mask;

	if (rights & (GENERIC_READ_ACCESS|FILE_READ_DATA)) {
		result |= PRSFS_READ;
	}

	if (rights & (GENERIC_WRITE_ACCESS|FILE_WRITE_DATA)) {
		result |= PRSFS_WRITE | PRSFS_LOCK;
	}

	return result;
}

static size_t afs_to_nt_acl_common(struct afs_acl *afs_acl,
				   SMB_STRUCT_STAT *psbuf,
				   uint32 security_info,
				   struct security_descriptor **ppdesc)
{
	struct security_ace *nt_ace_list;
	struct dom_sid owner_sid, group_sid;
	struct security_acl *psa = NULL;
	int good_aces;
	size_t sd_size;
	TALLOC_CTX *mem_ctx = talloc_tos();

	struct afs_ace *afs_ace;

	uid_to_sid(&owner_sid, psbuf->st_ex_uid);
	gid_to_sid(&group_sid, psbuf->st_ex_gid);

	if (afs_acl->num_aces) {
		nt_ace_list = TALLOC_ARRAY(mem_ctx, struct security_ace, afs_acl->num_aces);

		if (nt_ace_list == NULL)
			return 0;
	} else {
		nt_ace_list = NULL;
	}

	afs_ace = afs_acl->acelist;
	good_aces = 0;

	while (afs_ace != NULL) {
		uint32_t nt_rights;
		uint8 flag = SEC_ACE_FLAG_OBJECT_INHERIT |
			SEC_ACE_FLAG_CONTAINER_INHERIT;

		if (afs_ace->type == SID_NAME_UNKNOWN) {
			DEBUG(10, ("Ignoring unknown name %s\n",
				   afs_ace->name));
			afs_ace = afs_ace->next;
			continue;
		}

		if (S_ISDIR(psbuf->st_ex_mode))
			afs_to_nt_dir_rights(afs_ace->rights, &nt_rights,
					     &flag);
		else
			nt_rights = afs_to_nt_file_rights(afs_ace->rights);

		init_sec_ace(&nt_ace_list[good_aces++], &(afs_ace->sid),
			     SEC_ACE_TYPE_ACCESS_ALLOWED, nt_rights, flag);
		afs_ace = afs_ace->next;
	}

	psa = make_sec_acl(mem_ctx, NT4_ACL_REVISION,
			   good_aces, nt_ace_list);
	if (psa == NULL)
		return 0;

	*ppdesc = make_sec_desc(mem_ctx, SD_REVISION,
				SEC_DESC_SELF_RELATIVE,
				(security_info & SECINFO_OWNER)
				? &owner_sid : NULL,
				(security_info & SECINFO_GROUP)
				? &group_sid : NULL,
				NULL, psa, &sd_size);

	return sd_size;
}

static size_t afs_to_nt_acl(struct afs_acl *afs_acl,
			    struct connection_struct *conn,
			    struct smb_filename *smb_fname,
			    uint32 security_info,
			    struct security_descriptor **ppdesc)
{
	int ret;

	/* Get the stat struct for the owner info. */
	if (lp_posix_pathnames()) {
		ret = SMB_VFS_LSTAT(conn, smb_fname);
	} else {
		ret = SMB_VFS_STAT(conn, smb_fname);
	}
	if (ret == -1) {
		return 0;
	}

	return afs_to_nt_acl_common(afs_acl, &smb_fname->st, security_info,
				    ppdesc);
}

static size_t afs_fto_nt_acl(struct afs_acl *afs_acl,
			     struct files_struct *fsp,
			     uint32 security_info,
			     struct security_descriptor **ppdesc)
{
	SMB_STRUCT_STAT sbuf;

	if (fsp->fh->fd == -1) {
		/* Get the stat struct for the owner info. */
		return afs_to_nt_acl(afs_acl, fsp->conn, fsp->fsp_name,
				     security_info, ppdesc);
	}

	if(SMB_VFS_FSTAT(fsp, &sbuf) != 0) {
		return 0;
	}

	return afs_to_nt_acl_common(afs_acl, &sbuf, security_info, ppdesc);
}

static bool mappable_sid(const struct dom_sid *sid)
{
	struct dom_sid domain_sid;
	
	if (dom_sid_compare(sid, &global_sid_Builtin_Administrators) == 0)
		return True;

	if (dom_sid_compare(sid, &global_sid_World) == 0)
		return True;

	if (dom_sid_compare(sid, &global_sid_Authenticated_Users) == 0)
		return True;

	if (dom_sid_compare(sid, &global_sid_Builtin_Backup_Operators) == 0)
		return True;

	string_to_sid(&domain_sid, "S-1-5-21");

	if (sid_compare_domain(sid, &domain_sid) == 0)
		return True;

	return False;
}

static bool nt_to_afs_acl(const char *filename,
			  uint32 security_info_sent,
			  const struct security_descriptor *psd,
			  uint32 (*nt_to_afs_rights)(const char *filename,
						     const struct security_ace *ace),
			  struct afs_acl *afs_acl)
{
	const struct security_acl *dacl;
	int i;

	/* Currently we *only* look at the dacl */

	if (((security_info_sent & SECINFO_DACL) == 0) ||
	    (psd->dacl == NULL))
		return True;

	if (!init_afs_acl(afs_acl))
		return False;

	dacl = psd->dacl;

	for (i = 0; i < dacl->num_aces; i++) {
		const struct security_ace *ace = &(dacl->aces[i]);
		const char *dom_name, *name;
		enum lsa_SidType name_type;
		char *p;

		if (ace->type != SEC_ACE_TYPE_ACCESS_ALLOWED) {
			/* First cut: Only positive ACEs */
			return False;
		}

		if (!mappable_sid(&ace->trustee)) {
			DEBUG(10, ("Ignoring unmappable SID %s\n",
				   sid_string_dbg(&ace->trustee)));
			continue;
		}

		if (dom_sid_compare(&ace->trustee,
				&global_sid_Builtin_Administrators) == 0) {

			name = "system:administrators";

		} else if (dom_sid_compare(&ace->trustee,
				       &global_sid_World) == 0) {

			name = "system:anyuser";

		} else if (dom_sid_compare(&ace->trustee,
				       &global_sid_Authenticated_Users) == 0) {

			name = "system:authuser";

		} else if (dom_sid_compare(&ace->trustee,
				       &global_sid_Builtin_Backup_Operators)
			   == 0) {

			name = "system:backup";

		} else {

			if (!lookup_sid(talloc_tos(), &ace->trustee,
					&dom_name, &name, &name_type)) {
				DEBUG(1, ("AFSACL: Could not lookup SID %s on file %s\n",
					  sid_string_dbg(&ace->trustee),
					  filename));
				continue;
			}

			if ( (name_type == SID_NAME_USER) ||
			     (name_type == SID_NAME_DOM_GRP) ||
			     (name_type == SID_NAME_ALIAS) ) {
				char *tmp;
				tmp = talloc_asprintf(talloc_tos(), "%s%s%s",
						       dom_name, lp_winbind_separator(),
						       name);
				if (tmp == NULL) {
					return False;
				}
				strlower_m(tmp);
				name = tmp;
			}

			if (sidpts) {
				/* Expect all users/groups in pts as SIDs */
				name = talloc_strdup(
					talloc_tos(),
					sid_string_tos(&ace->trustee));
				if (name == NULL) {
					return False;
				}
			}
		}

		while ((p = strchr_m(name, ' ')) != NULL)
			*p = space_replacement;

		add_afs_ace(afs_acl, True, name,
			    nt_to_afs_rights(filename, ace));
	}

	return True;
}

static bool afs_get_afs_acl(char *filename, struct afs_acl *acl)
{
	struct afs_iob iob;

	int ret;

	char space[MAXSIZE];

	DEBUG(5, ("afs_get_afs_acl: %s\n", filename));

	iob.in_size = 0;
	iob.out_size = MAXSIZE;
	iob.in = iob.out = space;

	ret = afs_syscall(AFSCALL_PIOCTL, filename, VIOCGETAL,
			  (char *)&iob, 0);

	if (ret) {
		DEBUG(1, ("got error from PIOCTL: %d\n", ret));
		return False;
	}

	if (!init_afs_acl(acl))
		return False;

	if (!parse_afs_acl(acl, space)) {
		DEBUG(1, ("Could not parse AFS acl\n"));
		free_afs_acl(acl);
		return False;
	}

	return True;
}

/* For setting an AFS ACL we have to take care of the ACEs we could
 * not properly map to SIDs. Merge all of them into the new ACL. */

static void merge_unknown_aces(struct afs_acl *src, struct afs_acl *dst)
{
	struct afs_ace *ace;

	for (ace = src->acelist; ace != NULL; ace = ace->next)
	{
		struct afs_ace *copy;

		if (ace->type != SID_NAME_UNKNOWN) {
			DEBUG(10, ("Not merging known ACE for %s\n",
				   ace->name));
			continue;
		}

		DEBUG(10, ("Merging unknown ACE for %s\n", ace->name));

		copy = clone_afs_ace(dst->ctx, ace);

		if (copy == NULL) {
			DEBUG(0, ("Could not clone ACE for %s\n", ace->name));
			continue;
		}

		copy->next = dst->acelist;
		dst->acelist = copy;
		dst->num_aces += 1;
	}
}

static NTSTATUS afs_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
			   uint32 security_info_sent,
			   const struct security_descriptor *psd)
{
	struct afs_acl old_afs_acl, new_afs_acl;
	struct afs_acl dir_acl, file_acl;
	char acl_string[2049];
	struct afs_iob iob;
	int ret = -1;
	char *name = NULL;
	const char *fileacls;

	fileacls = lp_parm_const_string(SNUM(handle->conn), "afsacl", "fileacls",
					"yes");

	sidpts = lp_parm_bool(SNUM(handle->conn), "afsacl", "sidpts", False);

	ZERO_STRUCT(old_afs_acl);
	ZERO_STRUCT(new_afs_acl);
	ZERO_STRUCT(dir_acl);
	ZERO_STRUCT(file_acl);

	name = talloc_strdup(talloc_tos(), fsp->fsp_name->base_name);
	if (!name) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!fsp->is_directory) {
		/* We need to get the name of the directory containing the
		 * file, this is where the AFS acls live */
		char *p = strrchr(name, '/');
		if (p != NULL) {
			*p = '\0';
		} else {
			name = talloc_strdup(talloc_tos(), ".");
			if (!name) {
				return NT_STATUS_NO_MEMORY;
			}
		}
	}

	if (!afs_get_afs_acl(name, &old_afs_acl)) {
		DEBUG(3, ("Could not get old ACL of %s\n", fsp_str_dbg(fsp)));
		goto done;
	}

	split_afs_acl(&old_afs_acl, &dir_acl, &file_acl);

	if (fsp->is_directory) {

		if (!strequal(fileacls, "yes")) {
			/* Throw away file acls, we depend on the
			 * inheritance ACEs that also give us file
			 * permissions */
			free_afs_acl(&file_acl);
		}

		free_afs_acl(&dir_acl);
		if (!nt_to_afs_acl(fsp->fsp_name->base_name,
				   security_info_sent, psd,
				   nt_to_afs_dir_rights, &dir_acl))
			goto done;
	} else {
		if (strequal(fileacls, "no")) {
			ret = -1;
			goto done;
		}

		if (strequal(fileacls, "ignore")) {
			ret = 0;
			goto done;
		}

		free_afs_acl(&file_acl);
		if (!nt_to_afs_acl(fsp->fsp_name->base_name,
				   security_info_sent, psd,
				   nt_to_afs_file_rights, &file_acl))
			goto done;
	}

	merge_afs_acls(&dir_acl, &file_acl, &new_afs_acl);

	merge_unknown_aces(&old_afs_acl, &new_afs_acl);

	unparse_afs_acl(&new_afs_acl, acl_string);

	iob.in = acl_string;
	iob.in_size = 1+strlen(iob.in);
	iob.out = NULL;
	iob.out_size = 0;

	DEBUG(10, ("trying to set acl '%s' on file %s\n", iob.in, name));

	ret = afs_syscall(AFSCALL_PIOCTL, name, VIOCSETAL, (char *)&iob, 0);

	if (ret != 0) {
		DEBUG(10, ("VIOCSETAL returned %d\n", ret));
	}

 done:
	free_afs_acl(&dir_acl);
	free_afs_acl(&file_acl);
	free_afs_acl(&old_afs_acl);
	free_afs_acl(&new_afs_acl);

	return (ret == 0) ? NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;
}

static NTSTATUS afsacl_fget_nt_acl(struct vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   uint32 security_info,
				   struct security_descriptor **ppdesc)
{
	struct afs_acl acl;
	size_t sd_size;

	DEBUG(5, ("afsacl_fget_nt_acl: %s\n", fsp_str_dbg(fsp)));

	sidpts = lp_parm_bool(SNUM(fsp->conn), "afsacl", "sidpts", False);

	if (!afs_get_afs_acl(fsp->fsp_name->base_name, &acl)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	sd_size = afs_fto_nt_acl(&acl, fsp, security_info, ppdesc);

	free_afs_acl(&acl);

	return (sd_size != 0) ? NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;
}

static NTSTATUS afsacl_get_nt_acl(struct vfs_handle_struct *handle,
				  const char *name,  uint32 security_info,
				  struct security_descriptor **ppdesc)
{
	struct afs_acl acl;
	size_t sd_size;
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;

	DEBUG(5, ("afsacl_get_nt_acl: %s\n", name));

	sidpts = lp_parm_bool(SNUM(handle->conn), "afsacl", "sidpts", False);

	if (!afs_get_afs_acl(name, &acl)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = create_synthetic_smb_fname(talloc_tos(), name, NULL, NULL,
					    &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		free_afs_acl(&acl);
		return status;
	}

	sd_size = afs_to_nt_acl(&acl, handle->conn, smb_fname, security_info,
				ppdesc);
	TALLOC_FREE(smb_fname);

	free_afs_acl(&acl);

	return (sd_size != 0) ? NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;
}

NTSTATUS afsacl_fset_nt_acl(vfs_handle_struct *handle,
			 files_struct *fsp,
			 uint32 security_info_sent,
			 const struct security_descriptor *psd)
{
	return afs_set_nt_acl(handle, fsp, security_info_sent, psd);
}

static int afsacl_connect(vfs_handle_struct *handle, 
			  const char *service, 
			  const char *user)
{
	const char *spc;
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	spc = lp_parm_const_string(SNUM(handle->conn), "afsacl", "space", "%");

	if (spc != NULL)
		space_replacement = spc[0];

	return 0;
}

static struct vfs_fn_pointers vfs_afsacl_fns = {
	.connect_fn = afsacl_connect,
	.fget_nt_acl = afsacl_fget_nt_acl,
	.get_nt_acl = afsacl_get_nt_acl,
	.fset_nt_acl = afsacl_fset_nt_acl
};

NTSTATUS vfs_afsacl_init(void);
NTSTATUS vfs_afsacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "afsacl",
				&vfs_afsacl_fns);
}
