/*
   Unix SMB/CIFS implementation.

   Helpers to add users and groups to the DB

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2010
   Copyright (C) Matthias Dieter Wallnöfer 2009

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
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "../libds/common/flags.h"
#include "libcli/security/security.h"

#include "libds/common/flag_mapping.h"

/* Add a user, SAMR style, including the correct transaction
 * semantics.  Used by the SAMR server and by pdb_samba4 */
NTSTATUS dsdb_add_user(struct ldb_context *ldb,
		       TALLOC_CTX *mem_ctx,
		       const char *account_name,
		       uint32_t acct_flags,
		       struct dom_sid **sid,
		       struct ldb_dn **dn)
{
	const char *name;
	struct ldb_message *msg;
	int ret;
	const char *container, *obj_class=NULL;
	char *cn_name;
	size_t cn_name_len;

	const char *attrs[] = {
		"objectSid",
		"userAccountControl",
		NULL
	};

	uint32_t user_account_control;
	struct ldb_dn *account_dn;
	struct dom_sid *account_sid;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	/*
	 * Start a transaction, so we can query and do a subsequent atomic
	 * modify
	 */

	ret = ldb_transaction_start(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to start a transaction for user creation: %s\n",
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* check if the user already exists */
	name = samdb_search_string(ldb, tmp_ctx, NULL,
				   "sAMAccountName",
				   "(&(sAMAccountName=%s)(objectclass=user))",
				   ldb_binary_encode_string(tmp_ctx, account_name));
	if (name != NULL) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_USER_EXISTS;
	}

	cn_name = talloc_strdup(tmp_ctx, account_name);
	if (!cn_name) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	cn_name_len = strlen(cn_name);
	if (cn_name_len < 1) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_PARAMETER;
	}

	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* This must be one of these values *only* */
	if (acct_flags == ACB_NORMAL) {
		container = "CN=Users";
		obj_class = "user";

	} else if (acct_flags == ACB_WSTRUST) {
		if (cn_name[cn_name_len - 1] != '$') {
			ldb_transaction_cancel(ldb);
			return NT_STATUS_FOOBAR;
		}
		cn_name[cn_name_len - 1] = '\0';
		container = "CN=Computers";
		obj_class = "computer";

	} else if (acct_flags == ACB_SVRTRUST) {
		if (cn_name[cn_name_len - 1] != '$') {
			ldb_transaction_cancel(ldb);
			return NT_STATUS_FOOBAR;
		}
		cn_name[cn_name_len - 1] = '\0';
		container = "OU=Domain Controllers";
		obj_class = "computer";
	} else {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* add core elements to the ldb_message for the user */
	msg->dn = ldb_dn_copy(msg, ldb_get_default_basedn(ldb));
	if ( ! ldb_dn_add_child_fmt(msg->dn, "CN=%s,%s", cn_name, container)) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_FOOBAR;
	}

	ldb_msg_add_string(msg, "sAMAccountName", account_name);
	ldb_msg_add_string(msg, "objectClass", obj_class);

	/* create the user */
	ret = ldb_add(ldb, msg);
	switch (ret) {
	case LDB_SUCCESS:
		break;
	case LDB_ERR_ENTRY_ALREADY_EXISTS:
		ldb_transaction_cancel(ldb);
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_USER_EXISTS;
	case LDB_ERR_UNWILLING_TO_PERFORM:
	case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		ldb_transaction_cancel(ldb);
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_ACCESS_DENIED;
	default:
		ldb_transaction_cancel(ldb);
		DEBUG(0,("Failed to create user record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	account_dn = msg->dn;

	/* retrieve the sid and account control bits for the user just created */
	ret = dsdb_search_one(ldb, tmp_ctx, &msg,
			      account_dn, LDB_SCOPE_BASE, attrs, 0, NULL);

	if (ret != LDB_SUCCESS) {
		ldb_transaction_cancel(ldb);
		DEBUG(0,("Can't locate the account we just created %s: %s\n",
			 ldb_dn_get_linearized(account_dn), ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	account_sid = samdb_result_dom_sid(tmp_ctx, msg, "objectSid");
	if (account_sid == NULL) {
		ldb_transaction_cancel(ldb);
		DEBUG(0,("Apparently we failed to get the objectSid of the just created account record %s\n",
			 ldb_dn_get_linearized(msg->dn)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* Change the account control to be the correct account type.
	 * The default is for a workstation account */
	user_account_control = ldb_msg_find_attr_as_uint(msg, "userAccountControl", 0);
	user_account_control = (user_account_control &
				~(UF_NORMAL_ACCOUNT |
				  UF_INTERDOMAIN_TRUST_ACCOUNT |
				  UF_WORKSTATION_TRUST_ACCOUNT |
				  UF_SERVER_TRUST_ACCOUNT));
	user_account_control |= ds_acb2uf(acct_flags);

	talloc_free(msg);
	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = account_dn;

	if (samdb_msg_add_uint(ldb, tmp_ctx, msg,
			       "userAccountControl",
			       user_account_control) != LDB_SUCCESS) {
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* modify the samdb record */
	ret = dsdb_replace(ldb, msg, 0);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to modify account record %s to set userAccountControl: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		ldb_transaction_cancel(ldb);
		talloc_free(tmp_ctx);

		/* we really need samdb.c to return NTSTATUS */
		return NT_STATUS_UNSUCCESSFUL;
	}

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,("Failed to commit transaction to add and modify account record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	*dn = talloc_steal(mem_ctx, account_dn);
	*sid = talloc_steal(mem_ctx, account_sid);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/*
  called by samr_CreateDomainGroup and pdb_samba4
*/
NTSTATUS dsdb_add_domain_group(struct ldb_context *ldb,
			       TALLOC_CTX *mem_ctx,
			       const char *groupname,
			       struct dom_sid **sid,
			       struct ldb_dn **dn)
{
	const char *name;
	struct ldb_message *msg;
	struct dom_sid *group_sid;
	int ret;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	/* check if the group already exists */
	name = samdb_search_string(ldb, tmp_ctx, NULL,
				   "sAMAccountName",
				   "(&(sAMAccountName=%s)(objectclass=group))",
				   ldb_binary_encode_string(tmp_ctx, groupname));
	if (name != NULL) {
		return NT_STATUS_GROUP_EXISTS;
	}

	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* add core elements to the ldb_message for the user */
	msg->dn = ldb_dn_copy(tmp_ctx, ldb_get_default_basedn(ldb));
	ldb_dn_add_child_fmt(msg->dn, "CN=%s,CN=Users", groupname);
	if (!msg->dn) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	ldb_msg_add_string(msg, "sAMAccountName", groupname);
	ldb_msg_add_string(msg, "objectClass", "group");

	/* create the group */
	ret = ldb_add(ldb, msg);
	switch (ret) {
	case  LDB_SUCCESS:
		break;
	case  LDB_ERR_ENTRY_ALREADY_EXISTS:
		DEBUG(0,("Failed to create group record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_GROUP_EXISTS;
	case  LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		DEBUG(0,("Failed to create group record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_ACCESS_DENIED;
	default:
		DEBUG(0,("Failed to create group record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* retrieve the sid for the group just created */
	group_sid = samdb_search_dom_sid(ldb, tmp_ctx,
					 msg->dn, "objectSid", NULL);
	if (group_sid == NULL) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	*dn = talloc_steal(mem_ctx, msg->dn);
	*sid = talloc_steal(mem_ctx, group_sid);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

NTSTATUS dsdb_add_domain_alias(struct ldb_context *ldb,
			       TALLOC_CTX *mem_ctx,
			       const char *alias_name,
			       struct dom_sid **sid,
			       struct ldb_dn **dn)
{
	const char *name;
	struct ldb_message *msg;
	struct dom_sid *alias_sid;
	int ret;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	/* Check if alias already exists */
	name = samdb_search_string(ldb, tmp_ctx, NULL,
				   "sAMAccountName",
				   "(sAMAccountName=%s)(objectclass=group))",
				   ldb_binary_encode_string(mem_ctx, alias_name));

	if (name != NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_ALIAS_EXISTS;
	}

	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* add core elements to the ldb_message for the alias */
	msg->dn = ldb_dn_copy(mem_ctx, ldb_get_default_basedn(ldb));
	ldb_dn_add_child_fmt(msg->dn, "CN=%s,CN=Users", alias_name);
	if (!msg->dn) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ldb_msg_add_string(msg, "sAMAccountName", alias_name);
	ldb_msg_add_string(msg, "objectClass", "group");
	samdb_msg_add_int(ldb, mem_ctx, msg, "groupType", GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);

	/* create the alias */
	ret = ldb_add(ldb, msg);
	switch (ret) {
	case LDB_SUCCESS:
		break;
	case LDB_ERR_ENTRY_ALREADY_EXISTS:
		talloc_free(tmp_ctx);
		return NT_STATUS_ALIAS_EXISTS;
	case LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS:
		talloc_free(tmp_ctx);
		return NT_STATUS_ACCESS_DENIED;
	default:
		DEBUG(0,("Failed to create alias record %s: %s\n",
			 ldb_dn_get_linearized(msg->dn),
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* retrieve the sid for the alias just created */
	alias_sid = samdb_search_dom_sid(ldb, tmp_ctx,
					 msg->dn, "objectSid", NULL);

	*dn = talloc_steal(mem_ctx, msg->dn);
	*sid = talloc_steal(mem_ctx, alias_sid);
	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

/* Return the members of this group (which may be a domain group or an alias) */
NTSTATUS dsdb_enum_group_mem(struct ldb_context *ldb,
			     TALLOC_CTX *mem_ctx,
			     struct ldb_dn *dn,
			     struct dom_sid **members_out,
			     unsigned int *pnum_members)
{
	struct ldb_message *msg;
	unsigned int i, j;
	int ret;
	struct dom_sid *members;
	struct ldb_message_element *member_el;
	const char *attrs[] = { "member", NULL };
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	ret = dsdb_search_one(ldb, tmp_ctx, &msg, dn, LDB_SCOPE_BASE, attrs,
			      DSDB_SEARCH_SHOW_EXTENDED_DN, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	if (ret != LDB_SUCCESS) {
		DEBUG(1, ("dsdb_enum_group_mem: dsdb_search for %s failed: %s\n",
			  ldb_dn_get_linearized(dn), ldb_errstring(ldb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	member_el = ldb_msg_find_element(msg, "member");
	if (!member_el) {
		*members_out = NULL;
		*pnum_members = 0;
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	members = talloc_array(mem_ctx, struct dom_sid, member_el->num_values);
	if (members == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	j = 0;
	for (i=0; i <member_el->num_values; i++) {
		struct ldb_dn *member_dn = ldb_dn_from_ldb_val(tmp_ctx, ldb,
							       &member_el->values[i]);
		if (!member_dn || !ldb_dn_validate(member_dn)) {
			DEBUG(1, ("Could not parse %*.*s as a DN\n",
				  (int)member_el->values[i].length,
				  (int)member_el->values[i].length,
				  (const char *)member_el->values[i].data));
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		status = dsdb_get_extended_dn_sid(member_dn, &members[j],
						  "SID");
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			/* If we fail finding a SID then this is no error since
			 * it could be a non SAM object - e.g. a contact */
			continue;
		} else if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("When parsing DN '%s' we failed to parse it's SID component, so we cannot fetch the membership: %s\n",
				  ldb_dn_get_extended_linearized(tmp_ctx, member_dn, 1),
				  nt_errstr(status)));
			talloc_free(tmp_ctx);
			return status;
		}

		++j;
	}

	*members_out = talloc_steal(mem_ctx, members);
	*pnum_members = j;
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

NTSTATUS dsdb_lookup_rids(struct ldb_context *ldb,
			  TALLOC_CTX *mem_ctx,
			  const struct dom_sid *domain_sid,
			  unsigned int num_rids,
			  uint32_t *rids,
			  const char **names,
			  enum lsa_SidType *lsa_attrs)
{
	const char *attrs[] = { "sAMAccountType", "sAMAccountName", NULL };
	unsigned int i, num_mapped;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	num_mapped = 0;

	for (i=0; i<num_rids; i++) {
		struct ldb_message *msg;
		struct ldb_dn *dn;
		uint32_t attr;
		int rc;

		lsa_attrs[i] = SID_NAME_UNKNOWN;

		dn = ldb_dn_new_fmt(tmp_ctx, ldb, "<SID=%s>",
				    dom_sid_string(tmp_ctx,
						   dom_sid_add_rid(tmp_ctx, domain_sid,
								   rids[i])));
		if (dn == NULL) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		rc = dsdb_search_one(ldb, tmp_ctx, &msg, dn, LDB_SCOPE_BASE, attrs, 0, "samAccountName=*");
		if (rc == LDB_ERR_NO_SUCH_OBJECT) {
			continue;
		} else if (rc != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		names[i] = ldb_msg_find_attr_as_string(msg, "samAccountName", NULL);
		if (names[i] == NULL) {
			DEBUG(10, ("no samAccountName\n"));
			continue;
		}
		talloc_steal(names, names[i]);
		attr = ldb_msg_find_attr_as_uint(msg, "samAccountType", 0);
		lsa_attrs[i] = ds_atype_map(attr);
		if (lsa_attrs[i] == SID_NAME_UNKNOWN) {
			continue;
		}
		num_mapped += 1;
	}
	talloc_free(tmp_ctx);

	if (num_mapped == 0) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (num_mapped < num_rids) {
		return STATUS_SOME_UNMAPPED;
	}
	return NT_STATUS_OK;
}

