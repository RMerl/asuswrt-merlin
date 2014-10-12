/*
   Samba Unix/Linux SMB client library
   net ads commands for Group Policy
   Copyright (C) 2005-2008 Guenther Deschner (gd@samba.org)

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
#include "utils/net.h"
#include "ads.h"
#include "../libgpo/gpo.h"
#include "libgpo/gpo_proto.h"
#include "../libds/common/flags.h"

#ifdef HAVE_ADS

static int net_ads_gpo_refresh(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *mem_ctx;
	ADS_STRUCT *ads;
	ADS_STATUS status;
	const char *dn = NULL;
	struct GROUP_POLICY_OBJECT *gpo_list = NULL;
	struct GROUP_POLICY_OBJECT *read_list = NULL;
	uint32 uac = 0;
	uint32 flags = 0;
	struct GROUP_POLICY_OBJECT *gpo;
	NTSTATUS result;
	struct security_token *token = NULL;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s\n%s",
			 _("Usage:"),
			 _("net ads gpo refresh <username|machinename>"),
			 _("  Lists all GPOs assigned to an account and "
			   "downloads them\n"
			   "    username\tUser to refresh GPOs for\n"
			   "    machinename\tMachine to refresh GPOs for\n"));
		return -1;
	}

	mem_ctx = talloc_init("net_ads_gpo_refresh");
	if (mem_ctx == NULL) {
		return -1;
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		d_printf(_("failed to connect AD server: %s\n"), ads_errstr(status));
		goto out;
	}

	status = ads_find_samaccount(ads, mem_ctx, argv[0], &uac, &dn);
	if (!ADS_ERR_OK(status)) {
		d_printf(_("failed to find samaccount for %s\n"), argv[0]);
		goto out;
	}

	if (uac & UF_WORKSTATION_TRUST_ACCOUNT) {
		flags |= GPO_LIST_FLAG_MACHINE;
	}

	d_printf(_("\n%s: '%s' has dn: '%s'\n\n"),
		(uac & UF_WORKSTATION_TRUST_ACCOUNT) ? _("machine") : _("user"),
		argv[0], dn);

	d_printf(_("* fetching token "));
	if (uac & UF_WORKSTATION_TRUST_ACCOUNT) {
		status = gp_get_machine_token(ads, mem_ctx, NULL, dn, &token);
	} else {
		status = ads_get_sid_token(ads, mem_ctx, dn, &token);
	}

	if (!ADS_ERR_OK(status)) {
		d_printf(_("failed: %s\n"), ads_errstr(status));
		goto out;
	}
	d_printf(_("finished\n"));

	d_printf(_("* fetching GPO List "));
	status = ads_get_gpo_list(ads, mem_ctx, dn, flags, token, &gpo_list);
	if (!ADS_ERR_OK(status)) {
		d_printf(_("failed: %s\n"),
			 ads_errstr(status));
		goto out;
	}
	d_printf(_("finished\n"));

	d_printf(_("* Refreshing Group Policy Data "));
	if (!NT_STATUS_IS_OK(result = check_refresh_gpo_list(ads, mem_ctx,
	                                                     cache_path(GPO_CACHE_DIR),
	                                                     NULL,
							     flags,
							     gpo_list))) {
		d_printf(_("failed: %s\n"), nt_errstr(result));
		goto out;
	}
	d_printf(_("finished\n"));

	d_printf(_("* storing GPO list to registry "));

	{
		WERROR werr = gp_reg_state_store(mem_ctx, flags, dn,
						 token, gpo_list);
		if (!W_ERROR_IS_OK(werr)) {
			d_printf(_("failed: %s\n"), win_errstr(werr));
			goto out;
		}
	}

	d_printf(_("finished\n"));

	if (c->opt_verbose) {

		d_printf(_("* dumping GPO list\n"));

		for (gpo = gpo_list; gpo; gpo = gpo->next) {

			dump_gpo(ads, mem_ctx, gpo, 0);
#if 0
		char *server, *share, *nt_path, *unix_path;

		d_printf("--------------------------------------\n");
		d_printf("Name:\t\t\t%s\n", gpo->display_name);
		d_printf("LDAP GPO version:\t%d (user: %d, machine: %d)\n",
			gpo->version,
			GPO_VERSION_USER(gpo->version),
			GPO_VERSION_MACHINE(gpo->version));

		result = gpo_explode_filesyspath(mem_ctx, gpo->file_sys_path,
						 &server, &share, &nt_path,
						 &unix_path);
		if (!NT_STATUS_IS_OK(result)) {
			d_printf("got: %s\n", nt_errstr(result));
		}

		d_printf("GPO stored on server: %s, share: %s\n", server, share);
		d_printf("\tremote path:\t%s\n", nt_path);
		d_printf("\tlocal path:\t%s\n", unix_path);
#endif
		}
	}

	d_printf(_("* re-reading GPO list from registry "));

	{
		WERROR werr = gp_reg_state_read(mem_ctx, flags,
						&token->sids[0],
						&read_list);
		if (!W_ERROR_IS_OK(werr)) {
			d_printf(_("failed: %s\n"), win_errstr(werr));
			goto out;
		}
	}

	d_printf(_("finished\n"));

	if (c->opt_verbose) {

		d_printf(_("* dumping GPO list from registry\n"));

		for (gpo = read_list; gpo; gpo = gpo->next) {

			dump_gpo(ads, mem_ctx, gpo, 0);

#if 0
		char *server, *share, *nt_path, *unix_path;

		d_printf("--------------------------------------\n");
		d_printf("Name:\t\t\t%s\n", gpo->display_name);
		d_printf("LDAP GPO version:\t%d (user: %d, machine: %d)\n",
			gpo->version,
			GPO_VERSION_USER(gpo->version),
			GPO_VERSION_MACHINE(gpo->version));

		result = gpo_explode_filesyspath(mem_ctx, gpo->file_sys_path,
						 &server, &share, &nt_path,
						 &unix_path);
		if (!NT_STATUS_IS_OK(result)) {
			d_printf("got: %s\n", nt_errstr(result));
		}

		d_printf("GPO stored on server: %s, share: %s\n", server, share);
		d_printf("\tremote path:\t%s\n", nt_path);
		d_printf("\tlocal path:\t%s\n", unix_path);
#endif
		}
	}

 out:
	ads_destroy(&ads);
	talloc_destroy(mem_ctx);
	return 0;
}

static int net_ads_gpo_list_all(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	int num_reply = 0;
	LDAPMessage *msg = NULL;
	struct GROUP_POLICY_OBJECT gpo;
	TALLOC_CTX *mem_ctx;
	char *dn;
	const char *attrs[] = {
		"versionNumber",
		"flags",
		"gPCFileSysPath",
		"displayName",
		"name",
		"gPCMachineExtensionNames",
		"gPCUserExtensionNames",
		"ntSecurityDescriptor",
		NULL
	};

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads gpo listall\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List all GPOs on the DC"));
		return 0;
	}

	mem_ctx = talloc_init("net_ads_gpo_list_all");
	if (mem_ctx == NULL) {
		return -1;
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_do_search_all_sd_flags(ads, ads->config.bind_path,
					    LDAP_SCOPE_SUBTREE,
					    "(objectclass=groupPolicyContainer)",
					    attrs,
					    SECINFO_DACL,
					    &res);

	if (!ADS_ERR_OK(status)) {
		d_printf(_("search failed: %s\n"), ads_errstr(status));
		goto out;
	}

	num_reply = ads_count_replies(ads, res);

	d_printf(_("Got %d replies\n\n"), num_reply);

	/* dump the results */
	for (msg = ads_first_entry(ads, res);
	     msg;
	     msg = ads_next_entry(ads, msg)) {

		if ((dn = ads_get_dn(ads, mem_ctx, msg)) == NULL) {
			goto out;
		}

		status = ads_parse_gpo(ads, mem_ctx, msg, dn, &gpo);

		if (!ADS_ERR_OK(status)) {
			d_printf(_("ads_parse_gpo failed: %s\n"),
				ads_errstr(status));
			goto out;
		}

		dump_gpo(ads, mem_ctx, &gpo, 0);
	}

out:
	ads_msgfree(ads, res);

	TALLOC_FREE(mem_ctx);
	ads_destroy(&ads);

	return 0;
}

static int net_ads_gpo_list(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	TALLOC_CTX *mem_ctx;
	const char *dn = NULL;
	uint32 uac = 0;
	uint32 flags = 0;
	struct GROUP_POLICY_OBJECT *gpo_list;
	struct security_token *token = NULL;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s\n%s",
			 _("Usage:"),
			 _("net ads gpo list <username|machinename>"),
			 _("  Lists all GPOs for machine/user\n"
			   "    username\tUser to list GPOs for\n"
			   "    machinename\tMachine to list GPOs for\n"));
		return -1;
	}

	mem_ctx = talloc_init("net_ads_gpo_list");
	if (mem_ctx == NULL) {
		goto out;
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_find_samaccount(ads, mem_ctx, argv[0], &uac, &dn);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	if (uac & UF_WORKSTATION_TRUST_ACCOUNT) {
		flags |= GPO_LIST_FLAG_MACHINE;
	}

	d_printf(_("%s: '%s' has dn: '%s'\n"),
		(uac & UF_WORKSTATION_TRUST_ACCOUNT) ? _("machine") : _("user"),
		argv[0], dn);

	if (uac & UF_WORKSTATION_TRUST_ACCOUNT) {
		status = gp_get_machine_token(ads, mem_ctx, NULL, dn, &token);
	} else {
		status = ads_get_sid_token(ads, mem_ctx, dn, &token);
	}

	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_get_gpo_list(ads, mem_ctx, dn, flags, token, &gpo_list);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	dump_gpo_list(ads, mem_ctx, gpo_list, 0);

out:
	ads_msgfree(ads, res);

	talloc_destroy(mem_ctx);
	ads_destroy(&ads);

	return 0;
}

static int net_ads_gpo_apply(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *mem_ctx;
	ADS_STRUCT *ads;
	ADS_STATUS status;
	const char *dn = NULL;
	struct GROUP_POLICY_OBJECT *gpo_list;
	uint32 uac = 0;
	uint32 flags = 0;
	struct security_token *token = NULL;
	const char *filter = NULL;

	if (argc < 1 || c->display_usage) {
		d_printf("Usage:\n"
			 "net ads gpo apply <username|machinename>\n"
			 "  Apply GPOs for machine/user\n"
			 "    username\tUsername to apply GPOs for\n"
			 "    machinename\tMachine to apply GPOs for\n");
		return -1;
	}

	mem_ctx = talloc_init("net_ads_gpo_apply");
	if (mem_ctx == NULL) {
		goto out;
	}

	if (argc >= 2) {
		filter = cse_gpo_name_to_guid_string(argv[1]);
	}

	status = ads_startup(c, false, &ads);
	/* filter = cse_gpo_name_to_guid_string("Security"); */

	if (!ADS_ERR_OK(status)) {
		d_printf("got: %s\n", ads_errstr(status));
		goto out;
	}

	status = ads_find_samaccount(ads, mem_ctx, argv[0], &uac, &dn);
	if (!ADS_ERR_OK(status)) {
		d_printf("failed to find samaccount for %s: %s\n",
			argv[0], ads_errstr(status));
		goto out;
	}

	if (uac & UF_WORKSTATION_TRUST_ACCOUNT) {
		flags |= GPO_LIST_FLAG_MACHINE;
	}

	if (c->opt_verbose) {
		flags |= GPO_INFO_FLAG_VERBOSE;
	}

	d_printf("%s: '%s' has dn: '%s'\n",
		(uac & UF_WORKSTATION_TRUST_ACCOUNT) ? "machine" : "user",
		argv[0], dn);

	if (uac & UF_WORKSTATION_TRUST_ACCOUNT) {
		status = gp_get_machine_token(ads, mem_ctx, NULL, dn, &token);
	} else {
		status = ads_get_sid_token(ads, mem_ctx, dn, &token);
	}

	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_get_gpo_list(ads, mem_ctx, dn, flags, token, &gpo_list);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = gpo_process_gpo_list(ads, mem_ctx, token, gpo_list,
				      filter, flags);
	if (!ADS_ERR_OK(status)) {
		d_printf("failed to process gpo list: %s\n",
			ads_errstr(status));
		goto out;
	}

out:
	ads_destroy(&ads);
	talloc_destroy(mem_ctx);
	return 0;
}

static int net_ads_gpo_link_get(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	TALLOC_CTX *mem_ctx;
	struct GP_LINK gp_link;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s\n%s",
			 _("Usage:"),
			 _("net ads gpo linkget <container>"),
			 _("  Lists gPLink of a containter\n"
			   "    container\tContainer to get link for\n"));
		return -1;
	}

	mem_ctx = talloc_init("add_gpo_link");
	if (mem_ctx == NULL) {
		return -1;
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_get_gpo_link(ads, mem_ctx, argv[0], &gp_link);
	if (!ADS_ERR_OK(status)) {
		d_printf(_("get link for %s failed: %s\n"), argv[0],
			ads_errstr(status));
		goto out;
	}

	dump_gplink(ads, mem_ctx, &gp_link);

out:
	talloc_destroy(mem_ctx);
	ads_destroy(&ads);

	return 0;
}

static int net_ads_gpo_link_add(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	uint32 gpo_opt = 0;
	TALLOC_CTX *mem_ctx;

	if (argc < 2 || c->display_usage) {
		d_printf("%s\n%s\n%s",
			 _("Usage:"),
			 _("net ads gpo linkadd <linkdn> <gpodn> [options]"),
			 _("  Link a container to a GPO\n"
			   "    linkdn\tContainer to link to a GPO\n"
			   "    gpodn\tGPO to link container to\n"));
		d_printf(_("note: DNs must be provided properly escaped.\n"
			   "See RFC 4514 for details\n"));
		return -1;
	}

	mem_ctx = talloc_init("add_gpo_link");
	if (mem_ctx == NULL) {
		return -1;
	}

	if (argc == 3) {
		gpo_opt = atoi(argv[2]);
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_add_gpo_link(ads, mem_ctx, argv[0], argv[1], gpo_opt);
	if (!ADS_ERR_OK(status)) {
		d_printf(_("link add failed: %s\n"), ads_errstr(status));
		goto out;
	}

out:
	talloc_destroy(mem_ctx);
	ads_destroy(&ads);

	return 0;
}

#if 0 /* broken */

static int net_ads_gpo_link_delete(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	TALLOC_CTX *mem_ctx;

	if (argc < 2 || c->display_usage) {
		d_printf("Usage:\n"
			 "net ads gpo linkdelete <linkdn> <gpodn>\n"
			 "  Delete a GPO link\n"
			 "    <linkdn>\tContainer to delete GPO from\n"
			 "    <gpodn>\tGPO to delete from container\n");
		return -1;
	}

	mem_ctx = talloc_init("delete_gpo_link");
	if (mem_ctx == NULL) {
		return -1;
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	status = ads_delete_gpo_link(ads, mem_ctx, argv[0], argv[1]);
	if (!ADS_ERR_OK(status)) {
		d_printf("delete link failed: %s\n", ads_errstr(status));
		goto out;
	}

out:
	talloc_destroy(mem_ctx);
	ads_destroy(&ads);

	return 0;
}

#endif

static int net_ads_gpo_get_gpo(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	TALLOC_CTX *mem_ctx;
	struct GROUP_POLICY_OBJECT gpo;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s\n%s",
			 _("Usage:"),
			 _("net ads gpo getgpo <gpo>"),
			 _("  List speciefied GPO\n"
			   "    gpo\t\tGPO to list\n"));
		return -1;
	}

	mem_ctx = talloc_init("ads_gpo_get_gpo");
	if (mem_ctx == NULL) {
		return -1;
	}

	status = ads_startup(c, false, &ads);
	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	if (strnequal(argv[0], "CN={", strlen("CN={"))) {
		status = ads_get_gpo(ads, mem_ctx, argv[0], NULL, NULL, &gpo);
	} else {
		status = ads_get_gpo(ads, mem_ctx, NULL, argv[0], NULL, &gpo);
	}

	if (!ADS_ERR_OK(status)) {
		d_printf(_("get gpo for [%s] failed: %s\n"), argv[0],
			ads_errstr(status));
		goto out;
	}

	dump_gpo(ads, mem_ctx, &gpo, 1);

out:
	talloc_destroy(mem_ctx);
	ads_destroy(&ads);

	return 0;
}

int net_ads_gpo(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"apply",
			net_ads_gpo_apply,
			NET_TRANSPORT_ADS,
			"Apply GPO to container",
			"net ads gpo apply\n"
			"    Apply GPO to container"
		},
		{
			"getgpo",
			net_ads_gpo_get_gpo,
			NET_TRANSPORT_ADS,
			N_("List specified GPO"),
			N_("net ads gpo getgpo\n"
			   "    List specified GPO")
		},
		{
			"linkadd",
			net_ads_gpo_link_add,
			NET_TRANSPORT_ADS,
			N_("Link a container to a GPO"),
			N_("net ads gpo linkadd\n"
			   "    Link a container to a GPO")
		},
#if 0
		{
			"linkdelete",
			net_ads_gpo_link_delete,
			NET_TRANSPORT_ADS,
			"Delete GPO link from a container",
			"net ads gpo linkdelete\n"
			"    Delete GPO link from a container"
		},
#endif
		{
			"linkget",
			net_ads_gpo_link_get,
			NET_TRANSPORT_ADS,
			N_("Lists gPLink of containter"),
			N_("net ads gpo linkget\n"
			   "    Lists gPLink of containter")
		},
		{
			"list",
			net_ads_gpo_list,
			NET_TRANSPORT_ADS,
			N_("Lists all GPOs for machine/user"),
			N_("net ads gpo list\n"
			   "    Lists all GPOs for machine/user")
		},
		{
			"listall",
			net_ads_gpo_list_all,
			NET_TRANSPORT_ADS,
			N_("Lists all GPOs on a DC"),
			N_("net ads gpo listall\n"
			   "    Lists all GPOs on a DC")
		},
		{
			"refresh",
			net_ads_gpo_refresh,
			NET_TRANSPORT_ADS,
			N_("Lists all GPOs assigned to an account and "
			   "downloads them"),
			N_("net ads gpo refresh\n"
			   "    Lists all GPOs assigned to an account and "
			   "downloads them")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net ads gpo", func);
}

#endif /* HAVE_ADS */
