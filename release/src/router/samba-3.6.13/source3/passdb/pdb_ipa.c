/*
   Unix SMB/CIFS implementation.
   IPA helper functions for SAMBA
   Copyright (C) Sumit Bose <sbose@redhat.com> 2010

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
#include "libcli/security/dom_sid.h"
#include "../librpc/ndr/libndr.h"
#include "librpc/gen_ndr/samr.h"

#include "smbldap.h"

#define IPA_KEYTAB_SET_OID "2.16.840.1.113730.3.8.3.1"
#define IPA_MAGIC_ID_STR "999"

#define LDAP_TRUST_CONTAINER "ou=system"
#define LDAP_ATTRIBUTE_CN "cn"
#define LDAP_ATTRIBUTE_TRUST_TYPE "sambaTrustType"
#define LDAP_ATTRIBUTE_TRUST_ATTRIBUTES "sambaTrustAttributes"
#define LDAP_ATTRIBUTE_TRUST_DIRECTION "sambaTrustDirection"
#define LDAP_ATTRIBUTE_TRUST_PARTNER "sambaTrustPartner"
#define LDAP_ATTRIBUTE_FLAT_NAME "sambaFlatName"
#define LDAP_ATTRIBUTE_TRUST_AUTH_OUTGOING "sambaTrustAuthOutgoing"
#define LDAP_ATTRIBUTE_TRUST_AUTH_INCOMING "sambaTrustAuthIncoming"
#define LDAP_ATTRIBUTE_SECURITY_IDENTIFIER "sambaSecurityIdentifier"
#define LDAP_ATTRIBUTE_TRUST_FOREST_TRUST_INFO "sambaTrustForestTrustInfo"
#define LDAP_ATTRIBUTE_OBJECTCLASS "objectClass"

#define LDAP_OBJ_KRB_PRINCIPAL "krbPrincipal"
#define LDAP_OBJ_KRB_PRINCIPAL_AUX "krbPrincipalAux"
#define LDAP_ATTRIBUTE_KRB_PRINCIPAL "krbPrincipalName"

#define LDAP_OBJ_IPAOBJECT "ipaObject"
#define LDAP_OBJ_IPAHOST "ipaHost"
#define LDAP_OBJ_POSIXACCOUNT "posixAccount"

#define LDAP_OBJ_GROUPOFNAMES "groupOfNames"
#define LDAP_OBJ_NESTEDGROUP "nestedGroup"
#define LDAP_OBJ_IPAUSERGROUP "ipaUserGroup"
#define LDAP_OBJ_POSIXGROUP "posixGroup"

#define HAS_KRB_PRINCIPAL (1<<0)
#define HAS_KRB_PRINCIPAL_AUX (1<<1)
#define HAS_IPAOBJECT (1<<2)
#define HAS_IPAHOST (1<<3)
#define HAS_POSIXACCOUNT (1<<4)
#define HAS_GROUPOFNAMES (1<<5)
#define HAS_NESTEDGROUP (1<<6)
#define HAS_IPAUSERGROUP (1<<7)
#define HAS_POSIXGROUP (1<<8)

struct ipasam_privates {
	bool server_is_ipa;
	NTSTATUS (*ldapsam_add_sam_account)(struct pdb_methods *,
					    struct samu *sampass);
	NTSTATUS (*ldapsam_update_sam_account)(struct pdb_methods *,
					       struct samu *sampass);
	NTSTATUS (*ldapsam_create_user)(struct pdb_methods *my_methods,
					TALLOC_CTX *tmp_ctx, const char *name,
					uint32_t acb_info, uint32_t *rid);
	NTSTATUS (*ldapsam_create_dom_group)(struct pdb_methods *my_methods,
					     TALLOC_CTX *tmp_ctx,
					     const char *name,
					     uint32_t *rid);
};

static bool ipasam_get_trusteddom_pw(struct pdb_methods *methods,
				      const char *domain,
				      char** pwd,
				      struct dom_sid *sid,
				      time_t *pass_last_set_time)
{
	return false;
}

static bool ipasam_set_trusteddom_pw(struct pdb_methods *methods,
				      const char* domain,
				      const char* pwd,
				      const struct dom_sid *sid)
{
	return false;
}

static bool ipasam_del_trusteddom_pw(struct pdb_methods *methods,
				      const char *domain)
{
	return false;
}

static char *get_account_dn(const char *name)
{
	char *escape_name;
	char *dn;

	escape_name = escape_rdn_val_string_alloc(name);
	if (!escape_name) {
		return NULL;
	}

	if (name[strlen(name)-1] == '$') {
		dn = talloc_asprintf(talloc_tos(), "uid=%s,%s", escape_name,
				     lp_ldap_machine_suffix());
	} else {
		dn = talloc_asprintf(talloc_tos(), "uid=%s,%s", escape_name,
				     lp_ldap_user_suffix());
	}

	SAFE_FREE(escape_name);

	return dn;
}

static char *trusted_domain_dn(struct ldapsam_privates *ldap_state,
			       const char *domain)
{
	return talloc_asprintf(talloc_tos(), "%s=%s,%s,%s",
			       LDAP_ATTRIBUTE_CN, domain,
			       LDAP_TRUST_CONTAINER, ldap_state->domain_dn);
}

static char *trusted_domain_base_dn(struct ldapsam_privates *ldap_state)
{
	return talloc_asprintf(talloc_tos(), "%s,%s",
			       LDAP_TRUST_CONTAINER, ldap_state->domain_dn);
}

static bool get_trusted_domain_int(struct ldapsam_privates *ldap_state,
				   TALLOC_CTX *mem_ctx,
				   const char *filter, LDAPMessage **entry)
{
	int rc;
	char *base_dn = NULL;
	LDAPMessage *result = NULL;
	uint32_t num_result;

	base_dn = trusted_domain_base_dn(ldap_state);
	if (base_dn == NULL) {
		return false;
	}

	rc = smbldap_search(ldap_state->smbldap_state, base_dn,
			    LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
	TALLOC_FREE(base_dn);

	if (result != NULL) {
		talloc_autofree_ldapmsg(mem_ctx, result);
	}

	if (rc == LDAP_NO_SUCH_OBJECT) {
		*entry = NULL;
		return true;
	}

	if (rc != LDAP_SUCCESS) {
		return false;
	}

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result > 1) {
		DEBUG(1, ("get_trusted_domain_int: more than one "
			  "%s object with filter '%s'?!\n",
			  LDAP_OBJ_TRUSTED_DOMAIN, filter));
		return false;
	}

	if (num_result == 0) {
		DEBUG(1, ("get_trusted_domain_int: no "
			  "%s object with filter '%s'.\n",
			  LDAP_OBJ_TRUSTED_DOMAIN, filter));
		*entry = NULL;
	} else {
		*entry = ldap_first_entry(priv2ld(ldap_state), result);
	}

	return true;
}

static bool get_trusted_domain_by_name_int(struct ldapsam_privates *ldap_state,
					  TALLOC_CTX *mem_ctx,
					  const char *domain,
					  LDAPMessage **entry)
{
	char *filter = NULL;

	filter = talloc_asprintf(talloc_tos(),
				 "(&(objectClass=%s)(|(%s=%s)(%s=%s)(cn=%s)))",
				 LDAP_OBJ_TRUSTED_DOMAIN,
				 LDAP_ATTRIBUTE_FLAT_NAME, domain,
				 LDAP_ATTRIBUTE_TRUST_PARTNER, domain, domain);
	if (filter == NULL) {
		return false;
	}

	return get_trusted_domain_int(ldap_state, mem_ctx, filter, entry);
}

static bool get_trusted_domain_by_sid_int(struct ldapsam_privates *ldap_state,
					   TALLOC_CTX *mem_ctx,
					   const char *sid, LDAPMessage **entry)
{
	char *filter = NULL;

	filter = talloc_asprintf(talloc_tos(), "(&(objectClass=%s)(%s=%s))",
				 LDAP_OBJ_TRUSTED_DOMAIN,
				 LDAP_ATTRIBUTE_SECURITY_IDENTIFIER, sid);
	if (filter == NULL) {
		return false;
	}

	return get_trusted_domain_int(ldap_state, mem_ctx, filter, entry);
}

static bool get_uint32_t_from_ldap_msg(struct ldapsam_privates *ldap_state,
				       LDAPMessage *entry,
				       const char *attr,
				       uint32_t *val)
{
	char *dummy;
	long int l;
	char *endptr;

	dummy = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry,
						attr, talloc_tos());
	if (dummy == NULL) {
		DEBUG(9, ("Attribute %s not present.\n", attr));
		*val = 0;
		return true;
	}

	l = strtoul(dummy, &endptr, 10);
	TALLOC_FREE(dummy);

	if (l < 0 || l > UINT32_MAX || *endptr != '\0') {
		return false;
	}

	*val = l;

	return true;
}

static void get_data_blob_from_ldap_msg(TALLOC_CTX *mem_ctx,
					struct ldapsam_privates *ldap_state,
					LDAPMessage *entry, const char *attr,
					DATA_BLOB *_blob)
{
	char *dummy;
	DATA_BLOB blob;

	dummy = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry, attr,
						talloc_tos());
	if (dummy == NULL) {
		DEBUG(9, ("Attribute %s not present.\n", attr));
		ZERO_STRUCTP(_blob);
	} else {
		blob = base64_decode_data_blob(dummy);
		if (blob.length == 0) {
			ZERO_STRUCTP(_blob);
		} else {
			_blob->length = blob.length;
			_blob->data = talloc_steal(mem_ctx, blob.data);
		}
	}
	TALLOC_FREE(dummy);
}

static bool fill_pdb_trusted_domain(TALLOC_CTX *mem_ctx,
				    struct ldapsam_privates *ldap_state,
				    LDAPMessage *entry,
				    struct pdb_trusted_domain **_td)
{
	char *dummy;
	bool res;
	struct pdb_trusted_domain *td;

	if (entry == NULL) {
		return false;
	}

	td = talloc_zero(mem_ctx, struct pdb_trusted_domain);
	if (td == NULL) {
		return false;
	}

	/* All attributes are MAY */

	dummy = smbldap_talloc_single_attribute(priv2ld(ldap_state), entry,
						LDAP_ATTRIBUTE_SECURITY_IDENTIFIER,
						talloc_tos());
	if (dummy == NULL) {
		DEBUG(9, ("Attribute %s not present.\n",
			  LDAP_ATTRIBUTE_SECURITY_IDENTIFIER));
		ZERO_STRUCT(td->security_identifier);
	} else {
		res = string_to_sid(&td->security_identifier, dummy);
		TALLOC_FREE(dummy);
		if (!res) {
			return false;
		}
	}

	get_data_blob_from_ldap_msg(td, ldap_state, entry,
				    LDAP_ATTRIBUTE_TRUST_AUTH_INCOMING,
				    &td->trust_auth_incoming);

	get_data_blob_from_ldap_msg(td, ldap_state, entry,
				    LDAP_ATTRIBUTE_TRUST_AUTH_OUTGOING,
				    &td->trust_auth_outgoing);

	td->netbios_name = smbldap_talloc_single_attribute(priv2ld(ldap_state),
							   entry,
							   LDAP_ATTRIBUTE_FLAT_NAME,
							   td);
	if (td->netbios_name == NULL) {
		DEBUG(9, ("Attribute %s not present.\n",
			  LDAP_ATTRIBUTE_FLAT_NAME));
	}

	td->domain_name = smbldap_talloc_single_attribute(priv2ld(ldap_state),
							  entry,
							  LDAP_ATTRIBUTE_TRUST_PARTNER,
							  td);
	if (td->domain_name == NULL) {
		DEBUG(9, ("Attribute %s not present.\n",
			  LDAP_ATTRIBUTE_TRUST_PARTNER));
	}

	res = get_uint32_t_from_ldap_msg(ldap_state, entry,
					 LDAP_ATTRIBUTE_TRUST_DIRECTION,
					 &td->trust_direction);
	if (!res) {
		return false;
	}

	res = get_uint32_t_from_ldap_msg(ldap_state, entry,
					 LDAP_ATTRIBUTE_TRUST_ATTRIBUTES,
					 &td->trust_attributes);
	if (!res) {
		return false;
	}

	res = get_uint32_t_from_ldap_msg(ldap_state, entry,
					 LDAP_ATTRIBUTE_TRUST_TYPE,
					 &td->trust_type);
	if (!res) {
		return false;
	}

	get_data_blob_from_ldap_msg(td, ldap_state, entry,
				    LDAP_ATTRIBUTE_TRUST_FOREST_TRUST_INFO,
				    &td->trust_forest_trust_info);

	*_td = td;

	return true;
}

static NTSTATUS ipasam_get_trusted_domain(struct pdb_methods *methods,
					  TALLOC_CTX *mem_ctx,
					  const char *domain,
					  struct pdb_trusted_domain **td)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;

	DEBUG(10, ("ipasam_get_trusted_domain called for domain %s\n", domain));

	if (!get_trusted_domain_by_name_int(ldap_state, talloc_tos(), domain,
					    &entry)) {
		return NT_STATUS_UNSUCCESSFUL;
	}
	if (entry == NULL) {
		DEBUG(5, ("ipasam_get_trusted_domain: no such trusted domain: "
			  "%s\n", domain));
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (!fill_pdb_trusted_domain(mem_ctx, ldap_state, entry, td)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ipasam_get_trusted_domain_by_sid(struct pdb_methods *methods,
						 TALLOC_CTX *mem_ctx,
						 struct dom_sid *sid,
						 struct pdb_trusted_domain **td)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;
	char *sid_str;

	sid_str = sid_string_tos(sid);

	DEBUG(10, ("ipasam_get_trusted_domain_by_sid called for sid %s\n",
		   sid_str));

	if (!get_trusted_domain_by_sid_int(ldap_state, talloc_tos(), sid_str,
					   &entry)) {
		return NT_STATUS_UNSUCCESSFUL;
	}
	if (entry == NULL) {
		DEBUG(5, ("ipasam_get_trusted_domain_by_sid: no trusted domain "
			  "with sid: %s\n", sid_str));
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (!fill_pdb_trusted_domain(mem_ctx, ldap_state, entry, td)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static bool smbldap_make_mod_uint32_t(LDAP *ldap_struct, LDAPMessage *entry,
				      LDAPMod ***mods, const char *attribute,
				      const uint32_t val)
{
	char *dummy;

	dummy = talloc_asprintf(talloc_tos(), "%lu", (unsigned long) val);
	if (dummy == NULL) {
		return false;
	}
	smbldap_make_mod(ldap_struct, entry, mods, attribute, dummy);
	TALLOC_FREE(dummy);

	return true;
}

static NTSTATUS ipasam_set_trusted_domain(struct pdb_methods *methods,
					  const char* domain,
					  const struct pdb_trusted_domain *td)
{
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;
	LDAPMod **mods;
	bool res;
	char *trusted_dn = NULL;
	int ret;

	DEBUG(10, ("ipasam_set_trusted_domain called for domain %s\n", domain));

	res = get_trusted_domain_by_name_int(ldap_state, talloc_tos(), domain,
					     &entry);
	if (!res) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	mods = NULL;
	smbldap_make_mod(priv2ld(ldap_state), entry, &mods, "objectClass",
			 LDAP_OBJ_TRUSTED_DOMAIN);

	if (td->netbios_name != NULL) {
		smbldap_make_mod(priv2ld(ldap_state), entry, &mods,
				 LDAP_ATTRIBUTE_FLAT_NAME,
				 td->netbios_name);
	}

	if (td->domain_name != NULL) {
		smbldap_make_mod(priv2ld(ldap_state), entry, &mods,
				 LDAP_ATTRIBUTE_TRUST_PARTNER,
				 td->domain_name);
	}

	if (!is_null_sid(&td->security_identifier)) {
		smbldap_make_mod(priv2ld(ldap_state), entry, &mods,
				 LDAP_ATTRIBUTE_SECURITY_IDENTIFIER,
				 sid_string_tos(&td->security_identifier));
	}

	if (td->trust_type != 0) {
		res = smbldap_make_mod_uint32_t(priv2ld(ldap_state), entry,
						&mods, LDAP_ATTRIBUTE_TRUST_TYPE,
						td->trust_type);
		if (!res) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	if (td->trust_attributes != 0) {
		res = smbldap_make_mod_uint32_t(priv2ld(ldap_state), entry,
						&mods,
						LDAP_ATTRIBUTE_TRUST_ATTRIBUTES,
						td->trust_attributes);
		if (!res) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	if (td->trust_direction != 0) {
		res = smbldap_make_mod_uint32_t(priv2ld(ldap_state), entry,
						&mods,
						LDAP_ATTRIBUTE_TRUST_DIRECTION,
						td->trust_direction);
		if (!res) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	if (td->trust_auth_outgoing.data != NULL) {
		smbldap_make_mod_blob(priv2ld(ldap_state), entry, &mods,
				      LDAP_ATTRIBUTE_TRUST_AUTH_OUTGOING,
				      &td->trust_auth_outgoing);
	}

	if (td->trust_auth_incoming.data != NULL) {
		smbldap_make_mod_blob(priv2ld(ldap_state), entry, &mods,
				      LDAP_ATTRIBUTE_TRUST_AUTH_INCOMING,
				      &td->trust_auth_incoming);
	}

	if (td->trust_forest_trust_info.data != NULL) {
		smbldap_make_mod_blob(priv2ld(ldap_state), entry, &mods,
				      LDAP_ATTRIBUTE_TRUST_FOREST_TRUST_INFO,
				      &td->trust_forest_trust_info);
	}

	talloc_autofree_ldapmod(talloc_tos(), mods);

	trusted_dn = trusted_domain_dn(ldap_state, domain);
	if (trusted_dn == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	if (entry == NULL) {
		ret = smbldap_add(ldap_state->smbldap_state, trusted_dn, mods);
	} else {
		ret = smbldap_modify(ldap_state->smbldap_state, trusted_dn, mods);
	}

	if (ret != LDAP_SUCCESS) {
		DEBUG(1, ("error writing trusted domain data!\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	return NT_STATUS_OK;
}

static NTSTATUS ipasam_del_trusted_domain(struct pdb_methods *methods,
					   const char *domain)
{
	int ret;
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	LDAPMessage *entry = NULL;
	const char *dn;

	if (!get_trusted_domain_by_name_int(ldap_state, talloc_tos(), domain,
					    &entry)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (entry == NULL) {
		DEBUG(5, ("ipasam_del_trusted_domain: no such trusted domain: "
			  "%s\n", domain));
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	dn = smbldap_talloc_dn(talloc_tos(), priv2ld(ldap_state), entry);
	if (dn == NULL) {
		DEBUG(0,("ipasam_del_trusted_domain: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ret = smbldap_delete(ldap_state->smbldap_state, dn);
	if (ret != LDAP_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ipasam_enum_trusted_domains(struct pdb_methods *methods,
					    TALLOC_CTX *mem_ctx,
					    uint32_t *num_domains,
					    struct pdb_trusted_domain ***domains)
{
	int rc;
	struct ldapsam_privates *ldap_state =
		(struct ldapsam_privates *)methods->private_data;
	char *base_dn = NULL;
	char *filter = NULL;
	int scope = LDAP_SCOPE_SUBTREE;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;

	filter = talloc_asprintf(talloc_tos(), "(objectClass=%s)",
				 LDAP_OBJ_TRUSTED_DOMAIN);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	base_dn = trusted_domain_base_dn(ldap_state);
	if (base_dn == NULL) {
		TALLOC_FREE(filter);
		return NT_STATUS_NO_MEMORY;
	}

	rc = smbldap_search(ldap_state->smbldap_state, base_dn, scope, filter,
			    NULL, 0, &result);
	TALLOC_FREE(filter);
	TALLOC_FREE(base_dn);

	if (result != NULL) {
		talloc_autofree_ldapmsg(mem_ctx, result);
	}

	if (rc == LDAP_NO_SUCH_OBJECT) {
		*num_domains = 0;
		*domains = NULL;
		return NT_STATUS_OK;
	}

	if (rc != LDAP_SUCCESS) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	*num_domains = 0;
	if (!(*domains = TALLOC_ARRAY(mem_ctx, struct pdb_trusted_domain *, 1))) {
		DEBUG(1, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	for (entry = ldap_first_entry(priv2ld(ldap_state), result);
	     entry != NULL;
	     entry = ldap_next_entry(priv2ld(ldap_state), entry))
	{
		struct pdb_trusted_domain *dom_info;

		if (!fill_pdb_trusted_domain(*domains, ldap_state, entry,
					     &dom_info)) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		ADD_TO_ARRAY(*domains, struct pdb_trusted_domain *, dom_info,
			     domains, num_domains);

		if (*domains == NULL) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}
	}

	DEBUG(5, ("ipasam_enum_trusted_domains: got %d domains\n", *num_domains));
	return NT_STATUS_OK;
}

static NTSTATUS ipasam_enum_trusteddoms(struct pdb_methods *methods,
					 TALLOC_CTX *mem_ctx,
					 uint32_t *num_domains,
					 struct trustdom_info ***domains)
{
	NTSTATUS status;
	struct pdb_trusted_domain **td;
	int i;

	status = ipasam_enum_trusted_domains(methods, talloc_tos(),
					     num_domains, &td);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (*num_domains == 0) {
		return NT_STATUS_OK;
	}

	if (!(*domains = TALLOC_ARRAY(mem_ctx, struct trustdom_info *,
				      *num_domains))) {
		DEBUG(1, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < *num_domains; i++) {
		struct trustdom_info *dom_info;

		dom_info = TALLOC_P(*domains, struct trustdom_info);
		if (dom_info == NULL) {
			DEBUG(1, ("talloc failed\n"));
			return NT_STATUS_NO_MEMORY;
		}

		dom_info->name = talloc_steal(mem_ctx, td[i]->netbios_name);
		sid_copy(&dom_info->sid, &td[i]->security_identifier);

		(*domains)[i] = dom_info;
	}

	return NT_STATUS_OK;
}

static uint32_t pdb_ipasam_capabilities(struct pdb_methods *methods)
{
	return PDB_CAP_STORE_RIDS | PDB_CAP_ADS | PDB_CAP_TRUSTED_DOMAINS_EX;
}

static struct pdb_domain_info *pdb_ipasam_get_domain_info(struct pdb_methods *pdb_methods,
							  TALLOC_CTX *mem_ctx)
{
	struct pdb_domain_info *info;
	NTSTATUS status;
	struct ldapsam_privates *ldap_state =
			(struct ldapsam_privates *)pdb_methods->private_data;

	info = talloc(mem_ctx, struct pdb_domain_info);
	if (info == NULL) {
		return NULL;
	}

	info->name = talloc_strdup(info, ldap_state->domain_name);
	if (info->name == NULL) {
		goto fail;
	}

	/* TODO: read dns_domain, dns_forest and guid from LDAP */
	info->dns_domain = talloc_strdup(info, lp_realm());
	if (info->dns_domain == NULL) {
		goto fail;
	}
	strlower_m(info->dns_domain);
	info->dns_forest = talloc_strdup(info, info->dns_domain);
	sid_copy(&info->sid, &ldap_state->domain_sid);

	status = GUID_from_string("testguid", &info->guid);

	return info;

fail:
	TALLOC_FREE(info);
	return NULL;
}

static NTSTATUS modify_ipa_password_exop(struct ldapsam_privates *ldap_state,
					 struct samu *sampass)
{
	int ret;
	BerElement *ber = NULL;
	struct berval *bv = NULL;
	char *retoid = NULL;
	struct berval *retdata = NULL;
	const char *password;
	char *dn;

	password = pdb_get_plaintext_passwd(sampass);
	if (password == NULL || *password == '\0') {
		return NT_STATUS_INVALID_PARAMETER;
	}

	dn = get_account_dn(pdb_get_username(sampass));
	if (dn == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ber = ber_alloc_t( LBER_USE_DER );
	if (ber == NULL) {
		DEBUG(7, ("ber_alloc_t failed.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ret = ber_printf(ber, "{tsts}", LDAP_TAG_EXOP_MODIFY_PASSWD_ID, dn,
			 LDAP_TAG_EXOP_MODIFY_PASSWD_NEW, password);
	if (ret == -1) {
		DEBUG(7, ("ber_printf failed.\n"));
		ber_free(ber, 1);
		return NT_STATUS_UNSUCCESSFUL;
	}

	ret = ber_flatten(ber, &bv);
	ber_free(ber, 1);
	if (ret == -1) {
		DEBUG(1, ("ber_flatten failed.\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	ret = smbldap_extended_operation(ldap_state->smbldap_state,
					 LDAP_EXOP_MODIFY_PASSWD, bv, NULL,
					 NULL, &retoid, &retdata);
	ber_bvfree(bv);
	if (retdata) {
		ber_bvfree(retdata);
	}
	if (retoid) {
		ldap_memfree(retoid);
	}
	if (ret != LDAP_SUCCESS) {
		DEBUG(1, ("smbldap_extended_operation LDAP_EXOP_MODIFY_PASSWD failed.\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

static NTSTATUS ipasam_get_objectclasses(struct ldapsam_privates *ldap_state,
					 const char *dn, LDAPMessage *entry,
					 uint32_t *has_objectclass)
{
	char **objectclasses;
	size_t c;

	objectclasses = ldap_get_values(priv2ld(ldap_state), entry,
					LDAP_ATTRIBUTE_OBJECTCLASS);
	if (objectclasses == NULL) {
		DEBUG(0, ("Entry [%s] does not have any objectclasses.\n", dn));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	*has_objectclass = 0;
	for (c = 0; objectclasses[c] != NULL; c++) {
		if (strequal(objectclasses[c], LDAP_OBJ_KRB_PRINCIPAL)) {
			*has_objectclass |= HAS_KRB_PRINCIPAL;
		} else if (strequal(objectclasses[c],
			   LDAP_OBJ_KRB_PRINCIPAL_AUX)) {
			*has_objectclass |= HAS_KRB_PRINCIPAL_AUX;
		} else if (strequal(objectclasses[c], LDAP_OBJ_IPAOBJECT)) {
			*has_objectclass |= HAS_IPAOBJECT;
		} else if (strequal(objectclasses[c], LDAP_OBJ_IPAHOST)) {
			*has_objectclass |= HAS_IPAHOST;
		} else if (strequal(objectclasses[c], LDAP_OBJ_POSIXACCOUNT)) {
			*has_objectclass |= HAS_POSIXACCOUNT;
		} else if (strequal(objectclasses[c], LDAP_OBJ_GROUPOFNAMES)) {
			*has_objectclass |= HAS_GROUPOFNAMES;
		} else if (strequal(objectclasses[c], LDAP_OBJ_NESTEDGROUP)) {
			*has_objectclass |= HAS_NESTEDGROUP;
		} else if (strequal(objectclasses[c], LDAP_OBJ_IPAUSERGROUP)) {
			*has_objectclass |= HAS_IPAUSERGROUP;
		} else if (strequal(objectclasses[c], LDAP_OBJ_POSIXGROUP)) {
			*has_objectclass |= HAS_POSIXGROUP;
		}
	}
	ldap_value_free(objectclasses);

	return NT_STATUS_OK;
}

enum obj_type {
	IPA_NO_OBJ = 0,
	IPA_USER_OBJ,
	IPA_GROUP_OBJ
};

static NTSTATUS find_obj(struct ldapsam_privates *ldap_state, const char *name,
			 enum obj_type type, char **_dn,
			 uint32_t *_has_objectclass)
{
	int ret;
	char *username;
	char *filter;
	LDAPMessage *result = NULL;
	LDAPMessage *entry = NULL;
	int num_result;
	char *dn;
	uint32_t has_objectclass;
	NTSTATUS status;
	const char *obj_class = NULL;

	switch (type) {
		case IPA_USER_OBJ:
			obj_class = LDAP_OBJ_POSIXACCOUNT;
			break;
		case IPA_GROUP_OBJ:
			obj_class = LDAP_OBJ_POSIXGROUP;
			break;
		default:
			DEBUG(0, ("Unsupported IPA object.\n"));
			return NT_STATUS_INVALID_PARAMETER;
	}

	username = escape_ldap_string(talloc_tos(), name);
	if (username == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	filter = talloc_asprintf(talloc_tos(), "(&(uid=%s)(objectClass=%s))",
				 username, obj_class);
	if (filter == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	TALLOC_FREE(username);

	ret = smbldap_search_suffix(ldap_state->smbldap_state, filter, NULL,
				    &result);
	if (ret != LDAP_SUCCESS) {
		DEBUG(0, ("smbldap_search_suffix failed.\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	num_result = ldap_count_entries(priv2ld(ldap_state), result);

	if (num_result != 1) {
		if (num_result == 0) {
			switch (type) {
				case IPA_USER_OBJ:
					status = NT_STATUS_NO_SUCH_USER;
					break;
				case IPA_GROUP_OBJ:
					status = NT_STATUS_NO_SUCH_GROUP;
					break;
				default:
					status = NT_STATUS_INVALID_PARAMETER;
			}
		} else {
			DEBUG (0, ("find_user: More than one object with name [%s] ?!\n",
				   name));
			status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		goto done;
	}

	entry = ldap_first_entry(priv2ld(ldap_state), result);
	if (!entry) {
		DEBUG(0,("find_user: Out of memory!\n"));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	dn = smbldap_talloc_dn(talloc_tos(), priv2ld(ldap_state), entry);
	if (!dn) {
		DEBUG(0,("find_user: Out of memory!\n"));
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = ipasam_get_objectclasses(ldap_state, dn, entry, &has_objectclass);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	*_dn = dn;
	*_has_objectclass = has_objectclass;

	status = NT_STATUS_OK;

done:
	ldap_msgfree(result);

	return status;
}

static NTSTATUS find_user(struct ldapsam_privates *ldap_state, const char *name,
			  char **_dn, uint32_t *_has_objectclass)
{
	return find_obj(ldap_state, name, IPA_USER_OBJ, _dn, _has_objectclass);
}

static NTSTATUS find_group(struct ldapsam_privates *ldap_state, const char *name,
			   char **_dn, uint32_t *_has_objectclass)
{
	return find_obj(ldap_state, name, IPA_GROUP_OBJ, _dn, _has_objectclass);
}

static NTSTATUS ipasam_add_posix_account_objectclass(struct ldapsam_privates *ldap_state,
						     int ldap_op,
						     const char *dn,
						     const char *username)
{
	int ret;
	LDAPMod **mods = NULL;

	smbldap_set_mod(&mods, LDAP_MOD_ADD,
			"objectclass", "posixAccount");
	smbldap_set_mod(&mods, LDAP_MOD_ADD,
			"cn", username);
	smbldap_set_mod(&mods, LDAP_MOD_ADD,
			"uidNumber", IPA_MAGIC_ID_STR);
	smbldap_set_mod(&mods, LDAP_MOD_ADD,
			"gidNumber", "12345");
	smbldap_set_mod(&mods, LDAP_MOD_ADD,
			"homeDirectory", "/dev/null");

	if (ldap_op == LDAP_MOD_ADD) {
	    ret = smbldap_add(ldap_state->smbldap_state, dn, mods);
	} else {
	    ret = smbldap_modify(ldap_state->smbldap_state, dn, mods);
	}
	ldap_mods_free(mods, 1);
	if (ret != LDAP_SUCCESS) {
		DEBUG(1, ("failed to modify/add user with uid = %s (dn = %s)\n",
			  username, dn));
		return NT_STATUS_LDAP(ret);
	}

	return NT_STATUS_OK;
}

static NTSTATUS ipasam_add_ipa_group_objectclasses(struct ldapsam_privates *ldap_state,
						   const char *dn,
						   const char *name,
						   uint32_t has_objectclass)
{
	LDAPMod **mods = NULL;
	int ret;

	if (!(has_objectclass & HAS_GROUPOFNAMES)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_GROUPOFNAMES);
	}

	if (!(has_objectclass & HAS_NESTEDGROUP)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_NESTEDGROUP);
	}

	if (!(has_objectclass & HAS_IPAUSERGROUP)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_IPAUSERGROUP);
	}

	if (!(has_objectclass & HAS_IPAOBJECT)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_IPAOBJECT);
	}

	if (!(has_objectclass & HAS_POSIXGROUP)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_POSIXGROUP);
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_CN,
				name);
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_GIDNUMBER,
				IPA_MAGIC_ID_STR);
	}

	ret = smbldap_modify(ldap_state->smbldap_state, dn, mods);
	ldap_mods_free(mods, 1);
	if (ret != LDAP_SUCCESS) {
		DEBUG(1, ("failed to modify/add group %s (dn = %s)\n",
			  name, dn));
		return NT_STATUS_LDAP(ret);
	}

	return NT_STATUS_OK;
}

static NTSTATUS ipasam_add_ipa_objectclasses(struct ldapsam_privates *ldap_state,
					     const char *dn, const char *name,
					     const char *domain,
					     uint32_t acct_flags,
					     uint32_t has_objectclass)
{
	LDAPMod **mods = NULL;
	int ret;
	char *princ;

	if (!(has_objectclass & HAS_KRB_PRINCIPAL)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_KRB_PRINCIPAL);

		princ = talloc_asprintf(talloc_tos(), "%s@%s", name, lp_realm());
		if (princ == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		smbldap_set_mod(&mods, LDAP_MOD_ADD, LDAP_ATTRIBUTE_KRB_PRINCIPAL, princ);
	}

	if (!(has_objectclass & HAS_KRB_PRINCIPAL_AUX)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS,
				LDAP_OBJ_KRB_PRINCIPAL_AUX);
	}

	if (!(has_objectclass & HAS_IPAOBJECT)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				LDAP_ATTRIBUTE_OBJECTCLASS, LDAP_OBJ_IPAOBJECT);
	}

	if ((acct_flags != 0) &&
	    (((acct_flags & ACB_NORMAL) && name[strlen(name)-1] == '$') ||
	    ((acct_flags & (ACB_WSTRUST|ACB_SVRTRUST|ACB_DOMTRUST)) != 0))) {

		if (!(has_objectclass & HAS_IPAHOST)) {
			smbldap_set_mod(&mods, LDAP_MOD_ADD,
					LDAP_ATTRIBUTE_OBJECTCLASS,
					LDAP_OBJ_IPAHOST);

			if (domain == NULL) {
				return NT_STATUS_INVALID_PARAMETER;
			}

			smbldap_set_mod(&mods, LDAP_MOD_ADD,
					"fqdn", domain);
		}
	}

	if (!(has_objectclass & HAS_POSIXACCOUNT)) {
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				"objectclass", "posixAccount");
		smbldap_set_mod(&mods, LDAP_MOD_ADD, "cn", name);
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				"uidNumber", IPA_MAGIC_ID_STR);
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				"gidNumber", "12345");
		smbldap_set_mod(&mods, LDAP_MOD_ADD,
				"homeDirectory", "/dev/null");
	}

	if (mods != NULL) {
		ret = smbldap_modify(ldap_state->smbldap_state, dn, mods);
		ldap_mods_free(mods, 1);
		if (ret != LDAP_SUCCESS) {
			DEBUG(1, ("failed to modify/add user with uid = %s (dn = %s)\n",
				  name, dn));
			return NT_STATUS_LDAP(ret);
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ipasam_add_sam_account(struct pdb_methods *pdb_methods,
					   struct samu *sampass)
{
	NTSTATUS status;
	struct ldapsam_privates *ldap_state;
	const char *name;
	char *dn;
	uint32_t has_objectclass;
	uint32_t rid;
	struct dom_sid user_sid;

	ldap_state = (struct ldapsam_privates *)(pdb_methods->private_data);

	if (IS_SAM_SET(sampass, PDB_USERSID) ||
	    IS_SAM_CHANGED(sampass, PDB_USERSID)) {
		if (!pdb_new_rid(&rid)) {
			return NT_STATUS_DS_NO_MORE_RIDS;
		}
		sid_compose(&user_sid, get_global_sam_sid(), rid);
		if (!pdb_set_user_sid(sampass, &user_sid, PDB_SET)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	status = ldap_state->ipasam_privates->ldapsam_add_sam_account(pdb_methods,
								      sampass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (ldap_state->ipasam_privates->server_is_ipa) {
		name = pdb_get_username(sampass);
		if (name == NULL || *name == '\0') {
			return NT_STATUS_INVALID_PARAMETER;
		}

		status = find_user(ldap_state, name, &dn, &has_objectclass);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		status = ipasam_add_ipa_objectclasses(ldap_state, dn, name,
						      pdb_get_domain(sampass),
						      pdb_get_acct_ctrl(sampass),
						      has_objectclass);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		if (!(has_objectclass & HAS_POSIXACCOUNT)) {
			status = ipasam_add_posix_account_objectclass(ldap_state,
								      LDAP_MOD_REPLACE,
								      dn, name);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}

		if (pdb_get_init_flags(sampass, PDB_PLAINTEXT_PW) == PDB_CHANGED) {
			status = modify_ipa_password_exop(ldap_state, sampass);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_ipasam_update_sam_account(struct pdb_methods *pdb_methods,
					      struct samu *sampass)
{
	NTSTATUS status;
	struct ldapsam_privates *ldap_state;
	ldap_state = (struct ldapsam_privates *)(pdb_methods->private_data);

	status = ldap_state->ipasam_privates->ldapsam_update_sam_account(pdb_methods,
									 sampass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (ldap_state->ipasam_privates->server_is_ipa) {
		if (pdb_get_init_flags(sampass, PDB_PLAINTEXT_PW) == PDB_CHANGED) {
			status = modify_ipa_password_exop(ldap_state, sampass);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS ipasam_create_dom_group(struct pdb_methods *pdb_methods,
					TALLOC_CTX *tmp_ctx, const char *name,
					uint32_t *rid)
{
	NTSTATUS status;
	struct ldapsam_privates *ldap_state;
	int ldap_op = LDAP_MOD_REPLACE;
	char *dn;
	uint32_t has_objectclass = 0;

	ldap_state = (struct ldapsam_privates *)(pdb_methods->private_data);

	if (name == NULL || *name == '\0') {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = find_group(ldap_state, name, &dn, &has_objectclass);
	if (NT_STATUS_IS_OK(status)) {
		ldap_op = LDAP_MOD_REPLACE;
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)) {
		ldap_op = LDAP_MOD_ADD;
	} else {
		return status;
	}

	if (!(has_objectclass & HAS_POSIXGROUP)) {
		status = ipasam_add_ipa_group_objectclasses(ldap_state, dn,
							    name,
							    has_objectclass);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	status = ldap_state->ipasam_privates->ldapsam_create_dom_group(pdb_methods,
								       tmp_ctx,
								       name,
								       rid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}
static NTSTATUS ipasam_create_user(struct pdb_methods *pdb_methods,
				   TALLOC_CTX *tmp_ctx, const char *name,
				   uint32_t acb_info, uint32_t *rid)
{
	NTSTATUS status;
	struct ldapsam_privates *ldap_state;
	int ldap_op = LDAP_MOD_REPLACE;
	char *dn;
	uint32_t has_objectclass = 0;

	ldap_state = (struct ldapsam_privates *)(pdb_methods->private_data);

	if (name == NULL || *name == '\0') {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = find_user(ldap_state, name, &dn, &has_objectclass);
	if (NT_STATUS_IS_OK(status)) {
		ldap_op = LDAP_MOD_REPLACE;
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)) {
		char *escape_username;
		ldap_op = LDAP_MOD_ADD;
		escape_username = escape_rdn_val_string_alloc(name);
		if (!escape_username) {
			return NT_STATUS_NO_MEMORY;
		}
		if (name[strlen(name)-1] == '$') {
			dn = talloc_asprintf(tmp_ctx, "uid=%s,%s",
					     escape_username,
					     lp_ldap_machine_suffix());
		} else {
			dn = talloc_asprintf(tmp_ctx, "uid=%s,%s",
					     escape_username,
					     lp_ldap_user_suffix());
		}
		SAFE_FREE(escape_username);
		if (!dn) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		return status;
	}

	if (!(has_objectclass & HAS_POSIXACCOUNT)) {
		status = ipasam_add_posix_account_objectclass(ldap_state, ldap_op,
							      dn, name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		has_objectclass |= HAS_POSIXACCOUNT;
	}

	status = ldap_state->ipasam_privates->ldapsam_create_user(pdb_methods,
								  tmp_ctx, name,
								  acb_info, rid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = ipasam_add_ipa_objectclasses(ldap_state, dn, name, lp_realm(),
					      acb_info, has_objectclass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

static NTSTATUS pdb_init_IPA_ldapsam(struct pdb_methods **pdb_method, const char *location)
{
	struct ldapsam_privates *ldap_state;
	NTSTATUS status;

	status = pdb_init_ldapsam(pdb_method, location);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	(*pdb_method)->name = "IPA_ldapsam";

	ldap_state = (struct ldapsam_privates *)((*pdb_method)->private_data);
	ldap_state->ipasam_privates = talloc_zero(ldap_state,
						  struct ipasam_privates);
	if (ldap_state->ipasam_privates == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	ldap_state->is_ipa_ldap = True;

	ldap_state->ipasam_privates->server_is_ipa = smbldap_has_extension(
					priv2ld(ldap_state), IPA_KEYTAB_SET_OID);
	ldap_state->ipasam_privates->ldapsam_add_sam_account = (*pdb_method)->add_sam_account;
	ldap_state->ipasam_privates->ldapsam_update_sam_account = (*pdb_method)->update_sam_account;
	ldap_state->ipasam_privates->ldapsam_create_user = (*pdb_method)->create_user;
	ldap_state->ipasam_privates->ldapsam_create_dom_group = (*pdb_method)->create_dom_group;

	(*pdb_method)->add_sam_account = pdb_ipasam_add_sam_account;
	(*pdb_method)->update_sam_account = pdb_ipasam_update_sam_account;

	if (lp_parm_bool(-1, "ldapsam", "trusted", False)) {
		if (lp_parm_bool(-1, "ldapsam", "editposix", False)) {
			(*pdb_method)->create_user = ipasam_create_user;
			(*pdb_method)->create_dom_group = ipasam_create_dom_group;
		}
	}

	(*pdb_method)->capabilities = pdb_ipasam_capabilities;
	(*pdb_method)->get_domain_info = pdb_ipasam_get_domain_info;

	(*pdb_method)->get_trusteddom_pw = ipasam_get_trusteddom_pw;
	(*pdb_method)->set_trusteddom_pw = ipasam_set_trusteddom_pw;
	(*pdb_method)->del_trusteddom_pw = ipasam_del_trusteddom_pw;
	(*pdb_method)->enum_trusteddoms = ipasam_enum_trusteddoms;

	(*pdb_method)->get_trusted_domain = ipasam_get_trusted_domain;
	(*pdb_method)->get_trusted_domain_by_sid = ipasam_get_trusted_domain_by_sid;
	(*pdb_method)->set_trusted_domain = ipasam_set_trusted_domain;
	(*pdb_method)->del_trusted_domain = ipasam_del_trusted_domain;
	(*pdb_method)->enum_trusted_domains = ipasam_enum_trusted_domains;

	return NT_STATUS_OK;
}

NTSTATUS pdb_ipa_init(void)
{
	NTSTATUS nt_status;

	if (!NT_STATUS_IS_OK(nt_status = smb_register_passdb(PASSDB_INTERFACE_VERSION, "IPA_ldapsam", pdb_init_IPA_ldapsam)))
		return nt_status;

	return NT_STATUS_OK;
}
