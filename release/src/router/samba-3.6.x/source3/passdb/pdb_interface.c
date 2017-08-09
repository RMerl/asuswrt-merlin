/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Bartlett			2002
   Copyright (C) Jelmer Vernooij			2002
   Copyright (C) Simo Sorce				2003
   Copyright (C) Volker Lendecke			2006

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
#include "system/passwd.h"
#include "passdb.h"
#include "secrets.h"
#include "../librpc/gen_ndr/samr.h"
#include "memcache.h"
#include "nsswitch/winbind_client.h"
#include "../libcli/security/security.h"
#include "../lib/util/util_pw.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_PASSDB

static_decl_pdb;

static struct pdb_init_function_entry *backends = NULL;

static void lazy_initialize_passdb(void)
{
	static bool initialized = False;
	if(initialized) {
		return;
	}
	static_init_pdb;
	initialized = True;
}

static bool lookup_global_sam_rid(TALLOC_CTX *mem_ctx, uint32 rid,
				  const char **name,
				  enum lsa_SidType *psid_name_use,
				  union unid_t *unix_id);

NTSTATUS smb_register_passdb(int version, const char *name, pdb_init_function init) 
{
	struct pdb_init_function_entry *entry = backends;

	if(version != PASSDB_INTERFACE_VERSION) {
		DEBUG(0,("Can't register passdb backend!\n"
			 "You tried to register a passdb module with PASSDB_INTERFACE_VERSION %d, "
			 "while this version of samba uses version %d\n", 
			 version,PASSDB_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (!name || !init) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(5,("Attempting to register passdb backend %s\n", name));

	/* Check for duplicates */
	if (pdb_find_backend_entry(name)) {
		DEBUG(0,("There already is a passdb backend registered with the name %s!\n", name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = SMB_XMALLOC_P(struct pdb_init_function_entry);
	entry->name = smb_xstrdup(name);
	entry->init = init;

	DLIST_ADD(backends, entry);
	DEBUG(5,("Successfully added passdb backend '%s'\n", name));
	return NT_STATUS_OK;
}

struct pdb_init_function_entry *pdb_find_backend_entry(const char *name)
{
	struct pdb_init_function_entry *entry = backends;

	while(entry) {
		if (strcmp(entry->name, name)==0) return entry;
		entry = entry->next;
	}

	return NULL;
}

/*
 * The event context for the passdb backend. I know this is a bad hack and yet
 * another static variable, but our pdb API is a global thing per
 * definition. The first use for this is the LDAP idle function, more might be
 * added later.
 *
 * I don't feel too bad about this static variable, it replaces the
 * smb_idle_event_list that used to exist in lib/module.c.  -- VL
 */

static struct event_context *pdb_event_ctx;

struct event_context *pdb_get_event_context(void)
{
	return pdb_event_ctx;
}

/******************************************************************
  Make a pdb_methods from scratch
 *******************************************************************/

NTSTATUS make_pdb_method_name(struct pdb_methods **methods, const char *selected)
{
	char *module_name = smb_xstrdup(selected);
	char *module_location = NULL, *p;
	struct pdb_init_function_entry *entry;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	lazy_initialize_passdb();

	p = strchr(module_name, ':');

	if (p) {
		*p = 0;
		module_location = p+1;
		trim_char(module_location, ' ', ' ');
	}

	trim_char(module_name, ' ', ' ');


	DEBUG(5,("Attempting to find a passdb backend to match %s (%s)\n", selected, module_name));

	entry = pdb_find_backend_entry(module_name);

	/* Try to find a module that contains this module */
	if (!entry) { 
		DEBUG(2,("No builtin backend found, trying to load plugin\n"));
		if(NT_STATUS_IS_OK(smb_probe_module("pdb", module_name)) && !(entry = pdb_find_backend_entry(module_name))) {
			DEBUG(0,("Plugin is available, but doesn't register passdb backend %s\n", module_name));
			SAFE_FREE(module_name);
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	/* No such backend found */
	if(!entry) { 
		DEBUG(0,("No builtin nor plugin backend for %s found\n", module_name));
		SAFE_FREE(module_name);
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(5,("Found pdb backend %s\n", module_name));

	if ( !NT_STATUS_IS_OK( nt_status = entry->init(methods, module_location) ) ) {
		DEBUG(0,("pdb backend %s did not correctly init (error was %s)\n", 
			selected, nt_errstr(nt_status)));
		SAFE_FREE(module_name);
		return nt_status;
	}

	SAFE_FREE(module_name);

	DEBUG(5,("pdb backend %s has a valid init\n", selected));

	return nt_status;
}

/******************************************************************
 Return an already initialized pdb_methods structure
*******************************************************************/

static struct pdb_methods *pdb_get_methods_reload( bool reload ) 
{
	static struct pdb_methods *pdb = NULL;

	if ( pdb && reload ) {
		if (pdb->free_private_data != NULL) {
			pdb->free_private_data( &(pdb->private_data) );
		}
		if ( !NT_STATUS_IS_OK( make_pdb_method_name( &pdb, lp_passdb_backend() ) ) ) {
			char *msg = NULL;
			if (asprintf(&msg, "pdb_get_methods_reload: "
					"failed to get pdb methods for backend %s\n",
					lp_passdb_backend()) > 0) {
				smb_panic(msg);
			} else {
				smb_panic("pdb_get_methods_reload");
			}
		}
	}

	if ( !pdb ) {
		if ( !NT_STATUS_IS_OK( make_pdb_method_name( &pdb, lp_passdb_backend() ) ) ) {
			char *msg = NULL;
			if (asprintf(&msg, "pdb_get_methods_reload: "
					"failed to get pdb methods for backend %s\n",
					lp_passdb_backend()) > 0) {
				smb_panic(msg);
			} else {
				smb_panic("pdb_get_methods_reload");
			}
		}
	}

	return pdb;
}

static struct pdb_methods *pdb_get_methods(void)
{
	return pdb_get_methods_reload(False);
}

struct pdb_domain_info *pdb_get_domain_info(TALLOC_CTX *mem_ctx)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->get_domain_info(pdb, mem_ctx);
}

/**
 * @brief Check if the user account has been locked out and try to unlock it.
 *
 * If the user has been automatically locked out and a lockout duration is set,
 * then check if we can unlock the account and reset the bad password values.
 *
 * @param[in]  sampass  The sam user to check.
 *
 * @return              True if the function was successfull, false on an error.
 */
static bool pdb_try_account_unlock(struct samu *sampass)
{
	uint32_t acb_info = pdb_get_acct_ctrl(sampass);

	if ((acb_info & ACB_NORMAL) && (acb_info & ACB_AUTOLOCK)) {
		uint32_t lockout_duration;
		time_t bad_password_time;
		time_t now = time(NULL);
		bool ok;

		ok = pdb_get_account_policy(PDB_POLICY_LOCK_ACCOUNT_DURATION,
					    &lockout_duration);
		if (!ok) {
			DEBUG(0, ("pdb_try_account_unlock: "
				  "pdb_get_account_policy failed.\n"));
			return false;
		}

		if (lockout_duration == (uint32_t) -1 ||
		    lockout_duration == 0) {
			DEBUG(9, ("pdb_try_account_unlock: No reset duration, "
				  "can't reset autolock\n"));
			return false;
		}
		lockout_duration *= 60;

		bad_password_time = pdb_get_bad_password_time(sampass);
		if (bad_password_time == (time_t) 0) {
			DEBUG(2, ("pdb_try_account_unlock: Account %s "
				  "administratively locked out "
				  "with no bad password "
				  "time. Leaving locked out.\n",
				  pdb_get_username(sampass)));
			return true;
		}

		if ((bad_password_time +
		     convert_uint32_t_to_time_t(lockout_duration)) < now) {
			NTSTATUS status;

			pdb_set_acct_ctrl(sampass, acb_info & ~ACB_AUTOLOCK,
					  PDB_CHANGED);
			pdb_set_bad_password_count(sampass, 0, PDB_CHANGED);
			pdb_set_bad_password_time(sampass, 0, PDB_CHANGED);

			become_root();
			status = pdb_update_sam_account(sampass);
			unbecome_root();
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("_samr_OpenUser: Couldn't "
					  "update account %s - %s\n",
					  pdb_get_username(sampass),
					  nt_errstr(status)));
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief Get a sam user structure by the given username.
 *
 * This functions also checks if the account has been automatically locked out
 * and unlocks it if a lockout duration time has been defined and the time has
 * elapsed.
 *
 * @param[in]  sam_acct  The sam user structure to fill.
 *
 * @param[in]  username  The username to look for.
 *
 * @return               True on success, false on error.
 */
bool pdb_getsampwnam(struct samu *sam_acct, const char *username) 
{
	struct pdb_methods *pdb = pdb_get_methods();
	struct samu *for_cache;
	const struct dom_sid *user_sid;
	NTSTATUS status;
	bool ok;

	status = pdb->getsampwnam(pdb, sam_acct, username);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	ok = pdb_try_account_unlock(sam_acct);
	if (!ok) {
		DEBUG(1, ("pdb_getsampwnam: Failed to unlock account %s\n",
			  username));
	}

	for_cache = samu_new(NULL);
	if (for_cache == NULL) {
		return False;
	}

	if (!pdb_copy_sam_account(for_cache, sam_acct)) {
		TALLOC_FREE(for_cache);
		return False;
	}

	user_sid = pdb_get_user_sid(for_cache);

	memcache_add_talloc(NULL, PDB_GETPWSID_CACHE,
			    data_blob_const(user_sid, sizeof(*user_sid)),
			    &for_cache);

	return True;
}

/**********************************************************************
**********************************************************************/

static bool guest_user_info( struct samu *user )
{
	struct passwd *pwd;
	NTSTATUS result;
	const char *guestname = lp_guestaccount();

	pwd = Get_Pwnam_alloc(talloc_tos(), guestname);
	if (pwd == NULL) {
		DEBUG(0,("guest_user_info: Unable to locate guest account [%s]!\n", 
			guestname));
		return False;
	}

	result = samu_set_unix(user, pwd );

	TALLOC_FREE( pwd );

	return NT_STATUS_IS_OK( result );
}

/**
 * @brief Get a sam user structure by the given username.
 *
 * This functions also checks if the account has been automatically locked out
 * and unlocks it if a lockout duration time has been defined and the time has
 * elapsed.
 *
 *
 * @param[in]  sam_acct  The sam user structure to fill.
 *
 * @param[in]  sid       The user SDI to look up.
 *
 * @return               True on success, false on error.
 */
bool pdb_getsampwsid(struct samu *sam_acct, const struct dom_sid *sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	uint32_t rid;
	void *cache_data;
	bool ok = false;

	/* hard code the Guest RID of 501 */

	if ( !sid_peek_check_rid( get_global_sam_sid(), sid, &rid ) )
		return False;

	if ( rid == DOMAIN_RID_GUEST ) {
		DEBUG(6,("pdb_getsampwsid: Building guest account\n"));
		return guest_user_info( sam_acct );
	}

	/* check the cache first */

	cache_data = memcache_lookup_talloc(
		NULL, PDB_GETPWSID_CACHE, data_blob_const(sid, sizeof(*sid)));

	if (cache_data != NULL) {
		struct samu *cache_copy = talloc_get_type_abort(
			cache_data, struct samu);

		ok = pdb_copy_sam_account(sam_acct, cache_copy);
	} else {
		ok = NT_STATUS_IS_OK(pdb->getsampwsid(pdb, sam_acct, sid));
	}

	if (!ok) {
		return false;
	}

	ok = pdb_try_account_unlock(sam_acct);
	if (!ok) {
		DEBUG(1, ("pdb_getsampwsid: Failed to unlock account %s\n",
			  sam_acct->username));
	}

	return true;
}

static NTSTATUS pdb_default_create_user(struct pdb_methods *methods,
					TALLOC_CTX *tmp_ctx, const char *name,
					uint32_t acb_info, uint32_t *rid)
{
	struct samu *sam_pass;
	NTSTATUS status;
	struct passwd *pwd;

	if ((sam_pass = samu_new(tmp_ctx)) == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !(pwd = Get_Pwnam_alloc(tmp_ctx, name)) ) {
		char *add_script = NULL;
		int add_ret;
		fstring name2;

		if ((acb_info & ACB_NORMAL) && name[strlen(name)-1] != '$') {
			add_script = talloc_strdup(tmp_ctx,
					lp_adduser_script());
		} else {
			add_script = talloc_strdup(tmp_ctx,
					lp_addmachine_script());
		}

		if (!add_script || add_script[0] == '\0') {
			DEBUG(3, ("Could not find user %s and no add script "
				  "defined\n", name));
			return NT_STATUS_NO_SUCH_USER;
		}

		/* lowercase the username before creating the Unix account for 
		   compatibility with previous Samba releases */
		fstrcpy( name2, name );
		strlower_m( name2 );
		add_script = talloc_all_string_sub(tmp_ctx,
					add_script,
					"%u",
					name2);
		if (!add_script) {
			return NT_STATUS_NO_MEMORY;
		}
		add_ret = smbrun(add_script,NULL);
		DEBUG(add_ret ? 0 : 3, ("_samr_create_user: Running the command `%s' gave %d\n",
					add_script, add_ret));
		if (add_ret == 0) {
			smb_nscd_flush_user_cache();
		}

		flush_pwnam_cache();

		pwd = Get_Pwnam_alloc(tmp_ctx, name);

		if(pwd == NULL) {
			DEBUG(3, ("Could not find user %s, add script did not work\n", name));
			return NT_STATUS_NO_SUCH_USER;
		}
	}

	/* we have a valid SID coming out of this call */

	status = samu_alloc_rid_unix( sam_pass, pwd );

	TALLOC_FREE( pwd );

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("pdb_default_create_user: failed to create a new user structure: %s\n", nt_errstr(status)));
		return status;
	}

	if (!sid_peek_check_rid(get_global_sam_sid(),
				pdb_get_user_sid(sam_pass), rid)) {
		DEBUG(0, ("Could not get RID of fresh user\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* Use the username case specified in the original request */

	pdb_set_username( sam_pass, name, PDB_SET );

	/* Disable the account on creation, it does not have a reasonable password yet. */

	acb_info |= ACB_DISABLED;

	pdb_set_acct_ctrl(sam_pass, acb_info, PDB_CHANGED);

	status = pdb_add_sam_account(sam_pass);

	TALLOC_FREE(sam_pass);

	return status;
}

NTSTATUS pdb_create_user(TALLOC_CTX *mem_ctx, const char *name, uint32_t flags,
			 uint32_t *rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->create_user(pdb, mem_ctx, name, flags, rid);
}

/****************************************************************************
 Delete a UNIX user on demand.
****************************************************************************/

static int smb_delete_user(const char *unix_user)
{
	char *del_script = NULL;
	int ret;

	/* safety check */

	if ( strequal( unix_user, "root" ) ) {
		DEBUG(0,("smb_delete_user: Refusing to delete local system root account!\n"));
		return -1;
	}

	del_script = talloc_strdup(talloc_tos(), lp_deluser_script());
	if (!del_script || !*del_script) {
		return -1;
	}
	del_script = talloc_all_string_sub(talloc_tos(),
				del_script,
				"%u",
				unix_user);
	if (!del_script) {
		return -1;
	}
	ret = smbrun(del_script,NULL);
	flush_pwnam_cache();
	if (ret == 0) {
		smb_nscd_flush_user_cache();
	}
	DEBUG(ret ? 0 : 3,("smb_delete_user: Running the command `%s' gave %d\n",del_script,ret));

	return ret;
}

static NTSTATUS pdb_default_delete_user(struct pdb_methods *methods,
					TALLOC_CTX *mem_ctx,
					struct samu *sam_acct)
{
	NTSTATUS status;
	fstring username;

	status = pdb_delete_sam_account(sam_acct);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	 * Now delete the unix side ....
	 * note: we don't check if the delete really happened as the script is
	 * not necessary present and maybe the sysadmin doesn't want to delete
	 * the unix side
	 */

	/* always lower case the username before handing it off to 
	   external scripts */

	fstrcpy( username, pdb_get_username(sam_acct) );
	strlower_m( username );

	smb_delete_user( username );

	return status;
}

NTSTATUS pdb_delete_user(TALLOC_CTX *mem_ctx, struct samu *sam_acct)
{
	struct pdb_methods *pdb = pdb_get_methods();
	uid_t uid = -1;

	/* sanity check to make sure we don't delete root */

	if ( !sid_to_uid( pdb_get_user_sid(sam_acct), &uid ) ) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if ( uid == 0 ) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return pdb->delete_user(pdb, mem_ctx, sam_acct);
}

NTSTATUS pdb_add_sam_account(struct samu *sam_acct) 
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->add_sam_account(pdb, sam_acct);
}

NTSTATUS pdb_update_sam_account(struct samu *sam_acct) 
{
	struct pdb_methods *pdb = pdb_get_methods();

	memcache_flush(NULL, PDB_GETPWSID_CACHE);

	return pdb->update_sam_account(pdb, sam_acct);
}

NTSTATUS pdb_delete_sam_account(struct samu *sam_acct) 
{
	struct pdb_methods *pdb = pdb_get_methods();

	memcache_flush(NULL, PDB_GETPWSID_CACHE);

	return pdb->delete_sam_account(pdb, sam_acct);
}

NTSTATUS pdb_rename_sam_account(struct samu *oldname, const char *newname)
{
	struct pdb_methods *pdb = pdb_get_methods();
	uid_t uid;
	NTSTATUS status;

	memcache_flush(NULL, PDB_GETPWSID_CACHE);

	/* sanity check to make sure we don't rename root */

	if ( !sid_to_uid( pdb_get_user_sid(oldname), &uid ) ) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if ( uid == 0 ) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = pdb->rename_sam_account(pdb, oldname, newname);

	/* always flush the cache here just to be safe */
	flush_pwnam_cache();

	return status;
}

NTSTATUS pdb_update_login_attempts(struct samu *sam_acct, bool success)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->update_login_attempts(pdb, sam_acct, success);
}

bool pdb_getgrsid(GROUP_MAP *map, struct dom_sid sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return NT_STATUS_IS_OK(pdb->getgrsid(pdb, map, sid));
}

bool pdb_getgrgid(GROUP_MAP *map, gid_t gid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return NT_STATUS_IS_OK(pdb->getgrgid(pdb, map, gid));
}

bool pdb_getgrnam(GROUP_MAP *map, const char *name)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return NT_STATUS_IS_OK(pdb->getgrnam(pdb, map, name));
}

static NTSTATUS pdb_default_create_dom_group(struct pdb_methods *methods,
					     TALLOC_CTX *mem_ctx,
					     const char *name,
					     uint32_t *rid)
{
	struct dom_sid group_sid;
	struct group *grp;
	fstring tmp;

	grp = getgrnam(name);

	if (grp == NULL) {
		gid_t gid;

		if (smb_create_group(name, &gid) != 0) {
			return NT_STATUS_ACCESS_DENIED;
		}

		grp = getgrgid(gid);
	}

	if (grp == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (pdb_capabilities() & PDB_CAP_STORE_RIDS) {
		if (!pdb_new_rid(rid)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	} else {
		*rid = algorithmic_pdb_gid_to_group_rid( grp->gr_gid );
	}

	sid_compose(&group_sid, get_global_sam_sid(), *rid);

	return add_initial_entry(grp->gr_gid, sid_to_fstring(tmp, &group_sid),
				 SID_NAME_DOM_GRP, name, NULL);
}

NTSTATUS pdb_create_dom_group(TALLOC_CTX *mem_ctx, const char *name,
			      uint32_t *rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->create_dom_group(pdb, mem_ctx, name, rid);
}

static NTSTATUS pdb_default_delete_dom_group(struct pdb_methods *methods,
					     TALLOC_CTX *mem_ctx,
					     uint32_t rid)
{
	struct dom_sid group_sid;
	GROUP_MAP map;
	NTSTATUS status;
	struct group *grp;
	const char *grp_name;

	/* coverity */
	map.gid = (gid_t) -1;

	sid_compose(&group_sid, get_global_sam_sid(), rid);

	if (!get_domain_group_from_sid(group_sid, &map)) {
		DEBUG(10, ("Could not find group for rid %d\n", rid));
		return NT_STATUS_NO_SUCH_GROUP;
	}

	/* We need the group name for the smb_delete_group later on */

	if (map.gid == (gid_t)-1) {
		return NT_STATUS_NO_SUCH_GROUP;
	}

	grp = getgrgid(map.gid);
	if (grp == NULL) {
		return NT_STATUS_NO_SUCH_GROUP;
	}

	/* Copy the name, no idea what pdb_delete_group_mapping_entry does.. */

	grp_name = talloc_strdup(mem_ctx, grp->gr_name);
	if (grp_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = pdb_delete_group_mapping_entry(group_sid);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Don't check the result of smb_delete_group */

	smb_delete_group(grp_name);

	return NT_STATUS_OK;
}

NTSTATUS pdb_delete_dom_group(TALLOC_CTX *mem_ctx, uint32_t rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->delete_dom_group(pdb, mem_ctx, rid);
}

NTSTATUS pdb_add_group_mapping_entry(GROUP_MAP *map)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->add_group_mapping_entry(pdb, map);
}

NTSTATUS pdb_update_group_mapping_entry(GROUP_MAP *map)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->update_group_mapping_entry(pdb, map);
}

NTSTATUS pdb_delete_group_mapping_entry(struct dom_sid sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->delete_group_mapping_entry(pdb, sid);
}

bool pdb_enum_group_mapping(const struct dom_sid *sid, enum lsa_SidType sid_name_use, GROUP_MAP **pp_rmap,
			    size_t *p_num_entries, bool unix_only)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return NT_STATUS_IS_OK(pdb-> enum_group_mapping(pdb, sid, sid_name_use,
		pp_rmap, p_num_entries, unix_only));
}

NTSTATUS pdb_enum_group_members(TALLOC_CTX *mem_ctx,
				const struct dom_sid *sid,
				uint32_t **pp_member_rids,
				size_t *p_num_members)
{
	struct pdb_methods *pdb = pdb_get_methods();
	NTSTATUS result;

	result = pdb->enum_group_members(pdb, mem_ctx, 
			sid, pp_member_rids, p_num_members);

	/* special check for rid 513 */

	if ( !NT_STATUS_IS_OK( result ) ) {
		uint32_t rid;

		sid_peek_rid( sid, &rid );

		if ( rid == DOMAIN_RID_USERS ) {
			*p_num_members = 0;
			*pp_member_rids = NULL;

			return NT_STATUS_OK;
		}
	}

	return result;
}

NTSTATUS pdb_enum_group_memberships(TALLOC_CTX *mem_ctx, struct samu *user,
				    struct dom_sid **pp_sids, gid_t **pp_gids,
				    uint32_t *p_num_groups)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->enum_group_memberships(
		pdb, mem_ctx, user,
		pp_sids, pp_gids, p_num_groups);
}

static NTSTATUS pdb_default_set_unix_primary_group(struct pdb_methods *methods,
						   TALLOC_CTX *mem_ctx,
						   struct samu *sampass)
{
	struct group *grp;
	gid_t gid;

	if (!sid_to_gid(pdb_get_group_sid(sampass), &gid) ||
	    (grp = getgrgid(gid)) == NULL) {
		return NT_STATUS_INVALID_PRIMARY_GROUP;
	}

	if (smb_set_primary_group(grp->gr_name,
				  pdb_get_username(sampass)) != 0) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

NTSTATUS pdb_set_unix_primary_group(TALLOC_CTX *mem_ctx, struct samu *user)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->set_unix_primary_group(pdb, mem_ctx, user);
}

/*
 * Helper function to see whether a user is in a group. We can't use
 * user_in_group_sid here because this creates dependencies only smbd can
 * fulfil.
 */

static bool pdb_user_in_group(TALLOC_CTX *mem_ctx, struct samu *account,
			      const struct dom_sid *group_sid)
{
	struct dom_sid *sids;
	gid_t *gids;
	uint32_t i, num_groups;

	if (!NT_STATUS_IS_OK(pdb_enum_group_memberships(mem_ctx, account,
							&sids, &gids,
							&num_groups))) {
		return False;
	}

	for (i=0; i<num_groups; i++) {
		if (dom_sid_equal(group_sid, &sids[i])) {
			return True;
		}
	}
	return False;
}

static NTSTATUS pdb_default_add_groupmem(struct pdb_methods *methods,
					 TALLOC_CTX *mem_ctx,
					 uint32_t group_rid,
					 uint32_t member_rid)
{
	struct dom_sid group_sid, member_sid;
	struct samu *account = NULL;
	GROUP_MAP map;
	struct group *grp;
	struct passwd *pwd;
	const char *group_name;
	uid_t uid;

	/* coverity */
	map.gid = (gid_t) -1;

	sid_compose(&group_sid, get_global_sam_sid(), group_rid);
	sid_compose(&member_sid, get_global_sam_sid(), member_rid);

	if (!get_domain_group_from_sid(group_sid, &map) ||
	    (map.gid == (gid_t)-1) ||
	    ((grp = getgrgid(map.gid)) == NULL)) {
		return NT_STATUS_NO_SUCH_GROUP;
	}

	group_name = talloc_strdup(mem_ctx, grp->gr_name);
	if (group_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !(account = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_getsampwsid(account, &member_sid) ||
	    !sid_to_uid(&member_sid, &uid) ||
	    ((pwd = getpwuid_alloc(mem_ctx, uid)) == NULL)) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if (pdb_user_in_group(mem_ctx, account, &group_sid)) {
		return NT_STATUS_MEMBER_IN_GROUP;
	}

	/* 
	 * ok, the group exist, the user exist, the user is not in the group,
	 * we can (finally) add it to the group !
	 */

	smb_add_user_group(group_name, pwd->pw_name);

	if (!pdb_user_in_group(mem_ctx, account, &group_sid)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

NTSTATUS pdb_add_groupmem(TALLOC_CTX *mem_ctx, uint32_t group_rid,
			  uint32_t member_rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->add_groupmem(pdb, mem_ctx, group_rid, member_rid);
}

static NTSTATUS pdb_default_del_groupmem(struct pdb_methods *methods,
					 TALLOC_CTX *mem_ctx,
					 uint32_t group_rid,
					 uint32_t member_rid)
{
	struct dom_sid group_sid, member_sid;
	struct samu *account = NULL;
	GROUP_MAP map;
	struct group *grp;
	struct passwd *pwd;
	const char *group_name;
	uid_t uid;

	sid_compose(&group_sid, get_global_sam_sid(), group_rid);
	sid_compose(&member_sid, get_global_sam_sid(), member_rid);

	if (!get_domain_group_from_sid(group_sid, &map) ||
	    (map.gid == (gid_t)-1) ||
	    ((grp = getgrgid(map.gid)) == NULL)) {
		return NT_STATUS_NO_SUCH_GROUP;
	}

	group_name = talloc_strdup(mem_ctx, grp->gr_name);
	if (group_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !(account = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_getsampwsid(account, &member_sid) ||
	    !sid_to_uid(&member_sid, &uid) ||
	    ((pwd = getpwuid_alloc(mem_ctx, uid)) == NULL)) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if (!pdb_user_in_group(mem_ctx, account, &group_sid)) {
		return NT_STATUS_MEMBER_NOT_IN_GROUP;
	}

	/* 
	 * ok, the group exist, the user exist, the user is in the group,
	 * we can (finally) delete it from the group!
	 */

	smb_delete_user_group(group_name, pwd->pw_name);

	if (pdb_user_in_group(mem_ctx, account, &group_sid)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

NTSTATUS pdb_del_groupmem(TALLOC_CTX *mem_ctx, uint32_t group_rid,
			  uint32_t member_rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->del_groupmem(pdb, mem_ctx, group_rid, member_rid);
}

NTSTATUS pdb_create_alias(const char *name, uint32_t *rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->create_alias(pdb, name, rid);
}

NTSTATUS pdb_delete_alias(const struct dom_sid *sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->delete_alias(pdb, sid);
}

NTSTATUS pdb_get_aliasinfo(const struct dom_sid *sid, struct acct_info *info)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->get_aliasinfo(pdb, sid, info);
}

NTSTATUS pdb_set_aliasinfo(const struct dom_sid *sid, struct acct_info *info)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->set_aliasinfo(pdb, sid, info);
}

NTSTATUS pdb_add_aliasmem(const struct dom_sid *alias, const struct dom_sid *member)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->add_aliasmem(pdb, alias, member);
}

NTSTATUS pdb_del_aliasmem(const struct dom_sid *alias, const struct dom_sid *member)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->del_aliasmem(pdb, alias, member);
}

NTSTATUS pdb_enum_aliasmem(const struct dom_sid *alias, TALLOC_CTX *mem_ctx,
			   struct dom_sid **pp_members, size_t *p_num_members)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->enum_aliasmem(pdb, alias, mem_ctx, pp_members,
				  p_num_members);
}

NTSTATUS pdb_enum_alias_memberships(TALLOC_CTX *mem_ctx,
				    const struct dom_sid *domain_sid,
				    const struct dom_sid *members, size_t num_members,
				    uint32_t **pp_alias_rids,
				    size_t *p_num_alias_rids)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->enum_alias_memberships(pdb, mem_ctx,
						       domain_sid,
						       members, num_members,
						       pp_alias_rids,
						       p_num_alias_rids);
}

NTSTATUS pdb_lookup_rids(const struct dom_sid *domain_sid,
			 int num_rids,
			 uint32_t *rids,
			 const char **names,
			 enum lsa_SidType *attrs)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->lookup_rids(pdb, domain_sid, num_rids, rids, names, attrs);
}

/* 
 * NOTE: pdb_lookup_names is currently (2007-01-12) not used anywhere 
 *       in the samba code.
 *       Unlike _lsa_lookup_sids and _samr_lookup_rids, which eventually 
 *       also ask pdb_lookup_rids, thus looking up a bunch of rids at a time, 
 *       the pdb_ calls _lsa_lookup_names and _samr_lookup_names come
 *       down to are pdb_getsampwnam and pdb_getgrnam instead of
 *       pdb_lookup_names.
 *       But in principle, it the call belongs to the API and might get
 *       used in this context some day. 
 */
#if 0
NTSTATUS pdb_lookup_names(const struct dom_sid *domain_sid,
			  int num_names,
			  const char **names,
			  uint32_t *rids,
			  enum lsa_SidType *attrs)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->lookup_names(pdb, domain_sid, num_names, names, rids, attrs);
}
#endif

bool pdb_get_account_policy(enum pdb_policy_type type, uint32_t *value)
{
	struct pdb_methods *pdb = pdb_get_methods();
	NTSTATUS status;

	become_root();
	status = pdb->get_account_policy(pdb, type, value);
	unbecome_root();

	return NT_STATUS_IS_OK(status);	
}

bool pdb_set_account_policy(enum pdb_policy_type type, uint32_t value)
{
	struct pdb_methods *pdb = pdb_get_methods();
	NTSTATUS status;

	become_root();
	status = pdb->set_account_policy(pdb, type, value);
	unbecome_root();

	return NT_STATUS_IS_OK(status);
}

bool pdb_get_seq_num(time_t *seq_num)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return NT_STATUS_IS_OK(pdb->get_seq_num(pdb, seq_num));
}

bool pdb_uid_to_sid(uid_t uid, struct dom_sid *sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->uid_to_sid(pdb, uid, sid);
}

bool pdb_gid_to_sid(gid_t gid, struct dom_sid *sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->gid_to_sid(pdb, gid, sid);
}

bool pdb_sid_to_id(const struct dom_sid *sid, union unid_t *id,
		   enum lsa_SidType *type)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->sid_to_id(pdb, sid, id, type);
}

uint32_t pdb_capabilities(void)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->capabilities(pdb);
}

/********************************************************************
 Allocate a new RID from the passdb backend.  Verify that it is free
 by calling lookup_global_sam_rid() to verify that the RID is not
 in use.  This handles servers that have existing users or groups
 with add RIDs (assigned from previous algorithmic mappings)
********************************************************************/

bool pdb_new_rid(uint32_t *rid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	const char *name = NULL;
	enum lsa_SidType type;
	uint32_t allocated_rid = 0;
	int i;
	TALLOC_CTX *ctx;

	if ((pdb_capabilities() & PDB_CAP_STORE_RIDS) == 0) {
		DEBUG(0, ("Trying to allocate a RID when algorithmic RIDs "
			  "are active\n"));
		return False;
	}

	if (algorithmic_rid_base() != BASE_RID) {
		DEBUG(0, ("'algorithmic rid base' is set but a passdb backend "
			  "without algorithmic RIDs is chosen.\n"));
		DEBUGADD(0, ("Please map all used groups using 'net groupmap "
			     "add', set the maximum used RID\n"));
		DEBUGADD(0, ("and remove the parameter\n"));
		return False;
	}

	if ( (ctx = talloc_init("pdb_new_rid")) == NULL ) {
		DEBUG(0,("pdb_new_rid: Talloc initialization failure\n"));
		return False;
	}

	/* Attempt to get an unused RID (max tires is 250...yes that it is 
	   and arbitrary number I pulkled out of my head).   -- jerry */

	for ( i=0; allocated_rid==0 && i<250; i++ ) {
		/* get a new RID */

		if ( !pdb->new_rid(pdb, &allocated_rid) ) {
			return False;
		}

		/* validate that the RID is not in use */

		if ( lookup_global_sam_rid( ctx, allocated_rid, &name, &type, NULL ) ) {
			allocated_rid = 0;
		}
	}

	TALLOC_FREE( ctx );

	if ( allocated_rid == 0 ) {
		DEBUG(0,("pdb_new_rid: Failed to find unused RID\n"));
		return False;
	}

	*rid = allocated_rid;

	return True;
}

/***************************************************************
  Initialize the static context (at smbd startup etc). 

  If uninitialised, context will auto-init on first use.
 ***************************************************************/

bool initialize_password_db(bool reload, struct event_context *event_ctx)
{
	pdb_event_ctx = event_ctx;
	return (pdb_get_methods_reload(reload) != NULL);
}


/***************************************************************************
  Default implementations of some functions.
 ****************************************************************************/

static NTSTATUS pdb_default_getsampwnam (struct pdb_methods *methods, struct samu *user, const char *sname)
{
	return NT_STATUS_NO_SUCH_USER;
}

static NTSTATUS pdb_default_getsampwsid(struct pdb_methods *my_methods, struct samu * user, const struct dom_sid *sid)
{
	return NT_STATUS_NO_SUCH_USER;
}

static NTSTATUS pdb_default_add_sam_account (struct pdb_methods *methods, struct samu *newpwd)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_update_sam_account (struct pdb_methods *methods, struct samu *newpwd)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_delete_sam_account (struct pdb_methods *methods, struct samu *pwd)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_rename_sam_account (struct pdb_methods *methods, struct samu *pwd, const char *newname)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_update_login_attempts (struct pdb_methods *methods, struct samu *newpwd, bool success)
{
	/* Only the pdb_nds backend implements this, by
	 * default just return ok. */
	return NT_STATUS_OK;
}

static NTSTATUS pdb_default_get_account_policy(struct pdb_methods *methods, enum pdb_policy_type type, uint32_t *value)
{
	return account_policy_get(type, value) ? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS pdb_default_set_account_policy(struct pdb_methods *methods, enum pdb_policy_type type, uint32_t value)
{
	return account_policy_set(type, value) ? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS pdb_default_get_seq_num(struct pdb_methods *methods, time_t *seq_num)
{
	*seq_num = time(NULL);
	return NT_STATUS_OK;
}

static bool pdb_default_uid_to_sid(struct pdb_methods *methods, uid_t uid,
				   struct dom_sid *sid)
{
	struct samu *sampw = NULL;
	struct passwd *unix_pw;
	bool ret;

	unix_pw = sys_getpwuid( uid );

	if ( !unix_pw ) {
		DEBUG(4,("pdb_default_uid_to_sid: host has no idea of uid "
			 "%lu\n", (unsigned long)uid));
		return False;
	}

	if ( !(sampw = samu_new( NULL )) ) {
		DEBUG(0,("pdb_default_uid_to_sid: samu_new() failed!\n"));
		return False;
	}

	become_root();
	ret = NT_STATUS_IS_OK(
		methods->getsampwnam(methods, sampw, unix_pw->pw_name ));
	unbecome_root();

	if (!ret) {
		DEBUG(5, ("pdb_default_uid_to_sid: Did not find user "
			  "%s (%u)\n", unix_pw->pw_name, (unsigned int)uid));
		TALLOC_FREE(sampw);
		return False;
	}

	sid_copy(sid, pdb_get_user_sid(sampw));

	TALLOC_FREE(sampw);

	return True;
}

static bool pdb_default_gid_to_sid(struct pdb_methods *methods, gid_t gid,
				   struct dom_sid *sid)
{
	GROUP_MAP map;

	if (!NT_STATUS_IS_OK(methods->getgrgid(methods, &map, gid))) {
		return False;
	}

	sid_copy(sid, &map.sid);
	return True;
}

static bool pdb_default_sid_to_id(struct pdb_methods *methods,
				  const struct dom_sid *sid,
				  union unid_t *id, enum lsa_SidType *type)
{
	TALLOC_CTX *mem_ctx;
	bool ret = False;
	const char *name;
	uint32_t rid;

	mem_ctx = talloc_new(NULL);

	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return False;
	}

	if (sid_peek_check_rid(get_global_sam_sid(), sid, &rid)) {
		/* Here we might have users as well as groups and aliases */
		ret = lookup_global_sam_rid(mem_ctx, rid, &name, type, id);
		goto done;
	}

	/* check for "Unix User" */

	if ( sid_peek_check_rid(&global_sid_Unix_Users, sid, &rid) ) {
		id->uid = rid;
		*type = SID_NAME_USER;
		ret = True;		
		goto done;		
	}

	/* check for "Unix Group" */

	if ( sid_peek_check_rid(&global_sid_Unix_Groups, sid, &rid) ) {
		id->gid = rid;
		*type = SID_NAME_ALIAS;
		ret = True;		
		goto done;		
	}

	/* BUILTIN */

	if (sid_check_is_in_builtin(sid) ||
	    sid_check_is_in_wellknown_domain(sid)) {
		/* Here we only have aliases */
		GROUP_MAP map;
		if (!NT_STATUS_IS_OK(methods->getgrsid(methods, &map, *sid))) {
			DEBUG(10, ("Could not find map for sid %s\n",
				   sid_string_dbg(sid)));
			goto done;
		}
		if ((map.sid_name_use != SID_NAME_ALIAS) &&
		    (map.sid_name_use != SID_NAME_WKN_GRP)) {
			DEBUG(10, ("Map for sid %s is a %s, expected an "
				   "alias\n", sid_string_dbg(sid),
				   sid_type_lookup(map.sid_name_use)));
			goto done;
		}

		id->gid = map.gid;
		*type = SID_NAME_ALIAS;
		ret = True;
		goto done;
	}

	DEBUG(5, ("Sid %s is neither ours, a Unix SID, nor builtin\n",
		  sid_string_dbg(sid)));

 done:

	TALLOC_FREE(mem_ctx);
	return ret;
}

static bool get_memberuids(TALLOC_CTX *mem_ctx, gid_t gid, uid_t **pp_uids, uint32_t *p_num)
{
	struct group *grp;
	char **gr;
	struct passwd *pwd;
	bool winbind_env;
	bool ret = False;
 
	*pp_uids = NULL;
	*p_num = 0;

	/* We only look at our own sam, so don't care about imported stuff */
	winbind_env = winbind_env_set();
	(void)winbind_off();

	if ((grp = getgrgid(gid)) == NULL) {
		/* allow winbindd lookups, but only if they weren't already disabled */
		goto done;
	}

	/* Primary group members */
	setpwent();
	while ((pwd = getpwent()) != NULL) {
		if (pwd->pw_gid == gid) {
			if (!add_uid_to_array_unique(mem_ctx, pwd->pw_uid,
						pp_uids, p_num)) {
				goto done;
			}
		}
	}
	endpwent();

	/* Secondary group members */
	for (gr = grp->gr_mem; (*gr != NULL) && ((*gr)[0] != '\0'); gr += 1) {
		struct passwd *pw = getpwnam(*gr);

		if (pw == NULL)
			continue;
		if (!add_uid_to_array_unique(mem_ctx, pw->pw_uid, pp_uids, p_num)) {
			goto done;
		}
	}

	ret = True;

  done:

	/* allow winbindd lookups, but only if they weren't already disabled */
	if (!winbind_env) {
		(void)winbind_on();
	}

	return ret;
}

static NTSTATUS pdb_default_enum_group_members(struct pdb_methods *methods,
					       TALLOC_CTX *mem_ctx,
					       const struct dom_sid *group,
					       uint32_t **pp_member_rids,
					       size_t *p_num_members)
{
	gid_t gid;
	uid_t *uids;
	uint32_t i, num_uids;

	*pp_member_rids = NULL;
	*p_num_members = 0;

	if (!sid_to_gid(group, &gid))
		return NT_STATUS_NO_SUCH_GROUP;

	if(!get_memberuids(mem_ctx, gid, &uids, &num_uids))
		return NT_STATUS_NO_SUCH_GROUP;

	if (num_uids == 0)
		return NT_STATUS_OK;

	*pp_member_rids = TALLOC_ZERO_ARRAY(mem_ctx, uint32_t, num_uids);

	for (i=0; i<num_uids; i++) {
		struct dom_sid sid;

		uid_to_sid(&sid, uids[i]);

		if (!sid_check_is_in_our_domain(&sid)) {
			DEBUG(5, ("Inconsistent SAM -- group member uid not "
				  "in our domain\n"));
			continue;
		}

		sid_peek_rid(&sid, &(*pp_member_rids)[*p_num_members]);
		*p_num_members += 1;
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_default_enum_group_memberships(struct pdb_methods *methods,
						   TALLOC_CTX *mem_ctx,
						   struct samu *user,
						   struct dom_sid **pp_sids,
						   gid_t **pp_gids,
						   uint32_t *p_num_groups)
{
	size_t i;
	gid_t gid;
	struct passwd *pw;
	const char *username = pdb_get_username(user);


	/* Ignore the primary group SID.  Honor the real Unix primary group.
	   The primary group SID is only of real use to Windows clients */

	if ( !(pw = Get_Pwnam_alloc(mem_ctx, username)) ) {
		return NT_STATUS_NO_SUCH_USER;
	}

	gid = pw->pw_gid;

	TALLOC_FREE( pw );

	if (!getgroups_unix_user(mem_ctx, username, gid, pp_gids, p_num_groups)) {
		return NT_STATUS_NO_SUCH_USER;
	}

	if (*p_num_groups == 0) {
		smb_panic("primary group missing");
	}

	*pp_sids = TALLOC_ARRAY(mem_ctx, struct dom_sid, *p_num_groups);

	if (*pp_sids == NULL) {
		TALLOC_FREE(*pp_gids);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<*p_num_groups; i++) {
		gid_to_sid(&(*pp_sids)[i], (*pp_gids)[i]);
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Look up a rid in the SAM we're responsible for (i.e. passdb)
 ********************************************************************/

static bool lookup_global_sam_rid(TALLOC_CTX *mem_ctx, uint32_t rid,
				  const char **name,
				  enum lsa_SidType *psid_name_use,
				  union unid_t *unix_id)
{
	struct samu *sam_account = NULL;
	GROUP_MAP map;
	bool ret;
	struct dom_sid sid;

	*psid_name_use = SID_NAME_UNKNOWN;

	DEBUG(5,("lookup_global_sam_rid: looking up RID %u.\n",
		 (unsigned int)rid));

	sid_compose(&sid, get_global_sam_sid(), rid);

	/* see if the passdb can help us with the name of the user */

	if ( !(sam_account = samu_new( NULL )) ) {
		return False;
	}

	/* BEING ROOT BLOCK */
	become_root();
	if (pdb_getsampwsid(sam_account, &sid)) {
		struct passwd *pw;

		unbecome_root();		/* -----> EXIT BECOME_ROOT() */
		*name = talloc_strdup(mem_ctx, pdb_get_username(sam_account));
		if (!*name) {
			TALLOC_FREE(sam_account);
			return False;
		}

		*psid_name_use = SID_NAME_USER;

		TALLOC_FREE(sam_account);

		if (unix_id == NULL) {
			return True;
		}

		pw = Get_Pwnam_alloc(talloc_tos(), *name);
		if (pw == NULL) {
			return False;
		}
		unix_id->uid = pw->pw_uid;
		TALLOC_FREE(pw);
		return True;
	}
	TALLOC_FREE(sam_account);

	ret = pdb_getgrsid(&map, sid);
	unbecome_root();
	/* END BECOME_ROOT BLOCK */

	/* do not resolve SIDs to a name unless there is a valid 
	   gid associated with it */

	if ( ret && (map.gid != (gid_t)-1) ) {
		*name = talloc_strdup(mem_ctx, map.nt_name);
		*psid_name_use = map.sid_name_use;

		if ( unix_id ) {
			unix_id->gid = map.gid;
		}

		return True;
	}

	/* Windows will always map RID 513 to something.  On a non-domain 
	   controller, this gets mapped to SERVER\None. */

	if ( unix_id ) {
		DEBUG(5, ("Can't find a unix id for an unmapped group\n"));
		return False;
	}

	if ( rid == DOMAIN_RID_USERS ) {
		*name = talloc_strdup(mem_ctx, "None" );
		*psid_name_use = SID_NAME_DOM_GRP;

		return True;
	}

	return False;
}

static NTSTATUS pdb_default_lookup_rids(struct pdb_methods *methods,
					const struct dom_sid *domain_sid,
					int num_rids,
					uint32_t *rids,
					const char **names,
					enum lsa_SidType *attrs)
{
	int i;
	NTSTATUS result;
	bool have_mapped = False;
	bool have_unmapped = False;

	if (sid_check_is_builtin(domain_sid)) {

		for (i=0; i<num_rids; i++) {
			const char *name;

			if (lookup_builtin_rid(names, rids[i], &name)) {
				attrs[i] = SID_NAME_ALIAS;
				names[i] = name;
				DEBUG(5,("lookup_rids: %s:%d\n",
					 names[i], attrs[i]));
				have_mapped = True;
			} else {
				have_unmapped = True;
				attrs[i] = SID_NAME_UNKNOWN;
			}
		}
		goto done;
	}

	/* Should not happen, but better check once too many */
	if (!sid_check_is_domain(domain_sid)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	for (i = 0; i < num_rids; i++) {
		const char *name;

		if (lookup_global_sam_rid(names, rids[i], &name, &attrs[i],
					  NULL)) {
			if (name == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			names[i] = name;
			DEBUG(5,("lookup_rids: %s:%d\n", names[i], attrs[i]));
			have_mapped = True;
		} else {
			have_unmapped = True;
			attrs[i] = SID_NAME_UNKNOWN;
		}
	}

 done:

	result = NT_STATUS_NONE_MAPPED;

	if (have_mapped)
		result = have_unmapped ? STATUS_SOME_UNMAPPED : NT_STATUS_OK;

	return result;
}

#if 0
static NTSTATUS pdb_default_lookup_names(struct pdb_methods *methods,
					 const struct dom_sid *domain_sid,
					 int num_names,
					 const char **names,
					 uint32_t *rids,
					 enum lsa_SidType *attrs)
{
	int i;
	NTSTATUS result;
	bool have_mapped = False;
	bool have_unmapped = False;

	if (sid_check_is_builtin(domain_sid)) {

		for (i=0; i<num_names; i++) {
			uint32_t rid;

			if (lookup_builtin_name(names[i], &rid)) {
				attrs[i] = SID_NAME_ALIAS;
				rids[i] = rid;
				DEBUG(5,("lookup_rids: %s:%d\n",
					 names[i], attrs[i]));
				have_mapped = True;
			} else {
				have_unmapped = True;
				attrs[i] = SID_NAME_UNKNOWN;
			}
		}
		goto done;
	}

	/* Should not happen, but better check once too many */
	if (!sid_check_is_domain(domain_sid)) {
		return NT_STATUS_INVALID_HANDLE;
	}

	for (i = 0; i < num_names; i++) {
		if (lookup_global_sam_name(names[i], 0, &rids[i], &attrs[i])) {
			DEBUG(5,("lookup_names: %s-> %d:%d\n", names[i],
				 rids[i], attrs[i]));
			have_mapped = True;
		} else {
			have_unmapped = True;
			attrs[i] = SID_NAME_UNKNOWN;
		}
	}

 done:

	result = NT_STATUS_NONE_MAPPED;

	if (have_mapped)
		result = have_unmapped ? STATUS_SOME_UNMAPPED : NT_STATUS_OK;

	return result;
}
#endif

static int pdb_search_destructor(struct pdb_search *search)
{
	if ((!search->search_ended) && (search->search_end != NULL)) {
		search->search_end(search);
	}
	return 0;
}

struct pdb_search *pdb_search_init(TALLOC_CTX *mem_ctx,
				   enum pdb_search_type type)
{
	struct pdb_search *result;

	result = talloc(mem_ctx, struct pdb_search);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->type = type;
	result->cache = NULL;
	result->num_entries = 0;
	result->cache_size = 0;
	result->search_ended = False;
	result->search_end = NULL;

	/* Segfault appropriately if not initialized */
	result->next_entry = NULL;
	result->search_end = NULL;

	talloc_set_destructor(result, pdb_search_destructor);

	return result;
}

static void fill_displayentry(TALLOC_CTX *mem_ctx, uint32_t rid,
			      uint16_t acct_flags,
			      const char *account_name,
			      const char *fullname,
			      const char *description,
			      struct samr_displayentry *entry)
{
	entry->rid = rid;
	entry->acct_flags = acct_flags;

	if (account_name != NULL)
		entry->account_name = talloc_strdup(mem_ctx, account_name);
	else
		entry->account_name = "";

	if (fullname != NULL)
		entry->fullname = talloc_strdup(mem_ctx, fullname);
	else
		entry->fullname = "";

	if (description != NULL)
		entry->description = talloc_strdup(mem_ctx, description);
	else
		entry->description = "";
}

struct group_search {
	GROUP_MAP *groups;
	size_t num_groups, current_group;
};

static bool next_entry_groups(struct pdb_search *s,
			      struct samr_displayentry *entry)
{
	struct group_search *state = (struct group_search *)s->private_data;
	uint32_t rid;
	GROUP_MAP *map = &state->groups[state->current_group];

	if (state->current_group == state->num_groups)
		return False;

	sid_peek_rid(&map->sid, &rid);

	fill_displayentry(s, rid, 0, map->nt_name, NULL, map->comment, entry);

	state->current_group += 1;
	return True;
}

static void search_end_groups(struct pdb_search *search)
{
	struct group_search *state =
		(struct group_search *)search->private_data;
	SAFE_FREE(state->groups);
}

static bool pdb_search_grouptype(struct pdb_search *search,
				 const struct dom_sid *sid, enum lsa_SidType type)
{
	struct group_search *state;

	state = talloc(search, struct group_search);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	if (!pdb_enum_group_mapping(sid, type, &state->groups, &state->num_groups,
				    True)) {
		DEBUG(0, ("Could not enum groups\n"));
		return False;
	}

	state->current_group = 0;
	search->private_data = state;
	search->next_entry = next_entry_groups;
	search->search_end = search_end_groups;
	return True;
}

static bool pdb_default_search_groups(struct pdb_methods *methods,
				      struct pdb_search *search)
{
	return pdb_search_grouptype(search, get_global_sam_sid(), SID_NAME_DOM_GRP);
}

static bool pdb_default_search_aliases(struct pdb_methods *methods,
				       struct pdb_search *search,
				       const struct dom_sid *sid)
{

	return pdb_search_grouptype(search, sid, SID_NAME_ALIAS);
}

static struct samr_displayentry *pdb_search_getentry(struct pdb_search *search,
						     uint32_t idx)
{
	if (idx < search->num_entries)
		return &search->cache[idx];

	if (search->search_ended)
		return NULL;

	while (idx >= search->num_entries) {
		struct samr_displayentry entry;

		if (!search->next_entry(search, &entry)) {
			search->search_end(search);
			search->search_ended = True;
			break;
		}

		ADD_TO_LARGE_ARRAY(search, struct samr_displayentry,
				   entry, &search->cache, &search->num_entries,
				   &search->cache_size);
	}

	return (search->num_entries > idx) ? &search->cache[idx] : NULL;
}

struct pdb_search *pdb_search_users(TALLOC_CTX *mem_ctx, uint32_t acct_flags)
{
	struct pdb_methods *pdb = pdb_get_methods();
	struct pdb_search *result;

	result = pdb_search_init(mem_ctx, PDB_USER_SEARCH);
	if (result == NULL) {
		return NULL;
	}

	if (!pdb->search_users(pdb, result, acct_flags)) {
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

struct pdb_search *pdb_search_groups(TALLOC_CTX *mem_ctx)
{
	struct pdb_methods *pdb = pdb_get_methods();
	struct pdb_search *result;

	result = pdb_search_init(mem_ctx, PDB_GROUP_SEARCH);
	if (result == NULL) {
		 return NULL;
	}

	if (!pdb->search_groups(pdb, result)) {
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

struct pdb_search *pdb_search_aliases(TALLOC_CTX *mem_ctx, const struct dom_sid *sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	struct pdb_search *result;

	if (pdb == NULL) return NULL;

	result = pdb_search_init(mem_ctx, PDB_ALIAS_SEARCH);
	if (result == NULL) {
		return NULL;
	}

	if (!pdb->search_aliases(pdb, result, sid)) {
		TALLOC_FREE(result);
		return NULL;
	}
	return result;
}

uint32_t pdb_search_entries(struct pdb_search *search,
			  uint32_t start_idx, uint32_t max_entries,
			  struct samr_displayentry **result)
{
	struct samr_displayentry *end_entry;
	uint32_t end_idx = start_idx+max_entries-1;

	/* The first entry needs to be searched after the last. Otherwise the
	 * first entry might have moved due to a realloc during the search for
	 * the last entry. */

	end_entry = pdb_search_getentry(search, end_idx);
	*result = pdb_search_getentry(search, start_idx);

	if (end_entry != NULL)
		return max_entries;

	if (start_idx >= search->num_entries)
		return 0;

	return search->num_entries - start_idx;
}

/*******************************************************************
 trustdom methods
 *******************************************************************/

bool pdb_get_trusteddom_pw(const char *domain, char** pwd, struct dom_sid *sid,
			   time_t *pass_last_set_time)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->get_trusteddom_pw(pdb, domain, pwd, sid, 
			pass_last_set_time);
}

bool pdb_set_trusteddom_pw(const char* domain, const char* pwd,
			   const struct dom_sid *sid)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->set_trusteddom_pw(pdb, domain, pwd, sid);
}

bool pdb_del_trusteddom_pw(const char *domain)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->del_trusteddom_pw(pdb, domain);
}

NTSTATUS pdb_enum_trusteddoms(TALLOC_CTX *mem_ctx, uint32_t *num_domains,
			      struct trustdom_info ***domains)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->enum_trusteddoms(pdb, mem_ctx, num_domains, domains);
}

/*******************************************************************
 the defaults for trustdom methods: 
 these simply call the original passdb/secrets.c actions,
 to be replaced by pdb_ldap.
 *******************************************************************/

static bool pdb_default_get_trusteddom_pw(struct pdb_methods *methods,
					  const char *domain, 
					  char** pwd, 
					  struct dom_sid *sid,
	        	 		  time_t *pass_last_set_time)
{
	return secrets_fetch_trusted_domain_password(domain, pwd,
				sid, pass_last_set_time);

}

static bool pdb_default_set_trusteddom_pw(struct pdb_methods *methods, 
					  const char* domain, 
					  const char* pwd,
					  const struct dom_sid *sid)
{
	return secrets_store_trusted_domain_password(domain, pwd, sid);
}

static bool pdb_default_del_trusteddom_pw(struct pdb_methods *methods, 
					  const char *domain)
{
	return trusted_domain_password_delete(domain);
}

static NTSTATUS pdb_default_enum_trusteddoms(struct pdb_methods *methods,
					     TALLOC_CTX *mem_ctx, 
					     uint32_t *num_domains,
					     struct trustdom_info ***domains)
{
	return secrets_trusted_domains(mem_ctx, num_domains, domains);
}

/*******************************************************************
 trusted_domain methods
 *******************************************************************/

NTSTATUS pdb_get_trusted_domain(TALLOC_CTX *mem_ctx, const char *domain,
				struct pdb_trusted_domain **td)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->get_trusted_domain(pdb, mem_ctx, domain, td);
}

NTSTATUS pdb_get_trusted_domain_by_sid(TALLOC_CTX *mem_ctx, struct dom_sid *sid,
				struct pdb_trusted_domain **td)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->get_trusted_domain_by_sid(pdb, mem_ctx, sid, td);
}

NTSTATUS pdb_set_trusted_domain(const char* domain,
				const struct pdb_trusted_domain *td)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->set_trusted_domain(pdb, domain, td);
}

NTSTATUS pdb_del_trusted_domain(const char *domain)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->del_trusted_domain(pdb, domain);
}

NTSTATUS pdb_enum_trusted_domains(TALLOC_CTX *mem_ctx, uint32_t *num_domains,
				  struct pdb_trusted_domain ***domains)
{
	struct pdb_methods *pdb = pdb_get_methods();
	return pdb->enum_trusted_domains(pdb, mem_ctx, num_domains, domains);
}

static NTSTATUS pdb_default_get_trusted_domain(struct pdb_methods *methods,
					       TALLOC_CTX *mem_ctx,
					       const char *domain,
					       struct pdb_trusted_domain **td)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_get_trusted_domain_by_sid(struct pdb_methods *methods,
						      TALLOC_CTX *mem_ctx,
						      struct dom_sid *sid,
						      struct pdb_trusted_domain **td)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_set_trusted_domain(struct pdb_methods *methods,
					       const char* domain,
					       const struct pdb_trusted_domain *td)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_del_trusted_domain(struct pdb_methods *methods,
					       const char *domain)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_default_enum_trusted_domains(struct pdb_methods *methods,
						 TALLOC_CTX *mem_ctx,
						 uint32_t *num_domains,
						 struct pdb_trusted_domain ***domains)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static struct pdb_domain_info *pdb_default_get_domain_info(
	struct pdb_methods *m, TALLOC_CTX *mem_ctx)
{
	return NULL;
}

/*******************************************************************
 Create a pdb_methods structure and initialize it with the default
 operations.  In this way a passdb module can simply implement
 the functionality it cares about.  However, normally this is done 
 in groups of related functions.
*******************************************************************/

NTSTATUS make_pdb_method( struct pdb_methods **methods ) 
{
	/* allocate memory for the structure as its own talloc CTX */

	*methods = talloc_zero(NULL, struct pdb_methods);
	if (*methods == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	(*methods)->get_domain_info = pdb_default_get_domain_info;
	(*methods)->getsampwnam = pdb_default_getsampwnam;
	(*methods)->getsampwsid = pdb_default_getsampwsid;
	(*methods)->create_user = pdb_default_create_user;
	(*methods)->delete_user = pdb_default_delete_user;
	(*methods)->add_sam_account = pdb_default_add_sam_account;
	(*methods)->update_sam_account = pdb_default_update_sam_account;
	(*methods)->delete_sam_account = pdb_default_delete_sam_account;
	(*methods)->rename_sam_account = pdb_default_rename_sam_account;
	(*methods)->update_login_attempts = pdb_default_update_login_attempts;

	(*methods)->getgrsid = pdb_default_getgrsid;
	(*methods)->getgrgid = pdb_default_getgrgid;
	(*methods)->getgrnam = pdb_default_getgrnam;
	(*methods)->create_dom_group = pdb_default_create_dom_group;
	(*methods)->delete_dom_group = pdb_default_delete_dom_group;
	(*methods)->add_group_mapping_entry = pdb_default_add_group_mapping_entry;
	(*methods)->update_group_mapping_entry = pdb_default_update_group_mapping_entry;
	(*methods)->delete_group_mapping_entry = pdb_default_delete_group_mapping_entry;
	(*methods)->enum_group_mapping = pdb_default_enum_group_mapping;
	(*methods)->enum_group_members = pdb_default_enum_group_members;
	(*methods)->enum_group_memberships = pdb_default_enum_group_memberships;
	(*methods)->set_unix_primary_group = pdb_default_set_unix_primary_group;
	(*methods)->add_groupmem = pdb_default_add_groupmem;
	(*methods)->del_groupmem = pdb_default_del_groupmem;
	(*methods)->create_alias = pdb_default_create_alias;
	(*methods)->delete_alias = pdb_default_delete_alias;
	(*methods)->get_aliasinfo = pdb_default_get_aliasinfo;
	(*methods)->set_aliasinfo = pdb_default_set_aliasinfo;
	(*methods)->add_aliasmem = pdb_default_add_aliasmem;
	(*methods)->del_aliasmem = pdb_default_del_aliasmem;
	(*methods)->enum_aliasmem = pdb_default_enum_aliasmem;
	(*methods)->enum_alias_memberships = pdb_default_alias_memberships;
	(*methods)->lookup_rids = pdb_default_lookup_rids;
	(*methods)->get_account_policy = pdb_default_get_account_policy;
	(*methods)->set_account_policy = pdb_default_set_account_policy;
	(*methods)->get_seq_num = pdb_default_get_seq_num;
	(*methods)->uid_to_sid = pdb_default_uid_to_sid;
	(*methods)->gid_to_sid = pdb_default_gid_to_sid;
	(*methods)->sid_to_id = pdb_default_sid_to_id;

	(*methods)->search_groups = pdb_default_search_groups;
	(*methods)->search_aliases = pdb_default_search_aliases;

	(*methods)->get_trusteddom_pw = pdb_default_get_trusteddom_pw;
	(*methods)->set_trusteddom_pw = pdb_default_set_trusteddom_pw;
	(*methods)->del_trusteddom_pw = pdb_default_del_trusteddom_pw;
	(*methods)->enum_trusteddoms  = pdb_default_enum_trusteddoms;

	(*methods)->get_trusted_domain = pdb_default_get_trusted_domain;
	(*methods)->get_trusted_domain_by_sid = pdb_default_get_trusted_domain_by_sid;
	(*methods)->set_trusted_domain = pdb_default_set_trusted_domain;
	(*methods)->del_trusted_domain = pdb_default_del_trusted_domain;
	(*methods)->enum_trusted_domains = pdb_default_enum_trusted_domains;

	return NT_STATUS_OK;
}
