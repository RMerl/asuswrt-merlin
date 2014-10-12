/*
   Samba Unix/Linux SMB client library
   net ads commands for Group Policy

   Copyright (C) 2005-2008 Guenther Deschner
   Copyright (C) 2009 Wilco Baan Hofman

   Based on Guenther's work in net_ads_gpo.h (samba 3)

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
#include "samba_tool/samba_tool.h"
#include <ldb.h>
#include "auth/auth.h"
#include "param/param.h"
#include "libcli/security/sddl.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "lib/policy/policy.h"

static int net_gpo_list_all_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo listall [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo listall --help\n");
	return 0;
}

static int net_gpo_list_all(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct gp_object **gpo;
	const char **gpo_flags;
	unsigned int i, j;
	NTSTATUS rv;

	rv = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(rv)));
		return 1;
	}

	rv = gp_list_all_gpos(gp_ctx, &gpo);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to list all GPO's: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}

	for (i = 0; gpo[i] != NULL; i++) {
		gp_get_gpo_flags(gp_ctx, gpo[i]->flags, &gpo_flags);

		d_printf("GPO          : %s\n", gpo[i]->name);
		d_printf("display name : %s\n", gpo[i]->display_name);
		d_printf("path         : %s\n", gpo[i]->file_sys_path);
		d_printf("dn           : %s\n", gpo[i]->dn);
		d_printf("version      : %d\n", gpo[i]->version);
		if (gpo_flags[0] == NULL) {
			d_printf("flags        : NONE\n");
		} else {
			d_printf("flags        : %s\n", gpo_flags[0]);
			for (j = 1; gpo_flags[j] != NULL; j++) {
				d_printf("               %s\n", gpo_flags[i]);
			}
		}
		d_printf("\n");
		talloc_free(gpo_flags);
	}
	talloc_free(gp_ctx);

	return 0;
}

static int net_gpo_get_gpo_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo show <dn> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo show --help\n");
	return 0;
}

static int net_gpo_get_gpo(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct gp_object *gpo;
	const char **gpo_flags;
	char *sddl;
	int i;
	NTSTATUS rv;

	if (argc != 1) {
		return net_gpo_get_gpo_usage(ctx, argc, argv);
	}


	rv = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(rv)));
		return 1;
	}

	rv = gp_get_gpo_info(gp_ctx, argv[0], &gpo);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to get GPO: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}

	gp_get_gpo_flags(gp_ctx, gpo->flags, &gpo_flags);

	d_printf("GPO          : %s\n", gpo->name);
	d_printf("display name : %s\n", gpo->display_name);
	d_printf("path         : %s\n", gpo->file_sys_path);
	d_printf("dn           : %s\n", gpo->dn);
	d_printf("version      : %d\n", gpo->version);
	if (gpo_flags[0] == NULL) {
		d_printf("flags        : NONE\n");
	} else {
		d_printf("flags        : %s\n", gpo_flags[0]);
		for (i = 1; gpo_flags[i] != NULL; i++) {
			d_printf("               %s\n", gpo_flags[i]);
		}
	}
	sddl = sddl_encode(gp_ctx, gpo->security_descriptor, samdb_domain_sid(gp_ctx->ldb_ctx));
	if (sddl != NULL) {
		d_printf("ACL          : %s\n", sddl);
	}

	d_printf("\n");

	talloc_free(gp_ctx);
	return 0;
}

static int net_gpo_link_get_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo getlink <dn> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo getlink --help\n");
	return 0;
}

static int net_gpo_link_get(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct gp_link **links;
	NTSTATUS rv;
	unsigned int i,j;
	const char **options;

	if (argc != 1) {
		return net_gpo_link_get_usage(ctx, argc, argv);
	}

	rv = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(rv)));
		return 1;
	}

	rv = gp_get_gplinks(gp_ctx, argv[0], &links);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to get gplinks: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}

	for (i = 0; links[i] != NULL; i++) {
		gp_get_gplink_options(gp_ctx, links[i]->options, &options);

		d_printf("GPO DN  : %s\n", links[i]->dn);
		if (options[0] == NULL) {
			d_printf("Options : NONE\n");
		} else {
			d_printf("Options : %s\n", options[0]);
			for (j = 1; options[j] != NULL; j++) {
				d_printf("        : %s\n", options[j]);
			}
		}
		d_printf("\n");

		talloc_free(options);
	}

	talloc_free(gp_ctx);

	return 0;
}

static int net_gpo_list_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo list <username> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo list --help\n");
	return 0;
}

static int net_gpo_list(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct ldb_result *result;
	struct auth_user_info_dc *user_info_dc;
	struct auth_session_info *session_info;
	DATA_BLOB dummy = { NULL, 0 };
	const char **gpos;
	NTSTATUS status;
	int rv;
	unsigned int i;

	if (argc != 1) {
		return net_gpo_list_usage(ctx, argc, argv);
	}

	status = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(status)));
		return 1;
	}

	/* Find the user in the directory. We need extended DN's for group expansion
	 * in authsam_make_user_info_dc */
	rv = dsdb_search(gp_ctx->ldb_ctx,
			gp_ctx,
			&result,
			ldb_get_default_basedn(gp_ctx->ldb_ctx),
			LDB_SCOPE_SUBTREE,
			NULL,
			DSDB_SEARCH_SHOW_EXTENDED_DN,
			"(&(objectClass=user)(sAMAccountName=%s))", argv[0]);
	if (rv != LDB_SUCCESS) {
		DEBUG(0, ("LDB search failed: %s\n%s\n", ldb_strerror(rv),ldb_errstring(gp_ctx->ldb_ctx)));
		talloc_free(gp_ctx);
		return 1;
	}

        /* We expect exactly one record */
	if (result->count != 1) {
		DEBUG(0, ("Could not find SAM account with name %s\n", argv[0]));
		talloc_free(gp_ctx);
		return 1;
	}

	/* We need the server info, as this will contain the groups of this
	 * user, needed for a token */
	status = authsam_make_user_info_dc(gp_ctx,
			gp_ctx->ldb_ctx,
			lpcfg_netbios_name(gp_ctx->lp_ctx),
			lpcfg_sam_name(gp_ctx->lp_ctx),
			ldb_get_default_basedn(gp_ctx->ldb_ctx),
			result->msgs[0],
			dummy,
			dummy,
			&user_info_dc);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to make server information: %s\n", get_friendly_nt_error_msg(status)));
		talloc_free(gp_ctx);
		return 1;
	}

	/* The session info will contain the security token for this user */
	status = auth_generate_session_info(gp_ctx, gp_ctx->lp_ctx, gp_ctx->ldb_ctx, user_info_dc, 0, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to generate session information: %s\n", get_friendly_nt_error_msg(status)));
		talloc_free(gp_ctx);
		return 1;
	}

	status = gp_list_gpos(gp_ctx, session_info->security_token, &gpos);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to list gpos for user %s: %s\n", argv[0],
				get_friendly_nt_error_msg(status)));
		talloc_free(gp_ctx);
		return 1;
	}

	d_printf("GPO's for user %s:\n", argv[0]);
	for (i = 0; gpos[i] != NULL; i++) {
		d_printf("\t%s\n", gpos[i]);
	}

	talloc_free(gp_ctx);
	return 0;
}

static int net_gpo_link_set_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo setlink <container> <gpo> ['disable'] ['enforce'] [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo setlink --help\n");
	return 0;
}

static int net_gpo_link_set(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_link *gplink = talloc_zero(ctx, struct gp_link);
	struct gp_context *gp_ctx;
	unsigned int i;
	NTSTATUS status;

	if (argc < 2) {
		return net_gpo_link_set_usage(ctx, argc, argv);
	}

	if (argc >= 3) {
		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "disable") == 0) {
				gplink->options |= GPLINK_OPT_DISABLE;
			}
			if (strcmp(argv[i], "enforce") == 0) {
				gplink->options |= GPLINK_OPT_ENFORCE;
			}
		}
	}
	gplink->dn = argv[1];

	status = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(status)));
		return 1;
	}

	status = gp_set_gplink(gp_ctx, argv[0], gplink);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set GPO link on container: %s\n", get_friendly_nt_error_msg(status)));
		return 1;
	}
	d_printf("Set link on container.\nCurrent Group Policy links:\n");

	/* Display current links */
	net_gpo_link_get(ctx, 1, argv);

	talloc_free(gp_ctx);
	return 0;
}

static int net_gpo_link_del_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo dellink <container> <gpo> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo dellink --help\n");
	return 0;
}

static int net_gpo_link_del(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	NTSTATUS status;

	if (argc != 2) {
		return net_gpo_link_del_usage(ctx, argc, argv);
	}

	status = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(status)));
		return 1;
	}

	status = gp_del_gplink(gp_ctx, argv[0], argv[1]);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to delete gplink: %s\n", get_friendly_nt_error_msg(status)));
		talloc_free(gp_ctx);
		return 1;
	}
	d_printf("Deleted gplink.\nCurrent Group Policy links:\n\n");

	/* Display current links */
	net_gpo_link_get(ctx, 1, argv);

	talloc_free(gp_ctx);
	return 0;
}

static int net_gpo_inheritance_get_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo getinheritance <container> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo getinheritance --help\n");
	return 0;
}

static int net_gpo_inheritance_get(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	enum gpo_inheritance inheritance;
	NTSTATUS status;

	if (argc != 1) {
		return net_gpo_inheritance_get_usage(ctx, argc, argv);
	}

	status = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(status)));
		return 1;
	}

	status = gp_get_inheritance(gp_ctx, argv[0], &inheritance);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set GPO link on container: %s\n", get_friendly_nt_error_msg(status)));
		talloc_free(gp_ctx);
		return 1;
	}

	if (inheritance == GPO_BLOCK_INHERITANCE) {
		d_printf("container has GPO_BLOCK_INHERITANCE\n");
	} else {
		d_printf("container has GPO_INHERIT\n");
	}

	talloc_free(gp_ctx);
	return 0;
}

static int net_gpo_inheritance_set_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo setinheritance <container> <\"block\"|\"inherit\"> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo setinheritance --help\n");
	return 0;
}

static int net_gpo_inheritance_set(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	enum gpo_inheritance inheritance;
	NTSTATUS status;

	if (argc != 2) {
		return net_gpo_inheritance_set_usage(ctx, argc, argv);
	}

	if (strcmp(argv[1], "inherit") == 0) {
		inheritance = GPO_INHERIT;
	} else if (strcmp(argv[1], "block") == 0) {
		inheritance = GPO_BLOCK_INHERITANCE;
	} else {
		return net_gpo_inheritance_set_usage(ctx, argc, argv);
	}

	status = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(status)));
		return 1;
	}

	status = gp_set_inheritance(gp_ctx, argv[0], inheritance);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set GPO link on container: %s\n", get_friendly_nt_error_msg(status)));
		talloc_free(gp_ctx);
		return 1;
	}

	/* Display current links */
	net_gpo_inheritance_get(ctx, 1, argv);

	return 0;
}

static int net_gpo_fetch_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo fetch <container> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo fetch --help\n");
	return 0;
}

static int net_gpo_fetch(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct gp_object *gpo;
	const char *path;
	NTSTATUS rv;

	if (argc != 1) {
		return net_gpo_fetch_usage(ctx, argc, argv);
	}

	rv = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(rv)));
		return 1;
	}

	rv = gp_get_gpo_info(gp_ctx, argv[0], &gpo);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to get GPO: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}

	rv = gp_fetch_gpt(gp_ctx, gpo, &path);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to fetch GPO: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}
	d_printf("%s\n", path);

	return 0;
}
static int net_gpo_create_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo create <displayname> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo create --help\n");
	return 0;
}

static int net_gpo_create(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct gp_object *gpo;
	NTSTATUS rv;

	if (argc != 1) {
		return net_gpo_create_usage(ctx, argc, argv);
	}

	rv = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(rv)));
		return 1;
	}

	rv = gp_create_gpo(gp_ctx, argv[0], &gpo);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to create GPO: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}


	return 0;
}

static int net_gpo_set_acl_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo setacl <dn> <sddl> [options]\n");
	d_printf("For a list of available options, please type samba-tool gpo setacl --help\n");
	return 0;
}

static int net_gpo_set_acl(struct net_context *ctx, int argc, const char **argv)
{
	struct gp_context *gp_ctx;
	struct security_descriptor *sd;
	NTSTATUS rv;

	if (argc != 2) {
		return net_gpo_set_acl_usage(ctx, argc, argv);
	}

	rv = gp_init(ctx, ctx->lp_ctx, ctx->credentials, ctx->event_ctx, &gp_ctx);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to connect to DC's LDAP: %s\n", get_friendly_nt_error_msg(rv)));
		return 1;
	}

	/* Convert sddl to security descriptor */
	sd = sddl_decode(gp_ctx, argv[1], samdb_domain_sid(gp_ctx->ldb_ctx));
	if (sd == NULL) {
		DEBUG(0, ("Invalid SDDL\n"));
		talloc_free(gp_ctx);
		return 1;
	}

	rv = gp_set_acl(gp_ctx, argv[0], sd);
	if (!NT_STATUS_IS_OK(rv)) {
		DEBUG(0, ("Failed to set ACL on GPO: %s\n", get_friendly_nt_error_msg(rv)));
		talloc_free(gp_ctx);
		return 1;
	}

	talloc_free(gp_ctx);
	return 0;
}



static const struct net_functable net_gpo_functable[] = {
	{ "listall", "List all GPO's on a DC\n", net_gpo_list_all, net_gpo_list_all_usage },
	{ "list", "List all active GPO's for a machine/user\n", net_gpo_list, net_gpo_list_usage },
	{ "show", "Show information for a GPO\n", net_gpo_get_gpo, net_gpo_get_gpo_usage },
	{ "getlink", "List gPLink of container\n", net_gpo_link_get, net_gpo_link_get_usage },
	{ "setlink", "Link a GPO to a container\n", net_gpo_link_set, net_gpo_link_set_usage },
	{ "dellink", "Delete GPO link from a container\n", net_gpo_link_del, net_gpo_link_del_usage },
	{ "getinheritance", "Get inheritance flag from a container\n", net_gpo_inheritance_get, net_gpo_inheritance_get_usage },
	{ "setinheritance", "Set inheritance flag on a container\n", net_gpo_inheritance_set, net_gpo_inheritance_set_usage },
	{ "fetch", "Download a GPO\n", net_gpo_fetch, net_gpo_fetch_usage },
	{ "create", "Create a GPO\n", net_gpo_create, net_gpo_create_usage },
	{ "setacl", "Set ACL on a GPO\n", net_gpo_set_acl, net_gpo_set_acl_usage },
	{ NULL, NULL }
};

int net_gpo_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Syntax: samba-tool gpo <command> [options]\n");
	d_printf("For available commands, please type samba-tool gpo help\n");
	return 0;
}

int net_gpo(struct net_context *ctx, int argc, const char **argv)
{
	return net_run_function(ctx, argc, argv, net_gpo_functable, net_gpo_usage);
}
