/* 
   Unix SMB/CIFS implementation.
   LDAP protocol helper functions for SAMBA
   Copyright (C) Jean Fran�ois Micouleau	1998
   Copyright (C) Gerald Carter			2001-2003
   Copyright (C) Shahms King			2001
   Copyright (C) Andrew Bartlett		2002-2003
   Copyright (C) Stefan (metze) Metzmacher	2002-2003
   Copyright (C) Simo Sorce			2006

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

/* TODO:
*  persistent connections: if using NSS LDAP, many connections are made
*      however, using only one within Samba would be nice
*  
*  Clean up SSL stuff, compile on OpenLDAP 1.x, 2.x, and Netscape SDK
*
*  Other LDAP based login attributes: accountExpires, etc.
*  (should be the domain of Samba proper, but the sam_password/struct samu
*  structures don't have fields for some of these attributes)
*
*  SSL is done, but can't get the certificate based authentication to work
*  against on my test platform (Linux 2.4, OpenLDAP 2.x)
*/

/* NOTE: this will NOT work against an Active Directory server
*  due to the fact that the two password fields cannot be retrieved
*  from a server; recommend using security = domain in this situation
*  and/or winbind
*/

#include "includes.h"
#include "passdb.h"
#include "../libcli/auth/libcli_auth.h"
#include "secrets.h"
#include "idmap_cache.h"
#include "../libcli/security/security.h"
#include "../lib/util/util_pw.h"
#include "lib/winbind_util.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_PASSDB

#include <lber.h>
#include <ldap.h>


#include "smbldap.h"

/**********************************************************************
 Simple helper function to make stuff better readable
 **********************************************************************/

LDAP *priv2ld(struct ldapsam_privates *priv)
{
	return priv->smbldap_state->ldap_struct;
}

/**********************************************************************
 Get the attribute name given a user schame version.
 **********************************************************************/
 
static const char* get_userattr_key2string( int schema_ver, int key )
{
	switch ( schema_ver ) {
		case SCHEMAVER_SAMBAACCOUNT:
			return get_attr_key2string( attrib_map_v22, key );

		case SCHEMAVER_SAMBASAMACCOUNT:
			return get_attr_key2string( attrib_map_v30, key );

		default:
			DEBUG(0,("get_userattr_key2string: unknown schema version specified\n"));
			break;
	}
	return NULL;
}

/**********************************************************************
 Return the list of attribute names given a user schema version.
**********************************************************************/

const char** get_userattr_list( TALLOC_CTX *mem_ctx, int schema_ver )
{
	switch ( schema_ver ) {
		case SCHEMAVER_SAMBAACCOUNT:
			return get_attr_list( mem_ctx, attrib_map_v22 );

		case SCHEMAVER_SAMBASAMACCOUNT:
			return get_attr_list( mem_ctx, attrib_map_v30 );
		default:
			DEBUG(0,("get_userattr_list: unknown schema version specified!\n"));
			break;
	}

	return NULL;
}

/**************************************************************************
 Return the list of attribute names to delete given a user schema version.
**************************************************************************/

static const char** get_userattr_delete_list( TALLOC_CTX *mem_ctx,
					      int schema_ver )
{
	switch ( schema_ver ) {
		case SCHEMAVER_SAMBAACCOUNT:
			return get_attr_list( mem_ctx,
					      attrib_map_to_delete_v22 );

		case SCHEMAVER_SAMBASAMACCOUNT:
			return get_attr_list( mem_ctx,
					      attrib_map_to_delete_v30 );
		default:
			DEBUG(0,("get_userattr_delete_list: unknown schema version specified!\n"));
			break;
	}

	return NULL;
}


/*******************************************************************
 Generate the LDAP search filter for the objectclass based on the 
 version of the schema we are using.
******************************************************************/

static const char* get_objclass_filter( int schema_ver )
{
	fstring objclass_filter;
	char *result;

	switch( schema_ver ) {
		case SCHEMAVER_SAMBAACCOUNT:
			fstr_sprintf( objclass_filter, "(objectclass=%s)", LDAP_OBJ_SAMBAACCOUNT );
			break;
		case SCHEMAVER_SAMBASAMACCOUNT:
			fstr_sprintf( objclass_filter, "(objectclass=%s)", LDAP_OBJ_SAMBASAMACCOUNT );
			break;
		default:
			DEBUG(0,("get_objclass_filter: Invalid schema version specified!\n"));
			objclass_filter[0] = '\0';
			break;
	}

	result = talloc_strdup(talloc_tos(), objclass_filter);
	SMB_ASSERT(result != NULL);
	return result;
}

/*****************************************************************
 Scan a sequence number off OpenLDAP's syncrepl contextCSN
******************************************************************/

static NTSTATUS ldapsam_get_seq_num(struct pdb_methods *my_methods, time_t *seq_num)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	NTSTATUS ntstatus = NT_STATUS_UNSUCCESSFUL;
	LDAPMessage *msg = NULL;
	LDAPMessage *entry = NULL;
	TALLOC_CTX *mem_ctx;
	char **values = NULL;
	int rc, num_result, num_values, rid;
	char *suffix = NULL;
	char *tok;
	const char *p;
	const char **attrs;

	/* Unfortunatly there is no proper way to detect syncrepl-support in
	 * smbldap_connect_system(). The syncrepl OIDs are submitted for publication
	 * but do not show up in the root-DSE yet. Neither we can query the
	 * subschema-context for the syncProviderSubentry or syncConsumerSubentry
	 * objectclass. Currently we require lp_ldap_suffix() to show up as
	 * namingContext.  -  Guenther
	 */

	if (!lp_parm_bool(-1, "ldapsam", "syncrepl_seqnum", False)) {
		return ntstatus;
	}

	if (!seq_num) {
		DEBUG(3,("ldapsam_get_seq_num: no sequence_number\n"));
		return ntstatus;
	}

	if (!smbldap_has_naming_context(ldap_state->smbldap_state->ldap_struct, lp_ldap_suffix())) {
		DEBUG(3,("ldapsam_get_seq_num: DIT not configured to hold %s "
			 "as top-level namingContext\n", lp_ldap_suffix()));
		return ntstatus;
	}

	mem_ctx = talloc_init("ldapsam_get_seq_num");

	if (mem_ctx == NULL)
		return NT_STATUS_NO_MEMORY;

	if ((attrs = TALLOC_ARRAY(mem_ctx, const char *, 2)) == NULL) {
		ntstatus = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* if we got a syncrepl-rid (up to three digits long) we speak with a consumer */
	rid = lp_parm_int(-1, "ldapsam", "syncrepl_rid", -1);
	if (rid > 0) {

		/* consumer syncreplCookie: */
		/* csn=20050126161620Z#0000001#00#00000 */
		attrs[0] = talloc_strdup(mem_ctx, "syncreplCookie");
		attrs[1] = NULL;
		suffix = talloc_asprintf(mem_ctx,
				"cn=syncrepl%d,%s", rid, lp_ldap_suffix());
		if (!suffix) {
			ntstatus = NT_STATUS_NO_MEMORY;
			goto done;
		}
	} else {

		/* provider contextCSN */
		/* 20050126161620Z#000009#00#000000 */
		attrs[0] = talloc_strdup(mem_ctx, "contextCSN");
		attrs[1] = NULL;
		suffix = talloc_asprintf(mem_ctx,
				"cn=ldapsync,%s", lp_ldap_suffix());

		if (!suffix) {
			ntstatus = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	rc = smbldap_search(ldap_state->smbldap_state, suffix,
			    LDAP_SCOPE_BASE, "(objectclass=*)", attrs, 0, &msg);

	if (rc != LDAP_SUCCESS) {
		goto done;
	}

	num_result = ldap_count_entries(ldap_state->smbldap_state->ldap_struct, msg);
	if (num_result != 1) {
		DEBUG(3,("ldapsam_get_seq_num: Expected one entry, got %d\n", num_result));
		goto done;
	}

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct, msg);
	if (entry == NULL) {
		DEBUG(3,("ldapsam_get_seq_num: Could not retrieve entry\n"));
		goto done;
	}

	values = ldap_get_values(ldap_state->smbldap_state->ldap_struct, entry, attrs[0]);
	if (values == NULL) {
		DEBUG(3,("ldapsam_get_seq_num: no values\n"));
		goto done;
	}

	num_values = ldap_count_values(values);
	if (num_values == 0) {
		DEBUG(3,("ldapsam_get_seq_num: not a single value\n"));
		goto done;
	}

	p = values[0];
	if (!next_token_talloc(mem_ctx, &p, &tok, "#")) {
		DEBUG(0,("ldapsam_get_seq_num: failed to parse sequence number\n"));
		goto done;
	}

	p = tok;
	if (!strncmp(p, "csn=", strlen("csn=")))
		p += strlen("csn=");

	DEBUG(10,("ldapsam_get_seq_num: got %s: %s\n", attrs[0], p));

	*seq_num = generalized_to_unix_time(p);

	/* very basic sanity check */
	if (*seq_num <= 0) {
		DEBUG(3,("ldapsam_get_seq_num: invalid sequence number: %d\n", 
			(int)*seq_num));
		goto done;
	}

	ntstatus = NT_STATUS_OK;

 done:
	if (values != NULL)
		ldap_value_free(values);
	if (msg != NULL)
		ldap_msgfree(msg);
	if (mem_ctx)
		talloc_destroy(mem_ctx);

	return ntstatus;
}

/*******************************************************************
 Run the search by name.
******************************************************************/

int ldapsam_search_suffix_by_name(struct ldapsam_privates *ldap_state,
					  const char *user,
					  LDAPMessage ** result,
					  const char **attr)
{
	char *filter = NULL;
	char *escape_user = escape_ldap_string(talloc_tos(), user);
	int ret = -1;

	if (!escape_user) {
		return LDAP_NO_MEMORY;
	}

	/*
	 * in the filter expression, replace %u with the real name
	 * so in ldap filter, %u MUST exist :-)
	 */
	filter = talloc_asprintf(talloc_tos(), "(&%s%s)", "(uid=%u)",
		get_objclass_filter(ldap_state->schema_ver));
	if (!filter) {
		TALLOC_FREE(escape_user);
		return LDAP_NO_MEMORY;
	}
	/*
	 * have to use this here because $ is filtered out
	 * in string_sub
	 */

	filter = talloc_all_string_sub(talloc_tos(),
				filter, "%u", escape_user);
	TALLOC_FREE(escape_user);
	if (!filter) {
		return LDAP_NO_MEMORY;
	}

	ret = smbldap_search_suffix(ldap_state->smbldap_state,
			filter, attr, result);
	TALLOC_FREE(filter);
	return ret;
}

/*******************************************************************
 Run the search by rid.
******************************************************************/

static int ldapsam_search_suffix_by_rid (struct ldapsam_privates *ldap_state,
					 uint32_t rid, LDAPMessage ** result,
					 const char **attr)
{
	char *filter = NULL;
	int rc;

	filter = talloc_asprintf(talloc_tos(), "(&(rid=%i)%s)", rid,
		get_objclass_filter(ldap_state->schema_ver));
	if (!filter) {
		return LDAP_NO_MEMORY;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state,
			filter, attr, result);
	TALLOC_FREE(filter);
	return rc;
}

/*******************************************************************
 Run the search by SID.
******************************************************************/

static int ldapsam_search_suffix_by_sid (struct ldapsam_privates *ldap_state,
				 const struct dom_sid *sid, LDAPMessage ** result,
				 const char **attr)
{
	char *filter = NULL;
	int rc;
	fstring sid_string;

	filter = talloc_asprintf(talloc_tos(), "(&(%s=%s)%s)",
		get_userattr_key2string(ldap_state->schema_ver,
			LDAP_ATTR_USER_SID),
		sid_to_fstring(sid_string, sid),
		get_objclass_filter(ldap_state->schema_ver));
	if (!filter) {
		return LDAP_NO_MEMORY;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state,
			filter, attr, result);

	TALLOC_FREE(filter);
	return rc;
}

/*******************************************************************
 Delete complete object or objectclass and attrs from
 object found in search_result depending on lp_ldap_delete_dn
******************************************************************/

static int ldapsam_delete_entry(struct ldapsam_privates *priv,
				TALLOC_CTX *mem_ctx,
				LDAPMessage *entry,
				const char *objectclass,
				const char **attrs)
{
	LDAPMod **mods = NULL;
	char *name;
	const char *dn;
	BerElement *ptr = NULL;

	dn = smbldap_talloc_dn(mem_ctx, priv2ld(priv), entry);
	if (dn == NULL) {
		return LDAP_NO_MEMORY;
	}

	if (lp_ldap_delete_dn()) {
		return smbldap_delete(priv->smbldap_state, dn);
	}

	/* Ok, delete only the SAM attributes */

	for (name = ldap_first_attribute(priv2ld(priv), entry, &ptr);
	     name != NULL;
	     name = ldap_next_attribute(priv2ld(priv), entry, ptr)) {
		const char **attrib;

		/* We are only allowed to delete the attributes that
		   really exist. */

		for (attrib = attrs; *attrib != NULL; attrib++) {
			if (strequal(*attrib, name)) {
				DEBUG(10, ("ldapsam_delete_entry: deleting "
					   "attribute %s\n", name));
				smbldap_set_mod(&mods, LDAP_MOD_DELETE, name,
						NULL);
			}
		}
		ldap_memfree(name);
	}

	if (ptr != NULL) {
		ber_free(ptr, 0);
	}

	smbldap_set_mod(&mods, LDAP_MOD_DELETE, "objectClass", objectclass);
	talloc_autofree_ldapmod(mem_ctx, mods);

	return smbldap_modify(priv->smbldap_state, dn, mods);
}

static time_t ldapsam_get_entry_timestamp( struct ldapsam_privates *ldap_state, LDAPMessage * entry)
{
	char *temp;
	struct tm tm;

	temp = smbldap_talloc_single_attribute(ldap_state->smbldap_state->ldap_struct, entry,
			get_userattr_key2string(ldap_state->schema_ver,LDAP_ATTR_MOD_TIMESTAMP),
			talloc_tos());
	if (!temp) {
		return (time_t) 0;
	}

	if ( !strptime(temp, "%Y%m%d%H%M%SZ", &tm)) {
		DEBUG(2,("ldapsam_get_entry_timestamp: strptime failed on: %s\n",
			(char*)temp));
		TALLOC_FREE(temp);
		return (time_t) 0;
	}
	TALLOC_FREE(temp);
	tzset();
	return timegm(&tm);
}

/**********************************************************************
 Initialize struct samu from an LDAP query.
 (Based on init_sam_from_buffer in pdb_tdb.c)
*********************************************************************/

static bool init_sam_from_ldap(struct ldapsam_privates *ldap_state,
				struct samu * sampass,
				LDAPMessage * entry)
{
	time_t  logon_time,
			logoff_time,
			kickoff_time,
			pass_last_set_time,
			pass_can_change_time,
			pass_must_change_time,
			ldap_entry_time,
			bad_password_time;
	char *username = NULL,
			*domain = NULL,
			*nt_username = NULL,
			*fullname = NULL,
			*homedir = NULL,
			*dir_drive = NULL,
			*logon_script = NULL,
			*profile_path = NULL,
			*acct_desc = NULL,
			*workstations = NULL,
			*munged_dial = NULL;
	uint32_t 		user_rid;
	uint8 		smblmpwd[LM_HASH_LEN],
			smbntpwd[NT_HASH_LEN];
	bool 		use_samba_attrs = True;
	uint32_t 		acct_ctrl = 0;
	uint16_t		logon_divs;
	uint16_t 		bad_password_count = 0,
			logon_count = 0;
	uint32_t hours_len;
	uint8 		hours[MAX_HOURS_LEN];
	char *temp = NULL;
	struct login_cache cache_entry;
	uint32_t 		pwHistLen;
	bool expand_explicit = lp_passdb_expand_explicit();
	bool ret = false;
	TALLOC_CTX *ctx = talloc_init("init_sam_from_ldap");

	if (!ctx) {
		return false;
	}
	if (sampass == NULL || ldap_state == NULL || entry == NULL) {
		DEBUG(0, ("init_sam_from_ldap: NULL parameters found!\n"));
		goto fn_exit;
	}

	if (priv2ld(ldap_state) == NULL) {
		DEBUG(0, ("init_sam_from_ldap: ldap_state->smbldap_state->"
			  "ldap_struct is NULL!\n"));
		goto fn_exit;
	}

	if (!(username = smbldap_talloc_first_attribute(priv2ld(ldap_state),
					entry,
					"uid",
					ctx))) {
		DEBUG(1, ("init_sam_from_ldap: No uid attribute found for "
			  "this user!\n"));
		goto fn_exit;
	}

	DEBUG(2, ("init_sam_from_ldap: Entry found for user: %s\n", username));

	nt_username = talloc_strdup(ctx, username);
	if (!nt_username) {
		goto fn_exit;
	}

	domain = talloc_strdup(ctx, ldap_state->domain_name);
	if (!domain) {
		goto fn_exit;
	}

	pdb_set_username(sampass, username, PDB_SET);

	pdb_set_domain(sampass, domain, PDB_DEFAULT);
	pdb_set_nt_username(sampass, nt_username, PDB_SET);

	/* deal with different attributes between the schema first */

	if ( ldap_state->schema_ver == SCHEMAVER_SAMBASAMACCOUNT ) {
		if ((temp = smbldap_talloc_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_userattr_key2string(ldap_state->schema_ver,
					LDAP_ATTR_USER_SID),
				ctx))!=NULL) {
			pdb_set_user_sid_from_string(sampass, temp, PDB_SET);
		}
	} else {
		if ((temp = smbldap_talloc_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_userattr_key2string(ldap_state->schema_ver,
					LDAP_ATTR_USER_RID),
				ctx))!=NULL) {
			user_rid = (uint32_t)atol(temp);
			pdb_set_user_sid_from_rid(sampass, user_rid, PDB_SET);
		}
	}

	if (IS_SAM_DEFAULT(sampass, PDB_USERSID)) {
		DEBUG(1, ("init_sam_from_ldap: no %s or %s attribute found for this user %s\n", 
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_USER_SID),
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_USER_RID),
			username));
		return False;
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_PWD_LAST_SET),
			ctx);
	if (temp) {
		pass_last_set_time = (time_t) atol(temp);
		pdb_set_pass_last_set_time(sampass,
				pass_last_set_time, PDB_SET);
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_LOGON_TIME),
			ctx);
	if (temp) {
		logon_time = (time_t) atol(temp);
		pdb_set_logon_time(sampass, logon_time, PDB_SET);
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_LOGOFF_TIME),
			ctx);
	if (temp) {
		logoff_time = (time_t) atol(temp);
		pdb_set_logoff_time(sampass, logoff_time, PDB_SET);
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_KICKOFF_TIME),
			ctx);
	if (temp) {
		kickoff_time = (time_t) atol(temp);
		pdb_set_kickoff_time(sampass, kickoff_time, PDB_SET);
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_PWD_CAN_CHANGE),
			ctx);
	if (temp) {
		pass_can_change_time = (time_t) atol(temp);
		pdb_set_pass_can_change_time(sampass,
				pass_can_change_time, PDB_SET);
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_PWD_MUST_CHANGE),
			ctx);
	if (temp) {
		pass_must_change_time = (time_t) atol(temp);
		pdb_set_pass_must_change_time(sampass,
				pass_must_change_time, PDB_SET);
	}

	/* recommend that 'gecos' and 'displayName' should refer to the same
	 * attribute OID.  userFullName depreciated, only used by Samba
	 * primary rules of LDAP: don't make a new attribute when one is already defined
	 * that fits your needs; using cn then displayName rather than 'userFullName'
	 */

	fullname = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_DISPLAY_NAME),
			ctx);
	if (fullname) {
		pdb_set_fullname(sampass, fullname, PDB_SET);
	} else {
		fullname = smbldap_talloc_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_userattr_key2string(ldap_state->schema_ver,
					LDAP_ATTR_CN),
				ctx);
		if (fullname) {
			pdb_set_fullname(sampass, fullname, PDB_SET);
		}
	}

	dir_drive = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_HOME_DRIVE),
			ctx);
	if (dir_drive) {
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	} else {
		pdb_set_dir_drive( sampass, lp_logon_drive(), PDB_DEFAULT );
	}

	homedir = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_HOME_PATH),
			ctx);
	if (homedir) {
		if (expand_explicit) {
			homedir = talloc_sub_basic(ctx,
						username,
						domain,
						homedir);
			if (!homedir) {
				goto fn_exit;
			}
		}
		pdb_set_homedir(sampass, homedir, PDB_SET);
	} else {
		pdb_set_homedir(sampass,
			talloc_sub_basic(ctx, username, domain,
					 lp_logon_home()),
			PDB_DEFAULT);
	}

	logon_script = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_LOGON_SCRIPT),
			ctx);
	if (logon_script) {
		if (expand_explicit) {
			logon_script = talloc_sub_basic(ctx,
						username,
						domain,
						logon_script);
			if (!logon_script) {
				goto fn_exit;
			}
		}
		pdb_set_logon_script(sampass, logon_script, PDB_SET);
	} else {
		pdb_set_logon_script(sampass,
			talloc_sub_basic(ctx, username, domain,
					 lp_logon_script()),
			PDB_DEFAULT );
	}

	profile_path = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_PROFILE_PATH),
			ctx);
	if (profile_path) {
		if (expand_explicit) {
			profile_path = talloc_sub_basic(ctx,
						username,
						domain,
						profile_path);
			if (!profile_path) {
				goto fn_exit;
			}
		}
		pdb_set_profile_path(sampass, profile_path, PDB_SET);
	} else {
		pdb_set_profile_path(sampass,
			talloc_sub_basic(ctx, username, domain,
					  lp_logon_path()),
			PDB_DEFAULT );
	}

	acct_desc = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_DESC),
			ctx);
	if (acct_desc) {
		pdb_set_acct_desc(sampass, acct_desc, PDB_SET);
	}

	workstations = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_USER_WKS),
			ctx);
	if (workstations) {
		pdb_set_workstations(sampass, workstations, PDB_SET);
	}

	munged_dial = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_MUNGED_DIAL),
			ctx);
	if (munged_dial) {
		pdb_set_munged_dial(sampass, munged_dial, PDB_SET);
	}

	/* FIXME: hours stuff should be cleaner */

	logon_divs = 168;
	hours_len = 21;
	memset(hours, 0xff, hours_len);

	if (ldap_state->is_nds_ldap) {
		char *user_dn;
		size_t pwd_len;
		char clear_text_pw[512];

		/* Make call to Novell eDirectory ldap extension to get clear text password.
			NOTE: This will only work if we have an SSL connection to eDirectory. */
		user_dn = smbldap_talloc_dn(ctx, ldap_state->smbldap_state->ldap_struct, entry);
		if (user_dn != NULL) {
			DEBUG(3, ("init_sam_from_ldap: smbldap_talloc_dn(ctx, %s) returned '%s'\n", username, user_dn));

			pwd_len = sizeof(clear_text_pw);
			if (pdb_nds_get_password(ldap_state->smbldap_state, user_dn, &pwd_len, clear_text_pw) == LDAP_SUCCESS) {
				nt_lm_owf_gen(clear_text_pw, smbntpwd, smblmpwd);
				if (!pdb_set_lanman_passwd(sampass, smblmpwd, PDB_SET)) {
					TALLOC_FREE(user_dn);
					return False;
				}
				ZERO_STRUCT(smblmpwd);
				if (!pdb_set_nt_passwd(sampass, smbntpwd, PDB_SET)) {
					TALLOC_FREE(user_dn);
					return False;
				}
				ZERO_STRUCT(smbntpwd);
				use_samba_attrs = False;
			}

			TALLOC_FREE(user_dn);

		} else {
			DEBUG(0, ("init_sam_from_ldap: failed to get user_dn for '%s'\n", username));
		}
	}

	if (use_samba_attrs) {
		temp = smbldap_talloc_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_userattr_key2string(ldap_state->schema_ver,
					LDAP_ATTR_LMPW),
				ctx);
		if (temp) {
			pdb_gethexpwd(temp, smblmpwd);
			memset((char *)temp, '\0', strlen(temp)+1);
			if (!pdb_set_lanman_passwd(sampass, smblmpwd, PDB_SET)) {
				goto fn_exit;
			}
			ZERO_STRUCT(smblmpwd);
		}

		temp = smbldap_talloc_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_userattr_key2string(ldap_state->schema_ver,
					LDAP_ATTR_NTPW),
				ctx);
		if (temp) {
			pdb_gethexpwd(temp, smbntpwd);
			memset((char *)temp, '\0', strlen(temp)+1);
			if (!pdb_set_nt_passwd(sampass, smbntpwd, PDB_SET)) {
				goto fn_exit;
			}
			ZERO_STRUCT(smbntpwd);
		}
	}

	pwHistLen = 0;

	pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY, &pwHistLen);
	if (pwHistLen > 0){
		uint8 *pwhist = NULL;
		int i;
		char *history_string = TALLOC_ARRAY(ctx, char,
						MAX_PW_HISTORY_LEN*64);

		if (!history_string) {
			goto fn_exit;
		}

		pwHistLen = MIN(pwHistLen, MAX_PW_HISTORY_LEN);

		pwhist = TALLOC_ARRAY(ctx, uint8,
				      pwHistLen * PW_HISTORY_ENTRY_LEN);
		if (pwhist == NULL) {
			DEBUG(0, ("init_sam_from_ldap: talloc failed!\n"));
			goto fn_exit;
		}
		memset(pwhist, '\0', pwHistLen * PW_HISTORY_ENTRY_LEN);

		if (smbldap_get_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_userattr_key2string(ldap_state->schema_ver,
					LDAP_ATTR_PWD_HISTORY),
				history_string,
				MAX_PW_HISTORY_LEN*64)) {
			bool hex_failed = false;
			for (i = 0; i < pwHistLen; i++){
				/* Get the 16 byte salt. */
				if (!pdb_gethexpwd(&history_string[i*64],
					&pwhist[i*PW_HISTORY_ENTRY_LEN])) {
					hex_failed = true;
					break;
				}
				/* Get the 16 byte MD5 hash of salt+passwd. */
				if (!pdb_gethexpwd(&history_string[(i*64)+32],
					&pwhist[(i*PW_HISTORY_ENTRY_LEN)+
						PW_HISTORY_SALT_LEN])) {
					hex_failed = True;
					break;
				}
			}
			if (hex_failed) {
				DEBUG(2,("init_sam_from_ldap: Failed to get password history for user %s\n",
					username));
				memset(pwhist, '\0', pwHistLen * PW_HISTORY_ENTRY_LEN);
			}
		}
		if (!pdb_set_pw_history(sampass, pwhist, pwHistLen, PDB_SET)){
			goto fn_exit;
		}
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_ACB_INFO),
			ctx);
	if (temp) {
		acct_ctrl = pdb_decode_acct_ctrl(temp);

		if (acct_ctrl == 0) {
			acct_ctrl |= ACB_NORMAL;
		}

		pdb_set_acct_ctrl(sampass, acct_ctrl, PDB_SET);
	} else {
		acct_ctrl |= ACB_NORMAL;
	}

	pdb_set_hours_len(sampass, hours_len, PDB_SET);
	pdb_set_logon_divs(sampass, logon_divs, PDB_SET);

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_BAD_PASSWORD_COUNT),
			ctx);
	if (temp) {
		bad_password_count = (uint32_t) atol(temp);
		pdb_set_bad_password_count(sampass,
				bad_password_count, PDB_SET);
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_BAD_PASSWORD_TIME),
			ctx);
	if (temp) {
		bad_password_time = (time_t) atol(temp);
		pdb_set_bad_password_time(sampass, bad_password_time, PDB_SET);
	}


	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_LOGON_COUNT),
			ctx);
	if (temp) {
		logon_count = (uint32_t) atol(temp);
		pdb_set_logon_count(sampass, logon_count, PDB_SET);
	}

	/* pdb_set_unknown_6(sampass, unknown6, PDB_SET); */

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_userattr_key2string(ldap_state->schema_ver,
				LDAP_ATTR_LOGON_HOURS),
			ctx);
	if (temp) {
		pdb_gethexhours(temp, hours);
		memset((char *)temp, '\0', strlen(temp) +1);
		pdb_set_hours(sampass, hours, hours_len, PDB_SET);
		ZERO_STRUCT(hours);
	}

	if (lp_parm_bool(-1, "ldapsam", "trusted", False)) {
		struct passwd unix_pw;
		bool have_uid = false;
		bool have_gid = false;
		struct dom_sid mapped_gsid;
		const struct dom_sid *primary_gsid;

		ZERO_STRUCT(unix_pw);

		unix_pw.pw_name = username;
		unix_pw.pw_passwd = discard_const_p(char, "x");

		temp = smbldap_talloc_single_attribute(
				priv2ld(ldap_state),
				entry,
				"uidNumber",
				ctx);
		if (temp) {
			/* We've got a uid, feed the cache */
			unix_pw.pw_uid = strtoul(temp, NULL, 10);
			have_uid = true;
		}
		temp = smbldap_talloc_single_attribute(
				priv2ld(ldap_state),
				entry,
				"gidNumber",
				ctx);
		if (temp) {
			/* We've got a uid, feed the cache */
			unix_pw.pw_gid = strtoul(temp, NULL, 10);
			have_gid = true;
		}
		unix_pw.pw_gecos = smbldap_talloc_single_attribute(
				priv2ld(ldap_state),
				entry,
				"gecos",
				ctx);
		if (unix_pw.pw_gecos) {
			unix_pw.pw_gecos = fullname;
		}
		unix_pw.pw_dir = smbldap_talloc_single_attribute(
				priv2ld(ldap_state),
				entry,
				"homeDirectory",
				ctx);
		if (unix_pw.pw_dir) {
			unix_pw.pw_dir = discard_const_p(char, "");
		}
		unix_pw.pw_shell = smbldap_talloc_single_attribute(
				priv2ld(ldap_state),
				entry,
				"loginShell",
				ctx);
		if (unix_pw.pw_shell) {
			unix_pw.pw_shell = discard_const_p(char, "");
		}

		if (have_uid && have_gid) {
			sampass->unix_pw = tcopy_passwd(sampass, &unix_pw);
		} else {
			sampass->unix_pw = Get_Pwnam_alloc(sampass, unix_pw.pw_name);
		}

		if (sampass->unix_pw == NULL) {
			DEBUG(0,("init_sam_from_ldap: Failed to find Unix account for %s\n",
				 pdb_get_username(sampass)));
			goto fn_exit;
		}

		store_uid_sid_cache(pdb_get_user_sid(sampass),
				    sampass->unix_pw->pw_uid);
		idmap_cache_set_sid2uid(pdb_get_user_sid(sampass),
					sampass->unix_pw->pw_uid);

		gid_to_sid(&mapped_gsid, sampass->unix_pw->pw_gid);
		primary_gsid = pdb_get_group_sid(sampass);
		if (primary_gsid && dom_sid_equal(primary_gsid, &mapped_gsid)) {
			store_gid_sid_cache(primary_gsid,
					    sampass->unix_pw->pw_gid);
			idmap_cache_set_sid2gid(primary_gsid,
						sampass->unix_pw->pw_gid);
		}
	}

	/* check the timestamp of the cache vs ldap entry */
	if (!(ldap_entry_time = ldapsam_get_entry_timestamp(ldap_state,
							    entry))) {
		ret = true;
		goto fn_exit;
	}

	/* see if we have newer updates */
	if (!login_cache_read(sampass, &cache_entry)) {
		DEBUG (9, ("No cache entry, bad count = %u, bad time = %u\n",
			   (unsigned int)pdb_get_bad_password_count(sampass),
			   (unsigned int)pdb_get_bad_password_time(sampass)));
		ret = true;
		goto fn_exit;
	}

	DEBUG(7, ("ldap time is %u, cache time is %u, bad time = %u\n",
		  (unsigned int)ldap_entry_time,
		  (unsigned int)cache_entry.entry_timestamp,
		  (unsigned int)cache_entry.bad_password_time));

	if (ldap_entry_time > cache_entry.entry_timestamp) {
		/* cache is older than directory , so
		   we need to delete the entry but allow the
		   fields to be written out */
		login_cache_delentry(sampass);
	} else {
		/* read cache in */
		pdb_set_acct_ctrl(sampass,
				  pdb_get_acct_ctrl(sampass) |
				  (cache_entry.acct_ctrl & ACB_AUTOLOCK),
				  PDB_SET);
		pdb_set_bad_password_count(sampass,
					   cache_entry.bad_password_count,
					   PDB_SET);
		pdb_set_bad_password_time(sampass,
					  cache_entry.bad_password_time,
					  PDB_SET);
	}

	ret = true;

  fn_exit:

	TALLOC_FREE(ctx);
	return ret;
}

/**********************************************************************
 Initialize the ldap db from a struct samu. Called on update.
 (Based on init_buffer_from_sam in pdb_tdb.c)
*********************************************************************/

static bool init_ldap_from_sam (struct ldapsam_privates *ldap_state,
				LDAPMessage *existing,
				LDAPMod *** mods, struct samu * sampass,
				bool (*need_update)(const struct samu *,
						    enum pdb_elements))
{
	char *temp = NULL;
	uint32_t rid;

	if (mods == NULL || sampass == NULL) {
		DEBUG(0, ("init_ldap_from_sam: NULL parameters found!\n"));
		return False;
	}

	*mods = NULL;

	/*
	 * took out adding "objectclass: sambaAccount"
	 * do this on a per-mod basis
	 */
	if (need_update(sampass, PDB_USERNAME)) {
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods, 
			      "uid", pdb_get_username(sampass));
		if (ldap_state->is_nds_ldap) {
			smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods, 
				      "cn", pdb_get_username(sampass));
			smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods, 
				      "sn", pdb_get_username(sampass));
		}
	}

	DEBUG(2, ("init_ldap_from_sam: Setting entry for user: %s\n", pdb_get_username(sampass)));

	/* only update the RID if we actually need to */
	if (need_update(sampass, PDB_USERSID)) {
		fstring sid_string;
		const struct dom_sid *user_sid = pdb_get_user_sid(sampass);

		switch ( ldap_state->schema_ver ) {
			case SCHEMAVER_SAMBAACCOUNT:
				if (!sid_peek_check_rid(&ldap_state->domain_sid, user_sid, &rid)) {
					DEBUG(1, ("init_ldap_from_sam: User's SID (%s) is not for this domain (%s), cannot add to LDAP!\n", 
						  sid_string_dbg(user_sid),
						  sid_string_dbg(
							  &ldap_state->domain_sid)));
					return False;
				}
				if (asprintf(&temp, "%i", rid) < 0) {
					return false;
				}
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
					get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_USER_RID), 
					temp);
				SAFE_FREE(temp);
				break;

			case SCHEMAVER_SAMBASAMACCOUNT:
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
					get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_USER_SID), 
					sid_to_fstring(sid_string, user_sid));
				break;

			default:
				DEBUG(0,("init_ldap_from_sam: unknown schema version specified\n"));
				break;
		}
	}

	/* we don't need to store the primary group RID - so leaving it
	   'free' to hang off the unix primary group makes life easier */

	if (need_update(sampass, PDB_GROUPSID)) {
		fstring sid_string;
		const struct dom_sid *group_sid = pdb_get_group_sid(sampass);

		switch ( ldap_state->schema_ver ) {
			case SCHEMAVER_SAMBAACCOUNT:
				if (!sid_peek_check_rid(&ldap_state->domain_sid, group_sid, &rid)) {
					DEBUG(1, ("init_ldap_from_sam: User's Primary Group SID (%s) is not for this domain (%s), cannot add to LDAP!\n",
						  sid_string_dbg(group_sid),
						  sid_string_dbg(
							  &ldap_state->domain_sid)));
					return False;
				}

				if (asprintf(&temp, "%i", rid) < 0) {
					return false;
				}
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
					get_userattr_key2string(ldap_state->schema_ver, 
					LDAP_ATTR_PRIMARY_GROUP_RID), temp);
				SAFE_FREE(temp);
				break;

			case SCHEMAVER_SAMBASAMACCOUNT:
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
					get_userattr_key2string(ldap_state->schema_ver, 
					LDAP_ATTR_PRIMARY_GROUP_SID), sid_to_fstring(sid_string, group_sid));
				break;

			default:
				DEBUG(0,("init_ldap_from_sam: unknown schema version specified\n"));
				break;
		}

	}

	/* displayName, cn, and gecos should all be the same
	 *  most easily accomplished by giving them the same OID
	 *  gecos isn't set here b/c it should be handled by the
	 *  add-user script
	 *  We change displayName only and fall back to cn if
	 *  it does not exist.
	 */

	if (need_update(sampass, PDB_FULLNAME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_DISPLAY_NAME), 
			pdb_get_fullname(sampass));

	if (need_update(sampass, PDB_ACCTDESC))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_DESC), 
			pdb_get_acct_desc(sampass));

	if (need_update(sampass, PDB_WORKSTATIONS))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_USER_WKS), 
			pdb_get_workstations(sampass));

	if (need_update(sampass, PDB_MUNGEDDIAL))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_MUNGED_DIAL), 
			pdb_get_munged_dial(sampass));

	if (need_update(sampass, PDB_SMBHOME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_HOME_PATH), 
			pdb_get_homedir(sampass));

	if (need_update(sampass, PDB_DRIVE))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_HOME_DRIVE), 
			pdb_get_dir_drive(sampass));

	if (need_update(sampass, PDB_LOGONSCRIPT))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_LOGON_SCRIPT), 
			pdb_get_logon_script(sampass));

	if (need_update(sampass, PDB_PROFILE))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_PROFILE_PATH), 
			pdb_get_profile_path(sampass));

	if (asprintf(&temp, "%li", (long int)pdb_get_logon_time(sampass)) < 0) {
		return false;
	}
	if (need_update(sampass, PDB_LOGONTIME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_LOGON_TIME), temp);
	SAFE_FREE(temp);

	if (asprintf(&temp, "%li", (long int)pdb_get_logoff_time(sampass)) < 0) {
		return false;
	}
	if (need_update(sampass, PDB_LOGOFFTIME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_LOGOFF_TIME), temp);
	SAFE_FREE(temp);

	if (asprintf(&temp, "%li", (long int)pdb_get_kickoff_time(sampass)) < 0) {
		return false;
	}
	if (need_update(sampass, PDB_KICKOFFTIME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_KICKOFF_TIME), temp);
	SAFE_FREE(temp);

	if (asprintf(&temp, "%li", (long int)pdb_get_pass_can_change_time_noncalc(sampass)) < 0) {
		return false;
	}
	if (need_update(sampass, PDB_CANCHANGETIME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_PWD_CAN_CHANGE), temp);
	SAFE_FREE(temp);

	if (asprintf(&temp, "%li", (long int)pdb_get_pass_must_change_time(sampass)) < 0) {
		return false;
	}
	if (need_update(sampass, PDB_MUSTCHANGETIME))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_PWD_MUST_CHANGE), temp);
	SAFE_FREE(temp);

	if ((pdb_get_acct_ctrl(sampass)&(ACB_WSTRUST|ACB_SVRTRUST|ACB_DOMTRUST))
			|| (lp_ldap_passwd_sync()!=LDAP_PASSWD_SYNC_ONLY)) {

		if (need_update(sampass, PDB_LMPASSWD)) {
			const uchar *lm_pw = pdb_get_lanman_passwd(sampass);
			if (lm_pw) {
				char pwstr[34];
				pdb_sethexpwd(pwstr, lm_pw,
					      pdb_get_acct_ctrl(sampass));
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
						 get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_LMPW), 
						 pwstr);
			} else {
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
						 get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_LMPW), 
						 NULL);
			}
		}
		if (need_update(sampass, PDB_NTPASSWD)) {
			const uchar *nt_pw = pdb_get_nt_passwd(sampass);
			if (nt_pw) {
				char pwstr[34];
				pdb_sethexpwd(pwstr, nt_pw,
					      pdb_get_acct_ctrl(sampass));
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
						 get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_NTPW), 
						 pwstr);
			} else {
				smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
						 get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_NTPW), 
						 NULL);
			}
		}

		if (need_update(sampass, PDB_PWHISTORY)) {
			char *pwstr = NULL;
			uint32_t pwHistLen = 0;
			pdb_get_account_policy(PDB_POLICY_PASSWORD_HISTORY, &pwHistLen);

			pwstr = SMB_MALLOC_ARRAY(char, 1024);
			if (!pwstr) {
				return false;
			}
			if (pwHistLen == 0) {
				/* Remove any password history from the LDAP store. */
				memset(pwstr, '0', 64); /* NOTE !!!! '0' *NOT '\0' */
				pwstr[64] = '\0';
			} else {
				int i;
				uint32_t currHistLen = 0;
				const uint8 *pwhist = pdb_get_pw_history(sampass, &currHistLen);
				if (pwhist != NULL) {
					/* We can only store (1024-1/64 password history entries. */
					pwHistLen = MIN(pwHistLen, ((1024-1)/64));
					for (i=0; i< pwHistLen && i < currHistLen; i++) {
						/* Store the salt. */
						pdb_sethexpwd(&pwstr[i*64], &pwhist[i*PW_HISTORY_ENTRY_LEN], 0);
						/* Followed by the md5 hash of salt + md4 hash */
						pdb_sethexpwd(&pwstr[(i*64)+32],
							&pwhist[(i*PW_HISTORY_ENTRY_LEN)+PW_HISTORY_SALT_LEN], 0);
						DEBUG(100, ("pwstr=%s\n", pwstr));
					}
				}
			}
			smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
					 get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_PWD_HISTORY), 
					 pwstr);
			SAFE_FREE(pwstr);
		}

		if (need_update(sampass, PDB_PASSLASTSET)) {
			if (asprintf(&temp, "%li",
				(long int)pdb_get_pass_last_set_time(sampass)) < 0) {
				return false;
			}
			smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
				get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_PWD_LAST_SET), 
				temp);
			SAFE_FREE(temp);
		}
	}

	if (need_update(sampass, PDB_HOURS)) {
		const uint8 *hours = pdb_get_hours(sampass);
		if (hours) {
			char hourstr[44];
			pdb_sethexhours(hourstr, hours);
			smbldap_make_mod(ldap_state->smbldap_state->ldap_struct,
				existing,
				mods,
				get_userattr_key2string(ldap_state->schema_ver,
						LDAP_ATTR_LOGON_HOURS),
				hourstr);
		}
	}

	if (need_update(sampass, PDB_ACCTCTRL))
		smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, existing, mods,
			get_userattr_key2string(ldap_state->schema_ver, LDAP_ATTR_ACB_INFO), 
			pdb_encode_acct_ctrl (pdb_get_acct_ctrl(sampass), NEW_PW_FORMAT_SPACE_PADDED_LEN));

	/* password lockout cache:
	   - If we are now autolocking or clearing, we write to ldap
	   - If we are clearing, we delete the cache entry
	   - If the count is > 0, we update the cache

	   This even means when autolocking, we cache, just in case the
	   update doesn't work, and we have to cache the autolock flag */

	if (need_update(sampass, PDB_BAD_PASSWORD_COUNT))  /* &&
	    need_update(sampass, PDB_BAD_PASSWORD_TIME)) */ {
		uint16_t badcount = pdb_get_bad_password_count(sampass);
		time_t badtime = pdb_get_bad_password_time(sampass);
		uint32_t pol;
		pdb_get_account_policy(PDB_POLICY_BAD_ATTEMPT_LOCKOUT, &pol);

		DEBUG(3, ("updating bad password fields, policy=%u, count=%u, time=%u\n",
			(unsigned int)pol, (unsigned int)badcount, (unsigned int)badtime));

		if ((badcount >= pol) || (badcount == 0)) {
			DEBUG(7, ("making mods to update ldap, count=%u, time=%u\n",
				(unsigned int)badcount, (unsigned int)badtime));
			if (asprintf(&temp, "%li", (long)badcount) < 0) {
				return false;
			}
			smbldap_make_mod(
				ldap_state->smbldap_state->ldap_struct,
				existing, mods,
				get_userattr_key2string(
					ldap_state->schema_ver,
					LDAP_ATTR_BAD_PASSWORD_COUNT),
				temp);
			SAFE_FREE(temp);

			if (asprintf(&temp, "%li", (long int)badtime) < 0) {
				return false;
			}
			smbldap_make_mod(
				ldap_state->smbldap_state->ldap_struct,
				existing, mods,
				get_userattr_key2string(
					ldap_state->schema_ver,
					LDAP_ATTR_BAD_PASSWORD_TIME),
				temp);
			SAFE_FREE(temp);
		}
		if (badcount == 0) {
			DEBUG(7, ("bad password count is reset, deleting login cache entry for %s\n", pdb_get_nt_username(sampass)));
			login_cache_delentry(sampass);
		} else {
			struct login_cache cache_entry;

			cache_entry.entry_timestamp = time(NULL);
			cache_entry.acct_ctrl = pdb_get_acct_ctrl(sampass);
			cache_entry.bad_password_count = badcount;
			cache_entry.bad_password_time = badtime;

			DEBUG(7, ("Updating bad password count and time in login cache\n"));
			login_cache_write(sampass, &cache_entry);
		}
	}

	return True;
}

/**********************************************************************
 End enumeration of the LDAP password list.
*********************************************************************/

static void ldapsam_endsampwent(struct pdb_methods *my_methods)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	if (ldap_state->result) {
		ldap_msgfree(ldap_state->result);
		ldap_state->result = NULL;
	}
}

static void append_attr(TALLOC_CTX *mem_ctx, const char ***attr_list,
			const char *new_attr)
{
	int i;

	if (new_attr == NULL) {
		return;
	}

	for (i=0; (*attr_list)[i] != NULL; i++) {
		;
	}

	(*attr_list) = TALLOC_REALLOC_ARRAY(mem_ctx, (*attr_list),
					    const char *,  i+2);
	SMB_ASSERT((*attr_list) != NULL);
	(*attr_list)[i] = talloc_strdup((*attr_list), new_attr);
	(*attr_list)[i+1] = NULL;
}

static void ldapsam_add_unix_attributes(TALLOC_CTX *mem_ctx,
					const char ***attr_list)
{
	append_attr(mem_ctx, attr_list, "uidNumber");
	append_attr(mem_ctx, attr_list, "gidNumber");
	append_attr(mem_ctx, attr_list, "homeDirectory");
	append_attr(mem_ctx, attr_list, "loginShell");
	append_attr(mem_ctx, attr_list, "gecos");
}

/**********************************************************************
Get struct samu entry from LDAP by username.
*********************************************************************/

static NTSTATUS ldapsam_getsampwnam(struct pdb_methods *my_methods, struct samu *user, const char *sname)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int count;
	const char ** attr_list;
	int rc;

	attr_list = get_userattr_list( user, ldap_state->schema_ver );
	append_attr(user, &attr_list,
		    get_userattr_key2string(ldap_state->schema_ver,
					    LDAP_ATTR_MOD_TIMESTAMP));
	ldapsam_add_unix_attributes(user, &attr_list);
	rc = ldapsam_search_suffix_by_name(ldap_state, sname, &result,
					   attr_list);
	TALLOC_FREE( attr_list );

	if ( rc != LDAP_SUCCESS ) 
		return NT_STATUS_NO_SUCH_USER;

	count = ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result);

	if (count < 1) {
		DEBUG(4, ("ldapsam_getsampwnam: Unable to locate user [%s] count=%d\n", sname, count));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_USER;
	} else if (count > 1) {
		DEBUG(1, ("ldapsam_getsampwnam: Duplicate entries for this user [%s] Failing. count=%d\n", sname, count));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct, result);
	if (entry) {
		if (!init_sam_from_ldap(ldap_state, user, entry)) {
			DEBUG(1,("ldapsam_getsampwnam: init_sam_from_ldap failed for user '%s'!\n", sname));
			ldap_msgfree(result);
			return NT_STATUS_NO_SUCH_USER;
		}
		pdb_set_backend_private_data(user, result, NULL,
					     my_methods, PDB_CHANGED);
		talloc_autofree_ldapmsg(user, result);
		ret = NT_STATUS_OK;
	} else {
		ldap_msgfree(result);
	}
	return ret;
}

static int ldapsam_get_ldap_user_by_sid(struct ldapsam_privates *ldap_state, 
				   const struct dom_sid *sid, LDAPMessage **result)
{
	int rc = -1;
	const char ** attr_list;
	uint32_t rid;

	switch ( ldap_state->schema_ver ) {
		case SCHEMAVER_SAMBASAMACCOUNT: {
			TALLOC_CTX *tmp_ctx = talloc_new(NULL);
			if (tmp_ctx == NULL) {
				return LDAP_NO_MEMORY;
			}

			attr_list = get_userattr_list(tmp_ctx,
						      ldap_state->schema_ver);
			append_attr(tmp_ctx, &attr_list,
				    get_userattr_key2string(
					    ldap_state->schema_ver,
					    LDAP_ATTR_MOD_TIMESTAMP));
			ldapsam_add_unix_attributes(tmp_ctx, &attr_list);
			rc = ldapsam_search_suffix_by_sid(ldap_state, sid,
							  result, attr_list);
			TALLOC_FREE(tmp_ctx);

			if ( rc != LDAP_SUCCESS ) 
				return rc;
			break;
		}

		case SCHEMAVER_SAMBAACCOUNT:
			if (!sid_peek_check_rid(&ldap_state->domain_sid, sid, &rid)) {
				return rc;
			}

			attr_list = get_userattr_list(NULL,
						      ldap_state->schema_ver);
			rc = ldapsam_search_suffix_by_rid(ldap_state, rid, result, attr_list );
			TALLOC_FREE( attr_list );

			if ( rc != LDAP_SUCCESS ) 
				return rc;
			break;
	}
	return rc;
}

/**********************************************************************
 Get struct samu entry from LDAP by SID.
*********************************************************************/

static NTSTATUS ldapsam_getsampwsid(struct pdb_methods *my_methods, struct samu * user, const struct dom_sid *sid)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int count;
	int rc;

	rc = ldapsam_get_ldap_user_by_sid(ldap_state, 
					  sid, &result); 
	if (rc != LDAP_SUCCESS)
		return NT_STATUS_NO_SUCH_USER;

	count = ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result);

	if (count < 1) {
		DEBUG(4, ("ldapsam_getsampwsid: Unable to locate SID [%s] "
			  "count=%d\n", sid_string_dbg(sid), count));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_USER;
	}  else if (count > 1) {
		DEBUG(1, ("ldapsam_getsampwsid: More than one user with SID "
			  "[%s]. Failing. count=%d\n", sid_string_dbg(sid),
			  count));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct, result);
	if (!entry) {
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	if (!init_sam_from_ldap(ldap_state, user, entry)) {
		DEBUG(1,("ldapsam_getsampwsid: init_sam_from_ldap failed!\n"));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	pdb_set_backend_private_data(user, result, NULL,
				     my_methods, PDB_CHANGED);
	talloc_autofree_ldapmsg(user, result);
	return NT_STATUS_OK;
}	

/********************************************************************
 Do the actual modification - also change a plaintext passord if 
 it it set.
**********************************************************************/

static NTSTATUS ldapsam_modify_entry(struct pdb_methods *my_methods, 
				     struct samu *newpwd, char *dn,
				     LDAPMod **mods, int ldap_op, 
				     bool (*need_update)(const struct samu *, enum pdb_elements))
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	int rc;

	if (!newpwd || !dn) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!(pdb_get_acct_ctrl(newpwd)&(ACB_WSTRUST|ACB_SVRTRUST|ACB_DOMTRUST)) &&
			(lp_ldap_passwd_sync() != LDAP_PASSWD_SYNC_OFF) &&
			need_update(newpwd, PDB_PLAINTEXT_PW) &&
			(pdb_get_plaintext_passwd(newpwd)!=NULL)) {
		BerElement *ber;
		struct berval *bv;
		char *retoid = NULL;
		struct berval *retdata = NULL;
		char *utf8_password;
		char *utf8_dn;
		size_t converted_size;
		int ret;

		if (!ldap_state->is_nds_ldap) {

			if (!smbldap_has_extension(ldap_state->smbldap_state->ldap_struct, 
						   LDAP_EXOP_MODIFY_PASSWD)) {
				DEBUG(2, ("ldap password change requested, but LDAP "
					  "server does not support it -- ignoring\n"));
				return NT_STATUS_OK;
			}
		}

		if (!push_utf8_talloc(talloc_tos(), &utf8_password,
					pdb_get_plaintext_passwd(newpwd),
					&converted_size))
		{
			return NT_STATUS_NO_MEMORY;
		}

		if (!push_utf8_talloc(talloc_tos(), &utf8_dn, dn, &converted_size)) {
			TALLOC_FREE(utf8_password);
			return NT_STATUS_NO_MEMORY;
		}

		if ((ber = ber_alloc_t(LBER_USE_DER))==NULL) {
			DEBUG(0,("ber_alloc_t returns NULL\n"));
			TALLOC_FREE(utf8_password);
			TALLOC_FREE(utf8_dn);
			return NT_STATUS_UNSUCCESSFUL;
		}

		if ((ber_printf (ber, "{") < 0) ||
		    (ber_printf (ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_ID,
				 utf8_dn) < 0)) {
			DEBUG(0,("ldapsam_modify_entry: ber_printf returns a "
				 "value <0\n"));
			ber_free(ber,1);
			TALLOC_FREE(utf8_dn);
			TALLOC_FREE(utf8_password);
			return NT_STATUS_UNSUCCESSFUL;
		}

		if ((utf8_password != NULL) && (*utf8_password != '\0')) {
			ret = ber_printf(ber, "ts}",
					 LDAP_TAG_EXOP_MODIFY_PASSWD_NEW,
					 utf8_password);
		} else {
			ret = ber_printf(ber, "}");
		}

		if (ret < 0) {
			DEBUG(0,("ldapsam_modify_entry: ber_printf returns a "
				 "value <0\n"));
			ber_free(ber,1);
			TALLOC_FREE(utf8_dn);
			TALLOC_FREE(utf8_password);
			return NT_STATUS_UNSUCCESSFUL;
		}

	        if ((rc = ber_flatten (ber, &bv))<0) {
			DEBUG(0,("ldapsam_modify_entry: ber_flatten returns a value <0\n"));
			ber_free(ber,1);
			TALLOC_FREE(utf8_dn);
			TALLOC_FREE(utf8_password);
			return NT_STATUS_UNSUCCESSFUL;
		}

		TALLOC_FREE(utf8_dn);
		TALLOC_FREE(utf8_password);
		ber_free(ber, 1);

		if (!ldap_state->is_nds_ldap) {
			rc = smbldap_extended_operation(ldap_state->smbldap_state, 
							LDAP_EXOP_MODIFY_PASSWD,
							bv, NULL, NULL, &retoid, 
							&retdata);
		} else {
			rc = pdb_nds_set_password(ldap_state->smbldap_state, dn,
							pdb_get_plaintext_passwd(newpwd));
		}
		if (rc != LDAP_SUCCESS) {
			char *ld_error = NULL;

			if (rc == LDAP_OBJECT_CLASS_VIOLATION) {
				DEBUG(3, ("Could not set userPassword "
					  "attribute due to an objectClass "
					  "violation -- ignoring\n"));
				ber_bvfree(bv);
				return NT_STATUS_OK;
			}

			ldap_get_option(ldap_state->smbldap_state->ldap_struct, LDAP_OPT_ERROR_STRING,
					&ld_error);
			DEBUG(0,("ldapsam_modify_entry: LDAP Password could not be changed for user %s: %s\n\t%s\n",
				pdb_get_username(newpwd), ldap_err2string(rc), ld_error?ld_error:"unknown"));
			SAFE_FREE(ld_error);
			ber_bvfree(bv);
#if defined(LDAP_CONSTRAINT_VIOLATION)
			if (rc == LDAP_CONSTRAINT_VIOLATION)
				return NT_STATUS_PASSWORD_RESTRICTION;
#endif
			return NT_STATUS_UNSUCCESSFUL;
		} else {
			DEBUG(3,("ldapsam_modify_entry: LDAP Password changed for user %s\n",pdb_get_username(newpwd)));
#ifdef DEBUG_PASSWORD
			DEBUG(100,("ldapsam_modify_entry: LDAP Password changed to %s\n",pdb_get_plaintext_passwd(newpwd)));
#endif    
			if (retdata)
				ber_bvfree(retdata);
			if (retoid)
				ldap_memfree(retoid);
		}
		ber_bvfree(bv);
	}

	if (!mods) {
		DEBUG(5,("ldapsam_modify_entry: mods is empty: nothing to modify\n"));
		/* may be password change below however */
	} else {
		switch(ldap_op) {
			case LDAP_MOD_ADD:
				if (ldap_state->is_nds_ldap) {
					smbldap_set_mod(&mods, LDAP_MOD_ADD,
							"objectclass",
							"inetOrgPerson");
				} else {
					smbldap_set_mod(&mods, LDAP_MOD_ADD,
							"objectclass",
							LDAP_OBJ_ACCOUNT);
				}
				rc = smbldap_add(ldap_state->smbldap_state,
						 dn, mods);
				break;
			case LDAP_MOD_REPLACE:
				rc = smbldap_modify(ldap_state->smbldap_state,
						    dn ,mods);
				break;
			default:
				DEBUG(0,("ldapsam_modify_entry: Wrong LDAP operation type: %d!\n",
					 ldap_op));
				return NT_STATUS_INVALID_PARAMETER;
		}

		if (rc!=LDAP_SUCCESS) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	return NT_STATUS_OK;
}

/**********************************************************************
 Delete entry from LDAP for username.
*********************************************************************/

static NTSTATUS ldapsam_delete_sam_account(struct pdb_methods *my_methods,
					   struct samu * sam_acct)
{
	struct ldapsam_privates *priv =
		(struct ldapsam_privates *)my_methods->private_data;
	const char *sname;
	int rc;
	LDAPMessage *msg, *entry;
	NTSTATUS result = NT_STATUS_NO_MEMORY;
	const char **attr_list;
	TALLOC_CTX *mem_ctx;

	if (!sam_acct) {
		DEBUG(0, ("ldapsam_delete_sam_account: sam_acct was NULL!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	sname = pdb_get_username(sam_acct);

	DEBUG(3, ("ldapsam_delete_sam_account: Deleting user %s from "
		  "LDAP.\n", sname));

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		goto done;
	}

	attr_list = get_userattr_delete_list(mem_ctx, priv->schema_ver );
	if (attr_list == NULL) {
		goto done;
	}

	rc = ldapsam_search_suffix_by_name(priv, sname, &msg, attr_list);

	if ((rc != LDAP_SUCCESS) ||
	    (ldap_count_entries(priv2ld(priv), msg) != 1) ||
	    ((entry = ldap_first_entry(priv2ld(priv), msg)) == NULL)) {
		DEBUG(5, ("Could not find user %s\n", sname));
		result = NT_STATUS_NO_SUCH_USER;
		goto done;
	}

	rc = ldapsam_delete_entry(
		priv, mem_ctx, entry,
		priv->schema_ver == SCHEMAVER_SAMBASAMACCOUNT ?
		LDAP_OBJ_SAMBASAMACCOUNT : LDAP_OBJ_SAMBAACCOUNT,
		attr_list);

	result = (rc == LDAP_SUCCESS) ?
		NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;

 done:
	TALLOC_FREE(mem_ctx);
	return result;
}

/**********************************************************************
 Helper function to determine for update_sam_account whether
 we need LDAP modification.
*********************************************************************/

static bool element_is_changed(const struct samu *sampass,
			       enum pdb_elements element)
{
	return IS_SAM_CHANGED(sampass, element);
}

/**********************************************************************
 Update struct samu.
*********************************************************************/

static NTSTATUS ldapsam_update_sam_account(struct pdb_methods *my_methods, struct samu * newpwd)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	int rc = 0;
	char *dn;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	const char **attr_list;

	result = (LDAPMessage *)pdb_get_backend_private_data(newpwd, my_methods);
	if (!result) {
		attr_list = get_userattr_list(NULL, ldap_state->schema_ver);
		if (pdb_get_username(newpwd) == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		rc = ldapsam_search_suffix_by_name(ldap_state, pdb_get_username(newpwd), &result, attr_list );
		TALLOC_FREE( attr_list );
		if (rc != LDAP_SUCCESS) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		pdb_set_backend_private_data(newpwd, result, NULL,
					     my_methods, PDB_CHANGED);
		talloc_autofree_ldapmsg(newpwd, result);
	}

	if (ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result) == 0) {
		DEBUG(0, ("ldapsam_update_sam_account: No user to modify!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct, result);
	dn = smbldap_talloc_dn(talloc_tos(), ldap_state->smbldap_state->ldap_struct, entry);
	if (!dn) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(4, ("ldapsam_update_sam_account: user %s to be modified has dn: %s\n", pdb_get_username(newpwd), dn));

	if (!init_ldap_from_sam(ldap_state, entry, &mods, newpwd,
				element_is_changed)) {
		DEBUG(0, ("ldapsam_update_sam_account: init_ldap_from_sam failed!\n"));
		TALLOC_FREE(dn);
		if (mods != NULL)
			ldap_mods_free(mods,True);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if ((lp_ldap_passwd_sync() != LDAP_PASSWD_SYNC_ONLY)
	    && (mods == NULL)) {
		DEBUG(4,("ldapsam_update_sam_account: mods is empty: nothing to update for user: %s\n",
			 pdb_get_username(newpwd)));
		TALLOC_FREE(dn);
		return NT_STATUS_OK;
	}

	ret = ldapsam_modify_entry(my_methods,newpwd,dn,mods,LDAP_MOD_REPLACE, element_is_changed);

	if (mods != NULL) {
		ldap_mods_free(mods,True);
	}

	TALLOC_FREE(dn);

	/*
	 * We need to set the backend private data to NULL here. For example
	 * setuserinfo level 25 does a pdb_update_sam_account twice on the
	 * same one, and with the explicit delete / add logic for attribute
	 * values the second time we would use the wrong "old" value which
	 * does not exist in LDAP anymore. Thus the LDAP server would refuse
	 * the update.
	 * The existing LDAPMessage is still being auto-freed by the
	 * destructor.
	 */
	pdb_set_backend_private_data(newpwd, NULL, NULL, my_methods,
				     PDB_CHANGED);

	if (!NT_STATUS_IS_OK(ret)) {
		return ret;
	}

	DEBUG(2, ("ldapsam_update_sam_account: successfully modified uid = %s in the LDAP database\n",
		  pdb_get_username(newpwd)));
	return NT_STATUS_OK;
}

/***************************************************************************
 Renames a struct samu
 - The "rename user script" has full responsibility for changing everything
***************************************************************************/

static NTSTATUS ldapsam_del_groupmem(struct pdb_methods *my_methods,
				     TALLOC_CTX *tmp_ctx,
				     uint32_t group_rid,
				     uint32_t member_rid);

static NTSTATUS ldapsam_enum_group_memberships(struct pdb_methods *methods,
					       TALLOC_CTX *mem_ctx,
					       struct samu *user,
					       struct dom_sid **pp_sids,
					       gid_t **pp_gids,
					       uint32_t *p_num_groups);

static NTSTATUS ldapsam_rename_sam_account(struct pdb_methods *my_methods,
					   struct samu *old_acct,
					   const char *newname)
{
	const char *oldname;
	int rc;
	char *rename_script = NULL;
	fstring oldname_lower, newname_lower;

	if (!old_acct) {
		DEBUG(0, ("ldapsam_rename_sam_account: old_acct was NULL!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (!newname) {
		DEBUG(0, ("ldapsam_rename_sam_account: newname was NULL!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	oldname = pdb_get_username(old_acct);

	/* rename the posix user */
	rename_script = SMB_STRDUP(lp_renameuser_script());
	if (rename_script == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!(*rename_script)) {
		SAFE_FREE(rename_script);
		return NT_STATUS_ACCESS_DENIED;
	}

	DEBUG (3, ("ldapsam_rename_sam_account: Renaming user %s to %s.\n",
		   oldname, newname));

	/* We have to allow the account name to end with a '$'.
	   Also, follow the semantics in _samr_create_user() and lower case the
	   posix name but preserve the case in passdb */

	fstrcpy( oldname_lower, oldname );
	strlower_m( oldname_lower );
	fstrcpy( newname_lower, newname );
	strlower_m( newname_lower );
	rename_script = realloc_string_sub2(rename_script,
					"%unew",
					newname_lower,
					true,
					true);
	if (!rename_script) {
		return NT_STATUS_NO_MEMORY;
	}
	rename_script = realloc_string_sub2(rename_script,
					"%uold",
					oldname_lower,
					true,
					true);
	rc = smbrun(rename_script, NULL);

	DEBUG(rc ? 0 : 3,("Running the command `%s' gave %d\n",
			  rename_script, rc));

	SAFE_FREE(rename_script);

	if (rc == 0) {
		smb_nscd_flush_user_cache();
	}

	if (rc)
		return NT_STATUS_UNSUCCESSFUL;

	return NT_STATUS_OK;
}

/**********************************************************************
 Helper function to determine for update_sam_account whether
 we need LDAP modification.
 *********************************************************************/

static bool element_is_set_or_changed(const struct samu *sampass,
				      enum pdb_elements element)
{
	return (IS_SAM_SET(sampass, element) ||
		IS_SAM_CHANGED(sampass, element));
}

/**********************************************************************
 Add struct samu to LDAP.
*********************************************************************/

static NTSTATUS ldapsam_add_sam_account(struct pdb_methods *my_methods, struct samu * newpwd)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	int rc;
	LDAPMessage 	*result = NULL;
	LDAPMessage 	*entry  = NULL;
	LDAPMod 	**mods = NULL;
	int		ldap_op = LDAP_MOD_REPLACE;
	uint32_t		num_result;
	const char	**attr_list;
	char *escape_user = NULL;
	const char 	*username = pdb_get_username(newpwd);
	const struct dom_sid 	*sid = pdb_get_user_sid(newpwd);
	char *filter = NULL;
	char *dn = NULL;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *ctx = talloc_init("ldapsam_add_sam_account");

	if (!ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!username || !*username) {
		DEBUG(0, ("ldapsam_add_sam_account: Cannot add user without a username!\n"));
		status = NT_STATUS_INVALID_PARAMETER;
		goto fn_exit;
	}

	/* free this list after the second search or in case we exit on failure */
	attr_list = get_userattr_list(ctx, ldap_state->schema_ver);

	rc = ldapsam_search_suffix_by_name (ldap_state, username, &result, attr_list);

	if (rc != LDAP_SUCCESS) {
		goto fn_exit;
	}

	if (ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result) != 0) {
		DEBUG(0,("ldapsam_add_sam_account: User '%s' already in the base, with samba attributes\n", 
			 username));
		goto fn_exit;
	}
	ldap_msgfree(result);
	result = NULL;

	if (element_is_set_or_changed(newpwd, PDB_USERSID)) {
		rc = ldapsam_get_ldap_user_by_sid(ldap_state,
						  sid, &result);
		if (rc == LDAP_SUCCESS) {
			if (ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result) != 0) {
				DEBUG(0,("ldapsam_add_sam_account: SID '%s' "
					 "already in the base, with samba "
					 "attributes\n", sid_string_dbg(sid)));
				goto fn_exit;
			}
			ldap_msgfree(result);
			result = NULL;
		}
	}

	/* does the entry already exist but without a samba attributes?
	   we need to return the samba attributes here */

	escape_user = escape_ldap_string(talloc_tos(), username);
	filter = talloc_strdup(attr_list, "(uid=%u)");
	if (!filter) {
		status = NT_STATUS_NO_MEMORY;
		goto fn_exit;
	}
	filter = talloc_all_string_sub(attr_list, filter, "%u", escape_user);
	TALLOC_FREE(escape_user);
	if (!filter) {
		status = NT_STATUS_NO_MEMORY;
		goto fn_exit;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state,
				   filter, attr_list, &result);
	if ( rc != LDAP_SUCCESS ) {
		goto fn_exit;
	}

	num_result = ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result);

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_add_sam_account: More than one user with that uid exists: bailing out!\n"));
		goto fn_exit;
	}

	/* Check if we need to update an existing entry */
	if (num_result == 1) {
		DEBUG(3,("ldapsam_add_sam_account: User exists without samba attributes: adding them\n"));
		ldap_op = LDAP_MOD_REPLACE;
		entry = ldap_first_entry (ldap_state->smbldap_state->ldap_struct, result);
		dn = smbldap_talloc_dn(ctx, ldap_state->smbldap_state->ldap_struct, entry);
		if (!dn) {
			status = NT_STATUS_NO_MEMORY;
			goto fn_exit;
		}

	} else if (ldap_state->schema_ver == SCHEMAVER_SAMBASAMACCOUNT) {

		/* There might be a SID for this account already - say an idmap entry */

		filter = talloc_asprintf(ctx,
				"(&(%s=%s)(|(objectClass=%s)(objectClass=%s)))",
				 get_userattr_key2string(ldap_state->schema_ver,
					 LDAP_ATTR_USER_SID),
				 sid_string_talloc(ctx, sid),
				 LDAP_OBJ_IDMAP_ENTRY,
				 LDAP_OBJ_SID_ENTRY);
		if (!filter) {
			status = NT_STATUS_NO_MEMORY;
			goto fn_exit;
		}

		/* free old result before doing a new search */
		if (result != NULL) {
			ldap_msgfree(result);
			result = NULL;
		}
		rc = smbldap_search_suffix(ldap_state->smbldap_state,
					   filter, attr_list, &result);

		if ( rc != LDAP_SUCCESS ) {
			goto fn_exit;
		}

		num_result = ldap_count_entries(ldap_state->smbldap_state->ldap_struct, result);

		if (num_result > 1) {
			DEBUG (0, ("ldapsam_add_sam_account: More than one user with specified Sid exists: bailing out!\n"));
			goto fn_exit;
		}

		/* Check if we need to update an existing entry */
		if (num_result == 1) {

			DEBUG(3,("ldapsam_add_sam_account: User exists without samba attributes: adding them\n"));
			ldap_op = LDAP_MOD_REPLACE;
			entry = ldap_first_entry (ldap_state->smbldap_state->ldap_struct, result);
			dn = smbldap_talloc_dn (ctx, ldap_state->smbldap_state->ldap_struct, entry);
			if (!dn) {
				status = NT_STATUS_NO_MEMORY;
				goto fn_exit;
			}
		}
	}

	if (num_result == 0) {
		char *escape_username;
		/* Check if we need to add an entry */
		DEBUG(3,("ldapsam_add_sam_account: Adding new user\n"));
		ldap_op = LDAP_MOD_ADD;

		escape_username = escape_rdn_val_string_alloc(username);
		if (!escape_username) {
			status = NT_STATUS_NO_MEMORY;
			goto fn_exit;
		}

		if (username[strlen(username)-1] == '$') {
			dn = talloc_asprintf(ctx,
					"uid=%s,%s",
					escape_username,
					lp_ldap_machine_suffix());
		} else {
			dn = talloc_asprintf(ctx,
					"uid=%s,%s",
					escape_username,
					lp_ldap_user_suffix());
		}

		SAFE_FREE(escape_username);
		if (!dn) {
			status = NT_STATUS_NO_MEMORY;
			goto fn_exit;
		}
	}

	if (!init_ldap_from_sam(ldap_state, entry, &mods, newpwd,
				element_is_set_or_changed)) {
		DEBUG(0, ("ldapsam_add_sam_account: init_ldap_from_sam failed!\n"));
		if (mods != NULL) {
			ldap_mods_free(mods, true);
		}
		goto fn_exit;
	}

	if (mods == NULL) {
		DEBUG(0,("ldapsam_add_sam_account: mods is empty: nothing to add for user: %s\n",pdb_get_username(newpwd)));
		goto fn_exit;
	}
	switch ( ldap_state->schema_ver ) {
		case SCHEMAVER_SAMBAACCOUNT:
			smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectclass", LDAP_OBJ_SAMBAACCOUNT);
			break;
		case SCHEMAVER_SAMBASAMACCOUNT:
			smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectclass", LDAP_OBJ_SAMBASAMACCOUNT);
			break;
		default:
			DEBUG(0,("ldapsam_add_sam_account: invalid schema version specified\n"));
			break;
	}

	ret = ldapsam_modify_entry(my_methods,newpwd,dn,mods,ldap_op, element_is_set_or_changed);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("ldapsam_add_sam_account: failed to modify/add user with uid = %s (dn = %s)\n",
			 pdb_get_username(newpwd),dn));
		ldap_mods_free(mods, true);
		goto fn_exit;
	}

	DEBUG(2,("ldapsam_add_sam_account: added: uid == %s in the LDAP database\n", pdb_get_username(newpwd)));
	ldap_mods_free(mods, true);

	status = NT_STATUS_OK;

  fn_exit:

	TALLOC_FREE(ctx);
	if (result) {
		ldap_msgfree(result);
	}
	return status;
}

/**********************************************************************
 *********************************************************************/

static int ldapsam_search_one_group (struct ldapsam_privates *ldap_state,
				     const char *filter,
				     LDAPMessage ** result)
{
	int scope = LDAP_SCOPE_SUBTREE;
	int rc;
	const char **attr_list;

	attr_list = get_attr_list(NULL, groupmap_attr_list);
	rc = smbldap_search(ldap_state->smbldap_state,
			    lp_ldap_suffix (), scope,
			    filter, attr_list, 0, result);
	TALLOC_FREE(attr_list);

	return rc;
}

/**********************************************************************
 *********************************************************************/

static bool init_group_from_ldap(struct ldapsam_privates *ldap_state,
				 GROUP_MAP *map, LDAPMessage *entry)
{
	char *temp = NULL;
	TALLOC_CTX *ctx = talloc_init("init_group_from_ldap");

	if (ldap_state == NULL || map == NULL || entry == NULL ||
			ldap_state->smbldap_state->ldap_struct == NULL) {
		DEBUG(0, ("init_group_from_ldap: NULL parameters found!\n"));
		TALLOC_FREE(ctx);
		return false;
	}

	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_attr_key2string(groupmap_attr_list,
				LDAP_ATTR_GIDNUMBER),
			ctx);
	if (!temp) {
		DEBUG(0, ("init_group_from_ldap: Mandatory attribute %s not found\n", 
			get_attr_key2string( groupmap_attr_list, LDAP_ATTR_GIDNUMBER)));
		TALLOC_FREE(ctx);
		return false;
	}
	DEBUG(2, ("init_group_from_ldap: Entry found for group: %s\n", temp));

	map->gid = (gid_t)atol(temp);

	TALLOC_FREE(temp);
	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_attr_key2string(groupmap_attr_list,
				LDAP_ATTR_GROUP_SID),
			ctx);
	if (!temp) {
		DEBUG(0, ("init_group_from_ldap: Mandatory attribute %s not found\n",
			get_attr_key2string( groupmap_attr_list, LDAP_ATTR_GROUP_SID)));
		TALLOC_FREE(ctx);
		return false;
	}

	if (!string_to_sid(&map->sid, temp)) {
		DEBUG(1, ("SID string [%s] could not be read as a valid SID\n", temp));
		TALLOC_FREE(ctx);
		return false;
	}

	TALLOC_FREE(temp);
	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_attr_key2string(groupmap_attr_list,
				LDAP_ATTR_GROUP_TYPE),
			ctx);
	if (!temp) {
		DEBUG(0, ("init_group_from_ldap: Mandatory attribute %s not found\n",
			get_attr_key2string( groupmap_attr_list, LDAP_ATTR_GROUP_TYPE)));
		TALLOC_FREE(ctx);
		return false;
	}
	map->sid_name_use = (enum lsa_SidType)atol(temp);

	if ((map->sid_name_use < SID_NAME_USER) ||
			(map->sid_name_use > SID_NAME_UNKNOWN)) {
		DEBUG(0, ("init_group_from_ldap: Unknown Group type: %d\n", map->sid_name_use));
		TALLOC_FREE(ctx);
		return false;
	}

	TALLOC_FREE(temp);
	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_attr_key2string(groupmap_attr_list,
				LDAP_ATTR_DISPLAY_NAME),
			ctx);
	if (!temp) {
		temp = smbldap_talloc_single_attribute(
				ldap_state->smbldap_state->ldap_struct,
				entry,
				get_attr_key2string(groupmap_attr_list,
					LDAP_ATTR_CN),
				ctx);
		if (!temp) {
			DEBUG(0, ("init_group_from_ldap: Attributes cn not found either \
for gidNumber(%lu)\n",(unsigned long)map->gid));
			TALLOC_FREE(ctx);
			return false;
		}
	}
	fstrcpy(map->nt_name, temp);

	TALLOC_FREE(temp);
	temp = smbldap_talloc_single_attribute(
			ldap_state->smbldap_state->ldap_struct,
			entry,
			get_attr_key2string(groupmap_attr_list,
				LDAP_ATTR_DESC),
			ctx);
	if (!temp) {
		temp = talloc_strdup(ctx, "");
		if (!temp) {
			TALLOC_FREE(ctx);
			return false;
		}
	}
	fstrcpy(map->comment, temp);

	if (lp_parm_bool(-1, "ldapsam", "trusted", false)) {
		store_gid_sid_cache(&map->sid, map->gid);
		idmap_cache_set_sid2gid(&map->sid, map->gid);
	}

	TALLOC_FREE(ctx);
	return true;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_getgroup(struct pdb_methods *methods,
				 const char *filter,
				 GROUP_MAP *map)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int count;

	if (ldapsam_search_one_group(ldap_state, filter, &result)
	    != LDAP_SUCCESS) {
		return NT_STATUS_NO_SUCH_GROUP;
	}

	count = ldap_count_entries(priv2ld(ldap_state), result);

	if (count < 1) {
		DEBUG(4, ("ldapsam_getgroup: Did not find group, filter was "
			  "%s\n", filter));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_GROUP;
	}

	if (count > 1) {
		DEBUG(1, ("ldapsam_getgroup: Duplicate entries for filter %s: "
			  "count=%d\n", filter, count));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_GROUP;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);

	if (!entry) {
		ldap_msgfree(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!init_group_from_ldap(ldap_state, map, entry)) {
		DEBUG(1, ("ldapsam_getgroup: init_group_from_ldap failed for "
			  "group filter %s\n", filter));
		ldap_msgfree(result);
		return NT_STATUS_NO_SUCH_GROUP;
	}

	ldap_msgfree(result);
	return NT_STATUS_OK;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_getgrsid(struct pdb_methods *methods, GROUP_MAP *map,
				 struct dom_sid sid)
{
	char *filter = NULL;
	NTSTATUS status;
	fstring tmp;

	if (asprintf(&filter, "(&(objectClass=%s)(%s=%s))",
		LDAP_OBJ_GROUPMAP,
		get_attr_key2string(groupmap_attr_list, LDAP_ATTR_GROUP_SID),
		sid_to_fstring(tmp, &sid)) < 0) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ldapsam_getgroup(methods, filter, map);
	SAFE_FREE(filter);
	return status;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_getgrgid(struct pdb_methods *methods, GROUP_MAP *map,
				 gid_t gid)
{
	char *filter = NULL;
	NTSTATUS status;

	if (asprintf(&filter, "(&(objectClass=%s)(%s=%lu))",
		LDAP_OBJ_GROUPMAP,
		get_attr_key2string(groupmap_attr_list, LDAP_ATTR_GIDNUMBER),
		(unsigned long)gid) < 0) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ldapsam_getgroup(methods, filter, map);
	SAFE_FREE(filter);
	return status;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_getgrnam(struct pdb_methods *methods, GROUP_MAP *map,
				 const char *name)
{
	char *filter = NULL;
	char *escape_name = escape_ldap_string(talloc_tos(), name);
	NTSTATUS status;

	if (!escape_name) {
		return NT_STATUS_NO_MEMORY;
	}

	if (asprintf(&filter, "(&(objectClass=%s)(|(%s=%s)(%s=%s)))",
		LDAP_OBJ_GROUPMAP,
		get_attr_key2string(groupmap_attr_list, LDAP_ATTR_DISPLAY_NAME), escape_name,
		get_attr_key2string(groupmap_attr_list, LDAP_ATTR_CN),
		escape_name) < 0) {
		TALLOC_FREE(escape_name);
		return NT_STATUS_NO_MEMORY;
	}

	TALLOC_FREE(escape_name);
	status = ldapsam_getgroup(methods, filter, map);
	SAFE_FREE(filter);
	return status;
}

static bool ldapsam_extract_rid_from_entry(LDAP *ldap_struct,
					   LDAPMessage *entry,
					   const struct dom_sid *domain_sid,
					   uint32_t *rid)
{
	fstring str;
	struct dom_sid sid;

	if (!smbldap_get_single_attribute(ldap_struct, entry, "sambaSID",
					  str, sizeof(str)-1)) {
		DEBUG(10, ("Could not find sambaSID attribute\n"));
		return False;
	}

	if (!string_to_sid(&sid, str)) {
		DEBUG(10, ("Could not convert string %s to sid\n", str));
		return False;
	}

	if (dom_sid_compare_domain(&sid, domain_sid) != 0) {
		DEBUG(10, ("SID %s is not in expected domain %s\n",
			   str, sid_string_dbg(domain_sid)));
		return False;
	}

	if (!sid_peek_rid(&sid, rid)) {
		DEBUG(10, ("Could not peek into RID\n"));
		return False;
	}

	return True;
}

static NTSTATUS ldapsam_enum_group_members(struct pdb_methods *methods,
					   TALLOC_CTX *mem_ctx,
					   const struct dom_sid *group,
					   uint32_t **pp_member_rids,
					   size_t *p_num_members)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	struct smbldap_state *conn = ldap_state->smbldap_state;
	const char *id_attrs[] = { "memberUid", "gidNumber", NULL };
	const char *sid_attrs[] = { "sambaSID", NULL };
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	LDAPMessage *result = NULL;
	LDAPMessage *entry;
	char *filter;
	char **values = NULL;
	char **memberuid;
	char *gidstr;
	int rc, count;

	*pp_member_rids = NULL;
	*p_num_members = 0;

	filter = talloc_asprintf(mem_ctx,
				 "(&(objectClass=%s)"
				 "(objectClass=%s)"
				 "(sambaSID=%s))",
				 LDAP_OBJ_POSIXGROUP,
				 LDAP_OBJ_GROUPMAP,
				 sid_string_talloc(mem_ctx, group));
	if (filter == NULL) {
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	rc = smbldap_search(conn, lp_ldap_suffix(),
			    LDAP_SCOPE_SUBTREE, filter, id_attrs, 0,
			    &result);

	if (rc != LDAP_SUCCESS)
		goto done;

	talloc_autofree_ldapmsg(mem_ctx, result);

	count = ldap_count_entries(conn->ldap_struct, result);

	if (count > 1) {
		DEBUG(1, ("Found more than one groupmap entry for %s\n",
			  sid_string_dbg(group)));
		ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	if (count == 0) {
		ret = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	entry = ldap_first_entry(conn->ldap_struct, result);
	if (entry == NULL)
		goto done;

	gidstr = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "gidNumber", mem_ctx);
	if (!gidstr) {
		DEBUG (0, ("ldapsam_enum_group_members: Unable to find the group's gid!\n"));
		ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	values = ldap_get_values(conn->ldap_struct, entry, "memberUid");

	if ((values != NULL) && (values[0] != NULL)) {

		filter = talloc_asprintf(mem_ctx, "(&(objectClass=%s)(|", LDAP_OBJ_SAMBASAMACCOUNT);
		if (filter == NULL) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		for (memberuid = values; *memberuid != NULL; memberuid += 1) {
			char *escape_memberuid;

			escape_memberuid = escape_ldap_string(talloc_tos(),
							      *memberuid);
			if (escape_memberuid == NULL) {
				ret = NT_STATUS_NO_MEMORY;
				goto done;
			}

			filter = talloc_asprintf_append_buffer(filter, "(uid=%s)", escape_memberuid);
			TALLOC_FREE(escape_memberuid);
			if (filter == NULL) {
				ret = NT_STATUS_NO_MEMORY;
				goto done;
			}
		}

		filter = talloc_asprintf_append_buffer(filter, "))");
		if (filter == NULL) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		rc = smbldap_search(conn, lp_ldap_suffix(),
				    LDAP_SCOPE_SUBTREE, filter, sid_attrs, 0,
				    &result);

		if (rc != LDAP_SUCCESS)
			goto done;

		count = ldap_count_entries(conn->ldap_struct, result);
		DEBUG(10,("ldapsam_enum_group_members: found %d accounts\n", count));

		talloc_autofree_ldapmsg(mem_ctx, result);

		for (entry = ldap_first_entry(conn->ldap_struct, result);
		     entry != NULL;
		     entry = ldap_next_entry(conn->ldap_struct, entry))
		{
			char *sidstr;
			struct dom_sid sid;
			uint32_t rid;

			sidstr = smbldap_talloc_single_attribute(conn->ldap_struct,
								 entry, "sambaSID",
								 mem_ctx);
			if (!sidstr) {
				DEBUG(0, ("Severe DB error, %s can't miss the sambaSID"
					  "attribute\n", LDAP_OBJ_SAMBASAMACCOUNT));
				ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
				goto done;
			}

			if (!string_to_sid(&sid, sidstr))
				goto done;

			if (!sid_check_is_in_our_domain(&sid)) {
				DEBUG(0, ("Inconsistent SAM -- group member uid not "
					  "in our domain\n"));
				ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
				goto done;
			}

			sid_peek_rid(&sid, &rid);

			if (!add_rid_to_array_unique(mem_ctx, rid, pp_member_rids,
						p_num_members)) {
				ret = NT_STATUS_NO_MEMORY;
				goto done;
			}
		}
	}

	filter = talloc_asprintf(mem_ctx,
				 "(&(objectClass=%s)"
				 "(gidNumber=%s))",
				 LDAP_OBJ_SAMBASAMACCOUNT,
				 gidstr);

	rc = smbldap_search(conn, lp_ldap_suffix(),
			    LDAP_SCOPE_SUBTREE, filter, sid_attrs, 0,
			    &result);

	if (rc != LDAP_SUCCESS)
		goto done;

	talloc_autofree_ldapmsg(mem_ctx, result);

	for (entry = ldap_first_entry(conn->ldap_struct, result);
	     entry != NULL;
	     entry = ldap_next_entry(conn->ldap_struct, entry))
	{
		uint32_t rid;

		if (!ldapsam_extract_rid_from_entry(conn->ldap_struct,
						    entry,
						    get_global_sam_sid(),
						    &rid)) {
			DEBUG(0, ("Severe DB error, %s can't miss the samba SID"								"attribute\n", LDAP_OBJ_SAMBASAMACCOUNT));
			ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto done;
		}

		if (!add_rid_to_array_unique(mem_ctx, rid, pp_member_rids,
					p_num_members)) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	ret = NT_STATUS_OK;

 done:

	if (values)
		ldap_value_free(values);

	return ret;
}

static NTSTATUS ldapsam_enum_group_memberships(struct pdb_methods *methods,
					       TALLOC_CTX *mem_ctx,
					       struct samu *user,
					       struct dom_sid **pp_sids,
					       gid_t **pp_gids,
					       uint32_t *p_num_groups)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	struct smbldap_state *conn = ldap_state->smbldap_state;
	char *filter;
	const char *attrs[] = { "gidNumber", "sambaSID", NULL };
	char *escape_name;
	int rc, count;
	LDAPMessage *result = NULL;
	LDAPMessage *entry;
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	uint32_t num_sids;
	uint32_t num_gids;
	char *gidstr;
	gid_t primary_gid = -1;

	*pp_sids = NULL;
	num_sids = 0;

	if (pdb_get_username(user) == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	escape_name = escape_ldap_string(talloc_tos(), pdb_get_username(user));
	if (escape_name == NULL)
		return NT_STATUS_NO_MEMORY;

	if (user->unix_pw) {
		primary_gid = user->unix_pw->pw_gid;
	} else {
		/* retrieve the users primary gid */
		filter = talloc_asprintf(mem_ctx,
					 "(&(objectClass=%s)(uid=%s))",
					 LDAP_OBJ_SAMBASAMACCOUNT,
					 escape_name);
		if (filter == NULL) {
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		rc = smbldap_search(conn, lp_ldap_suffix(),
				    LDAP_SCOPE_SUBTREE, filter, attrs, 0, &result);

		if (rc != LDAP_SUCCESS)
			goto done;

		talloc_autofree_ldapmsg(mem_ctx, result);

		count = ldap_count_entries(priv2ld(ldap_state), result);

		switch (count) {
		case 0:
			DEBUG(1, ("User account [%s] not found!\n", pdb_get_username(user)));
			ret = NT_STATUS_NO_SUCH_USER;
			goto done;
		case 1:
			entry = ldap_first_entry(priv2ld(ldap_state), result);

			gidstr = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "gidNumber", mem_ctx);
			if (!gidstr) {
				DEBUG (1, ("Unable to find the member's gid!\n"));
				ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
				goto done;
			}
			primary_gid = strtoul(gidstr, NULL, 10);
			break;
		default:
			DEBUG(1, ("found more than one account with the same user name ?!\n"));
			ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto done;
		}
	}

	filter = talloc_asprintf(mem_ctx,
				 "(&(objectClass=%s)(|(memberUid=%s)(gidNumber=%u)))",
				 LDAP_OBJ_POSIXGROUP, escape_name, (unsigned int)primary_gid);
	if (filter == NULL) {
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	rc = smbldap_search(conn, lp_ldap_suffix(),
			    LDAP_SCOPE_SUBTREE, filter, attrs, 0, &result);

	if (rc != LDAP_SUCCESS)
		goto done;

	talloc_autofree_ldapmsg(mem_ctx, result);

	num_gids = 0;
	*pp_gids = NULL;

	num_sids = 0;
	*pp_sids = NULL;

	/* We need to add the primary group as the first gid/sid */

	if (!add_gid_to_array_unique(mem_ctx, primary_gid, pp_gids, &num_gids)) {
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* This sid will be replaced later */

	ret = add_sid_to_array_unique(mem_ctx, &global_sid_NULL, pp_sids,
				      &num_sids);
	if (!NT_STATUS_IS_OK(ret)) {
		goto done;
	}

	for (entry = ldap_first_entry(conn->ldap_struct, result);
	     entry != NULL;
	     entry = ldap_next_entry(conn->ldap_struct, entry))
	{
		fstring str;
		struct dom_sid sid;
		gid_t gid;
		char *end;

		if (!smbldap_get_single_attribute(conn->ldap_struct,
						  entry, "sambaSID",
						  str, sizeof(str)-1))
			continue;

		if (!string_to_sid(&sid, str))
			goto done;

		if (!smbldap_get_single_attribute(conn->ldap_struct,
						  entry, "gidNumber",
						  str, sizeof(str)-1))
			continue;

		gid = strtoul(str, &end, 10);

		if (PTR_DIFF(end, str) != strlen(str))
			goto done;

		if (gid == primary_gid) {
			sid_copy(&(*pp_sids)[0], &sid);
		} else {
			if (!add_gid_to_array_unique(mem_ctx, gid, pp_gids,
						&num_gids)) {
				ret = NT_STATUS_NO_MEMORY;
				goto done;
			}
			ret = add_sid_to_array_unique(mem_ctx, &sid, pp_sids,
						      &num_sids);
			if (!NT_STATUS_IS_OK(ret)) {
				goto done;
			}
		}
	}

	if (dom_sid_compare(&global_sid_NULL, &(*pp_sids)[0]) == 0) {
		DEBUG(3, ("primary group of [%s] not found\n",
			  pdb_get_username(user)));
		ret = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	*p_num_groups = num_sids;

	ret = NT_STATUS_OK;

 done:

	TALLOC_FREE(escape_name);
	return ret;
}

/**********************************************************************
 * Augment a posixGroup object with a sambaGroupMapping domgroup
 *********************************************************************/

static NTSTATUS ldapsam_map_posixgroup(TALLOC_CTX *mem_ctx,
				       struct ldapsam_privates *ldap_state,
				       GROUP_MAP *map)
{
	const char *filter, *dn;
	LDAPMessage *msg, *entry;
	LDAPMod **mods;
	int rc;

	filter = talloc_asprintf(mem_ctx,
				 "(&(objectClass=%s)(gidNumber=%u))",
				 LDAP_OBJ_POSIXGROUP, (unsigned int)map->gid);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter,
				   get_attr_list(mem_ctx, groupmap_attr_list),
				   &msg);
	talloc_autofree_ldapmsg(mem_ctx, msg);

	if ((rc != LDAP_SUCCESS) ||
	    (ldap_count_entries(ldap_state->smbldap_state->ldap_struct, msg) != 1) ||
	    ((entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct, msg)) == NULL)) {
		return NT_STATUS_NO_SUCH_GROUP;
	}

	dn = smbldap_talloc_dn(mem_ctx, ldap_state->smbldap_state->ldap_struct, entry);
	if (dn == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	mods = NULL;
	smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass",
			LDAP_OBJ_GROUPMAP);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, entry, &mods, "sambaSid",
			 sid_string_talloc(mem_ctx, &map->sid));
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, entry, &mods, "sambaGroupType",
			 talloc_asprintf(mem_ctx, "%d", map->sid_name_use));
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, entry, &mods, "displayName",
			 map->nt_name);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, entry, &mods, "description",
			 map->comment);
	talloc_autofree_ldapmod(mem_ctx, mods);

	rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);
	if (rc != LDAP_SUCCESS) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_add_group_mapping_entry(struct pdb_methods *methods,
						GROUP_MAP *map)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *msg = NULL;
	LDAPMod **mods = NULL;
	const char *attrs[] = { NULL };
	char *filter;

	char *dn;
	TALLOC_CTX *mem_ctx;
	NTSTATUS result;

	struct dom_sid sid;

	int rc;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	filter = talloc_asprintf(mem_ctx, "(sambaSid=%s)",
				 sid_string_talloc(mem_ctx, &map->sid));
	if (filter == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	rc = smbldap_search(ldap_state->smbldap_state, lp_ldap_suffix(),
			    LDAP_SCOPE_SUBTREE, filter, attrs, True, &msg);
	talloc_autofree_ldapmsg(mem_ctx, msg);

	if ((rc == LDAP_SUCCESS) &&
	    (ldap_count_entries(ldap_state->smbldap_state->ldap_struct, msg) > 0)) {

		DEBUG(3, ("SID %s already present in LDAP, refusing to add "
			  "group mapping entry\n", sid_string_dbg(&map->sid)));
		result = NT_STATUS_GROUP_EXISTS;
		goto done;
	}

	switch (map->sid_name_use) {

	case SID_NAME_DOM_GRP:
		/* To map a domain group we need to have a posix group
		   to attach to. */
		result = ldapsam_map_posixgroup(mem_ctx, ldap_state, map);
		goto done;
		break;

	case SID_NAME_ALIAS:
		if (!sid_check_is_in_our_domain(&map->sid) 
			&& !sid_check_is_in_builtin(&map->sid) ) 
		{
			DEBUG(3, ("Refusing to map sid %s as an alias, not in our domain\n",
				  sid_string_dbg(&map->sid)));
			result = NT_STATUS_INVALID_PARAMETER;
			goto done;
		}
		break;

	default:
		DEBUG(3, ("Got invalid use '%s' for mapping\n",
			  sid_type_lookup(map->sid_name_use)));
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	/* Domain groups have been mapped in a separate routine, we have to
	 * create an alias now */

	if (map->gid == -1) {
		DEBUG(10, ("Refusing to map gid==-1\n"));
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (pdb_gid_to_sid(map->gid, &sid)) {
		DEBUG(3, ("Gid %u is already mapped to SID %s, refusing to "
			  "add\n", (unsigned int)map->gid, sid_string_dbg(&sid)));
		result = NT_STATUS_GROUP_EXISTS;
		goto done;
	}

	/* Ok, enough checks done. It's still racy to go ahead now, but that's
	 * the best we can get out of LDAP. */

	dn = talloc_asprintf(mem_ctx, "sambaSid=%s,%s",
			     sid_string_talloc(mem_ctx, &map->sid),
			     lp_ldap_group_suffix());
	if (dn == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	mods = NULL;

	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "objectClass",
			 LDAP_OBJ_SID_ENTRY);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "objectClass",
			 LDAP_OBJ_GROUPMAP);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "sambaSid",
			 sid_string_talloc(mem_ctx, &map->sid));
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "sambaGroupType",
			 talloc_asprintf(mem_ctx, "%d", map->sid_name_use));
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "displayName",
			 map->nt_name);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "description",
			 map->comment);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, NULL, &mods, "gidNumber",
			 talloc_asprintf(mem_ctx, "%u", (unsigned int)map->gid));
	talloc_autofree_ldapmod(mem_ctx, mods);

	rc = smbldap_add(ldap_state->smbldap_state, dn, mods);

	result = (rc == LDAP_SUCCESS) ?
		NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;

 done:
	TALLOC_FREE(mem_ctx);
	return result;
}

/**********************************************************************
 * Update a group mapping entry. We're quite strict about what can be changed:
 * Only the description and displayname may be changed. It simply does not
 * make any sense to change the SID, gid or the type in a mapping.
 *********************************************************************/

static NTSTATUS ldapsam_update_group_mapping_entry(struct pdb_methods *methods,
						   GROUP_MAP *map)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	int rc;
	const char *filter, *dn;
	LDAPMessage *msg = NULL;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	TALLOC_CTX *mem_ctx;
	NTSTATUS result;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Make 100% sure that sid, gid and type are not changed by looking up
	 * exactly the values we're given in LDAP. */

	filter = talloc_asprintf(mem_ctx, "(&(objectClass=%s)"
				 "(sambaSid=%s)(gidNumber=%u)"
				 "(sambaGroupType=%d))",
				 LDAP_OBJ_GROUPMAP,
				 sid_string_talloc(mem_ctx, &map->sid),
				 (unsigned int)map->gid, map->sid_name_use);
	if (filter == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter,
				   get_attr_list(mem_ctx, groupmap_attr_list),
				   &msg);
	talloc_autofree_ldapmsg(mem_ctx, msg);

	if ((rc != LDAP_SUCCESS) ||
	    (ldap_count_entries(ldap_state->smbldap_state->ldap_struct, msg) != 1) ||
	    ((entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct, msg)) == NULL)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
	}

	dn = smbldap_talloc_dn(mem_ctx, ldap_state->smbldap_state->ldap_struct, entry);

	if (dn == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}

	mods = NULL;
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, entry, &mods, "displayName",
			 map->nt_name);
	smbldap_make_mod(ldap_state->smbldap_state->ldap_struct, entry, &mods, "description",
			 map->comment);
	talloc_autofree_ldapmod(mem_ctx, mods);

	if (mods == NULL) {
		DEBUG(4, ("ldapsam_update_group_mapping_entry: mods is empty: "
			  "nothing to do\n"));
		result = NT_STATUS_OK;
		goto done;
	}

	rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);

	if (rc != LDAP_SUCCESS) {
		result = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	DEBUG(2, ("ldapsam_update_group_mapping_entry: successfully modified "
		  "group %lu in LDAP\n", (unsigned long)map->gid));

	result = NT_STATUS_OK;

 done:
	TALLOC_FREE(mem_ctx);
	return result;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_delete_group_mapping_entry(struct pdb_methods *methods,
						   struct dom_sid sid)
{
	struct ldapsam_privates *priv =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *msg, *entry;
	int rc;
	NTSTATUS result;
	TALLOC_CTX *mem_ctx;
	char *filter;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	filter = talloc_asprintf(mem_ctx, "(&(objectClass=%s)(%s=%s))",
				 LDAP_OBJ_GROUPMAP, LDAP_ATTRIBUTE_SID,
				 sid_string_talloc(mem_ctx, &sid));
	if (filter == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto done;
	}
	rc = smbldap_search_suffix(priv->smbldap_state, filter,
				   get_attr_list(mem_ctx, groupmap_attr_list),
				   &msg);
	talloc_autofree_ldapmsg(mem_ctx, msg);

	if ((rc != LDAP_SUCCESS) ||
	    (ldap_count_entries(priv2ld(priv), msg) != 1) ||
	    ((entry = ldap_first_entry(priv2ld(priv), msg)) == NULL)) {
		result = NT_STATUS_NO_SUCH_GROUP;
		goto done;
 	}

	rc = ldapsam_delete_entry(priv, mem_ctx, entry, LDAP_OBJ_GROUPMAP,
				  get_attr_list(mem_ctx,
						groupmap_attr_list_to_delete));

	if ((rc == LDAP_NAMING_VIOLATION) ||
	    (rc == LDAP_NOT_ALLOWED_ON_RDN) ||
	    (rc == LDAP_OBJECT_CLASS_VIOLATION)) {
		const char *attrs[] = { "sambaGroupType", "description",
					"displayName", "sambaSIDList",
					NULL };

		/* Second try. Don't delete the sambaSID attribute, this is
		   for "old" entries that are tacked on a winbind
		   sambaIdmapEntry. */

		rc = ldapsam_delete_entry(priv, mem_ctx, entry,
					  LDAP_OBJ_GROUPMAP, attrs);
	}

	if ((rc == LDAP_NAMING_VIOLATION) ||
	    (rc == LDAP_NOT_ALLOWED_ON_RDN) ||
	    (rc == LDAP_OBJECT_CLASS_VIOLATION)) {
		const char *attrs[] = { "sambaGroupType", "description",
					"displayName", "sambaSIDList",
					"gidNumber", NULL };

		/* Third try. This is a post-3.0.21 alias (containing only
		 * sambaSidEntry and sambaGroupMapping classes), we also have
		 * to delete the gidNumber attribute, only the sambaSidEntry
		 * remains */

		rc = ldapsam_delete_entry(priv, mem_ctx, entry,
					  LDAP_OBJ_GROUPMAP, attrs);
	}

	result = (rc == LDAP_SUCCESS) ? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;

 done:
	TALLOC_FREE(mem_ctx);
	return result;
 }

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_setsamgrent(struct pdb_methods *my_methods,
				    bool update)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)my_methods->private_data;
	char *filter = NULL;
	int rc;
	const char **attr_list;

	filter = talloc_asprintf(NULL, "(objectclass=%s)", LDAP_OBJ_GROUPMAP);
	if (!filter) {
		return NT_STATUS_NO_MEMORY;
	}
	attr_list = get_attr_list( NULL, groupmap_attr_list );
	rc = smbldap_search(ldap_state->smbldap_state, lp_ldap_suffix(),
			    LDAP_SCOPE_SUBTREE, filter,
			    attr_list, 0, &ldap_state->result);
	TALLOC_FREE(attr_list);

	if (rc != LDAP_SUCCESS) {
		DEBUG(0, ("ldapsam_setsamgrent: LDAP search failed: %s\n",
			  ldap_err2string(rc)));
		DEBUG(3, ("ldapsam_setsamgrent: Query was: %s, %s\n",
			  lp_ldap_suffix(), filter));
		ldap_msgfree(ldap_state->result);
		ldap_state->result = NULL;
		TALLOC_FREE(filter);
		return NT_STATUS_UNSUCCESSFUL;
	}

	TALLOC_FREE(filter);

	DEBUG(2, ("ldapsam_setsamgrent: %d entries in the base!\n",
		  ldap_count_entries(ldap_state->smbldap_state->ldap_struct,
				     ldap_state->result)));

	ldap_state->entry =
		ldap_first_entry(ldap_state->smbldap_state->ldap_struct,
				 ldap_state->result);
	ldap_state->index = 0;

	return NT_STATUS_OK;
}

/**********************************************************************
 *********************************************************************/

static void ldapsam_endsamgrent(struct pdb_methods *my_methods)
{
	ldapsam_endsampwent(my_methods);
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_getsamgrent(struct pdb_methods *my_methods,
				    GROUP_MAP *map)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)my_methods->private_data;
	bool bret = False;

	while (!bret) {
		if (!ldap_state->entry)
			return ret;

		ldap_state->index++;
		bret = init_group_from_ldap(ldap_state, map,
					    ldap_state->entry);

		ldap_state->entry =
			ldap_next_entry(ldap_state->smbldap_state->ldap_struct,
					ldap_state->entry);	
	}

	return NT_STATUS_OK;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS ldapsam_enum_group_mapping(struct pdb_methods *methods,
					   const struct dom_sid *domsid, enum lsa_SidType sid_name_use,
					   GROUP_MAP **pp_rmap,
					   size_t *p_num_entries,
					   bool unix_only)
{
	GROUP_MAP map = { 0, };
	size_t entries = 0;

	*p_num_entries = 0;
	*pp_rmap = NULL;

	if (!NT_STATUS_IS_OK(ldapsam_setsamgrent(methods, False))) {
		DEBUG(0, ("ldapsam_enum_group_mapping: Unable to open "
			  "passdb\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	while (NT_STATUS_IS_OK(ldapsam_getsamgrent(methods, &map))) {
		if (sid_name_use != SID_NAME_UNKNOWN &&
		    sid_name_use != map.sid_name_use) {
			DEBUG(11,("ldapsam_enum_group_mapping: group %s is "
				  "not of the requested type\n", map.nt_name));
			continue;
		}
		if (unix_only==ENUM_ONLY_MAPPED && map.gid==-1) {
			DEBUG(11,("ldapsam_enum_group_mapping: group %s is "
				  "non mapped\n", map.nt_name));
			continue;
		}

		(*pp_rmap)=SMB_REALLOC_ARRAY((*pp_rmap), GROUP_MAP, entries+1);
		if (!(*pp_rmap)) {
			DEBUG(0,("ldapsam_enum_group_mapping: Unable to "
				 "enlarge group map!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}

		(*pp_rmap)[entries] = map;

		entries += 1;

	}
	ldapsam_endsamgrent(methods);

	*p_num_entries = entries;

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_modify_aliasmem(struct pdb_methods *methods,
					const struct dom_sid *alias,
					const struct dom_sid *member,
					int modop)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	char *dn = NULL;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int count;
	LDAPMod **mods = NULL;
	int rc;
	enum lsa_SidType type = SID_NAME_USE_NONE;
	fstring tmp;

	char *filter = NULL;

	if (sid_check_is_in_builtin(alias)) {
		type = SID_NAME_ALIAS;
	}

	if (sid_check_is_in_our_domain(alias)) {
		type = SID_NAME_ALIAS;
	}

	if (type == SID_NAME_USE_NONE) {
		DEBUG(5, ("SID %s is neither in builtin nor in our domain!\n",
			  sid_string_dbg(alias)));
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	if (asprintf(&filter,
		     "(&(objectClass=%s)(sambaSid=%s)(sambaGroupType=%d))",
		     LDAP_OBJ_GROUPMAP, sid_to_fstring(tmp, alias),
		     type) < 0) {
		return NT_STATUS_NO_MEMORY;
	}

	if (ldapsam_search_one_group(ldap_state, filter,
				     &result) != LDAP_SUCCESS) {
		SAFE_FREE(filter);
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	count = ldap_count_entries(ldap_state->smbldap_state->ldap_struct,
				   result);

	if (count < 1) {
		DEBUG(4, ("ldapsam_modify_aliasmem: Did not find alias\n"));
		ldap_msgfree(result);
		SAFE_FREE(filter);
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	if (count > 1) {
		DEBUG(1, ("ldapsam_modify_aliasmem: Duplicate entries for "
			  "filter %s: count=%d\n", filter, count));
		ldap_msgfree(result);
		SAFE_FREE(filter);
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	SAFE_FREE(filter);

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct,
				 result);

	if (!entry) {
		ldap_msgfree(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	dn = smbldap_talloc_dn(talloc_tos(), ldap_state->smbldap_state->ldap_struct, entry);
	if (!dn) {
		ldap_msgfree(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	smbldap_set_mod(&mods, modop,
			get_attr_key2string(groupmap_attr_list,
					    LDAP_ATTR_SID_LIST),
			sid_to_fstring(tmp, member));

	rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);

	ldap_mods_free(mods, True);
	ldap_msgfree(result);
	TALLOC_FREE(dn);

	if (rc == LDAP_TYPE_OR_VALUE_EXISTS) {
		return NT_STATUS_MEMBER_IN_ALIAS;
	}

	if (rc == LDAP_NO_SUCH_ATTRIBUTE) {
		return NT_STATUS_MEMBER_NOT_IN_ALIAS;
	}

	if (rc != LDAP_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_add_aliasmem(struct pdb_methods *methods,
				     const struct dom_sid *alias,
				     const struct dom_sid *member)
{
	return ldapsam_modify_aliasmem(methods, alias, member, LDAP_MOD_ADD);
}

static NTSTATUS ldapsam_del_aliasmem(struct pdb_methods *methods,
				     const struct dom_sid *alias,
				     const struct dom_sid *member)
{
	return ldapsam_modify_aliasmem(methods, alias, member,
				       LDAP_MOD_DELETE);
}

static NTSTATUS ldapsam_enum_aliasmem(struct pdb_methods *methods,
				      const struct dom_sid *alias,
				      TALLOC_CTX *mem_ctx,
				      struct dom_sid **pp_members,
				      size_t *p_num_members)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int count;
	char **values = NULL;
	int i;
	char *filter = NULL;
	uint32_t num_members = 0;
	enum lsa_SidType type = SID_NAME_USE_NONE;
	fstring tmp;

	*pp_members = NULL;
	*p_num_members = 0;

	if (sid_check_is_in_builtin(alias)) {
		type = SID_NAME_ALIAS;
	}

	if (sid_check_is_in_our_domain(alias)) {
		type = SID_NAME_ALIAS;
	}

	if (type == SID_NAME_USE_NONE) {
		DEBUG(5, ("SID %s is neither in builtin nor in our domain!\n",
			  sid_string_dbg(alias)));
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	if (asprintf(&filter,
		     "(&(objectClass=%s)(sambaSid=%s)(sambaGroupType=%d))",
		     LDAP_OBJ_GROUPMAP, sid_to_fstring(tmp, alias),
		     type) < 0) {
		return NT_STATUS_NO_MEMORY;
	}

	if (ldapsam_search_one_group(ldap_state, filter,
				     &result) != LDAP_SUCCESS) {
		SAFE_FREE(filter);
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	count = ldap_count_entries(ldap_state->smbldap_state->ldap_struct,
				   result);

	if (count < 1) {
		DEBUG(4, ("ldapsam_enum_aliasmem: Did not find alias\n"));
		ldap_msgfree(result);
		SAFE_FREE(filter);
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	if (count > 1) {
		DEBUG(1, ("ldapsam_enum_aliasmem: Duplicate entries for "
			  "filter %s: count=%d\n", filter, count));
		ldap_msgfree(result);
		SAFE_FREE(filter);
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	SAFE_FREE(filter);

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct,
				 result);

	if (!entry) {
		ldap_msgfree(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	values = ldap_get_values(ldap_state->smbldap_state->ldap_struct,
				 entry,
				 get_attr_key2string(groupmap_attr_list,
						     LDAP_ATTR_SID_LIST));

	if (values == NULL) {
		ldap_msgfree(result);
		return NT_STATUS_OK;
	}

	count = ldap_count_values(values);

	for (i=0; i<count; i++) {
		struct dom_sid member;
		NTSTATUS status;

		if (!string_to_sid(&member, values[i]))
			continue;

		status = add_sid_to_array(mem_ctx, &member, pp_members,
					  &num_members);
		if (!NT_STATUS_IS_OK(status)) {
			ldap_value_free(values);
			ldap_msgfree(result);
			return status;
		}
	}

	*p_num_members = num_members;
	ldap_value_free(values);
	ldap_msgfree(result);

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_alias_memberships(struct pdb_methods *methods,
					  TALLOC_CTX *mem_ctx,
					  const struct dom_sid *domain_sid,
					  const struct dom_sid *members,
					  size_t num_members,
					  uint32_t **pp_alias_rids,
					  size_t *p_num_alias_rids)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAP *ldap_struct;

	const char *attrs[] = { LDAP_ATTRIBUTE_SID, NULL };

	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int i;
	int rc;
	char *filter;
	enum lsa_SidType type = SID_NAME_USE_NONE;
	bool is_builtin = false;
	bool sid_added = false;

	*pp_alias_rids = NULL;
	*p_num_alias_rids = 0;

	if (sid_check_is_builtin(domain_sid)) {
		is_builtin = true;
		type = SID_NAME_ALIAS;
	}

	if (sid_check_is_domain(domain_sid)) {
		type = SID_NAME_ALIAS;
	}

	if (type == SID_NAME_USE_NONE) {
		DEBUG(5, ("SID %s is neither builtin nor domain!\n",
			  sid_string_dbg(domain_sid)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (num_members == 0) {
		return NT_STATUS_OK;
	}

	filter = talloc_asprintf(mem_ctx,
				 "(&(objectclass=%s)(sambaGroupType=%d)(|",
				 LDAP_OBJ_GROUPMAP, type);

	for (i=0; i<num_members; i++)
		filter = talloc_asprintf(mem_ctx, "%s(sambaSIDList=%s)",
					 filter,
					 sid_string_talloc(mem_ctx,
							   &members[i]));

	filter = talloc_asprintf(mem_ctx, "%s))", filter);

	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (is_builtin &&
	    ldap_state->search_cache.filter &&
	    strcmp(ldap_state->search_cache.filter, filter) == 0) {
		filter = talloc_move(filter, &ldap_state->search_cache.filter);
		result = ldap_state->search_cache.result;
		ldap_state->search_cache.result = NULL;
	} else {
		rc = smbldap_search(ldap_state->smbldap_state, lp_ldap_suffix(),
				    LDAP_SCOPE_SUBTREE, filter, attrs, 0, &result);
		if (rc != LDAP_SUCCESS) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		talloc_autofree_ldapmsg(filter, result);
	}

	ldap_struct = ldap_state->smbldap_state->ldap_struct;

	for (entry = ldap_first_entry(ldap_struct, result);
	     entry != NULL;
	     entry = ldap_next_entry(ldap_struct, entry))
	{
		fstring sid_str;
		struct dom_sid sid;
		uint32_t rid;

		if (!smbldap_get_single_attribute(ldap_struct, entry,
						  LDAP_ATTRIBUTE_SID,
						  sid_str,
						  sizeof(sid_str)-1))
			continue;

		if (!string_to_sid(&sid, sid_str))
			continue;

		if (!sid_peek_check_rid(domain_sid, &sid, &rid))
			continue;

		sid_added = true;

		if (!add_rid_to_array_unique(mem_ctx, rid, pp_alias_rids,
					p_num_alias_rids)) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (!is_builtin && !sid_added) {
		TALLOC_FREE(ldap_state->search_cache.filter);
		/*
		 * Note: result is a talloc child of filter because of the
		 * talloc_autofree_ldapmsg() usage
		 */
		ldap_state->search_cache.filter = talloc_move(ldap_state, &filter);
		ldap_state->search_cache.result = result;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_set_account_policy_in_ldap(struct pdb_methods *methods,
						   enum pdb_policy_type type,
						   uint32_t value)
{
	NTSTATUS ntstatus = NT_STATUS_UNSUCCESSFUL;
	int rc;
	LDAPMod **mods = NULL;
	fstring value_string;
	const char *policy_attr = NULL;

	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;

	DEBUG(10,("ldapsam_set_account_policy_in_ldap\n"));

	if (!ldap_state->domain_dn) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	policy_attr = get_account_policy_attr(type);
	if (policy_attr == NULL) {
		DEBUG(0,("ldapsam_set_account_policy_in_ldap: invalid "
			 "policy\n"));
		return ntstatus;
	}

	slprintf(value_string, sizeof(value_string) - 1, "%i", value);

	smbldap_set_mod(&mods, LDAP_MOD_REPLACE, policy_attr, value_string);

	rc = smbldap_modify(ldap_state->smbldap_state, ldap_state->domain_dn,
			    mods);

	ldap_mods_free(mods, True);

	if (rc != LDAP_SUCCESS) {
		return ntstatus;
	}

	if (!cache_account_policy_set(type, value)) {
		DEBUG(0,("ldapsam_set_account_policy_in_ldap: failed to "
			 "update local tdb cache\n"));
		return ntstatus;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_set_account_policy(struct pdb_methods *methods,
					   enum pdb_policy_type type,
					   uint32_t value)
{
	return ldapsam_set_account_policy_in_ldap(methods, type,
						  value);
}

static NTSTATUS ldapsam_get_account_policy_from_ldap(struct pdb_methods *methods,
						     enum pdb_policy_type type,
						     uint32_t *value)
{
	NTSTATUS ntstatus = NT_STATUS_UNSUCCESSFUL;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int count;
	int rc;
	char **vals = NULL;
	char *filter;
	const char *policy_attr = NULL;

	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;

	const char *attrs[2];

	DEBUG(10,("ldapsam_get_account_policy_from_ldap\n"));

	if (!ldap_state->domain_dn) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	policy_attr = get_account_policy_attr(type);
	if (!policy_attr) {
		DEBUG(0,("ldapsam_get_account_policy_from_ldap: invalid "
			 "policy index: %d\n", type));
		return ntstatus;
	}

	attrs[0] = policy_attr;
	attrs[1] = NULL;

	filter = talloc_asprintf(talloc_tos(), "(objectClass=%s)", LDAP_OBJ_DOMINFO);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	rc = smbldap_search(ldap_state->smbldap_state, ldap_state->domain_dn,
			    LDAP_SCOPE_BASE, filter, attrs, 0,
			    &result);
	TALLOC_FREE(filter);
	if (rc != LDAP_SUCCESS) {
		return ntstatus;
	}

	count = ldap_count_entries(priv2ld(ldap_state), result);
	if (count < 1) {
		goto out;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (entry == NULL) {
		goto out;
	}

	vals = ldap_get_values(priv2ld(ldap_state), entry, policy_attr);
	if (vals == NULL) {
		goto out;
	}

	*value = (uint32_t)atol(vals[0]);

	ntstatus = NT_STATUS_OK;

out:
	if (vals)
		ldap_value_free(vals);
	ldap_msgfree(result);

	return ntstatus;
}

/* wrapper around ldapsam_get_account_policy_from_ldap(), handles tdb as cache 

   - if user hasn't decided to use account policies inside LDAP just reuse the
     old tdb values

   - if there is a valid cache entry, return that
   - if there is an LDAP entry, update cache and return 
   - otherwise set to default, update cache and return

   Guenther
*/
static NTSTATUS ldapsam_get_account_policy(struct pdb_methods *methods,
					   enum pdb_policy_type type,
					   uint32_t *value)
{
	NTSTATUS ntstatus = NT_STATUS_UNSUCCESSFUL;

	if (cache_account_policy_get(type, value)) {
		DEBUG(11,("ldapsam_get_account_policy: got valid value from "
			  "cache\n"));
		return NT_STATUS_OK;
	}

	ntstatus = ldapsam_get_account_policy_from_ldap(methods, type,
							value);
	if (NT_STATUS_IS_OK(ntstatus)) {
		goto update_cache;
	}

	DEBUG(10,("ldapsam_get_account_policy: failed to retrieve from "
		  "ldap\n"));

#if 0
	/* should we automagically migrate old tdb value here ? */
	if (account_policy_get(type, value))
		goto update_ldap;

	DEBUG(10,("ldapsam_get_account_policy: no tdb for %d, trying "
		  "default\n", type));
#endif

	if (!account_policy_get_default(type, value)) {
		return ntstatus;
	}

/* update_ldap: */

	ntstatus = ldapsam_set_account_policy(methods, type, *value);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		return ntstatus;
	}

 update_cache:

	if (!cache_account_policy_set(type, *value)) {
		DEBUG(0,("ldapsam_get_account_policy: failed to update local "
			 "tdb as a cache\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_lookup_rids(struct pdb_methods *methods,
				    const struct dom_sid *domain_sid,
				    int num_rids,
				    uint32_t *rids,
				    const char **names,
				    enum lsa_SidType *attrs)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *msg = NULL;
	LDAPMessage *entry;
	char *allsids = NULL;
	int i, rc, num_mapped;
	NTSTATUS result = NT_STATUS_NO_MEMORY;
	TALLOC_CTX *mem_ctx;
	LDAP *ld;
	bool is_builtin;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		goto done;
	}

	if (!sid_check_is_builtin(domain_sid) &&
	    !sid_check_is_domain(domain_sid)) {
		result = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	if (num_rids == 0) {
		result = NT_STATUS_NONE_MAPPED;
		goto done;
	}

	for (i=0; i<num_rids; i++)
		attrs[i] = SID_NAME_UNKNOWN;

	allsids = talloc_strdup(mem_ctx, "");
	if (allsids == NULL) {
		goto done;
	}

	for (i=0; i<num_rids; i++) {
		struct dom_sid sid;
		sid_compose(&sid, domain_sid, rids[i]);
		allsids = talloc_asprintf_append_buffer(
			allsids, "(sambaSid=%s)",
			sid_string_talloc(mem_ctx, &sid));
		if (allsids == NULL) {
			goto done;
		}
	}

	/* First look for users */

	{
		char *filter;
		const char *ldap_attrs[] = { "uid", "sambaSid", NULL };

		filter = talloc_asprintf(
			mem_ctx, ("(&(objectClass=%s)(|%s))"),
			LDAP_OBJ_SAMBASAMACCOUNT, allsids);

		if (filter == NULL) {
			goto done;
		}

		rc = smbldap_search(ldap_state->smbldap_state,
				    lp_ldap_user_suffix(),
				    LDAP_SCOPE_SUBTREE, filter, ldap_attrs, 0,
				    &msg);
		talloc_autofree_ldapmsg(mem_ctx, msg);
	}

	if (rc != LDAP_SUCCESS)
		goto done;

	ld = ldap_state->smbldap_state->ldap_struct;
	num_mapped = 0;

	for (entry = ldap_first_entry(ld, msg);
	     entry != NULL;
	     entry = ldap_next_entry(ld, entry)) {
		uint32_t rid;
		int rid_index;
		const char *name;

		if (!ldapsam_extract_rid_from_entry(ld, entry, domain_sid,
						    &rid)) {
			DEBUG(2, ("Could not find sid from ldap entry\n"));
			continue;
		}

		name = smbldap_talloc_single_attribute(ld, entry, "uid",
						       names);
		if (name == NULL) {
			DEBUG(2, ("Could not retrieve uid attribute\n"));
			continue;
		}

		for (rid_index = 0; rid_index < num_rids; rid_index++) {
			if (rid == rids[rid_index])
				break;
		}

		if (rid_index == num_rids) {
			DEBUG(2, ("Got a RID not asked for: %d\n", rid));
			continue;
		}

		attrs[rid_index] = SID_NAME_USER;
		names[rid_index] = name;
		num_mapped += 1;
	}

	if (num_mapped == num_rids) {
		/* No need to look for groups anymore -- we're done */
		result = NT_STATUS_OK;
		goto done;
	}

	/* Same game for groups */

	{
		char *filter;
		const char *ldap_attrs[] = { "cn", "displayName", "sambaSid",
					     "sambaGroupType", NULL };

		filter = talloc_asprintf(
			mem_ctx, "(&(objectClass=%s)(|%s))",
			LDAP_OBJ_GROUPMAP, allsids);
		if (filter == NULL) {
			goto done;
		}

		rc = smbldap_search(ldap_state->smbldap_state,
				    lp_ldap_suffix(),
				    LDAP_SCOPE_SUBTREE, filter, ldap_attrs, 0,
				    &msg);
		talloc_autofree_ldapmsg(mem_ctx, msg);
	}

	if (rc != LDAP_SUCCESS)
		goto done;

	/* ldap_struct might have changed due to a reconnect */

	ld = ldap_state->smbldap_state->ldap_struct;

	/* For consistency checks, we already checked we're only domain or builtin */

	is_builtin = sid_check_is_builtin(domain_sid);

	for (entry = ldap_first_entry(ld, msg);
	     entry != NULL;
	     entry = ldap_next_entry(ld, entry))
	{
		uint32_t rid;
		int rid_index;
		const char *attr;
		enum lsa_SidType type;
		const char *dn = smbldap_talloc_dn(mem_ctx, ld, entry);

		attr = smbldap_talloc_single_attribute(ld, entry, "sambaGroupType",
						       mem_ctx);
		if (attr == NULL) {
			DEBUG(2, ("Could not extract type from ldap entry %s\n",
				  dn));
			continue;
		}

		type = (enum lsa_SidType)atol(attr);

		/* Consistency checks */
		if ((is_builtin && (type != SID_NAME_ALIAS)) ||
		    (!is_builtin && ((type != SID_NAME_ALIAS) &&
				     (type != SID_NAME_DOM_GRP)))) {
			DEBUG(2, ("Rejecting invalid group mapping entry %s\n", dn));
		}

		if (!ldapsam_extract_rid_from_entry(ld, entry, domain_sid,
						    &rid)) {
			DEBUG(2, ("Could not find sid from ldap entry %s\n", dn));
			continue;
		}

		attr = smbldap_talloc_single_attribute(ld, entry, "displayName", names);

		if (attr == NULL) {
			DEBUG(10, ("Could not retrieve 'displayName' attribute from %s\n",
				   dn));
			attr = smbldap_talloc_single_attribute(ld, entry, "cn", names);
		}

		if (attr == NULL) {
			DEBUG(2, ("Could not retrieve naming attribute from %s\n",
				  dn));
			continue;
		}

		for (rid_index = 0; rid_index < num_rids; rid_index++) {
			if (rid == rids[rid_index])
				break;
		}

		if (rid_index == num_rids) {
			DEBUG(2, ("Got a RID not asked for: %d\n", rid));
			continue;
		}

		attrs[rid_index] = type;
		names[rid_index] = attr;
		num_mapped += 1;
	}

	result = NT_STATUS_NONE_MAPPED;

	if (num_mapped > 0)
		result = (num_mapped == num_rids) ?
			NT_STATUS_OK : STATUS_SOME_UNMAPPED;
 done:
	TALLOC_FREE(mem_ctx);
	return result;
}

static char *get_ldap_filter(TALLOC_CTX *mem_ctx, const char *username)
{
	char *filter = NULL;
	char *escaped = NULL;
	char *result = NULL;

	if (asprintf(&filter, "(&%s(objectclass=%s))",
			  "(uid=%u)", LDAP_OBJ_SAMBASAMACCOUNT) < 0) {
		goto done;
	}

	escaped = escape_ldap_string(talloc_tos(), username);
	if (escaped == NULL) goto done;

	result = talloc_string_sub(mem_ctx, filter, "%u", username);

 done:
	SAFE_FREE(filter);
	TALLOC_FREE(escaped);

	return result;
}

static const char **talloc_attrs(TALLOC_CTX *mem_ctx, ...)
{
	int i, num = 0;
	va_list ap;
	const char **result;

	va_start(ap, mem_ctx);
	while (va_arg(ap, const char *) != NULL)
		num += 1;
	va_end(ap);

	if ((result = TALLOC_ARRAY(mem_ctx, const char *, num+1)) == NULL) {
		return NULL;
	}

	va_start(ap, mem_ctx);
	for (i=0; i<num; i++) {
		result[i] = talloc_strdup(result, va_arg(ap, const char*));
		if (result[i] == NULL) {
			talloc_free(result);
			va_end(ap);
			return NULL;
		}
	}
	va_end(ap);

	result[num] = NULL;
	return result;
}

struct ldap_search_state {
	struct smbldap_state *connection;

	uint32_t acct_flags;
	uint16_t group_type;

	const char *base;
	int scope;
	const char *filter;
	const char **attrs;
	int attrsonly;
	void *pagedresults_cookie;

	LDAPMessage *entries, *current_entry;
	bool (*ldap2displayentry)(struct ldap_search_state *state,
				  TALLOC_CTX *mem_ctx,
				  LDAP *ld, LDAPMessage *entry,
				  struct samr_displayentry *result);
};

static bool ldapsam_search_firstpage(struct pdb_search *search)
{
	struct ldap_search_state *state =
		(struct ldap_search_state *)search->private_data;
	LDAP *ld;
	int rc = LDAP_OPERATIONS_ERROR;

	state->entries = NULL;

	if (state->connection->paged_results) {
		rc = smbldap_search_paged(state->connection, state->base,
					  state->scope, state->filter,
					  state->attrs, state->attrsonly,
					  lp_ldap_page_size(), &state->entries,
					  &state->pagedresults_cookie);
	}

	if ((rc != LDAP_SUCCESS) || (state->entries == NULL)) {

		if (state->entries != NULL) {
			/* Left over from unsuccessful paged attempt */
			ldap_msgfree(state->entries);
			state->entries = NULL;
		}

		rc = smbldap_search(state->connection, state->base,
				    state->scope, state->filter, state->attrs,
				    state->attrsonly, &state->entries);

		if ((rc != LDAP_SUCCESS) || (state->entries == NULL))
			return False;

		/* Ok, the server was lying. It told us it could do paged
		 * searches when it could not. */
		state->connection->paged_results = False;
	}

        ld = state->connection->ldap_struct;
        if ( ld == NULL) {
                DEBUG(5, ("Don't have an LDAP connection right after a "
			  "search\n"));
                return False;
        }
        state->current_entry = ldap_first_entry(ld, state->entries);

	return True;
}

static bool ldapsam_search_nextpage(struct pdb_search *search)
{
	struct ldap_search_state *state =
		(struct ldap_search_state *)search->private_data;
	int rc;

	if (!state->connection->paged_results) {
		/* There is no next page when there are no paged results */
		return False;
	}

	rc = smbldap_search_paged(state->connection, state->base,
				  state->scope, state->filter, state->attrs,
				  state->attrsonly, lp_ldap_page_size(),
				  &state->entries,
				  &state->pagedresults_cookie);

	if ((rc != LDAP_SUCCESS) || (state->entries == NULL))
		return False;

	state->current_entry = ldap_first_entry(state->connection->ldap_struct, state->entries);

	if (state->current_entry == NULL) {
		ldap_msgfree(state->entries);
		state->entries = NULL;
		return false;
	}

	return True;
}

static bool ldapsam_search_next_entry(struct pdb_search *search,
				      struct samr_displayentry *entry)
{
	struct ldap_search_state *state =
		(struct ldap_search_state *)search->private_data;
	bool result;

 retry:
	if ((state->entries == NULL) && (state->pagedresults_cookie == NULL))
		return False;

	if ((state->entries == NULL) &&
	    !ldapsam_search_nextpage(search))
		    return False;

	if (state->current_entry == NULL) {
		return false;
	}

	result = state->ldap2displayentry(state, search,
					  state->connection->ldap_struct,
					  state->current_entry, entry);

	if (!result) {
		char *dn;
		dn = ldap_get_dn(state->connection->ldap_struct, state->current_entry);
		DEBUG(5, ("Skipping entry %s\n", dn != NULL ? dn : "<NULL>"));
		if (dn != NULL) ldap_memfree(dn);
	}

	state->current_entry = ldap_next_entry(state->connection->ldap_struct, state->current_entry);

	if (state->current_entry == NULL) {
		ldap_msgfree(state->entries);
		state->entries = NULL;
	}

	if (!result) goto retry;

	return True;
}

static void ldapsam_search_end(struct pdb_search *search)
{
	struct ldap_search_state *state =
		(struct ldap_search_state *)search->private_data;
	int rc;

	if (state->pagedresults_cookie == NULL)
		return;

	if (state->entries != NULL)
		ldap_msgfree(state->entries);

	state->entries = NULL;
	state->current_entry = NULL;

	if (!state->connection->paged_results)
		return;

	/* Tell the LDAP server we're not interested in the rest anymore. */

	rc = smbldap_search_paged(state->connection, state->base, state->scope,
				  state->filter, state->attrs,
				  state->attrsonly, 0, &state->entries,
				  &state->pagedresults_cookie);

	if (rc != LDAP_SUCCESS)
		DEBUG(5, ("Could not end search properly\n"));

	return;
}

static bool ldapuser2displayentry(struct ldap_search_state *state,
				  TALLOC_CTX *mem_ctx,
				  LDAP *ld, LDAPMessage *entry,
				  struct samr_displayentry *result)
{
	char **vals;
	size_t converted_size;
	struct dom_sid sid;
	uint32_t acct_flags;

	vals = ldap_get_values(ld, entry, "sambaAcctFlags");
	if ((vals == NULL) || (vals[0] == NULL)) {
		DEBUG(5, ("\"sambaAcctFlags\" not found\n"));
		return False;
	}
	acct_flags = pdb_decode_acct_ctrl(vals[0]);
	ldap_value_free(vals);

	if ((state->acct_flags != 0) &&
	    ((state->acct_flags & acct_flags) == 0))
		return False;		

	result->acct_flags = acct_flags;
	result->account_name = "";
	result->fullname = "";
	result->description = "";

	vals = ldap_get_values(ld, entry, "uid");
	if ((vals == NULL) || (vals[0] == NULL)) {
		DEBUG(5, ("\"uid\" not found\n"));
		return False;
	}
	if (!pull_utf8_talloc(mem_ctx,
			      CONST_DISCARD(char **, &result->account_name),
			      vals[0], &converted_size))
	{
		DEBUG(0,("ldapuser2displayentry: pull_utf8_talloc failed: %s",
			 strerror(errno)));
	}

	ldap_value_free(vals);

	vals = ldap_get_values(ld, entry, "displayName");
	if ((vals == NULL) || (vals[0] == NULL))
		DEBUG(8, ("\"displayName\" not found\n"));
	else if (!pull_utf8_talloc(mem_ctx,
				   CONST_DISCARD(char **, &result->fullname),
				   vals[0], &converted_size))
	{
		DEBUG(0,("ldapuser2displayentry: pull_utf8_talloc failed: %s",
			 strerror(errno)));
	}

	ldap_value_free(vals);

	vals = ldap_get_values(ld, entry, "description");
	if ((vals == NULL) || (vals[0] == NULL))
		DEBUG(8, ("\"description\" not found\n"));
	else if (!pull_utf8_talloc(mem_ctx,
				   CONST_DISCARD(char **, &result->description),
				   vals[0], &converted_size))
	{
		DEBUG(0,("ldapuser2displayentry: pull_utf8_talloc failed: %s",
			 strerror(errno)));
	}

	ldap_value_free(vals);

	if ((result->account_name == NULL) ||
	    (result->fullname == NULL) ||
	    (result->description == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	vals = ldap_get_values(ld, entry, "sambaSid");
	if ((vals == NULL) || (vals[0] == NULL)) {
		DEBUG(0, ("\"objectSid\" not found\n"));
		return False;
	}

	if (!string_to_sid(&sid, vals[0])) {
		DEBUG(0, ("Could not convert %s to SID\n", vals[0]));
		ldap_value_free(vals);
		return False;
	}
	ldap_value_free(vals);

	if (!sid_peek_check_rid(get_global_sam_sid(), &sid, &result->rid)) {
		DEBUG(0, ("sid %s does not belong to our domain\n",
			  sid_string_dbg(&sid)));
		return False;
	}

	return True;
}


static bool ldapsam_search_users(struct pdb_methods *methods,
				 struct pdb_search *search,
				 uint32_t acct_flags)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	struct ldap_search_state *state;

	state = talloc(search, struct ldap_search_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	state->connection = ldap_state->smbldap_state;

	if ((acct_flags != 0) && ((acct_flags & ACB_NORMAL) != 0))
		state->base = lp_ldap_user_suffix();
	else if ((acct_flags != 0) &&
		 ((acct_flags & (ACB_WSTRUST|ACB_SVRTRUST|ACB_DOMTRUST)) != 0))
		state->base = lp_ldap_machine_suffix();
	else
		state->base = lp_ldap_suffix();

	state->acct_flags = acct_flags;
	state->base = talloc_strdup(search, state->base);
	state->scope = LDAP_SCOPE_SUBTREE;
	state->filter = get_ldap_filter(search, "*");
	state->attrs = talloc_attrs(search, "uid", "sambaSid",
				    "displayName", "description",
				    "sambaAcctFlags", NULL);
	state->attrsonly = 0;
	state->pagedresults_cookie = NULL;
	state->entries = NULL;
	state->ldap2displayentry = ldapuser2displayentry;

	if ((state->filter == NULL) || (state->attrs == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	search->private_data = state;
	search->next_entry = ldapsam_search_next_entry;
	search->search_end = ldapsam_search_end;

	return ldapsam_search_firstpage(search);
}

static bool ldapgroup2displayentry(struct ldap_search_state *state,
				   TALLOC_CTX *mem_ctx,
				   LDAP *ld, LDAPMessage *entry,
				   struct samr_displayentry *result)
{
	char **vals;
	size_t converted_size;
	struct dom_sid sid;
	uint16_t group_type;

	result->account_name = "";
	result->fullname = "";
	result->description = "";


	vals = ldap_get_values(ld, entry, "sambaGroupType");
	if ((vals == NULL) || (vals[0] == NULL)) {
		DEBUG(5, ("\"sambaGroupType\" not found\n"));
		if (vals != NULL) {
			ldap_value_free(vals);
		}
		return False;
	}

	group_type = atoi(vals[0]);

	if ((state->group_type != 0) &&
	    ((state->group_type != group_type))) {
		ldap_value_free(vals);
		return False;
	}

	ldap_value_free(vals);

	/* display name is the NT group name */

	vals = ldap_get_values(ld, entry, "displayName");
	if ((vals == NULL) || (vals[0] == NULL)) {
		DEBUG(8, ("\"displayName\" not found\n"));

		/* fallback to the 'cn' attribute */
		vals = ldap_get_values(ld, entry, "cn");
		if ((vals == NULL) || (vals[0] == NULL)) {
			DEBUG(5, ("\"cn\" not found\n"));
			return False;
		}
		if (!pull_utf8_talloc(mem_ctx,
				      CONST_DISCARD(char **,
						    &result->account_name),
				      vals[0], &converted_size))
		{
			DEBUG(0,("ldapgroup2displayentry: pull_utf8_talloc "
				  "failed: %s", strerror(errno)));
		}
	}
	else if (!pull_utf8_talloc(mem_ctx,
				   CONST_DISCARD(char **,
						 &result->account_name),
				   vals[0], &converted_size))
	{
		DEBUG(0,("ldapgroup2displayentry: pull_utf8_talloc failed: %s",
			  strerror(errno)));
	}

	ldap_value_free(vals);

	vals = ldap_get_values(ld, entry, "description");
	if ((vals == NULL) || (vals[0] == NULL))
		DEBUG(8, ("\"description\" not found\n"));
	else if (!pull_utf8_talloc(mem_ctx,
				   CONST_DISCARD(char **, &result->description),
				   vals[0], &converted_size))
	{
		DEBUG(0,("ldapgroup2displayentry: pull_utf8_talloc failed: %s",
			  strerror(errno)));
	}
	ldap_value_free(vals);

	if ((result->account_name == NULL) ||
	    (result->fullname == NULL) ||
	    (result->description == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	vals = ldap_get_values(ld, entry, "sambaSid");
	if ((vals == NULL) || (vals[0] == NULL)) {
		DEBUG(0, ("\"objectSid\" not found\n"));
		if (vals != NULL) {
			ldap_value_free(vals);
		}
		return False;
	}

	if (!string_to_sid(&sid, vals[0])) {
		DEBUG(0, ("Could not convert %s to SID\n", vals[0]));
		return False;
	}

	ldap_value_free(vals);

	switch (group_type) {
		case SID_NAME_DOM_GRP:
		case SID_NAME_ALIAS:

			if (!sid_peek_check_rid(get_global_sam_sid(), &sid, &result->rid) 
				&& !sid_peek_check_rid(&global_sid_Builtin, &sid, &result->rid)) 
			{
				DEBUG(0, ("%s is not in our domain\n",
					  sid_string_dbg(&sid)));
				return False;
			}
			break;

		default:
			DEBUG(0,("unknown group type: %d\n", group_type));
			return False;
	}

	result->acct_flags = 0;

	return True;
}

static bool ldapsam_search_grouptype(struct pdb_methods *methods,
				     struct pdb_search *search,
                                     const struct dom_sid *sid,
				     enum lsa_SidType type)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	struct ldap_search_state *state;
	fstring tmp;

	state = talloc(search, struct ldap_search_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	state->connection = ldap_state->smbldap_state;

	state->base = talloc_strdup(search, lp_ldap_suffix());
	state->connection = ldap_state->smbldap_state;
	state->scope = LDAP_SCOPE_SUBTREE;
	state->filter =	talloc_asprintf(search, "(&(objectclass=%s)"
					"(sambaGroupType=%d)(sambaSID=%s*))",
					 LDAP_OBJ_GROUPMAP,
					 type, sid_to_fstring(tmp, sid));
	state->attrs = talloc_attrs(search, "cn", "sambaSid",
				    "displayName", "description",
				    "sambaGroupType", NULL);
	state->attrsonly = 0;
	state->pagedresults_cookie = NULL;
	state->entries = NULL;
	state->group_type = type;
	state->ldap2displayentry = ldapgroup2displayentry;

	if ((state->filter == NULL) || (state->attrs == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		return False;
	}

	search->private_data = state;
	search->next_entry = ldapsam_search_next_entry;
	search->search_end = ldapsam_search_end;

	return ldapsam_search_firstpage(search);
}

static bool ldapsam_search_groups(struct pdb_methods *methods,
				  struct pdb_search *search)
{
	return ldapsam_search_grouptype(methods, search, get_global_sam_sid(), SID_NAME_DOM_GRP);
}

static bool ldapsam_search_aliases(struct pdb_methods *methods,
				   struct pdb_search *search,
				   const struct dom_sid *sid)
{
	return ldapsam_search_grouptype(methods, search, sid, SID_NAME_ALIAS);
}

static uint32_t ldapsam_capabilities(struct pdb_methods *methods)
{
	return PDB_CAP_STORE_RIDS;
}

static NTSTATUS ldapsam_get_new_rid(struct ldapsam_privates *priv,
				    uint32_t *rid)
{
	struct smbldap_state *smbldap_state = priv->smbldap_state;

	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	NTSTATUS status;
	char *value;
	int rc;
	uint32_t nextRid = 0;
	const char *dn;

	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	status = smbldap_search_domain_info(smbldap_state, &result,
					    get_global_sam_name(), False);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Could not get domain info: %s\n",
			  nt_errstr(status)));
		goto done;
	}

	talloc_autofree_ldapmsg(mem_ctx, result);

	entry = ldap_first_entry(priv2ld(priv), result);
	if (entry == NULL) {
		DEBUG(0, ("Could not get domain info entry\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	/* Find the largest of the three attributes "sambaNextRid",
	   "sambaNextGroupRid" and "sambaNextUserRid". I gave up on the
	   concept of differentiating between user and group rids, and will
	   use only "sambaNextRid" in the future. But for compatibility
	   reasons I look if others have chosen different strategies -- VL */

	value = smbldap_talloc_single_attribute(priv2ld(priv), entry,
						"sambaNextRid",	mem_ctx);
	if (value != NULL) {
		uint32_t tmp = (uint32_t)strtoul(value, NULL, 10);
		nextRid = MAX(nextRid, tmp);
	}

	value = smbldap_talloc_single_attribute(priv2ld(priv), entry,
						"sambaNextUserRid", mem_ctx);
	if (value != NULL) {
		uint32_t tmp = (uint32_t)strtoul(value, NULL, 10);
		nextRid = MAX(nextRid, tmp);
	}

	value = smbldap_talloc_single_attribute(priv2ld(priv), entry,
						"sambaNextGroupRid", mem_ctx);
	if (value != NULL) {
		uint32_t tmp = (uint32_t)strtoul(value, NULL, 10);
		nextRid = MAX(nextRid, tmp);
	}

	if (nextRid == 0) {
		nextRid = BASE_RID-1;
	}

	nextRid += 1;

	smbldap_make_mod(priv2ld(priv), entry, &mods, "sambaNextRid",
			 talloc_asprintf(mem_ctx, "%d", nextRid));
	talloc_autofree_ldapmod(mem_ctx, mods);

	if ((dn = smbldap_talloc_dn(mem_ctx, priv2ld(priv), entry)) == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	rc = smbldap_modify(smbldap_state, dn, mods);

	/* ACCESS_DENIED is used as a placeholder for "the modify failed,
	 * please retry" */

	status = (rc == LDAP_SUCCESS) ? NT_STATUS_OK : NT_STATUS_ACCESS_DENIED;

 done:
	if (NT_STATUS_IS_OK(status)) {
		*rid = nextRid;
	}

	TALLOC_FREE(mem_ctx);
	return status;
}

static NTSTATUS ldapsam_new_rid_internal(struct pdb_methods *methods, uint32_t *rid)
{
	int i;

	for (i=0; i<10; i++) {
		NTSTATUS result = ldapsam_get_new_rid(
			(struct ldapsam_privates *)methods->private_data, rid);
		if (NT_STATUS_IS_OK(result)) {
			return result;
		}

		if (!NT_STATUS_EQUAL(result, NT_STATUS_ACCESS_DENIED)) {
			return result;
		}

		/* The ldap update failed (maybe a race condition), retry */
	}

	/* Tried 10 times, fail. */
	return NT_STATUS_ACCESS_DENIED;
}

static bool ldapsam_new_rid(struct pdb_methods *methods, uint32_t *rid)
{
	NTSTATUS result = ldapsam_new_rid_internal(methods, rid);
	return NT_STATUS_IS_OK(result) ? True : False;
}

static bool ldapsam_sid_to_id(struct pdb_methods *methods,
			      const struct dom_sid *sid,
			      union unid_t *id, enum lsa_SidType *type)
{
	struct ldapsam_privates *priv =
		(struct ldapsam_privates *)methods->private_data;
	char *filter;
	const char *attrs[] = { "sambaGroupType", "gidNumber", "uidNumber",
				NULL };
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	bool ret = False;
	char *value;
	int rc;

	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return False;
	}

	filter = talloc_asprintf(mem_ctx,
				 "(&(sambaSid=%s)"
				 "(|(objectClass=%s)(objectClass=%s)))",
				 sid_string_talloc(mem_ctx, sid),
				 LDAP_OBJ_GROUPMAP, LDAP_OBJ_SAMBASAMACCOUNT);
	if (filter == NULL) {
		DEBUG(5, ("talloc_asprintf failed\n"));
		goto done;
	}

	rc = smbldap_search_suffix(priv->smbldap_state, filter,
				   attrs, &result);
	if (rc != LDAP_SUCCESS) {
		goto done;
	}
	talloc_autofree_ldapmsg(mem_ctx, result);

	if (ldap_count_entries(priv2ld(priv), result) != 1) {
		DEBUG(10, ("Got %d entries, expected one\n",
			   ldap_count_entries(priv2ld(priv), result)));
		goto done;
	}

	entry = ldap_first_entry(priv2ld(priv), result);

	value = smbldap_talloc_single_attribute(priv2ld(priv), entry,
						"sambaGroupType", mem_ctx);

	if (value != NULL) {
		const char *gid_str;
		/* It's a group */

		gid_str = smbldap_talloc_single_attribute(
			priv2ld(priv), entry, "gidNumber", mem_ctx);
		if (gid_str == NULL) {
			DEBUG(1, ("%s has sambaGroupType but no gidNumber\n",
				  smbldap_talloc_dn(mem_ctx, priv2ld(priv),
						    entry)));
			goto done;
		}

		id->gid = strtoul(gid_str, NULL, 10);
		*type = (enum lsa_SidType)strtoul(value, NULL, 10);
		store_gid_sid_cache(sid, id->gid);
		idmap_cache_set_sid2gid(sid, id->gid);
		ret = True;
		goto done;
	}

	/* It must be a user */

	value = smbldap_talloc_single_attribute(priv2ld(priv), entry,
						"uidNumber", mem_ctx);
	if (value == NULL) {
		DEBUG(1, ("Could not find uidNumber in %s\n",
			  smbldap_talloc_dn(mem_ctx, priv2ld(priv), entry)));
		goto done;
	}

	id->uid = strtoul(value, NULL, 10);
	*type = SID_NAME_USER;
	store_uid_sid_cache(sid, id->uid);
	idmap_cache_set_sid2uid(sid, id->uid);

	ret = True;
 done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

/**
 * Find the SID for a uid.
 * This is shortcut is only used if ldapsam:trusted is set to true.
 */
static bool ldapsam_uid_to_sid(struct pdb_methods *methods, uid_t uid,
			       struct dom_sid *sid)
{
	struct ldapsam_privates *priv =
		(struct ldapsam_privates *)methods->private_data;
	char *filter;
	const char *attrs[] = { "sambaSID", NULL };
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	bool ret = false;
	char *user_sid_string;
	struct dom_sid user_sid;
	int rc;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	filter = talloc_asprintf(tmp_ctx,
				 "(&(uidNumber=%u)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 (unsigned int)uid,
				 LDAP_OBJ_POSIXACCOUNT,
				 LDAP_OBJ_SAMBASAMACCOUNT);
	if (filter == NULL) {
		DEBUG(3, ("talloc_asprintf failed\n"));
		goto done;
	}

	rc = smbldap_search_suffix(priv->smbldap_state, filter, attrs, &result);
	if (rc != LDAP_SUCCESS) {
		goto done;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	if (ldap_count_entries(priv2ld(priv), result) != 1) {
		DEBUG(3, ("ERROR: Got %d entries for uid %u, expected one\n",
			   ldap_count_entries(priv2ld(priv), result),
			   (unsigned int)uid));
		goto done;
	}

	entry = ldap_first_entry(priv2ld(priv), result);

	user_sid_string = smbldap_talloc_single_attribute(priv2ld(priv), entry,
							  "sambaSID", tmp_ctx);
	if (user_sid_string == NULL) {
		DEBUG(1, ("Could not find sambaSID in object '%s'\n",
			  smbldap_talloc_dn(tmp_ctx, priv2ld(priv), entry)));
		goto done;
	}

	if (!string_to_sid(&user_sid, user_sid_string)) {
		DEBUG(3, ("Error calling sid_string_talloc for sid '%s'\n",
			  user_sid_string));
		goto done;
	}

	sid_copy(sid, &user_sid);

	store_uid_sid_cache(sid, uid);
	idmap_cache_set_sid2uid(sid, uid);

	ret = true;

 done:
	TALLOC_FREE(tmp_ctx);
	return ret;
}

/**
 * Find the SID for a gid.
 * This is shortcut is only used if ldapsam:trusted is set to true.
 */
static bool ldapsam_gid_to_sid(struct pdb_methods *methods, gid_t gid,
			       struct dom_sid *sid)
{
	struct ldapsam_privates *priv =
		(struct ldapsam_privates *)methods->private_data;
	char *filter;
	const char *attrs[] = { "sambaSID", NULL };
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	bool ret = false;
	char *group_sid_string;
	struct dom_sid group_sid;
	int rc;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	filter = talloc_asprintf(tmp_ctx,
				 "(&(gidNumber=%u)"
				 "(objectClass=%s))",
				 (unsigned int)gid,
				 LDAP_OBJ_GROUPMAP);
	if (filter == NULL) {
		DEBUG(3, ("talloc_asprintf failed\n"));
		goto done;
	}

	rc = smbldap_search_suffix(priv->smbldap_state, filter, attrs, &result);
	if (rc != LDAP_SUCCESS) {
		goto done;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	if (ldap_count_entries(priv2ld(priv), result) != 1) {
		DEBUG(3, ("ERROR: Got %d entries for gid %u, expected one\n",
			   ldap_count_entries(priv2ld(priv), result),
			   (unsigned int)gid));
		goto done;
	}

	entry = ldap_first_entry(priv2ld(priv), result);

	group_sid_string = smbldap_talloc_single_attribute(priv2ld(priv), entry,
							  "sambaSID", tmp_ctx);
	if (group_sid_string == NULL) {
		DEBUG(1, ("Could not find sambaSID in object '%s'\n",
			  smbldap_talloc_dn(tmp_ctx, priv2ld(priv), entry)));
		goto done;
	}

	if (!string_to_sid(&group_sid, group_sid_string)) {
		DEBUG(3, ("Error calling sid_string_talloc for sid '%s'\n",
			  group_sid_string));
		goto done;
	}

	sid_copy(sid, &group_sid);

	store_gid_sid_cache(sid, gid);
	idmap_cache_set_sid2gid(sid, gid);

	ret = true;

 done:
	TALLOC_FREE(tmp_ctx);
	return ret;
}


/*
 * The following functions are called only if
 * ldapsam:trusted and ldapsam:editposix are
 * set to true
 */

/*
 * ldapsam_create_user creates a new
 * posixAccount and sambaSamAccount object
 * in the ldap users subtree
 *
 * The uid is allocated by winbindd.
 */

static NTSTATUS ldapsam_create_user(struct pdb_methods *my_methods,
				    TALLOC_CTX *tmp_ctx, const char *name,
				    uint32_t acb_info, uint32_t *rid)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *entry = NULL;
	LDAPMessage *result = NULL;
	uint32_t num_result;
	bool is_machine = False;
	bool add_posix = False;
	LDAPMod **mods = NULL;
	struct samu *user;
	char *filter;
	char *username;
	char *homedir;
	char *gidstr;
	char *uidstr;
	char *shell;
	const char *dn = NULL;
	struct dom_sid group_sid;
	struct dom_sid user_sid;
	gid_t gid = -1;
	uid_t uid = -1;
	NTSTATUS ret;
	int rc;

	if (((acb_info & ACB_NORMAL) && name[strlen(name)-1] == '$') ||
	      acb_info & ACB_WSTRUST ||
	      acb_info & ACB_SVRTRUST ||
	      acb_info & ACB_DOMTRUST) {
		is_machine = True;
	}

	username = escape_ldap_string(talloc_tos(), name);
	filter = talloc_asprintf(tmp_ctx, "(&(uid=%s)(objectClass=%s))",
				 username, LDAP_OBJ_POSIXACCOUNT);
	TALLOC_FREE(username);

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_create_user: ldap search failed!\n"));
		return NT_STATUS_ACCESS_DENIED;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_create_user: More than one user with name [%s] ?!\n", name));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (num_result == 1) {
		char *tmp;
		/* check if it is just a posix account.
		 * or if there is a sid attached to this entry
		 */

		entry = ldap_first_entry(priv2ld(ldap_state), result);
		if (!entry) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		tmp = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "sambaSID", tmp_ctx);
		if (tmp) {
			DEBUG (1, ("ldapsam_create_user: The user [%s] already exist!\n", name));
			return NT_STATUS_USER_EXISTS;
		}

		/* it is just a posix account, retrieve the dn for later use */
		dn = smbldap_talloc_dn(tmp_ctx, priv2ld(ldap_state), entry);
		if (!dn) {
			DEBUG(0,("ldapsam_create_user: Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (num_result == 0) {
		add_posix = True;
	}

	/* Create the basic samu structure and generate the mods for the ldap commit */
	if (!NT_STATUS_IS_OK((ret = ldapsam_new_rid_internal(my_methods, rid)))) {
		DEBUG(1, ("ldapsam_create_user: Could not allocate a new RID\n"));
		return ret;
	}

	sid_compose(&user_sid, get_global_sam_sid(), *rid);

	user = samu_new(tmp_ctx);
	if (!user) {
		DEBUG(1,("ldapsam_create_user: Unable to allocate user struct\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (!pdb_set_username(user, name, PDB_SET)) {
		DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	if (!pdb_set_domain(user, get_global_sam_name(), PDB_SET)) {
		DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	if (is_machine) {
		if (acb_info & ACB_NORMAL) {
			if (!pdb_set_acct_ctrl(user, ACB_WSTRUST, PDB_SET)) {
				DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
				return NT_STATUS_UNSUCCESSFUL;
			}
		} else {
			if (!pdb_set_acct_ctrl(user, acb_info, PDB_SET)) {
				DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
				return NT_STATUS_UNSUCCESSFUL;
			}
		}
	} else {
		if (!pdb_set_acct_ctrl(user, ACB_NORMAL | ACB_DISABLED, PDB_SET)) {
			DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	if (!pdb_set_user_sid(user, &user_sid, PDB_SET)) {
		DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (!init_ldap_from_sam(ldap_state, entry, &mods, user, element_is_set_or_changed)) {
		DEBUG(1,("ldapsam_create_user: Unable to fill user structs\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (ldap_state->schema_ver != SCHEMAVER_SAMBASAMACCOUNT) {
		DEBUG(1,("ldapsam_create_user: Unsupported schema version\n"));
	}
	smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", LDAP_OBJ_SAMBASAMACCOUNT);

	if (add_posix) {
		char *escape_name;

		DEBUG(3,("ldapsam_create_user: Creating new posix user\n"));

		/* retrieve the Domain Users group gid */
		if (!sid_compose(&group_sid, get_global_sam_sid(), DOMAIN_RID_USERS) ||
		    !sid_to_gid(&group_sid, &gid)) {
			DEBUG (0, ("ldapsam_create_user: Unable to get the Domain Users gid: bailing out!\n"));
			return NT_STATUS_INVALID_PRIMARY_GROUP;
		}

		/* lets allocate a new userid for this user */
		if (!winbind_allocate_uid(&uid)) {
			DEBUG (0, ("ldapsam_create_user: Unable to allocate a new user id: bailing out!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}


		if (is_machine) {
			/* TODO: choose a more appropriate default for machines */
			homedir = talloc_sub_specified(tmp_ctx, lp_template_homedir(), "SMB_workstations_home", ldap_state->domain_name, uid, gid);
			shell = talloc_strdup(tmp_ctx, "/bin/false");
		} else {
			homedir = talloc_sub_specified(tmp_ctx, lp_template_homedir(), name, ldap_state->domain_name, uid, gid);
			shell = talloc_sub_specified(tmp_ctx, lp_template_shell(), name, ldap_state->domain_name, uid, gid);
		}
		uidstr = talloc_asprintf(tmp_ctx, "%u", (unsigned int)uid);
		gidstr = talloc_asprintf(tmp_ctx, "%u", (unsigned int)gid);

		escape_name = escape_rdn_val_string_alloc(name);
		if (!escape_name) {
			DEBUG (0, ("ldapsam_create_user: Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}

		if (is_machine) {
			dn = talloc_asprintf(tmp_ctx, "uid=%s,%s", escape_name, lp_ldap_machine_suffix ());
		} else {
			dn = talloc_asprintf(tmp_ctx, "uid=%s,%s", escape_name, lp_ldap_user_suffix ());
		}

		SAFE_FREE(escape_name);

		if (!homedir || !shell || !uidstr || !gidstr || !dn) {
			DEBUG (0, ("ldapsam_create_user: Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}

		smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", LDAP_OBJ_ACCOUNT);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", LDAP_OBJ_POSIXACCOUNT);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "cn", name);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "uidNumber", uidstr);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "gidNumber", gidstr);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "homeDirectory", homedir);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "loginShell", shell);
	}

	talloc_autofree_ldapmod(tmp_ctx, mods);

	if (add_posix) {	
		rc = smbldap_add(ldap_state->smbldap_state, dn, mods);
	} else {
		rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);
	}	

	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_create_user: failed to create a new user [%s] (dn = %s)\n", name ,dn));
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(2,("ldapsam_create_user: added account [%s] in the LDAP database\n", name));

	flush_pwnam_cache();

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_delete_user(struct pdb_methods *my_methods, TALLOC_CTX *tmp_ctx, struct samu *sam_acct)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int num_result;
	const char *dn;
	char *filter;
	int rc;

	DEBUG(0,("ldapsam_delete_user: Attempt to delete user [%s]\n", pdb_get_username(sam_acct)));

	filter = talloc_asprintf(tmp_ctx,
				 "(&(uid=%s)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 pdb_get_username(sam_acct),
				 LDAP_OBJ_POSIXACCOUNT,
				 LDAP_OBJ_SAMBASAMACCOUNT);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_delete_user: user search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result == 0) {
		DEBUG(0,("ldapsam_delete_user: user not found!\n"));
		return NT_STATUS_NO_SUCH_USER;
	}

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_delete_user: More than one user with name [%s] ?!\n", pdb_get_username(sam_acct)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (!entry) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* it is just a posix account, retrieve the dn for later use */
	dn = smbldap_talloc_dn(tmp_ctx, priv2ld(ldap_state), entry);
	if (!dn) {
		DEBUG(0,("ldapsam_delete_user: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* try to remove memberships first */
	{
		NTSTATUS status;
		struct dom_sid *sids = NULL;
		gid_t *gids = NULL;
		uint32_t num_groups = 0;
		int i;
		uint32_t user_rid = pdb_get_user_rid(sam_acct);

		status = ldapsam_enum_group_memberships(my_methods,
							tmp_ctx,
							sam_acct,
							&sids,
							&gids,
							&num_groups);
		if (!NT_STATUS_IS_OK(status)) {
			goto delete_dn;
		}

		for (i=0; i < num_groups; i++) {

			uint32_t group_rid;

			sid_peek_rid(&sids[i], &group_rid);

			ldapsam_del_groupmem(my_methods,
					     tmp_ctx,
					     group_rid,
					     user_rid);
		}
	}

 delete_dn:

	rc = smbldap_delete(ldap_state->smbldap_state, dn);
	if (rc != LDAP_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	flush_pwnam_cache();

	return NT_STATUS_OK;
}

/*
 * ldapsam_create_group creates a new
 * posixGroup and sambaGroupMapping object
 * in the ldap groups subtree
 *
 * The gid is allocated by winbindd.
 */

static NTSTATUS ldapsam_create_dom_group(struct pdb_methods *my_methods,
					 TALLOC_CTX *tmp_ctx,
					 const char *name,
					 uint32_t *rid)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	NTSTATUS ret;
	LDAPMessage *entry = NULL;
	LDAPMessage *result = NULL;
	uint32_t num_result;
	bool is_new_entry = False;
	LDAPMod **mods = NULL;
	char *filter;
	char *groupsidstr;
	char *groupname;
	char *grouptype;
	char *gidstr;
	const char *dn = NULL;
	struct dom_sid group_sid;
	gid_t gid = -1;
	int rc;

	groupname = escape_ldap_string(talloc_tos(), name);
	filter = talloc_asprintf(tmp_ctx, "(&(cn=%s)(objectClass=%s))",
				 groupname, LDAP_OBJ_POSIXGROUP);
	TALLOC_FREE(groupname);

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_create_group: ldap search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_create_group: There exists more than one group with name [%s]: bailing out!\n", name));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (num_result == 1) {
		char *tmp;
		/* check if it is just a posix group.
		 * or if there is a sid attached to this entry
		 */

		entry = ldap_first_entry(priv2ld(ldap_state), result);
		if (!entry) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		tmp = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "sambaSID", tmp_ctx);
		if (tmp) {
			DEBUG (1, ("ldapsam_create_group: The group [%s] already exist!\n", name));
			return NT_STATUS_GROUP_EXISTS;
		}

		/* it is just a posix group, retrieve the gid and the dn for later use */
		tmp = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "gidNumber", tmp_ctx);
		if (!tmp) {
			DEBUG (1, ("ldapsam_create_group: Couldn't retrieve the gidNumber for [%s]?!?!\n", name));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		gid = strtoul(tmp, NULL, 10);

		dn = smbldap_talloc_dn(tmp_ctx, priv2ld(ldap_state), entry);
		if (!dn) {
			DEBUG(0,("ldapsam_create_group: Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (num_result == 0) {
		is_new_entry = true;
	}

	if (!NT_STATUS_IS_OK((ret = ldapsam_new_rid_internal(my_methods, rid)))) {
		DEBUG(1, ("ldapsam_create_group: Could not allocate a new RID\n"));
		return ret;
	}

	sid_compose(&group_sid, get_global_sam_sid(), *rid);

	groupsidstr = talloc_strdup(tmp_ctx, sid_string_talloc(tmp_ctx,
							       &group_sid));
	grouptype = talloc_asprintf(tmp_ctx, "%d", SID_NAME_DOM_GRP);

	if (!groupsidstr || !grouptype) {
		DEBUG(0,("ldapsam_create_group: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", LDAP_OBJ_GROUPMAP);
	smbldap_set_mod(&mods, LDAP_MOD_ADD, "sambaSid", groupsidstr);
	smbldap_set_mod(&mods, LDAP_MOD_ADD, "sambaGroupType", grouptype);
	smbldap_set_mod(&mods, LDAP_MOD_ADD, "displayName", name);

	if (is_new_entry) {
		char *escape_name;

		DEBUG(3,("ldapsam_create_user: Creating new posix group\n"));

		/* lets allocate a new groupid for this group */
		if (!winbind_allocate_gid(&gid)) {
			DEBUG (0, ("ldapsam_create_group: Unable to allocate a new group id: bailing out!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}

		gidstr = talloc_asprintf(tmp_ctx, "%u", (unsigned int)gid);

		escape_name = escape_rdn_val_string_alloc(name);
		if (!escape_name) {
			DEBUG (0, ("ldapsam_create_group: Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}

		dn = talloc_asprintf(tmp_ctx, "cn=%s,%s", escape_name, lp_ldap_group_suffix());

		SAFE_FREE(escape_name);

		if (!gidstr || !dn) {
			DEBUG (0, ("ldapsam_create_group: Out of memory!\n"));
			return NT_STATUS_NO_MEMORY;
		}

		smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectclass", LDAP_OBJ_POSIXGROUP);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "cn", name);
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "gidNumber", gidstr);
	}

	talloc_autofree_ldapmod(tmp_ctx, mods);

	if (is_new_entry) {	
		rc = smbldap_add(ldap_state->smbldap_state, dn, mods);
#if 0
		if (rc == LDAP_OBJECT_CLASS_VIOLATION) {
			/* This call may fail with rfc2307bis schema */
			/* Retry adding a structural class */
			smbldap_set_mod(&mods, LDAP_MOD_ADD, "objectClass", "????");
			rc = smbldap_add(ldap_state->smbldap_state, dn, mods);
		}
#endif
	} else {
		rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);
	}	

	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_create_group: failed to create a new group [%s] (dn = %s)\n", name ,dn));
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(2,("ldapsam_create_group: added group [%s] in the LDAP database\n", name));

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_delete_dom_group(struct pdb_methods *my_methods, TALLOC_CTX *tmp_ctx, uint32_t rid)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int num_result;
	const char *dn;
	char *gidstr;
	char *filter;
	struct dom_sid group_sid;
	int rc;

	/* get the group sid */
	sid_compose(&group_sid, get_global_sam_sid(), rid);

	filter = talloc_asprintf(tmp_ctx,
				 "(&(sambaSID=%s)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 sid_string_talloc(tmp_ctx, &group_sid),
				 LDAP_OBJ_POSIXGROUP,
				 LDAP_OBJ_GROUPMAP);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(1,("ldapsam_delete_dom_group: group search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result == 0) {
		DEBUG(1,("ldapsam_delete_dom_group: group not found!\n"));
		return NT_STATUS_NO_SUCH_GROUP;
	}

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_delete_dom_group: More than one group with the same SID ?!\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (!entry) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* here it is, retrieve the dn for later use */
	dn = smbldap_talloc_dn(tmp_ctx, priv2ld(ldap_state), entry);
	if (!dn) {
		DEBUG(0,("ldapsam_delete_dom_group: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	gidstr = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "gidNumber", tmp_ctx);
	if (!gidstr) {
		DEBUG (0, ("ldapsam_delete_dom_group: Unable to find the group's gid!\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* check no user have this group marked as primary group */
	filter = talloc_asprintf(tmp_ctx,
				 "(&(gidNumber=%s)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 gidstr,
				 LDAP_OBJ_POSIXACCOUNT,
				 LDAP_OBJ_SAMBASAMACCOUNT);

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(1,("ldapsam_delete_dom_group: accounts search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result != 0) {
		DEBUG(3,("ldapsam_delete_dom_group: Can't delete group, it is a primary group for %d users\n", num_result));
		return NT_STATUS_MEMBERS_PRIMARY_GROUP;
	}

	rc = smbldap_delete(ldap_state->smbldap_state, dn);
	if (rc != LDAP_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_change_groupmem(struct pdb_methods *my_methods,
					TALLOC_CTX *tmp_ctx,
					uint32_t group_rid,
					uint32_t member_rid,
					int modop)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *entry = NULL;
	LDAPMessage *result = NULL;
	uint32_t num_result;
	LDAPMod **mods = NULL;
	char *filter;
	char *uidstr;
	const char *dn = NULL;
	struct dom_sid group_sid;
	struct dom_sid member_sid;
	int rc;

	switch (modop) {
	case LDAP_MOD_ADD:
		DEBUG(1,("ldapsam_change_groupmem: add new member(rid=%d) to a domain group(rid=%d)", member_rid, group_rid));
		break;
	case LDAP_MOD_DELETE:
		DEBUG(1,("ldapsam_change_groupmem: delete member(rid=%d) from a domain group(rid=%d)", member_rid, group_rid));
		break;
	default:
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* get member sid  */
	sid_compose(&member_sid, get_global_sam_sid(), member_rid);

	/* get the group sid */
	sid_compose(&group_sid, get_global_sam_sid(), group_rid);

	filter = talloc_asprintf(tmp_ctx,
				 "(&(sambaSID=%s)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 sid_string_talloc(tmp_ctx, &member_sid),
				 LDAP_OBJ_POSIXACCOUNT,
				 LDAP_OBJ_SAMBASAMACCOUNT);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* get the member uid */
	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(1,("ldapsam_change_groupmem: member search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result == 0) {
		DEBUG(1,("ldapsam_change_groupmem: member not found!\n"));
		return NT_STATUS_NO_SUCH_MEMBER;
	}

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_change_groupmem: More than one account with the same SID ?!\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (!entry) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (modop == LDAP_MOD_DELETE) {
		/* check if we are trying to remove the member from his primary group */
		char *gidstr;
		gid_t user_gid, group_gid;

		gidstr = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "gidNumber", tmp_ctx);
		if (!gidstr) {
			DEBUG (0, ("ldapsam_change_groupmem: Unable to find the member's gid!\n"));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		user_gid = strtoul(gidstr, NULL, 10);

		if (!sid_to_gid(&group_sid, &group_gid)) {
			DEBUG (0, ("ldapsam_change_groupmem: Unable to get group gid from SID!\n"));
			return NT_STATUS_UNSUCCESSFUL;
		}

		if (user_gid == group_gid) {
			DEBUG (3, ("ldapsam_change_groupmem: can't remove user from its own primary group!\n"));
			return NT_STATUS_MEMBERS_PRIMARY_GROUP;
		}
	}

	/* here it is, retrieve the uid for later use */
	uidstr = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, "uid", tmp_ctx);
	if (!uidstr) {
		DEBUG (0, ("ldapsam_change_groupmem: Unable to find the member's name!\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	filter = talloc_asprintf(tmp_ctx,
				 "(&(sambaSID=%s)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 sid_string_talloc(tmp_ctx, &group_sid),
				 LDAP_OBJ_POSIXGROUP,
				 LDAP_OBJ_GROUPMAP);

	/* get the group */
	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(1,("ldapsam_change_groupmem: group search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(tmp_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result == 0) {
		DEBUG(1,("ldapsam_change_groupmem: group not found!\n"));
		return NT_STATUS_NO_SUCH_GROUP;
	}

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_change_groupmem: More than one group with the same SID ?!\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (!entry) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* here it is, retrieve the dn for later use */
	dn = smbldap_talloc_dn(tmp_ctx, priv2ld(ldap_state), entry);
	if (!dn) {
		DEBUG(0,("ldapsam_change_groupmem: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	smbldap_set_mod(&mods, modop, "memberUid", uidstr);

	talloc_autofree_ldapmod(tmp_ctx, mods);

	rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);
	if (rc != LDAP_SUCCESS) {
		if (rc == LDAP_TYPE_OR_VALUE_EXISTS && modop == LDAP_MOD_ADD) {
			DEBUG(1,("ldapsam_change_groupmem: member is already in group, add failed!\n"));
			return NT_STATUS_MEMBER_IN_GROUP;
		}
		if (rc == LDAP_NO_SUCH_ATTRIBUTE && modop == LDAP_MOD_DELETE) {
			DEBUG(1,("ldapsam_change_groupmem: member is not in group, delete failed!\n"));
			return NT_STATUS_MEMBER_NOT_IN_GROUP;
		}
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ldapsam_add_groupmem(struct pdb_methods *my_methods,
				     TALLOC_CTX *tmp_ctx,
				     uint32_t group_rid,
				     uint32_t member_rid)
{
	return ldapsam_change_groupmem(my_methods, tmp_ctx, group_rid, member_rid, LDAP_MOD_ADD);
}
static NTSTATUS ldapsam_del_groupmem(struct pdb_methods *my_methods,
				     TALLOC_CTX *tmp_ctx,
				     uint32_t group_rid,
				     uint32_t member_rid)
{
	return ldapsam_change_groupmem(my_methods, tmp_ctx, group_rid, member_rid, LDAP_MOD_DELETE);
}

static NTSTATUS ldapsam_set_primary_group(struct pdb_methods *my_methods,
					  TALLOC_CTX *mem_ctx,
					  struct samu *sampass)
{
	struct ldapsam_privates *ldap_state = (struct ldapsam_privates *)my_methods->private_data;
	LDAPMessage *entry = NULL;
	LDAPMessage *result = NULL;
	uint32_t num_result;
	LDAPMod **mods = NULL;
	char *filter;
	char *escape_username;
	char *gidstr;
	const char *dn = NULL;
	gid_t gid;
	int rc;

	DEBUG(0,("ldapsam_set_primary_group: Attempt to set primary group for user [%s]\n", pdb_get_username(sampass)));

	if (!sid_to_gid(pdb_get_group_sid(sampass), &gid)) {
		DEBUG(0,("ldapsam_set_primary_group: failed to retrieve gid from user's group SID!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	gidstr = talloc_asprintf(mem_ctx, "%u", (unsigned int)gid);
	if (!gidstr) {
		DEBUG(0,("ldapsam_set_primary_group: Out of Memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	escape_username = escape_ldap_string(talloc_tos(),
					     pdb_get_username(sampass));
	if (escape_username== NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	filter = talloc_asprintf(mem_ctx,
				 "(&(uid=%s)"
				 "(objectClass=%s)"
				 "(objectClass=%s))",
				 escape_username,
				 LDAP_OBJ_POSIXACCOUNT,
				 LDAP_OBJ_SAMBASAMACCOUNT);

	TALLOC_FREE(escape_username);

	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rc = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL, &result);
	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_set_primary_group: user search failed!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	talloc_autofree_ldapmsg(mem_ctx, result);

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result == 0) {
		DEBUG(0,("ldapsam_set_primary_group: user not found!\n"));
		return NT_STATUS_NO_SUCH_USER;
	}

	if (num_result > 1) {
		DEBUG (0, ("ldapsam_set_primary_group: More than one user with name [%s] ?!\n", pdb_get_username(sampass)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (!entry) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* retrieve the dn for later use */
	dn = smbldap_talloc_dn(mem_ctx, priv2ld(ldap_state), entry);
	if (!dn) {
		DEBUG(0,("ldapsam_set_primary_group: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* remove the old one, and add the new one, this way we do not risk races */
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods, "gidNumber", gidstr);

	if (mods == NULL) {
		return NT_STATUS_OK;
	}

	rc = smbldap_modify(ldap_state->smbldap_state, dn, mods);

	if (rc != LDAP_SUCCESS) {
		DEBUG(0,("ldapsam_set_primary_group: failed to modify [%s] primary group to [%s]\n",
			 pdb_get_username(sampass), gidstr));
		return NT_STATUS_UNSUCCESSFUL;
	}

	flush_pwnam_cache();

	return NT_STATUS_OK;
}


/**********************************************************************
 trusted domains functions
 *********************************************************************/

static char *trusteddom_dn(struct ldapsam_privates *ldap_state,
			   const char *domain)
{
	return talloc_asprintf(talloc_tos(), "sambaDomainName=%s,%s", domain,
			       ldap_state->domain_dn);
}

static bool get_trusteddom_pw_int(struct ldapsam_privates *ldap_state,
				  TALLOC_CTX *mem_ctx,
				  const char *domain, LDAPMessage **entry)
{
	int rc;
	char *filter;
	int scope = LDAP_SCOPE_SUBTREE;
	const char **attrs = NULL; /* NULL: get all attrs */
	int attrsonly = 0; /* 0: return values too */
	LDAPMessage *result = NULL;
	char *trusted_dn;
	uint32_t num_result;

	filter = talloc_asprintf(talloc_tos(),
				 "(&(objectClass=%s)(sambaDomainName=%s))",
				 LDAP_OBJ_TRUSTDOM_PASSWORD, domain);

	trusted_dn = trusteddom_dn(ldap_state, domain);
	if (trusted_dn == NULL) {
		return False;
	}
	rc = smbldap_search(ldap_state->smbldap_state, trusted_dn, scope,
			    filter, attrs, attrsonly, &result);

	if (result != NULL) {
		talloc_autofree_ldapmsg(mem_ctx, result);
	}

	if (rc == LDAP_NO_SUCH_OBJECT) {
		*entry = NULL;
		return True;
	}

	if (rc != LDAP_SUCCESS) {
		return False;
	}

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result > 1) {
		DEBUG(1, ("ldapsam_get_trusteddom_pw: more than one "
			  "%s object for domain '%s'?!\n",
			  LDAP_OBJ_TRUSTDOM_PASSWORD, domain));
		return False;
	}

	if (num_result == 0) {
		DEBUG(1, ("ldapsam_get_trusteddom_pw: no "
			  "%s object for domain %s.\n",
			  LDAP_OBJ_TRUSTDOM_PASSWORD, domain));
		*entry = NULL;
	} else {
		*entry = ldap_first_entry(priv2ld(ldap_state), result);
	}

	return True;
}

static bool ldapsam_get_trusteddom_pw(struct pdb_methods *methods,
				      const char *domain,
				      char** pwd,
				      struct dom_sid *sid,
	        	 	      time_t *pass_last_set_time)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;

	DEBUG(10, ("ldapsam_get_trusteddom_pw called for domain %s\n", domain));

	if (!get_trusteddom_pw_int(ldap_state, talloc_tos(), domain, &entry) ||
	    (entry == NULL))
	{
		return False;
	}

	/* password */
	if (pwd != NULL) {
		char *pwd_str;
		pwd_str = smbldap_talloc_single_attribute(priv2ld(ldap_state),
				entry, "sambaClearTextPassword", talloc_tos());
		if (pwd_str == NULL) {
			return False;
		}
		/* trusteddom_pw routines do not use talloc yet... */
		*pwd = SMB_STRDUP(pwd_str);
		if (*pwd == NULL) {
			return False;
		}
	}

	/* last change time */
	if (pass_last_set_time != NULL) {
		char *time_str;
		time_str = smbldap_talloc_single_attribute(priv2ld(ldap_state),
				entry, "sambaPwdLastSet", talloc_tos());
		if (time_str == NULL) {
			return False;
		}
		*pass_last_set_time = (time_t)atol(time_str);
	}

	/* domain sid */
	if (sid != NULL) {
		char *sid_str;
		struct dom_sid dom_sid;
		sid_str = smbldap_talloc_single_attribute(priv2ld(ldap_state),
							  entry, "sambaSID",
							  talloc_tos());
		if (sid_str == NULL) {
			return False;
		}
		if (!string_to_sid(&dom_sid, sid_str)) {
			return False;
		}
		sid_copy(sid, &dom_sid);
	}

	return True;
}

static bool ldapsam_set_trusteddom_pw(struct pdb_methods *methods,
				      const char* domain,
				      const char* pwd,
				      const struct dom_sid *sid)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;
	LDAPMod **mods = NULL;
	char *prev_pwd = NULL;
	char *trusted_dn = NULL;
	int rc;

	DEBUG(10, ("ldapsam_set_trusteddom_pw called for domain %s\n", domain));

	/*
	 * get the current entry (if there is one) in order to put the
	 * current password into the previous password attribute
	 */
	if (!get_trusteddom_pw_int(ldap_state, talloc_tos(), domain, &entry)) {
		return False;
	}

	mods = NULL;
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods, "objectClass",
			 LDAP_OBJ_TRUSTDOM_PASSWORD);
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods, "sambaDomainName",
			 domain);
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods, "sambaSID",
			 sid_string_tos(sid));
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods, "sambaPwdLastSet",
			 talloc_asprintf(talloc_tos(), "%li", (long int)time(NULL)));
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods,
			 "sambaClearTextPassword", pwd);

	if (entry != NULL) {
		prev_pwd = smbldap_talloc_single_attribute(priv2ld(ldap_state),
				entry, "sambaClearTextPassword", talloc_tos());
		if (prev_pwd != NULL) {
			smbldap_make_mod(priv2ld(ldap_state), entry, &mods,
					 "sambaPreviousClearTextPassword",
					 prev_pwd);
		}
	}

	talloc_autofree_ldapmod(talloc_tos(), mods);

	trusted_dn = trusteddom_dn(ldap_state, domain);
	if (trusted_dn == NULL) {
		return False;
	}
	if (entry == NULL) {
		rc = smbldap_add(ldap_state->smbldap_state, trusted_dn, mods);
	} else {
		rc = smbldap_modify(ldap_state->smbldap_state, trusted_dn, mods);
	}

	if (rc != LDAP_SUCCESS) {
		DEBUG(1, ("error writing trusted domain password!\n"));
		return False;
	}

	return True;
}

static bool ldapsam_del_trusteddom_pw(struct pdb_methods *methods,
				      const char *domain)
{
	int rc;
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;
	const char *trusted_dn;

	if (!get_trusteddom_pw_int(ldap_state, talloc_tos(), domain, &entry)) {
		return False;
	}

	if (entry == NULL) {
		DEBUG(5, ("ldapsam_del_trusteddom_pw: no such trusted domain: "
			  "%s\n", domain));
		return True;
	}

	trusted_dn = smbldap_talloc_dn(talloc_tos(), priv2ld(ldap_state),
				       entry);
	if (trusted_dn == NULL) {
		DEBUG(0,("ldapsam_del_trusteddom_pw: Out of memory!\n"));
		return False;
	}

	rc = smbldap_delete(ldap_state->smbldap_state, trusted_dn);
	if (rc != LDAP_SUCCESS) {
		return False;
	}

	return True;
}

static NTSTATUS ldapsam_enum_trusteddoms(struct pdb_methods *methods,
					 TALLOC_CTX *mem_ctx,
					 uint32_t *num_domains,
					 struct trustdom_info ***domains)
{
	int rc;
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	char *filter;
	int scope = LDAP_SCOPE_SUBTREE;
	const char *attrs[] = { "sambaDomainName", "sambaSID", NULL };
	int attrsonly = 0; /* 0: return values too */
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;

	filter = talloc_asprintf(talloc_tos(), "(objectClass=%s)",
				 LDAP_OBJ_TRUSTDOM_PASSWORD);

	rc = smbldap_search(ldap_state->smbldap_state,
			    ldap_state->domain_dn,
			    scope,
			    filter,
			    attrs,
			    attrsonly,
			    &result);

	if (result != NULL) {
		talloc_autofree_ldapmsg(mem_ctx, result);
	}

	if (rc != LDAP_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	*num_domains = 0;
	if (!(*domains = TALLOC_ARRAY(mem_ctx, struct trustdom_info *, 1))) {
		DEBUG(1, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	for (entry = ldap_first_entry(priv2ld(ldap_state), result);
	     entry != NULL;
	     entry = ldap_next_entry(priv2ld(ldap_state), entry))
	{
		char *dom_name, *dom_sid_str;
		struct trustdom_info *dom_info;

		dom_info = TALLOC_P(*domains, struct trustdom_info);
		if (dom_info == NULL) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}

		dom_name = smbldap_talloc_single_attribute(priv2ld(ldap_state),
							   entry,
							   "sambaDomainName",
							   talloc_tos());
		if (dom_name == NULL) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}
		dom_info->name = dom_name;

		dom_sid_str = smbldap_talloc_single_attribute(
					priv2ld(ldap_state), entry, "sambaSID",
					talloc_tos());
		if (dom_sid_str == NULL) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}
		if (!string_to_sid(&dom_info->sid, dom_sid_str)) {
			DEBUG(1, ("Error calling string_to_sid on SID %s\n",
				  dom_sid_str));
			return NT_STATUS_UNSUCCESSFUL;
		}

		ADD_TO_ARRAY(*domains, struct trustdom_info *, dom_info,
			     domains, num_domains);

		if (*domains == NULL) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}
	}

	DEBUG(5, ("ldapsam_enum_trusteddoms: got %d domains\n", *num_domains));
	return NT_STATUS_OK;
}


/**********************************************************************
 Housekeeping
 *********************************************************************/

static void free_private_data(void **vp) 
{
	struct ldapsam_privates **ldap_state = (struct ldapsam_privates **)vp;

	smbldap_free_struct(&(*ldap_state)->smbldap_state);

	if ((*ldap_state)->result != NULL) {
		ldap_msgfree((*ldap_state)->result);
		(*ldap_state)->result = NULL;
	}
	if ((*ldap_state)->domain_dn != NULL) {
		SAFE_FREE((*ldap_state)->domain_dn);
	}

	*ldap_state = NULL;

	/* No need to free any further, as it is talloc()ed */
}

/*********************************************************************
 Intitalise the parts of the pdb_methods structure that are common to 
 all pdb_ldap modes
*********************************************************************/

static NTSTATUS pdb_init_ldapsam_common(struct pdb_methods **pdb_method, const char *location)
{
	NTSTATUS nt_status;
	struct ldapsam_privates *ldap_state;

	if (!NT_STATUS_IS_OK(nt_status = make_pdb_method( pdb_method ))) {
		return nt_status;
	}

	(*pdb_method)->name = "ldapsam";

	(*pdb_method)->getsampwnam = ldapsam_getsampwnam;
	(*pdb_method)->getsampwsid = ldapsam_getsampwsid;
	(*pdb_method)->add_sam_account = ldapsam_add_sam_account;
	(*pdb_method)->update_sam_account = ldapsam_update_sam_account;
	(*pdb_method)->delete_sam_account = ldapsam_delete_sam_account;
	(*pdb_method)->rename_sam_account = ldapsam_rename_sam_account;

	(*pdb_method)->getgrsid = ldapsam_getgrsid;
	(*pdb_method)->getgrgid = ldapsam_getgrgid;
	(*pdb_method)->getgrnam = ldapsam_getgrnam;
	(*pdb_method)->add_group_mapping_entry = ldapsam_add_group_mapping_entry;
	(*pdb_method)->update_group_mapping_entry = ldapsam_update_group_mapping_entry;
	(*pdb_method)->delete_group_mapping_entry = ldapsam_delete_group_mapping_entry;
	(*pdb_method)->enum_group_mapping = ldapsam_enum_group_mapping;

	(*pdb_method)->get_account_policy = ldapsam_get_account_policy;
	(*pdb_method)->set_account_policy = ldapsam_set_account_policy;

	(*pdb_method)->get_seq_num = ldapsam_get_seq_num;

	(*pdb_method)->capabilities = ldapsam_capabilities;
	(*pdb_method)->new_rid = ldapsam_new_rid;

	(*pdb_method)->get_trusteddom_pw = ldapsam_get_trusteddom_pw;
	(*pdb_method)->set_trusteddom_pw = ldapsam_set_trusteddom_pw;
	(*pdb_method)->del_trusteddom_pw = ldapsam_del_trusteddom_pw;
	(*pdb_method)->enum_trusteddoms = ldapsam_enum_trusteddoms;

	/* TODO: Setup private data and free */

	if ( !(ldap_state = TALLOC_ZERO_P(*pdb_method, struct ldapsam_privates)) ) {
		DEBUG(0, ("pdb_init_ldapsam_common: talloc() failed for ldapsam private_data!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = smbldap_init(*pdb_method, pdb_get_event_context(),
				 location, &ldap_state->smbldap_state);

	if ( !NT_STATUS_IS_OK(nt_status) ) {
		return nt_status;
	}

	if ( !(ldap_state->domain_name = talloc_strdup(*pdb_method, get_global_sam_name()) ) ) {
		return NT_STATUS_NO_MEMORY;
	}

	(*pdb_method)->private_data = ldap_state;

	(*pdb_method)->free_private_data = free_private_data;

	return NT_STATUS_OK;
}

/**********************************************************************
 Initialise the 'compat' mode for pdb_ldap
 *********************************************************************/

NTSTATUS pdb_init_ldapsam_compat(struct pdb_methods **pdb_method, const char *location)
{
	NTSTATUS nt_status;
	struct ldapsam_privates *ldap_state;
	char *uri = talloc_strdup( NULL, location );

	trim_char( uri, '\"', '\"' );
	nt_status = pdb_init_ldapsam_common( pdb_method, uri );
	if ( uri )
		TALLOC_FREE( uri );

	if ( !NT_STATUS_IS_OK(nt_status) ) {
		return nt_status;
	}

	(*pdb_method)->name = "ldapsam_compat";

	ldap_state = (struct ldapsam_privates *)((*pdb_method)->private_data);
	ldap_state->schema_ver = SCHEMAVER_SAMBAACCOUNT;

	sid_copy(&ldap_state->domain_sid, get_global_sam_sid());

	return NT_STATUS_OK;
}

/**********************************************************************
 Initialise the normal mode for pdb_ldap
 *********************************************************************/

NTSTATUS pdb_init_ldapsam(struct pdb_methods **pdb_method, const char *location)
{
	NTSTATUS nt_status;
	struct ldapsam_privates *ldap_state = NULL;
	uint32_t alg_rid_base;
	char *alg_rid_base_string = NULL;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	struct dom_sid ldap_domain_sid;
	struct dom_sid secrets_domain_sid;
	char *domain_sid_string = NULL;
	char *dn = NULL;
	char *uri = talloc_strdup( NULL, location );

	trim_char( uri, '\"', '\"' );
	nt_status = pdb_init_ldapsam_common(pdb_method, uri);

	TALLOC_FREE(uri);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	(*pdb_method)->name = "ldapsam";

	(*pdb_method)->add_aliasmem = ldapsam_add_aliasmem;
	(*pdb_method)->del_aliasmem = ldapsam_del_aliasmem;
	(*pdb_method)->enum_aliasmem = ldapsam_enum_aliasmem;
	(*pdb_method)->enum_alias_memberships = ldapsam_alias_memberships;
	(*pdb_method)->search_users = ldapsam_search_users;
	(*pdb_method)->search_groups = ldapsam_search_groups;
	(*pdb_method)->search_aliases = ldapsam_search_aliases;

	if (lp_parm_bool(-1, "ldapsam", "trusted", False)) {
		(*pdb_method)->enum_group_members = ldapsam_enum_group_members;
		(*pdb_method)->enum_group_memberships =
			ldapsam_enum_group_memberships;
		(*pdb_method)->lookup_rids = ldapsam_lookup_rids;
		(*pdb_method)->sid_to_id = ldapsam_sid_to_id;
		(*pdb_method)->uid_to_sid = ldapsam_uid_to_sid;
		(*pdb_method)->gid_to_sid = ldapsam_gid_to_sid;

		if (lp_parm_bool(-1, "ldapsam", "editposix", False)) {
			(*pdb_method)->create_user = ldapsam_create_user;
			(*pdb_method)->delete_user = ldapsam_delete_user;
			(*pdb_method)->create_dom_group = ldapsam_create_dom_group;
			(*pdb_method)->delete_dom_group = ldapsam_delete_dom_group;
			(*pdb_method)->add_groupmem = ldapsam_add_groupmem;
			(*pdb_method)->del_groupmem = ldapsam_del_groupmem;
			(*pdb_method)->set_unix_primary_group = ldapsam_set_primary_group;
		}
	}

	ldap_state = (struct ldapsam_privates *)((*pdb_method)->private_data);
	ldap_state->schema_ver = SCHEMAVER_SAMBASAMACCOUNT;

	/* Try to setup the Domain Name, Domain SID, algorithmic rid base */

	nt_status = smbldap_search_domain_info(ldap_state->smbldap_state,
					       &result,
					       ldap_state->domain_name, True);

	if ( !NT_STATUS_IS_OK(nt_status) ) {
		DEBUG(2, ("pdb_init_ldapsam: WARNING: Could not get domain "
			  "info, nor add one to the domain\n"));
		DEBUGADD(2, ("pdb_init_ldapsam: Continuing on regardless, "
			     "will be unable to allocate new users/groups, "
			     "and will risk BDCs having inconsistent SIDs\n"));
		sid_copy(&ldap_state->domain_sid, get_global_sam_sid());
		return NT_STATUS_OK;
	}

	/* Given that the above might fail, everything below this must be
	 * optional */

	entry = ldap_first_entry(ldap_state->smbldap_state->ldap_struct,
				 result);
	if (!entry) {
		DEBUG(0, ("pdb_init_ldapsam: Could not get domain info "
			  "entry\n"));
		ldap_msgfree(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	dn = smbldap_talloc_dn(talloc_tos(), ldap_state->smbldap_state->ldap_struct, entry);
	if (!dn) {
		ldap_msgfree(result);
		return NT_STATUS_UNSUCCESSFUL;
	}

	ldap_state->domain_dn = smb_xstrdup(dn);
	TALLOC_FREE(dn);

	domain_sid_string = smbldap_talloc_single_attribute(
		    ldap_state->smbldap_state->ldap_struct,
		    entry,
		    get_userattr_key2string(ldap_state->schema_ver,
					    LDAP_ATTR_USER_SID),
		    talloc_tos());

	if (domain_sid_string) {
		bool found_sid;
		if (!string_to_sid(&ldap_domain_sid, domain_sid_string)) {
			DEBUG(1, ("pdb_init_ldapsam: SID [%s] could not be "
				  "read as a valid SID\n", domain_sid_string));
			ldap_msgfree(result);
			TALLOC_FREE(domain_sid_string);
			return NT_STATUS_INVALID_PARAMETER;
		}
		found_sid = secrets_fetch_domain_sid(ldap_state->domain_name,
						     &secrets_domain_sid);
		if (!found_sid || !dom_sid_equal(&secrets_domain_sid,
					     &ldap_domain_sid)) {
			DEBUG(1, ("pdb_init_ldapsam: Resetting SID for domain "
				  "%s based on pdb_ldap results %s -> %s\n",
				  ldap_state->domain_name,
				  sid_string_dbg(&secrets_domain_sid),
				  sid_string_dbg(&ldap_domain_sid)));

			/* reset secrets.tdb sid */
			secrets_store_domain_sid(ldap_state->domain_name,
						 &ldap_domain_sid);
			DEBUG(1, ("New global sam SID: %s\n",
				  sid_string_dbg(get_global_sam_sid())));
		}
		sid_copy(&ldap_state->domain_sid, &ldap_domain_sid);
		TALLOC_FREE(domain_sid_string);
	}

	alg_rid_base_string = smbldap_talloc_single_attribute(
		    ldap_state->smbldap_state->ldap_struct,
		    entry,
		    get_attr_key2string( dominfo_attr_list,
					 LDAP_ATTR_ALGORITHMIC_RID_BASE ),
		    talloc_tos());
	if (alg_rid_base_string) {
		alg_rid_base = (uint32_t)atol(alg_rid_base_string);
		if (alg_rid_base != algorithmic_rid_base()) {
			DEBUG(0, ("The value of 'algorithmic RID base' has "
				  "changed since the LDAP\n"
				  "database was initialised.  Aborting. \n"));
			ldap_msgfree(result);
			TALLOC_FREE(alg_rid_base_string);
			return NT_STATUS_UNSUCCESSFUL;
		}
		TALLOC_FREE(alg_rid_base_string);
	}
	ldap_msgfree(result);

	return NT_STATUS_OK;
}

NTSTATUS pdb_ldap_init(void)
{
	NTSTATUS nt_status;
	if (!NT_STATUS_IS_OK(nt_status = smb_register_passdb(PASSDB_INTERFACE_VERSION, "ldapsam", pdb_init_ldapsam)))
		return nt_status;

	if (!NT_STATUS_IS_OK(nt_status = smb_register_passdb(PASSDB_INTERFACE_VERSION, "ldapsam_compat", pdb_init_ldapsam_compat)))
		return nt_status;

	/* Let pdb_nds register backends */
	pdb_nds_init();

	pdb_ipa_init();

	return NT_STATUS_OK;
}
