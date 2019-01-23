/* 
   Unix SMB/CIFS implementation.
   
   Extract the user/system database from a remote SamSync server

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Volker Lendecke 2004
   
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
#include "libnet/libnet.h"
#include "libcli/ldap/ldap_ndr.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "../lib/util/util_ldb.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "ldb_wrap.h"
#include "libcli/security/security.h"
#include "param/param.h"

struct samsync_ldb_secret {
	struct samsync_ldb_secret *prev, *next;
	DATA_BLOB secret;
	char *name;
	NTTIME mtime;
};

struct samsync_ldb_trusted_domain {
	struct samsync_ldb_trusted_domain *prev, *next;
        struct dom_sid *sid;
	char *name;
};

struct samsync_ldb_state {
	/* Values from the LSA lookup */
	const struct libnet_SamSync_state *samsync_state;

	struct dom_sid *dom_sid[3];
	struct ldb_context *sam_ldb, *remote_ldb, *pdb;
	struct ldb_dn *base_dn[3];
	struct samsync_ldb_secret *secrets;
	struct samsync_ldb_trusted_domain *trusted_domains;
};

/* This wrapper is needed for the "ADD_OR_DEL" macros */
static int samdb_msg_add_string(struct ldb_context *sam_ldb,
				TALLOC_CTX *mem_ctx, struct ldb_message *msg,
				const char *attr_name, const char *str)
{
	return ldb_msg_add_string(msg, attr_name, str);
}

static NTSTATUS samsync_ldb_add_foreignSecurityPrincipal(TALLOC_CTX *mem_ctx,
							 struct samsync_ldb_state *state,
							 struct dom_sid *sid,
							 struct ldb_dn **fsp_dn,
							 char **error_string)
{
	const char *sidstr = dom_sid_string(mem_ctx, sid);
	/* We assume that ForeignSecurityPrincipals are under the BASEDN of the main domain */
	struct ldb_dn *basedn = samdb_search_dn(state->sam_ldb, mem_ctx,
						state->base_dn[SAM_DATABASE_DOMAIN],
						"(&(objectClass=container)(cn=ForeignSecurityPrincipals))");
	struct ldb_message *msg;
	int ret;

	if (!sidstr) {
		return NT_STATUS_NO_MEMORY;
	}

	if (basedn == NULL) {
		*error_string = talloc_asprintf(mem_ctx, 
						"Failed to find DN for "
						"ForeignSecurityPrincipal container under %s",
						ldb_dn_get_linearized(state->base_dn[SAM_DATABASE_DOMAIN]));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	
	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* add core elements to the ldb_message for the alias */
	msg->dn = basedn;
	if ( ! ldb_dn_add_child_fmt(msg->dn, "CN=%s", sidstr))
		return NT_STATUS_UNSUCCESSFUL;
	
	ldb_msg_add_string(msg, "objectClass", "foreignSecurityPrincipal");

	*fsp_dn = msg->dn;

	/* create the alias */
	ret = ldb_add(state->sam_ldb, msg);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to create foreignSecurityPrincipal "
						"record %s: %s",
						ldb_dn_get_linearized(msg->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_domain(TALLOC_CTX *mem_ctx,
					  struct samsync_ldb_state *state,
					  enum netr_SamDatabaseID database,
					  struct netr_DELTA_ENUM *delta,
					  char **error_string) 
{
	struct netr_DELTA_DOMAIN *domain = delta->delta_union.domain;
	const char *domain_name = domain->domain_name.string;
	struct ldb_message *msg;
	int ret;
	
	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (database == SAM_DATABASE_DOMAIN) {
		struct ldb_dn *partitions_basedn;
		const char *domain_attrs[] =  {"nETBIOSName", "nCName", NULL};
		struct ldb_message **msgs_domain;
		int ret_domain;

		partitions_basedn = samdb_partitions_dn(state->sam_ldb, mem_ctx);

		ret_domain = gendb_search(state->sam_ldb, mem_ctx, partitions_basedn, &msgs_domain, domain_attrs,
					  "(&(&(nETBIOSName=%s)(objectclass=crossRef))(ncName=*))", 
					  domain_name);
		if (ret_domain == -1) {
			*error_string = talloc_asprintf(mem_ctx, "gendb_search for domain failed: %s", ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		
		if (ret_domain != 1) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to find existing domain record for %s: %d results", domain_name,
							ret_domain);
			return NT_STATUS_NO_SUCH_DOMAIN;		
		}

		state->base_dn[database] = samdb_result_dn(state->sam_ldb, state, msgs_domain[0], "nCName", NULL);

		if (state->dom_sid[database]) {
			/* Update the domain sid with the incoming
			 * domain (found on LSA pipe, database sid may
			 * be random) */
			samdb_msg_add_dom_sid(state->sam_ldb, mem_ctx, 
					      msg, "objectSid", state->dom_sid[database]);
		} else {
			/* Well, we will have to use the one from the database */
			state->dom_sid[database] = samdb_search_dom_sid(state->sam_ldb, state,
									state->base_dn[database], 
									"objectSid", NULL);
		}

		if (state->samsync_state->domain_guid) {
			struct ldb_val v;
			NTSTATUS status;
			status = GUID_to_ndr_blob(state->samsync_state->domain_guid, msg, &v);
			if (!NT_STATUS_IS_OK(status)) {
				*error_string = talloc_asprintf(mem_ctx, "ndr_push of domain GUID failed!");
				return status;
			}
			
			ldb_msg_add_value(msg, "objectGUID", &v, NULL);
		}
	} else if (database == SAM_DATABASE_BUILTIN) {
		/* work out the builtin_dn - useful for so many calls its worth
		   fetching here */
		const char *dnstring = samdb_search_string(state->sam_ldb, mem_ctx, NULL,
							   "distinguishedName", "objectClass=builtinDomain");
		state->base_dn[database] = ldb_dn_new(state, state->sam_ldb, dnstring);
		if ( ! ldb_dn_validate(state->base_dn[database])) {
			return NT_STATUS_INTERNAL_ERROR;
		}
	} else {
		/* PRIVs DB */
		return NT_STATUS_INVALID_PARAMETER;
	}

	msg->dn = talloc_reference(mem_ctx, state->base_dn[database]);
	if (!msg->dn) {
		return NT_STATUS_NO_MEMORY;
	}

	ldb_msg_add_string(msg, "oEMInformation",
			   domain->oem_information.string);

	samdb_msg_add_int64(state->sam_ldb, mem_ctx, 
			    msg, "forceLogoff", domain->force_logoff_time);

	samdb_msg_add_uint(state->sam_ldb, mem_ctx, 
			   msg, "minPwdLen", domain->min_password_length);

	samdb_msg_add_int64(state->sam_ldb, mem_ctx, 
			    msg, "maxPwdAge", domain->max_password_age);

	samdb_msg_add_int64(state->sam_ldb, mem_ctx, 
			    msg, "minPwdAge", domain->min_password_age);

	samdb_msg_add_uint(state->sam_ldb, mem_ctx, 
			   msg, "pwdHistoryLength", domain->password_history_length);

	samdb_msg_add_uint64(state->sam_ldb, mem_ctx, 
			     msg, "modifiedCount", 
			     domain->sequence_num);

	samdb_msg_add_uint64(state->sam_ldb, mem_ctx, 
			     msg, "creationTime", domain->domain_create_time);

	/* TODO: Account lockout, password properties */
	
	ret = dsdb_replace(state->sam_ldb, msg, 0);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx,
						"Failed to modify domain record %s: %s",
						ldb_dn_get_linearized(msg->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_user(TALLOC_CTX *mem_ctx,
					struct samsync_ldb_state *state,
					enum netr_SamDatabaseID database,
					struct netr_DELTA_ENUM *delta,
					char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_USER *user = delta->delta_union.user;
	const char *container, *obj_class;
	char *cn_name;
	int cn_name_len;
	const struct dom_sid *user_sid;
	struct ldb_message *msg;
	struct ldb_message **msgs;
	struct ldb_message **remote_msgs = NULL;
	unsigned int i;
	int ret;
	uint32_t acb;
	bool add = false;
	const char *attrs[] = { NULL };
	/* we may change this to a global search, then fill in only the things not in ldap later */
	const char *remote_attrs[] = { "userPrincipalName", "servicePrincipalName", 
				       "msDS-KeyVersionNumber", "objectGUID", NULL};

	user_sid = dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid);
	if (!user_sid) {
		return NT_STATUS_NO_MEMORY;
	}

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	msg->dn = NULL;
	/* search for the user, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database],
			   &msgs, attrs, "(&(objectClass=user)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, user_sid));

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "LDB for user %s failed: %s", 
						dom_sid_string(mem_ctx, user_sid),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		add = true;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one user with SID: %s in local LDB", 
						dom_sid_string(mem_ctx, user_sid));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else {
		msg->dn = msgs[0]->dn;
		talloc_steal(msg, msgs[0]->dn);
	}

	/* and do the same on the remote database */
	if (state->remote_ldb) {
		ret = gendb_search(state->remote_ldb, mem_ctx, state->base_dn[database],
				   &remote_msgs, remote_attrs, "(&(objectClass=user)(objectSid=%s))", 
				   ldap_encode_ndr_dom_sid(mem_ctx, user_sid));
		
		if (ret == -1) {
			*error_string = talloc_asprintf(mem_ctx, "remote LDAP for user %s failed: %s", 
							dom_sid_string(mem_ctx, user_sid),
							ldb_errstring(state->remote_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		} else if (ret == 0) {
			*error_string = talloc_asprintf(mem_ctx, "User exists in samsync but not in remote LDAP domain! (base: %s, SID: %s)", 
							ldb_dn_get_linearized(state->base_dn[database]),
							dom_sid_string(mem_ctx, user_sid));
			return NT_STATUS_NO_SUCH_USER;
		} else if (ret > 1) {
			*error_string = talloc_asprintf(mem_ctx, "More than one user in remote LDAP domain with SID: %s", 
							dom_sid_string(mem_ctx, user_sid));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
			
			/* Try to put things in the same location as the remote server */
		} else if (add) {
			msg->dn = talloc_steal(msg, remote_msgs[0]->dn);
		}
	}

	cn_name   = talloc_strdup(mem_ctx, user->account_name.string);
	NT_STATUS_HAVE_NO_MEMORY(cn_name);
	cn_name_len = strlen(cn_name);

#define ADD_OR_DEL(type, attrib, field) do {				\
		if (user->field) {					\
			samdb_msg_add_ ## type(state->sam_ldb, mem_ctx, msg, \
					       attrib, user->field);	\
		} else if (!add) {					\
			samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg, \
					     attrib);			\
		}							\
        } while (0);

        ADD_OR_DEL(string, "samAccountName", account_name.string);
        ADD_OR_DEL(string, "displayName", full_name.string);

	if (samdb_msg_add_dom_sid(state->sam_ldb, mem_ctx, msg, 
				  "objectSid", dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))) {
		return NT_STATUS_NO_MEMORY; 
	}

        ADD_OR_DEL(uint, "primaryGroupID", primary_gid);
        ADD_OR_DEL(string, "homeDirectory", home_directory.string);
        ADD_OR_DEL(string, "homeDrive", home_drive.string);
        ADD_OR_DEL(string, "scriptPath", logon_script.string);
	ADD_OR_DEL(string, "description", description.string);
	ADD_OR_DEL(string, "userWorkstations", workstations.string);

	ADD_OR_DEL(uint64, "lastLogon", last_logon);
	ADD_OR_DEL(uint64, "lastLogoff", last_logoff);

	if (samdb_msg_add_logon_hours(state->sam_ldb, mem_ctx, msg, "logonHours", &user->logon_hours) != 0) { 
		return NT_STATUS_NO_MEMORY; 
	}

	ADD_OR_DEL(uint, "badPwdCount", bad_password_count);
	ADD_OR_DEL(uint, "logonCount", logon_count);

	ADD_OR_DEL(uint64, "pwdLastSet", last_password_change);
	ADD_OR_DEL(uint64, "accountExpires", acct_expiry);
	
	if (samdb_msg_add_acct_flags(state->sam_ldb, mem_ctx, msg, 
				     "userAccountControl", user->acct_flags) != 0) { 
		return NT_STATUS_NO_MEMORY; 
	} 
	
	if (user->lm_password_present) {
		samdb_msg_add_hash(state->sam_ldb, mem_ctx, msg,  
				   "dBCSPwd", &user->lmpassword);
	} else if (!add) {
		samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg,  
				     "dBCSPwd"); 
	}
	if (user->nt_password_present) {
		samdb_msg_add_hash(state->sam_ldb, mem_ctx, msg,  
				   "unicodePwd", &user->ntpassword);
	} else if (!add) {
		samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg,  
				     "unicodePwd"); 
	}
	    
	ADD_OR_DEL(string, "comment", comment.string);

	if (samdb_msg_add_parameters(state->sam_ldb, mem_ctx, msg, "userParameters", &user->parameters) != 0) {
		return NT_STATUS_NO_MEMORY;
	}

	ADD_OR_DEL(uint, "countryCode", country_code);
	ADD_OR_DEL(uint, "codePage", code_page);

        ADD_OR_DEL(string, "profilePath", profile_path.string);

#undef ADD_OR_DEL

	for (i=0; remote_attrs[i]; i++) {
		struct ldb_message_element *el = ldb_msg_find_element(remote_msgs[0], remote_attrs[i]);
		if (!el) {
			samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg,  
					     remote_attrs[i]); 
		} else {
			ldb_msg_add(msg, el, LDB_FLAG_MOD_REPLACE);
		}
	}

	acb = user->acct_flags;
	if (acb & (ACB_WSTRUST)) {
		cn_name[cn_name_len - 1] = '\0';
		container = "Computers";
		obj_class = "computer";
		
	} else if (acb & ACB_SVRTRUST) {
		if (cn_name[cn_name_len - 1] != '$') {
			return NT_STATUS_FOOBAR;		
		}
		cn_name[cn_name_len - 1] = '\0';
		container = "Domain Controllers";
		obj_class = "computer";
	} else {
		container = "Users";
		obj_class = "user";
	}
	if (add) {
		ldb_msg_add_string(msg, "objectClass", obj_class);
		if (!msg->dn) {
			msg->dn = ldb_dn_copy(mem_ctx, state->base_dn[database]);
			ldb_dn_add_child_fmt(msg->dn, "CN=%s,CN=%s", cn_name, container);
			if (!msg->dn) {
				return NT_STATUS_NO_MEMORY;		
			}
		}

		ret = ldb_add(state->sam_ldb, msg);
		if (ret != LDB_SUCCESS) {
			struct ldb_dn *first_try_dn = msg->dn;
			/* Try again with the default DN */
			if (!remote_msgs) {
				*error_string = talloc_asprintf(mem_ctx, "Failed to create user record.  Tried %s: %s",
								ldb_dn_get_linearized(first_try_dn),
								ldb_errstring(state->sam_ldb));
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			} else {
				msg->dn = talloc_steal(msg, remote_msgs[0]->dn);
				ret = ldb_add(state->sam_ldb, msg);
				if (ret != LDB_SUCCESS) {
					*error_string = talloc_asprintf(mem_ctx, "Failed to create user record.  Tried both %s and %s: %s",
									ldb_dn_get_linearized(first_try_dn),
									ldb_dn_get_linearized(msg->dn),
									ldb_errstring(state->sam_ldb));
					return NT_STATUS_INTERNAL_DB_CORRUPTION;
				}
			}
		}
	} else {
		ret = dsdb_replace(state->sam_ldb, msg, 0);
		if (ret != LDB_SUCCESS) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to modify user record %s: %s",
							ldb_dn_get_linearized(msg->dn),
							ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_delete_user(TALLOC_CTX *mem_ctx,
					struct samsync_ldb_state *state,
					enum netr_SamDatabaseID database,
					struct netr_DELTA_ENUM *delta,
					char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct ldb_message **msgs;
	int ret;
	const char *attrs[] = { NULL };

	/* search for the user, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database],
			   &msgs, attrs, "(&(objectClass=user)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		return NT_STATUS_NO_SUCH_USER;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one user with SID: %s", 
						dom_sid_string(mem_ctx, 
							       dom_sid_add_rid(mem_ctx, 
									       state->dom_sid[database], 
									       rid)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_delete(state->sam_ldb, msgs[0]->dn);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to delete user record %s: %s",
						ldb_dn_get_linearized(msgs[0]->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_group(TALLOC_CTX *mem_ctx,
					 struct samsync_ldb_state *state,
					 enum netr_SamDatabaseID database,
					 struct netr_DELTA_ENUM *delta,
					 char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_GROUP *group = delta->delta_union.group;
	const char *container, *obj_class;
	const char *cn_name;

	struct ldb_message *msg;
	struct ldb_message **msgs;
	int ret;
	bool add = false;
	const char *attrs[] = { NULL };

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* search for the group, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
			   "(&(objectClass=group)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		add = true;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one group/alias with SID: %s", 
						dom_sid_string(mem_ctx, 
							       dom_sid_add_rid(mem_ctx, 
									       state->dom_sid[database], 
									       rid)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else {
		msg->dn = talloc_steal(msg, msgs[0]->dn);
	}

	cn_name   = group->group_name.string;

#define ADD_OR_DEL(type, attrib, field) do {				\
		if (group->field) {					\
			samdb_msg_add_ ## type(state->sam_ldb, mem_ctx, msg, \
					       attrib, group->field);	\
		} else if (!add) {					\
			samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg, \
					     attrib);			\
		}							\
        } while (0);

        ADD_OR_DEL(string, "samAccountName", group_name.string);

	if (samdb_msg_add_dom_sid(state->sam_ldb, mem_ctx, msg, 
				  "objectSid", dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))) {
		return NT_STATUS_NO_MEMORY; 
	}

	ADD_OR_DEL(string, "description", description.string);

#undef ADD_OR_DEL

	container = "Users";
	obj_class = "group";

	if (add) {
		ldb_msg_add_string(msg, "objectClass", obj_class);
		msg->dn = ldb_dn_copy(mem_ctx, state->base_dn[database]);
		ldb_dn_add_child_fmt(msg->dn, "CN=%s,CN=%s", cn_name, container);
		if (!msg->dn) {
			return NT_STATUS_NO_MEMORY;		
		}

		ret = ldb_add(state->sam_ldb, msg);
		if (ret != LDB_SUCCESS) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to create group record %s: %s",
							ldb_dn_get_linearized(msg->dn),
							ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	} else {
		ret = dsdb_replace(state->sam_ldb, msg, 0);
		if (ret != LDB_SUCCESS) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to modify group record %s: %s",
							ldb_dn_get_linearized(msg->dn),
							ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_delete_group(TALLOC_CTX *mem_ctx,
					 struct samsync_ldb_state *state,
					 enum netr_SamDatabaseID database,
					 struct netr_DELTA_ENUM *delta,
					 char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct ldb_message **msgs;
	int ret;
	const char *attrs[] = { NULL };

	/* search for the group, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
			   "(&(objectClass=group)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		return NT_STATUS_NO_SUCH_GROUP;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one group/alias with SID: %s", 
						dom_sid_string(mem_ctx, 
							       dom_sid_add_rid(mem_ctx, 
									       state->dom_sid[database], 
									       rid)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	
	ret = ldb_delete(state->sam_ldb, msgs[0]->dn);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to delete group record %s: %s",
						ldb_dn_get_linearized(msgs[0]->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_group_member(TALLOC_CTX *mem_ctx,
						struct samsync_ldb_state *state,
						enum netr_SamDatabaseID database,
						struct netr_DELTA_ENUM *delta,
						char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_GROUP_MEMBER *group_member = delta->delta_union.group_member;
	struct ldb_message *msg;
	struct ldb_message **msgs;
	int ret;
	const char *attrs[] = { NULL };
	const char *str_dn;
	uint32_t i;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* search for the group, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
			   "(&(objectClass=group)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		return NT_STATUS_NO_SUCH_GROUP;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one group/alias with SID: %s", 
						dom_sid_string(mem_ctx, 
							       dom_sid_add_rid(mem_ctx, 
									       state->dom_sid[database], 
									       rid)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else {
		msg->dn = talloc_steal(msg, msgs[0]->dn);
	}
	
	talloc_free(msgs);

	for (i=0; i<group_member->num_rids; i++) {
		/* search for the group, by rid */
		ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
				   "(&(objectClass=user)(objectSid=%s))", 
				   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], group_member->rids[i]))); 
		
		if (ret == -1) {
			*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		} else if (ret == 0) {
			return NT_STATUS_NO_SUCH_USER;
		} else if (ret > 1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		} else {
			str_dn = ldb_dn_alloc_linearized(msg, msgs[0]->dn);
			NT_STATUS_HAVE_NO_MEMORY(str_dn);
			ret = ldb_msg_add_string(msg, "member", str_dn);
			if (ret != LDB_SUCCESS) return NT_STATUS_NO_MEMORY;
		}
		
		talloc_free(msgs);
	}
	
	ret = dsdb_replace(state->sam_ldb, msg, 0);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to modify group record %s: %s",
						ldb_dn_get_linearized(msg->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_alias(TALLOC_CTX *mem_ctx,
					 struct samsync_ldb_state *state,
					 enum netr_SamDatabaseID database,
					 struct netr_DELTA_ENUM *delta,
					 char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_ALIAS *alias = delta->delta_union.alias;
	const char *container, *obj_class;
	const char *cn_name;

	struct ldb_message *msg;
	struct ldb_message **msgs;
	int ret;
	bool add = false;
	const char *attrs[] = { NULL };

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* search for the alias, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
			   "(&(objectClass=group)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		add = true;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one group/alias with SID: %s", 
						dom_sid_string(mem_ctx, 
							       dom_sid_add_rid(mem_ctx, 
									       state->dom_sid[database], 
									       rid)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else {
		msg->dn = talloc_steal(mem_ctx, msgs[0]->dn);
	}

	cn_name   = alias->alias_name.string;

#define ADD_OR_DEL(type, attrib, field) do {				\
		if (alias->field) {					\
			samdb_msg_add_ ## type(state->sam_ldb, mem_ctx, msg, \
					       attrib, alias->field);	\
		} else if (!add) {					\
			samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg, \
					     attrib);			\
		}							\
	} while (0);

	ADD_OR_DEL(string, "samAccountName", alias_name.string);

	if (samdb_msg_add_dom_sid(state->sam_ldb, mem_ctx, msg, 
				  "objectSid", dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))) {
		return NT_STATUS_NO_MEMORY; 
	}

	ADD_OR_DEL(string, "description", description.string);

#undef ADD_OR_DEL

	samdb_msg_add_uint(state->sam_ldb, mem_ctx, msg, "groupType", 0x80000004);

	container = "Users";
	obj_class = "group";

	if (add) {
		ldb_msg_add_string(msg, "objectClass", obj_class);
		msg->dn = ldb_dn_copy(mem_ctx, state->base_dn[database]);
		ldb_dn_add_child_fmt(msg->dn, "CN=%s,CN=%s", cn_name, container);
		if (!msg->dn) {
			return NT_STATUS_NO_MEMORY;		
		}

		ret = ldb_add(state->sam_ldb, msg);
		if (ret != LDB_SUCCESS) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to create alias record %s: %s",
							ldb_dn_get_linearized(msg->dn),
							ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	} else {
		ret = dsdb_replace(state->sam_ldb, msg, 0);
		if (ret != LDB_SUCCESS) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to modify alias record %s: %s",
							ldb_dn_get_linearized(msg->dn),
							ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_delete_alias(TALLOC_CTX *mem_ctx,
					 struct samsync_ldb_state *state,
					 enum netr_SamDatabaseID database,
					 struct netr_DELTA_ENUM *delta,
					 char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct ldb_message **msgs;
	int ret;
	const char *attrs[] = { NULL };

	/* search for the alias, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
			   "(&(objectClass=group)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		return NT_STATUS_NO_SUCH_ALIAS;
	} else if (ret > 1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	ret = ldb_delete(state->sam_ldb, msgs[0]->dn);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to delete alias record %s: %s",
						ldb_dn_get_linearized(msgs[0]->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_alias_member(TALLOC_CTX *mem_ctx,
						struct samsync_ldb_state *state,
						enum netr_SamDatabaseID database,
						struct netr_DELTA_ENUM *delta,
						char **error_string) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_ALIAS_MEMBER *alias_member = delta->delta_union.alias_member;
	struct ldb_message *msg;
	struct ldb_message **msgs;
	int ret;
	const char *attrs[] = { NULL };
	uint32_t i;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* search for the alias, by rid */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[database], &msgs, attrs,
			   "(&(objectClass=group)(objectSid=%s))", 
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid_add_rid(mem_ctx, state->dom_sid[database], rid))); 
		
	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		return NT_STATUS_NO_SUCH_GROUP;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one group/alias with SID: %s", 
						dom_sid_string(mem_ctx, 
							       dom_sid_add_rid(mem_ctx, 
									       state->dom_sid[database], 
									       rid)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else {
		msg->dn = talloc_steal(msg, msgs[0]->dn);
	}
	
	talloc_free(msgs);

	for (i=0; i<alias_member->sids.num_sids; i++) {
		struct ldb_dn *alias_member_dn;
		const char *str_dn;
		/* search for members, in the top basedn (normal users are builtin aliases) */
		ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[SAM_DATABASE_DOMAIN], &msgs, attrs,
				   "(objectSid=%s)", 
				   ldap_encode_ndr_dom_sid(mem_ctx, alias_member->sids.sids[i].sid)); 

		if (ret == -1) {
			*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		} else if (ret == 0) {
			NTSTATUS nt_status;
			nt_status = samsync_ldb_add_foreignSecurityPrincipal(mem_ctx, state,
									     alias_member->sids.sids[i].sid, 
									     &alias_member_dn, 
									     error_string);
			if (!NT_STATUS_IS_OK(nt_status)) {
				return nt_status;
			}
		} else if (ret > 1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		} else {
			alias_member_dn = msgs[0]->dn;
		}
		str_dn = ldb_dn_alloc_linearized(msg, alias_member_dn);
		NT_STATUS_HAVE_NO_MEMORY(str_dn);
		ret = ldb_msg_add_string(msg, "member", str_dn);
		if (ret != LDB_SUCCESS) return NT_STATUS_NO_MEMORY;
	
		talloc_free(msgs);
	}

	ret = dsdb_replace(state->sam_ldb, msg, 0);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to modify group record %s: %s",
						ldb_dn_get_linearized(msg->dn),
						ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_handle_account(TALLOC_CTX *mem_ctx,
					   struct samsync_ldb_state *state,
					   enum netr_SamDatabaseID database,
					   struct netr_DELTA_ENUM *delta,
					   char **error_string) 
{
	struct dom_sid *sid = delta->delta_id_union.sid;
	struct netr_DELTA_ACCOUNT *account = delta->delta_union.account;

	struct ldb_message *msg;
	int ret;
	uint32_t i;
	char *dnstr, *sidstr;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sidstr = dom_sid_string(msg, sid);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(sidstr, msg);

	dnstr = talloc_asprintf(msg, "sid=%s", sidstr);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(dnstr, msg);

	msg->dn = ldb_dn_new(msg, state->pdb, dnstr);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg->dn, msg);

	for (i=0; i< account->privilege_entries; i++) {
		ldb_msg_add_string(msg, "privilege", account->privilege_name[i].string);
	}

	ret = dsdb_replace(state->pdb, msg, 0);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		if (samdb_msg_add_dom_sid(state->pdb, msg, msg, "objectSid", sid) != LDB_SUCCESS) {
			talloc_free(msg);
			return NT_STATUS_NO_MEMORY;
		}
		ldb_msg_add_string(msg, "comment", "added via samsync");
		ret = ldb_add(state->pdb, msg);		
	}

	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to modify privilege record %s",
						ldb_dn_get_linearized(msg->dn));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS samsync_ldb_delete_account(TALLOC_CTX *mem_ctx,
					   struct samsync_ldb_state *state,
					   enum netr_SamDatabaseID database,
					   struct netr_DELTA_ENUM *delta,
					   char **error_string) 
{
	struct dom_sid *sid = delta->delta_id_union.sid;

	struct ldb_message *msg;
	struct ldb_message **msgs;
	int ret;
	const char *attrs[] = { NULL };

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* search for the account, by sid, in the top basedn */
	ret = gendb_search(state->sam_ldb, mem_ctx, state->base_dn[SAM_DATABASE_DOMAIN], &msgs, attrs,
			   "(objectSid=%s)", 
			   ldap_encode_ndr_dom_sid(mem_ctx, sid)); 

	if (ret == -1) {
		*error_string = talloc_asprintf(mem_ctx, "gendb_search failed: %s", ldb_errstring(state->sam_ldb));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (ret == 0) {
		return NT_STATUS_NO_SUCH_USER;
	} else if (ret > 1) {
		*error_string = talloc_asprintf(mem_ctx, "More than one account with SID: %s", 
						dom_sid_string(mem_ctx, sid));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else {
		msg->dn = talloc_steal(msg, msgs[0]->dn);
	}

	samdb_msg_add_delete(state->sam_ldb, mem_ctx, msg,  
			     "privilege");

	ret = dsdb_replace(state->sam_ldb, msg, 0);
	if (ret != LDB_SUCCESS) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to modify privilege record %s",
						ldb_dn_get_linearized(msg->dn));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

static NTSTATUS libnet_samsync_ldb_fn(TALLOC_CTX *mem_ctx, 		
				      void *private_data,
				      enum netr_SamDatabaseID database,
				      struct netr_DELTA_ENUM *delta,
				      char **error_string)
{
	NTSTATUS nt_status = NT_STATUS_OK;
	struct samsync_ldb_state *state = talloc_get_type(private_data, struct samsync_ldb_state);

	*error_string = NULL;
	switch (delta->delta_type) {
	case NETR_DELTA_DOMAIN:
	{
		nt_status = samsync_ldb_handle_domain(mem_ctx, 
						      state,
						      database,
						      delta,
						      error_string);
		break;
	}
	case NETR_DELTA_USER:
	{
		nt_status = samsync_ldb_handle_user(mem_ctx, 
						    state,
						    database,
						    delta,
						    error_string);
		break;
	}
	case NETR_DELTA_DELETE_USER:
	{
		nt_status = samsync_ldb_delete_user(mem_ctx, 
						    state,
						    database,
						    delta,
						    error_string);
		break;
	}
	case NETR_DELTA_GROUP:
	{
		nt_status = samsync_ldb_handle_group(mem_ctx, 
						     state,
						     database,
						     delta,
						     error_string);
		break;
	}
	case NETR_DELTA_DELETE_GROUP:
	{
		nt_status = samsync_ldb_delete_group(mem_ctx, 
						     state,
						     database,
						     delta,
						     error_string);
		break;
	}
	case NETR_DELTA_GROUP_MEMBER:
	{
		nt_status = samsync_ldb_handle_group_member(mem_ctx, 
							    state,
							    database,
							    delta,
							    error_string);
		break;
	}
	case NETR_DELTA_ALIAS:
	{
		nt_status = samsync_ldb_handle_alias(mem_ctx, 
						     state,
						     database,
						     delta,
						     error_string);
		break;
	}
	case NETR_DELTA_DELETE_ALIAS:
	{
		nt_status = samsync_ldb_delete_alias(mem_ctx, 
						     state,
						     database,
						     delta,
						     error_string);
		break;
	}
	case NETR_DELTA_ALIAS_MEMBER:
	{
		nt_status = samsync_ldb_handle_alias_member(mem_ctx, 
							    state,
							    database,
							    delta,
							    error_string);
		break;
	}
	case NETR_DELTA_ACCOUNT:
	{
		nt_status = samsync_ldb_handle_account(mem_ctx, 
						       state,
						       database,
						       delta,
						       error_string);
		break;
	}
	case NETR_DELTA_DELETE_ACCOUNT:
	{
		nt_status = samsync_ldb_delete_account(mem_ctx, 
						       state,
						       database,
						       delta,
						       error_string);
		break;
	}
	default:
		/* Can't dump them all right now */
		break;
	}
	if (!NT_STATUS_IS_OK(nt_status) && !*error_string) {
		*error_string = talloc_asprintf(mem_ctx, "Failed to handle samsync delta: %s", nt_errstr(nt_status));
	}
	return nt_status;
}

static NTSTATUS libnet_samsync_ldb_init(TALLOC_CTX *mem_ctx, 		
					void *private_data,
					struct libnet_SamSync_state *samsync_state,
					char **error_string)
{
	struct samsync_ldb_state *state = talloc_get_type(private_data, struct samsync_ldb_state);
	const char *server = dcerpc_server_name(samsync_state->netlogon_pipe);
	char *ldap_url;

	state->samsync_state = samsync_state;

	ZERO_STRUCT(state->dom_sid);
	if (state->samsync_state->domain_sid) {
		state->dom_sid[SAM_DATABASE_DOMAIN] = dom_sid_dup(state, state->samsync_state->domain_sid);
	}

	state->dom_sid[SAM_DATABASE_BUILTIN] = dom_sid_parse_talloc(state, SID_BUILTIN);

	if (state->samsync_state->realm) {
		if (!server || !*server) {
			/* huh?  how do we not have a server name?  */
			*error_string = talloc_strdup(mem_ctx, "No DCE/RPC server name available.  How did we connect?");
			return NT_STATUS_INVALID_PARAMETER;
		}
		ldap_url = talloc_asprintf(state, "ldap://%s", server);
		
		state->remote_ldb = ldb_wrap_connect(mem_ctx, 
						     NULL,
						     state->samsync_state->machine_net_ctx->lp_ctx,
						     ldap_url, 
						     NULL, state->samsync_state->machine_net_ctx->cred,
						     0);
		if (!state->remote_ldb) {
			*error_string = talloc_asprintf(mem_ctx, "Failed to connect to remote LDAP server at %s (used to extract additional data in SamSync replication)", ldap_url);
			return NT_STATUS_NO_LOGON_SERVERS;
		}
	} else {
		state->remote_ldb = NULL;
	}
	return NT_STATUS_OK;
}

NTSTATUS libnet_samsync_ldb(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_samsync_ldb *r)
{
	NTSTATUS nt_status;
	struct libnet_SamSync r2;
	struct samsync_ldb_state *state = talloc(mem_ctx, struct samsync_ldb_state);

	if (!state) {
		return NT_STATUS_NO_MEMORY;
	}

	state->secrets         = NULL;
	state->trusted_domains = NULL;

	state->sam_ldb         = samdb_connect(mem_ctx, 
					       ctx->event_ctx,
					       ctx->lp_ctx,
					       r->in.session_info,
						   0);
	if (!state->sam_ldb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	state->pdb             = privilege_connect(mem_ctx, 
						   ctx->lp_ctx);
	if (!state->pdb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	r2.out.error_string    = NULL;
	r2.in.binding_string   = r->in.binding_string;
	r2.in.init_fn          = libnet_samsync_ldb_init;
	r2.in.delta_fn         = libnet_samsync_ldb_fn;
	r2.in.fn_ctx           = state;
	r2.in.machine_account  = NULL; /* TODO:  Create a machine account, fill this in, and the delete it */
	nt_status              = libnet_SamSync_netlogon(ctx, state, &r2);
	r->out.error_string    = r2.out.error_string;
	talloc_steal(mem_ctx, r->out.error_string);

	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(state);
		return nt_status;
	}
	talloc_free(state);
	return nt_status;
}
