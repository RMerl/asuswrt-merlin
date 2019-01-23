/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Jelmer Vernooij 2008
 *  Copyright (C) Wilco Baan Hofman 2008-2010
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
#include "param/param.h"
#include <ldb.h>
#include "lib/ldb-samba/ldb_wrap.h"
#include "auth/credentials/credentials.h"
#include "../librpc/gen_ndr/nbt.h"
#include "libcli/libcli.h"
#include "libnet/libnet.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "../libcli/security/security.h"
#include "libcli/ldap/ldap_ndr.h"
#include "../lib/talloc/talloc.h"
#include "lib/policy/policy.h"

struct gpo_stringmap {
	const char *str;
	uint32_t flags;
};
static const struct gpo_stringmap gplink_options [] = {
	{ "GPLINK_OPT_DISABLE", GPLINK_OPT_DISABLE },
	{ "GPLINK_OPT_ENFORCE", GPLINK_OPT_ENFORCE },
	{ NULL, 0 }
};
static const struct gpo_stringmap gpo_flags [] = {
	{ "GPO_FLAG_USER_DISABLE", GPO_FLAG_USER_DISABLE },
	{ "GPO_FLAG_MACHINE_DISABLE", GPO_FLAG_MACHINE_DISABLE },
	{ NULL, 0 }
};
static const struct gpo_stringmap gpo_inheritance [] = {
	{ "GPO_INHERIT", GPO_INHERIT },
	{ "GPO_BLOCK_INHERITANCE", GPO_BLOCK_INHERITANCE },
	{ NULL, 0 }
};


static NTSTATUS parse_gpo(TALLOC_CTX *mem_ctx, struct ldb_message *msg, struct gp_object **ret)
{
	struct gp_object *gpo = talloc(mem_ctx, struct gp_object);
	enum ndr_err_code ndr_err;
	const DATA_BLOB *data;

	NT_STATUS_HAVE_NO_MEMORY(gpo);

	gpo->dn = talloc_strdup(mem_ctx, ldb_dn_get_linearized(msg->dn));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->dn, gpo);

	DEBUG(9, ("Parsing GPO LDAP data for %s\n", gpo->dn));

	gpo->display_name = talloc_strdup(gpo, ldb_msg_find_attr_as_string(msg, "displayName", ""));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->display_name, gpo);

	gpo->name = talloc_strdup(gpo, ldb_msg_find_attr_as_string(msg, "name", ""));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->name, gpo);

	gpo->flags = ldb_msg_find_attr_as_uint(msg, "flags", 0);
	gpo->version = ldb_msg_find_attr_as_uint(msg, "versionNumber", 0);

	gpo->file_sys_path = talloc_strdup(gpo, ldb_msg_find_attr_as_string(msg, "gPCFileSysPath", ""));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->file_sys_path, gpo);

	/* Pull the security descriptor through the NDR library */
	data = ldb_msg_find_ldb_val(msg, "nTSecurityDescriptor");
	gpo->security_descriptor = talloc(gpo, struct security_descriptor);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->security_descriptor, gpo);

	ndr_err = ndr_pull_struct_blob(data,
			mem_ctx,
			gpo->security_descriptor,
			(ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	*ret = gpo;
	return NT_STATUS_OK;
}

NTSTATUS gp_get_gpo_flags(TALLOC_CTX *mem_ctx, uint32_t flags, const char ***ret)
{
	unsigned int i, count=0;
	const char **flag_strs = talloc_array(mem_ctx, const char *, 1);

	NT_STATUS_HAVE_NO_MEMORY(flag_strs);

	flag_strs[0] = NULL;

	for (i = 0; gpo_flags[i].str != NULL; i++) {
		if (flags & gpo_flags[i].flags) {
			flag_strs = talloc_realloc(mem_ctx, flag_strs, const char *, count+2);
			NT_STATUS_HAVE_NO_MEMORY(flag_strs);
			flag_strs[count] = gpo_flags[i].str;
			flag_strs[count+1] = NULL;
			count++;
		}
	}
	*ret = flag_strs;
	return NT_STATUS_OK;
}

NTSTATUS gp_get_gplink_options(TALLOC_CTX *mem_ctx, uint32_t options, const char ***ret)
{
	unsigned int i, count=0;
	const char **flag_strs = talloc_array(mem_ctx, const char *, 1);

	NT_STATUS_HAVE_NO_MEMORY(flag_strs);
	flag_strs[0] = NULL;

	for (i = 0; gplink_options[i].str != NULL; i++) {
		if (options & gplink_options[i].flags) {
			flag_strs = talloc_realloc(mem_ctx, flag_strs, const char *, count+2);
			NT_STATUS_HAVE_NO_MEMORY(flag_strs);
			flag_strs[count] = gplink_options[i].str;
			flag_strs[count+1] = NULL;
			count++;
		}
	}
	*ret = flag_strs;
	return NT_STATUS_OK;
}

NTSTATUS gp_init(TALLOC_CTX *mem_ctx,
		struct loadparm_context *lp_ctx,
		struct cli_credentials *credentials,
		struct tevent_context *ev_ctx,
		struct gp_context **gp_ctx)
{

	struct libnet_LookupDCs *io;
	char *url;
	struct libnet_context *net_ctx;
	struct ldb_context *ldb_ctx;
	NTSTATUS rv;

	/* Initialise the libnet context */
	net_ctx = libnet_context_init(ev_ctx, lp_ctx);
	net_ctx->cred = credentials;

	/* Prepare libnet lookup structure for looking a DC (PDC is correct). */
	io = talloc_zero(mem_ctx, struct libnet_LookupDCs);
	NT_STATUS_HAVE_NO_MEMORY(io);
	io->in.name_type = NBT_NAME_PDC;
	io->in.domain_name = lpcfg_workgroup(lp_ctx);

	/* Find Active DC's */
	rv = libnet_LookupDCs(net_ctx, mem_ctx, io);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to lookup DCs in domain\n"));
		return rv;
	}

	/* Connect to ldap://DC_NAME with all relevant contexts*/
	url = talloc_asprintf(mem_ctx, "ldap://%s", io->out.dcs[0].name);
	NT_STATUS_HAVE_NO_MEMORY(url);
	ldb_ctx = ldb_wrap_connect(mem_ctx, net_ctx->event_ctx, lp_ctx,
			url, NULL, net_ctx->cred, 0);
	if (ldb_ctx == NULL) {
		DEBUG(0, ("Can't connect to DC's LDAP with url %s\n", url));
		return NT_STATUS_UNSUCCESSFUL;
	}


	*gp_ctx = talloc_zero(mem_ctx, struct gp_context);
	NT_STATUS_HAVE_NO_MEMORY(gp_ctx);

	(*gp_ctx)->lp_ctx = lp_ctx;
	(*gp_ctx)->credentials = credentials;
	(*gp_ctx)->ev_ctx = ev_ctx;
	(*gp_ctx)->ldb_ctx = ldb_ctx;
	(*gp_ctx)->active_dc = io->out.dcs[0];

	/* We don't need to keep the libnet context */
	talloc_free(net_ctx);
	return NT_STATUS_OK;
}

NTSTATUS gp_list_all_gpos(struct gp_context *gp_ctx, struct gp_object ***ret)
{
	struct ldb_result *result;
	int rv;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct ldb_dn *dn;
	struct gp_object **gpo;
	unsigned int i; /* same as in struct ldb_result */
	const char **attrs;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* Create full ldb dn of the policies base object */
	dn = ldb_get_default_basedn(gp_ctx->ldb_ctx);
	rv = ldb_dn_add_child(dn, ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, "CN=Policies,CN=System"));
	if (!rv) {
		DEBUG(0, ("Can't append subtree to DN\n"));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(10, ("Searching for policies in DN: %s\n", ldb_dn_get_linearized(dn)));

	attrs = talloc_array(mem_ctx, const char *, 7);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(attrs, mem_ctx);

	attrs[0] = "nTSecurityDescriptor";
	attrs[1] = "versionNumber";
	attrs[2] = "flags";
	attrs[3] = "name";
	attrs[4] = "displayName";
	attrs[5] = "gPCFileSysPath";
	attrs[6] = NULL;

	rv = ldb_search(gp_ctx->ldb_ctx, mem_ctx, &result, dn, LDB_SCOPE_ONELEVEL, attrs, "(objectClass=groupPolicyContainer)");
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv), ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	gpo = talloc_array(gp_ctx, struct gp_object *, result->count+1);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo, mem_ctx);

	gpo[result->count] = NULL;

	for (i = 0; i < result->count; i++) {
		status = parse_gpo(gp_ctx, result->msgs[i], &gpo[i]);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to parse GPO.\n"));
			talloc_free(mem_ctx);
			return status;
		}
	}

	talloc_free(mem_ctx);

	*ret = gpo;
	return NT_STATUS_OK;
}

NTSTATUS gp_get_gpo_info(struct gp_context *gp_ctx, const char *dn_str, struct gp_object **ret)
{
	struct ldb_result *result;
	struct ldb_dn *dn;
	struct gp_object *gpo;
	int rv;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	const char **attrs;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* Create an ldb dn struct for the dn string */
	dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);

	attrs = talloc_array(mem_ctx, const char *, 7);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(attrs, mem_ctx);

	attrs[0] = "nTSecurityDescriptor";
	attrs[1] = "versionNumber";
	attrs[2] = "flags";
	attrs[3] = "name";
	attrs[4] = "displayName";
	attrs[5] = "gPCFileSysPath";
	attrs[6] = NULL;

	rv = ldb_search(gp_ctx->ldb_ctx,
			mem_ctx,
			&result,
			dn,
			LDB_SCOPE_BASE,
			attrs,
			"objectClass=groupPolicyContainer");
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv), ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* We expect exactly one record */
	if (result->count != 1) {
		DEBUG(0, ("Could not find GPC with dn %s\n", dn_str));
		talloc_free(mem_ctx);
		return NT_STATUS_NOT_FOUND;
	}

	status = parse_gpo(gp_ctx, result->msgs[0], &gpo);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to parse GPO.\n"));
		talloc_free(mem_ctx);
		return status;
	}

	talloc_free(mem_ctx);

	*ret = gpo;
	return NT_STATUS_OK;
}

static NTSTATUS parse_gplink (TALLOC_CTX *mem_ctx, const char *gplink_str, struct gp_link ***ret)
{
	int start, idx=0;
	int pos;
	struct gp_link **gplinks;
	char *buf, *end;
	const char *gplink_start = "[LDAP://";

	gplinks = talloc_array(mem_ctx, struct gp_link *, 1);
	NT_STATUS_HAVE_NO_MEMORY(gplinks);

	gplinks[0] = NULL;

	/* Assuming every gPLink starts with "[LDAP://" */
	start = strlen(gplink_start);

	for (pos = start; pos < strlen(gplink_str); pos++) {
		if (gplink_str[pos] == ';') {
			gplinks = talloc_realloc(mem_ctx, gplinks, struct gp_link *, idx+2);
			NT_STATUS_HAVE_NO_MEMORY(gplinks);
			gplinks[idx] = talloc(mem_ctx, struct gp_link);
			NT_STATUS_HAVE_NO_MEMORY(gplinks[idx]);
			gplinks[idx]->dn = talloc_strndup(mem_ctx,
			                                  gplink_str + start,
			                                  pos - start);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gplinks[idx]->dn, gplinks);

			for (start = pos + 1; gplink_str[pos] != ']'; pos++);

			buf = talloc_strndup(gplinks, gplink_str + start, pos - start);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(buf, gplinks);
			gplinks[idx]->options = (uint32_t) strtoll(buf, &end, 0);
			talloc_free(buf);

			/* Set the last entry in the array to be NULL */
			gplinks[idx + 1] = NULL;

			/* Increment the array index, the string position past
			   the next "[LDAP://", and set the start reference */
			idx++;
			pos += strlen(gplink_start)+1;
			start = pos;
		}
	}

	*ret = gplinks;
	return NT_STATUS_OK;
}


NTSTATUS gp_get_gplinks(struct gp_context *gp_ctx, const char *dn_str, struct gp_link ***ret)
{
	TALLOC_CTX *mem_ctx;
	struct ldb_dn *dn;
	struct ldb_result *result;
	struct gp_link **gplinks;
	char *gplink_str;
	int rv;
	unsigned int i, j;
	NTSTATUS status;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);

	rv = ldb_search(gp_ctx->ldb_ctx, mem_ctx, &result, dn, LDB_SCOPE_BASE, NULL, "(objectclass=*)");
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv), ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	for (i = 0; i < result->count; i++) {
		for (j = 0; j < result->msgs[i]->num_elements; j++) {
			struct ldb_message_element *element = &result->msgs[i]->elements[j];

			if (strcmp(element->name, "gPLink") == 0) {
				SMB_ASSERT(element->num_values > 0);
				gplink_str = talloc_strdup(mem_ctx, (char *) element->values[0].data);
				NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gplink_str, mem_ctx);
				goto found;
			}
		}
	}
	gplink_str = talloc_strdup(mem_ctx, "");
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gplink_str, mem_ctx);

	found:

	status = parse_gplink(gp_ctx, gplink_str, &gplinks);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to parse gPLink\n"));
		return status;
	}

	talloc_free(mem_ctx);

	*ret = gplinks;
	return NT_STATUS_OK;
}

NTSTATUS gp_list_gpos(struct gp_context *gp_ctx, struct security_token *token, const char ***ret)
{
	TALLOC_CTX *mem_ctx;
	const char **gpos;
	struct ldb_result *result;
	char *sid;
	struct ldb_dn *dn;
	struct ldb_message_element *element;
	bool inherit;
	const char *attrs[] = { "objectClass", NULL };
	int rv;
	NTSTATUS status;
	unsigned int count = 0;
	unsigned int i;
	enum {
		ACCOUNT_TYPE_USER = 0,
		ACCOUNT_TYPE_MACHINE = 1
	} account_type;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	sid = ldap_encode_ndr_dom_sid(mem_ctx,
				      &token->sids[PRIMARY_USER_SID_INDEX]);
	NT_STATUS_HAVE_NO_MEMORY(sid);

	/* Find the user DN and objectclass via the sid from the security token */
	rv = ldb_search(gp_ctx->ldb_ctx,
			mem_ctx,
			&result,
			ldb_get_default_basedn(gp_ctx->ldb_ctx),
			LDB_SCOPE_SUBTREE,
			attrs,
			"(&(objectclass=user)(objectSid=%s))", sid);
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv),
				ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	if (result->count != 1) {
		DEBUG(0, ("Could not find user with sid %s.\n", sid));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	DEBUG(10,("Found DN for this user: %s\n", ldb_dn_get_linearized(result->msgs[0]->dn)));

	element = ldb_msg_find_element(result->msgs[0], "objectClass");

	/* We need to know if this account is a user or machine. */
	account_type = ACCOUNT_TYPE_USER;
	for (i = 0; i < element->num_values; i++) {
		if (strcmp((char *)element->values[i].data, "computer") == 0) {
			account_type = ACCOUNT_TYPE_MACHINE;
			DEBUG(10, ("This user is a machine\n"));
		}
	}

	gpos = talloc_array(gp_ctx, const char *, 1);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpos, mem_ctx);
	gpos[0] = NULL;

	/* Walk through the containers until we hit the root */
	inherit = 1;
	dn = ldb_dn_get_parent(mem_ctx, result->msgs[0]->dn);
	while (ldb_dn_compare_base(ldb_get_default_basedn(gp_ctx->ldb_ctx), dn) == 0) {
		const char *gpo_attrs[] = { "gPLink", "gPOptions", NULL };
		struct gp_link **gplinks;
		enum gpo_inheritance gpoptions;

		DEBUG(10, ("Getting gPLinks for DN: %s\n", ldb_dn_get_linearized(dn)));

		/* Get the gPLink and gPOptions attributes from the container */
		rv = ldb_search(gp_ctx->ldb_ctx,
				mem_ctx,
				&result,
				dn,
				LDB_SCOPE_BASE,
				gpo_attrs,
				"objectclass=*");
		if (rv != LDB_SUCCESS) {
			DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv),
					ldb_errstring(gp_ctx->ldb_ctx)));
			talloc_free(mem_ctx);
			return NT_STATUS_UNSUCCESSFUL;
		}

		/* Parse the gPLink attribute, put it into a nice struct array */
		status = parse_gplink(mem_ctx, ldb_msg_find_attr_as_string(result->msgs[0], "gPLink", ""), &gplinks);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to parse gPLink\n"));
			talloc_free(mem_ctx);
			return status;
		}

		/* Check all group policy links on this container */
		for (i = 0; gplinks[i] != NULL; i++) {
			struct gp_object *gpo;
			uint32_t access_granted;

			/* If inheritance was blocked at a higher level and this
			 * gplink is not enforced, it should not be applied */
			if (!inherit && !(gplinks[i]->options & GPLINK_OPT_ENFORCE))
				continue;

			/* Don't apply disabled links */
			if (gplinks[i]->options & GPLINK_OPT_DISABLE)
				continue;

			/* Get GPO information */
			status = gp_get_gpo_info(gp_ctx, gplinks[i]->dn, &gpo);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("Failed to get gpo information for %s\n", gplinks[i]->dn));
				talloc_free(mem_ctx);
				return status;
			}

			/* If the account does not have read access, this GPO does not apply
			 * to this account */
			status = se_access_check(gpo->security_descriptor,
					token,
					(SEC_STD_READ_CONTROL | SEC_ADS_LIST | SEC_ADS_READ_PROP),
					&access_granted);
			if (!NT_STATUS_IS_OK(status)) {
				continue;
			}

			/* If the account is a user and the GPO has user disabled flag, or
			 * a machine and the GPO has machine disabled flag, this GPO does
			 * not apply to this account */
			if ((account_type == ACCOUNT_TYPE_USER &&
					(gpo->flags & GPO_FLAG_USER_DISABLE)) ||
					(account_type == ACCOUNT_TYPE_MACHINE &&
					(gpo->flags & GPO_FLAG_MACHINE_DISABLE))) {
				continue;
			}

			/* Add the GPO to the list */
			gpos = talloc_realloc(gp_ctx, gpos, const char *, count+2);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpos, mem_ctx);
			gpos[count] = talloc_strdup(gp_ctx, gplinks[i]->dn);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpos[count], mem_ctx);
			gpos[count+1] = NULL;
			count++;

			/* Clean up */
			talloc_free(gpo);
		}

		/* If inheritance is blocked, then we should only add enforced gPLinks
		 * higher up */
		gpoptions = ldb_msg_find_attr_as_uint(result->msgs[0], "gPOptions", 0);
		if (gpoptions == GPO_BLOCK_INHERITANCE) {
			inherit = 0;
		}
		dn = ldb_dn_get_parent(mem_ctx, dn);
	}

	talloc_free(mem_ctx);

	*ret = gpos;
	return NT_STATUS_OK;
}

NTSTATUS gp_set_gplink(struct gp_context *gp_ctx, const char *dn_str, struct gp_link *gplink)
{
	TALLOC_CTX *mem_ctx;
	struct ldb_result *result;
	struct ldb_dn *dn;
	struct ldb_message *msg;
	const char *attrs[] = { "gPLink", NULL };
	const char *gplink_str;
	int rv;
	char *start;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);

	rv = ldb_search(gp_ctx->ldb_ctx, mem_ctx, &result, dn, LDB_SCOPE_BASE, attrs, "(objectclass=*)");
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv), ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (result->count != 1) {
		talloc_free(mem_ctx);
		return NT_STATUS_NOT_FOUND;
	}

	gplink_str = ldb_msg_find_attr_as_string(result->msgs[0], "gPLink", "");

	/* If this GPO link already exists, alter the options, else add it */
	if ((start = strcasestr(gplink_str, gplink->dn)) != NULL) {
		start += strlen(gplink->dn);
		*start = '\0';
		start++;
		while (*start != ']' && *start != '\0') {
			start++;
		}
		gplink_str = talloc_asprintf(mem_ctx, "%s;%d%s", gplink_str, gplink->options, start);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gplink_str, mem_ctx);

	} else {
		/* Prepend the new GPO link to the string. This list is backwards in priority. */
		gplink_str = talloc_asprintf(mem_ctx, "[LDAP://%s;%d]%s", gplink->dn, gplink->options, gplink_str);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gplink_str, mem_ctx);
	}



	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = dn;

	rv = ldb_msg_add_string(msg, "gPLink", gplink_str);
	if (rv != 0) {
		DEBUG(0, ("LDB message add string failed: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;

	rv = ldb_modify(gp_ctx->ldb_ctx, msg);
	if (rv != 0) {
		DEBUG(0, ("LDB modify failed: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

NTSTATUS gp_del_gplink(struct gp_context *gp_ctx, const char *dn_str, const char *gplink_dn)
{
	TALLOC_CTX *mem_ctx;
	struct ldb_result *result;
	struct ldb_dn *dn;
	struct ldb_message *msg;
	const char *attrs[] = { "gPLink", NULL };
	const char *gplink_str, *search_string;
	int rv;
	char *p;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);

	rv = ldb_search(gp_ctx->ldb_ctx, mem_ctx, &result, dn, LDB_SCOPE_BASE, attrs, "(objectclass=*)");
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv), ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (result->count != 1) {
		talloc_free(mem_ctx);
		return NT_STATUS_NOT_FOUND;
	}

	gplink_str = ldb_msg_find_attr_as_string(result->msgs[0], "gPLink", "");

	/* If this GPO link already exists, alter the options, else add it */
	search_string = talloc_asprintf(mem_ctx, "[LDAP://%s]", gplink_dn);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(search_string, mem_ctx);

	p = strcasestr(gplink_str, search_string);
	if (p == NULL) {
		talloc_free(mem_ctx);
		return NT_STATUS_NOT_FOUND;
	}

	*p = '\0';
	p++;
	while (*p != ']' && *p != '\0') {
		p++;
	}
	p++;
	gplink_str = talloc_asprintf(mem_ctx, "%s%s", gplink_str, p);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gplink_str, mem_ctx);


	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = dn;

	if (strcmp(gplink_str, "") == 0) {
		rv = ldb_msg_add_empty(msg, "gPLink", LDB_FLAG_MOD_DELETE, NULL);
		if (rv != 0) {
			DEBUG(0, ("LDB message add empty element failed: %s\n", ldb_strerror(rv)));
			talloc_free(mem_ctx);
			return NT_STATUS_UNSUCCESSFUL;
		}
	} else {
		rv = ldb_msg_add_string(msg, "gPLink", gplink_str);
		if (rv != 0) {
			DEBUG(0, ("LDB message add string failed: %s\n", ldb_strerror(rv)));
			talloc_free(mem_ctx);
			return NT_STATUS_UNSUCCESSFUL;
		}
		msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	}
	rv = ldb_modify(gp_ctx->ldb_ctx, msg);
	if (rv != 0) {
		DEBUG(0, ("LDB modify failed: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

NTSTATUS gp_get_inheritance(struct gp_context *gp_ctx, const char *dn_str, enum gpo_inheritance *inheritance)
{
	TALLOC_CTX *mem_ctx;
	struct ldb_result *result;
	struct ldb_dn *dn;
	const char *attrs[] = { "gPOptions", NULL };
	int rv;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);

	rv = ldb_search(gp_ctx->ldb_ctx, mem_ctx, &result, dn, LDB_SCOPE_BASE, attrs, "(objectclass=*)");
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv), ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (result->count != 1) {
		talloc_free(mem_ctx);
		return NT_STATUS_NOT_FOUND;
	}

	*inheritance = ldb_msg_find_attr_as_uint(result->msgs[0], "gPOptions", 0);

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

NTSTATUS gp_set_inheritance(struct gp_context *gp_ctx, const char *dn_str, enum gpo_inheritance inheritance)
{
	char *inheritance_string;
	struct ldb_message *msg;
	int rv;

	msg = ldb_msg_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(msg);

	msg->dn = ldb_dn_new(msg, gp_ctx->ldb_ctx, dn_str);

	inheritance_string = talloc_asprintf(msg, "%d", inheritance);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(inheritance_string, msg);

	rv = ldb_msg_add_string(msg, "gPOptions", inheritance_string);
	if (rv != 0) {
		DEBUG(0, ("LDB message add string failed: %s\n", ldb_strerror(rv)));
		talloc_free(msg);
		return NT_STATUS_UNSUCCESSFUL;
	}
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;

	rv = ldb_modify(gp_ctx->ldb_ctx, msg);
	if (rv != 0) {
		DEBUG(0, ("LDB modify failed: %s\n", ldb_strerror(rv)));
		talloc_free(msg);
		return NT_STATUS_UNSUCCESSFUL;
	}

	talloc_free(msg);
	return NT_STATUS_OK;
}

NTSTATUS gp_create_ldap_gpo(struct gp_context *gp_ctx, struct gp_object *gpo)
{
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx;
	int rv;
	char *dn_str, *flags_str, *version_str;
	struct ldb_dn *child_dn, *gpo_dn;

	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* CN={GUID} */
	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = ldb_get_default_basedn(gp_ctx->ldb_ctx);
	dn_str = talloc_asprintf(mem_ctx, "CN=%s,CN=Policies,CN=System", gpo->name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(dn_str, mem_ctx);

	child_dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);
	rv = ldb_dn_add_child(msg->dn, child_dn);
	if (!rv) goto ldb_msg_add_error;

	flags_str = talloc_asprintf(mem_ctx, "%d", gpo->flags);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(flags_str, mem_ctx);

	version_str = talloc_asprintf(mem_ctx, "%d", gpo->version);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(version_str, mem_ctx);

	rv = ldb_msg_add_string(msg, "objectClass", "top");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "objectClass", "container");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "objectClass", "groupPolicyContainer");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "displayName", gpo->display_name);
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "name", gpo->name);
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "CN", gpo->name);
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "gPCFileSysPath", gpo->file_sys_path);
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "flags", flags_str);
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "versionNumber", version_str);
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "showInAdvancedViewOnly", "TRUE");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "gpCFunctionalityVersion", "2");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;

	rv = ldb_add(gp_ctx->ldb_ctx, msg);
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB add error: %s\n", ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	gpo_dn = msg->dn;

	/* CN=User */
	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = ldb_dn_copy(mem_ctx, gpo_dn);
	child_dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, "CN=User");
	rv = ldb_dn_add_child(msg->dn, child_dn);
	if (!rv) goto ldb_msg_add_error;

	rv = ldb_msg_add_string(msg, "objectClass", "top");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "objectClass", "container");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "showInAdvancedViewOnly", "TRUE");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "CN", "User");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "name", "User");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;

	rv = ldb_add(gp_ctx->ldb_ctx, msg);
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB add error: %s\n", ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* CN=Machine */
	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = ldb_dn_copy(mem_ctx, gpo_dn);
	child_dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, "CN=Machine");
	rv = ldb_dn_add_child(msg->dn, child_dn);
	if (!rv) goto ldb_msg_add_error;

	rv = ldb_msg_add_string(msg, "objectClass", "top");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "objectClass", "container");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "showInAdvancedViewOnly", "TRUE");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "CN", "Machine");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;
	rv = ldb_msg_add_string(msg, "name", "Machine");
	if (rv != LDB_SUCCESS) goto ldb_msg_add_error;

	rv = ldb_add(gp_ctx->ldb_ctx, msg);
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB add error: %s\n", ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	gpo->dn = talloc_strdup(gpo, ldb_dn_get_linearized(gpo_dn));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->dn, mem_ctx);

	talloc_free(mem_ctx);
	return NT_STATUS_OK;

	ldb_msg_add_error:
	DEBUG(0, ("LDB Error adding element to ldb message\n"));
	talloc_free(mem_ctx);
	return NT_STATUS_UNSUCCESSFUL;
}

NTSTATUS gp_set_ads_acl (struct gp_context *gp_ctx, const char *dn_str, const struct security_descriptor *sd)
{
	TALLOC_CTX *mem_ctx;
	DATA_BLOB data;
	enum ndr_err_code ndr_err;
	struct ldb_message *msg;
	int rv;

	/* Create a forked memory context to clean up easily */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* Push the security descriptor through the NDR library */
	ndr_err = ndr_push_struct_blob(&data,
			mem_ctx,
			sd,
			(ndr_push_flags_fn_t)ndr_push_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}


	/* Create a LDB message */
	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, dn_str);

	rv = ldb_msg_add_value(msg, "nTSecurityDescriptor", &data, NULL);
	if (rv != 0) {
		DEBUG(0, ("LDB message add element failed for adding nTSecurityDescriptor: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;

	rv = ldb_modify(gp_ctx->ldb_ctx, msg);
	if (rv != 0) {
		DEBUG(0, ("LDB modify failed: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

/* This function sets flags, version and displayName on a GPO */
NTSTATUS gp_set_ldap_gpo(struct gp_context *gp_ctx, struct gp_object *gpo)
{
	int rv;
	TALLOC_CTX *mem_ctx;
	struct ldb_message *msg;
	char *version_str, *flags_str;

	mem_ctx = talloc_new(gp_ctx);

	msg = ldb_msg_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	msg->dn = ldb_dn_new(mem_ctx, gp_ctx->ldb_ctx, gpo->dn);

	version_str = talloc_asprintf(mem_ctx, "%d", gpo->version);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	flags_str = talloc_asprintf(mem_ctx, "%d", gpo->flags);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(msg, mem_ctx);

	rv = ldb_msg_add_string(msg, "flags", flags_str);
	if (rv != 0) {
		DEBUG(0, ("LDB message add string failed for flags: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;

	rv = ldb_msg_add_string(msg, "version", version_str);
	if (rv != 0) {
		DEBUG(0, ("LDB message add string failed for version: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	msg->elements[1].flags = LDB_FLAG_MOD_REPLACE;

	rv = ldb_msg_add_string(msg, "displayName", gpo->display_name);
	if (rv != 0) {
		DEBUG(0, ("LDB message add string failed for displayName: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	msg->elements[2].flags = LDB_FLAG_MOD_REPLACE;

	rv = ldb_modify(gp_ctx->ldb_ctx, msg);
	if (rv != 0) {
		DEBUG(0, ("LDB modify failed: %s\n", ldb_strerror(rv)));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}
