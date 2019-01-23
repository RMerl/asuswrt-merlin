/*
   Samba Unix/Linux SMB client library
   net ads commands
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Remus Koos (remuskoos@yahoo.com)
   Copyright (C) 2002 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2006 Gerald (Jerry) Carter (jerry@samba.org)

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
#include "rpc_client/cli_pipe.h"
#include "librpc/gen_ndr/ndr_krb5pac.h"
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "nsswitch/libwbclient/wbclient.h"
#include "ads.h"
#include "libads/cldap.h"
#include "libads/dns.h"
#include "../libds/common/flags.h"
#include "librpc/gen_ndr/libnet_join.h"
#include "libnet/libnet_join.h"
#include "smb_krb5.h"
#include "secrets.h"
#include "krb5_env.h"
#include "../libcli/security/security.h"
#include "libsmb/libsmb.h"
#include "utils/net_dns.h"

#ifdef HAVE_ADS

/* when we do not have sufficient input parameters to contact a remote domain
 * we always fall back to our own realm - Guenther*/

static const char *assume_own_realm(struct net_context *c)
{
	if (!c->opt_host && strequal(lp_workgroup(), c->opt_target_workgroup)) {
		return lp_realm();
	}

	return NULL;
}

/*
  do a cldap netlogon query
*/
static int net_ads_cldap_netlogon(struct net_context *c, ADS_STRUCT *ads)
{
	char addr[INET6_ADDRSTRLEN];
	struct NETLOGON_SAM_LOGON_RESPONSE_EX reply;

	print_sockaddr(addr, sizeof(addr), &ads->ldap.ss);
	if ( !ads_cldap_netlogon_5(talloc_tos(), addr, ads->server.realm, &reply ) ) {
		d_fprintf(stderr, _("CLDAP query failed!\n"));
		return -1;
	}

	d_printf(_("Information for Domain Controller: %s\n\n"),
		addr);

	d_printf(_("Response Type: "));
	switch (reply.command) {
	case LOGON_SAM_LOGON_USER_UNKNOWN_EX:
		d_printf("LOGON_SAM_LOGON_USER_UNKNOWN_EX\n");
		break;
	case LOGON_SAM_LOGON_RESPONSE_EX:
		d_printf("LOGON_SAM_LOGON_RESPONSE_EX\n");
		break;
	default:
		d_printf("0x%x\n", reply.command);
		break;
	}

	d_printf(_("GUID: %s\n"), GUID_string(talloc_tos(),&reply.domain_uuid));

	d_printf(_("Flags:\n"
		   "\tIs a PDC:                                   %s\n"
		   "\tIs a GC of the forest:                      %s\n"
		   "\tIs an LDAP server:                          %s\n"
		   "\tSupports DS:                                %s\n"
		   "\tIs running a KDC:                           %s\n"
		   "\tIs running time services:                   %s\n"
		   "\tIs the closest DC:                          %s\n"
		   "\tIs writable:                                %s\n"
		   "\tHas a hardware clock:                       %s\n"
		   "\tIs a non-domain NC serviced by LDAP server: %s\n"
		   "\tIs NT6 DC that has some secrets:            %s\n"
		   "\tIs NT6 DC that has all secrets:             %s\n"),
		   (reply.server_type & NBT_SERVER_PDC) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_GC) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_LDAP) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_DS) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_KDC) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_TIMESERV) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_CLOSEST) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_WRITABLE) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_GOOD_TIMESERV) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_NDNC) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_SELECT_SECRET_DOMAIN_6) ? _("yes") : _("no"),
		   (reply.server_type & NBT_SERVER_FULL_SECRET_DOMAIN_6) ? _("yes") : _("no"));


	printf(_("Forest:\t\t\t%s\n"), reply.forest);
	printf(_("Domain:\t\t\t%s\n"), reply.dns_domain);
	printf(_("Domain Controller:\t%s\n"), reply.pdc_dns_name);

	printf(_("Pre-Win2k Domain:\t%s\n"), reply.domain_name);
	printf(_("Pre-Win2k Hostname:\t%s\n"), reply.pdc_name);

	if (*reply.user_name) printf(_("User name:\t%s\n"), reply.user_name);

	printf(_("Server Site Name :\t\t%s\n"), reply.server_site);
	printf(_("Client Site Name :\t\t%s\n"), reply.client_site);

	d_printf(_("NT Version: %d\n"), reply.nt_version);
	d_printf(_("LMNT Token: %.2x\n"), reply.lmnt_token);
	d_printf(_("LM20 Token: %.2x\n"), reply.lm20_token);

	return 0;
}

/*
  this implements the CLDAP based netlogon lookup requests
  for finding the domain controller of a ADS domain
*/
static int net_ads_lookup(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	int ret;

	if (c->display_usage) {
		d_printf("%s\n"
			 "net ads lookup\n"
			 "    %s",
			 _("Usage:"),
			 _("Find the ADS DC using CLDAP lookup.\n"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup_nobind(c, false, &ads))) {
		d_fprintf(stderr, _("Didn't find the cldap server!\n"));
		ads_destroy(&ads);
		return -1;
	}

	if (!ads->config.realm) {
		ads->config.realm = CONST_DISCARD(char *, c->opt_target_workgroup);
		ads->ldap.port = 389;
	}

	ret = net_ads_cldap_netlogon(c, ads);
	ads_destroy(&ads);
	return ret;
}



static int net_ads_info(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	char addr[INET6_ADDRSTRLEN];

	if (c->display_usage) {
		d_printf("%s\n"
			 "net ads info\n"
			 "    %s",
			 _("Usage:"),
			 _("Display information about an Active Directory "
			   "server.\n"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup_nobind(c, false, &ads))) {
		d_fprintf(stderr, _("Didn't find the ldap server!\n"));
		return -1;
	}

	if (!ads || !ads->config.realm) {
		d_fprintf(stderr, _("Didn't find the ldap server!\n"));
		ads_destroy(&ads);
		return -1;
	}

	/* Try to set the server's current time since we didn't do a full
	   TCP LDAP session initially */

	if ( !ADS_ERR_OK(ads_current_time( ads )) ) {
		d_fprintf( stderr, _("Failed to get server's current time!\n"));
	}

	print_sockaddr(addr, sizeof(addr), &ads->ldap.ss);

	d_printf(_("LDAP server: %s\n"), addr);
	d_printf(_("LDAP server name: %s\n"), ads->config.ldap_server_name);
	d_printf(_("Realm: %s\n"), ads->config.realm);
	d_printf(_("Bind Path: %s\n"), ads->config.bind_path);
	d_printf(_("LDAP port: %d\n"), ads->ldap.port);
	d_printf(_("Server time: %s\n"),
			 http_timestring(talloc_tos(), ads->config.current_time));

	d_printf(_("KDC server: %s\n"), ads->auth.kdc_server );
	d_printf(_("Server time offset: %d\n"), ads->auth.time_offset );

	ads_destroy(&ads);
	return 0;
}

static void use_in_memory_ccache(void) {
	/* Use in-memory credentials cache so we do not interfere with
	 * existing credentials */
	setenv(KRB5_ENV_CCNAME, "MEMORY:net_ads", 1);
}

static ADS_STATUS ads_startup_int(struct net_context *c, bool only_own_domain,
				  uint32 auth_flags, ADS_STRUCT **ads_ret)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS status;
	bool need_password = false;
	bool second_time = false;
	char *cp;
	const char *realm = NULL;
	bool tried_closest_dc = false;

	/* lp_realm() should be handled by a command line param,
	   However, the join requires that realm be set in smb.conf
	   and compares our realm with the remote server's so this is
	   ok until someone needs more flexibility */

	*ads_ret = NULL;

retry_connect:
 	if (only_own_domain) {
		realm = lp_realm();
	} else {
		realm = assume_own_realm(c);
	}

	ads = ads_init(realm, c->opt_target_workgroup, c->opt_host);

	if (!c->opt_user_name) {
		c->opt_user_name = "administrator";
	}

	if (c->opt_user_specified) {
		need_password = true;
	}

retry:
	if (!c->opt_password && need_password && !c->opt_machine_pass) {
		c->opt_password = net_prompt_pass(c, c->opt_user_name);
		if (!c->opt_password) {
			ads_destroy(&ads);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

	if (c->opt_password) {
		use_in_memory_ccache();
		SAFE_FREE(ads->auth.password);
		ads->auth.password = smb_xstrdup(c->opt_password);
	}

	ads->auth.flags |= auth_flags;
	SAFE_FREE(ads->auth.user_name);
	ads->auth.user_name = smb_xstrdup(c->opt_user_name);

       /*
        * If the username is of the form "name@realm",
        * extract the realm and convert to upper case.
        * This is only used to establish the connection.
        */
       if ((cp = strchr_m(ads->auth.user_name, '@'))!=0) {
		*cp++ = '\0';
		SAFE_FREE(ads->auth.realm);
		ads->auth.realm = smb_xstrdup(cp);
		strupper_m(ads->auth.realm);
       }

	status = ads_connect(ads);

	if (!ADS_ERR_OK(status)) {

		if (NT_STATUS_EQUAL(ads_ntstatus(status),
				    NT_STATUS_NO_LOGON_SERVERS)) {
			DEBUG(0,("ads_connect: %s\n", ads_errstr(status)));
			ads_destroy(&ads);
			return status;
		}

		if (!need_password && !second_time && !(auth_flags & ADS_AUTH_NO_BIND)) {
			need_password = true;
			second_time = true;
			goto retry;
		} else {
			ads_destroy(&ads);
			return status;
		}
	}

	/* when contacting our own domain, make sure we use the closest DC.
	 * This is done by reconnecting to ADS because only the first call to
	 * ads_connect will give us our own sitename */

	if ((only_own_domain || !c->opt_host) && !tried_closest_dc) {

		tried_closest_dc = true; /* avoid loop */

		if (!ads_closest_dc(ads)) {

			namecache_delete(ads->server.realm, 0x1C);
			namecache_delete(ads->server.workgroup, 0x1C);

			ads_destroy(&ads);
			ads = NULL;

			goto retry_connect;
		}
	}

	*ads_ret = ads;
	return status;
}

ADS_STATUS ads_startup(struct net_context *c, bool only_own_domain, ADS_STRUCT **ads)
{
	return ads_startup_int(c, only_own_domain, 0, ads);
}

ADS_STATUS ads_startup_nobind(struct net_context *c, bool only_own_domain, ADS_STRUCT **ads)
{
	return ads_startup_int(c, only_own_domain, ADS_AUTH_NO_BIND, ads);
}

/*
  Check to see if connection can be made via ads.
  ads_startup() stores the password in opt_password if it needs to so
  that rpc or rap can use it without re-prompting.
*/
static int net_ads_check_int(const char *realm, const char *workgroup, const char *host)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;

	if ( (ads = ads_init( realm, workgroup, host )) == NULL ) {
		return -1;
	}

	ads->auth.flags |= ADS_AUTH_NO_BIND;

        status = ads_connect(ads);
        if ( !ADS_ERR_OK(status) ) {
                return -1;
        }

	ads_destroy(&ads);
	return 0;
}

int net_ads_check_our_domain(struct net_context *c)
{
	return net_ads_check_int(lp_realm(), lp_workgroup(), NULL);
}

int net_ads_check(struct net_context *c)
{
	return net_ads_check_int(NULL, c->opt_workgroup, c->opt_host);
}

/*
   determine the netbios workgroup name for a domain
 */
static int net_ads_workgroup(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	char addr[INET6_ADDRSTRLEN];
	struct NETLOGON_SAM_LOGON_RESPONSE_EX reply;

	if (c->display_usage) {
		d_printf  ("%s\n"
			   "net ads workgroup\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Print the workgroup name"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup_nobind(c, false, &ads))) {
		d_fprintf(stderr, _("Didn't find the cldap server!\n"));
		return -1;
	}

	if (!ads->config.realm) {
		ads->config.realm = CONST_DISCARD(char *, c->opt_target_workgroup);
		ads->ldap.port = 389;
	}

	print_sockaddr(addr, sizeof(addr), &ads->ldap.ss);
	if ( !ads_cldap_netlogon_5(talloc_tos(), addr, ads->server.realm, &reply ) ) {
		d_fprintf(stderr, _("CLDAP query failed!\n"));
		ads_destroy(&ads);
		return -1;
	}

	d_printf(_("Workgroup: %s\n"), reply.domain_name);

	ads_destroy(&ads);

	return 0;
}



static bool usergrp_display(ADS_STRUCT *ads, char *field, void **values, void *data_area)
{
	char **disp_fields = (char **) data_area;

	if (!field) { /* must be end of record */
		if (disp_fields[0]) {
			if (!strchr_m(disp_fields[0], '$')) {
				if (disp_fields[1])
					d_printf("%-21.21s %s\n",
					       disp_fields[0], disp_fields[1]);
				else
					d_printf("%s\n", disp_fields[0]);
			}
		}
		SAFE_FREE(disp_fields[0]);
		SAFE_FREE(disp_fields[1]);
		return true;
	}
	if (!values) /* must be new field, indicate string field */
		return true;
	if (StrCaseCmp(field, "sAMAccountName") == 0) {
		disp_fields[0] = SMB_STRDUP((char *) values[0]);
	}
	if (StrCaseCmp(field, "description") == 0)
		disp_fields[1] = SMB_STRDUP((char *) values[0]);
	return true;
}

static int net_ads_user_usage(struct net_context *c, int argc, const char **argv)
{
	return net_user_usage(c, argc, argv);
}

static int ads_user_add(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	char *upn, *userdn;
	LDAPMessage *res=NULL;
	int rc = -1;
	char *ou_str = NULL;

	if (argc < 1 || c->display_usage)
		return net_ads_user_usage(c, argc, argv);

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	status = ads_find_user_acct(ads, &res, argv[0]);

	if (!ADS_ERR_OK(status)) {
		d_fprintf(stderr, _("ads_user_add: %s\n"), ads_errstr(status));
		goto done;
	}

	if (ads_count_replies(ads, res)) {
		d_fprintf(stderr, _("ads_user_add: User %s already exists\n"),
			  argv[0]);
		goto done;
	}

	if (c->opt_container) {
		ou_str = SMB_STRDUP(c->opt_container);
	} else {
		ou_str = ads_default_ou_string(ads, DS_GUID_USERS_CONTAINER);
	}

	status = ads_add_user_acct(ads, argv[0], ou_str, c->opt_comment);

	if (!ADS_ERR_OK(status)) {
		d_fprintf(stderr, _("Could not add user %s: %s\n"), argv[0],
			 ads_errstr(status));
		goto done;
	}

	/* if no password is to be set, we're done */
	if (argc == 1) {
		d_printf(_("User %s added\n"), argv[0]);
		rc = 0;
		goto done;
	}

	/* try setting the password */
	if (asprintf(&upn, "%s@%s", argv[0], ads->config.realm) == -1) {
		goto done;
	}
	status = ads_krb5_set_password(ads->auth.kdc_server, upn, argv[1],
				       ads->auth.time_offset);
	SAFE_FREE(upn);
	if (ADS_ERR_OK(status)) {
		d_printf(_("User %s added\n"), argv[0]);
		rc = 0;
		goto done;
	}

	/* password didn't set, delete account */
	d_fprintf(stderr, _("Could not add user %s. "
			    "Error setting password %s\n"),
		 argv[0], ads_errstr(status));
	ads_msgfree(ads, res);
	status=ads_find_user_acct(ads, &res, argv[0]);
	if (ADS_ERR_OK(status)) {
		userdn = ads_get_dn(ads, talloc_tos(), res);
		ads_del_dn(ads, userdn);
		TALLOC_FREE(userdn);
	}

 done:
	if (res)
		ads_msgfree(ads, res);
	ads_destroy(&ads);
	SAFE_FREE(ou_str);
	return rc;
}

static int ads_user_info(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	TALLOC_CTX *frame;
	int ret = 0;
	wbcErr wbc_status;
	const char *attrs[] = {"memberOf", "primaryGroupID", NULL};
	char *searchstring=NULL;
	char **grouplist;
	char *primary_group;
	char *escaped_user;
	struct dom_sid primary_group_sid;
	uint32_t group_rid;
	enum wbcSidType type;

	if (argc < 1 || c->display_usage) {
		return net_ads_user_usage(c, argc, argv);
	}

	frame = talloc_new(talloc_tos());
	if (frame == NULL) {
		return -1;
	}

	escaped_user = escape_ldap_string(frame, argv[0]);
	if (!escaped_user) {
		d_fprintf(stderr,
			  _("ads_user_info: failed to escape user %s\n"),
			  argv[0]);
		return -1;
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		ret = -1;
		goto error;
	}

	if (asprintf(&searchstring, "(sAMAccountName=%s)", escaped_user) == -1) {
		ret =-1;
		goto error;
	}
	rc = ads_search(ads, &res, searchstring, attrs);
	SAFE_FREE(searchstring);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("ads_search: %s\n"), ads_errstr(rc));
		ret = -1;
		goto error;
	}

	if (!ads_pull_uint32(ads, res, "primaryGroupID", &group_rid)) {
		d_fprintf(stderr, _("ads_pull_uint32 failed\n"));
		ret = -1;
		goto error;
	}

	rc = ads_domain_sid(ads, &primary_group_sid);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("ads_domain_sid: %s\n"), ads_errstr(rc));
		ret = -1;
		goto error;
	}

	sid_append_rid(&primary_group_sid, group_rid);

	wbc_status = wbcLookupSid((struct wbcDomainSid *)&primary_group_sid,
				  NULL, /* don't look up domain */
				  &primary_group,
				  &type);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "wbcLookupSid: %s\n",
			  wbcErrorString(wbc_status));
		ret = -1;
		goto error;
	}

	d_printf("%s\n", primary_group);

	wbcFreeMemory(primary_group);

	grouplist = ldap_get_values((LDAP *)ads->ldap.ld,
				    (LDAPMessage *)res, "memberOf");

	if (grouplist) {
		int i;
		char **groupname;
		for (i=0;grouplist[i];i++) {
			groupname = ldap_explode_dn(grouplist[i], 1);
			d_printf("%s\n", groupname[0]);
			ldap_value_free(groupname);
		}
		ldap_value_free(grouplist);
	}

error:
	if (res) ads_msgfree(ads, res);
	if (ads) ads_destroy(&ads);
	TALLOC_FREE(frame);
	return ret;
}

static int ads_user_delete(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	char *userdn;

	if (argc < 1) {
		return net_ads_user_usage(c, argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	rc = ads_find_user_acct(ads, &res, argv[0]);
	if (!ADS_ERR_OK(rc) || ads_count_replies(ads, res) != 1) {
		d_printf(_("User %s does not exist.\n"), argv[0]);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}
	userdn = ads_get_dn(ads, talloc_tos(), res);
	ads_msgfree(ads, res);
	rc = ads_del_dn(ads, userdn);
	TALLOC_FREE(userdn);
	if (ADS_ERR_OK(rc)) {
		d_printf(_("User %s deleted\n"), argv[0]);
		ads_destroy(&ads);
		return 0;
	}
	d_fprintf(stderr, _("Error deleting user %s: %s\n"), argv[0],
		 ads_errstr(rc));
	ads_destroy(&ads);
	return -1;
}

int net_ads_user(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			ads_user_add,
			NET_TRANSPORT_ADS,
			N_("Add an AD user"),
			N_("net ads user add\n"
			   "    Add an AD user")
		},
		{
			"info",
			ads_user_info,
			NET_TRANSPORT_ADS,
			N_("Display information about an AD user"),
			N_("net ads user info\n"
			   "    Display information about an AD user")
		},
		{
			"delete",
			ads_user_delete,
			NET_TRANSPORT_ADS,
			N_("Delete an AD user"),
			N_("net ads user delete\n"
			   "    Delete an AD user")
		},
		{NULL, NULL, 0, NULL, NULL}
	};
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *shortattrs[] = {"sAMAccountName", NULL};
	const char *longattrs[] = {"sAMAccountName", "description", NULL};
	char *disp_fields[2] = {NULL, NULL};

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(  "%s\n"
			           "net ads user\n"
				   "    %s\n",
				 _("Usage:"),
				 _("List AD users"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
			return -1;
		}

		if (c->opt_long_list_entries)
			d_printf(_("\nUser name             Comment"
				   "\n-----------------------------\n"));

		rc = ads_do_search_all_fn(ads, ads->config.bind_path,
					  LDAP_SCOPE_SUBTREE,
					  "(objectCategory=user)",
					  c->opt_long_list_entries ? longattrs :
					  shortattrs, usergrp_display,
					  disp_fields);
		ads_destroy(&ads);
		return ADS_ERR_OK(rc) ? 0 : -1;
	}

	return net_run_function(c, argc, argv, "net ads user", func);
}

static int net_ads_group_usage(struct net_context *c, int argc, const char **argv)
{
	return net_group_usage(c, argc, argv);
}

static int ads_group_add(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	LDAPMessage *res=NULL;
	int rc = -1;
	char *ou_str = NULL;

	if (argc < 1 || c->display_usage) {
		return net_ads_group_usage(c, argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	status = ads_find_user_acct(ads, &res, argv[0]);

	if (!ADS_ERR_OK(status)) {
		d_fprintf(stderr, _("ads_group_add: %s\n"), ads_errstr(status));
		goto done;
	}

	if (ads_count_replies(ads, res)) {
		d_fprintf(stderr, _("ads_group_add: Group %s already exists\n"), argv[0]);
		goto done;
	}

	if (c->opt_container) {
		ou_str = SMB_STRDUP(c->opt_container);
	} else {
		ou_str = ads_default_ou_string(ads, DS_GUID_USERS_CONTAINER);
	}

	status = ads_add_group_acct(ads, argv[0], ou_str, c->opt_comment);

	if (ADS_ERR_OK(status)) {
		d_printf(_("Group %s added\n"), argv[0]);
		rc = 0;
	} else {
		d_fprintf(stderr, _("Could not add group %s: %s\n"), argv[0],
			 ads_errstr(status));
	}

 done:
	if (res)
		ads_msgfree(ads, res);
	ads_destroy(&ads);
	SAFE_FREE(ou_str);
	return rc;
}

static int ads_group_delete(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	char *groupdn;

	if (argc < 1 || c->display_usage) {
		return net_ads_group_usage(c, argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	rc = ads_find_user_acct(ads, &res, argv[0]);
	if (!ADS_ERR_OK(rc) || ads_count_replies(ads, res) != 1) {
		d_printf(_("Group %s does not exist.\n"), argv[0]);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}
	groupdn = ads_get_dn(ads, talloc_tos(), res);
	ads_msgfree(ads, res);
	rc = ads_del_dn(ads, groupdn);
	TALLOC_FREE(groupdn);
	if (ADS_ERR_OK(rc)) {
		d_printf(_("Group %s deleted\n"), argv[0]);
		ads_destroy(&ads);
		return 0;
	}
	d_fprintf(stderr, _("Error deleting group %s: %s\n"), argv[0],
		 ads_errstr(rc));
	ads_destroy(&ads);
	return -1;
}

int net_ads_group(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			ads_group_add,
			NET_TRANSPORT_ADS,
			N_("Add an AD group"),
			N_("net ads group add\n"
			   "    Add an AD group")
		},
		{
			"delete",
			ads_group_delete,
			NET_TRANSPORT_ADS,
			N_("Delete an AD group"),
			N_("net ads group delete\n"
			   "    Delete an AD group")
		},
		{NULL, NULL, 0, NULL, NULL}
	};
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *shortattrs[] = {"sAMAccountName", NULL};
	const char *longattrs[] = {"sAMAccountName", "description", NULL};
	char *disp_fields[2] = {NULL, NULL};

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(  "%s\n"
				   "net ads group\n"
				   "    %s\n",
				 _("Usage:"),
				 _("List AD groups"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
			return -1;
		}

		if (c->opt_long_list_entries)
			d_printf(_("\nGroup name            Comment"
				   "\n-----------------------------\n"));
		rc = ads_do_search_all_fn(ads, ads->config.bind_path,
					  LDAP_SCOPE_SUBTREE,
					  "(objectCategory=group)",
					  c->opt_long_list_entries ? longattrs :
					  shortattrs, usergrp_display,
					  disp_fields);

		ads_destroy(&ads);
		return ADS_ERR_OK(rc) ? 0 : -1;
	}
	return net_run_function(c, argc, argv, "net ads group", func);
}

static int net_ads_status(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads status\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Display machine account details"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		return -1;
	}

	rc = ads_find_machine_acct(ads, &res, global_myname());
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("ads_find_machine_acct: %s\n"), ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, _("No machine account for '%s' found\n"), global_myname());
		ads_destroy(&ads);
		return -1;
	}

	ads_dump(ads, res);
	ads_destroy(&ads);
	return 0;
}

/*******************************************************************
 Leave an AD domain.  Windows XP disables the machine account.
 We'll try the same.  The old code would do an LDAP delete.
 That only worked using the machine creds because added the machine
 with full control to the computer object's ACL.
*******************************************************************/

static int net_ads_leave(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *ctx;
	struct libnet_UnjoinCtx *r = NULL;
	WERROR werr;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads leave\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Leave an AD domain"));
		return 0;
	}

	if (!*lp_realm()) {
		d_fprintf(stderr, _("No realm set, are we joined ?\n"));
		return -1;
	}

	if (!(ctx = talloc_init("net_ads_leave"))) {
		d_fprintf(stderr, _("Could not initialise talloc context.\n"));
		return -1;
	}

	if (!c->opt_kerberos) {
		use_in_memory_ccache();
	}

	if (!c->msg_ctx) {
		d_fprintf(stderr, _("Could not initialise message context. "
			"Try running as root\n"));
		return -1;
	}

	werr = libnet_init_UnjoinCtx(ctx, &r);
	if (!W_ERROR_IS_OK(werr)) {
		d_fprintf(stderr, _("Could not initialise unjoin context.\n"));
		return -1;
	}

	r->in.debug		= true;
	r->in.use_kerberos	= c->opt_kerberos;
	r->in.dc_name		= c->opt_host;
	r->in.domain_name	= lp_realm();
	r->in.admin_account	= c->opt_user_name;
	r->in.admin_password	= net_prompt_pass(c, c->opt_user_name);
	r->in.modify_config	= lp_config_backend_is_registry();

	/* Try to delete it, but if that fails, disable it.  The
	   WKSSVC_JOIN_FLAGS_ACCOUNT_DELETE really means "disable */
	r->in.unjoin_flags	= WKSSVC_JOIN_FLAGS_JOIN_TYPE |
				  WKSSVC_JOIN_FLAGS_ACCOUNT_DELETE;
	r->in.delete_machine_account = true;
	r->in.msg_ctx		= c->msg_ctx;

	werr = libnet_Unjoin(ctx, r);
	if (!W_ERROR_IS_OK(werr)) {
		d_printf(_("Failed to leave domain: %s\n"),
			 r->out.error_string ? r->out.error_string :
			 get_friendly_werror_msg(werr));
		goto done;
	}

	if (r->out.deleted_machine_account) {
		d_printf(_("Deleted account for '%s' in realm '%s'\n"),
			r->in.machine_name, r->out.dns_domain_name);
		goto done;
	}

	/* We couldn't delete it - see if the disable succeeded. */
	if (r->out.disabled_machine_account) {
		d_printf(_("Disabled account for '%s' in realm '%s'\n"),
			r->in.machine_name, r->out.dns_domain_name);
		werr = WERR_OK;
		goto done;
	}

	/* Based on what we requseted, we shouldn't get here, but if
	   we did, it means the secrets were removed, and therefore
	   we have left the domain */
	d_fprintf(stderr, _("Machine '%s' Left domain '%s'\n"),
		  r->in.machine_name, r->out.dns_domain_name);

 done:
	TALLOC_FREE(r);
	TALLOC_FREE(ctx);

	if (W_ERROR_IS_OK(werr)) {
		return 0;
	}

	return -1;
}

static NTSTATUS net_ads_join_ok(struct net_context *c)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS status;
	fstring dc_name;
	struct sockaddr_storage dcip;

	if (!secrets_init()) {
		DEBUG(1,("Failed to initialise secrets database\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	net_use_krb_machine_account(c);

	get_dc_name(lp_workgroup(), lp_realm(), dc_name, &dcip);

	status = ads_startup(c, true, &ads);
	if (!ADS_ERR_OK(status)) {
		return ads_ntstatus(status);
	}

	ads_destroy(&ads);
	return NT_STATUS_OK;
}

/*
  check that an existing join is OK
 */
int net_ads_testjoin(struct net_context *c, int argc, const char **argv)
{
	NTSTATUS status;
	use_in_memory_ccache();

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads testjoin\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Test if the existing join is ok"));
		return 0;
	}

	/* Display success or failure */
	status = net_ads_join_ok(c);
	if (!NT_STATUS_IS_OK(status)) {
		fprintf(stderr, _("Join to domain is not valid: %s\n"),
			get_friendly_nt_error_msg(status));
		return -1;
	}

	printf(_("Join is OK\n"));
	return 0;
}

/*******************************************************************
  Simple configu checks before beginning the join
 ********************************************************************/

static WERROR check_ads_config( void )
{
	if (lp_server_role() != ROLE_DOMAIN_MEMBER ) {
		d_printf(_("Host is not configured as a member server.\n"));
		return WERR_INVALID_DOMAIN_ROLE;
	}

	if (strlen(global_myname()) > 15) {
		d_printf(_("Our netbios name can be at most 15 chars long, "
			   "\"%s\" is %u chars long\n"), global_myname(),
			 (unsigned int)strlen(global_myname()));
		return WERR_INVALID_COMPUTERNAME;
	}

	if ( lp_security() == SEC_ADS && !*lp_realm()) {
		d_fprintf(stderr, _("realm must be set in in %s for ADS "
			  "join to succeed.\n"), get_dyn_CONFIGFILE());
		return WERR_INVALID_PARAM;
	}

	return WERR_OK;
}

/*******************************************************************
 Send a DNS update request
*******************************************************************/

#if defined(WITH_DNS_UPDATES)
#include "../lib/addns/dns.h"

static NTSTATUS net_update_dns_internal(struct net_context *c,
					TALLOC_CTX *ctx, ADS_STRUCT *ads,
					const char *machine_name,
					const struct sockaddr_storage *addrs,
					int num_addrs)
{
	struct dns_rr_ns *nameservers = NULL;
	int ns_count = 0, i;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	DNS_ERROR dns_err;
	fstring dns_server;
	const char *dnsdomain = NULL;
	char *root_domain = NULL;

	if ( (dnsdomain = strchr_m( machine_name, '.')) == NULL ) {
		d_printf(_("No DNS domain configured for %s. "
			   "Unable to perform DNS Update.\n"), machine_name);
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}
	dnsdomain++;

	status = ads_dns_lookup_ns( ctx, dnsdomain, &nameservers, &ns_count );
	if ( !NT_STATUS_IS_OK(status) || (ns_count == 0)) {
		/* Child domains often do not have NS records.  Look
		   for the NS record for the forest root domain
		   (rootDomainNamingContext in therootDSE) */

		const char *rootname_attrs[] = 	{ "rootDomainNamingContext", NULL };
		LDAPMessage *msg = NULL;
		char *root_dn;
		ADS_STATUS ads_status;

		if ( !ads->ldap.ld ) {
			ads_status = ads_connect( ads );
			if ( !ADS_ERR_OK(ads_status) ) {
				DEBUG(0,("net_update_dns_internal: Failed to connect to our DC!\n"));
				goto done;
			}
		}

		ads_status = ads_do_search(ads, "", LDAP_SCOPE_BASE,
				       "(objectclass=*)", rootname_attrs, &msg);
		if (!ADS_ERR_OK(ads_status)) {
			goto done;
		}

		root_dn = ads_pull_string(ads, ctx, msg,  "rootDomainNamingContext");
		if ( !root_dn ) {
			ads_msgfree( ads, msg );
			goto done;
		}

		root_domain = ads_build_domain( root_dn );

		/* cleanup */
		ads_msgfree( ads, msg );

		/* try again for NS servers */

		status = ads_dns_lookup_ns( ctx, root_domain, &nameservers, &ns_count );

		if ( !NT_STATUS_IS_OK(status) || (ns_count == 0)) {
			DEBUG(3,("net_update_dns_internal: Failed to find name server for the %s "
			 "realm\n", ads->config.realm));
			goto done;
		}

		dnsdomain = root_domain;

	}

	for (i=0; i < ns_count; i++) {

		uint32_t flags = DNS_UPDATE_SIGNED |
				 DNS_UPDATE_UNSIGNED |
				 DNS_UPDATE_UNSIGNED_SUFFICIENT |
				 DNS_UPDATE_PROBE |
				 DNS_UPDATE_PROBE_SUFFICIENT;

		if (c->opt_force) {
			flags &= ~DNS_UPDATE_PROBE_SUFFICIENT;
			flags &= ~DNS_UPDATE_UNSIGNED_SUFFICIENT;
		}

		status = NT_STATUS_UNSUCCESSFUL;

		/* Now perform the dns update - we'll try non-secure and if we fail,
		   we'll follow it up with a secure update */

		fstrcpy( dns_server, nameservers[i].hostname );

		dns_err = DoDNSUpdate(dns_server, dnsdomain, machine_name, addrs, num_addrs, flags);
		if (ERR_DNS_IS_OK(dns_err)) {
			status = NT_STATUS_OK;
			goto done;
		}

		if (ERR_DNS_EQUAL(dns_err, ERROR_DNS_INVALID_NAME_SERVER) ||
		    ERR_DNS_EQUAL(dns_err, ERROR_DNS_CONNECTION_FAILED) ||
		    ERR_DNS_EQUAL(dns_err, ERROR_DNS_SOCKET_ERROR)) {
			DEBUG(1,("retrying DNS update with next nameserver after receiving %s\n",
				dns_errstr(dns_err)));
			continue;
		}

		d_printf(_("DNS Update for %s failed: %s\n"),
			machine_name, dns_errstr(dns_err));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

done:

	SAFE_FREE( root_domain );

	return status;
}

static NTSTATUS net_update_dns_ext(struct net_context *c,
				   TALLOC_CTX *mem_ctx, ADS_STRUCT *ads,
				   const char *hostname,
				   struct sockaddr_storage *iplist,
				   int num_addrs)
{
	struct sockaddr_storage *iplist_alloc = NULL;
	fstring machine_name;
	NTSTATUS status;

	if (hostname) {
		fstrcpy(machine_name, hostname);
	} else {
		name_to_fqdn( machine_name, global_myname() );
	}
	strlower_m( machine_name );

	if (num_addrs == 0 || iplist == NULL) {
		/*
		 * Get our ip address
		 * (not the 127.0.0.x address but a real ip address)
		 */
		num_addrs = get_my_ip_address(&iplist_alloc);
		if ( num_addrs <= 0 ) {
			DEBUG(4, ("net_update_dns_ext: Failed to find my "
				  "non-loopback IP addresses!\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
		iplist = iplist_alloc;
	}

	status = net_update_dns_internal(c, mem_ctx, ads, machine_name,
					 iplist, num_addrs);

	SAFE_FREE(iplist_alloc);
	return status;
}

static NTSTATUS net_update_dns(struct net_context *c, TALLOC_CTX *mem_ctx, ADS_STRUCT *ads, const char *hostname)
{
	NTSTATUS status;

	status = net_update_dns_ext(c, mem_ctx, ads, hostname, NULL, 0);
	return status;
}
#endif


/*******************************************************************
 ********************************************************************/

static int net_ads_join_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net ads join [options]\n"
	           "Valid options:\n"));
	d_printf(_("   createupn[=UPN]    Set the userPrincipalName attribute during the join.\n"
		   "                      The deault UPN is in the form host/netbiosname@REALM.\n"));
	d_printf(_("   createcomputer=OU  Precreate the computer account in a specific OU.\n"
		   "                      The OU string read from top to bottom without RDNs and delimited by a '/'.\n"
		   "                      E.g. \"createcomputer=Computers/Servers/Unix\"\n"
		   "                      NB: A backslash '\\' is used as escape at multiple levels and may\n"
		   "                          need to be doubled or even quadrupled.  It is not used as a separator.\n"));
	d_printf(_("   osName=string      Set the operatingSystem attribute during the join.\n"));
	d_printf(_("   osVer=string       Set the operatingSystemVersion attribute during the join.\n"
		   "                      NB: osName and osVer must be specified together for either to take effect.\n"
		   "                          Also, the operatingSystemService attribute is also set when along with\n"
		   "                          the two other attributes.\n"));

	return -1;
}

/*******************************************************************
 ********************************************************************/

int net_ads_join(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *ctx = NULL;
	struct libnet_JoinCtx *r = NULL;
	const char *domain = lp_realm();
	WERROR werr = WERR_SETUP_NOT_JOINED;
	bool createupn = false;
	const char *machineupn = NULL;
	const char *create_in_ou = NULL;
	int i;
	const char *os_name = NULL;
	const char *os_version = NULL;
	bool modify_config = lp_config_backend_is_registry();

	if (c->display_usage)
		return net_ads_join_usage(c, argc, argv);

	if (!modify_config) {

		werr = check_ads_config();
		if (!W_ERROR_IS_OK(werr)) {
			d_fprintf(stderr, _("Invalid configuration.  Exiting....\n"));
			goto fail;
		}
	}

	if (!(ctx = talloc_init("net_ads_join"))) {
		d_fprintf(stderr, _("Could not initialise talloc context.\n"));
		werr = WERR_NOMEM;
		goto fail;
	}

	if (!c->opt_kerberos) {
		use_in_memory_ccache();
	}

	werr = libnet_init_JoinCtx(ctx, &r);
	if (!W_ERROR_IS_OK(werr)) {
		goto fail;
	}

	/* process additional command line args */

	for ( i=0; i<argc; i++ ) {
		if ( !StrnCaseCmp(argv[i], "createupn", strlen("createupn")) ) {
			createupn = true;
			machineupn = get_string_param(argv[i]);
		}
		else if ( !StrnCaseCmp(argv[i], "createcomputer", strlen("createcomputer")) ) {
			if ( (create_in_ou = get_string_param(argv[i])) == NULL ) {
				d_fprintf(stderr, _("Please supply a valid OU path.\n"));
				werr = WERR_INVALID_PARAM;
				goto fail;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "osName", strlen("osName")) ) {
			if ( (os_name = get_string_param(argv[i])) == NULL ) {
				d_fprintf(stderr, _("Please supply a operating system name.\n"));
				werr = WERR_INVALID_PARAM;
				goto fail;
			}
		}
		else if ( !StrnCaseCmp(argv[i], "osVer", strlen("osVer")) ) {
			if ( (os_version = get_string_param(argv[i])) == NULL ) {
				d_fprintf(stderr, _("Please supply a valid operating system version.\n"));
				werr = WERR_INVALID_PARAM;
				goto fail;
			}
		}
		else {
			domain = argv[i];
		}
	}

	if (!*domain) {
		d_fprintf(stderr, _("Please supply a valid domain name\n"));
		werr = WERR_INVALID_PARAM;
		goto fail;
	}

	if (!c->msg_ctx) {
		d_fprintf(stderr, _("Could not initialise message context. "
			"Try running as root\n"));
		werr = WERR_ACCESS_DENIED;
		goto fail;
	}

	/* Do the domain join here */

	r->in.domain_name	= domain;
	r->in.create_upn	= createupn;
	r->in.upn		= machineupn;
	r->in.account_ou	= create_in_ou;
	r->in.os_name		= os_name;
	r->in.os_version	= os_version;
	r->in.dc_name		= c->opt_host;
	r->in.admin_account	= c->opt_user_name;
	r->in.admin_password	= net_prompt_pass(c, c->opt_user_name);
	r->in.debug		= true;
	r->in.use_kerberos	= c->opt_kerberos;
	r->in.modify_config	= modify_config;
	r->in.join_flags	= WKSSVC_JOIN_FLAGS_JOIN_TYPE |
				  WKSSVC_JOIN_FLAGS_ACCOUNT_CREATE |
				  WKSSVC_JOIN_FLAGS_DOMAIN_JOIN_IF_JOINED;
	r->in.msg_ctx		= c->msg_ctx;

	werr = libnet_Join(ctx, r);
	if (W_ERROR_EQUAL(werr, WERR_DCNOTFOUND) &&
	    strequal(domain, lp_realm())) {
		r->in.domain_name = lp_workgroup();
		werr = libnet_Join(ctx, r);
	}
	if (!W_ERROR_IS_OK(werr)) {
		goto fail;
	}

	/* Check the short name of the domain */

	if (!modify_config && !strequal(lp_workgroup(), r->out.netbios_domain_name)) {
		d_printf(_("The workgroup in %s does not match the short\n"
			   "domain name obtained from the server.\n"
			   "Using the name [%s] from the server.\n"
			   "You should set \"workgroup = %s\" in %s.\n"),
			 get_dyn_CONFIGFILE(), r->out.netbios_domain_name,
			 r->out.netbios_domain_name, get_dyn_CONFIGFILE());
	}

	d_printf(_("Using short domain name -- %s\n"), r->out.netbios_domain_name);

	if (r->out.dns_domain_name) {
		d_printf(_("Joined '%s' to dns domain '%s'\n"), r->in.machine_name,
			r->out.dns_domain_name);
	} else {
		d_printf(_("Joined '%s' to domain '%s'\n"), r->in.machine_name,
			r->out.netbios_domain_name);
	}

#if defined(WITH_DNS_UPDATES)
	/*
	 * In a clustered environment, don't do dynamic dns updates:
	 * Registering the set of ip addresses that are assigned to
	 * the interfaces of the node that performs the join does usually
	 * not have the desired effect, since the local interfaces do not
	 * carry the complete set of the cluster's public IP addresses.
	 * And it can also contain internal addresses that should not
	 * be visible to the outside at all.
	 * In order to do dns updates in a clustererd setup, use
	 * net ads dns register.
	 */
	if (lp_clustering()) {
		d_fprintf(stderr, _("Not doing automatic DNS update in a"
				    "clustered setup.\n"));
		goto done;
	}

	if (r->out.domain_is_ad) {
		/* We enter this block with user creds */
		ADS_STRUCT *ads_dns = NULL;

		if ( (ads_dns = ads_init( lp_realm(), NULL, NULL )) != NULL ) {
			/* kinit with the machine password */

			use_in_memory_ccache();
			if (asprintf( &ads_dns->auth.user_name, "%s$", global_myname()) == -1) {
				goto fail;
			}
			ads_dns->auth.password = secrets_fetch_machine_password(
				r->out.netbios_domain_name, NULL, NULL );
			ads_dns->auth.realm = SMB_STRDUP( r->out.dns_domain_name );
			strupper_m(ads_dns->auth.realm );
			ads_kinit_password( ads_dns );
		}

		if ( !ads_dns || !NT_STATUS_IS_OK(net_update_dns(c, ctx, ads_dns, NULL)) ) {
			d_fprintf( stderr, _("DNS update failed!\n") );
		}

		/* exit from this block using machine creds */
		ads_destroy(&ads_dns);
	}

done:
#endif

	TALLOC_FREE(r);
	TALLOC_FREE( ctx );

	return 0;

fail:
	/* issue an overall failure message at the end. */
	d_printf(_("Failed to join domain: %s\n"),
		r && r->out.error_string ? r->out.error_string :
		get_friendly_werror_msg(werr));
	TALLOC_FREE( ctx );

        return -1;
}

/*******************************************************************
 ********************************************************************/

static int net_ads_dns_register(struct net_context *c, int argc, const char **argv)
{
#if defined(WITH_DNS_UPDATES)
	ADS_STRUCT *ads;
	ADS_STATUS status;
	NTSTATUS ntstatus;
	TALLOC_CTX *ctx;
	const char *hostname = NULL;
	const char **addrs_list = NULL;
	struct sockaddr_storage *addrs = NULL;
	int num_addrs = 0;
	int count;

#ifdef DEVELOPER
	talloc_enable_leak_report();
#endif

	if (argc <= 1 && lp_clustering() && lp_cluster_addresses() == NULL) {
		d_fprintf(stderr, _("Refusing DNS updates with automatic "
				    "detection of addresses in a clustered "
				    "setup.\n"));
		c->display_usage = true;
	}

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads dns register [hostname [IP [IP...]]]\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Register hostname with DNS\n"));
		return -1;
	}

	if (!(ctx = talloc_init("net_ads_dns"))) {
		d_fprintf(stderr, _("Could not initialise talloc context\n"));
		return -1;
	}

	if (argc >= 1) {
		hostname = argv[0];
	}

	if (argc > 1) {
		num_addrs = argc - 1;
		addrs_list = &argv[1];
	} else if (lp_clustering()) {
		addrs_list = lp_cluster_addresses();
		num_addrs = str_list_length(addrs_list);
	}

	if (num_addrs > 0) {
		addrs = talloc_zero_array(ctx, struct sockaddr_storage, num_addrs);
		if (addrs == NULL) {
			d_fprintf(stderr, _("Error allocating memory!\n"));
			talloc_free(ctx);
			return -1;
		}
	}

	for (count = 0; count < num_addrs; count++) {
		if (!interpret_string_addr(&addrs[count], addrs_list[count], 0)) {
			d_fprintf(stderr, "%s '%s'.\n",
					  _("Cannot interpret address"),
					  addrs_list[count]);
			talloc_free(ctx);
			return -1;
		}
	}

	status = ads_startup(c, true, &ads);
	if ( !ADS_ERR_OK(status) ) {
		DEBUG(1, ("error on ads_startup: %s\n", ads_errstr(status)));
		TALLOC_FREE(ctx);
		return -1;
	}

	ntstatus = net_update_dns_ext(c, ctx, ads, hostname, addrs, num_addrs);
	if (!NT_STATUS_IS_OK(ntstatus)) {
		d_fprintf( stderr, _("DNS update failed!\n") );
		ads_destroy( &ads );
		TALLOC_FREE( ctx );
		return -1;
	}

	d_fprintf( stderr, _("Successfully registered hostname with DNS\n") );

	ads_destroy(&ads);
	TALLOC_FREE( ctx );

	return 0;
#else
	d_fprintf(stderr,
		  _("DNS update support not enabled at compile time!\n"));
	return -1;
#endif
}

static int net_ads_dns_gethostbyname(struct net_context *c, int argc, const char **argv)
{
#if defined(WITH_DNS_UPDATES)
	DNS_ERROR err;

#ifdef DEVELOPER
	talloc_enable_leak_report();
#endif

	if (argc != 2 || c->display_usage) {
		d_printf(  "%s\n"
			   "    %s\n"
			   "    %s\n",
			 _("Usage:"),
			 _("net ads dns gethostbyname <server> <name>\n"),
			 _("  Look up hostname from the AD\n"
			   "    server\tName server to use\n"
			   "    name\tName to look up\n"));
		return -1;
	}

	err = do_gethostbyname(argv[0], argv[1]);

	d_printf(_("do_gethostbyname returned %s (%d)\n"),
		dns_errstr(err), ERROR_DNS_V(err));
#endif
	return 0;
}

static int net_ads_dns(struct net_context *c, int argc, const char *argv[])
{
	struct functable func[] = {
		{
			"register",
			net_ads_dns_register,
			NET_TRANSPORT_ADS,
			N_("Add host dns entry to AD"),
			N_("net ads dns register\n"
			   "    Add host dns entry to AD")
		},
		{
			"gethostbyname",
			net_ads_dns_gethostbyname,
			NET_TRANSPORT_ADS,
			N_("Look up host"),
			N_("net ads dns gethostbyname\n"
			   "    Look up host")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net ads dns", func);
}

/*******************************************************************
 ********************************************************************/

int net_ads_printer_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
"\nnet ads printer search <printer>"
"\n\tsearch for a printer in the directory\n"
"\nnet ads printer info <printer> <server>"
"\n\tlookup info in directory for printer on server"
"\n\t(note: printer defaults to \"*\", server defaults to local)\n"
"\nnet ads printer publish <printername>"
"\n\tpublish printer in directory"
"\n\t(note: printer name is required)\n"
"\nnet ads printer remove <printername>"
"\n\tremove printer from directory"
"\n\t(note: printer name is required)\n"));
	return -1;
}

/*******************************************************************
 ********************************************************************/

static int net_ads_printer_search(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads printer search\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List printers in the AD"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	rc = ads_find_printers(ads, &res);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("ads_find_printer: %s\n"), ads_errstr(rc));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
	 	return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, _("No results found\n"));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	ads_dump(ads, res);
	ads_msgfree(ads, res);
	ads_destroy(&ads);
	return 0;
}

static int net_ads_printer_info(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *servername, *printername;
	LDAPMessage *res = NULL;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net ads printer info [printername [servername]]\n"
			   "  Display printer info from AD\n"
			   "    printername\tPrinter name or wildcard\n"
			   "    servername\tName of the print server\n"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	if (argc > 0) {
		printername = argv[0];
	} else {
		printername = "*";
	}

	if (argc > 1) {
		servername =  argv[1];
	} else {
		servername = global_myname();
	}

	rc = ads_find_printer_on_server(ads, &res, printername, servername);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("Server '%s' not found: %s\n"),
			servername, ads_errstr(rc));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, _("Printer '%s' not found\n"), printername);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	ads_dump(ads, res);
	ads_msgfree(ads, res);
	ads_destroy(&ads);

	return 0;
}

static int net_ads_printer_publish(struct net_context *c, int argc, const char **argv)
{
        ADS_STRUCT *ads;
        ADS_STATUS rc;
	const char *servername, *printername;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct sockaddr_storage server_ss;
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx = talloc_init("net_ads_printer_publish");
	ADS_MODLIST mods = ads_init_mods(mem_ctx);
	char *prt_dn, *srv_dn, **srv_cn;
	char *srv_cn_escaped = NULL, *printername_escaped = NULL;
	LDAPMessage *res = NULL;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net ads printer publish <printername> [servername]\n"
			   "  Publish printer in AD\n"
			   "    printername\tName of the printer\n"
			   "    servername\tName of the print server\n"));
		talloc_destroy(mem_ctx);
		return -1;
	}

	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		talloc_destroy(mem_ctx);
		return -1;
	}

	printername = argv[0];

	if (argc == 2) {
		servername = argv[1];
	} else {
		servername = global_myname();
	}

	/* Get printer data from SPOOLSS */

	resolve_name(servername, &server_ss, 0x20, false);

	nt_status = cli_full_connection(&cli, global_myname(), servername,
					&server_ss, 0,
					"IPC$", "IPC",
					c->opt_user_name, c->opt_workgroup,
					c->opt_password ? c->opt_password : "",
					CLI_FULL_CONNECTION_USE_KERBEROS,
					Undefined);

	if (NT_STATUS_IS_ERR(nt_status)) {
		d_fprintf(stderr, _("Unable to open a connection to %s to "
			            "obtain data for %s\n"),
			  servername, printername);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	/* Publish on AD server */

	ads_find_machine_acct(ads, &res, servername);

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, _("Could not find machine account for server "
				    "%s\n"),
			 servername);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	srv_dn = ldap_get_dn((LDAP *)ads->ldap.ld, (LDAPMessage *)res);
	srv_cn = ldap_explode_dn(srv_dn, 1);

	srv_cn_escaped = escape_rdn_val_string_alloc(srv_cn[0]);
	printername_escaped = escape_rdn_val_string_alloc(printername);
	if (!srv_cn_escaped || !printername_escaped) {
		SAFE_FREE(srv_cn_escaped);
		SAFE_FREE(printername_escaped);
		d_fprintf(stderr, _("Internal error, out of memory!"));
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	if (asprintf(&prt_dn, "cn=%s-%s,%s", srv_cn_escaped, printername_escaped, srv_dn) == -1) {
		SAFE_FREE(srv_cn_escaped);
		SAFE_FREE(printername_escaped);
		d_fprintf(stderr, _("Internal error, out of memory!"));
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	SAFE_FREE(srv_cn_escaped);
	SAFE_FREE(printername_escaped);

	nt_status = cli_rpc_pipe_open_noauth(cli, &ndr_table_spoolss.syntax_id, &pipe_hnd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		d_fprintf(stderr, _("Unable to open a connection to the spoolss pipe on %s\n"),
			 servername);
		SAFE_FREE(prt_dn);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	if (!W_ERROR_IS_OK(get_remote_printer_publishing_data(pipe_hnd, mem_ctx, &mods,
							      printername))) {
		SAFE_FREE(prt_dn);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

        rc = ads_add_printer_entry(ads, prt_dn, mem_ctx, &mods);
        if (!ADS_ERR_OK(rc)) {
                d_fprintf(stderr, "ads_publish_printer: %s\n", ads_errstr(rc));
		SAFE_FREE(prt_dn);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
                return -1;
        }

        d_printf("published printer\n");
	SAFE_FREE(prt_dn);
	ads_destroy(&ads);
	talloc_destroy(mem_ctx);

	return 0;
}

static int net_ads_printer_remove(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *servername;
	char *prt_dn;
	LDAPMessage *res = NULL;

	if (argc < 1 || c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net ads printer remove <printername> [servername]\n"
			   "  Remove a printer from the AD\n"
			   "    printername\tName of the printer\n"
			   "    servername\tName of the print server\n"));
		return -1;
	}

	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		return -1;
	}

	if (argc > 1) {
		servername = argv[1];
	} else {
		servername = global_myname();
	}

	rc = ads_find_printer_on_server(ads, &res, argv[0], servername);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("ads_find_printer_on_server: %s\n"), ads_errstr(rc));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, _("Printer '%s' not found\n"), argv[1]);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	prt_dn = ads_get_dn(ads, talloc_tos(), res);
	ads_msgfree(ads, res);
	rc = ads_del_dn(ads, prt_dn);
	TALLOC_FREE(prt_dn);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("ads_del_dn: %s\n"), ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	ads_destroy(&ads);
	return 0;
}

static int net_ads_printer(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"search",
			net_ads_printer_search,
			NET_TRANSPORT_ADS,
			N_("Search for a printer"),
			N_("net ads printer search\n"
			   "    Search for a printer")
		},
		{
			"info",
			net_ads_printer_info,
			NET_TRANSPORT_ADS,
			N_("Display printer information"),
			N_("net ads printer info\n"
			   "    Display printer information")
		},
		{
			"publish",
			net_ads_printer_publish,
			NET_TRANSPORT_ADS,
			N_("Publish a printer"),
			N_("net ads printer publish\n"
			   "    Publish a printer")
		},
		{
			"remove",
			net_ads_printer_remove,
			NET_TRANSPORT_ADS,
			N_("Delete a printer"),
			N_("net ads printer remove\n"
			   "    Delete a printer")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net ads printer", func);
}


static int net_ads_password(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	const char *auth_principal = c->opt_user_name;
	const char *auth_password = c->opt_password;
	char *realm = NULL;
	char *new_password = NULL;
	char *chr, *prompt;
	const char *user;
	ADS_STATUS ret;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net ads password <username>\n"
			   "  Change password for user\n"
			   "    username\tName of user to change password for\n"));
		return 0;
	}

	if (c->opt_user_name == NULL || c->opt_password == NULL) {
		d_fprintf(stderr, _("You must supply an administrator "
				    "username/password\n"));
		return -1;
	}

	if (argc < 1) {
		d_fprintf(stderr, _("ERROR: You must say which username to "
				    "change password for\n"));
		return -1;
	}

	user = argv[0];
	if (!strchr_m(user, '@')) {
		if (asprintf(&chr, "%s@%s", argv[0], lp_realm()) == -1) {
			return -1;
		}
		user = chr;
	}

	use_in_memory_ccache();
	chr = strchr_m(auth_principal, '@');
	if (chr) {
		realm = ++chr;
	} else {
		realm = lp_realm();
	}

	/* use the realm so we can eventually change passwords for users
	in realms other than default */
	if (!(ads = ads_init(realm, c->opt_workgroup, c->opt_host))) {
		return -1;
	}

	/* we don't actually need a full connect, but it's the easy way to
		fill in the KDC's addresss */
	ads_connect(ads);

	if (!ads->config.realm) {
		d_fprintf(stderr, _("Didn't find the kerberos server!\n"));
		ads_destroy(&ads);
		return -1;
	}

	if (argv[1]) {
		new_password = (char *)argv[1];
	} else {
		if (asprintf(&prompt, _("Enter new password for %s:"), user) == -1) {
			return -1;
		}
		new_password = getpass(prompt);
		free(prompt);
	}

	ret = kerberos_set_password(ads->auth.kdc_server, auth_principal,
				auth_password, user, new_password, ads->auth.time_offset);
	if (!ADS_ERR_OK(ret)) {
		d_fprintf(stderr, _("Password change failed: %s\n"), ads_errstr(ret));
		ads_destroy(&ads);
		return -1;
	}

	d_printf(_("Password change for %s completed.\n"), user);
	ads_destroy(&ads);

	return 0;
}

int net_ads_changetrustpw(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	char *host_principal;
	fstring my_name;
	ADS_STATUS ret;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads changetrustpw\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Change the machine account's trust password"));
		return 0;
	}

	if (!secrets_init()) {
		DEBUG(1,("Failed to initialise secrets database\n"));
		return -1;
	}

	net_use_krb_machine_account(c);

	use_in_memory_ccache();

	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		return -1;
	}

	fstrcpy(my_name, global_myname());
	strlower_m(my_name);
	if (asprintf(&host_principal, "%s$@%s", my_name, ads->config.realm) == -1) {
		ads_destroy(&ads);
		return -1;
	}
	d_printf(_("Changing password for principal: %s\n"), host_principal);

	ret = ads_change_trust_account_password(ads, host_principal);

	if (!ADS_ERR_OK(ret)) {
		d_fprintf(stderr, _("Password change failed: %s\n"), ads_errstr(ret));
		ads_destroy(&ads);
		SAFE_FREE(host_principal);
		return -1;
	}

	d_printf(_("Password change for principal %s succeeded.\n"), host_principal);

	if (USE_SYSTEM_KEYTAB) {
		d_printf(_("Attempting to update system keytab with new password.\n"));
		if (ads_keytab_create_default(ads)) {
			d_printf(_("Failed to update system keytab.\n"));
		}
	}

	ads_destroy(&ads);
	SAFE_FREE(host_principal);

	return 0;
}

/*
  help for net ads search
*/
static int net_ads_search_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
		"\nnet ads search <expression> <attributes...>\n"
		"\nPerform a raw LDAP search on a ADS server and dump the results.\n"
		"The expression is a standard LDAP search expression, and the\n"
		"attributes are a list of LDAP fields to show in the results.\n\n"
		"Example: net ads search '(objectCategory=group)' sAMAccountName\n\n"
		));
	net_common_flags_usage(c, argc, argv);
	return -1;
}


/*
  general ADS search function. Useful in diagnosing problems in ADS
*/
static int net_ads_search(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *ldap_exp;
	const char **attrs;
	LDAPMessage *res = NULL;

	if (argc < 1 || c->display_usage) {
		return net_ads_search_usage(c, argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	ldap_exp = argv[0];
	attrs = (argv + 1);

	rc = ads_do_search_retry(ads, ads->config.bind_path,
			       LDAP_SCOPE_SUBTREE,
			       ldap_exp, attrs, &res);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("search failed: %s\n"), ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	d_printf(_("Got %d replies\n\n"), ads_count_replies(ads, res));

	/* dump the results */
	ads_dump(ads, res);

	ads_msgfree(ads, res);
	ads_destroy(&ads);

	return 0;
}


/*
  help for net ads search
*/
static int net_ads_dn_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
		"\nnet ads dn <dn> <attributes...>\n"
		"\nperform a raw LDAP search on a ADS server and dump the results\n"
		"The DN standard LDAP DN, and the attributes are a list of LDAP fields \n"
		"to show in the results\n\n"
		"Example: net ads dn 'CN=administrator,CN=Users,DC=my,DC=domain' sAMAccountName\n\n"
		"Note: the DN must be provided properly escaped. See RFC 4514 for details\n\n"
		));
	net_common_flags_usage(c, argc, argv);
	return -1;
}


/*
  general ADS search function. Useful in diagnosing problems in ADS
*/
static int net_ads_dn(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *dn;
	const char **attrs;
	LDAPMessage *res = NULL;

	if (argc < 1 || c->display_usage) {
		return net_ads_dn_usage(c, argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	dn = argv[0];
	attrs = (argv + 1);

	rc = ads_do_search_all(ads, dn,
			       LDAP_SCOPE_BASE,
			       "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("search failed: %s\n"), ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	d_printf("Got %d replies\n\n", ads_count_replies(ads, res));

	/* dump the results */
	ads_dump(ads, res);

	ads_msgfree(ads, res);
	ads_destroy(&ads);

	return 0;
}

/*
  help for net ads sid search
*/
static int net_ads_sid_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
		"\nnet ads sid <sid> <attributes...>\n"
		"\nperform a raw LDAP search on a ADS server and dump the results\n"
		"The SID is in string format, and the attributes are a list of LDAP fields \n"
		"to show in the results\n\n"
		"Example: net ads sid 'S-1-5-32' distinguishedName\n\n"
		));
	net_common_flags_usage(c, argc, argv);
	return -1;
}


/*
  general ADS search function. Useful in diagnosing problems in ADS
*/
static int net_ads_sid(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *sid_string;
	const char **attrs;
	LDAPMessage *res = NULL;
	struct dom_sid sid;

	if (argc < 1 || c->display_usage) {
		return net_ads_sid_usage(c, argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(c, false, &ads))) {
		return -1;
	}

	sid_string = argv[0];
	attrs = (argv + 1);

	if (!string_to_sid(&sid, sid_string)) {
		d_fprintf(stderr, _("could not convert sid\n"));
		ads_destroy(&ads);
		return -1;
	}

	rc = ads_search_retry_sid(ads, &res, &sid, attrs);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, _("search failed: %s\n"), ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	d_printf(_("Got %d replies\n\n"), ads_count_replies(ads, res));

	/* dump the results */
	ads_dump(ads, res);

	ads_msgfree(ads, res);
	ads_destroy(&ads);

	return 0;
}

static int net_ads_keytab_flush(struct net_context *c, int argc, const char **argv)
{
	int ret;
	ADS_STRUCT *ads;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads keytab flush\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Delete the whole keytab"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		return -1;
	}
	ret = ads_keytab_flush(ads);
	ads_destroy(&ads);
	return ret;
}

static int net_ads_keytab_add(struct net_context *c, int argc, const char **argv)
{
	int i;
	int ret = 0;
	ADS_STRUCT *ads;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net ads keytab add <principal> [principal ...]\n"
			   "  Add principals to local keytab\n"
			   "    principal\tKerberos principal to add to "
			   "keytab\n"));
		return 0;
	}

	d_printf(_("Processing principals to add...\n"));
	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		return -1;
	}
	for (i = 0; i < argc; i++) {
		ret |= ads_keytab_add_entry(ads, argv[i]);
	}
	ads_destroy(&ads);
	return ret;
}

static int net_ads_keytab_create(struct net_context *c, int argc, const char **argv)
{
	ADS_STRUCT *ads;
	int ret;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads keytab create\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Create new default keytab"));
		return 0;
	}

	if (!ADS_ERR_OK(ads_startup(c, true, &ads))) {
		return -1;
	}
	ret = ads_keytab_create_default(ads);
	ads_destroy(&ads);
	return ret;
}

static int net_ads_keytab_list(struct net_context *c, int argc, const char **argv)
{
	const char *keytab = NULL;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net ads keytab list [keytab]\n"
			   "  List a local keytab\n"
			   "    keytab\tKeytab to list\n"));
		return 0;
	}

	if (argc >= 1) {
		keytab = argv[0];
	}

	return ads_keytab_list(keytab);
}


int net_ads_keytab(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			net_ads_keytab_add,
			NET_TRANSPORT_ADS,
			N_("Add a service principal"),
			N_("net ads keytab add\n"
			   "    Add a service principal")
		},
		{
			"create",
			net_ads_keytab_create,
			NET_TRANSPORT_ADS,
			N_("Create a fresh keytab"),
			N_("net ads keytab create\n"
			   "    Create a fresh keytab")
		},
		{
			"flush",
			net_ads_keytab_flush,
			NET_TRANSPORT_ADS,
			N_("Remove all keytab entries"),
			N_("net ads keytab flush\n"
			   "    Remove all keytab entries")
		},
		{
			"list",
			net_ads_keytab_list,
			NET_TRANSPORT_ADS,
			N_("List a keytab"),
			N_("net ads keytab list\n"
			   "    List a keytab")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (!USE_KERBEROS_KEYTAB) {
		d_printf(_("\nWarning: \"kerberos method\" must be set to a "
		    "keytab method to use keytab functions.\n"));
	}

	return net_run_function(c, argc, argv, "net ads keytab", func);
}

static int net_ads_kerberos_renew(struct net_context *c, int argc, const char **argv)
{
	int ret = -1;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads kerberos renew\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Renew TGT from existing credential cache"));
		return 0;
	}

	ret = smb_krb5_renew_ticket(NULL, NULL, NULL, NULL);
	if (ret) {
		d_printf(_("failed to renew kerberos ticket: %s\n"),
			error_message(ret));
	}
	return ret;
}

static int net_ads_kerberos_pac(struct net_context *c, int argc, const char **argv)
{
	struct PAC_LOGON_INFO *info = NULL;
	TALLOC_CTX *mem_ctx = NULL;
	NTSTATUS status;
	int ret = -1;
	const char *impersonate_princ_s = NULL;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads kerberos pac\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Dump the Kerberos PAC"));
		return 0;
	}

	mem_ctx = talloc_init("net_ads_kerberos_pac");
	if (!mem_ctx) {
		goto out;
	}

	if (argc > 0) {
		impersonate_princ_s = argv[0];
	}

	c->opt_password = net_prompt_pass(c, c->opt_user_name);

	status = kerberos_return_pac(mem_ctx,
				     c->opt_user_name,
				     c->opt_password,
				     0,
				     NULL,
				     NULL,
				     NULL,
				     true,
				     true,
				     2592000, /* one month */
				     impersonate_princ_s,
				     &info);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("failed to query kerberos PAC: %s\n"),
			nt_errstr(status));
		goto out;
	}

	if (info) {
		const char *s;
		s = NDR_PRINT_STRUCT_STRING(mem_ctx, PAC_LOGON_INFO, info);
		d_printf(_("The Pac: %s\n"), s);
	}

	ret = 0;
 out:
	TALLOC_FREE(mem_ctx);
	return ret;
}

static int net_ads_kerberos_kinit(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *mem_ctx = NULL;
	int ret = -1;
	NTSTATUS status;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net ads kerberos kinit\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Get Ticket Granting Ticket (TGT) for the user"));
		return 0;
	}

	mem_ctx = talloc_init("net_ads_kerberos_kinit");
	if (!mem_ctx) {
		goto out;
	}

	c->opt_password = net_prompt_pass(c, c->opt_user_name);

	ret = kerberos_kinit_password_ext(c->opt_user_name,
					  c->opt_password,
					  0,
					  NULL,
					  NULL,
					  NULL,
					  true,
					  true,
					  2592000, /* one month */
					  &status);
	if (ret) {
		d_printf(_("failed to kinit password: %s\n"),
			nt_errstr(status));
	}
 out:
	return ret;
}

int net_ads_kerberos(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"kinit",
			net_ads_kerberos_kinit,
			NET_TRANSPORT_ADS,
			N_("Retrieve Ticket Granting Ticket (TGT)"),
			N_("net ads kerberos kinit\n"
			   "    Receive Ticket Granting Ticket (TGT)")
		},
		{
			"renew",
			net_ads_kerberos_renew,
			NET_TRANSPORT_ADS,
			N_("Renew Ticket Granting Ticket from credential cache"),
			N_("net ads kerberos renew\n"
			   "    Renew Ticket Granting Ticket (TGT) from "
			   "credential cache")
		},
		{
			"pac",
			net_ads_kerberos_pac,
			NET_TRANSPORT_ADS,
			N_("Dump Kerberos PAC"),
			N_("net ads kerberos pac\n"
			   "    Dump Kerberos PAC")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net ads kerberos", func);
}

int net_ads(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"info",
			net_ads_info,
			NET_TRANSPORT_ADS,
			N_("Display details on remote ADS server"),
			N_("net ads info\n"
			   "    Display details on remote ADS server")
		},
		{
			"join",
			net_ads_join,
			NET_TRANSPORT_ADS,
			N_("Join the local machine to ADS realm"),
			N_("net ads join\n"
			   "    Join the local machine to ADS realm")
		},
		{
			"testjoin",
			net_ads_testjoin,
			NET_TRANSPORT_ADS,
			N_("Validate machine account"),
			N_("net ads testjoin\n"
			   "    Validate machine account")
		},
		{
			"leave",
			net_ads_leave,
			NET_TRANSPORT_ADS,
			N_("Remove the local machine from ADS"),
			N_("net ads leave\n"
			   "    Remove the local machine from ADS")
		},
		{
			"status",
			net_ads_status,
			NET_TRANSPORT_ADS,
			N_("Display machine account details"),
			N_("net ads status\n"
			   "    Display machine account details")
		},
		{
			"user",
			net_ads_user,
			NET_TRANSPORT_ADS,
			N_("List/modify users"),
			N_("net ads user\n"
			   "    List/modify users")
		},
		{
			"group",
			net_ads_group,
			NET_TRANSPORT_ADS,
			N_("List/modify groups"),
			N_("net ads group\n"
			   "    List/modify groups")
		},
		{
			"dns",
			net_ads_dns,
			NET_TRANSPORT_ADS,
			N_("Issue dynamic DNS update"),
			N_("net ads dns\n"
			   "    Issue dynamic DNS update")
		},
		{
			"password",
			net_ads_password,
			NET_TRANSPORT_ADS,
			N_("Change user passwords"),
			N_("net ads password\n"
			   "    Change user passwords")
		},
		{
			"changetrustpw",
			net_ads_changetrustpw,
			NET_TRANSPORT_ADS,
			N_("Change trust account password"),
			N_("net ads changetrustpw\n"
			   "    Change trust account password")
		},
		{
			"printer",
			net_ads_printer,
			NET_TRANSPORT_ADS,
			N_("List/modify printer entries"),
			N_("net ads printer\n"
			   "    List/modify printer entries")
		},
		{
			"search",
			net_ads_search,
			NET_TRANSPORT_ADS,
			N_("Issue LDAP search using filter"),
			N_("net ads search\n"
			   "    Issue LDAP search using filter")
		},
		{
			"dn",
			net_ads_dn,
			NET_TRANSPORT_ADS,
			N_("Issue LDAP search by DN"),
			N_("net ads dn\n"
			   "    Issue LDAP search by DN")
		},
		{
			"sid",
			net_ads_sid,
			NET_TRANSPORT_ADS,
			N_("Issue LDAP search by SID"),
			N_("net ads sid\n"
			   "    Issue LDAP search by SID")
		},
		{
			"workgroup",
			net_ads_workgroup,
			NET_TRANSPORT_ADS,
			N_("Display workgroup name"),
			N_("net ads workgroup\n"
			   "    Display the workgroup name")
		},
		{
			"lookup",
			net_ads_lookup,
			NET_TRANSPORT_ADS,
			N_("Perfom CLDAP query on DC"),
			N_("net ads lookup\n"
			   "    Find the ADS DC using CLDAP lookups")
		},
		{
			"keytab",
			net_ads_keytab,
			NET_TRANSPORT_ADS,
			N_("Manage local keytab file"),
			N_("net ads keytab\n"
			   "    Manage local keytab file")
		},
		{
			"gpo",
			net_ads_gpo,
			NET_TRANSPORT_ADS,
			N_("Manage group policy objects"),
			N_("net ads gpo\n"
			   "    Manage group policy objects")
		},
		{
			"kerberos",
			net_ads_kerberos,
			NET_TRANSPORT_ADS,
			N_("Manage kerberos keytab"),
			N_("net ads kerberos\n"
			   "    Manage kerberos keytab")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net ads", func);
}

#else

static int net_ads_noads(void)
{
	d_fprintf(stderr, _("ADS support not compiled in\n"));
	return -1;
}

int net_ads_keytab(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_kerberos(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_changetrustpw(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_join(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_user(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_group(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_gpo(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

/* this one shouldn't display a message */
int net_ads_check(struct net_context *c)
{
	return -1;
}

int net_ads_check_our_domain(struct net_context *c)
{
	return -1;
}

int net_ads(struct net_context *c, int argc, const char **argv)
{
	return net_ads_noads();
}

#endif	/* WITH_ADS */
