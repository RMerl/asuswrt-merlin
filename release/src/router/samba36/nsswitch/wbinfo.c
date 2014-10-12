/*
   Unix SMB/CIFS implementation.

   Winbind status program.

   Copyright (C) Tim Potter      2000-2003
   Copyright (C) Andrew Bartlett 2002-2007
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
#include "popt_common.h"
#include "winbind_client.h"
#include "libwbclient/wbclient.h"
#include "lib/popt/popt.h"
#include "../libcli/auth/libcli_auth.h"
#if (_SAMBA_BUILD_) >= 4
#include "lib/cmdline/popt_common.h"
#endif

#ifdef DBGC_CLASS
#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND
#endif

static struct wbcInterfaceDetails *init_interface_details(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	static struct wbcInterfaceDetails *details;

	if (details) {
		return details;
	}

	wbc_status = wbcInterfaceDetails(&details);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "could not obtain winbind interface "
				  "details: %s\n", wbcErrorString(wbc_status));
	}

	return details;
}

static char winbind_separator(void)
{
	struct wbcInterfaceDetails *details;
	static bool got_sep;
	static char sep;

	if (got_sep)
		return sep;

	details = init_interface_details();

	if (!details) {
		d_fprintf(stderr, "could not obtain winbind separator!\n");
		return 0;
	}

	sep = details->winbind_separator;
	got_sep = true;

	if (!sep) {
		d_fprintf(stderr, "winbind separator was NULL!\n");
		return 0;
	}

	return sep;
}

static const char *get_winbind_domain(void)
{
	static struct wbcInterfaceDetails *details;

	details = init_interface_details();

	if (!details) {
		d_fprintf(stderr, "could not obtain winbind domain name!\n");
		return 0;
	}

	return details->netbios_domain;
}

static const char *get_winbind_netbios_name(void)
{
	static struct wbcInterfaceDetails *details;

	details = init_interface_details();

	if (!details) {
		d_fprintf(stderr, "could not obtain winbind netbios name!\n");
		return 0;
	}

	return details->netbios_name;
}

/* Copy of parse_domain_user from winbindd_util.c.  Parse a string of the
   form DOMAIN/user into a domain and a user */

static bool parse_wbinfo_domain_user(const char *domuser, fstring domain,
				     fstring user)
{

	char *p = strchr(domuser,winbind_separator());

	if (!p) {
		/* Maybe it was a UPN? */
		if ((p = strchr(domuser, '@')) != NULL) {
			fstrcpy(domain, "");
			fstrcpy(user, domuser);
			return true;
		}

		fstrcpy(user, domuser);
		fstrcpy(domain, get_winbind_domain());
		return true;
	}

	fstrcpy(user, p+1);
	fstrcpy(domain, domuser);
	domain[PTR_DIFF(p, domuser)] = 0;
	strupper_m(domain);

	return true;
}

/* Parse string of "uid,sid" or "gid,sid" into separate int and string values.
 * Return true if input was valid, false otherwise. */
static bool parse_mapping_arg(char *arg, int *id, char **sid)
{
	char *tmp, *endptr;

	if (!arg || !*arg)
		return false;

	tmp = strtok(arg, ",");
	*sid = strtok(NULL, ",");

	if (!tmp || !*tmp || !*sid || !**sid)
		return false;

	/* Because atoi() can return 0 on invalid input, which would be a valid
	 * UID/GID we must use strtoul() and do error checking */
	*id = strtoul(tmp, &endptr, 10);

	if (endptr[0] != '\0')
		return false;

	return true;
}

/* pull pwent info for a given user */

static bool wbinfo_get_userinfo(char *user)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct passwd *pwd = NULL;

	wbc_status = wbcGetpwnam(user, &pwd);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetpwnam: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	d_printf("%s:%s:%u:%u:%s:%s:%s\n",
		 pwd->pw_name,
		 pwd->pw_passwd,
		 (unsigned int)pwd->pw_uid,
		 (unsigned int)pwd->pw_gid,
		 pwd->pw_gecos,
		 pwd->pw_dir,
		 pwd->pw_shell);

	wbcFreeMemory(pwd);

	return true;
}

/* pull pwent info for a given uid */
static bool wbinfo_get_uidinfo(int uid)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct passwd *pwd = NULL;

	wbc_status = wbcGetpwuid(uid, &pwd);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetpwuid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	d_printf("%s:%s:%u:%u:%s:%s:%s\n",
		 pwd->pw_name,
		 pwd->pw_passwd,
		 (unsigned int)pwd->pw_uid,
		 (unsigned int)pwd->pw_gid,
		 pwd->pw_gecos,
		 pwd->pw_dir,
		 pwd->pw_shell);

	wbcFreeMemory(pwd);

	return true;
}

static bool wbinfo_get_user_sidinfo(const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct passwd *pwd = NULL;
	struct wbcDomainSid sid;

	wbc_status = wbcStringToSid(sid_str, &sid);
	wbc_status = wbcGetpwsid(&sid, &pwd);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetpwsid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	d_printf("%s:%s:%u:%u:%s:%s:%s\n",
		 pwd->pw_name,
		 pwd->pw_passwd,
		 (unsigned int)pwd->pw_uid,
		 (unsigned int)pwd->pw_gid,
		 pwd->pw_gecos,
		 pwd->pw_dir,
		 pwd->pw_shell);

	return true;
}


/* pull grent for a given group */
static bool wbinfo_get_groupinfo(const char *group)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct group *grp;
	char **mem;

	wbc_status = wbcGetgrnam(group, &grp);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetgrnam: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	d_printf("%s:%s:%u:",
		 grp->gr_name,
		 grp->gr_passwd,
		 (unsigned int)grp->gr_gid);

	mem = grp->gr_mem;
	while (*mem != NULL) {
		d_printf("%s%s", *mem, *(mem+1) != NULL ? "," : "");
		mem += 1;
	}
	d_printf("\n");

	wbcFreeMemory(grp);

	return true;
}

/* pull grent for a given gid */
static bool wbinfo_get_gidinfo(int gid)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct group *grp;
	char **mem;

	wbc_status = wbcGetgrgid(gid, &grp);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetgrgid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	d_printf("%s:%s:%u:",
		 grp->gr_name,
		 grp->gr_passwd,
		 (unsigned int)grp->gr_gid);

	mem = grp->gr_mem;
	while (*mem != NULL) {
		d_printf("%s%s", *mem, *(mem+1) != NULL ? "," : "");
		mem += 1;
	}
	d_printf("\n");

	wbcFreeMemory(grp);

	return true;
}

/* List groups a user is a member of */

static bool wbinfo_get_usergroups(const char *user)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uint32_t num_groups;
	uint32_t i;
	gid_t *groups = NULL;

	/* Send request */

	wbc_status = wbcGetGroups(user, &num_groups, &groups);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetGroups: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	for (i = 0; i < num_groups; i++) {
		d_printf("%d\n", (int)groups[i]);
	}

	wbcFreeMemory(groups);

	return true;
}


/* List group SIDs a user SID is a member of */
static bool wbinfo_get_usersids(const char *user_sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uint32_t num_sids;
	uint32_t i;
	struct wbcDomainSid user_sid, *sids = NULL;

	/* Send request */

	wbc_status = wbcStringToSid(user_sid_str, &user_sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcLookupUserSids(&user_sid, false, &num_sids, &sids);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcLookupUserSids: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	for (i = 0; i < num_sids; i++) {
		char str[WBC_SID_STRING_BUFLEN];
		wbcSidToStringBuf(&sids[i], str, sizeof(str));
		d_printf("%s\n", str);
	}

	wbcFreeMemory(sids);

	return true;
}

static bool wbinfo_get_userdomgroups(const char *user_sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uint32_t num_sids;
	uint32_t i;
	struct wbcDomainSid user_sid, *sids = NULL;

	/* Send request */

	wbc_status = wbcStringToSid(user_sid_str, &user_sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcLookupUserSids(&user_sid, true, &num_sids, &sids);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcLookupUserSids: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	for (i = 0; i < num_sids; i++) {
		char str[WBC_SID_STRING_BUFLEN];
		wbcSidToStringBuf(&sids[i], str, sizeof(str));
		d_printf("%s\n", str);
	}

	wbcFreeMemory(sids);

	return true;
}

static bool wbinfo_get_sidaliases(const char *domain,
				  const char *user_sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainInfo *dinfo = NULL;
	uint32_t i;
	struct wbcDomainSid user_sid;
	uint32_t *alias_rids = NULL;
	uint32_t num_alias_rids;
	char domain_sid_str[WBC_SID_STRING_BUFLEN];

	/* Send request */
	if ((domain == NULL) || (strequal(domain, ".")) ||
           (domain[0] == '\0')) {
		domain = get_winbind_domain();
	}

	/* Send request */

	wbc_status = wbcDomainInfo(domain, &dinfo);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "wbcDomainInfo(%s) failed: %s\n", domain,
			  wbcErrorString(wbc_status));
		goto done;
	}
	wbc_status = wbcStringToSid(user_sid_str, &user_sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		goto done;
	}

	wbc_status = wbcGetSidAliases(&dinfo->sid, &user_sid, 1,
	    &alias_rids, &num_alias_rids);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		goto done;
	}

	wbcSidToStringBuf(&dinfo->sid, domain_sid_str, sizeof(domain_sid_str));

	for (i = 0; i < num_alias_rids; i++) {
		d_printf("%s-%d\n", domain_sid_str, alias_rids[i]);
	}

	wbcFreeMemory(alias_rids);

done:
	wbcFreeMemory(dinfo);
	return (WBC_ERR_SUCCESS == wbc_status);
}


/* Convert NetBIOS name to IP */

static bool wbinfo_wins_byname(const char *name)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *ip = NULL;

	wbc_status = wbcResolveWinsByName(name, &ip);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcResolveWinsByName: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("%s\n", ip);

	wbcFreeMemory(ip);

	return true;
}

/* Convert IP to NetBIOS name */

static bool wbinfo_wins_byip(const char *ip)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *name = NULL;

	wbc_status = wbcResolveWinsByIP(ip, &name);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcResolveWinsByIP: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("%s\n", name);

	wbcFreeMemory(name);

	return true;
}

/* List all/trusted domains */

static bool wbinfo_list_domains(bool list_all_domains, bool verbose)
{
	struct wbcDomainInfo *domain_list = NULL;
	size_t num_domains;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	bool print_all = !list_all_domains && verbose;
	int i;

	wbc_status = wbcListTrusts(&domain_list, &num_domains);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcListTrusts: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	if (print_all) {
		d_printf("%-16s%-24s%-12s%-12s%-5s%-5s\n",
			 "Domain Name", "DNS Domain", "Trust Type",
			 "Transitive", "In", "Out");
	}

	for (i=0; i<num_domains; i++) {
		if (print_all) {
			d_printf("%-16s", domain_list[i].short_name);
		} else {
			d_printf("%s", domain_list[i].short_name);
			d_printf("\n");
			continue;
		}

		d_printf("%-24s", domain_list[i].dns_name);

		switch(domain_list[i].trust_type) {
		case WBC_DOMINFO_TRUSTTYPE_NONE:
			d_printf("None        ");
			break;
		case WBC_DOMINFO_TRUSTTYPE_FOREST:
			d_printf("Forest      ");
			break;
		case WBC_DOMINFO_TRUSTTYPE_EXTERNAL:
			d_printf("External    ");
			break;
		case WBC_DOMINFO_TRUSTTYPE_IN_FOREST:
			d_printf("In-Forest   ");
			break;
		}

		if (domain_list[i].trust_flags & WBC_DOMINFO_TRUST_TRANSITIVE) {
			d_printf("Yes         ");
		} else {
			d_printf("No          ");
		}

		if (domain_list[i].trust_flags & WBC_DOMINFO_TRUST_INCOMING) {
			d_printf("Yes  ");
		} else {
			d_printf("No   ");
		}

		if (domain_list[i].trust_flags & WBC_DOMINFO_TRUST_OUTGOING) {
			d_printf("Yes  ");
		} else {
			d_printf("No   ");
		}

		d_printf("\n");
	}

	wbcFreeMemory(domain_list);

	return true;
}

/* List own domain */

static bool wbinfo_list_own_domain(void)
{
	d_printf("%s\n", get_winbind_domain());

	return true;
}

/* show sequence numbers */
static bool wbinfo_show_sequence(const char *domain)
{
	d_printf("This command has been deprecated.  Please use the "
		 "--online-status option instead.\n");
	return false;
}

/* show sequence numbers */
static bool wbinfo_show_onlinestatus(const char *domain)
{
	struct wbcDomainInfo *domain_list = NULL;
	size_t num_domains;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	int i;

	wbc_status = wbcListTrusts(&domain_list, &num_domains);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcListTrusts: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	for (i=0; i<num_domains; i++) {
		bool is_offline;

		if (domain) {
			if (!strequal(domain_list[i].short_name, domain)) {
				continue;
			}
		}

		is_offline = (domain_list[i].domain_flags &
			      WBC_DOMINFO_DOMAIN_OFFLINE);

		d_printf("%s : %s\n",
			 domain_list[i].short_name,
			 is_offline ? "offline" : "online" );
	}

	wbcFreeMemory(domain_list);

	return true;
}


/* Show domain info */

static bool wbinfo_domain_info(const char *domain)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainInfo *dinfo = NULL;
	char sid_str[WBC_SID_STRING_BUFLEN];

	if ((domain == NULL) || (strequal(domain, ".")) || (domain[0] == '\0')){
		domain = get_winbind_domain();
	}

	/* Send request */

	wbc_status = wbcDomainInfo(domain, &dinfo);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcDomainInfo: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbcSidToStringBuf(&dinfo->sid, sid_str, sizeof(sid_str));

	/* Display response */

	d_printf("Name              : %s\n", dinfo->short_name);
	d_printf("Alt_Name          : %s\n", dinfo->dns_name);

	d_printf("SID               : %s\n", sid_str);

	d_printf("Active Directory  : %s\n",
		 (dinfo->domain_flags & WBC_DOMINFO_DOMAIN_AD) ? "Yes" : "No");
	d_printf("Native            : %s\n",
		 (dinfo->domain_flags & WBC_DOMINFO_DOMAIN_NATIVE) ?
		 "Yes" : "No");

	d_printf("Primary           : %s\n",
		 (dinfo->domain_flags & WBC_DOMINFO_DOMAIN_PRIMARY) ?
		 "Yes" : "No");

	wbcFreeMemory(dinfo);

	return true;
}

/* Get a foreign DC's name */
static bool wbinfo_getdcname(const char *domain_name)
{
	struct winbindd_request request;
	struct winbindd_response response;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	fstrcpy(request.domain_name, domain_name);

	/* Send request */

	if (winbindd_request_response(WINBINDD_GETDCNAME, &request,
				      &response) != NSS_STATUS_SUCCESS) {
		d_fprintf(stderr, "Could not get dc name for %s\n",domain_name);
		return false;
	}

	/* Display response */

	d_printf("%s\n", response.data.dc_name);

	return true;
}

/* Find a DC */
static bool wbinfo_dsgetdcname(const char *domain_name, uint32_t flags)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainControllerInfoEx *dc_info;
	char *str = NULL;

	wbc_status = wbcLookupDomainControllerEx(domain_name, NULL, NULL,
						 flags | DS_DIRECTORY_SERVICE_REQUIRED,
						 &dc_info);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		printf("Could not find dc for %s\n", domain_name);
		return false;
	}

	wbcGuidToString(dc_info->domain_guid, &str);

	d_printf("%s\n", dc_info->dc_unc);
	d_printf("%s\n", dc_info->dc_address);
	d_printf("%d\n", dc_info->dc_address_type);
	d_printf("%s\n", str);
	d_printf("%s\n", dc_info->domain_name);
	d_printf("%s\n", dc_info->forest_name);
	d_printf("0x%08x\n", dc_info->dc_flags);
	d_printf("%s\n", dc_info->dc_site_name);
	d_printf("%s\n", dc_info->client_site_name);

	return true;
}

/* Check trust account password */

static bool wbinfo_check_secret(const char *domain)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcAuthErrorInfo *error = NULL;
	const char *domain_name;

	if (domain) {
		domain_name = domain;
	} else {
		domain_name = get_winbind_domain();
	}

	wbc_status = wbcCheckTrustCredentials(domain_name, &error);

	d_printf("checking the trust secret for domain %s via RPC calls %s\n",
		domain_name,
		WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	if (wbc_status == WBC_ERR_AUTH_ERROR) {
		d_fprintf(stderr, "error code was %s (0x%x)\n",
			  error->nt_string, error->nt_status);
		wbcFreeMemory(error);
	}
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcCheckTrustCredentials: "
			  "%s\n", wbcErrorString(wbc_status));
		return false;
	}

	return true;
}

/* Find the currently connected DCs */

static bool wbinfo_dc_info(const char *domain_name)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	size_t i, num_dcs;
	const char **dc_names, **dc_ips;

	wbc_status = wbcDcInfo(domain_name, &num_dcs,
			       &dc_names, &dc_ips);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		printf("Could not find dc info %s\n",
		       domain_name ? domain_name : "our domain");
		return false;
	}

	for (i=0; i<num_dcs; i++) {
		printf("%s (%s)\n", dc_names[i], dc_ips[i]);
	}
	wbcFreeMemory(dc_names);
	wbcFreeMemory(dc_ips);

	return true;
}

/* Change trust account password */

static bool wbinfo_change_secret(const char *domain)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcAuthErrorInfo *error = NULL;
	const char *domain_name;

	if (domain) {
		domain_name = domain;
	} else {
		domain_name = get_winbind_domain();
	}

	wbc_status = wbcChangeTrustCredentials(domain_name, &error);

	d_printf("changing the trust secret for domain %s via RPC calls %s\n",
		domain_name,
		WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	if (wbc_status == WBC_ERR_AUTH_ERROR) {
		d_fprintf(stderr, "error code was %s (0x%x)\n",
			  error->nt_string, error->nt_status);
		wbcFreeMemory(error);
	}
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcChangeTrustCredentials: "
			  "%s\n", wbcErrorString(wbc_status));
		return false;
	}

	return true;
}

/* Check DC connection */

static bool wbinfo_ping_dc(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcAuthErrorInfo *error = NULL;

	wbc_status = wbcPingDc(NULL, &error);

	d_printf("checking the NETLOGON dc connection %s\n",
		 WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	if (wbc_status == WBC_ERR_AUTH_ERROR) {
		d_fprintf(stderr, "error code was %s (0x%x)\n",
			  error->nt_string, error->nt_status);
		wbcFreeMemory(error);
	}
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcPingDc: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	return true;
}

/* Convert uid to sid */

static bool wbinfo_uid_to_sid(uid_t uid)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	char sid_str[WBC_SID_STRING_BUFLEN];

	/* Send request */

	wbc_status = wbcUidToSid(uid, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcUidToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbcSidToStringBuf(&sid, sid_str, sizeof(sid_str));

	/* Display response */

	d_printf("%s\n", sid_str);

	return true;
}

/* Convert gid to sid */

static bool wbinfo_gid_to_sid(gid_t gid)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	char sid_str[WBC_SID_STRING_BUFLEN];

	/* Send request */

	wbc_status = wbcGidToSid(gid, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGidToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbcSidToStringBuf(&sid, sid_str, sizeof(sid_str));

	/* Display response */

	d_printf("%s\n", sid_str);

	return true;
}

/* Convert sid to uid */

static bool wbinfo_sid_to_uid(const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	uid_t uid;

	/* Send request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcSidToUid(&sid, &uid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcSidToUid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("%d\n", (int)uid);

	return true;
}

static bool wbinfo_sid_to_gid(const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	gid_t gid;

	/* Send request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcSidToGid(&sid, &gid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcSidToGid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("%d\n", (int)gid);

	return true;
}

static bool wbinfo_sids_to_unix_ids(const char *arg)
{
	char sidstr[WBC_SID_STRING_BUFLEN];
	struct wbcDomainSid *sids;
	struct wbcUnixId *unix_ids;
	int i, num_sids;
	const char *p;
	wbcErr wbc_status;


	num_sids = 0;
	sids = NULL;
	p = arg;

	while (next_token(&p, sidstr, LIST_SEP, sizeof(sidstr))) {
		sids = talloc_realloc(talloc_tos(), sids, struct wbcDomainSid,
				      num_sids+1);
		if (sids == NULL) {
			d_fprintf(stderr, "talloc failed\n");
			return false;
		}
		wbc_status = wbcStringToSid(sidstr, &sids[num_sids]);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			d_fprintf(stderr, "wbcSidToString(%s) failed: %s\n",
				  sidstr, wbcErrorString(wbc_status));
			TALLOC_FREE(sids);
			return false;
		}
		num_sids += 1;
	}

	unix_ids = talloc_array(talloc_tos(), struct wbcUnixId, num_sids);
	if (unix_ids == NULL) {
		TALLOC_FREE(sids);
		return false;
	}

	wbc_status = wbcSidsToUnixIds(sids, num_sids, unix_ids);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "wbcSidsToUnixIds failed: %s\n",
			  wbcErrorString(wbc_status));
		TALLOC_FREE(sids);
		return false;
	}

	for (i=0; i<num_sids; i++) {

		wbcSidToStringBuf(&sids[i], sidstr, sizeof(sidstr));

		switch(unix_ids[i].type) {
		case WBC_ID_TYPE_UID:
			d_printf("%s -> uid %d\n", sidstr, unix_ids[i].id.uid);
			break;
		case WBC_ID_TYPE_GID:
			d_printf("%s -> gid %d\n", sidstr, unix_ids[i].id.gid);
			break;
		default:
			d_printf("%s -> unmapped\n", sidstr);
			break;
		}
	}

	TALLOC_FREE(sids);
	TALLOC_FREE(unix_ids);

	return true;
}

static bool wbinfo_allocate_uid(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uid_t uid;

	/* Send request */

	wbc_status = wbcAllocateUid(&uid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcAllocateUid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("New uid: %u\n", (unsigned int)uid);

	return true;
}

static bool wbinfo_allocate_gid(void)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	gid_t gid;

	/* Send request */

	wbc_status = wbcAllocateGid(&gid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcAllocateGid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("New gid: %u\n", (unsigned int)gid);

	return true;
}

static bool wbinfo_set_uid_mapping(uid_t uid, const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;

	/* Send request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcSetUidMapping(uid, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcSetUidMapping: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("uid %u now mapped to sid %s\n",
		(unsigned int)uid, sid_str);

	return true;
}

static bool wbinfo_set_gid_mapping(gid_t gid, const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;

	/* Send request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcSetGidMapping(gid, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcSetGidMapping: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("gid %u now mapped to sid %s\n",
		(unsigned int)gid, sid_str);

	return true;
}

static bool wbinfo_remove_uid_mapping(uid_t uid, const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;

	/* Send request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcRemoveUidMapping(uid, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcRemoveUidMapping: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("Removed uid %u to sid %s mapping\n",
		(unsigned int)uid, sid_str);

	return true;
}

static bool wbinfo_remove_gid_mapping(gid_t gid, const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;

	/* Send request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcRemoveGidMapping(gid, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcRemoveGidMapping: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("Removed gid %u to sid %s mapping\n",
		(unsigned int)gid, sid_str);

	return true;
}

/* Convert sid to string */

static bool wbinfo_lookupsid(const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	char *domain;
	char *name;
	enum wbcSidType type;

	/* Send off request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcLookupSid(&sid, &domain, &name, &type);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcLookupSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("%s%c%s %d\n",
		 domain, winbind_separator(), name, type);

	return true;
}

/* Convert sid to fullname */

static bool wbinfo_lookupsid_fullname(const char *sid_str)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	char *domain;
	char *name;
	enum wbcSidType type;

	/* Send off request */

	wbc_status = wbcStringToSid(sid_str, &sid);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcStringToSid: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcGetDisplayName(&sid, &domain, &name, &type);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcGetDisplayName: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	/* Display response */

	d_printf("%s%c%s %d\n",
		 domain, winbind_separator(), name, type);

	return true;
}

/* Lookup a list of RIDs */

static bool wbinfo_lookuprids(const char *domain, const char *arg)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainInfo *dinfo = NULL;
	char *domain_name = NULL;
	const char **names = NULL;
	enum wbcSidType *types = NULL;
	size_t i;
	int num_rids;
	uint32_t *rids = NULL;
	const char *p;
	char *ridstr;
	TALLOC_CTX *mem_ctx = NULL;
	bool ret = false;

	if ((domain == NULL) || (strequal(domain, ".")) || (domain[0] == '\0')){
		domain = get_winbind_domain();
	}

	/* Send request */

	wbc_status = wbcDomainInfo(domain, &dinfo);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_printf("wbcDomainInfo(%s) failed: %s\n", domain,
			 wbcErrorString(wbc_status));
		goto done;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		d_printf("talloc_new failed\n");
		goto done;
	}

	num_rids = 0;
	rids = NULL;
	p = arg;

	while (next_token_talloc(mem_ctx, &p, &ridstr, " ,\n")) {
		uint32_t rid = strtoul(ridstr, NULL, 10);
		rids = talloc_realloc(mem_ctx, rids, uint32_t, num_rids + 1);
		if (rids == NULL) {
			d_printf("talloc_realloc failed\n");
		}
		rids[num_rids] = rid;
		num_rids += 1;
	}

	if (rids == NULL) {
		d_printf("no rids\n");
		goto done;
	}

	wbc_status = wbcLookupRids(&dinfo->sid, num_rids, rids,
				   (const char **)&domain_name, &names, &types);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_printf("winbind_lookup_rids failed: %s\n",
			 wbcErrorString(wbc_status));
		goto done;
	}

	d_printf("Domain: %s\n", domain_name);

	for (i=0; i<num_rids; i++) {
		d_printf("%8d: %s (%s)\n", rids[i], names[i],
			 wbcSidTypeString(types[i]));
	}

	ret = true;
done:
	wbcFreeMemory(dinfo);
	wbcFreeMemory(domain_name);
	wbcFreeMemory(names);
	wbcFreeMemory(types);
	TALLOC_FREE(mem_ctx);
	return ret;
}

static bool wbinfo_lookup_sids(const char *arg)
{
	char sidstr[WBC_SID_STRING_BUFLEN];
	struct wbcDomainSid *sids;
	struct wbcDomainInfo *domains;
	struct wbcTranslatedName *names;
	int num_domains;
	int i, num_sids;
	const char *p;
	wbcErr wbc_status;


	num_sids = 0;
	sids = NULL;
	p = arg;

	while (next_token(&p, sidstr, LIST_SEP, sizeof(sidstr))) {
		sids = talloc_realloc(talloc_tos(), sids, struct wbcDomainSid,
				      num_sids+1);
		if (sids == NULL) {
			d_fprintf(stderr, "talloc failed\n");
			return false;
		}
		wbc_status = wbcStringToSid(sidstr, &sids[num_sids]);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			d_fprintf(stderr, "wbcSidToString(%s) failed: %s\n",
				  sidstr, wbcErrorString(wbc_status));
			TALLOC_FREE(sids);
			return false;
		}
		num_sids += 1;
	}

	wbc_status = wbcLookupSids(sids, num_sids, &domains, &num_domains,
				   &names);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "wbcLookupSids failed: %s\n",
			  wbcErrorString(wbc_status));
		TALLOC_FREE(sids);
		return false;
	}

	for (i=0; i<num_sids; i++) {
		const char *domain = NULL;

		wbcSidToStringBuf(&sids[i], sidstr, sizeof(sidstr));

		if (names[i].domain_index >= num_domains) {
			domain = "<none>";
		} else if (names[i].domain_index < 0) {
			domain = "<none>";
		} else {
			domain = domains[names[i].domain_index].short_name;
		}

		if (names[i].type == WBC_SID_NAME_DOMAIN) {
			d_printf("%s -> %s %d\n", sidstr,
				domain,
				names[i].type);
		} else {
			d_printf("%s -> %s%c%s %d\n", sidstr,
				domain,
				winbind_separator(),
				names[i].name, names[i].type);
		}
	}
	return true;
}

/* Convert string to sid */

static bool wbinfo_lookupname(const char *full_name)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	char sid_str[WBC_SID_STRING_BUFLEN];
	enum wbcSidType type;
	fstring domain_name;
	fstring account_name;

	/* Send off request */

	parse_wbinfo_domain_user(full_name, domain_name,
				 account_name);

	wbc_status = wbcLookupName(domain_name, account_name,
				   &sid, &type);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcLookupName: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	wbcSidToStringBuf(&sid, sid_str, sizeof(sid_str));

	/* Display response */

	d_printf("%s %s (%d)\n", sid_str, wbcSidTypeString(type), type);

	return true;
}

static char *wbinfo_prompt_pass(TALLOC_CTX *mem_ctx,
				const char *prefix,
				const char *username)
{
	char *prompt;
	const char *ret = NULL;

	prompt = talloc_asprintf(mem_ctx, "Enter %s's ", username);
	if (!prompt) {
		return NULL;
	}
	if (prefix) {
		prompt = talloc_asprintf_append(prompt, "%s ", prefix);
		if (!prompt) {
			return NULL;
		}
	}
	prompt = talloc_asprintf_append(prompt, "password: ");
	if (!prompt) {
		return NULL;
	}

	ret = getpass(prompt);
	TALLOC_FREE(prompt);

	return talloc_strdup(mem_ctx, ret);
}

/* Authenticate a user with a plaintext password */

static bool wbinfo_auth_krb5(char *username, const char *cctype, uint32_t flags)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *s = NULL;
	char *p = NULL;
	char *password = NULL;
	char *name = NULL;
	char *local_cctype = NULL;
	uid_t uid;
	struct wbcLogonUserParams params;
	struct wbcLogonUserInfo *info;
	struct wbcAuthErrorInfo *error;
	struct wbcUserPasswordPolicyInfo *policy;
	TALLOC_CTX *frame = talloc_tos();

	if ((s = talloc_strdup(frame, username)) == NULL) {
		return false;
	}

	if ((p = strchr(s, '%')) != NULL) {
		*p = 0;
		p++;
		password = talloc_strdup(frame, p);
	} else {
		password = wbinfo_prompt_pass(frame, NULL, username);
	}

	local_cctype = talloc_strdup(frame, cctype);

	name = s;

	uid = geteuid();

	params.username = name;
	params.password = password;
	params.num_blobs = 0;
	params.blobs = NULL;

	wbc_status = wbcAddNamedBlob(&params.num_blobs,
				     &params.blobs,
				     "flags",
				     0,
				     (uint8_t *)&flags,
				     sizeof(flags));
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcAddNamedBlob: %s\n",
			  wbcErrorString(wbc_status));
		goto done;
	}

	wbc_status = wbcAddNamedBlob(&params.num_blobs,
				     &params.blobs,
				     "user_uid",
				     0,
				     (uint8_t *)&uid,
				     sizeof(uid));
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcAddNamedBlob: %s\n",
			  wbcErrorString(wbc_status));
		goto done;
	}

	wbc_status = wbcAddNamedBlob(&params.num_blobs,
				     &params.blobs,
				     "krb5_cc_type",
				     0,
				     (uint8_t *)local_cctype,
				     strlen(cctype)+1);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcAddNamedBlob: %s\n",
			  wbcErrorString(wbc_status));
		goto done;
	}

	wbc_status = wbcLogonUser(&params, &info, &error, &policy);

	d_printf("plaintext kerberos password authentication for [%s] %s "
		 "(requesting cctype: %s)\n",
		 username, WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed",
		 cctype);

	if (error) {
		d_fprintf(stderr,
			 "error code was %s (0x%x)\nerror message was: %s\n",
			 error->nt_string,
			 error->nt_status,
			 error->display_string);
	}

	if (WBC_ERROR_IS_OK(wbc_status)) {
		if (flags & WBFLAG_PAM_INFO3_TEXT) {
			if (info && info->info && info->info->user_flags &
			    NETLOGON_CACHED_ACCOUNT) {
				d_printf("user_flgs: "
					 "NETLOGON_CACHED_ACCOUNT\n");
			}
		}

		if (info) {
			int i;
			for (i=0; i < info->num_blobs; i++) {
				if (strequal(info->blobs[i].name,
					     "krb5ccname")) {
					d_printf("credentials were put "
						 "in: %s\n",
						(const char *)
						      info->blobs[i].blob.data);
					break;
				}
			}
		} else {
			d_printf("no credentials cached\n");
		}
	}
 done:

	wbcFreeMemory(params.blobs);

	return WBC_ERROR_IS_OK(wbc_status);
}

/* Authenticate a user with a plaintext password */

static bool wbinfo_auth(char *username)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *s = NULL;
	char *p = NULL;
	char *password = NULL;
	char *name = NULL;
	TALLOC_CTX *frame = talloc_tos();

	if ((s = talloc_strdup(frame, username)) == NULL) {
		return false;
	}

	if ((p = strchr(s, '%')) != NULL) {
		*p = 0;
		p++;
		password = talloc_strdup(frame, p);
	} else {
		password = wbinfo_prompt_pass(frame, NULL, username);
	}

	name = s;

	wbc_status = wbcAuthenticateUser(name, password);

	d_printf("plaintext password authentication %s\n",
		 WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

#if 0
	if (response.data.auth.nt_status)
		d_fprintf(stderr,
			 "error code was %s (0x%x)\nerror message was: %s\n",
			 response.data.auth.nt_status_string,
			 response.data.auth.nt_status,
			 response.data.auth.error_string);
#endif

	return WBC_ERROR_IS_OK(wbc_status);
}

/* Authenticate a user with a challenge/response */

static bool wbinfo_auth_crap(char *username, bool use_ntlmv2, bool use_lanman)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcAuthUserParams params;
	struct wbcAuthUserInfo *info = NULL;
	struct wbcAuthErrorInfo *err = NULL;
	DATA_BLOB lm = data_blob_null;
	DATA_BLOB nt = data_blob_null;
	fstring name_user;
	fstring name_domain;
	char *pass;
	char *p;
	TALLOC_CTX *frame = talloc_tos();

	p = strchr(username, '%');

	if (p) {
		*p = 0;
		pass = talloc_strdup(frame, p + 1);
	} else {
		pass = wbinfo_prompt_pass(frame, NULL, username);
	}

	parse_wbinfo_domain_user(username, name_domain, name_user);

	params.account_name	= name_user;
	params.domain_name	= name_domain;
	params.workstation_name	= NULL;

	params.flags		= 0;
	params.parameter_control= WBC_MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT |
				  WBC_MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT;

	params.level		= WBC_AUTH_USER_LEVEL_RESPONSE;

	generate_random_buffer(params.password.response.challenge, 8);

	if (use_ntlmv2) {
		DATA_BLOB server_chal;
		DATA_BLOB names_blob;

		server_chal = data_blob(params.password.response.challenge, 8);

		/* Pretend this is a login to 'us', for blob purposes */
		names_blob = NTLMv2_generate_names_blob(NULL,
						get_winbind_netbios_name(),
						get_winbind_domain());

		if (!SMBNTLMv2encrypt(NULL, name_user, name_domain, pass,
				      &server_chal,
				      &names_blob,
				      &lm, &nt, NULL, NULL)) {
			data_blob_free(&names_blob);
			data_blob_free(&server_chal);
			TALLOC_FREE(pass);
			return false;
		}
		data_blob_free(&names_blob);
		data_blob_free(&server_chal);

	} else {
		if (use_lanman) {
			bool ok;
			lm = data_blob(NULL, 24);
			ok = SMBencrypt(pass,
					params.password.response.challenge,
					lm.data);
			if (!ok) {
				data_blob_free(&lm);
			}
		}
		nt = data_blob(NULL, 24);
		SMBNTencrypt(pass, params.password.response.challenge,
			     nt.data);
	}

	params.password.response.nt_length	= nt.length;
	params.password.response.nt_data	= nt.data;
	params.password.response.lm_length	= lm.length;
	params.password.response.lm_data	= lm.data;

	wbc_status = wbcAuthenticateUserEx(&params, &info, &err);

	/* Display response */

	d_printf("challenge/response password authentication %s\n",
		 WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	if (wbc_status == WBC_ERR_AUTH_ERROR) {
		d_fprintf(stderr,
			 "error code was %s (0x%x)\nerror message was: %s\n",
			 err->nt_string,
			 err->nt_status,
			 err->display_string);
		wbcFreeMemory(err);
	} else if (WBC_ERROR_IS_OK(wbc_status)) {
		wbcFreeMemory(info);
	}

	data_blob_free(&nt);
	data_blob_free(&lm);

	return WBC_ERROR_IS_OK(wbc_status);
}

/* Authenticate a user with a plaintext password */

static bool wbinfo_pam_logon(char *username)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcLogonUserParams params;
	struct wbcAuthErrorInfo *error = NULL;
	char *s = NULL;
	char *p = NULL;
	TALLOC_CTX *frame = talloc_tos();
	uint32_t flags;
	uint32_t uid;

	ZERO_STRUCT(params);

	if ((s = talloc_strdup(frame, username)) == NULL) {
		return false;
	}

	if ((p = strchr(s, '%')) != NULL) {
		*p = 0;
		p++;
		params.password = talloc_strdup(frame, p);
	} else {
		params.password = wbinfo_prompt_pass(frame, NULL, username);
	}
	params.username = s;

	flags = WBFLAG_PAM_CACHED_LOGIN;

	wbc_status = wbcAddNamedBlob(&params.num_blobs, &params.blobs,
				     "flags", 0,
				     (uint8_t *)&flags, sizeof(flags));
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_printf("wbcAddNamedBlob failed: %s\n",
			 wbcErrorString(wbc_status));
		return false;
	}

	uid = getuid();

	wbc_status = wbcAddNamedBlob(&params.num_blobs, &params.blobs,
				     "user_uid", 0,
				     (uint8_t *)&uid, sizeof(uid));
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_printf("wbcAddNamedBlob failed: %s\n",
			 wbcErrorString(wbc_status));
		return false;
	}

	wbc_status = wbcLogonUser(&params, NULL, &error, NULL);

	wbcFreeMemory(params.blobs);

	d_printf("plaintext password authentication %s\n",
		 WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	if (!WBC_ERROR_IS_OK(wbc_status) && (error != NULL)) {
		d_fprintf(stderr,
			  "error code was %s (0x%x)\nerror message was: %s\n",
			  error->nt_string,
			  (int)error->nt_status,
			  error->display_string);
		wbcFreeMemory(error);
	}
	return WBC_ERROR_IS_OK(wbc_status);
}

/* Save creds with winbind */

static bool wbinfo_ccache_save(char *username)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *s = NULL;
	char *p = NULL;
	char *password = NULL;
	char *name = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	s = talloc_strdup(frame, username);
	if (s == NULL) {
		return false;
	}

	p = strchr(s, '%');
	if (p != NULL) {
		*p = 0;
		p++;
		password = talloc_strdup(frame, p);
	} else {
		password = wbinfo_prompt_pass(frame, NULL, username);
	}

	name = s;

	wbc_status = wbcCredentialSave(name, password);

	d_printf("saving creds %s\n",
		 WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	TALLOC_FREE(frame);

	return WBC_ERROR_IS_OK(wbc_status);
}

#ifdef WITH_FAKE_KASERVER
/* Authenticate a user with a plaintext password and set a token */

static bool wbinfo_klog(char *username)
{
	struct winbindd_request request;
	struct winbindd_response response;
	NSS_STATUS result;
	char *p;

	/* Send off request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	p = strchr(username, '%');

	if (p) {
		*p = 0;
		fstrcpy(request.data.auth.user, username);
		fstrcpy(request.data.auth.pass, p + 1);
		*p = '%';
	} else {
		fstrcpy(request.data.auth.user, username);
		fstrcpy(request.data.auth.pass, getpass("Password: "));
	}

	request.flags |= WBFLAG_PAM_AFS_TOKEN;

	result = winbindd_request_response(WINBINDD_PAM_AUTH, &request,
					   &response);

	/* Display response */

	d_printf("plaintext password authentication %s\n",
		 (result == NSS_STATUS_SUCCESS) ? "succeeded" : "failed");

	if (response.data.auth.nt_status)
		d_fprintf(stderr,
			 "error code was %s (0x%x)\nerror message was: %s\n",
			 response.data.auth.nt_status_string,
			 response.data.auth.nt_status,
			 response.data.auth.error_string);

	if (result != NSS_STATUS_SUCCESS)
		return false;

	if (response.extra_data.data == NULL) {
		d_fprintf(stderr, "Did not get token data\n");
		return false;
	}

	if (!afs_settoken_str((char *)response.extra_data.data)) {
		d_fprintf(stderr, "Could not set token\n");
		return false;
	}

	d_printf("Successfully created AFS token\n");
	return true;
}
#else
static bool wbinfo_klog(char *username)
{
	d_fprintf(stderr, "No AFS support compiled in.\n");
	return false;
}
#endif

/* Print domain users */

static bool print_domain_users(const char *domain)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uint32_t i;
	uint32_t num_users = 0;
	const char **users = NULL;

	/* Send request to winbind daemon */

	/* '.' is the special sign for our own domain */
	if (domain && strcmp(domain, ".") == 0) {
		domain = get_winbind_domain();
	}

	wbc_status = wbcListUsers(domain, &num_users, &users);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		return false;
	}

	for (i=0; i < num_users; i++) {
		d_printf("%s\n", users[i]);
	}

	wbcFreeMemory(users);

	return true;
}

/* Print domain groups */

static bool print_domain_groups(const char *domain)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	uint32_t i;
	uint32_t num_groups = 0;
	const char **groups = NULL;

	/* Send request to winbind daemon */

	/* '.' is the special sign for our own domain */
	if (domain && strcmp(domain, ".") == 0) {
		domain = get_winbind_domain();
	}

	wbc_status = wbcListGroups(domain, &num_groups, &groups);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		d_fprintf(stderr, "failed to call wbcListGroups: %s\n",
			  wbcErrorString(wbc_status));
		return false;
	}

	for (i=0; i < num_groups; i++) {
		d_printf("%s\n", groups[i]);
	}

	wbcFreeMemory(groups);

	return true;
}

/* Set the authorised user for winbindd access in secrets.tdb */

static bool wbinfo_set_auth_user(char *username)
{
	d_fprintf(stderr, "This functionality was moved to the 'net' utility.\n"
			  "See 'net help setauthuser' for details.\n");
	return false;
}

static void wbinfo_get_auth_user(void)
{
	d_fprintf(stderr, "This functionality was moved to the 'net' utility.\n"
			  "See 'net help getauthuser' for details.\n");
}

static bool wbinfo_ping(void)
{
	wbcErr wbc_status;

	wbc_status = wbcPing();

	/* Display response */

	d_printf("Ping to winbindd %s\n",
		 WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	return WBC_ERROR_IS_OK(wbc_status);
}

static bool wbinfo_change_user_password(const char *username)
{
	wbcErr wbc_status;
	char *old_password = NULL;
	char *new_password = NULL;
	TALLOC_CTX *frame = talloc_tos();

	old_password = wbinfo_prompt_pass(frame, "old", username);
	new_password = wbinfo_prompt_pass(frame, "new", username);

	wbc_status = wbcChangeUserPassword(username, old_password,new_password);

	/* Display response */

	d_printf("Password change for user %s %s\n", username,
		WBC_ERROR_IS_OK(wbc_status) ? "succeeded" : "failed");

	return WBC_ERROR_IS_OK(wbc_status);
}

/* Main program */

enum {
	OPT_SET_AUTH_USER = 1000,
	OPT_GET_AUTH_USER,
	OPT_DOMAIN_NAME,
	OPT_SEQUENCE,
	OPT_GETDCNAME,
	OPT_DSGETDCNAME,
	OPT_DC_INFO,
	OPT_USERDOMGROUPS,
	OPT_SIDALIASES,
	OPT_USERSIDS,
	OPT_LOOKUP_SIDS,
	OPT_ALLOCATE_UID,
	OPT_ALLOCATE_GID,
	OPT_SET_UID_MAPPING,
	OPT_SET_GID_MAPPING,
	OPT_REMOVE_UID_MAPPING,
	OPT_REMOVE_GID_MAPPING,
	OPT_SIDS_TO_XIDS,
	OPT_SEPARATOR,
	OPT_LIST_ALL_DOMAINS,
	OPT_LIST_OWN_DOMAIN,
	OPT_UID_INFO,
	OPT_USER_SIDINFO,
	OPT_GROUP_INFO,
	OPT_GID_INFO,
	OPT_VERBOSE,
	OPT_ONLINESTATUS,
	OPT_CHANGE_USER_PASSWORD,
	OPT_CCACHE_SAVE,
	OPT_SID_TO_FULLNAME,
	OPT_NTLMV2,
	OPT_PAM_LOGON,
	OPT_LOGOFF,
	OPT_LOGOFF_USER,
	OPT_LOGOFF_UID,
	OPT_LANMAN
};

int main(int argc, char **argv, char **envp)
{
	int opt;
	TALLOC_CTX *frame = talloc_stackframe();
	poptContext pc;
	static char *string_arg;
	char *string_subarg = NULL;
	static char *opt_domain_name;
	static int int_arg;
	int int_subarg = -1;
	int result = 1;
	bool verbose = false;
	bool use_ntlmv2 = false;
	bool use_lanman = false;
	char *logoff_user = getenv("USER");
	int logoff_uid = geteuid();

	struct poptOption long_options[] = {
		POPT_AUTOHELP

		/* longName, shortName, argInfo, argPtr, value, descrip,
		   argDesc */

		{ "domain-users", 'u', POPT_ARG_NONE, 0, 'u', "Lists all domain users", "domain"},
		{ "domain-groups", 'g', POPT_ARG_NONE, 0, 'g', "Lists all domain groups", "domain" },
		{ "WINS-by-name", 'N', POPT_ARG_STRING, &string_arg, 'N', "Converts NetBIOS name to IP", "NETBIOS-NAME" },
		{ "WINS-by-ip", 'I', POPT_ARG_STRING, &string_arg, 'I', "Converts IP address to NetBIOS name", "IP" },
		{ "name-to-sid", 'n', POPT_ARG_STRING, &string_arg, 'n', "Converts name to sid", "NAME" },
		{ "sid-to-name", 's', POPT_ARG_STRING, &string_arg, 's', "Converts sid to name", "SID" },
		{ "sid-to-fullname", 0, POPT_ARG_STRING, &string_arg,
		  OPT_SID_TO_FULLNAME, "Converts sid to fullname", "SID" },
		{ "lookup-rids", 'R', POPT_ARG_STRING, &string_arg, 'R', "Converts RIDs to names", "RIDs" },
		{ "lookup-sids", 0, POPT_ARG_STRING, &string_arg,
		  OPT_LOOKUP_SIDS, "Converts SIDs to types and names",
		  "Sid-List"},
		{ "uid-to-sid", 'U', POPT_ARG_INT, &int_arg, 'U', "Converts uid to sid" , "UID" },
		{ "gid-to-sid", 'G', POPT_ARG_INT, &int_arg, 'G', "Converts gid to sid", "GID" },
		{ "sid-to-uid", 'S', POPT_ARG_STRING, &string_arg, 'S', "Converts sid to uid", "SID" },
		{ "sid-to-gid", 'Y', POPT_ARG_STRING, &string_arg, 'Y', "Converts sid to gid", "SID" },
		{ "allocate-uid", 0, POPT_ARG_NONE, 0, OPT_ALLOCATE_UID,
		  "Get a new UID out of idmap" },
		{ "allocate-gid", 0, POPT_ARG_NONE, 0, OPT_ALLOCATE_GID,
		  "Get a new GID out of idmap" },
		{ "set-uid-mapping", 0, POPT_ARG_STRING, &string_arg, OPT_SET_UID_MAPPING, "Create or modify uid to sid mapping in idmap", "UID,SID" },
		{ "set-gid-mapping", 0, POPT_ARG_STRING, &string_arg, OPT_SET_GID_MAPPING, "Create or modify gid to sid mapping in idmap", "GID,SID" },
		{ "remove-uid-mapping", 0, POPT_ARG_STRING, &string_arg, OPT_REMOVE_UID_MAPPING, "Remove uid to sid mapping in idmap", "UID,SID" },
		{ "remove-gid-mapping", 0, POPT_ARG_STRING, &string_arg, OPT_REMOVE_GID_MAPPING, "Remove gid to sid mapping in idmap", "GID,SID" },
		{ "sids-to-unix-ids", 0, POPT_ARG_STRING, &string_arg,
		  OPT_SIDS_TO_XIDS, "Translate SIDs to Unix IDs", "Sid-List" },
		{ "check-secret", 't', POPT_ARG_NONE, 0, 't', "Check shared secret" },
		{ "change-secret", 'c', POPT_ARG_NONE, 0, 'c', "Change shared secret" },
		{ "ping-dc", 'P', POPT_ARG_NONE, 0, 'P',
		  "Check the NETLOGON connection" },
		{ "trusted-domains", 'm', POPT_ARG_NONE, 0, 'm', "List trusted domains" },
		{ "all-domains", 0, POPT_ARG_NONE, 0, OPT_LIST_ALL_DOMAINS, "List all domains (trusted and own domain)" },
		{ "own-domain", 0, POPT_ARG_NONE, 0, OPT_LIST_OWN_DOMAIN, "List own domain" },
		{ "sequence", 0, POPT_ARG_NONE, 0, OPT_SEQUENCE, "Deprecated command, see --online-status" },
		{ "online-status", 0, POPT_ARG_NONE, 0, OPT_ONLINESTATUS, "Show whether domains are marked as online or offline"},
		{ "domain-info", 'D', POPT_ARG_STRING, &string_arg, 'D', "Show most of the info we have about the domain" },
		{ "user-info", 'i', POPT_ARG_STRING, &string_arg, 'i', "Get user info", "USER" },
		{ "uid-info", 0, POPT_ARG_INT, &int_arg, OPT_UID_INFO, "Get user info from uid", "UID" },
		{ "group-info", 0, POPT_ARG_STRING, &string_arg, OPT_GROUP_INFO, "Get group info", "GROUP" },
		{ "user-sidinfo", 0, POPT_ARG_STRING, &string_arg, OPT_USER_SIDINFO, "Get user info from sid", "SID" },
		{ "gid-info", 0, POPT_ARG_INT, &int_arg, OPT_GID_INFO, "Get group info from gid", "GID" },
		{ "user-groups", 'r', POPT_ARG_STRING, &string_arg, 'r', "Get user groups", "USER" },
		{ "user-domgroups", 0, POPT_ARG_STRING, &string_arg,
		  OPT_USERDOMGROUPS, "Get user domain groups", "SID" },
		{ "sid-aliases", 0, POPT_ARG_STRING, &string_arg, OPT_SIDALIASES, "Get sid aliases", "SID" },
		{ "user-sids", 0, POPT_ARG_STRING, &string_arg, OPT_USERSIDS, "Get user group sids for user SID", "SID" },
		{ "authenticate", 'a', POPT_ARG_STRING, &string_arg, 'a', "authenticate user", "user%password" },
		{ "pam-logon", 0, POPT_ARG_STRING, &string_arg, OPT_PAM_LOGON,
		  "do a pam logon equivalent", "user%password" },
		{ "logoff", 0, POPT_ARG_NONE, NULL, OPT_LOGOFF,
		  "log off user", "uid" },
		{ "logoff-user", 0, POPT_ARG_STRING, &logoff_user,
		  OPT_LOGOFF_USER, "username to log off" },
		{ "logoff-uid", 0, POPT_ARG_INT, &logoff_uid,
		  OPT_LOGOFF_UID, "uid to log off" },
		{ "set-auth-user", 0, POPT_ARG_STRING, &string_arg, OPT_SET_AUTH_USER, "Store user and password used by winbindd (root only)", "user%password" },
		{ "ccache-save", 0, POPT_ARG_STRING, &string_arg,
		  OPT_CCACHE_SAVE, "Store user and password for ccache "
		  "operation", "user%password" },
		{ "getdcname", 0, POPT_ARG_STRING, &string_arg, OPT_GETDCNAME,
		  "Get a DC name for a foreign domain", "domainname" },
		{ "dsgetdcname", 0, POPT_ARG_STRING, &string_arg, OPT_DSGETDCNAME, "Find a DC for a domain", "domainname" },
		{ "dc-info", 0, POPT_ARG_STRING, &string_arg, OPT_DC_INFO,
		  "Find the currently known DCs", "domainname" },
		{ "get-auth-user", 0, POPT_ARG_NONE, NULL, OPT_GET_AUTH_USER, "Retrieve user and password used by winbindd (root only)", NULL },
		{ "ping", 'p', POPT_ARG_NONE, 0, 'p', "Ping winbindd to see if it is alive" },
		{ "domain", 0, POPT_ARG_STRING, &opt_domain_name, OPT_DOMAIN_NAME, "Define to the domain to restrict operation", "domain" },
#ifdef WITH_FAKE_KASERVER
		{ "klog", 'k', POPT_ARG_STRING, &string_arg, 'k', "set an AFS token from winbind", "user%password" },
#endif
#ifdef HAVE_KRB5
		{ "krb5auth", 'K', POPT_ARG_STRING, &string_arg, 'K', "authenticate user using Kerberos", "user%password" },
			/* destroys wbinfo --help output */
			/* "user%password,DOM\\user%password,user@EXAMPLE.COM,EXAMPLE.COM\\user%password" }, */
#endif
		{ "separator", 0, POPT_ARG_NONE, 0, OPT_SEPARATOR, "Get the active winbind separator", NULL },
		{ "verbose", 0, POPT_ARG_NONE, 0, OPT_VERBOSE, "Print additional information per command", NULL },
		{ "change-user-password", 0, POPT_ARG_STRING, &string_arg, OPT_CHANGE_USER_PASSWORD, "Change the password for a user", NULL },
		{ "ntlmv2", 0, POPT_ARG_NONE, 0, OPT_NTLMV2, "Use NTLMv2 cryptography for user authentication", NULL},
		{ "lanman", 0, POPT_ARG_NONE, 0, OPT_LANMAN, "Use lanman cryptography for user authentication", NULL},
		POPT_COMMON_VERSION
		POPT_TABLEEND
	};

	/* Samba client initialisation */
	load_case_tables();


	/* Parse options */

	pc = poptGetContext("wbinfo", argc, (const char **)argv,
			    long_options, 0);

	/* Parse command line options */

	if (argc == 1) {
		poptPrintHelp(pc, stderr, 0);
		return 1;
	}

	while((opt = poptGetNextOpt(pc)) != -1) {
		/* get the generic configuration parameters like --domain */
		switch (opt) {
		case OPT_VERBOSE:
			verbose = true;
			break;
		case OPT_NTLMV2:
			use_ntlmv2 = true;
			break;
		case OPT_LANMAN:
			use_lanman = true;
			break;
		}
	}

	poptFreeContext(pc);

	pc = poptGetContext(NULL, argc, (const char **)argv, long_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'u':
			if (!print_domain_users(opt_domain_name)) {
				d_fprintf(stderr,
					  "Error looking up domain users\n");
				goto done;
			}
			break;
		case 'g':
			if (!print_domain_groups(opt_domain_name)) {
				d_fprintf(stderr,
					  "Error looking up domain groups\n");
				goto done;
			}
			break;
		case 's':
			if (!wbinfo_lookupsid(string_arg)) {
				d_fprintf(stderr,
					  "Could not lookup sid %s\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_SID_TO_FULLNAME:
			if (!wbinfo_lookupsid_fullname(string_arg)) {
				d_fprintf(stderr, "Could not lookup sid %s\n",
					  string_arg);
				goto done;
			}
			break;
		case 'R':
			if (!wbinfo_lookuprids(opt_domain_name, string_arg)) {
				d_fprintf(stderr, "Could not lookup RIDs %s\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_LOOKUP_SIDS:
			if (!wbinfo_lookup_sids(string_arg)) {
				d_fprintf(stderr, "Could not lookup SIDs %s\n",
					  string_arg);
				goto done;
			}
			break;
		case 'n':
			if (!wbinfo_lookupname(string_arg)) {
				d_fprintf(stderr, "Could not lookup name %s\n",
					  string_arg);
				goto done;
			}
			break;
		case 'N':
			if (!wbinfo_wins_byname(string_arg)) {
				d_fprintf(stderr,
					  "Could not lookup WINS by name %s\n",
					  string_arg);
				goto done;
			}
			break;
		case 'I':
			if (!wbinfo_wins_byip(string_arg)) {
				d_fprintf(stderr,
					  "Could not lookup WINS by IP %s\n",
					  string_arg);
				goto done;
			}
			break;
		case 'U':
			if (!wbinfo_uid_to_sid(int_arg)) {
				d_fprintf(stderr,
					  "Could not convert uid %d to sid\n",
					  int_arg);
				goto done;
			}
			break;
		case 'G':
			if (!wbinfo_gid_to_sid(int_arg)) {
				d_fprintf(stderr,
					  "Could not convert gid %d to sid\n",
					  int_arg);
				goto done;
			}
			break;
		case 'S':
			if (!wbinfo_sid_to_uid(string_arg)) {
				d_fprintf(stderr,
					  "Could not convert sid %s to uid\n",
					  string_arg);
				goto done;
			}
			break;
		case 'Y':
			if (!wbinfo_sid_to_gid(string_arg)) {
				d_fprintf(stderr,
					  "Could not convert sid %s to gid\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_ALLOCATE_UID:
			if (!wbinfo_allocate_uid()) {
				d_fprintf(stderr, "Could not allocate a uid\n");
				goto done;
			}
			break;
		case OPT_ALLOCATE_GID:
			if (!wbinfo_allocate_gid()) {
				d_fprintf(stderr, "Could not allocate a gid\n");
				goto done;
			}
			break;
		case OPT_SET_UID_MAPPING:
			if (!parse_mapping_arg(string_arg, &int_subarg,
				&string_subarg) ||
			    !wbinfo_set_uid_mapping(int_subarg, string_subarg))
			{
				d_fprintf(stderr, "Could not create or modify "
					  "uid to sid mapping\n");
				goto done;
			}
			break;
		case OPT_SET_GID_MAPPING:
			if (!parse_mapping_arg(string_arg, &int_subarg,
			        &string_subarg) ||
			    !wbinfo_set_gid_mapping(int_subarg, string_subarg))
			{
				d_fprintf(stderr, "Could not create or modify "
					  "gid to sid mapping\n");
				goto done;
			}
			break;
		case OPT_REMOVE_UID_MAPPING:
			if (!parse_mapping_arg(string_arg, &int_subarg,
				&string_subarg) ||
			    !wbinfo_remove_uid_mapping(int_subarg,
				string_subarg))
			{
				d_fprintf(stderr, "Could not remove uid to sid "
				    "mapping\n");
				goto done;
			}
			break;
		case OPT_REMOVE_GID_MAPPING:
			if (!parse_mapping_arg(string_arg, &int_subarg,
			        &string_subarg) ||
			    !wbinfo_remove_gid_mapping(int_subarg,
			        string_subarg))
			{
				d_fprintf(stderr, "Could not remove gid to sid "
				    "mapping\n");
				goto done;
			}
			break;
		case OPT_SIDS_TO_XIDS:
			if (!wbinfo_sids_to_unix_ids(string_arg)) {
				d_fprintf(stderr, "wbinfo_sids_to_unix_ids "
					  "failed\n");
				goto done;
			}
			break;
		case 't':
			if (!wbinfo_check_secret(opt_domain_name)) {
				d_fprintf(stderr, "Could not check secret\n");
				goto done;
			}
			break;
		case 'c':
			if (!wbinfo_change_secret(opt_domain_name)) {
				d_fprintf(stderr, "Could not change secret\n");
				goto done;
			}
			break;
		case 'P':
			if (!wbinfo_ping_dc()) {
				d_fprintf(stderr, "Could not ping our DC\n");
				goto done;
			}
			break;
		case 'm':
			if (!wbinfo_list_domains(false, verbose)) {
				d_fprintf(stderr,
					  "Could not list trusted domains\n");
				goto done;
			}
			break;
		case OPT_SEQUENCE:
			if (!wbinfo_show_sequence(opt_domain_name)) {
				d_fprintf(stderr,
					  "Could not show sequence numbers\n");
				goto done;
			}
			break;
		case OPT_ONLINESTATUS:
			if (!wbinfo_show_onlinestatus(opt_domain_name)) {
				d_fprintf(stderr,
					  "Could not show online-status\n");
				goto done;
			}
			break;
		case 'D':
			if (!wbinfo_domain_info(string_arg)) {
				d_fprintf(stderr,
					  "Could not get domain info\n");
				goto done;
			}
			break;
		case 'i':
			if (!wbinfo_get_userinfo(string_arg)) {
				d_fprintf(stderr,
					  "Could not get info for user %s\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_USER_SIDINFO:
			if ( !wbinfo_get_user_sidinfo(string_arg)) {
				d_fprintf(stderr,
					  "Could not get info for user "
					  "sid %s\n", string_arg);
				goto done;
			}
			break;
		case OPT_UID_INFO:
			if ( !wbinfo_get_uidinfo(int_arg)) {
				d_fprintf(stderr, "Could not get info for uid "
						"%d\n", int_arg);
				goto done;
			}
			break;
		case OPT_GROUP_INFO:
			if ( !wbinfo_get_groupinfo(string_arg)) {
				d_fprintf(stderr, "Could not get info for "
					  "group %s\n", string_arg);
				goto done;
			}
			break;
		case OPT_GID_INFO:
			if ( !wbinfo_get_gidinfo(int_arg)) {
				d_fprintf(stderr, "Could not get info for gid "
						"%d\n", int_arg);
				goto done;
			}
			break;
		case 'r':
			if (!wbinfo_get_usergroups(string_arg)) {
				d_fprintf(stderr,
					  "Could not get groups for user %s\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_USERSIDS:
			if (!wbinfo_get_usersids(string_arg)) {
				d_fprintf(stderr, "Could not get group SIDs "
					  "for user SID %s\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_USERDOMGROUPS:
			if (!wbinfo_get_userdomgroups(string_arg)) {
				d_fprintf(stderr, "Could not get user's domain "
					 "groups for user SID %s\n",
					 string_arg);
				goto done;
			}
			break;
		case OPT_SIDALIASES:
			if (!wbinfo_get_sidaliases(opt_domain_name,
						   string_arg)) {
				d_fprintf(stderr, "Could not get sid aliases "
					 "for user SID %s\n", string_arg);
				goto done;
			}
			break;
		case 'a': {
				bool got_error = false;

				if (!wbinfo_auth(string_arg)) {
					d_fprintf(stderr,
						  "Could not authenticate user "
						  "%s with plaintext "
						  "password\n", string_arg);
					got_error = true;
				}

				if (!wbinfo_auth_crap(string_arg, use_ntlmv2,
						      use_lanman)) {
					d_fprintf(stderr,
						"Could not authenticate user "
						"%s with challenge/response\n",
						string_arg);
					got_error = true;
				}

				if (got_error)
					goto done;
				break;
			}
		case OPT_PAM_LOGON:
			if (!wbinfo_pam_logon(string_arg)) {
				d_fprintf(stderr, "pam_logon failed for %s\n",
					  string_arg);
				goto done;
			}
			break;
		case OPT_LOGOFF:
		{
			wbcErr wbc_status;

			wbc_status = wbcLogoffUser(logoff_user, logoff_uid,
						   "");
			d_printf("Logoff %s (%d): %s\n", logoff_user,
				 logoff_uid, wbcErrorString(wbc_status));
			break;
		}
		case 'K': {
				uint32_t flags = WBFLAG_PAM_KRB5 |
						 WBFLAG_PAM_CACHED_LOGIN |
						WBFLAG_PAM_FALLBACK_AFTER_KRB5 |
						 WBFLAG_PAM_INFO3_TEXT |
						 WBFLAG_PAM_CONTACT_TRUSTDOM;

				if (!wbinfo_auth_krb5(string_arg, "FILE",
						      flags)) {
					d_fprintf(stderr,
						"Could not authenticate user "
						"[%s] with Kerberos "
						"(ccache: %s)\n", string_arg,
						"FILE");
					goto done;
				}
				break;
			}
		case 'k':
			if (!wbinfo_klog(string_arg)) {
				d_fprintf(stderr, "Could not klog user\n");
				goto done;
			}
			break;
		case 'p':
			if (!wbinfo_ping()) {
				d_fprintf(stderr, "could not ping winbindd!\n");
				goto done;
			}
			break;
		case OPT_SET_AUTH_USER:
			if (!wbinfo_set_auth_user(string_arg)) {
				goto done;
			}
			break;
		case OPT_GET_AUTH_USER:
			wbinfo_get_auth_user();
			goto done;
			break;
		case OPT_CCACHE_SAVE:
			if (!wbinfo_ccache_save(string_arg)) {
				goto done;
			}
			break;
		case OPT_GETDCNAME:
			if (!wbinfo_getdcname(string_arg)) {
				goto done;
			}
			break;
		case OPT_DSGETDCNAME:
			if (!wbinfo_dsgetdcname(string_arg, 0)) {
				goto done;
			}
			break;
		case OPT_DC_INFO:
			if (!wbinfo_dc_info(string_arg)) {
				goto done;
			}
			break;
		case OPT_SEPARATOR: {
			const char sep = winbind_separator();
			if ( !sep ) {
				goto done;
			}
			d_printf("%c\n", sep);
			break;
		}
		case OPT_LIST_ALL_DOMAINS:
			if (!wbinfo_list_domains(true, verbose)) {
				goto done;
			}
			break;
		case OPT_LIST_OWN_DOMAIN:
			if (!wbinfo_list_own_domain()) {
				goto done;
			}
			break;
		case OPT_CHANGE_USER_PASSWORD:
			if (!wbinfo_change_user_password(string_arg)) {
				d_fprintf(stderr,
					"Could not change user password "
					 "for user %s\n", string_arg);
				goto done;
			}
			break;

		/* generic configuration options */
		case OPT_DOMAIN_NAME:
		case OPT_VERBOSE:
		case OPT_NTLMV2:
		case OPT_LANMAN:
		case OPT_LOGOFF_USER:
		case OPT_LOGOFF_UID:
			break;
		default:
			d_fprintf(stderr, "Invalid option\n");
			poptPrintHelp(pc, stderr, 0);
			goto done;
		}
	}

	result = 0;

	/* Exit code */

 done:
	talloc_free(frame);

	poptFreeContext(pc);
	return result;
}
