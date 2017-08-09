/*
   Samba Unix/Linux SMB client library
   net lookup command
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "utils/net.h"
#include "libads/sitename_cache.h"
#include "libads/dns.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "smb_krb5.h"
#include "../libcli/security/security.h"
#include "passdb/lookup_sid.h"

int net_lookup_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
"  net lookup [host] HOSTNAME[#<type>]\n\tgives IP for a hostname\n\n"
"  net lookup ldap [domain]\n\tgives IP of domain's ldap server\n\n"
"  net lookup kdc [realm]\n\tgives IP of realm's kerberos KDC\n\n"
"  net lookup pdc [domain|realm]\n\tgives IP of realm's kerberos KDC\n\n"
"  net lookup dc [domain]\n\tgives IP of domains Domain Controllers\n\n"
"  net lookup master [domain|wg]\n\tgive IP of master browser\n\n"
"  net lookup name [name]\n\tLookup name's sid and type\n\n"
"  net lookup sid [sid]\n\tGive sid's name and type\n\n"
"  net lookup dsgetdcname [name] [flags] [sitename]\n\n"
));
	return -1;
}

/* lookup a hostname giving an IP */
static int net_lookup_host(struct net_context *c, int argc, const char **argv)
{
	struct sockaddr_storage ss;
	int name_type = 0x20;
	char addr[INET6_ADDRSTRLEN];
	const char *name = argv[0];
	char *p;

	if (argc == 0)
		return net_lookup_usage(c, argc, argv);

	p = strchr_m(name,'#');
	if (p) {
		*p = '\0';
		sscanf(++p,"%x",&name_type);
	}

	if (!resolve_name(name, &ss, name_type, false)) {
		/* we deliberately use DEBUG() here to send it to stderr
		   so scripts aren't mucked up */
		DEBUG(0,("Didn't find %s#%02x\n", name, name_type));
		return -1;
	}

	print_sockaddr(addr, sizeof(addr), &ss);
	d_printf("%s\n", addr);
	return 0;
}

#ifdef HAVE_ADS
static void print_ldap_srvlist(struct dns_rr_srv *dclist, int numdcs )
{
	struct sockaddr_storage ss;
	int i;

	for ( i=0; i<numdcs; i++ ) {
		if (resolve_name(dclist[i].hostname, &ss, 0x20, true) ) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr), &ss);
#ifdef HAVE_IPV6
			if (ss.ss_family == AF_INET6) {
				d_printf("[%s]:%d\n", addr, dclist[i].port);
			}
#endif
			if (ss.ss_family == AF_INET) {
				d_printf("%s:%d\n", addr, dclist[i].port);
			}
		}
	}
}
#endif

static int net_lookup_ldap(struct net_context *c, int argc, const char **argv)
{
#ifdef HAVE_ADS
	const char *domain;
	struct sockaddr_storage ss;
	struct dns_rr_srv *dcs = NULL;
	int numdcs = 0;
	char *sitename;
	TALLOC_CTX *ctx;
	NTSTATUS status;
	int ret;
	char h_name[MAX_DNS_NAME_LENGTH];

	if (argc > 0)
		domain = argv[0];
	else
		domain = c->opt_target_workgroup;

	sitename = sitename_fetch(domain);

	if ( (ctx = talloc_init("net_lookup_ldap")) == NULL ) {
		d_fprintf(stderr,"net_lookup_ldap: talloc_init() %s!\n",
			  _("failed"));
		SAFE_FREE(sitename);
		return -1;
	}

	DEBUG(9, ("Lookup up ldap for domain %s\n", domain));

	status = ads_dns_query_dcs( ctx, domain, sitename, &dcs, &numdcs );
	if ( NT_STATUS_IS_OK(status) && numdcs ) {
		print_ldap_srvlist(dcs, numdcs);
		TALLOC_FREE( ctx );
		SAFE_FREE(sitename);
		return 0;
	}

     	DEBUG(9, ("Looking up PDC for domain %s\n", domain));
	if (!get_pdc_ip(domain, &ss)) {
		TALLOC_FREE( ctx );
		SAFE_FREE(sitename);
		return -1;
	}

	ret = sys_getnameinfo((struct sockaddr *)&ss,
			sizeof(struct sockaddr_storage),
			h_name, sizeof(h_name),
			NULL, 0,
			NI_NAMEREQD);

	if (ret) {
		TALLOC_FREE( ctx );
		SAFE_FREE(sitename);
		return -1;
	}

	DEBUG(9, ("Found PDC with DNS name %s\n", h_name));
	domain = strchr(h_name, '.');
	if (!domain) {
		TALLOC_FREE( ctx );
		SAFE_FREE(sitename);
		return -1;
	}
	domain++;

	DEBUG(9, ("Looking up ldap for domain %s\n", domain));

	status = ads_dns_query_dcs( ctx, domain, sitename, &dcs, &numdcs );
	if ( NT_STATUS_IS_OK(status) && numdcs ) {
		print_ldap_srvlist(dcs, numdcs);
		TALLOC_FREE( ctx );
		SAFE_FREE(sitename);
		return 0;
	}

	TALLOC_FREE( ctx );
	SAFE_FREE(sitename);

	return -1;
#endif
	DEBUG(1,("No ADS support\n"));
	return -1;
}

static int net_lookup_dc(struct net_context *c, int argc, const char **argv)
{
	struct ip_service *ip_list;
	struct sockaddr_storage ss;
	char *pdc_str = NULL;
	const char *domain = NULL;
	char *sitename = NULL;
	int count, i;
	char addr[INET6_ADDRSTRLEN];
	bool sec_ads = (lp_security() == SEC_ADS);

	if (sec_ads) {
		domain = lp_realm();
	} else {
		domain = c->opt_target_workgroup;
	}

	if (argc > 0)
		domain=argv[0];

	/* first get PDC */
	if (!get_pdc_ip(domain, &ss))
		return -1;

	print_sockaddr(addr, sizeof(addr), &ss);
	if (asprintf(&pdc_str, "%s", addr) == -1) {
		return -1;
	}
	d_printf("%s\n", pdc_str);

	sitename = sitename_fetch(domain);
	if (!NT_STATUS_IS_OK(get_sorted_dc_list(domain, sitename,
					&ip_list, &count, sec_ads))) {
		SAFE_FREE(pdc_str);
		SAFE_FREE(sitename);
		return 0;
	}
	SAFE_FREE(sitename);
	for (i=0;i<count;i++) {
		print_sockaddr(addr, sizeof(addr), &ip_list[i].ss);
		if (!strequal(pdc_str, addr))
			d_printf("%s\n", addr);
	}
	SAFE_FREE(pdc_str);
	return 0;
}

static int net_lookup_pdc(struct net_context *c, int argc, const char **argv)
{
	struct sockaddr_storage ss;
	char *pdc_str = NULL;
	const char *domain;
	char addr[INET6_ADDRSTRLEN];

	if (lp_security() == SEC_ADS) {
		domain = lp_realm();
	} else {
		domain = c->opt_target_workgroup;
	}

	if (argc > 0)
		domain=argv[0];

	/* first get PDC */
	if (!get_pdc_ip(domain, &ss))
		return -1;

	print_sockaddr(addr, sizeof(addr), &ss);
	if (asprintf(&pdc_str, "%s", addr) == -1) {
		return -1;
	}
	d_printf("%s\n", pdc_str);
	SAFE_FREE(pdc_str);
	return 0;
}


static int net_lookup_master(struct net_context *c, int argc, const char **argv)
{
	struct sockaddr_storage master_ss;
	const char *domain = c->opt_target_workgroup;
	char addr[INET6_ADDRSTRLEN];

	if (argc > 0)
		domain=argv[0];

	if (!find_master_ip(domain, &master_ss))
		return -1;
	print_sockaddr(addr, sizeof(addr), &master_ss);
	d_printf("%s\n", addr);
	return 0;
}

static int net_lookup_kdc(struct net_context *c, int argc, const char **argv)
{
#ifdef HAVE_KRB5
	krb5_error_code rc;
	krb5_context ctx;
	struct ip_service *kdcs;
	const char *realm;
	int num_kdcs = 0;
	int i;
	NTSTATUS status;

	initialize_krb5_error_table();
	rc = krb5_init_context(&ctx);
	if (rc) {
		DEBUG(1,("krb5_init_context failed (%s)\n",
			 error_message(rc)));
		return -1;
	}

	if (argc > 0) {
                realm = argv[0];
	} else if (lp_realm() && *lp_realm()) {
		realm = lp_realm();
	} else {
		char **realms;

		rc = krb5_get_host_realm(ctx, NULL, &realms);
		if (rc) {
			DEBUG(1,("krb5_gethost_realm failed (%s)\n",
				 error_message(rc)));
			return -1;
		}
		realm = (const char *) *realms;
	}

	status = get_kdc_list(realm, NULL, &kdcs, &num_kdcs);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("get_kdc_list failed (%s)\n", nt_errstr(status)));
		return -1;
	}

	for (i = 0; i < num_kdcs; i++) {
		char addr[INET6_ADDRSTRLEN];

		print_sockaddr(addr, sizeof(addr), &kdcs[i].ss);

		d_printf("%s:%hd\n", addr, kdcs[i].port);
	}

	return 0;
#endif
	DEBUG(1, ("No kerberos support\n"));
	return -1;
}

static int net_lookup_name(struct net_context *c, int argc, const char **argv)
{
	const char *dom, *name;
	struct dom_sid sid;
	enum lsa_SidType type;

	if (argc != 1) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net lookup name <name>\n"));
		return -1;
	}

	if (!lookup_name(talloc_tos(), argv[0], LOOKUP_NAME_ALL,
			 &dom, &name, &sid, &type)) {
		d_printf(_("Could not lookup name %s\n"), argv[0]);
		return -1;
	}

	d_printf("%s %d (%s) %s\\%s\n", sid_string_tos(&sid),
		 type, sid_type_lookup(type), dom, name);
	return 0;
}

static int net_lookup_sid(struct net_context *c, int argc, const char **argv)
{
	const char *dom, *name;
	struct dom_sid sid;
	enum lsa_SidType type;

	if (argc != 1) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net lookup sid <sid>\n"));
		return -1;
	}

	if (!string_to_sid(&sid, argv[0])) {
		d_printf(_("Could not convert %s to SID\n"), argv[0]);
		return -1;
	}

	if (!lookup_sid(talloc_tos(), &sid,
			&dom, &name, &type)) {
		d_printf(_("Could not lookup name %s\n"), argv[0]);
		return -1;
	}

	d_printf("%s %d (%s) %s\\%s\n", sid_string_tos(&sid),
		 type, sid_type_lookup(type), dom, name);
	return 0;
}

static int net_lookup_dsgetdcname(struct net_context *c, int argc, const char **argv)
{
	NTSTATUS status;
	const char *domain_name = NULL;
	const char *site_name = NULL;
	uint32_t flags = 0;
	struct netr_DsRGetDCNameInfo *info = NULL;
	TALLOC_CTX *mem_ctx;
	char *s = NULL;

	if (argc < 1 || argc > 3) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net lookup dsgetdcname "
			   "<name> <flags> <sitename>\n"));
		return -1;
	}

	mem_ctx = talloc_init("net_lookup_dsgetdcname");
	if (!mem_ctx) {
		return -1;
	}

	domain_name = argv[0];

	if (argc >= 2)
		sscanf(argv[1], "%x", &flags);

	if (!flags) {
		flags |= DS_DIRECTORY_SERVICE_REQUIRED;
	}

	if (argc == 3) {
		site_name = argv[2];
	}

        if (!c->msg_ctx) {
		d_fprintf(stderr, _("Could not initialise message context. "
			"Try running as root\n"));
		return -1;
        }

	status = dsgetdcname(mem_ctx, c->msg_ctx, domain_name, NULL, site_name,
			     flags, &info);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("failed with: %s\n"), nt_errstr(status));
		TALLOC_FREE(mem_ctx);
		return -1;
	}

	s = NDR_PRINT_STRUCT_STRING(mem_ctx, netr_DsRGetDCNameInfo, info);
	printf("%s\n", s);
	TALLOC_FREE(s);

	TALLOC_FREE(mem_ctx);
	return 0;
}


/* lookup hosts or IP addresses using internal samba lookup fns */
int net_lookup(struct net_context *c, int argc, const char **argv)
{
	int i;

	struct functable table[] = {
		{"HOST", net_lookup_host},
		{"LDAP", net_lookup_ldap},
		{"DC", net_lookup_dc},
		{"PDC", net_lookup_pdc},
		{"MASTER", net_lookup_master},
		{"KDC", net_lookup_kdc},
		{"NAME", net_lookup_name},
		{"SID", net_lookup_sid},
		{"DSGETDCNAME", net_lookup_dsgetdcname},
		{NULL, NULL}
	};

	if (argc < 1) {
		d_printf(_("\nUsage: \n"));
		return net_lookup_usage(c, argc, argv);
	}
	for (i=0; table[i].funcname; i++) {
		if (StrCaseCmp(argv[0], table[i].funcname) == 0)
			return table[i].fn(c, argc-1, argv+1);
	}

	/* Default to lookup a hostname so 'net lookup foo#1b' can be
	   used instead of 'net lookup host foo#1b'.  The host syntax
	   is a bit confusing as non #00 names can't really be
	   considered hosts as such. */

	return net_lookup_host(c, argc, argv);
}
