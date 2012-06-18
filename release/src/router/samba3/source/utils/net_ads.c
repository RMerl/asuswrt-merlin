/* 
   Samba Unix/Linux SMB client library 
   net ads commands
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Remus Koos (remuskoos@yahoo.com)
   Copyright (C) 2002 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2006 Gerald (Jerry) Carter (jerry@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include "includes.h"
#include "utils/net.h"

#ifdef HAVE_ADS

int net_ads_usage(int argc, const char **argv)
{
	d_printf("join [createupn[=principal]] [createcomputer=<org_unit>]\n");
	d_printf("    Join the local machine to a ADS realm\n");
	d_printf("leave\n");
	d_printf("    Remove the local machine from a ADS realm\n");
	d_printf("testjoin\n");
	d_printf("    Validates the machine account in the domain\n");
	d_printf("user\n");
	d_printf("    List, add, or delete users in the realm\n");
	d_printf("group\n");
	d_printf("    List, add, or delete groups in the realm\n");
	d_printf("info\n");
	d_printf("    Displays details regarding a specific AD server\n");
	d_printf("status\n");
	d_printf("    Display details regarding the machine's account in AD\n");
	d_printf("lookup\n");
	d_printf("    Performs CLDAP query of AD domain controllers\n");
	d_printf("password <username@realm> <password> -Uadmin_username@realm%%admin_pass\n");
	d_printf("    Change a user's password using an admin account\n");
	d_printf("    (note: use realm in UPPERCASE, prompts if password is obmitted)\n");
	d_printf("changetrustpw\n");
	d_printf("    Change the trust account password of this machine in the AD tree\n");
	d_printf("printer [info | publish | remove] <printername> <servername>\n");
	d_printf("    Lookup, add, or remove directory entry for a printer\n");
	d_printf("{search,dn,sid}\n");
	d_printf("    Issue LDAP search queries using a general filter, by DN, or by SID\n");
	d_printf("keytab\n");
	d_printf("    Manage a local keytab file based on the machine account in AD\n");
	d_printf("dns\n");
	d_printf("    Issue a dynamic DNS update request the server's hostname\n");
	d_printf("    (using the machine credentials)\n");
	
	return -1;
}

/* when we do not have sufficient input parameters to contact a remote domain
 * we always fall back to our own realm - Guenther*/

static const char *assume_own_realm(void)
{
	if (!opt_host && strequal(lp_workgroup(), opt_target_workgroup)) {
		return lp_realm();
	}

	return NULL;
}

/*
  do a cldap netlogon query
*/
static int net_ads_cldap_netlogon(ADS_STRUCT *ads)
{
	struct cldap_netlogon_reply reply;

	if ( !ads_cldap_netlogon( inet_ntoa(ads->ldap_ip), ads->server.realm, &reply ) ) {
		d_fprintf(stderr, "CLDAP query failed!\n");
		return -1;
	}

	d_printf("Information for Domain Controller: %s\n\n", 
		inet_ntoa(ads->ldap_ip));

	d_printf("Response Type: ");
	switch (reply.type) {
	case SAMLOGON_AD_UNK_R:
		d_printf("SAMLOGON\n");
		break;
	case SAMLOGON_AD_R:
		d_printf("SAMLOGON_USER\n");
		break;
	default:
		d_printf("0x%x\n", reply.type);
		break;
	}
	d_printf("GUID: %s\n", 
		 smb_uuid_string_static(smb_uuid_unpack_static(reply.guid))); 
	d_printf("Flags:\n"
		 "\tIs a PDC:                                   %s\n"
		 "\tIs a GC of the forest:                      %s\n"
		 "\tIs an LDAP server:                          %s\n"
		 "\tSupports DS:                                %s\n"
		 "\tIs running a KDC:                           %s\n"
		 "\tIs running time services:                   %s\n"
		 "\tIs the closest DC:                          %s\n"
		 "\tIs writable:                                %s\n"
		 "\tHas a hardware clock:                       %s\n"
		 "\tIs a non-domain NC serviced by LDAP server: %s\n",
		 (reply.flags & ADS_PDC) ? "yes" : "no",
		 (reply.flags & ADS_GC) ? "yes" : "no",
		 (reply.flags & ADS_LDAP) ? "yes" : "no",
		 (reply.flags & ADS_DS) ? "yes" : "no",
		 (reply.flags & ADS_KDC) ? "yes" : "no",
		 (reply.flags & ADS_TIMESERV) ? "yes" : "no",
		 (reply.flags & ADS_CLOSEST) ? "yes" : "no",
		 (reply.flags & ADS_WRITABLE) ? "yes" : "no",
		 (reply.flags & ADS_GOOD_TIMESERV) ? "yes" : "no",
		 (reply.flags & ADS_NDNC) ? "yes" : "no");

	printf("Forest:\t\t\t%s\n", reply.forest);
	printf("Domain:\t\t\t%s\n", reply.domain);
	printf("Domain Controller:\t%s\n", reply.hostname);

	printf("Pre-Win2k Domain:\t%s\n", reply.netbios_domain);
	printf("Pre-Win2k Hostname:\t%s\n", reply.netbios_hostname);

	if (*reply.unk) printf("Unk:\t\t\t%s\n", reply.unk);
	if (*reply.user_name) printf("User name:\t%s\n", reply.user_name);

	printf("Server Site Name :\t\t%s\n", reply.server_site_name);
	printf("Client Site Name :\t\t%s\n", reply.client_site_name);

	d_printf("NT Version: %d\n", reply.version);
	d_printf("LMNT Token: %.2x\n", reply.lmnt_token);
	d_printf("LM20 Token: %.2x\n", reply.lm20_token);

	return 0;
}


/*
  this implements the CLDAP based netlogon lookup requests
  for finding the domain controller of a ADS domain
*/
static int net_ads_lookup(int argc, const char **argv)
{
	ADS_STRUCT *ads;

	if (!ADS_ERR_OK(ads_startup_nobind(False, &ads))) {
		d_fprintf(stderr, "Didn't find the cldap server!\n");
		return -1;
	}

	if (!ads->config.realm) {
		ads->config.realm = CONST_DISCARD(char *, opt_target_workgroup);
		ads->ldap_port = 389;
	}

	return net_ads_cldap_netlogon(ads);
}



static int net_ads_info(int argc, const char **argv)
{
	ADS_STRUCT *ads;

	if (!ADS_ERR_OK(ads_startup_nobind(False, &ads))) {
		d_fprintf(stderr, "Didn't find the ldap server!\n");
		return -1;
	}

	if (!ads || !ads->config.realm) {
		d_fprintf(stderr, "Didn't find the ldap server!\n");
		return -1;
	}

	/* Try to set the server's current time since we didn't do a full
	   TCP LDAP session initially */

	if ( !ADS_ERR_OK(ads_current_time( ads )) ) {
		d_fprintf( stderr, "Failed to get server's current time!\n");
	}

	d_printf("LDAP server: %s\n", inet_ntoa(ads->ldap_ip));
	d_printf("LDAP server name: %s\n", ads->config.ldap_server_name);
	d_printf("Realm: %s\n", ads->config.realm);
	d_printf("Bind Path: %s\n", ads->config.bind_path);
	d_printf("LDAP port: %d\n", ads->ldap_port);
	d_printf("Server time: %s\n", http_timestring(ads->config.current_time));

	d_printf("KDC server: %s\n", ads->auth.kdc_server );
	d_printf("Server time offset: %d\n", ads->auth.time_offset );

	return 0;
}

static void use_in_memory_ccache(void) {
	/* Use in-memory credentials cache so we do not interfere with
	 * existing credentials */
	setenv(KRB5_ENV_CCNAME, "MEMORY:net_ads", 1);
}

static ADS_STATUS ads_startup_int(BOOL only_own_domain, uint32 auth_flags, ADS_STRUCT **ads_ret)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS status;
	BOOL need_password = False;
	BOOL second_time = False;
	char *cp;
	const char *realm = NULL;
	BOOL tried_closest_dc = False;

	/* lp_realm() should be handled by a command line param, 
	   However, the join requires that realm be set in smb.conf
	   and compares our realm with the remote server's so this is
	   ok until someone needs more flexibility */

	*ads_ret = NULL;

retry_connect:
 	if (only_own_domain) {
		realm = lp_realm();
	} else {
		realm = assume_own_realm();
	}

	ads = ads_init(realm, opt_target_workgroup, opt_host);

	if (!opt_user_name) {
		opt_user_name = "administrator";
	}

	if (opt_user_specified) {
		need_password = True;
	}

retry:
	if (!opt_password && need_password && !opt_machine_pass) {
		char *prompt = NULL;
		asprintf(&prompt,"%s's password: ", opt_user_name);
		if (!prompt) {
			ads_destroy(&ads);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
		opt_password = getpass(prompt);
		free(prompt);
	}

	if (opt_password) {
		use_in_memory_ccache();
		SAFE_FREE(ads->auth.password);
		ads->auth.password = smb_xstrdup(opt_password);
	}

	ads->auth.flags |= auth_flags;
	SAFE_FREE(ads->auth.user_name);
	ads->auth.user_name = smb_xstrdup(opt_user_name);

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
			need_password = True;
			second_time = True;
			goto retry;
		} else {
			ads_destroy(&ads);
			return status;
		}
	}

	/* when contacting our own domain, make sure we use the closest DC.
	 * This is done by reconnecting to ADS because only the first call to
	 * ads_connect will give us our own sitename */

	if ((only_own_domain || !opt_host) && !tried_closest_dc) {

		tried_closest_dc = True; /* avoid loop */

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

ADS_STATUS ads_startup(BOOL only_own_domain, ADS_STRUCT **ads)
{
	return ads_startup_int(only_own_domain, 0, ads);
}

ADS_STATUS ads_startup_nobind(BOOL only_own_domain, ADS_STRUCT **ads)
{
	return ads_startup_int(only_own_domain, ADS_AUTH_NO_BIND, ads);
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

int net_ads_check_our_domain(void)
{
	return net_ads_check_int(lp_realm(), lp_workgroup(), NULL);
}

int net_ads_check(void)
{
	return net_ads_check_int(NULL, opt_workgroup, opt_host);
}
/* 
   determine the netbios workgroup name for a domain
 */
static int net_ads_workgroup(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	struct cldap_netlogon_reply reply;

	if (!ADS_ERR_OK(ads_startup_nobind(False, &ads))) {
		d_fprintf(stderr, "Didn't find the cldap server!\n");
		return -1;
	}
	
	if (!ads->config.realm) {
		ads->config.realm = CONST_DISCARD(char *, opt_target_workgroup);
		ads->ldap_port = 389;
	}
	
	if ( !ads_cldap_netlogon( inet_ntoa(ads->ldap_ip), ads->server.realm, &reply ) ) {
		d_fprintf(stderr, "CLDAP query failed!\n");
		return -1;
	}

	d_printf("Workgroup: %s\n", reply.netbios_domain);

	ads_destroy(&ads);
	
	return 0;
}



static BOOL usergrp_display(char *field, void **values, void *data_area)
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
		return True;
	}
	if (!values) /* must be new field, indicate string field */
		return True;
	if (StrCaseCmp(field, "sAMAccountName") == 0) {
		disp_fields[0] = SMB_STRDUP((char *) values[0]);
	}
	if (StrCaseCmp(field, "description") == 0)
		disp_fields[1] = SMB_STRDUP((char *) values[0]);
	return True;
}

static int net_ads_user_usage(int argc, const char **argv)
{
	return net_help_user(argc, argv);
} 

static int ads_user_add(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	char *upn, *userdn;
	LDAPMessage *res=NULL;
	int rc = -1;
	char *ou_str = NULL;

	if (argc < 1) return net_ads_user_usage(argc, argv);
	
	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	status = ads_find_user_acct(ads, &res, argv[0]);

	if (!ADS_ERR_OK(status)) {
		d_fprintf(stderr, "ads_user_add: %s\n", ads_errstr(status));
		goto done;
	}
	
	if (ads_count_replies(ads, res)) {
		d_fprintf(stderr, "ads_user_add: User %s already exists\n", argv[0]);
		goto done;
	}

	if (opt_container) {
		ou_str = SMB_STRDUP(opt_container);
	} else {
		ou_str = ads_default_ou_string(ads, WELL_KNOWN_GUID_USERS);
	}

	status = ads_add_user_acct(ads, argv[0], ou_str, opt_comment);

	if (!ADS_ERR_OK(status)) {
		d_fprintf(stderr, "Could not add user %s: %s\n", argv[0],
			 ads_errstr(status));
		goto done;
	}

	/* if no password is to be set, we're done */
	if (argc == 1) { 
		d_printf("User %s added\n", argv[0]);
		rc = 0;
		goto done;
	}

	/* try setting the password */
	asprintf(&upn, "%s@%s", argv[0], ads->config.realm);
	status = ads_krb5_set_password(ads->auth.kdc_server, upn, argv[1], 
				       ads->auth.time_offset);
	safe_free(upn);
	if (ADS_ERR_OK(status)) {
		d_printf("User %s added\n", argv[0]);
		rc = 0;
		goto done;
	}

	/* password didn't set, delete account */
	d_fprintf(stderr, "Could not add user %s.  Error setting password %s\n",
		 argv[0], ads_errstr(status));
	ads_msgfree(ads, res);
	status=ads_find_user_acct(ads, &res, argv[0]);
	if (ADS_ERR_OK(status)) {
		userdn = ads_get_dn(ads, res);
		ads_del_dn(ads, userdn);
		ads_memfree(ads, userdn);
	}

 done:
	if (res)
		ads_msgfree(ads, res);
	ads_destroy(&ads);
	SAFE_FREE(ou_str);
	return rc;
}

static int ads_user_info(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res;
	const char *attrs[] = {"memberOf", NULL};
	char *searchstring=NULL;
	char **grouplist;
	char *escaped_user;

	if (argc < 1) {
		return net_ads_user_usage(argc, argv);
	}

	escaped_user = escape_ldap_string_alloc(argv[0]);

	if (!escaped_user) {
		d_fprintf(stderr, "ads_user_info: failed to escape user %s\n", argv[0]);
		return -1;
	}

	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		SAFE_FREE(escaped_user);
		return -1;
	}

	asprintf(&searchstring, "(sAMAccountName=%s)", escaped_user);
	rc = ads_search(ads, &res, searchstring, attrs);
	safe_free(searchstring);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "ads_search: %s\n", ads_errstr(rc));
		ads_destroy(&ads);
		SAFE_FREE(escaped_user);
		return -1;
	}
	
	grouplist = ldap_get_values((LDAP *)ads->ld,
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
	
	ads_msgfree(ads, res);
	ads_destroy(&ads);
	SAFE_FREE(escaped_user);
	return 0;
}

static int ads_user_delete(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	char *userdn;

	if (argc < 1) {
		return net_ads_user_usage(argc, argv);
	}
	
	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	rc = ads_find_user_acct(ads, &res, argv[0]);
	if (!ADS_ERR_OK(rc) || ads_count_replies(ads, res) != 1) {
		d_printf("User %s does not exist.\n", argv[0]);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}
	userdn = ads_get_dn(ads, res);
	ads_msgfree(ads, res);
	rc = ads_del_dn(ads, userdn);
	ads_memfree(ads, userdn);
	if (ADS_ERR_OK(rc)) {
		d_printf("User %s deleted\n", argv[0]);
		ads_destroy(&ads);
		return 0;
	}
	d_fprintf(stderr, "Error deleting user %s: %s\n", argv[0], 
		 ads_errstr(rc));
	ads_destroy(&ads);
	return -1;
}

int net_ads_user(int argc, const char **argv)
{
	struct functable func[] = {
		{"ADD", ads_user_add},
		{"INFO", ads_user_info},
		{"DELETE", ads_user_delete},
		{NULL, NULL}
	};
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *shortattrs[] = {"sAMAccountName", NULL};
	const char *longattrs[] = {"sAMAccountName", "description", NULL};
	char *disp_fields[2] = {NULL, NULL};
	
	if (argc == 0) {
		if (!ADS_ERR_OK(ads_startup(False, &ads))) {
			return -1;
		}

		if (opt_long_list_entries)
			d_printf("\nUser name             Comment"\
				 "\n-----------------------------\n");

		rc = ads_do_search_all_fn(ads, ads->config.bind_path, 
					  LDAP_SCOPE_SUBTREE,
					  "(objectCategory=user)", 
					  opt_long_list_entries ? longattrs :
					  shortattrs, usergrp_display, 
					  disp_fields);
		ads_destroy(&ads);
		return ADS_ERR_OK(rc) ? 0 : -1;
	}

	return net_run_function(argc, argv, func, net_ads_user_usage);
}

static int net_ads_group_usage(int argc, const char **argv)
{
	return net_help_group(argc, argv);
} 

static int ads_group_add(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS status;
	LDAPMessage *res=NULL;
	int rc = -1;
	char *ou_str = NULL;

	if (argc < 1) {
		return net_ads_group_usage(argc, argv);
	}
	
	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	status = ads_find_user_acct(ads, &res, argv[0]);

	if (!ADS_ERR_OK(status)) {
		d_fprintf(stderr, "ads_group_add: %s\n", ads_errstr(status));
		goto done;
	}
	
	if (ads_count_replies(ads, res)) {
		d_fprintf(stderr, "ads_group_add: Group %s already exists\n", argv[0]);
		goto done;
	}

	if (opt_container) {
		ou_str = SMB_STRDUP(opt_container);
	} else {
		ou_str = ads_default_ou_string(ads, WELL_KNOWN_GUID_USERS);
	}

	status = ads_add_group_acct(ads, argv[0], ou_str, opt_comment);

	if (ADS_ERR_OK(status)) {
		d_printf("Group %s added\n", argv[0]);
		rc = 0;
	} else {
		d_fprintf(stderr, "Could not add group %s: %s\n", argv[0],
			 ads_errstr(status));
	}

 done:
	if (res)
		ads_msgfree(ads, res);
	ads_destroy(&ads);
	SAFE_FREE(ou_str);
	return rc;
}

static int ads_group_delete(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	char *groupdn;

	if (argc < 1) {
		return net_ads_group_usage(argc, argv);
	}
	
	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	rc = ads_find_user_acct(ads, &res, argv[0]);
	if (!ADS_ERR_OK(rc) || ads_count_replies(ads, res) != 1) {
		d_printf("Group %s does not exist.\n", argv[0]);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}
	groupdn = ads_get_dn(ads, res);
	ads_msgfree(ads, res);
	rc = ads_del_dn(ads, groupdn);
	ads_memfree(ads, groupdn);
	if (ADS_ERR_OK(rc)) {
		d_printf("Group %s deleted\n", argv[0]);
		ads_destroy(&ads);
		return 0;
	}
	d_fprintf(stderr, "Error deleting group %s: %s\n", argv[0], 
		 ads_errstr(rc));
	ads_destroy(&ads);
	return -1;
}

int net_ads_group(int argc, const char **argv)
{
	struct functable func[] = {
		{"ADD", ads_group_add},
		{"DELETE", ads_group_delete},
		{NULL, NULL}
	};
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *shortattrs[] = {"sAMAccountName", NULL};
	const char *longattrs[] = {"sAMAccountName", "description", NULL};
	char *disp_fields[2] = {NULL, NULL};

	if (argc == 0) {
		if (!ADS_ERR_OK(ads_startup(False, &ads))) {
			return -1;
		}

		if (opt_long_list_entries)
			d_printf("\nGroup name            Comment"\
				 "\n-----------------------------\n");
		rc = ads_do_search_all_fn(ads, ads->config.bind_path, 
					  LDAP_SCOPE_SUBTREE, 
					  "(objectCategory=group)", 
					  opt_long_list_entries ? longattrs : 
					  shortattrs, usergrp_display, 
					  disp_fields);

		ads_destroy(&ads);
		return ADS_ERR_OK(rc) ? 0 : -1;
	}
	return net_run_function(argc, argv, func, net_ads_group_usage);
}

static int net_ads_status(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res;

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}

	rc = ads_find_machine_acct(ads, &res, global_myname());
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "ads_find_machine_acct: %s\n", ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, "No machine account for '%s' found\n", global_myname());
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

static int net_ads_leave(int argc, const char **argv)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS adsret;
	NTSTATUS status;
	int ret = -1;
	struct cli_state *cli = NULL;
	TALLOC_CTX *ctx;
	DOM_SID *dom_sid = NULL;
	char *short_domain_name = NULL;      

	if (!secrets_init()) {
		DEBUG(1,("Failed to initialise secrets database\n"));
		return -1;
	}

	if (!(ctx = talloc_init("net_ads_leave"))) {
		d_fprintf(stderr, "Could not initialise talloc context.\n");
		return -1;
	}

	/* The finds a DC and takes care of getting the 
	   user creds if necessary */

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}

	/* make RPC calls here */

	if ( !NT_STATUS_IS_OK(connect_to_ipc_krb5(&cli, &ads->ldap_ip, 
		ads->config.ldap_server_name)) )
	{
		goto done;
	}
	
	if ( !NT_STATUS_IS_OK(netdom_get_domain_sid( ctx, cli, &short_domain_name, &dom_sid )) ) {
		goto done;
	}

	saf_delete( short_domain_name );

	status = netdom_leave_domain(ctx, cli, dom_sid);

	/* Try and delete it via LDAP - the old way we used to. */

	adsret = ads_leave_realm(ads, global_myname());
	if (ADS_ERR_OK(adsret)) {
		d_printf("Deleted account for '%s' in realm '%s'\n",
			global_myname(), ads->config.realm);
		ret = 0;
	} else {
		/* We couldn't delete it - see if the disable succeeded. */
		if (NT_STATUS_IS_OK(status)) {
			d_printf("Disabled account for '%s' in realm '%s'\n",
				global_myname(), ads->config.realm);
			ret = 0;
		} else {
			d_fprintf(stderr, "Failed to disable machine account for '%s' in realm '%s'\n",
				global_myname(), ads->config.realm);
		}
	}

done:

	if ( cli ) 
		cli_shutdown(cli);

	ads_destroy(&ads);
	TALLOC_FREE( ctx );

	return ret;
}

static NTSTATUS net_ads_join_ok(void)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS status;

	if (!secrets_init()) {
		DEBUG(1,("Failed to initialise secrets database\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	net_use_krb_machine_account();

	status = ads_startup(True, &ads);
	if (!ADS_ERR_OK(status)) {
		return ads_ntstatus(status);
	}

	ads_destroy(&ads);
	return NT_STATUS_OK;
}

/*
  check that an existing join is OK
 */
int net_ads_testjoin(int argc, const char **argv)
{
	NTSTATUS status;
	use_in_memory_ccache();

	/* Display success or failure */
	status = net_ads_join_ok();
	if (!NT_STATUS_IS_OK(status)) {
		fprintf(stderr,"Join to domain is not valid: %s\n", 
			get_friendly_nt_error_msg(status));
		return -1;
	}

	printf("Join is OK\n");
	return 0;
}

/*******************************************************************
  Simple configu checks before beginning the join
 ********************************************************************/

static NTSTATUS check_ads_config( void )
{
	if (lp_server_role() != ROLE_DOMAIN_MEMBER ) {
		d_printf("Host is not configured as a member server.\n");
		return NT_STATUS_INVALID_DOMAIN_ROLE;
	}

	if (strlen(global_myname()) > 15) {
		d_printf("Our netbios name can be at most 15 chars long, "
			 "\"%s\" is %u chars long\n", global_myname(),
			 (unsigned int)strlen(global_myname()));
		return NT_STATUS_NAME_TOO_LONG;
	}

	if ( lp_security() == SEC_ADS && !*lp_realm()) {
		d_fprintf(stderr, "realm must be set in in %s for ADS "
			"join to succeed.\n", dyn_CONFIGFILE);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!secrets_init()) {
		DEBUG(1,("Failed to initialise secrets database\n"));
		/* This is a good bet for failure of secrets_init ... */
		return NT_STATUS_ACCESS_DENIED;
	}
	
	return NT_STATUS_OK;
}

/*******************************************************************
 Do the domain join
 ********************************************************************/

static NTSTATUS net_join_domain(TALLOC_CTX *ctx, const char *servername, 
				struct in_addr *ip, char **domain, 
				DOM_SID **dom_sid, 
				const char *password)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct cli_state *cli = NULL;

	ret = connect_to_ipc_krb5(&cli, ip, servername);
	if ( !NT_STATUS_IS_OK(ret) ) {
		goto done;
	}
	
	ret = netdom_get_domain_sid( ctx, cli, domain, dom_sid );
	if ( !NT_STATUS_IS_OK(ret) ) {
		goto done;
	}

	/* cli->server_domain is not filled in when using krb5 
	   session setups */

	saf_store( *domain, cli->desthost );

	ret = netdom_join_domain( ctx, cli, *dom_sid, password, ND_TYPE_AD );

done:
	if ( cli ) 
		cli_shutdown(cli);

	return ret;
}

/*******************************************************************
 Set a machines dNSHostName and servicePrincipalName attributes
 ********************************************************************/

static ADS_STATUS net_set_machine_spn(TALLOC_CTX *ctx, ADS_STRUCT *ads_s )
{
	ADS_STATUS status = ADS_ERROR(LDAP_SERVER_DOWN);
	char *new_dn;
	ADS_MODLIST mods;
	const char *servicePrincipalName[3] = {NULL, NULL, NULL};
	char *psp;
	fstring my_fqdn;
	LDAPMessage *res = NULL;
	char *dn_string = NULL;
	const char *machine_name = global_myname();
	int count;
	
	if ( !machine_name ) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	
	/* Find our DN */
	
	status = ads_find_machine_acct(ads_s, &res, machine_name);
	if (!ADS_ERR_OK(status)) 
		return status;
		
	if ( (count = ads_count_replies(ads_s, res)) != 1 ) {
		DEBUG(1,("net_set_machine_spn: %d entries returned!\n", count));
		return ADS_ERROR(LDAP_NO_MEMORY);	
	}
	
	if ( (dn_string = ads_get_dn(ads_s, res)) == NULL ) {
		DEBUG(1, ("ads_add_machine_acct: ads_get_dn returned NULL (malloc failure?)\n"));
		goto done;
	}
	
	new_dn = talloc_strdup(ctx, dn_string);
	ads_memfree(ads_s, dn_string);
	if (!new_dn) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	/* Windows only creates HOST/shortname & HOST/fqdn. */
	   
	if ( !(psp = talloc_asprintf(ctx, "HOST/%s", machine_name)) ) 
		goto done;
	strupper_m(psp);
	servicePrincipalName[0] = psp;

	name_to_fqdn(my_fqdn, machine_name);
	strlower_m(my_fqdn);
	if ( !(psp = talloc_asprintf(ctx, "HOST/%s", my_fqdn)) ) 
		goto done;
	servicePrincipalName[1] = psp;
	
	if (!(mods = ads_init_mods(ctx))) {
		goto done;
	}
	
	/* fields of primary importance */
	
	ads_mod_str(ctx, &mods, "dNSHostName", my_fqdn);
	ads_mod_strlist(ctx, &mods, "servicePrincipalName", servicePrincipalName);

	status = ads_gen_mod(ads_s, new_dn, mods);

done:
	ads_msgfree(ads_s, res);
	
	return status;
}

/*******************************************************************
 Set a machines dNSHostName and servicePrincipalName attributes
 ********************************************************************/

static ADS_STATUS net_set_machine_upn(TALLOC_CTX *ctx, ADS_STRUCT *ads_s, const char *upn )
{
	ADS_STATUS status = ADS_ERROR(LDAP_SERVER_DOWN);
	char *new_dn;
	ADS_MODLIST mods;
	LDAPMessage *res = NULL;
	char *dn_string = NULL;
	const char *machine_name = global_myname();
	int count;
	
	if ( !machine_name ) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	
	/* Find our DN */
	
	status = ads_find_machine_acct(ads_s, &res, machine_name);
	if (!ADS_ERR_OK(status)) 
		return status;
		
	if ( (count = ads_count_replies(ads_s, res)) != 1 ) {
		DEBUG(1,("net_set_machine_spn: %d entries returned!\n", count));
		return ADS_ERROR(LDAP_NO_MEMORY);	
	}
	
	if ( (dn_string = ads_get_dn(ads_s, res)) == NULL ) {
		DEBUG(1, ("ads_add_machine_acct: ads_get_dn returned NULL (malloc failure?)\n"));
		goto done;
	}
	
	new_dn = talloc_strdup(ctx, dn_string);
	ads_memfree(ads_s, dn_string);
	if (!new_dn) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	
	/* now do the mods */
	
	if (!(mods = ads_init_mods(ctx))) {
		goto done;
	}
	
	/* fields of primary importance */
	
	ads_mod_str(ctx, &mods, "userPrincipalName", upn);

	status = ads_gen_mod(ads_s, new_dn, mods);

done:
	ads_msgfree(ads_s, res);
	
	return status;
}

/*******************************************************************
 Set a machines dNSHostName and servicePrincipalName attributes
 ********************************************************************/

static ADS_STATUS net_set_os_attributes(TALLOC_CTX *ctx, ADS_STRUCT *ads_s, 
					const char *os_name, const char *os_version )
{
	ADS_STATUS status = ADS_ERROR(LDAP_SERVER_DOWN);
	char *new_dn;
	ADS_MODLIST mods;
	LDAPMessage *res = NULL;
	char *dn_string = NULL;
	const char *machine_name = global_myname();
	int count;
	char *os_sp = NULL;
	
	if ( !os_name || !os_version ) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	
	/* Find our DN */
	
	status = ads_find_machine_acct(ads_s, &res, machine_name);
	if (!ADS_ERR_OK(status)) 
		return status;
		
	if ( (count = ads_count_replies(ads_s, res)) != 1 ) {
		DEBUG(1,("net_set_machine_spn: %d entries returned!\n", count));
		return ADS_ERROR(LDAP_NO_MEMORY);	
	}
	
	if ( (dn_string = ads_get_dn(ads_s, res)) == NULL ) {
		DEBUG(1, ("ads_add_machine_acct: ads_get_dn returned NULL (malloc failure?)\n"));
		goto done;
	}
	
	new_dn = talloc_strdup(ctx, dn_string);
	ads_memfree(ads_s, dn_string);
	if (!new_dn) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	
	/* now do the mods */
	
	if (!(mods = ads_init_mods(ctx))) {
		goto done;
	}

	os_sp = talloc_asprintf( ctx, "Samba %s", SAMBA_VERSION_STRING );
	
	/* fields of primary importance */
	
	ads_mod_str(ctx, &mods, "operatingSystem", os_name);
	ads_mod_str(ctx, &mods, "operatingSystemVersion", os_version);
	if ( os_sp )
		ads_mod_str(ctx, &mods, "operatingSystemServicePack", os_sp);

	status = ads_gen_mod(ads_s, new_dn, mods);

done:
	ads_msgfree(ads_s, res);
	TALLOC_FREE( os_sp );	
	
	return status;
}

/*******************************************************************
  join a domain using ADS (LDAP mods)
 ********************************************************************/

static ADS_STATUS net_precreate_machine_acct( ADS_STRUCT *ads, const char *ou )
{
	ADS_STATUS rc = ADS_ERROR(LDAP_SERVER_DOWN);
	char *dn, *ou_str;
	LDAPMessage *res = NULL;

	ou_str = ads_ou_string(ads, ou);
	if ((asprintf(&dn, "%s,%s", ou_str, ads->config.bind_path)) == -1) {
		SAFE_FREE(ou_str);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	rc = ads_search_dn(ads, &res, dn, NULL);
	ads_msgfree(ads, res);

	if (ADS_ERR_OK(rc)) {
		/* Attempt to create the machine account and bail if this fails.
		   Assume that the admin wants exactly what they requested */

		rc = ads_create_machine_acct( ads, global_myname(), dn );
		if ( rc.error_type == ENUM_ADS_ERROR_LDAP && rc.err.rc == LDAP_ALREADY_EXISTS ) {
			rc = ADS_SUCCESS;
		}
	}

	SAFE_FREE( ou_str );
	SAFE_FREE( dn );

	return rc;
}

/************************************************************************
 ************************************************************************/

static BOOL net_derive_salting_principal( TALLOC_CTX *ctx, ADS_STRUCT *ads )
{
	uint32 domain_func;
	ADS_STATUS status;
	fstring salt;
	char *std_salt;
	LDAPMessage *res = NULL;
	const char *machine_name = global_myname();

	status = ads_domain_func_level( ads, &domain_func );
	if ( !ADS_ERR_OK(status) ) {
		DEBUG(2,("Failed to determine domain functional level!\n"));
		return False;
	}

	/* go ahead and setup the default salt */

	if ( (std_salt = kerberos_standard_des_salt()) == NULL ) {
		d_fprintf(stderr, "net_derive_salting_principal: failed to obtain stanard DES salt\n");
		return False;
	}

	fstrcpy( salt, std_salt );
	SAFE_FREE( std_salt );
	
	/* if it's a Windows functional domain, we have to look for the UPN */
	   
	if ( domain_func == DS_DOMAIN_FUNCTION_2000 ) {	
		char *upn;
		int count;
		
		status = ads_find_machine_acct(ads, &res, machine_name);
		if (!ADS_ERR_OK(status)) {
			return False;
		}
		
		if ( (count = ads_count_replies(ads, res)) != 1 ) {
			DEBUG(1,("net_set_machine_spn: %d entries returned!\n", count));
			return False;
		}
		
		upn = ads_pull_string(ads, ctx, res, "userPrincipalName");
		if ( upn ) {
			fstrcpy( salt, upn );
		}
		
		ads_msgfree(ads, res);
	}

	return kerberos_secrets_store_des_salt( salt );
}

/*******************************************************************
 Send a DNS update request
*******************************************************************/

#if defined(WITH_DNS_UPDATES)
#include "dns.h"
DNS_ERROR DoDNSUpdate(char *pszServerName,
		      const char *pszDomainName,
		      const char *pszHostName,
		      const struct in_addr *iplist, int num_addrs );


static NTSTATUS net_update_dns_internal(TALLOC_CTX *ctx, ADS_STRUCT *ads,
					const char *machine_name,
					const struct in_addr *addrs,
					int num_addrs)
{
	struct dns_rr_ns *nameservers = NULL;
	int ns_count = 0;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	DNS_ERROR dns_err;
	fstring dns_server;
	const char *dnsdomain = NULL;	
	char *root_domain = NULL;	

	if ( (dnsdomain = strchr_m( machine_name, '.')) == NULL ) {
		d_printf("No DNS domain configured for %s. "
			 "Unable to perform DNS Update.\n", machine_name);
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
		
		if ( !ads->ld ) {
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
		DEBUG(3,("net_ads_join: Failed to find name server for the %s "
			 "realm\n", ads->config.realm));
		goto done;
	}

		dnsdomain = root_domain;		
		
	}

	/* Now perform the dns update - we'll try non-secure and if we fail,
	   we'll follow it up with a secure update */

	fstrcpy( dns_server, nameservers[0].hostname );

	dns_err = DoDNSUpdate(dns_server, dnsdomain, machine_name, addrs, num_addrs);
	if (!ERR_DNS_IS_OK(dns_err)) {
		status = NT_STATUS_UNSUCCESSFUL;
	}

done:

	SAFE_FREE( root_domain );
	
	return status;
}

static NTSTATUS net_update_dns(TALLOC_CTX *mem_ctx, ADS_STRUCT *ads)
{
	int num_addrs;
	struct in_addr *iplist = NULL;
	fstring machine_name;
	NTSTATUS status;

	name_to_fqdn( machine_name, global_myname() );
	strlower_m( machine_name );

	/* Get our ip address (not the 127.0.0.x address but a real ip
	 * address) */

	num_addrs = get_my_ip_address( &iplist );
	if ( num_addrs <= 0 ) {
		DEBUG(4,("net_ads_join: Failed to find my non-loopback IP "
			 "addresses!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = net_update_dns_internal(mem_ctx, ads, machine_name,
					 iplist, num_addrs);
	SAFE_FREE( iplist );
	return status;
}
#endif


/*******************************************************************
 utility function to parse an integer parameter from 
 "parameter = value"
**********************************************************/
static char* get_string_param( const char* param )
{
	char *p;
	
	if ( (p = strchr( param, '=' )) == NULL )
		return NULL;
		
	return (p+1);
}

/*******************************************************************
 ********************************************************************/
 
static int net_ads_join_usage(int argc, const char **argv)
{
	d_printf("net ads join [options]\n");
	d_printf("Valid options:\n");
	d_printf("   createupn[=UPN]    Set the userPrincipalName attribute during the join.\n");
	d_printf("                      The deault UPN is in the form host/netbiosname@REALM.\n");
	d_printf("   createcomputer=OU  Precreate the computer account in a specific OU.\n");
	d_printf("                      The OU string read from top to bottom without RDNs and delimited by a '/'.\n");
	d_printf("                      E.g. \"createcomputer=Computers/Servers/Unix\"\n");
	d_printf("                      NB: A backslash '\\' is used as escape at multiple levels and may\n");
	d_printf("                          need to be doubled or even quadrupled.  It is not used as a separator");

	return -1;
}

/*******************************************************************
 ********************************************************************/
 
int net_ads_join(int argc, const char **argv)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS status;
	NTSTATUS nt_status;
	char *machine_account = NULL;
	char *short_domain_name = NULL;
	char *tmp_password, *password;
	TALLOC_CTX *ctx = NULL;
	DOM_SID *domain_sid = NULL;
	BOOL createupn = False;
	const char *machineupn = NULL;
	const char *create_in_ou = NULL;
	int i;
	fstring dc_name;
	struct in_addr dcip;
	const char *os_name = NULL;
	const char *os_version = NULL;
	
	nt_status = check_ads_config();
	if (!NT_STATUS_IS_OK(nt_status)) {
		d_fprintf(stderr, "Invalid configuration.  Exiting....\n");
		goto fail;
	}

	/* find a DC to initialize the server affinity cache */

	get_dc_name( lp_workgroup(), lp_realm(), dc_name, &dcip );

	status = ads_startup(True, &ads);
	if (!ADS_ERR_OK(status)) {
		DEBUG(1, ("error on ads_startup: %s\n", ads_errstr(status)));
		nt_status = ads_ntstatus(status);
		goto fail;
	}

	if (strcmp(ads->config.realm, lp_realm()) != 0) {
		d_fprintf(stderr, "realm of remote server (%s) and realm in %s "
			"(%s) DO NOT match.  Aborting join\n", ads->config.realm, 
			dyn_CONFIGFILE, lp_realm());
		nt_status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	if (!(ctx = talloc_init("net_ads_join"))) {
		d_fprintf(stderr, "Could not initialise talloc context.\n");
		nt_status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	/* process additional command line args */
	
	for ( i=0; i<argc; i++ ) {
		if ( !StrnCaseCmp(argv[i], "createupn", strlen("createupn")) ) {
			createupn = True;
			machineupn = get_string_param(argv[i]);
		}
		else if ( !StrnCaseCmp(argv[i], "createcomputer", strlen("createcomputer")) ) {
			if ( (create_in_ou = get_string_param(argv[i])) == NULL ) {
				d_fprintf(stderr, "Please supply a valid OU path.\n");
				nt_status = NT_STATUS_INVALID_PARAMETER;
				goto fail;
			}		
		}
		else if ( !StrnCaseCmp(argv[i], "osName", strlen("osName")) ) {
			if ( (os_name = get_string_param(argv[i])) == NULL ) {
				d_fprintf(stderr, "Please supply a operating system name.\n");
				nt_status = NT_STATUS_INVALID_PARAMETER;
				goto fail;
			}		
		}
		else if ( !StrnCaseCmp(argv[i], "osVer", strlen("osVer")) ) {
			if ( (os_version = get_string_param(argv[i])) == NULL ) {
				d_fprintf(stderr, "Please supply a valid operating system version.\n");
				nt_status = NT_STATUS_INVALID_PARAMETER;
				goto fail;
			}		
		}
		else {
			d_fprintf(stderr, "Bad option: %s\n", argv[i]);
			nt_status = NT_STATUS_INVALID_PARAMETER;
			goto fail;
		}
	}

	/* If we were given an OU, try to create the machine in 
	   the OU account first and then do the normal RPC join */

	if  ( create_in_ou ) {
		status = net_precreate_machine_acct( ads, create_in_ou );
		if ( !ADS_ERR_OK(status) ) {
			d_fprintf( stderr, "Failed to pre-create the machine object "
				"in OU %s.\n", argv[0]);
			DEBUG(1, ("error calling net_precreate_machine_acct: %s\n", 
				  ads_errstr(status)));
			nt_status = ads_ntstatus(status);
			goto fail;
		}
	}

	/* Do the domain join here */

	tmp_password = generate_random_str(DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH);
	password = talloc_strdup(ctx, tmp_password);
	
	nt_status = net_join_domain(ctx, ads->config.ldap_server_name, 
				    &ads->ldap_ip, &short_domain_name, &domain_sid, password);
	if ( !NT_STATUS_IS_OK(nt_status) ) {
		DEBUG(1, ("call of net_join_domain failed: %s\n", 
			  get_friendly_nt_error_msg(nt_status)));
		goto fail;
	}

	/* Check the short name of the domain */
	
	if ( !strequal(lp_workgroup(), short_domain_name) ) {
		d_printf("The workgroup in %s does not match the short\n", dyn_CONFIGFILE);
		d_printf("domain name obtained from the server.\n");
		d_printf("Using the name [%s] from the server.\n", short_domain_name);
		d_printf("You should set \"workgroup = %s\" in %s.\n", 
			 short_domain_name, dyn_CONFIGFILE);
	}
	
	d_printf("Using short domain name -- %s\n", short_domain_name);

	/*  HACK ALERT!  Store the sid and password under both the lp_workgroup() 
	    value from smb.conf and the string returned from the server.  The former is
	    neede to bootstrap winbindd's first connection to the DC to get the real 
	    short domain name   --jerry */
	   
	if ( (netdom_store_machine_account( lp_workgroup(), domain_sid, password ) == -1)
		|| (netdom_store_machine_account( short_domain_name, domain_sid, password ) == -1) )
	{
		/* issue an internal error here for now.
		 * everything else would mean changing tdb routines. */
		nt_status = NT_STATUS_INTERNAL_ERROR;
		goto fail;
	}

	saf_join_store(ads->server.workgroup, ads->config.ldap_server_name);
	saf_join_store(ads->server.realm, ads->config.ldap_server_name);

	/* Verify that everything is ok */

	if ( net_rpc_join_ok(short_domain_name, ads->config.ldap_server_name, &ads->ldap_ip) != 0 ) {
		d_fprintf(stderr, "Failed to verify membership in domain!\n");
		goto fail;
	}	

	/* create the dNSHostName & servicePrincipalName values */
	
	status = net_set_machine_spn( ctx, ads );
	if ( !ADS_ERR_OK(status) )  {

		d_fprintf(stderr, "Failed to set servicePrincipalNames. Please ensure that\n");
		d_fprintf(stderr, "the DNS domain of this server matches the AD domain,\n");
		d_fprintf(stderr, "Or rejoin with using Domain Admin credentials.\n");
		
		/* Disable the machine account in AD.  Better to fail than to leave 
		   a confused admin.  */
		
		if ( net_ads_leave( 0, NULL ) != 0 ) {
			d_fprintf( stderr, "Failed to disable machine account in AD.  Please do so manually.\n");
		}
		
		/* clear out the machine password */
		
		netdom_store_machine_account( lp_workgroup(), domain_sid, "" ); 
		netdom_store_machine_account( short_domain_name, domain_sid, "" );
		
		nt_status = ads_ntstatus(status);
		goto fail;
	}

	if ( !net_derive_salting_principal( ctx, ads ) ) {
		DEBUG(1,("Failed to determine salting principal\n"));
		goto fail;
	}

	if ( createupn ) {
		pstring upn;
		
		/* default to using the short UPN name */
		if ( !machineupn ) {
			snprintf( upn, sizeof(upn), "host/%s@%s", global_myname(), 
				ads->config.realm );
			machineupn = upn;
		}
		
		status = net_set_machine_upn( ctx, ads, machineupn );
		if ( !ADS_ERR_OK(status) )  {
			d_fprintf(stderr, "Failed to set userPrincipalName.  Are you a Domain Admin?\n");
		}
	}

	/* Try to set the operatingSystem attributes if asked */

	if ( os_name && os_version ) {
		status = net_set_os_attributes( ctx, ads, os_name, os_version );
		if ( !ADS_ERR_OK(status) )  {
			d_fprintf(stderr, "Failed to set operatingSystem attributes.  "
				  "Are you a Domain Admin?\n");
		}
	}

	/* Now build the keytab, using the same ADS connection */

	if (lp_use_kerberos_keytab() && ads_keytab_create_default(ads)) {
		DEBUG(1,("Error creating host keytab!\n"));
	}

#if defined(WITH_DNS_UPDATES)
	/* We enter this block with user creds */
	ads_kdestroy( NULL );	
	ads_destroy(&ads);
	ads = NULL;
	
	if ( (ads = ads_init( lp_realm(), NULL, NULL )) != NULL ) {
		/* kinit with the machine password */

		use_in_memory_ccache();
		asprintf( &ads->auth.user_name, "%s$", global_myname() );
		ads->auth.password = secrets_fetch_machine_password(
			lp_workgroup(), NULL, NULL );
		ads->auth.realm = SMB_STRDUP( lp_realm() );
		ads_kinit_password( ads );
	}
	
	if ( !ads || !NT_STATUS_IS_OK(net_update_dns( ctx, ads )) ) {
		d_fprintf( stderr, "DNS update failed!\n" );
	}
	
	/* exit from this block using machine creds */
#endif

	d_printf("Joined '%s' to realm '%s'\n", global_myname(), ads->server.realm);

	SAFE_FREE(machine_account);
	TALLOC_FREE( ctx );
	ads_destroy(&ads);
	
	return 0;

fail:
	/* issue an overall failure message at the end. */
	d_printf("Failed to join domain: %s\n", get_friendly_nt_error_msg(nt_status));

	SAFE_FREE(machine_account);
	TALLOC_FREE( ctx );
        ads_destroy(&ads);

        return -1;

}

/*******************************************************************
 ********************************************************************/
 
static int net_ads_dns_usage(int argc, const char **argv)
{
#if defined(WITH_DNS_UPDATES)
	d_printf("net ads dns <command>\n");
	d_printf("Valid commands:\n");
	d_printf("   register         Issue a dynamic DNS update request for our hostname\n");

	return 0;
#else
	d_fprintf(stderr, "DNS update support not enabled at compile time!\n");
	return -1;
#endif
}

/*******************************************************************
 ********************************************************************/
 
static int net_ads_dns_register(int argc, const char **argv)
{
#if defined(WITH_DNS_UPDATES)
	ADS_STRUCT *ads;
	ADS_STATUS status;
	TALLOC_CTX *ctx;
	
#ifdef DEVELOPER
	talloc_enable_leak_report();
#endif
	
	if (argc > 0) {
		d_fprintf(stderr, "net ads dns register\n");
		return -1;
	}

	if (!(ctx = talloc_init("net_ads_dns"))) {
		d_fprintf(stderr, "Could not initialise talloc context\n");
		return -1;
	}

	status = ads_startup(True, &ads);
	if ( !ADS_ERR_OK(status) ) {
		DEBUG(1, ("error on ads_startup: %s\n", ads_errstr(status)));
		TALLOC_FREE(ctx);
		return -1;
	}

	if ( !NT_STATUS_IS_OK(net_update_dns(ctx, ads)) ) {		
		d_fprintf( stderr, "DNS update failed!\n" );
		ads_destroy( &ads );
		TALLOC_FREE( ctx );
		return -1;
	}
	
	d_fprintf( stderr, "Successfully registered hostname with DNS\n" );

	ads_destroy(&ads);
	TALLOC_FREE( ctx );
	
	return 0;
#else
	d_fprintf(stderr, "DNS update support not enabled at compile time!\n");
	return -1;
#endif
}

#if defined(WITH_DNS_UPDATES)
DNS_ERROR do_gethostbyname(const char *server, const char *host);
#endif

static int net_ads_dns_gethostbyname(int argc, const char **argv)
{
#if defined(WITH_DNS_UPDATES)
	DNS_ERROR err;
	
#ifdef DEVELOPER
	talloc_enable_leak_report();
#endif

	if (argc != 2) {
		d_fprintf(stderr, "net ads dns gethostbyname <server> "
			  "<name>\n");
		return -1;
	}

	err = do_gethostbyname(argv[0], argv[1]);

	d_printf("do_gethostbyname returned %d\n", ERROR_DNS_V(err));
#endif
	return 0;
}

static int net_ads_dns(int argc, const char *argv[])
{
	struct functable func[] = {
		{"REGISTER", net_ads_dns_register},
		{"GETHOSTBYNAME", net_ads_dns_gethostbyname},
		{NULL, NULL}
	};

	return net_run_function(argc, argv, func, net_ads_dns_usage);
}

/*******************************************************************
 ********************************************************************/

int net_ads_printer_usage(int argc, const char **argv)
{
	d_printf(
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
"\n\t(note: printer name is required)\n");
	return -1;
}

/*******************************************************************
 ********************************************************************/

static int net_ads_printer_search(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	LDAPMessage *res = NULL;

	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	rc = ads_find_printers(ads, &res);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "ads_find_printer: %s\n", ads_errstr(rc));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
	 	return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, "No results found\n");
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	ads_dump(ads, res);
	ads_msgfree(ads, res);
	ads_destroy(&ads);
	return 0;
}

static int net_ads_printer_info(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *servername, *printername;
	LDAPMessage *res = NULL;

	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
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
		d_fprintf(stderr, "Server '%s' not found: %s\n", 
			servername, ads_errstr(rc));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, "Printer '%s' not found\n", printername);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	ads_dump(ads, res);
	ads_msgfree(ads, res);
	ads_destroy(&ads);

	return 0;
}

void do_drv_upgrade_printer(int msg_type, struct process_id src,
			    void *buf, size_t len, void *private_data)
{
	return;
}

static int net_ads_printer_publish(int argc, const char **argv)
{
        ADS_STRUCT *ads;
        ADS_STATUS rc;
	const char *servername, *printername;
	struct cli_state *cli;
	struct rpc_pipe_client *pipe_hnd;
	struct in_addr 		server_ip;
	NTSTATUS nt_status;
	TALLOC_CTX *mem_ctx = talloc_init("net_ads_printer_publish");
	ADS_MODLIST mods = ads_init_mods(mem_ctx);
	char *prt_dn, *srv_dn, **srv_cn;
	char *srv_cn_escaped = NULL, *printername_escaped = NULL;
	LDAPMessage *res = NULL;

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		talloc_destroy(mem_ctx);
		return -1;
	}

	if (argc < 1) {
		talloc_destroy(mem_ctx);
		return net_ads_printer_usage(argc, argv);
	}
	
	printername = argv[0];

	if (argc == 2) {
		servername = argv[1];
	} else {
		servername = global_myname();
	}
		
	/* Get printer data from SPOOLSS */

	resolve_name(servername, &server_ip, 0x20);

	nt_status = cli_full_connection(&cli, global_myname(), servername, 
					&server_ip, 0,
					"IPC$", "IPC",  
					opt_user_name, opt_workgroup,
					opt_password ? opt_password : "", 
					CLI_FULL_CONNECTION_USE_KERBEROS, 
					Undefined, NULL);

	if (NT_STATUS_IS_ERR(nt_status)) {
		d_fprintf(stderr, "Unable to open a connnection to %s to obtain data "
			 "for %s\n", servername, printername);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	/* Publish on AD server */

	ads_find_machine_acct(ads, &res, servername);

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, "Could not find machine account for server %s\n", 
			 servername);
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	srv_dn = ldap_get_dn((LDAP *)ads->ld, (LDAPMessage *)res);
	srv_cn = ldap_explode_dn(srv_dn, 1);

	srv_cn_escaped = escape_rdn_val_string_alloc(srv_cn[0]);
	printername_escaped = escape_rdn_val_string_alloc(printername);
	if (!srv_cn_escaped || !printername_escaped) {
		SAFE_FREE(srv_cn_escaped);
		SAFE_FREE(printername_escaped);
		d_fprintf(stderr, "Internal error, out of memory!");
		ads_destroy(&ads);
		talloc_destroy(mem_ctx);
		return -1;
	}

	asprintf(&prt_dn, "cn=%s-%s,%s", srv_cn_escaped, printername_escaped, srv_dn);

	SAFE_FREE(srv_cn_escaped);
	SAFE_FREE(printername_escaped);

	pipe_hnd = cli_rpc_pipe_open_noauth(cli, PI_SPOOLSS, &nt_status);
	if (!pipe_hnd) {
		d_fprintf(stderr, "Unable to open a connnection to the spoolss pipe on %s\n",
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

static int net_ads_printer_remove(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *servername;
	char *prt_dn;
	LDAPMessage *res = NULL;

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}

	if (argc < 1) {
		return net_ads_printer_usage(argc, argv);
	}

	if (argc > 1) {
		servername = argv[1];
	} else {
		servername = global_myname();
	}

	rc = ads_find_printer_on_server(ads, &res, argv[0], servername);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "ads_find_printer_on_server: %s\n", ads_errstr(rc));
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	if (ads_count_replies(ads, res) == 0) {
		d_fprintf(stderr, "Printer '%s' not found\n", argv[1]);
		ads_msgfree(ads, res);
		ads_destroy(&ads);
		return -1;
	}

	prt_dn = ads_get_dn(ads, res);
	ads_msgfree(ads, res);
	rc = ads_del_dn(ads, prt_dn);
	ads_memfree(ads, prt_dn);

	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "ads_del_dn: %s\n", ads_errstr(rc));
		ads_destroy(&ads);
		return -1;
	}

	ads_destroy(&ads);
	return 0;
}

static int net_ads_printer(int argc, const char **argv)
{
	struct functable func[] = {
		{"SEARCH", net_ads_printer_search},
		{"INFO", net_ads_printer_info},
		{"PUBLISH", net_ads_printer_publish},
		{"REMOVE", net_ads_printer_remove},
		{NULL, NULL}
	};
	
	return net_run_function(argc, argv, func, net_ads_printer_usage);
}


static int net_ads_password(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	const char *auth_principal = opt_user_name;
	const char *auth_password = opt_password;
	char *realm = NULL;
	char *new_password = NULL;
	char *c, *prompt;
	const char *user;
	ADS_STATUS ret;

	if (opt_user_name == NULL || opt_password == NULL) {
		d_fprintf(stderr, "You must supply an administrator username/password\n");
		return -1;
	}

	if (argc < 1) {
		d_fprintf(stderr, "ERROR: You must say which username to change password for\n");
		return -1;
	}

	user = argv[0];
	if (!strchr_m(user, '@')) {
		asprintf(&c, "%s@%s", argv[0], lp_realm());
		user = c;
	}

	use_in_memory_ccache();    
	c = strchr_m(auth_principal, '@');
	if (c) {
		realm = ++c;
	} else {
		realm = lp_realm();
	}

	/* use the realm so we can eventually change passwords for users 
	in realms other than default */
	if (!(ads = ads_init(realm, opt_workgroup, opt_host))) {
		return -1;
	}

	/* we don't actually need a full connect, but it's the easy way to
		fill in the KDC's addresss */
	ads_connect(ads);
    
	if (!ads || !ads->config.realm) {
		d_fprintf(stderr, "Didn't find the kerberos server!\n");
		return -1;
	}

	if (argv[1]) {
		new_password = (char *)argv[1];
	} else {
		asprintf(&prompt, "Enter new password for %s:", user);
		new_password = getpass(prompt);
		free(prompt);
	}

	ret = kerberos_set_password(ads->auth.kdc_server, auth_principal, 
				auth_password, user, new_password, ads->auth.time_offset);
	if (!ADS_ERR_OK(ret)) {
		d_fprintf(stderr, "Password change failed: %s\n", ads_errstr(ret));
		ads_destroy(&ads);
		return -1;
	}

	d_printf("Password change for %s completed.\n", user);
	ads_destroy(&ads);

	return 0;
}

int net_ads_changetrustpw(int argc, const char **argv)
{    
	ADS_STRUCT *ads;
	char *host_principal;
	fstring my_name;
	ADS_STATUS ret;

	if (!secrets_init()) {
		DEBUG(1,("Failed to initialise secrets database\n"));
		return -1;
	}

	net_use_krb_machine_account();

	use_in_memory_ccache();

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}

	fstrcpy(my_name, global_myname());
	strlower_m(my_name);
	asprintf(&host_principal, "%s$@%s", my_name, ads->config.realm);
	d_printf("Changing password for principal: %s\n", host_principal);

	ret = ads_change_trust_account_password(ads, host_principal);

	if (!ADS_ERR_OK(ret)) {
		d_fprintf(stderr, "Password change failed: %s\n", ads_errstr(ret));
		ads_destroy(&ads);
		SAFE_FREE(host_principal);
		return -1;
	}
    
	d_printf("Password change for principal %s succeeded.\n", host_principal);

	if (lp_use_kerberos_keytab()) {
		d_printf("Attempting to update system keytab with new password.\n");
		if (ads_keytab_create_default(ads)) {
			d_printf("Failed to update system keytab.\n");
		}
	}

	ads_destroy(&ads);
	SAFE_FREE(host_principal);

	return 0;
}

/*
  help for net ads search
*/
static int net_ads_search_usage(int argc, const char **argv)
{
	d_printf(
		"\nnet ads search <expression> <attributes...>\n"\
		"\nperform a raw LDAP search on a ADS server and dump the results\n"\
		"The expression is a standard LDAP search expression, and the\n"\
		"attributes are a list of LDAP fields to show in the results\n\n"\
		"Example: net ads search '(objectCategory=group)' sAMAccountName\n\n"
		);
	net_common_flags_usage(argc, argv);
	return -1;
}


/*
  general ADS search function. Useful in diagnosing problems in ADS
*/
static int net_ads_search(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *ldap_exp;
	const char **attrs;
	LDAPMessage *res = NULL;

	if (argc < 1) {
		return net_ads_search_usage(argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	ldap_exp = argv[0];
	attrs = (argv + 1);

	rc = ads_do_search_all(ads, ads->config.bind_path,
			       LDAP_SCOPE_SUBTREE,
			       ldap_exp, attrs, &res);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "search failed: %s\n", ads_errstr(rc));
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
  help for net ads search
*/
static int net_ads_dn_usage(int argc, const char **argv)
{
	d_printf(
		"\nnet ads dn <dn> <attributes...>\n"\
		"\nperform a raw LDAP search on a ADS server and dump the results\n"\
		"The DN standard LDAP DN, and the attributes are a list of LDAP fields \n"\
		"to show in the results\n\n"\
		"Example: net ads dn 'CN=administrator,CN=Users,DC=my,DC=domain' sAMAccountName\n\n"
		"Note: the DN must be provided properly escaped. See RFC 4514 for details\n\n"
		);
	net_common_flags_usage(argc, argv);
	return -1;
}


/*
  general ADS search function. Useful in diagnosing problems in ADS
*/
static int net_ads_dn(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *dn;
	const char **attrs;
	LDAPMessage *res = NULL;

	if (argc < 1) {
		return net_ads_dn_usage(argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	dn = argv[0];
	attrs = (argv + 1);

	rc = ads_do_search_all(ads, dn, 
			       LDAP_SCOPE_BASE,
			       "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "search failed: %s\n", ads_errstr(rc));
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
static int net_ads_sid_usage(int argc, const char **argv)
{
	d_printf(
		"\nnet ads sid <sid> <attributes...>\n"\
		"\nperform a raw LDAP search on a ADS server and dump the results\n"\
		"The SID is in string format, and the attributes are a list of LDAP fields \n"\
		"to show in the results\n\n"\
		"Example: net ads sid 'S-1-5-32' distinguishedName\n\n"
		);
	net_common_flags_usage(argc, argv);
	return -1;
}


/*
  general ADS search function. Useful in diagnosing problems in ADS
*/
static int net_ads_sid(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	ADS_STATUS rc;
	const char *sid_string;
	const char **attrs;
	LDAPMessage *res = NULL;
	DOM_SID sid;

	if (argc < 1) {
		return net_ads_sid_usage(argc, argv);
	}

	if (!ADS_ERR_OK(ads_startup(False, &ads))) {
		return -1;
	}

	sid_string = argv[0];
	attrs = (argv + 1);

	if (!string_to_sid(&sid, sid_string)) {
		d_fprintf(stderr, "could not convert sid\n");
		ads_destroy(&ads);
		return -1;
	}

	rc = ads_search_retry_sid(ads, &res, &sid, attrs);
	if (!ADS_ERR_OK(rc)) {
		d_fprintf(stderr, "search failed: %s\n", ads_errstr(rc));
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


static int net_ads_keytab_usage(int argc, const char **argv)
{
	d_printf(
		"net ads keytab <COMMAND>\n"\
"<COMMAND> can be either:\n"\
"  CREATE    Creates a fresh keytab\n"\
"  ADD       Adds new service principal\n"\
"  FLUSH     Flushes out all keytab entries\n"\
"  HELP      Prints this help message\n"\
"The ADD command will take arguments, the other commands\n"\
"will not take any arguments.   The arguments given to ADD\n"\
"should be a list of principals to add.  For example, \n"\
"   net ads keytab add srv1 srv2\n"\
"will add principals for the services srv1 and srv2 to the\n"\
"system's keytab.\n"\
"\n"
		);
	return -1;
}

static int net_ads_keytab_flush(int argc, const char **argv)
{
	int ret;
	ADS_STRUCT *ads;

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}
	ret = ads_keytab_flush(ads);
	ads_destroy(&ads);
	return ret;
}

static int net_ads_keytab_add(int argc, const char **argv)
{
	int i;
	int ret = 0;
	ADS_STRUCT *ads;

	d_printf("Processing principals to add...\n");
	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}
	for (i = 0; i < argc; i++) {
		ret |= ads_keytab_add_entry(ads, argv[i]);
	}
	ads_destroy(&ads);
	return ret;
}

static int net_ads_keytab_create(int argc, const char **argv)
{
	ADS_STRUCT *ads;
	int ret;

	if (!ADS_ERR_OK(ads_startup(True, &ads))) {
		return -1;
	}
	ret = ads_keytab_create_default(ads);
	ads_destroy(&ads);
	return ret;
}

int net_ads_keytab(int argc, const char **argv)
{
	struct functable func[] = {
		{"CREATE", net_ads_keytab_create},
		{"ADD", net_ads_keytab_add},
		{"FLUSH", net_ads_keytab_flush},
		{"HELP", net_ads_keytab_usage},
		{NULL, NULL}
	};

	if (!lp_use_kerberos_keytab()) {
		d_printf("\nWarning: \"use kerberos keytab\" must be set to \"true\" in order to \
use keytab functions.\n");
	}

	return net_run_function(argc, argv, func, net_ads_keytab_usage);
}

int net_ads_help(int argc, const char **argv)
{
	struct functable func[] = {
		{"USER", net_ads_user_usage},
		{"GROUP", net_ads_group_usage},
		{"PRINTER", net_ads_printer_usage},
		{"SEARCH", net_ads_search_usage},
		{"INFO", net_ads_info},
		{"JOIN", net_ads_join_usage},
		{"DNS", net_ads_dns_usage},
		{"LEAVE", net_ads_leave},
		{"STATUS", net_ads_status},
		{"PASSWORD", net_ads_password},
		{"CHANGETRUSTPW", net_ads_changetrustpw},
		{NULL, NULL}
	};

	return net_run_function(argc, argv, func, net_ads_usage);
}

int net_ads(int argc, const char **argv)
{
	struct functable func[] = {
		{"INFO", net_ads_info},
		{"JOIN", net_ads_join},
		{"TESTJOIN", net_ads_testjoin},
		{"LEAVE", net_ads_leave},
		{"STATUS", net_ads_status},
		{"USER", net_ads_user},
		{"GROUP", net_ads_group},
		{"DNS", net_ads_dns},
		{"PASSWORD", net_ads_password},
		{"CHANGETRUSTPW", net_ads_changetrustpw},
		{"PRINTER", net_ads_printer},
		{"SEARCH", net_ads_search},
		{"DN", net_ads_dn},
		{"SID", net_ads_sid},
		{"WORKGROUP", net_ads_workgroup},
		{"LOOKUP", net_ads_lookup},
		{"KEYTAB", net_ads_keytab},
		{"GPO", net_ads_gpo},
		{"HELP", net_ads_help},
		{NULL, NULL}
	};
	
	return net_run_function(argc, argv, func, net_ads_usage);
}

#else

static int net_ads_noads(void)
{
	d_fprintf(stderr, "ADS support not compiled in\n");
	return -1;
}

int net_ads_keytab(int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_usage(int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_help(int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_changetrustpw(int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_join(int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_user(int argc, const char **argv)
{
	return net_ads_noads();
}

int net_ads_group(int argc, const char **argv)
{
	return net_ads_noads();
}

/* this one shouldn't display a message */
int net_ads_check(void)
{
	return -1;
}

int net_ads_check_our_domain(void)
{
	return -1;
}

int net_ads(int argc, const char **argv)
{
	return net_ads_usage(argc, argv);
}

#endif	/* WITH_ADS */
