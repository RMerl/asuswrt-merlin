/*
   Unix SMB/CIFS implementation.
   pdb_ldap with ads schema
   Copyright (C) Volker Lendecke 2009

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
#include "passdb.h"
#include "tldap.h"
#include "tldap_util.h"
#include "../libds/common/flags.h"
#include "secrets.h"
#include "../librpc/gen_ndr/samr.h"
#include "../libcli/ldap/ldap_ndr.h"
#include "../libcli/security/security.h"
#include "../libds/common/flag_mapping.h"

struct pdb_ads_state {
	struct sockaddr_un socket_address;
	struct tldap_context *ld;
	struct dom_sid domainsid;
	struct GUID domainguid;
	char *domaindn;
	char *configdn;
	char *netbiosname;
};

struct pdb_ads_samu_private {
	char *dn;
	struct tldap_message *ldapmsg;
};

static bool pdb_ads_gid_to_sid(struct pdb_methods *m, gid_t gid,
			       struct dom_sid *sid);
static bool pdb_ads_dnblob2sid(struct pdb_ads_state *state, DATA_BLOB *dnblob,
			       struct dom_sid *psid);
static NTSTATUS pdb_ads_sid2dn(struct pdb_ads_state *state,
			       const struct dom_sid *sid,
			       TALLOC_CTX *mem_ctx, char **pdn);
static struct tldap_context *pdb_ads_ld(struct pdb_ads_state *state);
static int pdb_ads_search_fmt(struct pdb_ads_state *state, const char *base,
			      int scope, const char *attrs[], int num_attrs,
			      int attrsonly,
			      TALLOC_CTX *mem_ctx, struct tldap_message ***res,
			      const char *fmt, ...);
static NTSTATUS pdb_ads_getsamupriv(struct pdb_ads_state *state,
				    const char *filter,
				    TALLOC_CTX *mem_ctx,
				    struct pdb_ads_samu_private **presult);

static bool pdb_ads_pull_time(struct tldap_message *msg, const char *attr,
			      time_t *ptime)
{
	uint64_t tmp;

	if (!tldap_pull_uint64(msg, attr, &tmp)) {
		return false;
	}
	*ptime = nt_time_to_unix(tmp);
	return true;
}

static gid_t pdb_ads_sid2gid(const struct dom_sid *sid)
{
	uint32_t rid;
	sid_peek_rid(sid, &rid);
	return rid;
}

static char *pdb_ads_domaindn2dns(TALLOC_CTX *mem_ctx, char *dn)
{
	char *result, *p;

	result = talloc_string_sub2(mem_ctx, dn, "DC=", "", false, false,
				    true);
	if (result == NULL) {
		return NULL;
	}

	while ((p = strchr_m(result, ',')) != NULL) {
		*p = '.';
	}

	return result;
}

static struct pdb_domain_info *pdb_ads_get_domain_info(
	struct pdb_methods *m, TALLOC_CTX *mem_ctx)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct pdb_domain_info *info;
	struct tldap_message *rootdse;
	char *tmp;

	info = talloc(mem_ctx, struct pdb_domain_info);
	if (info == NULL) {
		return NULL;
	}
	info->name = talloc_strdup(info, state->netbiosname);
	if (info->name == NULL) {
		goto fail;
	}
	info->dns_domain = pdb_ads_domaindn2dns(info, state->domaindn);
	if (info->dns_domain == NULL) {
		goto fail;
	}

	rootdse = tldap_rootdse(state->ld);
	tmp = tldap_talloc_single_attribute(rootdse, "rootDomainNamingContext",
					    talloc_tos());
	if (tmp == NULL) {
		goto fail;
	}
	info->dns_forest = pdb_ads_domaindn2dns(info, tmp);
	TALLOC_FREE(tmp);
	if (info->dns_forest == NULL) {
		goto fail;
	}
	info->sid = state->domainsid;
	info->guid = state->domainguid;
	return info;

fail:
	TALLOC_FREE(info);
	return NULL;
}

static struct pdb_ads_samu_private *pdb_ads_get_samu_private(
	struct pdb_methods *m, struct samu *sam)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct pdb_ads_samu_private *result;
	char *sidstr, *filter;
	NTSTATUS status;

	result = (struct pdb_ads_samu_private *)
		pdb_get_backend_private_data(sam, m);

	if (result != NULL) {
		return talloc_get_type_abort(
			result, struct pdb_ads_samu_private);
	}

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), pdb_get_user_sid(sam));
	if (sidstr == NULL) {
		return NULL;
	}

	filter = talloc_asprintf(
		talloc_tos(), "(&(objectsid=%s)(objectclass=user))", sidstr);
	TALLOC_FREE(sidstr);
	if (filter == NULL) {
		return NULL;
	}

	status = pdb_ads_getsamupriv(state, filter, sam, &result);
	TALLOC_FREE(filter);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return result;
}

static NTSTATUS pdb_ads_init_sam_from_priv(struct pdb_methods *m,
					   struct samu *sam,
					   struct pdb_ads_samu_private *priv)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS status = NT_STATUS_INTERNAL_DB_CORRUPTION;
	struct tldap_message *entry = priv->ldapmsg;
	char *str;
	time_t tmp_time;
	struct dom_sid sid;
	uint64_t n;
	uint32_t i;
	DATA_BLOB blob;

	str = tldap_talloc_single_attribute(entry, "samAccountName", sam);
	if (str == NULL) {
		DEBUG(10, ("no samAccountName\n"));
		goto fail;
	}
	pdb_set_username(sam, str, PDB_SET);

	if (pdb_ads_pull_time(entry, "lastLogon", &tmp_time)) {
		pdb_set_logon_time(sam, tmp_time, PDB_SET);
	}
	if (pdb_ads_pull_time(entry, "lastLogoff", &tmp_time)) {
		pdb_set_logoff_time(sam, tmp_time, PDB_SET);
	}
	if (pdb_ads_pull_time(entry, "pwdLastSet", &tmp_time)) {
		pdb_set_pass_last_set_time(sam, tmp_time, PDB_SET);
	}
	if (pdb_ads_pull_time(entry, "accountExpires", &tmp_time)) {
		pdb_set_kickoff_time(sam, tmp_time, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "displayName",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_fullname(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "homeDirectory",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_homedir(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "homeDrive", talloc_tos());
	if (str != NULL) {
		pdb_set_dir_drive(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "scriptPath", talloc_tos());
	if (str != NULL) {
		pdb_set_logon_script(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "profilePath",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_profile_path(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "profilePath",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_profile_path(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "comment",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_comment(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "description",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_acct_desc(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "userWorkstations",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_workstations(sam, str, PDB_SET);
	}

	str = tldap_talloc_single_attribute(entry, "userParameters",
					    talloc_tos());
	if (str != NULL) {
		pdb_set_munged_dial(sam, str, PDB_SET);
	}

	if (!tldap_pull_binsid(entry, "objectSid", &sid)) {
		DEBUG(10, ("Could not pull SID\n"));
		goto fail;
	}
	pdb_set_user_sid(sam, &sid, PDB_SET);

	if (!tldap_pull_uint64(entry, "userAccountControl", &n)) {
		DEBUG(10, ("Could not pull userAccountControl\n"));
		goto fail;
	}
	pdb_set_acct_ctrl(sam, ds_uf2acb(n), PDB_SET);

	if (tldap_get_single_valueblob(entry, "unicodePwd", &blob)) {
		if (blob.length != NT_HASH_LEN) {
			DEBUG(0, ("Got NT hash of length %d, expected %d\n",
				  (int)blob.length, NT_HASH_LEN));
			goto fail;
		}
		pdb_set_nt_passwd(sam, blob.data, PDB_SET);
	}

	if (tldap_get_single_valueblob(entry, "dBCSPwd", &blob)) {
		if (blob.length != LM_HASH_LEN) {
			DEBUG(0, ("Got LM hash of length %d, expected %d\n",
				  (int)blob.length, LM_HASH_LEN));
			goto fail;
		}
		pdb_set_lanman_passwd(sam, blob.data, PDB_SET);
	}

	if (tldap_pull_uint64(entry, "primaryGroupID", &n)) {
		sid_compose(&sid, &state->domainsid, n);
		pdb_set_group_sid(sam, &sid, PDB_SET);

	}

	if (tldap_pull_uint32(entry, "countryCode", &i)) {
		pdb_set_country_code(sam, i, PDB_SET);
	}

	if (tldap_pull_uint32(entry, "codePage", &i)) {
		pdb_set_code_page(sam, i, PDB_SET);
	}

	if (tldap_get_single_valueblob(entry, "logonHours", &blob)) {

		if (blob.length > MAX_HOURS_LEN) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto fail;
		}
		pdb_set_logon_divs(sam, blob.length * 8, PDB_SET);
		pdb_set_hours_len(sam, blob.length, PDB_SET);
		pdb_set_hours(sam, blob.data, blob.length, PDB_SET);

	} else {
		uint8_t hours[21];
		pdb_set_logon_divs(sam, sizeof(hours)/8, PDB_SET);
		pdb_set_hours_len(sam, sizeof(hours), PDB_SET);
		memset(hours, 0xff, sizeof(hours));
		pdb_set_hours(sam, hours, sizeof(hours), PDB_SET);
	}

	status = NT_STATUS_OK;
fail:
	TALLOC_FREE(frame);
	return status;
}

static bool pdb_ads_make_time_mod(struct tldap_message *existing,
				  TALLOC_CTX *mem_ctx,
				  struct tldap_mod **pmods, int *pnum_mods,
				  const char *attrib, time_t t)
{
	uint64_t nt_time;

	unix_to_nt_time(&nt_time, t);

	return tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, attrib,
		"%llu", nt_time);
}

static bool pdb_ads_init_ads_from_sam(struct pdb_ads_state *state,
				      struct tldap_message *existing,
				      TALLOC_CTX *mem_ctx,
				      struct tldap_mod **pmods, int *pnum_mods,
				      struct samu *sam)
{
	bool ret = true;
	DATA_BLOB blob;
	const char *pw;

	/* TODO: All fields :-) */

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "displayName",
		"%s", pdb_get_fullname(sam));

	pw = pdb_get_plaintext_passwd(sam);

	/*
	 * If we have the plain text pw, this is probably about to be
	 * set. Is this true always?
	 */
	if (pw != NULL) {
		char *pw_quote;
		uint8_t *pw_utf16;
		size_t pw_utf16_len;

		pw_quote = talloc_asprintf(talloc_tos(), "\"%s\"", pw);
		if (pw_quote == NULL) {
			ret = false;
			goto fail;
		}

		ret &= convert_string_talloc(talloc_tos(),
					     CH_UNIX, CH_UTF16LE,
					     pw_quote, strlen(pw_quote),
					     &pw_utf16, &pw_utf16_len, false);
		if (!ret) {
			goto fail;
		}
		blob = data_blob_const(pw_utf16, pw_utf16_len);

		ret &= tldap_add_mod_blobs(mem_ctx, pmods, pnum_mods,
					   TLDAP_MOD_REPLACE,
					   "unicodePwd", &blob, 1);
		TALLOC_FREE(pw_utf16);
		TALLOC_FREE(pw_quote);
	}

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "userAccountControl",
		"%d", ds_acb2uf(pdb_get_acct_ctrl(sam)));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "homeDirectory",
		"%s", pdb_get_homedir(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "homeDrive",
		"%s", pdb_get_dir_drive(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "scriptPath",
		"%s", pdb_get_logon_script(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "profilePath",
		"%s", pdb_get_profile_path(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "comment",
		"%s", pdb_get_comment(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "description",
		"%s", pdb_get_acct_desc(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "userWorkstations",
		"%s", pdb_get_workstations(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "userParameters",
		"%s", pdb_get_munged_dial(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "countryCode",
		"%i", (int)pdb_get_country_code(sam));

	ret &= tldap_make_mod_fmt(
		existing, mem_ctx, pmods, pnum_mods, "codePage",
		"%i", (int)pdb_get_code_page(sam));

	ret &= pdb_ads_make_time_mod(
		existing, mem_ctx, pmods, pnum_mods, "accountExpires",
		(int)pdb_get_kickoff_time(sam));

	ret &= tldap_make_mod_blob(
		existing, mem_ctx, pmods, pnum_mods, "logonHours",
		data_blob_const(pdb_get_hours(sam), pdb_get_hours_len(sam)));

fail:
	return ret;
}

static NTSTATUS pdb_ads_getsamupriv(struct pdb_ads_state *state,
				    const char *filter,
				    TALLOC_CTX *mem_ctx,
				    struct pdb_ads_samu_private **presult)
{
	const char * attrs[] = {
		"lastLogon", "lastLogoff", "pwdLastSet", "accountExpires",
		"sAMAccountName", "displayName", "homeDirectory",
		"homeDrive", "scriptPath", "profilePath", "description",
		"userWorkstations", "comment", "userParameters", "objectSid",
		"primaryGroupID", "userAccountControl", "logonHours",
		"badPwdCount", "logonCount", "countryCode", "codePage",
		"unicodePwd", "dBCSPwd" };
	struct tldap_message **users;
	int rc, count;
	struct pdb_ads_samu_private *result;

	result = talloc(mem_ctx, struct pdb_ads_samu_private);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, result,
				&users, "%s", filter);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		TALLOC_FREE(result);
		return NT_STATUS_LDAP(rc);
	}

	count = talloc_array_length(users);
	if (count != 1) {
		DEBUG(10, ("Expected 1 user, got %d\n", count));
		TALLOC_FREE(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	result->ldapmsg = users[0];
	if (!tldap_entry_dn(result->ldapmsg, &result->dn)) {
		DEBUG(10, ("Could not extract dn\n"));
		TALLOC_FREE(result);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	*presult = result;
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_getsampwfilter(struct pdb_methods *m,
				       struct pdb_ads_state *state,
				       struct samu *sam_acct,
				       const char *filter)
{
	struct pdb_ads_samu_private *priv;
	NTSTATUS status;

	status = pdb_ads_getsamupriv(state, filter, sam_acct, &priv);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("pdb_ads_getsamupriv failed: %s\n",
			   nt_errstr(status)));
		return status;
	}

	status = pdb_ads_init_sam_from_priv(m, sam_acct, priv);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("pdb_ads_init_sam_from_priv failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(priv);
		return status;
	}

	pdb_set_backend_private_data(sam_acct, priv, NULL, m, PDB_SET);
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_getsampwnam(struct pdb_methods *m,
				    struct samu *sam_acct,
				    const char *username)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	char *filter;

	filter = talloc_asprintf(
		talloc_tos(), "(&(samaccountname=%s)(objectclass=user))",
		username);
	NT_STATUS_HAVE_NO_MEMORY(filter);

	return pdb_ads_getsampwfilter(m, state, sam_acct, filter);
}

static NTSTATUS pdb_ads_getsampwsid(struct pdb_methods *m,
				    struct samu *sam_acct,
				    const struct dom_sid *sid)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	char *sidstr, *filter;

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	filter = talloc_asprintf(
		talloc_tos(), "(&(objectsid=%s)(objectclass=user))", sidstr);
	TALLOC_FREE(sidstr);
	NT_STATUS_HAVE_NO_MEMORY(filter);

	return pdb_ads_getsampwfilter(m, state, sam_acct, filter);
}

static NTSTATUS pdb_ads_create_user(struct pdb_methods *m,
				    TALLOC_CTX *tmp_ctx,
				    const char *name, uint32 acct_flags,
				    uint32 *rid)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	const char *attrs[1] = { "objectSid" };
	struct tldap_mod *mods = NULL;
	int num_mods = 0;
	struct tldap_message **user;
	struct dom_sid sid;
	char *dn;
	int rc;
	bool ok;

	dn = talloc_asprintf(talloc_tos(), "cn=%s,cn=users,%s", name,
			     state->domaindn);
	if (dn == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	/* TODO: Create machines etc */

	ok = true;
	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "objectClass", "user");
	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "samAccountName", "%s",
		name);
	if (!ok) {
		return NT_STATUS_NO_MEMORY;
	}


	rc = tldap_add(ld, dn, mods, num_mods, NULL, 0, NULL, 0);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_add failed %s\n",
			   tldap_errstr(talloc_tos(), ld, rc)));
		TALLOC_FREE(dn);
		return NT_STATUS_LDAP(rc);
	}

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&user,
				"(&(objectclass=user)(samaccountname=%s))",
				name);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not find just created user %s: %s\n",
			   name, tldap_errstr(talloc_tos(), state->ld, rc)));
		TALLOC_FREE(dn);
		return NT_STATUS_LDAP(rc);
	}

	if (talloc_array_length(user) != 1) {
		DEBUG(10, ("Got %d users, expected one\n",
			   (int)talloc_array_length(user)));
		TALLOC_FREE(dn);
		return NT_STATUS_LDAP(rc);
	}

	if (!tldap_pull_binsid(user[0], "objectSid", &sid)) {
		DEBUG(10, ("Could not fetch objectSid from user %s\n",
			   name));
		TALLOC_FREE(dn);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	sid_peek_rid(&sid, rid);
	TALLOC_FREE(dn);
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_delete_user(struct pdb_methods *m,
				    TALLOC_CTX *tmp_ctx,
				    struct samu *sam)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	NTSTATUS status;
	struct tldap_context *ld;
	char *dn;
	int rc;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	status = pdb_ads_sid2dn(state, pdb_get_user_sid(sam), talloc_tos(),
				&dn);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	rc = tldap_delete(ld, dn, NULL, 0, NULL, 0);
	TALLOC_FREE(dn);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_delete for %s failed: %s\n", dn,
			   tldap_errstr(talloc_tos(), ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_add_sam_account(struct pdb_methods *m,
					struct samu *sampass)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_update_sam_account(struct pdb_methods *m,
					   struct samu *sam)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct pdb_ads_samu_private *priv = pdb_ads_get_samu_private(m, sam);
	struct tldap_context *ld;
	struct tldap_mod *mods = NULL;
	int rc, num_mods = 0;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	if (!pdb_ads_init_ads_from_sam(state, priv->ldapmsg, talloc_tos(),
				       &mods, &num_mods, sam)) {
		return NT_STATUS_NO_MEMORY;
	}

	if (num_mods == 0) {
		/* Nothing to do, just return success */
		return NT_STATUS_OK;
	}

	rc = tldap_modify(ld, priv->dn, mods, num_mods, NULL, 0,
			  NULL, 0);
	TALLOC_FREE(mods);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_modify for %s failed: %s\n", priv->dn,
			   tldap_errstr(talloc_tos(), ld, rc)));
		return NT_STATUS_LDAP(rc);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_delete_sam_account(struct pdb_methods *m,
					   struct samu *username)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_rename_sam_account(struct pdb_methods *m,
					   struct samu *oldname,
					   const char *newname)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_update_login_attempts(struct pdb_methods *m,
					      struct samu *sam_acct,
					      bool success)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_getgrfilter(struct pdb_methods *m, GROUP_MAP *map,
				    const char *filter,
				    TALLOC_CTX *mem_ctx,
				    struct tldap_message **pmsg)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	const char *attrs[4] = { "objectSid", "description", "samAccountName",
				 "groupType" };
	char *str;
	struct tldap_message **group;
	uint32_t grouptype;
	int rc;

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&group, "%s", filter);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	if (talloc_array_length(group) != 1) {
		DEBUG(10, ("Expected 1 group, got %d\n",
			   (int)talloc_array_length(group)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (!tldap_pull_binsid(group[0], "objectSid", &map->sid)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	map->gid = pdb_ads_sid2gid(&map->sid);

	if (!tldap_pull_uint32(group[0], "groupType", &grouptype)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	switch (grouptype) {
	case GTYPE_SECURITY_BUILTIN_LOCAL_GROUP:
	case GTYPE_SECURITY_DOMAIN_LOCAL_GROUP:
		map->sid_name_use = SID_NAME_ALIAS;
		break;
	case GTYPE_SECURITY_GLOBAL_GROUP:
		map->sid_name_use = SID_NAME_DOM_GRP;
		break;
	default:
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	str = tldap_talloc_single_attribute(group[0], "samAccountName",
					    talloc_tos());
	if (str == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	fstrcpy(map->nt_name, str);
	TALLOC_FREE(str);

	str = tldap_talloc_single_attribute(group[0], "description",
					    talloc_tos());
	if (str != NULL) {
		fstrcpy(map->comment, str);
		TALLOC_FREE(str);
	} else {
		map->comment[0] = '\0';
	}

	if (pmsg != NULL) {
		*pmsg = talloc_move(mem_ctx, &group[0]);
	}
	TALLOC_FREE(group);
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_getgrsid(struct pdb_methods *m, GROUP_MAP *map,
				 struct dom_sid sid)
{
	char *filter;
	NTSTATUS status;

	filter = talloc_asprintf(talloc_tos(),
				 "(&(objectsid=%s)(objectclass=group))",
				 sid_string_talloc(talloc_tos(), &sid));
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = pdb_ads_getgrfilter(m, map, filter, NULL, NULL);
	TALLOC_FREE(filter);
	return status;
}

static NTSTATUS pdb_ads_getgrgid(struct pdb_methods *m, GROUP_MAP *map,
				 gid_t gid)
{
	struct dom_sid sid;
	pdb_ads_gid_to_sid(m, gid, &sid);
	return pdb_ads_getgrsid(m, map, sid);
}

static NTSTATUS pdb_ads_getgrnam(struct pdb_methods *m, GROUP_MAP *map,
				 const char *name)
{
	char *filter;
	NTSTATUS status;

	filter = talloc_asprintf(talloc_tos(),
				 "(&(samaccountname=%s)(objectclass=group))",
				 name);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = pdb_ads_getgrfilter(m, map, filter, NULL, NULL);
	TALLOC_FREE(filter);
	return status;
}

static NTSTATUS pdb_ads_create_dom_group(struct pdb_methods *m,
					 TALLOC_CTX *mem_ctx, const char *name,
					 uint32 *rid)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	const char *attrs[1] = { "objectSid" };
	int num_mods = 0;
	struct tldap_mod *mods = NULL;
	struct tldap_message **alias;
	struct dom_sid sid;
	char *dn;
	int rc;
	bool ok = true;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	dn = talloc_asprintf(talloc_tos(), "cn=%s,cn=users,%s", name,
			     state->domaindn);
	if (dn == NULL) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "samAccountName", "%s",
		name);
	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "objectClass", "group");
	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "groupType",
		"%d", (int)GTYPE_SECURITY_GLOBAL_GROUP);

	if (!ok) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	rc = tldap_add(ld, dn, mods, num_mods, NULL, 0, NULL, 0);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_add failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		TALLOC_FREE(frame);
		return NT_STATUS_LDAP(rc);
	}

	rc = pdb_ads_search_fmt(
		state, state->domaindn, TLDAP_SCOPE_SUB,
		attrs, ARRAY_SIZE(attrs), 0, talloc_tos(), &alias,
		"(&(objectclass=group)(samaccountname=%s))", name);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not find just created alias %s: %s\n",
			   name, tldap_errstr(talloc_tos(), state->ld, rc)));
		TALLOC_FREE(frame);
		return NT_STATUS_LDAP(rc);
	}

	if (talloc_array_length(alias) != 1) {
		DEBUG(10, ("Got %d alias, expected one\n",
			   (int)talloc_array_length(alias)));
		TALLOC_FREE(frame);
		return NT_STATUS_LDAP(rc);
	}

	if (!tldap_pull_binsid(alias[0], "objectSid", &sid)) {
		DEBUG(10, ("Could not fetch objectSid from alias %s\n",
			   name));
		TALLOC_FREE(frame);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	sid_peek_rid(&sid, rid);
	TALLOC_FREE(frame);
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_delete_dom_group(struct pdb_methods *m,
					 TALLOC_CTX *mem_ctx, uint32 rid)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	struct dom_sid sid;
	char *sidstr;
	struct tldap_message **msg;
	char *dn;
	int rc;

	sid_compose(&sid, &state->domainsid, rid);

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), &sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				NULL, 0, 0, talloc_tos(), &msg,
				("(&(objectSid=%s)(objectClass=group))"),
				sidstr);
	TALLOC_FREE(sidstr);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}

	switch talloc_array_length(msg) {
	case 0:
		return NT_STATUS_NO_SUCH_GROUP;
	case 1:
		break;
	default:
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (!tldap_entry_dn(msg[0], &dn)) {
		TALLOC_FREE(msg);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		TALLOC_FREE(msg);
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	rc = tldap_delete(ld, dn, NULL, 0, NULL, 0);
	TALLOC_FREE(msg);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_delete failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_add_group_mapping_entry(struct pdb_methods *m,
						GROUP_MAP *map)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_update_group_mapping_entry(struct pdb_methods *m,
						   GROUP_MAP *map)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	struct tldap_mod *mods = NULL;
	char *filter;
	struct tldap_message *existing;
	char *dn;
	GROUP_MAP existing_map;
	int rc, num_mods = 0;
	bool ret;
	NTSTATUS status;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	filter = talloc_asprintf(talloc_tos(),
				 "(&(objectsid=%s)(objectclass=group))",
				 sid_string_talloc(talloc_tos(), &map->sid));
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	status = pdb_ads_getgrfilter(m, &existing_map, filter,
				     talloc_tos(), &existing);
	TALLOC_FREE(filter);

	if (!tldap_entry_dn(existing, &dn)) {
		return NT_STATUS_LDAP(TLDAP_DECODING_ERROR);
	}

	ret = true;

	ret &= tldap_make_mod_fmt(
		existing, talloc_tos(), &mods, &num_mods, "description",
		"%s", map->comment);
	ret &= tldap_make_mod_fmt(
		existing, talloc_tos(), &mods, &num_mods, "samaccountname",
		"%s", map->nt_name);

	if (!ret) {
		return NT_STATUS_NO_MEMORY;
	}

	if (num_mods == 0) {
		TALLOC_FREE(existing);
		return NT_STATUS_OK;
	}

	rc = tldap_modify(ld, dn, mods, num_mods, NULL, 0, NULL, 0);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_modify for %s failed: %s\n", dn,
			   tldap_errstr(talloc_tos(), ld, rc)));
		TALLOC_FREE(existing);
		return NT_STATUS_LDAP(rc);
	}
	TALLOC_FREE(existing);
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_delete_group_mapping_entry(struct pdb_methods *m,
						   struct dom_sid sid)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_enum_group_mapping(struct pdb_methods *m,
					   const struct dom_sid *sid,
					   enum lsa_SidType sid_name_use,
					   GROUP_MAP **pp_rmap,
					   size_t *p_num_entries,
					   bool unix_only)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_enum_group_members(struct pdb_methods *m,
					   TALLOC_CTX *mem_ctx,
					   const struct dom_sid *group,
					   uint32 **pmembers,
					   size_t *pnum_members)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	const char *attrs[1] = { "member" };
	char *sidstr;
	struct tldap_message **msg;
	int i, rc, num_members;
	DATA_BLOB *blobs;
	uint32_t *members;

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), group);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&msg, "(objectsid=%s)", sidstr);
	TALLOC_FREE(sidstr);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	switch talloc_array_length(msg) {
	case 0:
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		break;
	case 1:
		break;
	default:
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
		break;
	}

	if (!tldap_entry_values(msg[0], "member", &blobs, &num_members)) {
		*pmembers = NULL;
		*pnum_members = 0;
		return NT_STATUS_OK;
	}

	members = talloc_array(mem_ctx, uint32_t, num_members);
	if (members == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_members; i++) {
		struct dom_sid sid;
		if (!pdb_ads_dnblob2sid(state, &blobs[i], &sid)
		    || !sid_peek_rid(&sid, &members[i])) {
			TALLOC_FREE(members);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	*pmembers = members;
	*pnum_members = num_members;
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_enum_group_memberships(struct pdb_methods *m,
					       TALLOC_CTX *mem_ctx,
					       struct samu *user,
					       struct dom_sid **pp_sids,
					       gid_t **pp_gids,
					       uint32_t *p_num_groups)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct pdb_ads_samu_private *priv;
	const char *attrs[1] = { "objectSid" };
	struct tldap_message **groups;
	int i, rc, count;
	size_t num_groups;
	struct dom_sid *group_sids;
	gid_t *gids;

	priv = pdb_ads_get_samu_private(m, user);
	if (priv != NULL) {
		rc = pdb_ads_search_fmt(
			state, state->domaindn, TLDAP_SCOPE_SUB,
			attrs, ARRAY_SIZE(attrs), 0, talloc_tos(), &groups,
			"(&(member=%s)(grouptype=%d)(objectclass=group))",
			priv->dn, GTYPE_SECURITY_GLOBAL_GROUP);
		if (rc != TLDAP_SUCCESS) {
			DEBUG(10, ("ldap_search failed %s\n",
				   tldap_errstr(talloc_tos(), state->ld, rc)));
			return NT_STATUS_LDAP(rc);
		}
		count = talloc_array_length(groups);
	} else {
		/*
		 * This happens for artificial samu users
		 */
		DEBUG(10, ("Could not get pdb_ads_samu_private\n"));
		count = 0;
	}

	group_sids = talloc_array(mem_ctx, struct dom_sid, count+1);
	if (group_sids == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	gids = talloc_array(mem_ctx, gid_t, count+1);
	if (gids == NULL) {
		TALLOC_FREE(group_sids);
		return NT_STATUS_NO_MEMORY;
	}

	sid_copy(&group_sids[0], pdb_get_group_sid(user));
	if (!sid_to_gid(&group_sids[0], &gids[0])) {
		TALLOC_FREE(gids);
		TALLOC_FREE(group_sids);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	num_groups = 1;

	for (i=0; i<count; i++) {
		if (!tldap_pull_binsid(groups[i], "objectSid",
				       &group_sids[num_groups])) {
			continue;
		}
		gids[num_groups] = pdb_ads_sid2gid(&group_sids[num_groups]);

		num_groups += 1;
		if (num_groups == count) {
			break;
		}
	}

	*pp_sids = group_sids;
	*pp_gids = gids;
	*p_num_groups = num_groups;
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_set_unix_primary_group(struct pdb_methods *m,
					       TALLOC_CTX *mem_ctx,
					       struct samu *user)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_mod_groupmem(struct pdb_methods *m,
				     TALLOC_CTX *mem_ctx,
				     uint32 grouprid, uint32 memberrid,
				     int mod_op)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	TALLOC_CTX *frame = talloc_stackframe();
	struct tldap_context *ld;
	struct dom_sid groupsid, membersid;
	char *groupdn, *memberdn;
	struct tldap_mod *mods;
	int num_mods;
	int rc;
	NTSTATUS status;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	sid_compose(&groupsid, &state->domainsid, grouprid);
	sid_compose(&membersid, &state->domainsid, memberrid);

	status = pdb_ads_sid2dn(state, &groupsid, talloc_tos(), &groupdn);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_SUCH_GROUP;
	}
	status = pdb_ads_sid2dn(state, &membersid, talloc_tos(), &memberdn);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_SUCH_USER;
	}

	mods = NULL;
	num_mods = 0;

	if (!tldap_add_mod_str(talloc_tos(), &mods, &num_mods, mod_op,
			       "member", memberdn)) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	rc = tldap_modify(ld, groupdn, mods, num_mods, NULL, 0, NULL, 0);
	TALLOC_FREE(frame);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_modify failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		if ((mod_op == TLDAP_MOD_ADD) &&
		    (rc == TLDAP_ALREADY_EXISTS)) {
			return NT_STATUS_MEMBER_IN_GROUP;
		}
		if ((mod_op == TLDAP_MOD_DELETE) &&
		    (rc == TLDAP_UNWILLING_TO_PERFORM)) {
			return NT_STATUS_MEMBER_NOT_IN_GROUP;
		}
		return NT_STATUS_LDAP(rc);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_add_groupmem(struct pdb_methods *m,
				     TALLOC_CTX *mem_ctx,
				     uint32 group_rid, uint32 member_rid)
{
	return pdb_ads_mod_groupmem(m, mem_ctx, group_rid, member_rid,
				    TLDAP_MOD_ADD);
}

static NTSTATUS pdb_ads_del_groupmem(struct pdb_methods *m,
				     TALLOC_CTX *mem_ctx,
				     uint32 group_rid, uint32 member_rid)
{
	return pdb_ads_mod_groupmem(m, mem_ctx, group_rid, member_rid,
				    TLDAP_MOD_DELETE);
}

static NTSTATUS pdb_ads_create_alias(struct pdb_methods *m,
				     const char *name, uint32 *rid)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	const char *attrs[1] = { "objectSid" };
	int num_mods = 0;
	struct tldap_mod *mods = NULL;
	struct tldap_message **alias;
	struct dom_sid sid;
	char *dn;
	int rc;
	bool ok = true;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	dn = talloc_asprintf(talloc_tos(), "cn=%s,cn=users,%s", name,
			     state->domaindn);
	if (dn == NULL) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "samAccountName", "%s",
		name);
	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "objectClass", "group");
	ok &= tldap_make_mod_fmt(
		NULL, talloc_tos(), &mods, &num_mods, "groupType",
		"%d", (int)GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);

	if (!ok) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	rc = tldap_add(ld, dn, mods, num_mods, NULL, 0, NULL, 0);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_add failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		TALLOC_FREE(frame);
		return NT_STATUS_LDAP(rc);
	}

	rc = pdb_ads_search_fmt(
		state, state->domaindn, TLDAP_SCOPE_SUB,
		attrs, ARRAY_SIZE(attrs), 0, talloc_tos(), &alias,
		"(&(objectclass=group)(samaccountname=%s))", name);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not find just created alias %s: %s\n",
			   name, tldap_errstr(talloc_tos(), state->ld, rc)));
		TALLOC_FREE(frame);
		return NT_STATUS_LDAP(rc);
	}

	if (talloc_array_length(alias) != 1) {
		DEBUG(10, ("Got %d alias, expected one\n",
			   (int)talloc_array_length(alias)));
		TALLOC_FREE(frame);
		return NT_STATUS_LDAP(rc);
	}

	if (!tldap_pull_binsid(alias[0], "objectSid", &sid)) {
		DEBUG(10, ("Could not fetch objectSid from alias %s\n",
			   name));
		TALLOC_FREE(frame);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	sid_peek_rid(&sid, rid);
	TALLOC_FREE(frame);
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_delete_alias(struct pdb_methods *m,
				     const struct dom_sid *sid)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	struct tldap_message **alias;
	char *sidstr, *dn = NULL;
	int rc;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), sid);
	if (sidstr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				NULL, 0, 0, talloc_tos(), &alias,
				"(&(objectSid=%s)(objectclass=group)"
				"(|(grouptype=%d)(grouptype=%d)))",
				sidstr, GTYPE_SECURITY_BUILTIN_LOCAL_GROUP,
				GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);
	TALLOC_FREE(sidstr);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	if (talloc_array_length(alias) != 1) {
		DEBUG(10, ("Expected 1 alias, got %d\n",
			   (int)talloc_array_length(alias)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (!tldap_entry_dn(alias[0], &dn)) {
		DEBUG(10, ("Could not get DN for alias %s\n",
			   sid_string_dbg(sid)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	rc = tldap_delete(ld, dn, NULL, 0, NULL, 0);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_delete failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_set_aliasinfo(struct pdb_methods *m,
				      const struct dom_sid *sid,
				      struct acct_info *info)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	const char *attrs[3] = { "objectSid", "description",
				 "samAccountName" };
	struct tldap_message **msg;
	char *sidstr, *dn;
	int rc;
	struct tldap_mod *mods;
	int num_mods;
	bool ok;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&msg, "(&(objectSid=%s)(objectclass=group)"
				"(|(grouptype=%d)(grouptype=%d)))",
				sidstr, GTYPE_SECURITY_BUILTIN_LOCAL_GROUP,
				GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);
	TALLOC_FREE(sidstr);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	switch talloc_array_length(msg) {
	case 0:
		return NT_STATUS_NO_SUCH_ALIAS;
	case 1:
		break;
	default:
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (!tldap_entry_dn(msg[0], &dn)) {
		TALLOC_FREE(msg);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	mods = NULL;
	num_mods = 0;
	ok = true;

	ok &= tldap_make_mod_fmt(
		msg[0], msg, &mods, &num_mods, "description",
		"%s", info->acct_desc);
	ok &= tldap_make_mod_fmt(
		msg[0], msg, &mods, &num_mods, "samAccountName",
		"%s", info->acct_name);
	if (!ok) {
		TALLOC_FREE(msg);
		return NT_STATUS_NO_MEMORY;
	}
	if (num_mods == 0) {
		/* no change */
		TALLOC_FREE(msg);
		return NT_STATUS_OK;
	}

	rc = tldap_modify(ld, dn, mods, num_mods, NULL, 0, NULL, 0);
	TALLOC_FREE(msg);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_modify failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_sid2dn(struct pdb_ads_state *state,
			       const struct dom_sid *sid,
			       TALLOC_CTX *mem_ctx, char **pdn)
{
	struct tldap_message **msg;
	char *sidstr, *dn;
	int rc;

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				NULL, 0, 0, talloc_tos(), &msg,
				"(objectsid=%s)", sidstr);
	TALLOC_FREE(sidstr);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}

	switch talloc_array_length(msg) {
	case 0:
		return NT_STATUS_NOT_FOUND;
	case 1:
		break;
	default:
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (!tldap_entry_dn(msg[0], &dn)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	dn = talloc_strdup(mem_ctx, dn);
	if (dn == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	TALLOC_FREE(msg);

	*pdn = dn;
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_mod_aliasmem(struct pdb_methods *m,
				     const struct dom_sid *alias,
				     const struct dom_sid *member,
				     int mod_op)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct tldap_context *ld;
	TALLOC_CTX *frame = talloc_stackframe();
	struct tldap_mod *mods;
	int num_mods;
	int rc;
	char *aliasdn, *memberdn;
	NTSTATUS status;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return NT_STATUS_LDAP(TLDAP_SERVER_DOWN);
	}

	status = pdb_ads_sid2dn(state, alias, talloc_tos(), &aliasdn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("pdb_ads_sid2dn (%s) failed: %s\n",
			   sid_string_dbg(alias), nt_errstr(status)));
		TALLOC_FREE(frame);
		return NT_STATUS_NO_SUCH_ALIAS;
	}
	status = pdb_ads_sid2dn(state, member, talloc_tos(), &memberdn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("pdb_ads_sid2dn (%s) failed: %s\n",
			   sid_string_dbg(member), nt_errstr(status)));
		TALLOC_FREE(frame);
		return status;
	}

	mods = NULL;
	num_mods = 0;

	if (!tldap_add_mod_str(talloc_tos(), &mods, &num_mods, mod_op,
			       "member", memberdn)) {
		TALLOC_FREE(frame);
		return NT_STATUS_NO_MEMORY;
	}

	rc = tldap_modify(ld, aliasdn, mods, num_mods, NULL, 0, NULL, 0);
	TALLOC_FREE(frame);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_modify failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		if (rc == TLDAP_TYPE_OR_VALUE_EXISTS) {
			return NT_STATUS_MEMBER_IN_ALIAS;
		}
		if (rc == TLDAP_NO_SUCH_ATTRIBUTE) {
			return NT_STATUS_MEMBER_NOT_IN_ALIAS;
		}
		return NT_STATUS_LDAP(rc);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_add_aliasmem(struct pdb_methods *m,
				     const struct dom_sid *alias,
				     const struct dom_sid *member)
{
	return pdb_ads_mod_aliasmem(m, alias, member, TLDAP_MOD_ADD);
}

static NTSTATUS pdb_ads_del_aliasmem(struct pdb_methods *m,
				     const struct dom_sid *alias,
				     const struct dom_sid *member)
{
	return pdb_ads_mod_aliasmem(m, alias, member, TLDAP_MOD_DELETE);
}

static bool pdb_ads_dnblob2sid(struct pdb_ads_state *state, DATA_BLOB *dnblob,
			       struct dom_sid *psid)
{
	const char *attrs[1] = { "objectSid" };
	struct tldap_message **msg;
	char *dn;
	size_t len;
	int rc;
	bool ret;

	if (!convert_string_talloc(talloc_tos(), CH_UTF8, CH_UNIX,
				   dnblob->data, dnblob->length, &dn, &len,
				   false)) {
		return false;
	}
	rc = pdb_ads_search_fmt(state, dn, TLDAP_SCOPE_BASE,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&msg, "(objectclass=*)");
	TALLOC_FREE(dn);
	if (talloc_array_length(msg) != 1) {
		DEBUG(10, ("Got %d objects, expected one\n",
			   (int)talloc_array_length(msg)));
		TALLOC_FREE(msg);
		return false;
	}

	ret = tldap_pull_binsid(msg[0], "objectSid", psid);
	TALLOC_FREE(msg);
	return ret;
}

static NTSTATUS pdb_ads_enum_aliasmem(struct pdb_methods *m,
				      const struct dom_sid *alias,
				      TALLOC_CTX *mem_ctx,
				      struct dom_sid **pmembers,
				      size_t *pnum_members)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	const char *attrs[1] = { "member" };
	char *sidstr;
	struct tldap_message **msg;
	int i, rc, num_members;
	DATA_BLOB *blobs;
	struct dom_sid *members;

	sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), alias);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&msg, "(objectsid=%s)", sidstr);
	TALLOC_FREE(sidstr);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}
	switch talloc_array_length(msg) {
	case 0:
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		break;
	case 1:
		break;
	default:
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
		break;
	}

	if (!tldap_entry_values(msg[0], "member", &blobs, &num_members)) {
		*pmembers = NULL;
		*pnum_members = 0;
		return NT_STATUS_OK;
	}

	members = talloc_array(mem_ctx, struct dom_sid, num_members);
	if (members == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_members; i++) {
		if (!pdb_ads_dnblob2sid(state, &blobs[i], &members[i])) {
			TALLOC_FREE(members);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	*pmembers = members;
	*pnum_members = num_members;
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_enum_alias_memberships(struct pdb_methods *m,
					       TALLOC_CTX *mem_ctx,
					       const struct dom_sid *domain_sid,
					       const struct dom_sid *members,
					       size_t num_members,
					       uint32_t **palias_rids,
					       size_t *pnum_alias_rids)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	const char *attrs[1] = { "objectSid" };
	struct tldap_message **msg = NULL;
	uint32_t *alias_rids = NULL;
	size_t num_alias_rids = 0;
	int i, rc, count;
	bool got_members = false;
	char *filter;
	NTSTATUS status;

	/*
	 * TODO: Get the filter right so that we only get the aliases from
	 * either the SAM or BUILTIN
	 */

	filter = talloc_asprintf(talloc_tos(),
				 "(&(|(grouptype=%d)(grouptype=%d))"
				 "(objectclass=group)(|",
				 GTYPE_SECURITY_BUILTIN_LOCAL_GROUP,
				 GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_members; i++) {
		char *dn;

		status = pdb_ads_sid2dn(state, &members[i], talloc_tos(), &dn);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("pdb_ads_sid2dn failed for %s: %s\n",
				   sid_string_dbg(&members[i]),
				   nt_errstr(status)));
			continue;
		}
		filter = talloc_asprintf_append_buffer(
			filter, "(member=%s)", dn);
		TALLOC_FREE(dn);
		if (filter == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		got_members = true;
	}

	if (!got_members) {
		goto done;
	}

	rc = pdb_ads_search_fmt(state, state->domaindn, TLDAP_SCOPE_SUB,
				attrs, ARRAY_SIZE(attrs), 0, talloc_tos(),
				&msg, "%s))", filter);
	TALLOC_FREE(filter);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("tldap_search failed %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return NT_STATUS_LDAP(rc);
	}

	count = talloc_array_length(msg);
	if (count == 0) {
		goto done;
	}

	alias_rids = talloc_array(mem_ctx, uint32_t, count);
	if (alias_rids == NULL) {
		TALLOC_FREE(msg);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<count; i++) {
		struct dom_sid sid;

		if (!tldap_pull_binsid(msg[i], "objectSid", &sid)) {
			DEBUG(10, ("Could not pull SID for member %d\n", i));
			continue;
		}
		if (sid_peek_check_rid(domain_sid, &sid,
				       &alias_rids[num_alias_rids])) {
			num_alias_rids += 1;
		}
	}
done:
	TALLOC_FREE(msg);
	*palias_rids = alias_rids;
	*pnum_alias_rids = 0;
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_lookup_rids(struct pdb_methods *m,
				    const struct dom_sid *domain_sid,
				    int num_rids,
				    uint32 *rids,
				    const char **names,
				    enum lsa_SidType *lsa_attrs)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	const char *attrs[2] = { "sAMAccountType", "sAMAccountName" };
	int i, num_mapped;

	if (num_rids == 0) {
		return NT_STATUS_NONE_MAPPED;
	}

	num_mapped = 0;

	for (i=0; i<num_rids; i++) {
		struct dom_sid sid;
		struct tldap_message **msg;
		char *sidstr;
		uint32_t attr;
		int rc;

		lsa_attrs[i] = SID_NAME_UNKNOWN;

		sid_compose(&sid, domain_sid, rids[i]);

		sidstr = ldap_encode_ndr_dom_sid(talloc_tos(), &sid);
		NT_STATUS_HAVE_NO_MEMORY(sidstr);

		rc = pdb_ads_search_fmt(state, state->domaindn,
					TLDAP_SCOPE_SUB, attrs,
					ARRAY_SIZE(attrs), 0, talloc_tos(),
					&msg, "(objectsid=%s)", sidstr);
		TALLOC_FREE(sidstr);
		if (rc != TLDAP_SUCCESS) {
			DEBUG(10, ("ldap_search failed %s\n",
				   tldap_errstr(talloc_tos(), state->ld, rc)));
			continue;
		}

		switch talloc_array_length(msg) {
		case 0:
			DEBUG(10, ("rid %d not found\n", (int)rids[i]));
			continue;
		case 1:
			break;
		default:
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		names[i] = tldap_talloc_single_attribute(
			msg[0], "samAccountName", talloc_tos());
		if (names[i] == NULL) {
			DEBUG(10, ("no samAccountName\n"));
			continue;
		}
		if (!tldap_pull_uint32(msg[0], "samAccountType", &attr)) {
			DEBUG(10, ("no samAccountType"));
			continue;
		}
		lsa_attrs[i] = ds_atype_map(attr);
		num_mapped += 1;
	}

	if (num_mapped == 0) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (num_mapped < num_rids) {
		return STATUS_SOME_UNMAPPED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS pdb_ads_lookup_names(struct pdb_methods *m,
				     const struct dom_sid *domain_sid,
				     int num_names,
				     const char **pp_names,
				     uint32 *rids,
				     enum lsa_SidType *attrs)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS pdb_ads_get_account_policy(struct pdb_methods *m,
					   enum pdb_policy_type type,
					   uint32_t *value)
{
	return account_policy_get(type, value)
		? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS pdb_ads_set_account_policy(struct pdb_methods *m,
					   enum pdb_policy_type type,
					   uint32_t value)
{
	return account_policy_set(type, value)
		? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS pdb_ads_get_seq_num(struct pdb_methods *m,
				    time_t *seq_num)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

struct pdb_ads_search_state {
	uint32_t acct_flags;
	struct samr_displayentry *entries;
	uint32_t num_entries;
	ssize_t array_size;
	uint32_t current;
};

static bool pdb_ads_next_entry(struct pdb_search *search,
			       struct samr_displayentry *entry)
{
	struct pdb_ads_search_state *state = talloc_get_type_abort(
		search->private_data, struct pdb_ads_search_state);

	if (state->current == state->num_entries) {
		return false;
	}

	entry->idx = state->entries[state->current].idx;
	entry->rid = state->entries[state->current].rid;
	entry->acct_flags = state->entries[state->current].acct_flags;

	entry->account_name = talloc_strdup(
		search, state->entries[state->current].account_name);
	entry->fullname = talloc_strdup(
		search, state->entries[state->current].fullname);
	entry->description = talloc_strdup(
		search, state->entries[state->current].description);

	if ((entry->account_name == NULL) || (entry->fullname == NULL)
	    || (entry->description == NULL)) {
		DEBUG(0, ("talloc_strdup failed\n"));
		return false;
	}

	state->current += 1;
	return true;
}

static void pdb_ads_search_end(struct pdb_search *search)
{
	struct pdb_ads_search_state *state = talloc_get_type_abort(
		search->private_data, struct pdb_ads_search_state);
	TALLOC_FREE(state);
}

static bool pdb_ads_search_filter(struct pdb_methods *m,
				  struct pdb_search *search,
				  const char *filter,
				  uint32_t acct_flags,
				  struct pdb_ads_search_state **pstate)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	struct pdb_ads_search_state *sstate;
	const char * attrs[] = { "objectSid", "sAMAccountName", "displayName",
				 "userAccountControl", "description" };
	struct tldap_message **users;
	int i, rc, num_users;

	sstate = talloc_zero(search, struct pdb_ads_search_state);
	if (sstate == NULL) {
		return false;
	}
	sstate->acct_flags = acct_flags;

	rc = pdb_ads_search_fmt(
		state, state->domaindn, TLDAP_SCOPE_SUB,
		attrs, ARRAY_SIZE(attrs), 0, talloc_tos(), &users,
		"%s", filter);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("ldap_search_ext_s failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return false;
	}

	num_users = talloc_array_length(users);

	sstate->entries = talloc_array(sstate, struct samr_displayentry,
				       num_users);
	if (sstate->entries == NULL) {
		DEBUG(10, ("talloc failed\n"));
		return false;
	}

	sstate->num_entries = 0;

	for (i=0; i<num_users; i++) {
		struct samr_displayentry *e;
		struct dom_sid sid;
		uint32_t ctrl;

		e = &sstate->entries[sstate->num_entries];

		e->idx = sstate->num_entries;
		if (!tldap_pull_binsid(users[i], "objectSid", &sid)) {
			DEBUG(10, ("Could not pull sid\n"));
			continue;
		}
		sid_peek_rid(&sid, &e->rid);

		if (tldap_pull_uint32(users[i], "userAccountControl", &ctrl)) {

			e->acct_flags = ds_uf2acb(ctrl);

			DEBUG(10, ("pdb_ads_search_filter: Found %x, "
				   "filter %x\n", (int)e->acct_flags,
				   (int)sstate->acct_flags));


			if ((sstate->acct_flags != 0) &&
			    ((sstate->acct_flags & e->acct_flags) == 0)) {
				continue;
			}

			if (e->acct_flags & (ACB_WSTRUST|ACB_SVRTRUST)) {
				e->acct_flags |= ACB_NORMAL;
			}
		} else {
			e->acct_flags = ACB_NORMAL;
		}

		if (e->rid == DOMAIN_RID_GUEST) {
			/*
			 * Guest is specially crafted in s3. Make
			 * QueryDisplayInfo match QueryUserInfo
			 */
			e->account_name = lp_guestaccount();
			e->fullname = lp_guestaccount();
			e->description = "";
			e->acct_flags = ACB_NORMAL;
		} else {
			e->account_name = tldap_talloc_single_attribute(
				users[i], "samAccountName", sstate->entries);
			e->fullname = tldap_talloc_single_attribute(
				users[i], "displayName", sstate->entries);
			e->description = tldap_talloc_single_attribute(
				users[i], "description", sstate->entries);
		}
		if (e->account_name == NULL) {
			return false;
		}
		if (e->fullname == NULL) {
			e->fullname = "";
		}
		if (e->description == NULL) {
			e->description = "";
		}

		sstate->num_entries += 1;
		if (sstate->num_entries >= num_users) {
			break;
		}
	}

	search->private_data = sstate;
	search->next_entry = pdb_ads_next_entry;
	search->search_end = pdb_ads_search_end;
	*pstate = sstate;
	return true;
}

static bool pdb_ads_search_users(struct pdb_methods *m,
				 struct pdb_search *search,
				 uint32 acct_flags)
{
	struct pdb_ads_search_state *sstate;
	char *filter;
	bool ret;

	DEBUG(10, ("pdb_ads_search_users got flags %x\n", acct_flags));

	if (acct_flags & ACB_NORMAL) {
		filter = talloc_asprintf(
			talloc_tos(),
			"(&(objectclass=user)(sAMAccountType=%d))",
			ATYPE_NORMAL_ACCOUNT);
	} else if (acct_flags & ACB_WSTRUST) {
		filter = talloc_asprintf(
			talloc_tos(),
			"(&(objectclass=user)(sAMAccountType=%d))",
			ATYPE_WORKSTATION_TRUST);
	} else {
		filter = talloc_strdup(talloc_tos(), "(objectclass=user)");
	}
	if (filter == NULL) {
		return false;
	}

	ret = pdb_ads_search_filter(m, search, filter, acct_flags, &sstate);
	TALLOC_FREE(filter);
	if (!ret) {
		return false;
	}
	return true;
}

static bool pdb_ads_search_groups(struct pdb_methods *m,
				  struct pdb_search *search)
{
	struct pdb_ads_search_state *sstate;
	char *filter;
	bool ret;

	filter = talloc_asprintf(talloc_tos(),
				 "(&(grouptype=%d)(objectclass=group))",
				 GTYPE_SECURITY_GLOBAL_GROUP);
	if (filter == NULL) {
		return false;
	}
	ret = pdb_ads_search_filter(m, search, filter, 0, &sstate);
	TALLOC_FREE(filter);
	if (!ret) {
		return false;
	}
	return true;
}

static bool pdb_ads_search_aliases(struct pdb_methods *m,
				   struct pdb_search *search,
				   const struct dom_sid *sid)
{
	struct pdb_ads_search_state *sstate;
	char *filter;
	bool ret;

	filter = talloc_asprintf(
		talloc_tos(), "(&(grouptype=%d)(objectclass=group))",
		sid_check_is_builtin(sid)
		? GTYPE_SECURITY_BUILTIN_LOCAL_GROUP
		: GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);

	if (filter == NULL) {
		return false;
	}
	ret = pdb_ads_search_filter(m, search, filter, 0, &sstate);
	TALLOC_FREE(filter);
	if (!ret) {
		return false;
	}
	return true;
}

static bool pdb_ads_uid_to_sid(struct pdb_methods *m, uid_t uid,
			       struct dom_sid *sid)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	sid_compose(sid, &state->domainsid, uid);
	return true;
}

static bool pdb_ads_gid_to_sid(struct pdb_methods *m, gid_t gid,
			       struct dom_sid *sid)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	sid_compose(sid, &state->domainsid, gid);
	return true;
}

static bool pdb_ads_sid_to_id(struct pdb_methods *m, const struct dom_sid *sid,
			      union unid_t *id, enum lsa_SidType *type)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		m->private_data, struct pdb_ads_state);
	const char *attrs[4] = { "objectClass", "samAccountType",
				 "uidNumber", "gidNumber" };
	struct tldap_message **msg;
	char *sidstr, *base;
	uint32_t atype;
	int rc;
	bool ret = false;

	sidstr = sid_binstring_hex(sid);
	if (sidstr == NULL) {
		return false;
	}
	base = talloc_asprintf(talloc_tos(), "<SID=%s>", sidstr);
	SAFE_FREE(sidstr);

	rc = pdb_ads_search_fmt(
		state, base, TLDAP_SCOPE_BASE,
		attrs, ARRAY_SIZE(attrs), 0, talloc_tos(), &msg,
		"(objectclass=*)");
	TALLOC_FREE(base);

	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("pdb_ads_search_fmt failed: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		return false;
	}
	if (talloc_array_length(msg) != 1) {
		DEBUG(10, ("Got %d objects, expected 1\n",
			   (int)talloc_array_length(msg)));
		goto fail;
	}
	if (!tldap_pull_uint32(msg[0], "samAccountType", &atype)) {
		DEBUG(10, ("samAccountType not found\n"));
		goto fail;
	}
	if (atype == ATYPE_ACCOUNT) {
		uint32_t uid;
		*type = SID_NAME_USER;
		if (!tldap_pull_uint32(msg[0], "uidNumber", &uid)) {
			DEBUG(10, ("Did not find uidNumber\n"));
			goto fail;
		}
		id->uid = uid;
	} else {
		uint32_t gid;
		*type = SID_NAME_DOM_GRP;
		if (!tldap_pull_uint32(msg[0], "gidNumber", &gid)) {
			DEBUG(10, ("Did not find gidNumber\n"));
			goto fail;
		}
		id->gid = gid;
	}
	ret = true;
fail:
	TALLOC_FREE(msg);
	return ret;
}

static uint32_t pdb_ads_capabilities(struct pdb_methods *m)
{
	return PDB_CAP_STORE_RIDS | PDB_CAP_ADS;
}

static bool pdb_ads_new_rid(struct pdb_methods *m, uint32 *rid)
{
	return false;
}

static bool pdb_ads_get_trusteddom_pw(struct pdb_methods *m,
				      const char *domain, char** pwd,
				      struct dom_sid *sid,
				      time_t *pass_last_set_time)
{
	return false;
}

static bool pdb_ads_set_trusteddom_pw(struct pdb_methods *m,
				      const char* domain, const char* pwd,
				      const struct dom_sid *sid)
{
	return false;
}

static bool pdb_ads_del_trusteddom_pw(struct pdb_methods *m,
				      const char *domain)
{
	return false;
}

static NTSTATUS pdb_ads_enum_trusteddoms(struct pdb_methods *m,
					 TALLOC_CTX *mem_ctx,
					 uint32 *num_domains,
					 struct trustdom_info ***domains)
{
	*num_domains = 0;
	*domains = NULL;
	return NT_STATUS_OK;
}

static void pdb_ads_init_methods(struct pdb_methods *m)
{
	m->name = "ads";
	m->get_domain_info = pdb_ads_get_domain_info;
	m->getsampwnam = pdb_ads_getsampwnam;
	m->getsampwsid = pdb_ads_getsampwsid;
	m->create_user = pdb_ads_create_user;
	m->delete_user = pdb_ads_delete_user;
	m->add_sam_account = pdb_ads_add_sam_account;
	m->update_sam_account = pdb_ads_update_sam_account;
	m->delete_sam_account = pdb_ads_delete_sam_account;
	m->rename_sam_account = pdb_ads_rename_sam_account;
	m->update_login_attempts = pdb_ads_update_login_attempts;
	m->getgrsid = pdb_ads_getgrsid;
	m->getgrgid = pdb_ads_getgrgid;
	m->getgrnam = pdb_ads_getgrnam;
	m->create_dom_group = pdb_ads_create_dom_group;
	m->delete_dom_group = pdb_ads_delete_dom_group;
	m->add_group_mapping_entry = pdb_ads_add_group_mapping_entry;
	m->update_group_mapping_entry = pdb_ads_update_group_mapping_entry;
	m->delete_group_mapping_entry =	pdb_ads_delete_group_mapping_entry;
	m->enum_group_mapping = pdb_ads_enum_group_mapping;
	m->enum_group_members = pdb_ads_enum_group_members;
	m->enum_group_memberships = pdb_ads_enum_group_memberships;
	m->set_unix_primary_group = pdb_ads_set_unix_primary_group;
	m->add_groupmem = pdb_ads_add_groupmem;
	m->del_groupmem = pdb_ads_del_groupmem;
	m->create_alias = pdb_ads_create_alias;
	m->delete_alias = pdb_ads_delete_alias;
	m->get_aliasinfo = pdb_default_get_aliasinfo;
	m->set_aliasinfo = pdb_ads_set_aliasinfo;
	m->add_aliasmem = pdb_ads_add_aliasmem;
	m->del_aliasmem = pdb_ads_del_aliasmem;
	m->enum_aliasmem = pdb_ads_enum_aliasmem;
	m->enum_alias_memberships = pdb_ads_enum_alias_memberships;
	m->lookup_rids = pdb_ads_lookup_rids;
	m->lookup_names = pdb_ads_lookup_names;
	m->get_account_policy = pdb_ads_get_account_policy;
	m->set_account_policy = pdb_ads_set_account_policy;
	m->get_seq_num = pdb_ads_get_seq_num;
	m->search_users = pdb_ads_search_users;
	m->search_groups = pdb_ads_search_groups;
	m->search_aliases = pdb_ads_search_aliases;
	m->uid_to_sid = pdb_ads_uid_to_sid;
	m->gid_to_sid = pdb_ads_gid_to_sid;
	m->sid_to_id = pdb_ads_sid_to_id;
	m->capabilities = pdb_ads_capabilities;
	m->new_rid = pdb_ads_new_rid;
	m->get_trusteddom_pw = pdb_ads_get_trusteddom_pw;
	m->set_trusteddom_pw = pdb_ads_set_trusteddom_pw;
	m->del_trusteddom_pw = pdb_ads_del_trusteddom_pw;
	m->enum_trusteddoms = pdb_ads_enum_trusteddoms;
}

static void free_private_data(void **vp)
{
	struct pdb_ads_state *state = talloc_get_type_abort(
		*vp, struct pdb_ads_state);

	TALLOC_FREE(state->ld);
	return;
}

/*
  this is used to catch debug messages from events
*/
static void s3_tldap_debug(void *context, enum tldap_debug_level level,
			   const char *fmt, va_list ap)  PRINTF_ATTRIBUTE(3,0);

static void s3_tldap_debug(void *context, enum tldap_debug_level level,
			   const char *fmt, va_list ap)
{
	int samba_level = -1;
	char *s = NULL;
	switch (level) {
	case TLDAP_DEBUG_FATAL:
		samba_level = 0;
		break;
	case TLDAP_DEBUG_ERROR:
		samba_level = 1;
		break;
	case TLDAP_DEBUG_WARNING:
		samba_level = 2;
		break;
	case TLDAP_DEBUG_TRACE:
		samba_level = 11;
		break;

	};
	if (vasprintf(&s, fmt, ap) == -1) {
		return;
	}
	DEBUG(samba_level, ("tldap: %s", s));
	free(s);
}

static struct tldap_context *pdb_ads_ld(struct pdb_ads_state *state)
{
	NTSTATUS status;
	int fd;

	if (tldap_connection_ok(state->ld)) {
		return state->ld;
	}
	TALLOC_FREE(state->ld);

	status = open_socket_out(
		(struct sockaddr_storage *)(void *)&state->socket_address,
		0, 0, &fd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not connect to %s: %s\n",
			   state->socket_address.sun_path, nt_errstr(status)));
		return NULL;
	}

	set_blocking(fd, false);

	state->ld = tldap_context_create(state, fd);
	if (state->ld == NULL) {
		close(fd);
		return NULL;
	}
	tldap_set_debug(state->ld, s3_tldap_debug, NULL);

	return state->ld;
}

int pdb_ads_search_fmt(struct pdb_ads_state *state, const char *base,
		       int scope, const char *attrs[], int num_attrs,
		       int attrsonly,
		       TALLOC_CTX *mem_ctx, struct tldap_message ***res,
		       const char *fmt, ...)
{
	struct tldap_context *ld;
	va_list ap;
	int ret;

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return TLDAP_SERVER_DOWN;
	}

	va_start(ap, fmt);
	ret = tldap_search_va(ld, base, scope, attrs, num_attrs, attrsonly,
			      mem_ctx, res, fmt, ap);
	va_end(ap);

	if (ret != TLDAP_SERVER_DOWN) {
		return ret;
	}

	/* retry once */
	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		return TLDAP_SERVER_DOWN;
	}

	va_start(ap, fmt);
	ret = tldap_search_va(ld, base, scope, attrs, num_attrs, attrsonly,
			      mem_ctx, res, fmt, ap);
	va_end(ap);
	return ret;
}

static NTSTATUS pdb_ads_connect(struct pdb_ads_state *state,
				const char *location)
{
	const char *domain_attrs[2] = { "objectSid", "objectGUID" };
	const char *ncname_attrs[1] = { "netbiosname" };
	struct tldap_context *ld;
	struct tldap_message *rootdse, **domain, **ncname;
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS status;
	int num_domains;
	int rc;

	ZERO_STRUCT(state->socket_address);
	state->socket_address.sun_family = AF_UNIX;
	strlcpy(state->socket_address.sun_path, location,
		sizeof(state->socket_address.sun_path));

	ld = pdb_ads_ld(state);
	if (ld == NULL) {
		status = NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		goto done;
	}

	rc = tldap_fetch_rootdse(ld);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not retrieve rootdse: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		status = NT_STATUS_LDAP(rc);
		goto done;
	}
	rootdse = tldap_rootdse(state->ld);

	state->domaindn = tldap_talloc_single_attribute(
		rootdse, "defaultNamingContext", state);
	if (state->domaindn == NULL) {
		DEBUG(10, ("Could not get defaultNamingContext\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}
	DEBUG(10, ("defaultNamingContext = %s\n", state->domaindn));

	state->configdn = tldap_talloc_single_attribute(
		rootdse, "configurationNamingContext", state);
	if (state->configdn == NULL) {
		DEBUG(10, ("Could not get configurationNamingContext\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}
	DEBUG(10, ("configurationNamingContext = %s\n", state->configdn));

	/*
	 * Figure out our domain's SID
	 */
	rc = pdb_ads_search_fmt(
		state, state->domaindn, TLDAP_SCOPE_BASE,
		domain_attrs, ARRAY_SIZE(domain_attrs), 0,
		talloc_tos(), &domain, "(objectclass=*)");
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not retrieve domain: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		status = NT_STATUS_LDAP(rc);
		goto done;
	}

	num_domains = talloc_array_length(domain);
	if (num_domains != 1) {
		DEBUG(10, ("Got %d domains, expected one\n", num_domains));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}
	if (!tldap_pull_binsid(domain[0], "objectSid", &state->domainsid)) {
		DEBUG(10, ("Could not retrieve domain SID\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}
	if (!tldap_pull_guid(domain[0], "objectGUID", &state->domainguid)) {
		DEBUG(10, ("Could not retrieve domain GUID\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}
	DEBUG(10, ("Domain SID: %s\n", sid_string_dbg(&state->domainsid)));

	/*
	 * Figure out our domain's short name
	 */
	rc = pdb_ads_search_fmt(
		state, state->configdn, TLDAP_SCOPE_SUB,
		ncname_attrs, ARRAY_SIZE(ncname_attrs), 0,
		talloc_tos(), &ncname, "(ncname=%s)", state->domaindn);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not retrieve ncname: %s\n",
			   tldap_errstr(talloc_tos(), state->ld, rc)));
		status = NT_STATUS_LDAP(rc);
		goto done;
	}
	if (talloc_array_length(ncname) != 1) {
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}

	state->netbiosname = tldap_talloc_single_attribute(
		ncname[0], "netbiosname", state);
	if (state->netbiosname == NULL) {
		DEBUG(10, ("Could not get netbiosname\n"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto done;
	}
	DEBUG(10, ("netbiosname: %s\n", state->netbiosname));

	if (!strequal(lp_workgroup(), state->netbiosname)) {
		DEBUG(1, ("ADS is different domain (%s) than ours (%s)\n",
			  state->netbiosname, lp_workgroup()));
		status = NT_STATUS_NO_SUCH_DOMAIN;
		goto done;
	}

	secrets_store_domain_sid(state->netbiosname, &state->domainsid);

	status = NT_STATUS_OK;
done:
	TALLOC_FREE(frame);
	return status;
}

static NTSTATUS pdb_init_ads(struct pdb_methods **pdb_method,
			     const char *location)
{
	struct pdb_methods *m;
	struct pdb_ads_state *state;
	char *tmp = NULL;
	NTSTATUS status;

	m = talloc(NULL, struct pdb_methods);
	if (m == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	state = talloc_zero(m, struct pdb_ads_state);
	if (state == NULL) {
		goto nomem;
	}
	m->private_data = state;
	m->free_private_data = free_private_data;
	pdb_ads_init_methods(m);

	if (location == NULL) {
		tmp = talloc_asprintf(talloc_tos(), "/%s/ldap_priv/ldapi",
				      lp_private_dir());
		location = tmp;
	}
	if (location == NULL) {
		goto nomem;
	}

	status = pdb_ads_connect(state, location);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("pdb_ads_connect failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	*pdb_method = m;
	return NT_STATUS_OK;
nomem:
	status = NT_STATUS_NO_MEMORY;
fail:
	TALLOC_FREE(m);
	return status;
}

NTSTATUS pdb_ads_init(void);
NTSTATUS pdb_ads_init(void)
{
	return smb_register_passdb(PASSDB_INTERFACE_VERSION, "ads",
				   pdb_init_ads);
}
