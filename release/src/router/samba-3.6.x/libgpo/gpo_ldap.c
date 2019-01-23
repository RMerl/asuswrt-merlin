/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2005,2007
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
#include "libgpo/gpo.h"
#include "auth.h"
#if _SAMBA_BUILD_ == 4
#include "libgpo/gpo_s4.h"
#include "source4/libgpo/ads_convenience.h"
#endif
#include "../libcli/security/security.h"

/****************************************************************
 parse the raw extension string into a GP_EXT structure
****************************************************************/

bool ads_parse_gp_ext(TALLOC_CTX *mem_ctx,
		      const char *extension_raw,
		      struct GP_EXT **gp_ext)
{
	bool ret = false;
	struct GP_EXT *ext = NULL;
	char **ext_list = NULL;
	char **ext_strings = NULL;
	int i;

	if (!extension_raw) {
		goto parse_error;
	}

	DEBUG(20,("ads_parse_gp_ext: %s\n", extension_raw));

	ext = talloc_zero(mem_ctx, struct GP_EXT);
	if (!ext) {
		goto parse_error;
	}

	ext_list = str_list_make(mem_ctx, extension_raw, "]");
	if (!ext_list) {
		goto parse_error;
	}

	for (i = 0; ext_list[i] != NULL; i++) {
		/* no op */
	}

	ext->num_exts = i;

	if (ext->num_exts) {
		ext->extensions 	= talloc_zero_array(mem_ctx, char *,
							    ext->num_exts);
		ext->extensions_guid	= talloc_zero_array(mem_ctx, char *,
							    ext->num_exts);
		ext->snapins		= talloc_zero_array(mem_ctx, char *,
							    ext->num_exts);
		ext->snapins_guid	= talloc_zero_array(mem_ctx, char *,
							    ext->num_exts);
	}

	ext->gp_extension = talloc_strdup(mem_ctx, extension_raw);

	if (!ext->extensions || !ext->extensions_guid ||
	    !ext->snapins || !ext->snapins_guid ||
	    !ext->gp_extension) {
		goto parse_error;
	}

	for (i = 0; ext_list[i] != NULL; i++) {

		int k;
		char *p, *q;

		DEBUGADD(10,("extension #%d\n", i));

		p = ext_list[i];

		if (p[0] == '[') {
			p++;
		}

		ext_strings = str_list_make(mem_ctx, p, "}");
		if (ext_strings == NULL) {
			goto parse_error;
		}

		for (k = 0; ext_strings[k] != NULL; k++) {
			/* no op */
		}

		q = ext_strings[0];

		if (q[0] == '{') {
			q++;
		}

		ext->extensions[i] = talloc_strdup(mem_ctx,
					   cse_gpo_guid_string_to_name(q));
		ext->extensions_guid[i] = talloc_strdup(mem_ctx, q);

		/* we might have no name for the guid */
		if (ext->extensions_guid[i] == NULL) {
			goto parse_error;
		}

		for (k = 1; ext_strings[k] != NULL; k++) {

			char *m = ext_strings[k];

			if (m[0] == '{') {
				m++;
			}

			/* FIXME: theoretically there could be more than one
			 * snapin per extension */
			ext->snapins[i] = talloc_strdup(mem_ctx,
				cse_snapin_gpo_guid_string_to_name(m));
			ext->snapins_guid[i] = talloc_strdup(mem_ctx, m);

			/* we might have no name for the guid */
			if (ext->snapins_guid[i] == NULL) {
				goto parse_error;
			}
		}
	}

	*gp_ext = ext;

	ret = true;

 parse_error:
	talloc_free(ext_list);
	talloc_free(ext_strings);

	return ret;
}

#ifdef HAVE_LDAP

/****************************************************************
 parse the raw link string into a GP_LINK structure
****************************************************************/

static ADS_STATUS gpo_parse_gplink(TALLOC_CTX *mem_ctx,
				   const char *gp_link_raw,
				   uint32_t options,
				   struct GP_LINK *gp_link)
{
	ADS_STATUS status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	char **link_list;
	int i;

	ZERO_STRUCTP(gp_link);

	DEBUG(10,("gpo_parse_gplink: gPLink: %s\n", gp_link_raw));

	link_list = str_list_make_v3(mem_ctx, gp_link_raw, "]");
	if (!link_list) {
		goto parse_error;
	}

	for (i = 0; link_list[i] != NULL; i++) {
		/* no op */
	}

	gp_link->gp_opts = options;
	gp_link->num_links = i;

	if (gp_link->num_links) {
		gp_link->link_names = talloc_zero_array(mem_ctx, char *,
							gp_link->num_links);
		gp_link->link_opts = talloc_zero_array(mem_ctx, uint32_t,
						       gp_link->num_links);
	}

	gp_link->gp_link = talloc_strdup(mem_ctx, gp_link_raw);

	if (!gp_link->link_names || !gp_link->link_opts || !gp_link->gp_link) {
		goto parse_error;
	}

	for (i = 0; link_list[i] != NULL; i++) {

		char *p, *q;

		DEBUGADD(10,("gpo_parse_gplink: processing link #%d\n", i));

		q = link_list[i];
		if (q[0] == '[') {
			q++;
		};

		p = strchr(q, ';');

		if (p == NULL) {
			goto parse_error;
		}

		gp_link->link_names[i] = talloc_strdup(mem_ctx, q);
		if (gp_link->link_names[i] == NULL) {
			goto parse_error;
		}
		gp_link->link_names[i][PTR_DIFF(p, q)] = 0;

		gp_link->link_opts[i] = atoi(p + 1);

		DEBUGADD(10,("gpo_parse_gplink: link: %s\n",
			gp_link->link_names[i]));
		DEBUGADD(10,("gpo_parse_gplink: opt: %d\n",
			gp_link->link_opts[i]));

	}

	status = ADS_SUCCESS;

 parse_error:
	talloc_free(link_list);

	return status;
}

/****************************************************************
 helper call to get a GP_LINK structure from a linkdn
****************************************************************/

ADS_STATUS ads_get_gpo_link(ADS_STRUCT *ads,
			    TALLOC_CTX *mem_ctx,
			    const char *link_dn,
			    struct GP_LINK *gp_link_struct)
{
	ADS_STATUS status;
	const char *attrs[] = {"gPLink", "gPOptions", NULL};
	LDAPMessage *res = NULL;
	const char *gp_link;
	uint32_t gp_options;

	ZERO_STRUCTP(gp_link_struct);

	status = ads_search_dn(ads, &res, link_dn, attrs);
	if (!ADS_ERR_OK(status)) {
		DEBUG(10,("ads_get_gpo_link: search failed with %s\n",
			ads_errstr(status)));
		return status;
	}

	if (ads_count_replies(ads, res) != 1) {
		DEBUG(10,("ads_get_gpo_link: no result\n"));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	gp_link = ads_pull_string(ads, mem_ctx, res, "gPLink");
	if (gp_link == NULL) {
		DEBUG(10,("ads_get_gpo_link: no 'gPLink' attribute found\n"));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_ATTRIBUTE);
	}

	/* perfectly legal to have no options */
	if (!ads_pull_uint32(ads, res, "gPOptions", &gp_options)) {
		DEBUG(10,("ads_get_gpo_link: "
			"no 'gPOptions' attribute found\n"));
		gp_options = 0;
	}

	ads_msgfree(ads, res);

	return gpo_parse_gplink(mem_ctx, gp_link, gp_options, gp_link_struct);
}

/****************************************************************
 helper call to add a gp link
****************************************************************/

ADS_STATUS ads_add_gpo_link(ADS_STRUCT *ads,
			    TALLOC_CTX *mem_ctx,
			    const char *link_dn,
			    const char *gpo_dn,
			    uint32_t gpo_opt)
{
	ADS_STATUS status;
	const char *attrs[] = {"gPLink", NULL};
	LDAPMessage *res = NULL;
	const char *gp_link, *gp_link_new;
	ADS_MODLIST mods;

	/* although ADS allows to set anything here, we better check here if
	 * the gpo_dn is sane */

	if (!strnequal(gpo_dn, "LDAP://CN={", strlen("LDAP://CN={")) != 0) {
		return ADS_ERROR(LDAP_INVALID_DN_SYNTAX);
	}

	status = ads_search_dn(ads, &res, link_dn, attrs);
	if (!ADS_ERR_OK(status)) {
		DEBUG(10,("ads_add_gpo_link: search failed with %s\n",
			ads_errstr(status)));
		return status;
	}

	if (ads_count_replies(ads, res) != 1) {
		DEBUG(10,("ads_add_gpo_link: no result\n"));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	gp_link = ads_pull_string(ads, mem_ctx, res, "gPLink");
	if (gp_link == NULL) {
		gp_link_new = talloc_asprintf(mem_ctx, "[%s;%d]",
			gpo_dn, gpo_opt);
	} else {
		gp_link_new = talloc_asprintf(mem_ctx, "%s[%s;%d]",
			gp_link, gpo_dn, gpo_opt);
	}

	ads_msgfree(ads, res);
	ADS_ERROR_HAVE_NO_MEMORY(gp_link_new);

	mods = ads_init_mods(mem_ctx);
	ADS_ERROR_HAVE_NO_MEMORY(mods);

	status = ads_mod_str(mem_ctx, &mods, "gPLink", gp_link_new);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	return ads_gen_mod(ads, link_dn, mods);
}

/****************************************************************
 helper call to delete add a gp link
****************************************************************/

/* untested & broken */
ADS_STATUS ads_delete_gpo_link(ADS_STRUCT *ads,
			       TALLOC_CTX *mem_ctx,
			       const char *link_dn,
			       const char *gpo_dn)
{
	ADS_STATUS status;
	const char *attrs[] = {"gPLink", NULL};
	LDAPMessage *res = NULL;
	const char *gp_link, *gp_link_new = NULL;
	ADS_MODLIST mods;

	/* check for a sane gpo_dn */
	if (gpo_dn[0] != '[') {
		DEBUG(10,("ads_delete_gpo_link: first char not: [\n"));
		return ADS_ERROR(LDAP_INVALID_DN_SYNTAX);
	}

	if (gpo_dn[strlen(gpo_dn)] != ']') {
		DEBUG(10,("ads_delete_gpo_link: last char not: ]\n"));
		return ADS_ERROR(LDAP_INVALID_DN_SYNTAX);
	}

	status = ads_search_dn(ads, &res, link_dn, attrs);
	if (!ADS_ERR_OK(status)) {
		DEBUG(10,("ads_delete_gpo_link: search failed with %s\n",
			ads_errstr(status)));
		return status;
	}

	if (ads_count_replies(ads, res) != 1) {
		DEBUG(10,("ads_delete_gpo_link: no result\n"));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	gp_link = ads_pull_string(ads, mem_ctx, res, "gPLink");
	if (gp_link == NULL) {
		return ADS_ERROR(LDAP_NO_SUCH_ATTRIBUTE);
	}

	/* find link to delete */
	/* gp_link_new = talloc_asprintf(mem_ctx, "%s[%s;%d]", gp_link,
					 gpo_dn, gpo_opt); */

	ads_msgfree(ads, res);
	ADS_ERROR_HAVE_NO_MEMORY(gp_link_new);

	mods = ads_init_mods(mem_ctx);
	ADS_ERROR_HAVE_NO_MEMORY(mods);

	status = ads_mod_str(mem_ctx, &mods, "gPLink", gp_link_new);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	return ads_gen_mod(ads, link_dn, mods);
}

/****************************************************************
 parse a GROUP_POLICY_OBJECT structure from an LDAPMessage result
****************************************************************/

 ADS_STATUS ads_parse_gpo(ADS_STRUCT *ads,
			  TALLOC_CTX *mem_ctx,
			  LDAPMessage *res,
			  const char *gpo_dn,
			  struct GROUP_POLICY_OBJECT *gpo)
{
	ZERO_STRUCTP(gpo);

	ADS_ERROR_HAVE_NO_MEMORY(res);

	if (gpo_dn) {
		gpo->ds_path = talloc_strdup(mem_ctx, gpo_dn);
	} else {
		gpo->ds_path = ads_get_dn(ads, mem_ctx, res);
	}

	ADS_ERROR_HAVE_NO_MEMORY(gpo->ds_path);

	if (!ads_pull_uint32(ads, res, "versionNumber", &gpo->version)) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	if (!ads_pull_uint32(ads, res, "flags", &gpo->options)) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	gpo->file_sys_path = ads_pull_string(ads, mem_ctx, res,
		"gPCFileSysPath");
	ADS_ERROR_HAVE_NO_MEMORY(gpo->file_sys_path);

	gpo->display_name = ads_pull_string(ads, mem_ctx, res,
		"displayName");
	ADS_ERROR_HAVE_NO_MEMORY(gpo->display_name);

	gpo->name = ads_pull_string(ads, mem_ctx, res,
		"name");
	ADS_ERROR_HAVE_NO_MEMORY(gpo->name);

	gpo->machine_extensions = ads_pull_string(ads, mem_ctx, res,
		"gPCMachineExtensionNames");
	gpo->user_extensions = ads_pull_string(ads, mem_ctx, res,
		"gPCUserExtensionNames");

	ads_pull_sd(ads, mem_ctx, res, "ntSecurityDescriptor",
		&gpo->security_descriptor);
	ADS_ERROR_HAVE_NO_MEMORY(gpo->security_descriptor);

	return ADS_ERROR(LDAP_SUCCESS);
}

/****************************************************************
 get a GROUP_POLICY_OBJECT structure based on different input parameters
****************************************************************/

ADS_STATUS ads_get_gpo(ADS_STRUCT *ads,
		       TALLOC_CTX *mem_ctx,
		       const char *gpo_dn,
		       const char *display_name,
		       const char *guid_name,
		       struct GROUP_POLICY_OBJECT *gpo)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	char *dn;
	const char *filter;
	const char *attrs[] = {
		"cn",
		"displayName",
		"flags",
		"gPCFileSysPath",
		"gPCFunctionalityVersion",
		"gPCMachineExtensionNames",
		"gPCUserExtensionNames",
		"gPCWQLFilter",
		"name",
		"ntSecurityDescriptor",
		"versionNumber",
		NULL};
	uint32_t sd_flags = SECINFO_DACL;

	ZERO_STRUCTP(gpo);

	if (!gpo_dn && !display_name && !guid_name) {
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	if (gpo_dn) {

		if (strnequal(gpo_dn, "LDAP://", strlen("LDAP://")) != 0) {
			gpo_dn = gpo_dn + strlen("LDAP://");
		}

		status = ads_search_retry_dn_sd_flags(ads, &res,
						      sd_flags,
						      gpo_dn, attrs);

	} else if (display_name || guid_name) {

		filter = talloc_asprintf(mem_ctx,
				 "(&(objectclass=groupPolicyContainer)(%s=%s))",
				 display_name ? "displayName" : "name",
				 display_name ? display_name : guid_name);
		ADS_ERROR_HAVE_NO_MEMORY(filter);

		status = ads_do_search_all_sd_flags(ads, ads->config.bind_path,
						    LDAP_SCOPE_SUBTREE, filter,
						    attrs, sd_flags, &res);
	}

	if (!ADS_ERR_OK(status)) {
		DEBUG(10,("ads_get_gpo: search failed with %s\n",
			ads_errstr(status)));
		return status;
	}

	if (ads_count_replies(ads, res) != 1) {
		DEBUG(10,("ads_get_gpo: no result\n"));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	dn = ads_get_dn(ads, mem_ctx, res);
	if (dn == NULL) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	status = ads_parse_gpo(ads, mem_ctx, res, dn, gpo);
	ads_msgfree(ads, res);
	TALLOC_FREE(dn);

	return status;
}

/****************************************************************
 add a gplink to the GROUP_POLICY_OBJECT linked list
****************************************************************/

static ADS_STATUS add_gplink_to_gpo_list(ADS_STRUCT *ads,
					 TALLOC_CTX *mem_ctx,
					 struct GROUP_POLICY_OBJECT **gpo_list,
					 const char *link_dn,
					 struct GP_LINK *gp_link,
					 enum GPO_LINK_TYPE link_type,
					 bool only_add_forced_gpos,
					 const struct security_token *token)
{
	ADS_STATUS status;
	int i;

	for (i = 0; i < gp_link->num_links; i++) {

		struct GROUP_POLICY_OBJECT *new_gpo = NULL;

		if (gp_link->link_opts[i] & GPO_LINK_OPT_DISABLED) {
			DEBUG(10,("skipping disabled GPO\n"));
			continue;
		}

		if (only_add_forced_gpos) {

			if (!(gp_link->link_opts[i] & GPO_LINK_OPT_ENFORCED)) {
				DEBUG(10,("skipping nonenforced GPO link "
					"because GPOPTIONS_BLOCK_INHERITANCE "
					"has been set\n"));
				continue;
			} else {
				DEBUG(10,("adding enforced GPO link although "
					"the GPOPTIONS_BLOCK_INHERITANCE "
					"has been set\n"));
			}
		}

		new_gpo = TALLOC_ZERO_P(mem_ctx, struct GROUP_POLICY_OBJECT);
		ADS_ERROR_HAVE_NO_MEMORY(new_gpo);

		status = ads_get_gpo(ads, mem_ctx, gp_link->link_names[i],
				     NULL, NULL, new_gpo);
		if (!ADS_ERR_OK(status)) {
			DEBUG(10,("failed to get gpo: %s\n",
				gp_link->link_names[i]));
			return status;
		}

		status = ADS_ERROR_NT(gpo_apply_security_filtering(new_gpo,
								   token));
		if (!ADS_ERR_OK(status)) {
			DEBUG(10,("skipping GPO \"%s\" as object "
				"has no access to it\n",
				new_gpo->display_name));
			talloc_free(new_gpo);
			continue;
		}

		new_gpo->link = link_dn;
		new_gpo->link_type = link_type;

		DLIST_ADD(*gpo_list, new_gpo);

		DEBUG(10,("add_gplink_to_gplist: added GPLINK #%d %s "
			"to GPO list\n", i, gp_link->link_names[i]));
	}

	return ADS_ERROR(LDAP_SUCCESS);
}

/****************************************************************
****************************************************************/

ADS_STATUS ads_get_sid_token(ADS_STRUCT *ads,
			     TALLOC_CTX *mem_ctx,
			     const char *dn,
			     struct security_token **token)
{
	ADS_STATUS status;
	struct dom_sid object_sid;
	struct dom_sid primary_group_sid;
	struct dom_sid *ad_token_sids;
	size_t num_ad_token_sids = 0;
	struct dom_sid *token_sids;
	uint32_t num_token_sids = 0;
	struct security_token *new_token = NULL;
	int i;

	status = ads_get_tokensids(ads, mem_ctx, dn,
				   &object_sid, &primary_group_sid,
				   &ad_token_sids, &num_ad_token_sids);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	token_sids = TALLOC_ARRAY(mem_ctx, struct dom_sid, 1);
	ADS_ERROR_HAVE_NO_MEMORY(token_sids);

	status = ADS_ERROR_NT(add_sid_to_array_unique(mem_ctx,
						      &primary_group_sid,
						      &token_sids,
						      &num_token_sids));
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	for (i = 0; i < num_ad_token_sids; i++) {

		if (sid_check_is_in_builtin(&ad_token_sids[i])) {
			continue;
		}

		status = ADS_ERROR_NT(add_sid_to_array_unique(mem_ctx,
							      &ad_token_sids[i],
							      &token_sids,
							      &num_token_sids));
		if (!ADS_ERR_OK(status)) {
			return status;
		}
	}

	new_token = create_local_nt_token(mem_ctx, &object_sid, false,
					  num_token_sids, token_sids);
	ADS_ERROR_HAVE_NO_MEMORY(new_token);

	*token = new_token;

	security_token_debug(DBGC_CLASS, 5, *token);

	return ADS_ERROR_LDAP(LDAP_SUCCESS);
}

/****************************************************************
****************************************************************/

static ADS_STATUS add_local_policy_to_gpo_list(TALLOC_CTX *mem_ctx,
					       struct GROUP_POLICY_OBJECT **gpo_list,
					       enum GPO_LINK_TYPE link_type)
{
	struct GROUP_POLICY_OBJECT *gpo = NULL;

	ADS_ERROR_HAVE_NO_MEMORY(gpo_list);

	gpo = TALLOC_ZERO_P(mem_ctx, struct GROUP_POLICY_OBJECT);
	ADS_ERROR_HAVE_NO_MEMORY(gpo);

	gpo->name = talloc_strdup(mem_ctx, "Local Policy");
	ADS_ERROR_HAVE_NO_MEMORY(gpo->name);

	gpo->display_name = talloc_strdup(mem_ctx, "Local Policy");
	ADS_ERROR_HAVE_NO_MEMORY(gpo->display_name);

	gpo->link_type = link_type;

	DLIST_ADD(*gpo_list, gpo);

	return ADS_ERROR_NT(NT_STATUS_OK);
}

/****************************************************************
 get the full list of GROUP_POLICY_OBJECTs for a given dn
****************************************************************/

ADS_STATUS ads_get_gpo_list(ADS_STRUCT *ads,
			    TALLOC_CTX *mem_ctx,
			    const char *dn,
			    uint32_t flags,
			    const struct security_token *token,
			    struct GROUP_POLICY_OBJECT **gpo_list)
{
	/* (L)ocal (S)ite (D)omain (O)rganizational(U)nit */

	ADS_STATUS status;
	struct GP_LINK gp_link;
	const char *parent_dn, *site_dn, *tmp_dn;
	bool add_only_forced_gpos = false;

	ZERO_STRUCTP(gpo_list);

	if (!dn) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}

	if (!ads_set_sasl_wrap_flags(ads, ADS_AUTH_SASL_SIGN)) {
		return ADS_ERROR(LDAP_INVALID_CREDENTIALS);
	}

	DEBUG(10,("ads_get_gpo_list: getting GPO list for [%s]\n", dn));

	/* (L)ocal */
	status = add_local_policy_to_gpo_list(mem_ctx, gpo_list,
					      GP_LINK_LOCAL);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	/* (S)ite */

	/* are site GPOs valid for users as well ??? */
	if (flags & GPO_LIST_FLAG_MACHINE) {

		status = ads_site_dn_for_machine(ads, mem_ctx,
						 ads->config.ldap_server_name,
						 &site_dn);
		if (!ADS_ERR_OK(status)) {
			return status;
		}

		DEBUG(10,("ads_get_gpo_list: query SITE: [%s] for GPOs\n",
			site_dn));

		status = ads_get_gpo_link(ads, mem_ctx, site_dn, &gp_link);
		if (ADS_ERR_OK(status)) {

			if (DEBUGLEVEL >= 100) {
				dump_gplink(ads, mem_ctx, &gp_link);
			}

			status = add_gplink_to_gpo_list(ads, mem_ctx, gpo_list,
							site_dn, &gp_link,
							GP_LINK_SITE,
							add_only_forced_gpos,
							token);
			if (!ADS_ERR_OK(status)) {
				return status;
			}

			if (flags & GPO_LIST_FLAG_SITEONLY) {
				return ADS_ERROR(LDAP_SUCCESS);
			}

			/* inheritance can't be blocked at the site level */
		}
	}

	tmp_dn = dn;

	while ((parent_dn = ads_parent_dn(tmp_dn)) &&
	       (!strequal(parent_dn, ads_parent_dn(ads->config.bind_path)))) {

		/* (D)omain */

		/* An account can just be a member of one domain */
		if (strncmp(parent_dn, "DC=", strlen("DC=")) == 0) {

			DEBUG(10,("ads_get_gpo_list: query DC: [%s] for GPOs\n",
				parent_dn));

			status = ads_get_gpo_link(ads, mem_ctx, parent_dn,
						  &gp_link);
			if (ADS_ERR_OK(status)) {

				if (DEBUGLEVEL >= 100) {
					dump_gplink(ads, mem_ctx, &gp_link);
				}

				/* block inheritance from now on */
				if (gp_link.gp_opts &
				    GPOPTIONS_BLOCK_INHERITANCE) {
					add_only_forced_gpos = true;
				}

				status = add_gplink_to_gpo_list(ads,
							mem_ctx,
							gpo_list,
							parent_dn,
							&gp_link,
							GP_LINK_DOMAIN,
							add_only_forced_gpos,
							token);
				if (!ADS_ERR_OK(status)) {
					return status;
				}
			}
		}

		tmp_dn = parent_dn;
	}

	/* reset dn again */
	tmp_dn = dn;

	while ((parent_dn = ads_parent_dn(tmp_dn)) &&
	       (!strequal(parent_dn, ads_parent_dn(ads->config.bind_path)))) {


		/* (O)rganizational(U)nit */

		/* An account can be a member of more OUs */
		if (strncmp(parent_dn, "OU=", strlen("OU=")) == 0) {

			DEBUG(10,("ads_get_gpo_list: query OU: [%s] for GPOs\n",
				parent_dn));

			status = ads_get_gpo_link(ads, mem_ctx, parent_dn,
						  &gp_link);
			if (ADS_ERR_OK(status)) {

				if (DEBUGLEVEL >= 100) {
					dump_gplink(ads, mem_ctx, &gp_link);
				}

				/* block inheritance from now on */
				if (gp_link.gp_opts &
				    GPOPTIONS_BLOCK_INHERITANCE) {
					add_only_forced_gpos = true;
				}

				status = add_gplink_to_gpo_list(ads,
							mem_ctx,
							gpo_list,
							parent_dn,
							&gp_link,
							GP_LINK_OU,
							add_only_forced_gpos,
							token);
				if (!ADS_ERR_OK(status)) {
					return status;
				}
			}
		}

		tmp_dn = parent_dn;

	};

	return ADS_ERROR(LDAP_SUCCESS);
}

#endif /* HAVE_LDAP */
