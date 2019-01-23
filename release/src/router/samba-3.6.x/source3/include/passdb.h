/* 
   Unix SMB/CIFS implementation.
   passdb structures and parameters
   Copyright (C) Gerald Carter 2001
   Copyright (C) Luke Kenneth Casson Leighton 1998 - 2000
   Copyright (C) Andrew Bartlett 2002
   Copyright (C) Simo Sorce 2003

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

#ifndef _PASSDB_H
#define _PASSDB_H

#include "../librpc/gen_ndr/lsa.h"

#include "mapping.h"

/**********************************************************************
 * Masks for mappings between unix uid and gid types and
 * NT RIDS.
 **********************************************************************/

/* Take the bottom bit. */
#define RID_TYPE_MASK 		1
#define RID_MULTIPLIER 		2

/* The two common types. */
#define USER_RID_TYPE 		0
#define GROUP_RID_TYPE 		1

/*
 * Flags for local user manipulation.
 */

#define LOCAL_ADD_USER 0x1
#define LOCAL_DELETE_USER 0x2
#define LOCAL_DISABLE_USER 0x4
#define LOCAL_ENABLE_USER 0x8
#define LOCAL_TRUST_ACCOUNT 0x10
#define LOCAL_SET_NO_PASSWORD 0x20
#define LOCAL_SET_PASSWORD 0x40
#define LOCAL_SET_LDAP_ADMIN_PW 0x80
#define LOCAL_INTERDOM_ACCOUNT 0x100
#define LOCAL_AM_ROOT 0x200  /* Act as root */

/*
 * Size of new password account encoding string.  This is enough space to
 * hold 11 ACB characters, plus the surrounding [] and a terminating null.
 * Do not change unless you are adding new ACB bits!
 */

#define NEW_PW_FORMAT_SPACE_PADDED_LEN 14

/* Password history contants. */
#define PW_HISTORY_SALT_LEN 16
#define SALTED_MD5_HASH_LEN 16
#define PW_HISTORY_ENTRY_LEN (PW_HISTORY_SALT_LEN+SALTED_MD5_HASH_LEN)
#define MAX_PW_HISTORY_LEN 24

/*
 * bit flags representing initialized fields in struct samu
 */
enum pdb_elements {
	PDB_UNINIT,
	PDB_SMBHOME,
	PDB_PROFILE,
	PDB_DRIVE,
	PDB_LOGONSCRIPT,
	PDB_LOGONTIME,
	PDB_LOGOFFTIME,
	PDB_KICKOFFTIME,
	PDB_BAD_PASSWORD_TIME,
	PDB_CANCHANGETIME,
	PDB_MUSTCHANGETIME,
	PDB_PLAINTEXT_PW,
	PDB_USERNAME,
	PDB_FULLNAME,
	PDB_DOMAIN,
	PDB_NTUSERNAME,
	PDB_HOURSLEN,
	PDB_LOGONDIVS,
	PDB_USERSID,
	PDB_GROUPSID,
	PDB_ACCTCTRL,
	PDB_PASSLASTSET,
	PDB_ACCTDESC,
	PDB_WORKSTATIONS,
	PDB_COMMENT,
	PDB_MUNGEDDIAL,
	PDB_HOURS,
	PDB_FIELDS_PRESENT,
	PDB_BAD_PASSWORD_COUNT,
	PDB_LOGON_COUNT,
	PDB_COUNTRY_CODE,
	PDB_CODE_PAGE,
	PDB_UNKNOWN6,
	PDB_LMPASSWD,
	PDB_NTPASSWD,
	PDB_PWHISTORY,
	PDB_BACKEND_PRIVATE_DATA,

	/* this must be the last element */
	PDB_COUNT
};

enum pdb_group_elements {
	PDB_GROUP_NAME,
	PDB_GROUP_SID,
	PDB_GROUP_SID_NAME_USE,
	PDB_GROUP_MEMBERS,

	/* this must be the last element */
	PDB_GROUP_COUNT
};


enum pdb_value_state {
	PDB_DEFAULT=0,
	PDB_SET,
	PDB_CHANGED
};

#define IS_SAM_SET(x, flag)	(pdb_get_init_flags(x, flag) == PDB_SET)
#define IS_SAM_CHANGED(x, flag)	(pdb_get_init_flags(x, flag) == PDB_CHANGED)
#define IS_SAM_DEFAULT(x, flag)	(pdb_get_init_flags(x, flag) == PDB_DEFAULT)

/* cache for bad password lockout data, to be used on replicated SAMs */
struct login_cache {
	time_t entry_timestamp;
	uint32_t acct_ctrl;
	uint16_t bad_password_count;
	time_t bad_password_time;
};

#define SAMU_BUFFER_V0		0
#define SAMU_BUFFER_V1		1
#define SAMU_BUFFER_V2		2
#define SAMU_BUFFER_V3		3
/* nothing changed from V3 to V4 */
#define SAMU_BUFFER_V4		4
#define SAMU_BUFFER_LATEST	SAMU_BUFFER_V4

#define MAX_HOURS_LEN 32

struct samu {
	struct pdb_methods *methods;

	/* initialization flags */
	struct bitmap *change_flags;
	struct bitmap *set_flags;

	time_t logon_time;            /* logon time */
	time_t logoff_time;           /* logoff time */
	time_t kickoff_time;          /* kickoff time */
	time_t bad_password_time;     /* last bad password entered */
	time_t pass_last_set_time;    /* password last set time */
	time_t pass_can_change_time;  /* password can change time */
	time_t pass_must_change_time; /* password must change time */

	const char *username;     /* UNIX username string */
	const char *domain;       /* Windows Domain name */
	const char *nt_username;  /* Windows username string */
	const char *full_name;    /* user's full name string */
	const char *home_dir;     /* home directory string */
	const char *dir_drive;    /* home directory drive string */
	const char *logon_script; /* logon script string */
	const char *profile_path; /* profile path string */
	const char *acct_desc;    /* user description string */
	const char *workstations; /* login from workstations string */
	const char *comment;
	const char *munged_dial;  /* munged path name and dial-back tel number */

	struct dom_sid user_sid;
	struct dom_sid *group_sid;

	DATA_BLOB lm_pw; /* .data is Null if no password */
	DATA_BLOB nt_pw; /* .data is Null if no password */
	DATA_BLOB nt_pw_his; /* nt hashed password history .data is Null if not available */
	char* plaintext_pw; /* is Null if not available */

	uint32_t acct_ctrl; /* account info (ACB_xxxx bit-mask) */
	uint32_t fields_present; /* 0x00ff ffff */

	uint16_t logon_divs; /* 168 - number of hours in a week */
	uint32_t hours_len; /* normally 21 bytes */
	uint8_t hours[MAX_HOURS_LEN];

	/* Was unknown_5. */
	uint16_t bad_password_count;
	uint16_t logon_count;

	uint16_t country_code;
	uint16_t code_page;

	uint32_t unknown_6; /* 0x0000 04ec */

	/* a tag for who added the private methods */

	const struct pdb_methods *backend_private_methods;
	void *backend_private_data; 
	void (*backend_private_data_free_fn)(void **);

	/* maintain a copy of the user's struct passwd */

	struct passwd *unix_pw;
};

struct acct_info {
	fstring acct_name; /* account name */
	fstring acct_desc; /* account name */
	uint32_t rid; /* domain-relative RID */
};

struct samr_displayentry {
	uint32_t idx;
	uint32_t rid;
	uint32_t acct_flags;
	const char *account_name;
	const char *fullname;
	const char *description;
};

enum pdb_search_type {
	PDB_USER_SEARCH,
	PDB_GROUP_SEARCH,
	PDB_ALIAS_SEARCH
};

struct pdb_search {
	enum pdb_search_type type;
	struct samr_displayentry *cache;
	uint32_t num_entries;
	ssize_t cache_size;
	bool search_ended;
	void *private_data;
	bool (*next_entry)(struct pdb_search *search,
			   struct samr_displayentry *entry);
	void (*search_end)(struct pdb_search *search);
};

struct pdb_domain_info {
	char *name;
	char *dns_domain;
	char *dns_forest;
	struct dom_sid sid;
	struct GUID guid;
};

struct pdb_trusted_domain {
	char *domain_name;
	char *netbios_name;
	struct dom_sid security_identifier;
	DATA_BLOB trust_auth_incoming;
	DATA_BLOB trust_auth_outgoing;
	uint32_t trust_direction;
	uint32_t trust_type;
	uint32_t trust_attributes;
	DATA_BLOB trust_forest_trust_info;
};

/*
 * trusted domain entry/entries returned by secrets_get_trusted_domains
 * (used in _lsa_enum_trust_dom call)
 */
struct trustdom_info {
	char *name;
	struct dom_sid sid;
};

/*
 * Types of account policy.
 */
enum pdb_policy_type {
	PDB_POLICY_MIN_PASSWORD_LEN = 1,
	PDB_POLICY_PASSWORD_HISTORY = 2,
	PDB_POLICY_USER_MUST_LOGON_TO_CHG_PASS	= 3,
	PDB_POLICY_MAX_PASSWORD_AGE = 4,
	PDB_POLICY_MIN_PASSWORD_AGE = 5,
	PDB_POLICY_LOCK_ACCOUNT_DURATION = 6,
	PDB_POLICY_RESET_COUNT_TIME = 7,
	PDB_POLICY_BAD_ATTEMPT_LOCKOUT = 8,
	PDB_POLICY_TIME_TO_LOGOUT = 9,
	PDB_POLICY_REFUSE_MACHINE_PW_CHANGE = 10
};

#define PDB_CAP_STORE_RIDS		0x0001
#define PDB_CAP_ADS			0x0002
#define PDB_CAP_TRUSTED_DOMAINS_EX	0x0004

/*****************************************************************
 Functions to be implemented by the new (v2) passdb API 
****************************************************************/

/*
 * This next constant specifies the version number of the PASSDB interface
 * this SAMBA will load. Increment this if *ANY* changes are made to the interface. 
 * Changed interface to fix int -> size_t problems. JRA.
 * There's no point in allocating arrays in
 * samr_lookup_rids twice. It was done in the srv_samr_nt.c code as well as in
 * the pdb module. Remove the latter, this might happen more often. VL.
 * changed to version 14 to move lookup_rids and lookup_names to return
 * enum lsa_SidType rather than uint32_t.
 * Changed to 16 for access to the trusted domain passwords (obnox).
 * Changed to 17, the sampwent interface is gone.
 * Changed to 18, pdb_rid_algorithm -> pdb_capabilities
 * Changed to 19, removed uid_to_rid
 */

#define PASSDB_INTERFACE_VERSION 19

struct pdb_methods 
{
	const char *name; /* What name got this module */

	struct pdb_domain_info *(*get_domain_info)(struct pdb_methods *,
						   TALLOC_CTX *mem_ctx);

	NTSTATUS (*getsampwnam)(struct pdb_methods *, struct samu *sam_acct, const char *username);

	NTSTATUS (*getsampwsid)(struct pdb_methods *, struct samu *sam_acct, const struct dom_sid *sid);

	NTSTATUS (*create_user)(struct pdb_methods *, TALLOC_CTX *tmp_ctx,
				const char *name, uint32_t acct_flags,
				uint32_t *rid);

	NTSTATUS (*delete_user)(struct pdb_methods *, TALLOC_CTX *tmp_ctx,
				struct samu *sam_acct);

	NTSTATUS (*add_sam_account)(struct pdb_methods *, struct samu *sampass);

	NTSTATUS (*update_sam_account)(struct pdb_methods *, struct samu *sampass);

	NTSTATUS (*delete_sam_account)(struct pdb_methods *, struct samu *username);

	NTSTATUS (*rename_sam_account)(struct pdb_methods *, struct samu *oldname, const char *newname);

	NTSTATUS (*update_login_attempts)(struct pdb_methods *methods, struct samu *sam_acct, bool success);

	NTSTATUS (*getgrsid)(struct pdb_methods *methods, GROUP_MAP *map, struct dom_sid sid);

	NTSTATUS (*getgrgid)(struct pdb_methods *methods, GROUP_MAP *map, gid_t gid);

	NTSTATUS (*getgrnam)(struct pdb_methods *methods, GROUP_MAP *map, const char *name);

	NTSTATUS (*create_dom_group)(struct pdb_methods *methods,
				     TALLOC_CTX *mem_ctx, const char *name,
				     uint32_t *rid);

	NTSTATUS (*delete_dom_group)(struct pdb_methods *methods,
				     TALLOC_CTX *mem_ctx, uint32_t rid);

	NTSTATUS (*add_group_mapping_entry)(struct pdb_methods *methods,
					    GROUP_MAP *map);

	NTSTATUS (*update_group_mapping_entry)(struct pdb_methods *methods,
					       GROUP_MAP *map);

	NTSTATUS (*delete_group_mapping_entry)(struct pdb_methods *methods,
					       struct dom_sid sid);

	NTSTATUS (*enum_group_mapping)(struct pdb_methods *methods,
				       const struct dom_sid *sid, enum lsa_SidType sid_name_use,
				       GROUP_MAP **pp_rmap, size_t *p_num_entries,
				       bool unix_only);

	NTSTATUS (*enum_group_members)(struct pdb_methods *methods,
				       TALLOC_CTX *mem_ctx,
				       const struct dom_sid *group,
				       uint32_t **pp_member_rids,
				       size_t *p_num_members);

	NTSTATUS (*enum_group_memberships)(struct pdb_methods *methods,
					   TALLOC_CTX *mem_ctx,
					   struct samu *user,
					   struct dom_sid **pp_sids, gid_t **pp_gids,
					   uint32_t *p_num_groups);

	NTSTATUS (*set_unix_primary_group)(struct pdb_methods *methods,
					   TALLOC_CTX *mem_ctx,
					   struct samu *user);

	NTSTATUS (*add_groupmem)(struct pdb_methods *methods,
				 TALLOC_CTX *mem_ctx,
				 uint32_t group_rid, uint32_t member_rid);

	NTSTATUS (*del_groupmem)(struct pdb_methods *methods,
				 TALLOC_CTX *mem_ctx,
				 uint32_t group_rid, uint32_t member_rid);

	NTSTATUS (*create_alias)(struct pdb_methods *methods,
				 const char *name, uint32_t *rid);

	NTSTATUS (*delete_alias)(struct pdb_methods *methods,
				 const struct dom_sid *sid);

	NTSTATUS (*get_aliasinfo)(struct pdb_methods *methods,
				  const struct dom_sid *sid,
				  struct acct_info *info);

	NTSTATUS (*set_aliasinfo)(struct pdb_methods *methods,
				  const struct dom_sid *sid,
				  struct acct_info *info);

	NTSTATUS (*add_aliasmem)(struct pdb_methods *methods,
				 const struct dom_sid *alias, const struct dom_sid *member);
	NTSTATUS (*del_aliasmem)(struct pdb_methods *methods,
				 const struct dom_sid *alias, const struct dom_sid *member);
	NTSTATUS (*enum_aliasmem)(struct pdb_methods *methods,
				  const struct dom_sid *alias, TALLOC_CTX *mem_ctx,
				  struct dom_sid **members, size_t *p_num_members);
	NTSTATUS (*enum_alias_memberships)(struct pdb_methods *methods,
					   TALLOC_CTX *mem_ctx,
					   const struct dom_sid *domain_sid,
					   const struct dom_sid *members,
					   size_t num_members,
					   uint32_t **pp_alias_rids,
					   size_t *p_num_alias_rids);

	NTSTATUS (*lookup_rids)(struct pdb_methods *methods,
				const struct dom_sid *domain_sid,
				int num_rids,
				uint32_t *rids,
				const char **pp_names,
				enum lsa_SidType *attrs);

	NTSTATUS (*lookup_names)(struct pdb_methods *methods,
				 const struct dom_sid *domain_sid,
				 int num_names,
				 const char **pp_names,
				 uint32_t *rids,
				 enum lsa_SidType *attrs);

	NTSTATUS (*get_account_policy)(struct pdb_methods *methods,
				       enum pdb_policy_type type,
				       uint32_t *value);

	NTSTATUS (*set_account_policy)(struct pdb_methods *methods,
				       enum pdb_policy_type type,
				       uint32_t value);

	NTSTATUS (*get_seq_num)(struct pdb_methods *methods, time_t *seq_num);

	bool (*search_users)(struct pdb_methods *methods,
			     struct pdb_search *search,
			     uint32_t acct_flags);
	bool (*search_groups)(struct pdb_methods *methods,
			      struct pdb_search *search);
	bool (*search_aliases)(struct pdb_methods *methods,
			       struct pdb_search *search,
			       const struct dom_sid *sid);

	bool (*uid_to_sid)(struct pdb_methods *methods, uid_t uid,
			   struct dom_sid *sid);
	bool (*gid_to_sid)(struct pdb_methods *methods, gid_t gid,
			   struct dom_sid *sid);
	bool (*sid_to_id)(struct pdb_methods *methods, const struct dom_sid *sid,
			  union unid_t *id, enum lsa_SidType *type);

	uint32_t (*capabilities)(struct pdb_methods *methods);
	bool (*new_rid)(struct pdb_methods *methods, uint32_t *rid);


	bool (*get_trusteddom_pw)(struct pdb_methods *methods,
				  const char *domain, char** pwd, 
				  struct dom_sid *sid, time_t *pass_last_set_time);
	bool (*set_trusteddom_pw)(struct pdb_methods *methods, 
				  const char* domain, const char* pwd,
				  const struct dom_sid *sid);
	bool (*del_trusteddom_pw)(struct pdb_methods *methods, 
				  const char *domain);
	NTSTATUS (*enum_trusteddoms)(struct pdb_methods *methods,
				     TALLOC_CTX *mem_ctx, uint32_t *num_domains,
				     struct trustdom_info ***domains);


	NTSTATUS (*get_trusted_domain)(struct pdb_methods *methods,
				       TALLOC_CTX *mem_ctx,
				       const char *domain,
				       struct pdb_trusted_domain **td);
	NTSTATUS (*get_trusted_domain_by_sid)(struct pdb_methods *methods,
					      TALLOC_CTX *mem_ctx,
					      struct dom_sid *sid,
					      struct pdb_trusted_domain **td);
	NTSTATUS (*set_trusted_domain)(struct pdb_methods *methods,
				       const char* domain,
				       const struct pdb_trusted_domain *td);
	NTSTATUS (*del_trusted_domain)(struct pdb_methods *methods,
				       const char *domain);
	NTSTATUS (*enum_trusted_domains)(struct pdb_methods *methods,
					 TALLOC_CTX *mem_ctx,
					 uint32_t *num_domains,
					 struct pdb_trusted_domain ***domains);

	void *private_data;  /* Private data of some kind */

	void (*free_private_data)(void **);
};

typedef NTSTATUS (*pdb_init_function)(struct pdb_methods **, const char *);

struct pdb_init_function_entry {
	const char *name;

	/* Function to create a member of the pdb_methods list */
	pdb_init_function init;

	struct pdb_init_function_entry *prev, *next;
};

#include "passdb/proto.h"
#include "passdb/machine_sid.h"
#include "passdb/lookup_sid.h"

#endif /* _PASSDB_H */
