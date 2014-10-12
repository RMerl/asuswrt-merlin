/* 
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Remus Koos 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   Copyright (C) Guenther Deschner 2005
   Copyright (C) Gerald Carter 2006

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
#include "ads.h"
#include "libads/sitename_cache.h"
#include "libads/cldap.h"
#include "libads/dns.h"
#include "../libds/common/flags.h"
#include "smbldap.h"
#include "../libcli/security/security.h"

#ifdef HAVE_LDAP

/**
 * @file ldap.c
 * @brief basic ldap client-side routines for ads server communications
 *
 * The routines contained here should do the necessary ldap calls for
 * ads setups.
 * 
 * Important note: attribute names passed into ads_ routines must
 * already be in UTF-8 format.  We do not convert them because in almost
 * all cases, they are just ascii (which is represented with the same
 * codepoints in UTF-8).  This may have to change at some point
 **/


#define LDAP_SERVER_TREE_DELETE_OID	"1.2.840.113556.1.4.805"

static SIG_ATOMIC_T gotalarm;

/***************************************************************
 Signal function to tell us we timed out.
****************************************************************/

static void gotalarm_sig(int signum)
{
	gotalarm = 1;
}

 LDAP *ldap_open_with_timeout(const char *server, int port, unsigned int to)
{
	LDAP *ldp = NULL;


	DEBUG(10, ("Opening connection to LDAP server '%s:%d', timeout "
		   "%u seconds\n", server, port, to));

	/* Setup timeout */
	gotalarm = 0;
	CatchSignal(SIGALRM, gotalarm_sig);
	alarm(to);
	/* End setup timeout. */

	ldp = ldap_open(server, port);

	if (ldp == NULL) {
		DEBUG(2,("Could not open connection to LDAP server %s:%d: %s\n",
			 server, port, strerror(errno)));
	} else {
		DEBUG(10, ("Connected to LDAP server '%s:%d'\n", server, port));
	}

	/* Teardown timeout. */
	CatchSignal(SIGALRM, SIG_IGN);
	alarm(0);

	return ldp;
}

static int ldap_search_with_timeout(LDAP *ld,
				    LDAP_CONST char *base,
				    int scope,
				    LDAP_CONST char *filter,
				    char **attrs,
				    int attrsonly,
				    LDAPControl **sctrls,
				    LDAPControl **cctrls,
				    int sizelimit,
				    LDAPMessage **res )
{
	struct timeval timeout;
	int result;

	/* Setup timeout for the ldap_search_ext_s call - local and remote. */
	timeout.tv_sec = lp_ldap_timeout();
	timeout.tv_usec = 0;

	/* Setup alarm timeout.... Do we need both of these ? JRA. */
	gotalarm = 0;
	CatchSignal(SIGALRM, gotalarm_sig);
	alarm(lp_ldap_timeout());
	/* End setup timeout. */

	result = ldap_search_ext_s(ld, base, scope, filter, attrs,
				   attrsonly, sctrls, cctrls, &timeout,
				   sizelimit, res);

	/* Teardown timeout. */
	CatchSignal(SIGALRM, SIG_IGN);
	alarm(0);

	if (gotalarm != 0)
		return LDAP_TIMELIMIT_EXCEEDED;

	/*
	 * A bug in OpenLDAP means ldap_search_ext_s can return
	 * LDAP_SUCCESS but with a NULL res pointer. Cope with
	 * this. See bug #6279 for details. JRA.
	 */

	if (*res == NULL) {
		return LDAP_TIMELIMIT_EXCEEDED;
	}

	return result;
}

/**********************************************
 Do client and server sitename match ?
**********************************************/

bool ads_sitename_match(ADS_STRUCT *ads)
{
	if (ads->config.server_site_name == NULL &&
	    ads->config.client_site_name == NULL ) {
		DEBUG(10,("ads_sitename_match: both null\n"));
		return True;
	}
	if (ads->config.server_site_name &&
	    ads->config.client_site_name &&
	    strequal(ads->config.server_site_name,
		     ads->config.client_site_name)) {
		DEBUG(10,("ads_sitename_match: name %s match\n", ads->config.server_site_name));
		return True;
	}
	DEBUG(10,("ads_sitename_match: no match between server: %s and client: %s\n",
		ads->config.server_site_name ? ads->config.server_site_name : "NULL",
		ads->config.client_site_name ? ads->config.client_site_name : "NULL"));
	return False;
}

/**********************************************
 Is this the closest DC ?
**********************************************/

bool ads_closest_dc(ADS_STRUCT *ads)
{
	if (ads->config.flags & NBT_SERVER_CLOSEST) {
		DEBUG(10,("ads_closest_dc: NBT_SERVER_CLOSEST flag set\n"));
		return True;
	}

	/* not sure if this can ever happen */
	if (ads_sitename_match(ads)) {
		DEBUG(10,("ads_closest_dc: NBT_SERVER_CLOSEST flag not set but sites match\n"));
		return True;
	}

	if (ads->config.client_site_name == NULL) {
		DEBUG(10,("ads_closest_dc: client belongs to no site\n"));
		return True;
	}

	DEBUG(10,("ads_closest_dc: %s is not the closest DC\n", 
		ads->config.ldap_server_name));

	return False;
}


/*
  try a connection to a given ldap server, returning True and setting the servers IP
  in the ads struct if successful
 */
static bool ads_try_connect(ADS_STRUCT *ads, const char *server, bool gc)
{
	char *srv;
	struct NETLOGON_SAM_LOGON_RESPONSE_EX cldap_reply;
	TALLOC_CTX *frame = talloc_stackframe();
	bool ret = false;

	if (!server || !*server) {
		TALLOC_FREE(frame);
		return False;
	}

	if (!is_ipaddress(server)) {
		struct sockaddr_storage ss;
		char addr[INET6_ADDRSTRLEN];

		if (!resolve_name(server, &ss, 0x20, true)) {
			DEBUG(5,("ads_try_connect: unable to resolve name %s\n",
				server ));
			TALLOC_FREE(frame);
			return false;
		}
		print_sockaddr(addr, sizeof(addr), &ss);
		srv = talloc_strdup(frame, addr);
	} else {
		/* this copes with inet_ntoa brokenness */
		srv = talloc_strdup(frame, server);
	}

	if (!srv) {
		TALLOC_FREE(frame);
		return false;
	}

	DEBUG(5,("ads_try_connect: sending CLDAP request to %s (realm: %s)\n", 
		srv, ads->server.realm));

	ZERO_STRUCT( cldap_reply );

	if ( !ads_cldap_netlogon_5(frame, srv, ads->server.realm, &cldap_reply ) ) {
		DEBUG(3,("ads_try_connect: CLDAP request %s failed.\n", srv));
		ret = false;
		goto out;
	}

	/* Check the CLDAP reply flags */

	if ( !(cldap_reply.server_type & NBT_SERVER_LDAP) ) {
		DEBUG(1,("ads_try_connect: %s's CLDAP reply says it is not an LDAP server!\n",
			srv));
		ret = false;
		goto out;
	}

	/* Fill in the ads->config values */

	SAFE_FREE(ads->config.realm);
	SAFE_FREE(ads->config.bind_path);
	SAFE_FREE(ads->config.ldap_server_name);
	SAFE_FREE(ads->config.server_site_name);
	SAFE_FREE(ads->config.client_site_name);
	SAFE_FREE(ads->server.workgroup);

	ads->config.flags	       = cldap_reply.server_type;
	ads->config.ldap_server_name   = SMB_STRDUP(cldap_reply.pdc_dns_name);
	ads->config.realm              = SMB_STRDUP(cldap_reply.dns_domain);
	strupper_m(ads->config.realm);
	ads->config.bind_path          = ads_build_dn(ads->config.realm);
	if (*cldap_reply.server_site) {
		ads->config.server_site_name =
			SMB_STRDUP(cldap_reply.server_site);
	}
	if (*cldap_reply.client_site) {
		ads->config.client_site_name =
			SMB_STRDUP(cldap_reply.client_site);
	}
	ads->server.workgroup          = SMB_STRDUP(cldap_reply.domain_name);

	ads->ldap.port = gc ? LDAP_GC_PORT : LDAP_PORT;
	if (!interpret_string_addr(&ads->ldap.ss, srv, 0)) {
		DEBUG(1,("ads_try_connect: unable to convert %s "
			"to an address\n",
			srv));
		ret = false;
		goto out;
	}

	/* Store our site name. */
	sitename_store( cldap_reply.domain_name, cldap_reply.client_site);
	sitename_store( cldap_reply.dns_domain, cldap_reply.client_site);

	ret = true;

 out:

	TALLOC_FREE(frame);
	return ret;
}

/**********************************************************************
 Try to find an AD dc using our internal name resolution routines
 Try the realm first and then then workgroup name if netbios is not 
 disabled
**********************************************************************/

static NTSTATUS ads_find_dc(ADS_STRUCT *ads)
{
	const char *c_domain;
	const char *c_realm;
	int count, i=0;
	struct ip_service *ip_list;
	const char *realm;
	const char *domain;
	bool got_realm = False;
	bool use_own_domain = False;
	char *sitename;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;

	/* if the realm and workgroup are both empty, assume they are ours */

	/* realm */
	c_realm = ads->server.realm;

	if ( !c_realm || !*c_realm ) {
		/* special case where no realm and no workgroup means our own */
		if ( !ads->server.workgroup || !*ads->server.workgroup ) {
			use_own_domain = True;
			c_realm = lp_realm();
		}
	}

	if (c_realm && *c_realm)
		got_realm = True;

	/* we need to try once with the realm name and fallback to the
	   netbios domain name if we fail (if netbios has not been disabled */

	if ( !got_realm	&& !lp_disable_netbios() ) {
		c_realm = ads->server.workgroup;
		if (!c_realm || !*c_realm) {
			if ( use_own_domain )
				c_realm = lp_workgroup();
		}
	}

	if ( !c_realm || !*c_realm ) {
		DEBUG(0,("ads_find_dc: no realm or workgroup!  Don't know what to do\n"));
		return NT_STATUS_INVALID_PARAMETER; /* rather need MISSING_PARAMETER ... */
	}

	if ( use_own_domain ) {
		c_domain = lp_workgroup();
	} else {
		c_domain = ads->server.workgroup;
	}

	realm = c_realm;
	domain = c_domain;

	/*
	 * In case of LDAP we use get_dc_name() as that
	 * creates the custom krb5.conf file
	 */
	if (!(ads->auth.flags & ADS_AUTH_NO_BIND)) {
		fstring srv_name;
		struct sockaddr_storage ip_out;

		DEBUG(6,("ads_find_dc: (ldap) looking for %s '%s'\n",
			(got_realm ? "realm" : "domain"), realm));

		if (get_dc_name(domain, realm, srv_name, &ip_out)) {
			/*
			 * we call ads_try_connect() to fill in the
			 * ads->config details
			 */
			if (ads_try_connect(ads, srv_name, false)) {
				return NT_STATUS_OK;
			}
		}

		return NT_STATUS_NO_LOGON_SERVERS;
	}

	sitename = sitename_fetch(realm);

 again:

	DEBUG(6,("ads_find_dc: (cldap) looking for %s '%s'\n",
		(got_realm ? "realm" : "domain"), realm));

	status = get_sorted_dc_list(realm, sitename, &ip_list, &count, got_realm);
	if (!NT_STATUS_IS_OK(status)) {
		/* fall back to netbios if we can */
		if ( got_realm && !lp_disable_netbios() ) {
			got_realm = False;
			goto again;
		}

		SAFE_FREE(sitename);
		return status;
	}

	/* if we fail this loop, then giveup since all the IP addresses returned were dead */
	for ( i=0; i<count; i++ ) {
		char server[INET6_ADDRSTRLEN];

		print_sockaddr(server, sizeof(server), &ip_list[i].ss);

		if ( !NT_STATUS_IS_OK(check_negative_conn_cache(realm, server)) )
			continue;

		if (!got_realm) {
			/* realm in this case is a workgroup name. We need
			   to ignore any IP addresses in the negative connection
			   cache that match ip addresses returned in the ad realm
			   case. It sucks that I have to reproduce the logic above... */
			c_realm = ads->server.realm;
			if ( !c_realm || !*c_realm ) {
				if ( !ads->server.workgroup || !*ads->server.workgroup ) {
					c_realm = lp_realm();
				}
			}
			if (c_realm && *c_realm &&
					!NT_STATUS_IS_OK(check_negative_conn_cache(c_realm, server))) {
				/* Ensure we add the workgroup name for this
				   IP address as negative too. */
				add_failed_connection_entry( realm, server, NT_STATUS_UNSUCCESSFUL );
				continue;
			}
		}

		if ( ads_try_connect(ads, server, false) ) {
			SAFE_FREE(ip_list);
			SAFE_FREE(sitename);
			return NT_STATUS_OK;
		}

		/* keep track of failures */
		add_failed_connection_entry( realm, server, NT_STATUS_UNSUCCESSFUL );
	}

	SAFE_FREE(ip_list);

	/* In case we failed to contact one of our closest DC on our site we
	 * need to try to find another DC, retry with a site-less SRV DNS query
	 * - Guenther */

	if (sitename) {
		DEBUG(1,("ads_find_dc: failed to find a valid DC on our site (%s), "
				"trying to find another DC\n", sitename));
		SAFE_FREE(sitename);
		namecache_delete(realm, 0x1C);
		goto again;
	}

	return NT_STATUS_NO_LOGON_SERVERS;
}

/*********************************************************************
 *********************************************************************/

static NTSTATUS ads_lookup_site(void)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS ads_status;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	ads = ads_init(lp_realm(), NULL, NULL);
	if (!ads) {
		return NT_STATUS_NO_MEMORY;
	}

	/* The NO_BIND here will find a DC and set the client site
	   but not establish the TCP connection */

	ads->auth.flags = ADS_AUTH_NO_BIND;
	ads_status = ads_connect(ads);
	if (!ADS_ERR_OK(ads_status)) {
		DEBUG(4, ("ads_lookup_site: ads_connect to our realm failed! (%s)\n",
			  ads_errstr(ads_status)));
	}
	nt_status = ads_ntstatus(ads_status);

	if (ads) {
		ads_destroy(&ads);
	}

	return nt_status;
}

/*********************************************************************
 *********************************************************************/

static const char* host_dns_domain(const char *fqdn)
{
	const char *p = fqdn;

	/* go to next char following '.' */

	if ((p = strchr_m(fqdn, '.')) != NULL) {
		p++;
	}

	return p;
}


/**
 * Connect to the Global Catalog server
 * @param ads Pointer to an existing ADS_STRUCT
 * @return status of connection
 *
 * Simple wrapper around ads_connect() that fills in the
 * GC ldap server information
 **/

ADS_STATUS ads_connect_gc(ADS_STRUCT *ads)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct dns_rr_srv *gcs_list;
	int num_gcs;
	char *realm = ads->server.realm;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	ADS_STATUS ads_status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	int i;
	bool done = false;
	char *sitename = NULL;

	if (!realm)
		realm = lp_realm();

	if ((sitename = sitename_fetch(realm)) == NULL) {
		ads_lookup_site();
		sitename = sitename_fetch(realm);
	}

	do {
		/* We try once with a sitename and once without
		   (unless we don't have a sitename and then we're
		   done */

		if (sitename == NULL)
			done = true;

		nt_status = ads_dns_query_gcs(frame, realm, sitename,
					      &gcs_list, &num_gcs);

		SAFE_FREE(sitename);

		if (!NT_STATUS_IS_OK(nt_status)) {
			ads_status = ADS_ERROR_NT(nt_status);
			goto done;
		}

		/* Loop until we get a successful connection or have gone
		   through them all.  When connecting a GC server, make sure that
		   the realm is the server's DNS name and not the forest root */

		for (i=0; i<num_gcs; i++) {
			ads->server.gc = true;
			ads->server.ldap_server = SMB_STRDUP(gcs_list[i].hostname);
			ads->server.realm = SMB_STRDUP(host_dns_domain(ads->server.ldap_server));
			ads_status = ads_connect(ads);
			if (ADS_ERR_OK(ads_status)) {
				/* Reset the bind_dn to "".  A Global Catalog server
				   may host  multiple domain trees in a forest.
				   Windows 2003 GC server will accept "" as the search
				   path to imply search all domain trees in the forest */

				SAFE_FREE(ads->config.bind_path);
				ads->config.bind_path = SMB_STRDUP("");


				goto done;
			}
			SAFE_FREE(ads->server.ldap_server);
			SAFE_FREE(ads->server.realm);
		}

	        TALLOC_FREE(gcs_list);
		num_gcs = 0;
	} while (!done);

done:
	SAFE_FREE(sitename);
	talloc_destroy(frame);

	return ads_status;
}


/**
 * Connect to the LDAP server
 * @param ads Pointer to an existing ADS_STRUCT
 * @return status of connection
 **/
ADS_STATUS ads_connect(ADS_STRUCT *ads)
{
	int version = LDAP_VERSION3;
	ADS_STATUS status;
	NTSTATUS ntstatus;
	char addr[INET6_ADDRSTRLEN];

	ZERO_STRUCT(ads->ldap);
	ads->ldap.last_attempt	= time_mono(NULL);
	ads->ldap.wrap_type	= ADS_SASLWRAP_TYPE_PLAIN;

	/* try with a user specified server */

	if (DEBUGLEVEL >= 11) {
		char *s = NDR_PRINT_STRUCT_STRING(talloc_tos(), ads_struct, ads);
		DEBUG(11,("ads_connect: entering\n"));
		DEBUGADD(11,("%s\n", s));
		TALLOC_FREE(s);
	}

	if (ads->server.ldap_server)
	{
		if (ads_try_connect(ads, ads->server.ldap_server, ads->server.gc)) {
			goto got_connection;
		}

		/* The choice of which GC use is handled one level up in
		   ads_connect_gc().  If we continue on from here with
		   ads_find_dc() we will get GC searches on port 389 which
		   doesn't work.   --jerry */

		if (ads->server.gc == true) {
			return ADS_ERROR(LDAP_OPERATIONS_ERROR);
		}
	}

	ntstatus = ads_find_dc(ads);
	if (NT_STATUS_IS_OK(ntstatus)) {
		goto got_connection;
	}

	status = ADS_ERROR_NT(ntstatus);
	goto out;

got_connection:

	print_sockaddr(addr, sizeof(addr), &ads->ldap.ss);
	DEBUG(3,("Successfully contacted LDAP server %s\n", addr));

	if (!ads->auth.user_name) {
		/* Must use the userPrincipalName value here or sAMAccountName
		   and not servicePrincipalName; found by Guenther Deschner */

		if (asprintf(&ads->auth.user_name, "%s$", global_myname() ) == -1) {
			DEBUG(0,("ads_connect: asprintf fail.\n"));
			ads->auth.user_name = NULL;
		}
	}

	if (!ads->auth.realm) {
		ads->auth.realm = SMB_STRDUP(ads->config.realm);
	}

	if (!ads->auth.kdc_server) {
		print_sockaddr(addr, sizeof(addr), &ads->ldap.ss);
		ads->auth.kdc_server = SMB_STRDUP(addr);
	}

#if KRB5_DNS_HACK
	/* this is a really nasty hack to avoid ADS DNS problems. It needs a patch
	   to MIT kerberos to work (tridge) */
	{
		char *env = NULL;
		if (asprintf(&env, "KRB5_KDC_ADDRESS_%s", ads->config.realm) > 0) {
			setenv(env, ads->auth.kdc_server, 1);
			free(env);
		}
	}
#endif

	/* If the caller() requested no LDAP bind, then we are done */

	if (ads->auth.flags & ADS_AUTH_NO_BIND) {
		status = ADS_SUCCESS;
		goto out;
	}

	ads->ldap.mem_ctx = talloc_init("ads LDAP connection memory");
	if (!ads->ldap.mem_ctx) {
		status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
		goto out;
	}

	/* Otherwise setup the TCP LDAP session */

	ads->ldap.ld = ldap_open_with_timeout(ads->config.ldap_server_name,
					      ads->ldap.port, lp_ldap_timeout());
	if (ads->ldap.ld == NULL) {
		status = ADS_ERROR(LDAP_OPERATIONS_ERROR);
		goto out;
	}
	DEBUG(3,("Connected to LDAP server %s\n", ads->config.ldap_server_name));

	/* cache the successful connection for workgroup and realm */
	if (ads_closest_dc(ads)) {
		saf_store( ads->server.workgroup, ads->config.ldap_server_name);
		saf_store( ads->server.realm, ads->config.ldap_server_name);
	}

	ldap_set_option(ads->ldap.ld, LDAP_OPT_PROTOCOL_VERSION, &version);

	if ( lp_ldap_ssl_ads() ) {
		status = ADS_ERROR(smb_ldap_start_tls(ads->ldap.ld, version));
		if (!ADS_ERR_OK(status)) {
			goto out;
		}
	}

	/* fill in the current time and offsets */

	status = ads_current_time( ads );
	if ( !ADS_ERR_OK(status) ) {
		goto out;
	}

	/* Now do the bind */

	if (ads->auth.flags & ADS_AUTH_ANON_BIND) {
		status = ADS_ERROR(ldap_simple_bind_s(ads->ldap.ld, NULL, NULL));
		goto out;
	}

	if (ads->auth.flags & ADS_AUTH_SIMPLE_BIND) {
		status = ADS_ERROR(ldap_simple_bind_s(ads->ldap.ld, ads->auth.user_name, ads->auth.password));
		goto out;
	}

	status = ads_sasl_bind(ads);

 out:
	if (DEBUGLEVEL >= 11) {
		char *s = NDR_PRINT_STRUCT_STRING(talloc_tos(), ads_struct, ads);
		DEBUG(11,("ads_connect: leaving with: %s\n",
			ads_errstr(status)));
		DEBUGADD(11,("%s\n", s));
		TALLOC_FREE(s);
	}

	return status;
}

/**
 * Connect to the LDAP server using given credentials
 * @param ads Pointer to an existing ADS_STRUCT
 * @return status of connection
 **/
ADS_STATUS ads_connect_user_creds(ADS_STRUCT *ads)
{
	ads->auth.flags |= ADS_AUTH_USER_CREDS;

	return ads_connect(ads);
}

/**
 * Disconnect the LDAP server
 * @param ads Pointer to an existing ADS_STRUCT
 **/
void ads_disconnect(ADS_STRUCT *ads)
{
	if (ads->ldap.ld) {
		ldap_unbind(ads->ldap.ld);
		ads->ldap.ld = NULL;
	}
	if (ads->ldap.wrap_ops && ads->ldap.wrap_ops->disconnect) {
		ads->ldap.wrap_ops->disconnect(ads);
	}
	if (ads->ldap.mem_ctx) {
		talloc_free(ads->ldap.mem_ctx);
	}
	ZERO_STRUCT(ads->ldap);
}

/*
  Duplicate a struct berval into talloc'ed memory
 */
static struct berval *dup_berval(TALLOC_CTX *ctx, const struct berval *in_val)
{
	struct berval *value;

	if (!in_val) return NULL;

	value = TALLOC_ZERO_P(ctx, struct berval);
	if (value == NULL)
		return NULL;
	if (in_val->bv_len == 0) return value;

	value->bv_len = in_val->bv_len;
	value->bv_val = (char *)TALLOC_MEMDUP(ctx, in_val->bv_val,
					      in_val->bv_len);
	return value;
}

/*
  Make a values list out of an array of (struct berval *)
 */
static struct berval **ads_dup_values(TALLOC_CTX *ctx, 
				      const struct berval **in_vals)
{
	struct berval **values;
	int i;

	if (!in_vals) return NULL;
	for (i=0; in_vals[i]; i++)
		; /* count values */
	values = TALLOC_ZERO_ARRAY(ctx, struct berval *, i+1);
	if (!values) return NULL;

	for (i=0; in_vals[i]; i++) {
		values[i] = dup_berval(ctx, in_vals[i]);
	}
	return values;
}

/*
  UTF8-encode a values list out of an array of (char *)
 */
static char **ads_push_strvals(TALLOC_CTX *ctx, const char **in_vals)
{
	char **values;
	int i;
	size_t size;

	if (!in_vals) return NULL;
	for (i=0; in_vals[i]; i++)
		; /* count values */
	values = TALLOC_ZERO_ARRAY(ctx, char *, i+1);
	if (!values) return NULL;

	for (i=0; in_vals[i]; i++) {
		if (!push_utf8_talloc(ctx, &values[i], in_vals[i], &size)) {
			TALLOC_FREE(values);
			return NULL;
		}
	}
	return values;
}

/*
  Pull a (char *) array out of a UTF8-encoded values list
 */
static char **ads_pull_strvals(TALLOC_CTX *ctx, const char **in_vals)
{
	char **values;
	int i;
	size_t converted_size;

	if (!in_vals) return NULL;
	for (i=0; in_vals[i]; i++)
		; /* count values */
	values = TALLOC_ZERO_ARRAY(ctx, char *, i+1);
	if (!values) return NULL;

	for (i=0; in_vals[i]; i++) {
		if (!pull_utf8_talloc(ctx, &values[i], in_vals[i],
				      &converted_size)) {
			DEBUG(0,("ads_pull_strvals: pull_utf8_talloc failed: "
				 "%s", strerror(errno)));
		}
	}
	return values;
}

/**
 * Do a search with paged results.  cookie must be null on the first
 *  call, and then returned on each subsequent call.  It will be null
 *  again when the entire search is complete 
 * @param ads connection to ads server 
 * @param bind_path Base dn for the search
 * @param scope Scope of search (LDAP_SCOPE_BASE | LDAP_SCOPE_ONE | LDAP_SCOPE_SUBTREE)
 * @param expr Search expression - specified in local charset
 * @param attrs Attributes to retrieve - specified in utf8 or ascii
 * @param res ** which will contain results - free res* with ads_msgfree()
 * @param count Number of entries retrieved on this page
 * @param cookie The paged results cookie to be returned on subsequent calls
 * @return status of search
 **/
static ADS_STATUS ads_do_paged_search_args(ADS_STRUCT *ads,
					   const char *bind_path,
					   int scope, const char *expr,
					   const char **attrs, void *args,
					   LDAPMessage **res, 
					   int *count, struct berval **cookie)
{
	int rc, i, version;
	char *utf8_expr, *utf8_path, **search_attrs = NULL;
	size_t converted_size;
	LDAPControl PagedResults, NoReferrals, ExternalCtrl, *controls[4], **rcontrols;
	BerElement *cookie_be = NULL;
	struct berval *cookie_bv= NULL;
	BerElement *ext_be = NULL;
	struct berval *ext_bv= NULL;

	TALLOC_CTX *ctx;
	ads_control *external_control = (ads_control *) args;

	*res = NULL;

	if (!(ctx = talloc_init("ads_do_paged_search_args")))
		return ADS_ERROR(LDAP_NO_MEMORY);

	/* 0 means the conversion worked but the result was empty 
	   so we only fail if it's -1.  In any case, it always 
	   at least nulls out the dest */
	if (!push_utf8_talloc(ctx, &utf8_expr, expr, &converted_size) ||
	    !push_utf8_talloc(ctx, &utf8_path, bind_path, &converted_size))
	{
		rc = LDAP_NO_MEMORY;
		goto done;
	}

	if (!attrs || !(*attrs))
		search_attrs = NULL;
	else {
		/* This would be the utf8-encoded version...*/
		/* if (!(search_attrs = ads_push_strvals(ctx, attrs))) */
		if (!(search_attrs = str_list_copy(talloc_tos(), attrs))) {
			rc = LDAP_NO_MEMORY;
			goto done;
		}
	}

	/* Paged results only available on ldap v3 or later */
	ldap_get_option(ads->ldap.ld, LDAP_OPT_PROTOCOL_VERSION, &version);
	if (version < LDAP_VERSION3) {
		rc =  LDAP_NOT_SUPPORTED;
		goto done;
	}

	cookie_be = ber_alloc_t(LBER_USE_DER);
	if (*cookie) {
		ber_printf(cookie_be, "{iO}", (ber_int_t) ads->config.ldap_page_size, *cookie);
		ber_bvfree(*cookie); /* don't need it from last time */
		*cookie = NULL;
	} else {
		ber_printf(cookie_be, "{io}", (ber_int_t) ads->config.ldap_page_size, "", 0);
	}
	ber_flatten(cookie_be, &cookie_bv);
	PagedResults.ldctl_oid = CONST_DISCARD(char *, ADS_PAGE_CTL_OID);
	PagedResults.ldctl_iscritical = (char) 1;
	PagedResults.ldctl_value.bv_len = cookie_bv->bv_len;
	PagedResults.ldctl_value.bv_val = cookie_bv->bv_val;

	NoReferrals.ldctl_oid = CONST_DISCARD(char *, ADS_NO_REFERRALS_OID);
	NoReferrals.ldctl_iscritical = (char) 0;
	NoReferrals.ldctl_value.bv_len = 0;
	NoReferrals.ldctl_value.bv_val = CONST_DISCARD(char *, "");

	if (external_control && 
	    (strequal(external_control->control, ADS_EXTENDED_DN_OID) || 
	     strequal(external_control->control, ADS_SD_FLAGS_OID))) {

		ExternalCtrl.ldctl_oid = CONST_DISCARD(char *, external_control->control);
		ExternalCtrl.ldctl_iscritical = (char) external_control->critical;

		/* win2k does not accept a ldctl_value beeing passed in */

		if (external_control->val != 0) {

			if ((ext_be = ber_alloc_t(LBER_USE_DER)) == NULL ) {
				rc = LDAP_NO_MEMORY;
				goto done;
			}

			if ((ber_printf(ext_be, "{i}", (ber_int_t) external_control->val)) == -1) {
				rc = LDAP_NO_MEMORY;
				goto done;
			}
			if ((ber_flatten(ext_be, &ext_bv)) == -1) {
				rc = LDAP_NO_MEMORY;
				goto done;
			}

			ExternalCtrl.ldctl_value.bv_len = ext_bv->bv_len;
			ExternalCtrl.ldctl_value.bv_val = ext_bv->bv_val;

		} else {
			ExternalCtrl.ldctl_value.bv_len = 0;
			ExternalCtrl.ldctl_value.bv_val = NULL;
		}

		controls[0] = &NoReferrals;
		controls[1] = &PagedResults;
		controls[2] = &ExternalCtrl;
		controls[3] = NULL;

	} else {
		controls[0] = &NoReferrals;
		controls[1] = &PagedResults;
		controls[2] = NULL;
	}

	/* we need to disable referrals as the openldap libs don't
	   handle them and paged results at the same time.  Using them
	   together results in the result record containing the server 
	   page control being removed from the result list (tridge/jmcd) 

	   leaving this in despite the control that says don't generate
	   referrals, in case the server doesn't support it (jmcd)
	*/
	ldap_set_option(ads->ldap.ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);

	rc = ldap_search_with_timeout(ads->ldap.ld, utf8_path, scope, utf8_expr, 
				      search_attrs, 0, controls,
				      NULL, LDAP_NO_LIMIT,
				      (LDAPMessage **)res);

	ber_free(cookie_be, 1);
	ber_bvfree(cookie_bv);

	if (rc) {
		DEBUG(3,("ads_do_paged_search_args: ldap_search_with_timeout(%s) -> %s\n", expr,
			 ldap_err2string(rc)));
		goto done;
	}

	rc = ldap_parse_result(ads->ldap.ld, *res, NULL, NULL, NULL,
					NULL, &rcontrols,  0);

	if (!rcontrols) {
		goto done;
	}

	for (i=0; rcontrols[i]; i++) {
		if (strcmp(ADS_PAGE_CTL_OID, rcontrols[i]->ldctl_oid) == 0) {
			cookie_be = ber_init(&rcontrols[i]->ldctl_value);
			ber_scanf(cookie_be,"{iO}", (ber_int_t *) count,
				  &cookie_bv);
			/* the berval is the cookie, but must be freed when
			   it is all done */
			if (cookie_bv->bv_len) /* still more to do */
				*cookie=ber_bvdup(cookie_bv);
			else
				*cookie=NULL;
			ber_bvfree(cookie_bv);
			ber_free(cookie_be, 1);
			break;
		}
	}
	ldap_controls_free(rcontrols);

done:
	talloc_destroy(ctx);

	if (ext_be) {
		ber_free(ext_be, 1);
	}

	if (ext_bv) {
		ber_bvfree(ext_bv);
	}

	/* if/when we decide to utf8-encode attrs, take out this next line */
	TALLOC_FREE(search_attrs);

	return ADS_ERROR(rc);
}

static ADS_STATUS ads_do_paged_search(ADS_STRUCT *ads, const char *bind_path,
				      int scope, const char *expr,
				      const char **attrs, LDAPMessage **res, 
				      int *count, struct berval **cookie)
{
	return ads_do_paged_search_args(ads, bind_path, scope, expr, attrs, NULL, res, count, cookie);
}


/**
 * Get all results for a search.  This uses ads_do_paged_search() to return 
 * all entries in a large search.
 * @param ads connection to ads server 
 * @param bind_path Base dn for the search
 * @param scope Scope of search (LDAP_SCOPE_BASE | LDAP_SCOPE_ONE | LDAP_SCOPE_SUBTREE)
 * @param expr Search expression
 * @param attrs Attributes to retrieve
 * @param res ** which will contain results - free res* with ads_msgfree()
 * @return status of search
 **/
 ADS_STATUS ads_do_search_all_args(ADS_STRUCT *ads, const char *bind_path,
				   int scope, const char *expr,
				   const char **attrs, void *args,
				   LDAPMessage **res)
{
	struct berval *cookie = NULL;
	int count = 0;
	ADS_STATUS status;

	*res = NULL;
	status = ads_do_paged_search_args(ads, bind_path, scope, expr, attrs, args, res,
				     &count, &cookie);

	if (!ADS_ERR_OK(status)) 
		return status;

#ifdef HAVE_LDAP_ADD_RESULT_ENTRY
	while (cookie) {
		LDAPMessage *res2 = NULL;
		ADS_STATUS status2;
		LDAPMessage *msg, *next;

		status2 = ads_do_paged_search_args(ads, bind_path, scope, expr, 
					      attrs, args, &res2, &count, &cookie);

		if (!ADS_ERR_OK(status2)) break;

		/* this relies on the way that ldap_add_result_entry() works internally. I hope
		   that this works on all ldap libs, but I have only tested with openldap */
		for (msg = ads_first_message(ads, res2); msg; msg = next) {
			next = ads_next_message(ads, msg);
			ldap_add_result_entry((LDAPMessage **)res, msg);
		}
		/* note that we do not free res2, as the memory is now
                   part of the main returned list */
	}
#else
	DEBUG(0, ("no ldap_add_result_entry() support in LDAP libs!\n"));
	status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
#endif

	return status;
}

 ADS_STATUS ads_do_search_all(ADS_STRUCT *ads, const char *bind_path,
			      int scope, const char *expr,
			      const char **attrs, LDAPMessage **res)
{
	return ads_do_search_all_args(ads, bind_path, scope, expr, attrs, NULL, res);
}

 ADS_STATUS ads_do_search_all_sd_flags(ADS_STRUCT *ads, const char *bind_path,
				       int scope, const char *expr,
				       const char **attrs, uint32 sd_flags, 
				       LDAPMessage **res)
{
	ads_control args;

	args.control = ADS_SD_FLAGS_OID;
	args.val = sd_flags;
	args.critical = True;

	return ads_do_search_all_args(ads, bind_path, scope, expr, attrs, &args, res);
}


/**
 * Run a function on all results for a search.  Uses ads_do_paged_search() and
 *  runs the function as each page is returned, using ads_process_results()
 * @param ads connection to ads server
 * @param bind_path Base dn for the search
 * @param scope Scope of search (LDAP_SCOPE_BASE | LDAP_SCOPE_ONE | LDAP_SCOPE_SUBTREE)
 * @param expr Search expression - specified in local charset
 * @param attrs Attributes to retrieve - specified in UTF-8 or ascii
 * @param fn Function which takes attr name, values list, and data_area
 * @param data_area Pointer which is passed to function on each call
 * @return status of search
 **/
ADS_STATUS ads_do_search_all_fn(ADS_STRUCT *ads, const char *bind_path,
				int scope, const char *expr, const char **attrs,
				bool (*fn)(ADS_STRUCT *, char *, void **, void *), 
				void *data_area)
{
	struct berval *cookie = NULL;
	int count = 0;
	ADS_STATUS status;
	LDAPMessage *res;

	status = ads_do_paged_search(ads, bind_path, scope, expr, attrs, &res,
				     &count, &cookie);

	if (!ADS_ERR_OK(status)) return status;

	ads_process_results(ads, res, fn, data_area);
	ads_msgfree(ads, res);

	while (cookie) {
		status = ads_do_paged_search(ads, bind_path, scope, expr, attrs,
					     &res, &count, &cookie);

		if (!ADS_ERR_OK(status)) break;

		ads_process_results(ads, res, fn, data_area);
		ads_msgfree(ads, res);
	}

	return status;
}

/**
 * Do a search with a timeout.
 * @param ads connection to ads server
 * @param bind_path Base dn for the search
 * @param scope Scope of search (LDAP_SCOPE_BASE | LDAP_SCOPE_ONE | LDAP_SCOPE_SUBTREE)
 * @param expr Search expression
 * @param attrs Attributes to retrieve
 * @param res ** which will contain results - free res* with ads_msgfree()
 * @return status of search
 **/
 ADS_STATUS ads_do_search(ADS_STRUCT *ads, const char *bind_path, int scope, 
			  const char *expr,
			  const char **attrs, LDAPMessage **res)
{
	int rc;
	char *utf8_expr, *utf8_path, **search_attrs = NULL;
	size_t converted_size;
	TALLOC_CTX *ctx;

	*res = NULL;
	if (!(ctx = talloc_init("ads_do_search"))) {
		DEBUG(1,("ads_do_search: talloc_init() failed!"));
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	/* 0 means the conversion worked but the result was empty 
	   so we only fail if it's negative.  In any case, it always 
	   at least nulls out the dest */
	if (!push_utf8_talloc(ctx, &utf8_expr, expr, &converted_size) ||
	    !push_utf8_talloc(ctx, &utf8_path, bind_path, &converted_size))
	{
		DEBUG(1,("ads_do_search: push_utf8_talloc() failed!"));
		rc = LDAP_NO_MEMORY;
		goto done;
	}

	if (!attrs || !(*attrs))
		search_attrs = NULL;
	else {
		/* This would be the utf8-encoded version...*/
		/* if (!(search_attrs = ads_push_strvals(ctx, attrs)))  */
		if (!(search_attrs = str_list_copy(talloc_tos(), attrs)))
		{
			DEBUG(1,("ads_do_search: str_list_copy() failed!"));
			rc = LDAP_NO_MEMORY;
			goto done;
		}
	}

	/* see the note in ads_do_paged_search - we *must* disable referrals */
	ldap_set_option(ads->ldap.ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);

	rc = ldap_search_with_timeout(ads->ldap.ld, utf8_path, scope, utf8_expr,
				      search_attrs, 0, NULL, NULL, 
				      LDAP_NO_LIMIT,
				      (LDAPMessage **)res);

	if (rc == LDAP_SIZELIMIT_EXCEEDED) {
		DEBUG(3,("Warning! sizelimit exceeded in ldap. Truncating.\n"));
		rc = 0;
	}

 done:
	talloc_destroy(ctx);
	/* if/when we decide to utf8-encode attrs, take out this next line */
	TALLOC_FREE(search_attrs);
	return ADS_ERROR(rc);
}
/**
 * Do a general ADS search
 * @param ads connection to ads server
 * @param res ** which will contain results - free res* with ads_msgfree()
 * @param expr Search expression
 * @param attrs Attributes to retrieve
 * @return status of search
 **/
 ADS_STATUS ads_search(ADS_STRUCT *ads, LDAPMessage **res, 
		       const char *expr, const char **attrs)
{
	return ads_do_search(ads, ads->config.bind_path, LDAP_SCOPE_SUBTREE, 
			     expr, attrs, res);
}

/**
 * Do a search on a specific DistinguishedName
 * @param ads connection to ads server
 * @param res ** which will contain results - free res* with ads_msgfree()
 * @param dn DistinguishName to search
 * @param attrs Attributes to retrieve
 * @return status of search
 **/
 ADS_STATUS ads_search_dn(ADS_STRUCT *ads, LDAPMessage **res, 
			  const char *dn, const char **attrs)
{
	return ads_do_search(ads, dn, LDAP_SCOPE_BASE, "(objectclass=*)",
			     attrs, res);
}

/**
 * Free up memory from a ads_search
 * @param ads connection to ads server
 * @param msg Search results to free
 **/
 void ads_msgfree(ADS_STRUCT *ads, LDAPMessage *msg)
{
	if (!msg) return;
	ldap_msgfree(msg);
}

/**
 * Get a dn from search results
 * @param ads connection to ads server
 * @param msg Search result
 * @return dn string
 **/
 char *ads_get_dn(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, LDAPMessage *msg)
{
	char *utf8_dn, *unix_dn;
	size_t converted_size;

	utf8_dn = ldap_get_dn(ads->ldap.ld, msg);

	if (!utf8_dn) {
		DEBUG (5, ("ads_get_dn: ldap_get_dn failed\n"));
		return NULL;
	}

	if (!pull_utf8_talloc(mem_ctx, &unix_dn, utf8_dn, &converted_size)) {
		DEBUG(0,("ads_get_dn: string conversion failure utf8 [%s]\n",
			utf8_dn ));
		return NULL;
	}
	ldap_memfree(utf8_dn);
	return unix_dn;
}

/**
 * Get the parent from a dn
 * @param dn the dn to return the parent from
 * @return parent dn string
 **/
char *ads_parent_dn(const char *dn)
{
	char *p;

	if (dn == NULL) {
		return NULL;
	}

	p = strchr(dn, ',');

	if (p == NULL) {
		return NULL;
	}

	return p+1;
}

/**
 * Find a machine account given a hostname
 * @param ads connection to ads server
 * @param res ** which will contain results - free res* with ads_msgfree()
 * @param host Hostname to search for
 * @return status of search
 **/
 ADS_STATUS ads_find_machine_acct(ADS_STRUCT *ads, LDAPMessage **res,
				  const char *machine)
{
	ADS_STATUS status;
	char *expr;
	const char *attrs[] = {"*", "nTSecurityDescriptor", NULL};

	*res = NULL;

	/* the easiest way to find a machine account anywhere in the tree
	   is to look for hostname$ */
	if (asprintf(&expr, "(samAccountName=%s$)", machine) == -1) {
		DEBUG(1, ("asprintf failed!\n"));
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	status = ads_search(ads, res, expr, attrs);
	SAFE_FREE(expr);
	return status;
}

/**
 * Initialize a list of mods to be used in a modify request
 * @param ctx An initialized TALLOC_CTX
 * @return allocated ADS_MODLIST
 **/
ADS_MODLIST ads_init_mods(TALLOC_CTX *ctx)
{
#define ADS_MODLIST_ALLOC_SIZE 10
	LDAPMod **mods;

	if ((mods = TALLOC_ZERO_ARRAY(ctx, LDAPMod *, ADS_MODLIST_ALLOC_SIZE + 1)))
		/* -1 is safety to make sure we don't go over the end.
		   need to reset it to NULL before doing ldap modify */
		mods[ADS_MODLIST_ALLOC_SIZE] = (LDAPMod *) -1;

	return (ADS_MODLIST)mods;
}


/*
  add an attribute to the list, with values list already constructed
*/
static ADS_STATUS ads_modlist_add(TALLOC_CTX *ctx, ADS_MODLIST *mods, 
				  int mod_op, const char *name, 
				  const void *_invals)
{
	const void **invals = (const void **)_invals;
	int curmod;
	LDAPMod **modlist = (LDAPMod **) *mods;
	struct berval **ber_values = NULL;
	char **char_values = NULL;

	if (!invals) {
		mod_op = LDAP_MOD_DELETE;
	} else {
		if (mod_op & LDAP_MOD_BVALUES)
			ber_values = ads_dup_values(ctx, 
						(const struct berval **)invals);
		else
			char_values = ads_push_strvals(ctx, 
						  (const char **) invals);
	}

	/* find the first empty slot */
	for (curmod=0; modlist[curmod] && modlist[curmod] != (LDAPMod *) -1;
	     curmod++);
	if (modlist[curmod] == (LDAPMod *) -1) {
		if (!(modlist = TALLOC_REALLOC_ARRAY(ctx, modlist, LDAPMod *,
				curmod+ADS_MODLIST_ALLOC_SIZE+1)))
			return ADS_ERROR(LDAP_NO_MEMORY);
		memset(&modlist[curmod], 0, 
		       ADS_MODLIST_ALLOC_SIZE*sizeof(LDAPMod *));
		modlist[curmod+ADS_MODLIST_ALLOC_SIZE] = (LDAPMod *) -1;
		*mods = (ADS_MODLIST)modlist;
	}

	if (!(modlist[curmod] = TALLOC_ZERO_P(ctx, LDAPMod)))
		return ADS_ERROR(LDAP_NO_MEMORY);
	modlist[curmod]->mod_type = talloc_strdup(ctx, name);
	if (mod_op & LDAP_MOD_BVALUES) {
		modlist[curmod]->mod_bvalues = ber_values;
	} else if (mod_op & LDAP_MOD_DELETE) {
		modlist[curmod]->mod_values = NULL;
	} else {
		modlist[curmod]->mod_values = char_values;
	}

	modlist[curmod]->mod_op = mod_op;
	return ADS_ERROR(LDAP_SUCCESS);
}

/**
 * Add a single string value to a mod list
 * @param ctx An initialized TALLOC_CTX
 * @param mods An initialized ADS_MODLIST
 * @param name The attribute name to add
 * @param val The value to add - NULL means DELETE
 * @return ADS STATUS indicating success of add
 **/
ADS_STATUS ads_mod_str(TALLOC_CTX *ctx, ADS_MODLIST *mods, 
		       const char *name, const char *val)
{
	const char *values[2];

	values[0] = val;
	values[1] = NULL;

	if (!val)
		return ads_modlist_add(ctx, mods, LDAP_MOD_DELETE, name, NULL);
	return ads_modlist_add(ctx, mods, LDAP_MOD_REPLACE, name, values);
}

/**
 * Add an array of string values to a mod list
 * @param ctx An initialized TALLOC_CTX
 * @param mods An initialized ADS_MODLIST
 * @param name The attribute name to add
 * @param vals The array of string values to add - NULL means DELETE
 * @return ADS STATUS indicating success of add
 **/
ADS_STATUS ads_mod_strlist(TALLOC_CTX *ctx, ADS_MODLIST *mods,
			   const char *name, const char **vals)
{
	if (!vals)
		return ads_modlist_add(ctx, mods, LDAP_MOD_DELETE, name, NULL);
	return ads_modlist_add(ctx, mods, LDAP_MOD_REPLACE, 
			       name, (const void **) vals);
}

#if 0
/**
 * Add a single ber-encoded value to a mod list
 * @param ctx An initialized TALLOC_CTX
 * @param mods An initialized ADS_MODLIST
 * @param name The attribute name to add
 * @param val The value to add - NULL means DELETE
 * @return ADS STATUS indicating success of add
 **/
static ADS_STATUS ads_mod_ber(TALLOC_CTX *ctx, ADS_MODLIST *mods, 
			      const char *name, const struct berval *val)
{
	const struct berval *values[2];

	values[0] = val;
	values[1] = NULL;
	if (!val)
		return ads_modlist_add(ctx, mods, LDAP_MOD_DELETE, name, NULL);
	return ads_modlist_add(ctx, mods, LDAP_MOD_REPLACE|LDAP_MOD_BVALUES,
			       name, (const void **) values);
}
#endif

/**
 * Perform an ldap modify
 * @param ads connection to ads server
 * @param mod_dn DistinguishedName to modify
 * @param mods list of modifications to perform
 * @return status of modify
 **/
ADS_STATUS ads_gen_mod(ADS_STRUCT *ads, const char *mod_dn, ADS_MODLIST mods)
{
	int ret,i;
	char *utf8_dn = NULL;
	size_t converted_size;
	/* 
	   this control is needed to modify that contains a currently 
	   non-existent attribute (but allowable for the object) to run
	*/
	LDAPControl PermitModify = {
                CONST_DISCARD(char *, ADS_PERMIT_MODIFY_OID),
		{0, NULL},
		(char) 1};
	LDAPControl *controls[2];

	controls[0] = &PermitModify;
	controls[1] = NULL;

	if (!push_utf8_talloc(talloc_tos(), &utf8_dn, mod_dn, &converted_size)) {
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	/* find the end of the list, marked by NULL or -1 */
	for(i=0;(mods[i]!=0)&&(mods[i]!=(LDAPMod *) -1);i++);
	/* make sure the end of the list is NULL */
	mods[i] = NULL;
	ret = ldap_modify_ext_s(ads->ldap.ld, utf8_dn,
				(LDAPMod **) mods, controls, NULL);
	TALLOC_FREE(utf8_dn);
	return ADS_ERROR(ret);
}

/**
 * Perform an ldap add
 * @param ads connection to ads server
 * @param new_dn DistinguishedName to add
 * @param mods list of attributes and values for DN
 * @return status of add
 **/
ADS_STATUS ads_gen_add(ADS_STRUCT *ads, const char *new_dn, ADS_MODLIST mods)
{
	int ret, i;
	char *utf8_dn = NULL;
	size_t converted_size;

	if (!push_utf8_talloc(talloc_tos(), &utf8_dn, new_dn, &converted_size)) {
		DEBUG(1, ("ads_gen_add: push_utf8_talloc failed!"));
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	/* find the end of the list, marked by NULL or -1 */
	for(i=0;(mods[i]!=0)&&(mods[i]!=(LDAPMod *) -1);i++);
	/* make sure the end of the list is NULL */
	mods[i] = NULL;

	ret = ldap_add_s(ads->ldap.ld, utf8_dn, (LDAPMod**)mods);
	TALLOC_FREE(utf8_dn);
	return ADS_ERROR(ret);
}

/**
 * Delete a DistinguishedName
 * @param ads connection to ads server
 * @param new_dn DistinguishedName to delete
 * @return status of delete
 **/
ADS_STATUS ads_del_dn(ADS_STRUCT *ads, char *del_dn)
{
	int ret;
	char *utf8_dn = NULL;
	size_t converted_size;
	if (!push_utf8_talloc(talloc_tos(), &utf8_dn, del_dn, &converted_size)) {
		DEBUG(1, ("ads_del_dn: push_utf8_talloc failed!"));
		return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
	}

	ret = ldap_delete_s(ads->ldap.ld, utf8_dn);
	TALLOC_FREE(utf8_dn);
	return ADS_ERROR(ret);
}

/**
 * Build an org unit string
 *  if org unit is Computers or blank then assume a container, otherwise
 *  assume a / separated list of organisational units.
 * jmcd: '\' is now used for escapes so certain chars can be in the ou (e.g. #)
 * @param ads connection to ads server
 * @param org_unit Organizational unit
 * @return org unit string - caller must free
 **/
char *ads_ou_string(ADS_STRUCT *ads, const char *org_unit)
{
	char *ret = NULL;

	if (!org_unit || !*org_unit) {

		ret = ads_default_ou_string(ads, DS_GUID_COMPUTERS_CONTAINER);

		/* samba4 might not yet respond to a wellknownobject-query */
		return ret ? ret : SMB_STRDUP("cn=Computers");
	}

	if (strequal(org_unit, "Computers")) {
		return SMB_STRDUP("cn=Computers");
	}

	/* jmcd: removed "\\" from the separation chars, because it is
	   needed as an escape for chars like '#' which are valid in an
	   OU name */
	return ads_build_path(org_unit, "/", "ou=", 1);
}

/**
 * Get a org unit string for a well-known GUID
 * @param ads connection to ads server
 * @param wknguid Well known GUID
 * @return org unit string - caller must free
 **/
char *ads_default_ou_string(ADS_STRUCT *ads, const char *wknguid)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	char *base, *wkn_dn = NULL, *ret = NULL, **wkn_dn_exp = NULL,
		**bind_dn_exp = NULL;
	const char *attrs[] = {"distinguishedName", NULL};
	int new_ln, wkn_ln, bind_ln, i;

	if (wknguid == NULL) {
		return NULL;
	}

	if (asprintf(&base, "<WKGUID=%s,%s>", wknguid, ads->config.bind_path ) == -1) {
		DEBUG(1, ("asprintf failed!\n"));
		return NULL;
	}

	status = ads_search_dn(ads, &res, base, attrs);
	if (!ADS_ERR_OK(status)) {
		DEBUG(1,("Failed while searching for: %s\n", base));
		goto out;
	}

	if (ads_count_replies(ads, res) != 1) {
		goto out;
	}

	/* substitute the bind-path from the well-known-guid-search result */
	wkn_dn = ads_get_dn(ads, talloc_tos(), res);
	if (!wkn_dn) {
		goto out;
	}

	wkn_dn_exp = ldap_explode_dn(wkn_dn, 0);
	if (!wkn_dn_exp) {
		goto out;
	}

	bind_dn_exp = ldap_explode_dn(ads->config.bind_path, 0);
	if (!bind_dn_exp) {
		goto out;
	}

	for (wkn_ln=0; wkn_dn_exp[wkn_ln]; wkn_ln++)
		;
	for (bind_ln=0; bind_dn_exp[bind_ln]; bind_ln++)
		;

	new_ln = wkn_ln - bind_ln;

	ret = SMB_STRDUP(wkn_dn_exp[0]);
	if (!ret) {
		goto out;
	}

	for (i=1; i < new_ln; i++) {
		char *s = NULL;

		if (asprintf(&s, "%s,%s", ret, wkn_dn_exp[i]) == -1) {
			SAFE_FREE(ret);
			goto out;
		}

		SAFE_FREE(ret);
		ret = SMB_STRDUP(s);
		free(s);
		if (!ret) {
			goto out;
		}
	}

 out:
	SAFE_FREE(base);
	ads_msgfree(ads, res);
	TALLOC_FREE(wkn_dn);
	if (wkn_dn_exp) {
		ldap_value_free(wkn_dn_exp);
	}
	if (bind_dn_exp) {
		ldap_value_free(bind_dn_exp);
	}

	return ret;
}

/**
 * Adds (appends) an item to an attribute array, rather then
 * replacing the whole list
 * @param ctx An initialized TALLOC_CTX
 * @param mods An initialized ADS_MODLIST
 * @param name name of the ldap attribute to append to
 * @param vals an array of values to add
 * @return status of addition
 **/

ADS_STATUS ads_add_strlist(TALLOC_CTX *ctx, ADS_MODLIST *mods,
				const char *name, const char **vals)
{
	return ads_modlist_add(ctx, mods, LDAP_MOD_ADD, name,
			       (const void *) vals);
}

/**
 * Determines the an account's current KVNO via an LDAP lookup
 * @param ads An initialized ADS_STRUCT
 * @param account_name the NT samaccountname.
 * @return the kvno for the account, or -1 in case of a failure.
 **/

uint32 ads_get_kvno(ADS_STRUCT *ads, const char *account_name)
{
	LDAPMessage *res = NULL;
	uint32 kvno = (uint32)-1;      /* -1 indicates a failure */
	char *filter;
	const char *attrs[] = {"msDS-KeyVersionNumber", NULL};
	char *dn_string = NULL;
	ADS_STATUS ret = ADS_ERROR(LDAP_SUCCESS);

	DEBUG(5,("ads_get_kvno: Searching for account %s\n", account_name));
	if (asprintf(&filter, "(samAccountName=%s)", account_name) == -1) {
		return kvno;
	}
	ret = ads_search(ads, &res, filter, attrs);
	SAFE_FREE(filter);
	if (!ADS_ERR_OK(ret) || (ads_count_replies(ads, res) != 1)) {
		DEBUG(1,("ads_get_kvno: Account for %s not found.\n", account_name));
		ads_msgfree(ads, res);
		return kvno;
	}

	dn_string = ads_get_dn(ads, talloc_tos(), res);
	if (!dn_string) {
		DEBUG(0,("ads_get_kvno: out of memory.\n"));
		ads_msgfree(ads, res);
		return kvno;
	}
	DEBUG(5,("ads_get_kvno: Using: %s\n", dn_string));
	TALLOC_FREE(dn_string);

	/* ---------------------------------------------------------
	 * 0 is returned as a default KVNO from this point on...
	 * This is done because Windows 2000 does not support key
	 * version numbers.  Chances are that a failure in the next
	 * step is simply due to Windows 2000 being used for a
	 * domain controller. */
	kvno = 0;

	if (!ads_pull_uint32(ads, res, "msDS-KeyVersionNumber", &kvno)) {
		DEBUG(3,("ads_get_kvno: Error Determining KVNO!\n"));
		DEBUG(3,("ads_get_kvno: Windows 2000 does not support KVNO's, so this may be normal.\n"));
		ads_msgfree(ads, res);
		return kvno;
	}

	/* Success */
	DEBUG(5,("ads_get_kvno: Looked Up KVNO of: %d\n", kvno));
	ads_msgfree(ads, res);
	return kvno;
}

/**
 * Determines the computer account's current KVNO via an LDAP lookup
 * @param ads An initialized ADS_STRUCT
 * @param machine_name the NetBIOS name of the computer, which is used to identify the computer account.
 * @return the kvno for the computer account, or -1 in case of a failure.
 **/

uint32_t ads_get_machine_kvno(ADS_STRUCT *ads, const char *machine_name)
{
	char *computer_account = NULL;
	uint32_t kvno = -1;

	if (asprintf(&computer_account, "%s$", machine_name) < 0) {
		return kvno;
	}

	kvno = ads_get_kvno(ads, computer_account);
	free(computer_account);

	return kvno;
}

/**
 * This clears out all registered spn's for a given hostname
 * @param ads An initilaized ADS_STRUCT
 * @param machine_name the NetBIOS name of the computer.
 * @return 0 upon success, non-zero otherwise.
 **/

ADS_STATUS ads_clear_service_principal_names(ADS_STRUCT *ads, const char *machine_name)
{
	TALLOC_CTX *ctx;
	LDAPMessage *res = NULL;
	ADS_MODLIST mods;
	const char *servicePrincipalName[1] = {NULL};
	ADS_STATUS ret = ADS_ERROR(LDAP_SUCCESS);
	char *dn_string = NULL;

	ret = ads_find_machine_acct(ads, &res, machine_name);
	if (!ADS_ERR_OK(ret) || ads_count_replies(ads, res) != 1) {
		DEBUG(5,("ads_clear_service_principal_names: WARNING: Host Account for %s not found... skipping operation.\n", machine_name));
		DEBUG(5,("ads_clear_service_principal_names: WARNING: Service Principals for %s have NOT been cleared.\n", machine_name));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	DEBUG(5,("ads_clear_service_principal_names: Host account for %s found\n", machine_name));
	ctx = talloc_init("ads_clear_service_principal_names");
	if (!ctx) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	if (!(mods = ads_init_mods(ctx))) {
		talloc_destroy(ctx);
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	ret = ads_mod_strlist(ctx, &mods, "servicePrincipalName", servicePrincipalName);
	if (!ADS_ERR_OK(ret)) {
		DEBUG(1,("ads_clear_service_principal_names: Error creating strlist.\n"));
		ads_msgfree(ads, res);
		talloc_destroy(ctx);
		return ret;
	}
	dn_string = ads_get_dn(ads, talloc_tos(), res);
	if (!dn_string) {
		talloc_destroy(ctx);
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	ret = ads_gen_mod(ads, dn_string, mods);
	TALLOC_FREE(dn_string);
	if (!ADS_ERR_OK(ret)) {
		DEBUG(1,("ads_clear_service_principal_names: Error: Updating Service Principals for machine %s in LDAP\n",
			machine_name));
		ads_msgfree(ads, res);
		talloc_destroy(ctx);
		return ret;
	}

	ads_msgfree(ads, res);
	talloc_destroy(ctx);
	return ret;
}

/**
 * This adds a service principal name to an existing computer account
 * (found by hostname) in AD.
 * @param ads An initialized ADS_STRUCT
 * @param machine_name the NetBIOS name of the computer, which is used to identify the computer account.
 * @param my_fqdn The fully qualified DNS name of the machine
 * @param spn A string of the service principal to add, i.e. 'host'
 * @return 0 upon sucess, or non-zero if a failure occurs
 **/

ADS_STATUS ads_add_service_principal_name(ADS_STRUCT *ads, const char *machine_name, 
                                          const char *my_fqdn, const char *spn)
{
	ADS_STATUS ret;
	TALLOC_CTX *ctx;
	LDAPMessage *res = NULL;
	char *psp1, *psp2;
	ADS_MODLIST mods;
	char *dn_string = NULL;
	const char *servicePrincipalName[3] = {NULL, NULL, NULL};

	ret = ads_find_machine_acct(ads, &res, machine_name);
	if (!ADS_ERR_OK(ret) || ads_count_replies(ads, res) != 1) {
		DEBUG(1,("ads_add_service_principal_name: WARNING: Host Account for %s not found... skipping operation.\n",
			machine_name));
		DEBUG(1,("ads_add_service_principal_name: WARNING: Service Principal '%s/%s@%s' has NOT been added.\n",
			spn, machine_name, ads->config.realm));
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	DEBUG(1,("ads_add_service_principal_name: Host account for %s found\n", machine_name));
	if (!(ctx = talloc_init("ads_add_service_principal_name"))) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	/* add short name spn */

	if ( (psp1 = talloc_asprintf(ctx, "%s/%s", spn, machine_name)) == NULL ) {
		talloc_destroy(ctx);
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}
	strlower_m(&psp1[strlen(spn) + 1]);
	servicePrincipalName[0] = psp1;

	DEBUG(5,("ads_add_service_principal_name: INFO: Adding %s to host %s\n", 
		psp1, machine_name));


	/* add fully qualified spn */

	if ( (psp2 = talloc_asprintf(ctx, "%s/%s", spn, my_fqdn)) == NULL ) {
		ret = ADS_ERROR(LDAP_NO_MEMORY);
		goto out;
	}
	strlower_m(&psp2[strlen(spn) + 1]);
	servicePrincipalName[1] = psp2;

	DEBUG(5,("ads_add_service_principal_name: INFO: Adding %s to host %s\n", 
		psp2, machine_name));

	if ( (mods = ads_init_mods(ctx)) == NULL ) {
		ret = ADS_ERROR(LDAP_NO_MEMORY);
		goto out;
	}

	ret = ads_add_strlist(ctx, &mods, "servicePrincipalName", servicePrincipalName);
	if (!ADS_ERR_OK(ret)) {
		DEBUG(1,("ads_add_service_principal_name: Error: Updating Service Principals in LDAP\n"));
		goto out;
	}

	if ( (dn_string = ads_get_dn(ads, ctx, res)) == NULL ) {
		ret = ADS_ERROR(LDAP_NO_MEMORY);
		goto out;
	}

	ret = ads_gen_mod(ads, dn_string, mods);
	if (!ADS_ERR_OK(ret)) {
		DEBUG(1,("ads_add_service_principal_name: Error: Updating Service Principals in LDAP\n"));
		goto out;
	}

 out:
	TALLOC_FREE( ctx );
	ads_msgfree(ads, res);
	return ret;
}

/**
 * adds a machine account to the ADS server
 * @param ads An intialized ADS_STRUCT
 * @param machine_name - the NetBIOS machine name of this account.
 * @param account_type A number indicating the type of account to create
 * @param org_unit The LDAP path in which to place this account
 * @return 0 upon success, or non-zero otherwise
**/

ADS_STATUS ads_create_machine_acct(ADS_STRUCT *ads, const char *machine_name, 
                                   const char *org_unit)
{
	ADS_STATUS ret;
	char *samAccountName, *controlstr;
	TALLOC_CTX *ctx;
	ADS_MODLIST mods;
	char *machine_escaped = NULL;
	char *new_dn;
	const char *objectClass[] = {"top", "person", "organizationalPerson",
				     "user", "computer", NULL};
	LDAPMessage *res = NULL;
	uint32 acct_control = ( UF_WORKSTATION_TRUST_ACCOUNT |\
	                        UF_DONT_EXPIRE_PASSWD |\
			        UF_ACCOUNTDISABLE );

	if (!(ctx = talloc_init("ads_add_machine_acct")))
		return ADS_ERROR(LDAP_NO_MEMORY);

	ret = ADS_ERROR(LDAP_NO_MEMORY);

	machine_escaped = escape_rdn_val_string_alloc(machine_name);
	if (!machine_escaped) {
		goto done;
	}

	new_dn = talloc_asprintf(ctx, "cn=%s,%s", machine_escaped, org_unit);
	samAccountName = talloc_asprintf(ctx, "%s$", machine_name);

	if ( !new_dn || !samAccountName ) {
		goto done;
	}

#ifndef ENCTYPE_ARCFOUR_HMAC
	acct_control |= UF_USE_DES_KEY_ONLY;
#endif

	if (!(controlstr = talloc_asprintf(ctx, "%u", acct_control))) {
		goto done;
	}

	if (!(mods = ads_init_mods(ctx))) {
		goto done;
	}

	ads_mod_str(ctx, &mods, "cn", machine_name);
	ads_mod_str(ctx, &mods, "sAMAccountName", samAccountName);
	ads_mod_strlist(ctx, &mods, "objectClass", objectClass);
	ads_mod_str(ctx, &mods, "userAccountControl", controlstr);

	ret = ads_gen_add(ads, new_dn, mods);

done:
	SAFE_FREE(machine_escaped);
	ads_msgfree(ads, res);
	talloc_destroy(ctx);

	return ret;
}

/**
 * move a machine account to another OU on the ADS server
 * @param ads - An intialized ADS_STRUCT
 * @param machine_name - the NetBIOS machine name of this account.
 * @param org_unit - The LDAP path in which to place this account
 * @param moved - whether we moved the machine account (optional)
 * @return 0 upon success, or non-zero otherwise
**/

ADS_STATUS ads_move_machine_acct(ADS_STRUCT *ads, const char *machine_name, 
                                 const char *org_unit, bool *moved)
{
	ADS_STATUS rc;
	int ldap_status;
	LDAPMessage *res = NULL;
	char *filter = NULL;
	char *computer_dn = NULL;
	char *parent_dn;
	char *computer_rdn = NULL;
	bool need_move = False;

	if (asprintf(&filter, "(samAccountName=%s$)", machine_name) == -1) {
		rc = ADS_ERROR(LDAP_NO_MEMORY);
		goto done;
	}

	/* Find pre-existing machine */
	rc = ads_search(ads, &res, filter, NULL);
	if (!ADS_ERR_OK(rc)) {
		goto done;
	}

	computer_dn = ads_get_dn(ads, talloc_tos(), res);
	if (!computer_dn) {
		rc = ADS_ERROR(LDAP_NO_MEMORY);
		goto done;
	}

	parent_dn = ads_parent_dn(computer_dn);
	if (strequal(parent_dn, org_unit)) {
		goto done;
	}

	need_move = True;

	if (asprintf(&computer_rdn, "CN=%s", machine_name) == -1) {
		rc = ADS_ERROR(LDAP_NO_MEMORY);
		goto done;
	}

	ldap_status = ldap_rename_s(ads->ldap.ld, computer_dn, computer_rdn, 
				    org_unit, 1, NULL, NULL);
	rc = ADS_ERROR(ldap_status);

done:
	ads_msgfree(ads, res);
	SAFE_FREE(filter);
	TALLOC_FREE(computer_dn);
	SAFE_FREE(computer_rdn);

	if (!ADS_ERR_OK(rc)) {
		need_move = False;
	}

	if (moved) {
		*moved = need_move;
	}

	return rc;
}

/*
  dump a binary result from ldap
*/
static void dump_binary(ADS_STRUCT *ads, const char *field, struct berval **values)
{
	int i, j;
	for (i=0; values[i]; i++) {
		printf("%s: ", field);
		for (j=0; j<values[i]->bv_len; j++) {
			printf("%02X", (unsigned char)values[i]->bv_val[j]);
		}
		printf("\n");
	}
}

static void dump_guid(ADS_STRUCT *ads, const char *field, struct berval **values)
{
	int i;
	for (i=0; values[i]; i++) {
		NTSTATUS status;
		DATA_BLOB in = data_blob_const(values[i]->bv_val, values[i]->bv_len);
		struct GUID guid;

		status = GUID_from_ndr_blob(&in, &guid);
		if (NT_STATUS_IS_OK(status)) {
			printf("%s: %s\n", field, GUID_string(talloc_tos(), &guid));
		} else {
			printf("%s: INVALID GUID\n", field);
		}
	}
}

/*
  dump a sid result from ldap
*/
static void dump_sid(ADS_STRUCT *ads, const char *field, struct berval **values)
{
	int i;
	for (i=0; values[i]; i++) {
		struct dom_sid sid;
		fstring tmp;
		if (!sid_parse(values[i]->bv_val, values[i]->bv_len, &sid)) {
			return;
		}
		printf("%s: %s\n", field, sid_to_fstring(tmp, &sid));
	}
}

/*
  dump ntSecurityDescriptor
*/
static void dump_sd(ADS_STRUCT *ads, const char *filed, struct berval **values)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct security_descriptor *psd;
	NTSTATUS status;

	status = unmarshall_sec_desc(talloc_tos(), (uint8 *)values[0]->bv_val,
				     values[0]->bv_len, &psd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("unmarshall_sec_desc failed: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(frame);
		return;
	}

	if (psd) {
		ads_disp_sd(ads, talloc_tos(), psd);
	}

	TALLOC_FREE(frame);
}

/*
  dump a string result from ldap
*/
static void dump_string(const char *field, char **values)
{
	int i;
	for (i=0; values[i]; i++) {
		printf("%s: %s\n", field, values[i]);
	}
}

/*
  dump a field from LDAP on stdout
  used for debugging
*/

static bool ads_dump_field(ADS_STRUCT *ads, char *field, void **values, void *data_area)
{
	const struct {
		const char *name;
		bool string;
		void (*handler)(ADS_STRUCT *, const char *, struct berval **);
	} handlers[] = {
		{"objectGUID", False, dump_guid},
		{"netbootGUID", False, dump_guid},
		{"nTSecurityDescriptor", False, dump_sd},
		{"dnsRecord", False, dump_binary},
		{"objectSid", False, dump_sid},
		{"tokenGroups", False, dump_sid},
		{"tokenGroupsNoGCAcceptable", False, dump_sid},
		{"tokengroupsGlobalandUniversal", False, dump_sid},
		{"mS-DS-CreatorSID", False, dump_sid},
		{"msExchMailboxGuid", False, dump_guid},
		{NULL, True, NULL}
	};
	int i;

	if (!field) { /* must be end of an entry */
		printf("\n");
		return False;
	}

	for (i=0; handlers[i].name; i++) {
		if (StrCaseCmp(handlers[i].name, field) == 0) {
			if (!values) /* first time, indicate string or not */
				return handlers[i].string;
			handlers[i].handler(ads, field, (struct berval **) values);
			break;
		}
	}
	if (!handlers[i].name) {
		if (!values) /* first time, indicate string conversion */
			return True;
		dump_string(field, (char **)values);
	}
	return False;
}

/**
 * Dump a result from LDAP on stdout
 *  used for debugging
 * @param ads connection to ads server
 * @param res Results to dump
 **/

 void ads_dump(ADS_STRUCT *ads, LDAPMessage *res)
{
	ads_process_results(ads, res, ads_dump_field, NULL);
}

/**
 * Walk through results, calling a function for each entry found.
 *  The function receives a field name, a berval * array of values,
 *  and a data area passed through from the start.  The function is
 *  called once with null for field and values at the end of each
 *  entry.
 * @param ads connection to ads server
 * @param res Results to process
 * @param fn Function for processing each result
 * @param data_area user-defined area to pass to function
 **/
 void ads_process_results(ADS_STRUCT *ads, LDAPMessage *res,
			  bool (*fn)(ADS_STRUCT *, char *, void **, void *),
			  void *data_area)
{
	LDAPMessage *msg;
	TALLOC_CTX *ctx;
	size_t converted_size;

	if (!(ctx = talloc_init("ads_process_results")))
		return;

	for (msg = ads_first_entry(ads, res); msg; 
	     msg = ads_next_entry(ads, msg)) {
		char *utf8_field;
		BerElement *b;

		for (utf8_field=ldap_first_attribute(ads->ldap.ld,
						     (LDAPMessage *)msg,&b); 
		     utf8_field;
		     utf8_field=ldap_next_attribute(ads->ldap.ld,
						    (LDAPMessage *)msg,b)) {
			struct berval **ber_vals;
			char **str_vals, **utf8_vals;
			char *field;
			bool string; 

			if (!pull_utf8_talloc(ctx, &field, utf8_field,
					      &converted_size))
			{
				DEBUG(0,("ads_process_results: "
					 "pull_utf8_talloc failed: %s",
					 strerror(errno)));
			}

			string = fn(ads, field, NULL, data_area);

			if (string) {
				utf8_vals = ldap_get_values(ads->ldap.ld,
					       	 (LDAPMessage *)msg, field);
				str_vals = ads_pull_strvals(ctx, 
						  (const char **) utf8_vals);
				fn(ads, field, (void **) str_vals, data_area);
				ldap_value_free(utf8_vals);
			} else {
				ber_vals = ldap_get_values_len(ads->ldap.ld, 
						 (LDAPMessage *)msg, field);
				fn(ads, field, (void **) ber_vals, data_area);

				ldap_value_free_len(ber_vals);
			}
			ldap_memfree(utf8_field);
		}
		ber_free(b, 0);
		talloc_free_children(ctx);
		fn(ads, NULL, NULL, data_area); /* completed an entry */

	}
	talloc_destroy(ctx);
}

/**
 * count how many replies are in a LDAPMessage
 * @param ads connection to ads server
 * @param res Results to count
 * @return number of replies
 **/
int ads_count_replies(ADS_STRUCT *ads, void *res)
{
	return ldap_count_entries(ads->ldap.ld, (LDAPMessage *)res);
}

/**
 * pull the first entry from a ADS result
 * @param ads connection to ads server
 * @param res Results of search
 * @return first entry from result
 **/
 LDAPMessage *ads_first_entry(ADS_STRUCT *ads, LDAPMessage *res)
{
	return ldap_first_entry(ads->ldap.ld, res);
}

/**
 * pull the next entry from a ADS result
 * @param ads connection to ads server
 * @param res Results of search
 * @return next entry from result
 **/
 LDAPMessage *ads_next_entry(ADS_STRUCT *ads, LDAPMessage *res)
{
	return ldap_next_entry(ads->ldap.ld, res);
}

/**
 * pull the first message from a ADS result
 * @param ads connection to ads server
 * @param res Results of search
 * @return first message from result
 **/
 LDAPMessage *ads_first_message(ADS_STRUCT *ads, LDAPMessage *res)
{
	return ldap_first_message(ads->ldap.ld, res);
}

/**
 * pull the next message from a ADS result
 * @param ads connection to ads server
 * @param res Results of search
 * @return next message from result
 **/
 LDAPMessage *ads_next_message(ADS_STRUCT *ads, LDAPMessage *res)
{
	return ldap_next_message(ads->ldap.ld, res);
}

/**
 * pull a single string from a ADS result
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX to use for allocating result string
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @return Result string in talloc context
 **/
 char *ads_pull_string(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, LDAPMessage *msg,
		       const char *field)
{
	char **values;
	char *ret = NULL;
	char *ux_string;
	size_t converted_size;

	values = ldap_get_values(ads->ldap.ld, msg, field);
	if (!values)
		return NULL;

	if (values[0] && pull_utf8_talloc(mem_ctx, &ux_string, values[0],
					  &converted_size))
	{
		ret = ux_string;
	}
	ldap_value_free(values);
	return ret;
}

/**
 * pull an array of strings from a ADS result
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX to use for allocating result string
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @return Result strings in talloc context
 **/
 char **ads_pull_strings(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
			 LDAPMessage *msg, const char *field,
			 size_t *num_values)
{
	char **values;
	char **ret = NULL;
	int i;
	size_t converted_size;

	values = ldap_get_values(ads->ldap.ld, msg, field);
	if (!values)
		return NULL;

	*num_values = ldap_count_values(values);

	ret = TALLOC_ARRAY(mem_ctx, char *, *num_values + 1);
	if (!ret) {
		ldap_value_free(values);
		return NULL;
	}

	for (i=0;i<*num_values;i++) {
		if (!pull_utf8_talloc(mem_ctx, &ret[i], values[i],
				      &converted_size))
		{
			ldap_value_free(values);
			return NULL;
		}
	}
	ret[i] = NULL;

	ldap_value_free(values);
	return ret;
}

/**
 * pull an array of strings from a ADS result 
 *  (handle large multivalue attributes with range retrieval)
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX to use for allocating result string
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @param current_strings strings returned by a previous call to this function
 * @param next_attribute The next query should ask for this attribute
 * @param num_values How many values did we get this time?
 * @param more_values Are there more values to get?
 * @return Result strings in talloc context
 **/
 char **ads_pull_strings_range(ADS_STRUCT *ads, 
			       TALLOC_CTX *mem_ctx,
			       LDAPMessage *msg, const char *field,
			       char **current_strings,
			       const char **next_attribute,
			       size_t *num_strings,
			       bool *more_strings)
{
	char *attr;
	char *expected_range_attrib, *range_attr;
	BerElement *ptr = NULL;
	char **strings;
	char **new_strings;
	size_t num_new_strings;
	unsigned long int range_start;
	unsigned long int range_end;

	/* we might have been given the whole lot anyway */
	if ((strings = ads_pull_strings(ads, mem_ctx, msg, field, num_strings))) {
		*more_strings = False;
		return strings;
	}

	expected_range_attrib = talloc_asprintf(mem_ctx, "%s;Range=", field);

	/* look for Range result */
	for (attr = ldap_first_attribute(ads->ldap.ld, (LDAPMessage *)msg, &ptr); 
	     attr; 
	     attr = ldap_next_attribute(ads->ldap.ld, (LDAPMessage *)msg, ptr)) {
		/* we ignore the fact that this is utf8, as all attributes are ascii... */
		if (strnequal(attr, expected_range_attrib, strlen(expected_range_attrib))) {
			range_attr = attr;
			break;
		}
		ldap_memfree(attr);
	}
	if (!attr) {
		ber_free(ptr, 0);
		/* nothing here - this field is just empty */
		*more_strings = False;
		return NULL;
	}

	if (sscanf(&range_attr[strlen(expected_range_attrib)], "%lu-%lu", 
		   &range_start, &range_end) == 2) {
		*more_strings = True;
	} else {
		if (sscanf(&range_attr[strlen(expected_range_attrib)], "%lu-*", 
			   &range_start) == 1) {
			*more_strings = False;
		} else {
			DEBUG(1, ("ads_pull_strings_range:  Cannot parse Range attriubte (%s)\n", 
				  range_attr));
			ldap_memfree(range_attr);
			*more_strings = False;
			return NULL;
		}
	}

	if ((*num_strings) != range_start) {
		DEBUG(1, ("ads_pull_strings_range: Range attribute (%s) doesn't start at %u, but at %lu"
			  " - aborting range retreival\n",
			  range_attr, (unsigned int)(*num_strings) + 1, range_start));
		ldap_memfree(range_attr);
		*more_strings = False;
		return NULL;
	}

	new_strings = ads_pull_strings(ads, mem_ctx, msg, range_attr, &num_new_strings);

	if (*more_strings && ((*num_strings + num_new_strings) != (range_end + 1))) {
		DEBUG(1, ("ads_pull_strings_range: Range attribute (%s) tells us we have %lu "
			  "strings in this bunch, but we only got %lu - aborting range retreival\n",
			  range_attr, (unsigned long int)range_end - range_start + 1, 
			  (unsigned long int)num_new_strings));
		ldap_memfree(range_attr);
		*more_strings = False;
		return NULL;
	}

	strings = TALLOC_REALLOC_ARRAY(mem_ctx, current_strings, char *,
				 *num_strings + num_new_strings);

	if (strings == NULL) {
		ldap_memfree(range_attr);
		*more_strings = False;
		return NULL;
	}

	if (new_strings && num_new_strings) {
		memcpy(&strings[*num_strings], new_strings,
		       sizeof(*new_strings) * num_new_strings);
	}

	(*num_strings) += num_new_strings;

	if (*more_strings) {
		*next_attribute = talloc_asprintf(mem_ctx,
						  "%s;range=%d-*", 
						  field,
						  (int)*num_strings);

		if (!*next_attribute) {
			DEBUG(1, ("talloc_asprintf for next attribute failed!\n"));
			ldap_memfree(range_attr);
			*more_strings = False;
			return NULL;
		}
	}

	ldap_memfree(range_attr);

	return strings;
}

/**
 * pull a single uint32 from a ADS result
 * @param ads connection to ads server
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @param v Pointer to int to store result
 * @return boolean inidicating success
*/
 bool ads_pull_uint32(ADS_STRUCT *ads, LDAPMessage *msg, const char *field,
		      uint32 *v)
{
	char **values;

	values = ldap_get_values(ads->ldap.ld, msg, field);
	if (!values)
		return False;
	if (!values[0]) {
		ldap_value_free(values);
		return False;
	}

	*v = atoi(values[0]);
	ldap_value_free(values);
	return True;
}

/**
 * pull a single objectGUID from an ADS result
 * @param ads connection to ADS server
 * @param msg results of search
 * @param guid 37-byte area to receive text guid
 * @return boolean indicating success
 **/
 bool ads_pull_guid(ADS_STRUCT *ads, LDAPMessage *msg, struct GUID *guid)
{
	DATA_BLOB blob;
	NTSTATUS status;

	if (!smbldap_talloc_single_blob(talloc_tos(), ads->ldap.ld, msg, "objectGUID",
					&blob)) {
		return false;
	}

	status = GUID_from_ndr_blob(&blob, guid);
	talloc_free(blob.data);
	return NT_STATUS_IS_OK(status);
}


/**
 * pull a single struct dom_sid from a ADS result
 * @param ads connection to ads server
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @param sid Pointer to sid to store result
 * @return boolean inidicating success
*/
 bool ads_pull_sid(ADS_STRUCT *ads, LDAPMessage *msg, const char *field,
		   struct dom_sid *sid)
{
	return smbldap_pull_sid(ads->ldap.ld, msg, field, sid);
}

/**
 * pull an array of struct dom_sids from a ADS result
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX for allocating sid array
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @param sids pointer to sid array to allocate
 * @return the count of SIDs pulled
 **/
 int ads_pull_sids(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
		   LDAPMessage *msg, const char *field, struct dom_sid **sids)
{
	struct berval **values;
	bool ret;
	int count, i;

	values = ldap_get_values_len(ads->ldap.ld, msg, field);

	if (!values)
		return 0;

	for (i=0; values[i]; i++)
		/* nop */ ;

	if (i) {
		(*sids) = TALLOC_ARRAY(mem_ctx, struct dom_sid, i);
		if (!(*sids)) {
			ldap_value_free_len(values);
			return 0;
		}
	} else {
		(*sids) = NULL;
	}

	count = 0;
	for (i=0; values[i]; i++) {
		ret = sid_parse(values[i]->bv_val, values[i]->bv_len, &(*sids)[count]);
		if (ret) {
			DEBUG(10, ("pulling SID: %s\n",
				   sid_string_dbg(&(*sids)[count])));
			count++;
		}
	}

	ldap_value_free_len(values);
	return count;
}

/**
 * pull a struct security_descriptor from a ADS result
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX for allocating sid array
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @param sd Pointer to *struct security_descriptor to store result (talloc()ed)
 * @return boolean inidicating success
*/
 bool ads_pull_sd(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
		  LDAPMessage *msg, const char *field,
		  struct security_descriptor **sd)
{
	struct berval **values;
	bool ret = true;

	values = ldap_get_values_len(ads->ldap.ld, msg, field);

	if (!values) return false;

	if (values[0]) {
		NTSTATUS status;
		status = unmarshall_sec_desc(mem_ctx,
					     (uint8 *)values[0]->bv_val,
					     values[0]->bv_len, sd);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("unmarshall_sec_desc failed: %s\n",
				  nt_errstr(status)));
			ret = false;
		}
	}

	ldap_value_free_len(values);
	return ret;
}

/* 
 * in order to support usernames longer than 21 characters we need to 
 * use both the sAMAccountName and the userPrincipalName attributes 
 * It seems that not all users have the userPrincipalName attribute set
 *
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX for allocating sid array
 * @param msg Results of search
 * @return the username
 */
 char *ads_pull_username(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx,
			 LDAPMessage *msg)
{
#if 0	/* JERRY */
	char *ret, *p;

	/* lookup_name() only works on the sAMAccountName to 
	   returning the username portion of userPrincipalName
	   breaks winbindd_getpwnam() */

	ret = ads_pull_string(ads, mem_ctx, msg, "userPrincipalName");
	if (ret && (p = strchr_m(ret, '@'))) {
		*p = 0;
		return ret;
	}
#endif
	return ads_pull_string(ads, mem_ctx, msg, "sAMAccountName");
}


/**
 * find the update serial number - this is the core of the ldap cache
 * @param ads connection to ads server
 * @param ads connection to ADS server
 * @param usn Pointer to retrieved update serial number
 * @return status of search
 **/
ADS_STATUS ads_USN(ADS_STRUCT *ads, uint32 *usn)
{
	const char *attrs[] = {"highestCommittedUSN", NULL};
	ADS_STATUS status;
	LDAPMessage *res;

	status = ads_do_search_retry(ads, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) 
		return status;

	if (ads_count_replies(ads, res) != 1) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_RESULTS_RETURNED);
	}

	if (!ads_pull_uint32(ads, res, "highestCommittedUSN", usn)) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_ATTRIBUTE);
	}

	ads_msgfree(ads, res);
	return ADS_SUCCESS;
}

/* parse a ADS timestring - typical string is
   '20020917091222.0Z0' which means 09:12.22 17th September
   2002, timezone 0 */
static time_t ads_parse_time(const char *str)
{
	struct tm tm;

	ZERO_STRUCT(tm);

	if (sscanf(str, "%4d%2d%2d%2d%2d%2d", 
		   &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
		   &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
		return 0;
	}
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	return timegm(&tm);
}

/********************************************************************
********************************************************************/

ADS_STATUS ads_current_time(ADS_STRUCT *ads)
{
	const char *attrs[] = {"currentTime", NULL};
	ADS_STATUS status;
	LDAPMessage *res;
	char *timestr;
	TALLOC_CTX *ctx;
	ADS_STRUCT *ads_s = ads;

	if (!(ctx = talloc_init("ads_current_time"))) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

        /* establish a new ldap tcp session if necessary */

	if ( !ads->ldap.ld ) {
		if ( (ads_s = ads_init( ads->server.realm, ads->server.workgroup, 
			ads->server.ldap_server )) == NULL )
		{
			goto done;
		}
		ads_s->auth.flags = ADS_AUTH_ANON_BIND;
		status = ads_connect( ads_s );
		if ( !ADS_ERR_OK(status))
			goto done;
	}

	status = ads_do_search(ads_s, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) {
		goto done;
	}

	timestr = ads_pull_string(ads_s, ctx, res, "currentTime");
	if (!timestr) {
		ads_msgfree(ads_s, res);
		status = ADS_ERROR(LDAP_NO_RESULTS_RETURNED);
		goto done;
	}

	/* but save the time and offset in the original ADS_STRUCT */	

	ads->config.current_time = ads_parse_time(timestr);

	if (ads->config.current_time != 0) {
		ads->auth.time_offset = ads->config.current_time - time(NULL);
		DEBUG(4,("time offset is %d seconds\n", ads->auth.time_offset));
	}

	ads_msgfree(ads, res);

	status = ADS_SUCCESS;

done:
	/* free any temporary ads connections */
	if ( ads_s != ads ) {
		ads_destroy( &ads_s );
	}
	talloc_destroy(ctx);

	return status;
}

/********************************************************************
********************************************************************/

ADS_STATUS ads_domain_func_level(ADS_STRUCT *ads, uint32 *val)
{
	const char *attrs[] = {"domainFunctionality", NULL};
	ADS_STATUS status;
	LDAPMessage *res;
	ADS_STRUCT *ads_s = ads;

	*val = DS_DOMAIN_FUNCTION_2000;

        /* establish a new ldap tcp session if necessary */

	if ( !ads->ldap.ld ) {
		if ( (ads_s = ads_init( ads->server.realm, ads->server.workgroup, 
			ads->server.ldap_server )) == NULL )
		{
			status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
			goto done;
		}
		ads_s->auth.flags = ADS_AUTH_ANON_BIND;
		status = ads_connect( ads_s );
		if ( !ADS_ERR_OK(status))
			goto done;
	}

	/* If the attribute does not exist assume it is a Windows 2000 
	   functional domain */

	status = ads_do_search(ads_s, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) {
		if ( status.err.rc == LDAP_NO_SUCH_ATTRIBUTE ) {
			status = ADS_SUCCESS;
		}
		goto done;
	}

	if ( !ads_pull_uint32(ads_s, res, "domainFunctionality", val) ) {
		DEBUG(5,("ads_domain_func_level: Failed to pull the domainFunctionality attribute.\n"));
	}
	DEBUG(3,("ads_domain_func_level: %d\n", *val));


	ads_msgfree(ads, res);

done:
	/* free any temporary ads connections */
	if ( ads_s != ads ) {
		ads_destroy( &ads_s );
	}

	return status;
}

/**
 * find the domain sid for our domain
 * @param ads connection to ads server
 * @param sid Pointer to domain sid
 * @return status of search
 **/
ADS_STATUS ads_domain_sid(ADS_STRUCT *ads, struct dom_sid *sid)
{
	const char *attrs[] = {"objectSid", NULL};
	LDAPMessage *res;
	ADS_STATUS rc;

	rc = ads_do_search_retry(ads, ads->config.bind_path, LDAP_SCOPE_BASE, "(objectclass=*)", 
			   attrs, &res);
	if (!ADS_ERR_OK(rc)) return rc;
	if (!ads_pull_sid(ads, res, "objectSid", sid)) {
		ads_msgfree(ads, res);
		return ADS_ERROR_SYSTEM(ENOENT);
	}
	ads_msgfree(ads, res);

	return ADS_SUCCESS;
}

/**
 * find our site name 
 * @param ads connection to ads server
 * @param mem_ctx Pointer to talloc context
 * @param site_name Pointer to the sitename
 * @return status of search
 **/
ADS_STATUS ads_site_dn(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, const char **site_name)
{
	ADS_STATUS status;
	LDAPMessage *res;
	const char *dn, *service_name;
	const char *attrs[] = { "dsServiceName", NULL };

	status = ads_do_search(ads, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	service_name = ads_pull_string(ads, mem_ctx, res, "dsServiceName");
	if (service_name == NULL) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_RESULTS_RETURNED);
	}

	ads_msgfree(ads, res);

	/* go up three levels */
	dn = ads_parent_dn(ads_parent_dn(ads_parent_dn(service_name)));
	if (dn == NULL) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	*site_name = talloc_strdup(mem_ctx, dn);
	if (*site_name == NULL) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	return status;
	/*
	dsServiceName: CN=NTDS Settings,CN=W2K3DC,CN=Servers,CN=Default-First-Site-Name,CN=Sites,CN=Configuration,DC=ber,DC=suse,DC=de
	*/						 
}

/**
 * find the site dn where a machine resides
 * @param ads connection to ads server
 * @param mem_ctx Pointer to talloc context
 * @param computer_name name of the machine
 * @param site_name Pointer to the sitename
 * @return status of search
 **/
ADS_STATUS ads_site_dn_for_machine(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, const char *computer_name, const char **site_dn)
{
	ADS_STATUS status;
	LDAPMessage *res;
	const char *parent, *filter;
	char *config_context = NULL;
	char *dn;

	/* shortcut a query */
	if (strequal(computer_name, ads->config.ldap_server_name)) {
		return ads_site_dn(ads, mem_ctx, site_dn);
	}

	status = ads_config_path(ads, mem_ctx, &config_context);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	filter = talloc_asprintf(mem_ctx, "(cn=%s)", computer_name);
	if (filter == NULL) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	status = ads_do_search(ads, config_context, LDAP_SCOPE_SUBTREE, 
			       filter, NULL, &res);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	if (ads_count_replies(ads, res) != 1) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	dn = ads_get_dn(ads, mem_ctx, res);
	if (dn == NULL) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	/* go up three levels */
	parent = ads_parent_dn(ads_parent_dn(ads_parent_dn(dn)));
	if (parent == NULL) {
		ads_msgfree(ads, res);
		TALLOC_FREE(dn);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	*site_dn = talloc_strdup(mem_ctx, parent);
	if (*site_dn == NULL) {
		ads_msgfree(ads, res);
		TALLOC_FREE(dn);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	TALLOC_FREE(dn);
	ads_msgfree(ads, res);

	return status;
}

/**
 * get the upn suffixes for a domain
 * @param ads connection to ads server
 * @param mem_ctx Pointer to talloc context
 * @param suffixes Pointer to an array of suffixes
 * @param num_suffixes Pointer to the number of suffixes
 * @return status of search
 **/
ADS_STATUS ads_upn_suffixes(ADS_STRUCT *ads, TALLOC_CTX *mem_ctx, char ***suffixes, size_t *num_suffixes)
{
	ADS_STATUS status;
	LDAPMessage *res;
	const char *base;
	char *config_context = NULL;
	const char *attrs[] = { "uPNSuffixes", NULL };

	status = ads_config_path(ads, mem_ctx, &config_context);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	base = talloc_asprintf(mem_ctx, "cn=Partitions,%s", config_context);
	if (base == NULL) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	status = ads_search_dn(ads, &res, base, attrs);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	if (ads_count_replies(ads, res) != 1) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_SUCH_OBJECT);
	}

	(*suffixes) = ads_pull_strings(ads, mem_ctx, res, "uPNSuffixes", num_suffixes);
	if ((*suffixes) == NULL) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	ads_msgfree(ads, res);

	return status;
}

/**
 * get the joinable ous for a domain
 * @param ads connection to ads server
 * @param mem_ctx Pointer to talloc context
 * @param ous Pointer to an array of ous
 * @param num_ous Pointer to the number of ous
 * @return status of search
 **/
ADS_STATUS ads_get_joinable_ous(ADS_STRUCT *ads,
				TALLOC_CTX *mem_ctx,
				char ***ous,
				size_t *num_ous)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	LDAPMessage *msg = NULL;
	const char *attrs[] = { "dn", NULL };
	int count = 0;

	status = ads_search(ads, &res,
			    "(|(objectClass=domain)(objectclass=organizationalUnit))",
			    attrs);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	count = ads_count_replies(ads, res);
	if (count < 1) {
		ads_msgfree(ads, res);
		return ADS_ERROR(LDAP_NO_RESULTS_RETURNED);
	}

	for (msg = ads_first_entry(ads, res); msg;
	     msg = ads_next_entry(ads, msg)) {

		char *dn = NULL;

		dn = ads_get_dn(ads, talloc_tos(), msg);
		if (!dn) {
			ads_msgfree(ads, res);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		if (!add_string_to_array(mem_ctx, dn,
					 (const char ***)ous,
					 (int *)num_ous)) {
			TALLOC_FREE(dn);
			ads_msgfree(ads, res);
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		TALLOC_FREE(dn);
	}

	ads_msgfree(ads, res);

	return status;
}


/**
 * pull a struct dom_sid from an extended dn string
 * @param mem_ctx TALLOC_CTX
 * @param extended_dn string
 * @param flags string type of extended_dn
 * @param sid pointer to a struct dom_sid
 * @return NT_STATUS_OK on success,
 *	   NT_INVALID_PARAMETER on error,
 *	   NT_STATUS_NOT_FOUND if no SID present
 **/
ADS_STATUS ads_get_sid_from_extended_dn(TALLOC_CTX *mem_ctx,
					const char *extended_dn,
					enum ads_extended_dn_flags flags,
					struct dom_sid *sid)
{
	char *p, *q, *dn;

	if (!extended_dn) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}

	/* otherwise extended_dn gets stripped off */
	if ((dn = talloc_strdup(mem_ctx, extended_dn)) == NULL) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}
	/*
	 * ADS_EXTENDED_DN_HEX_STRING:
	 * <GUID=238e1963cb390f4bb032ba0105525a29>;<SID=010500000000000515000000bb68c8fd6b61b427572eb04556040000>;CN=gd,OU=berlin,OU=suse,DC=ber,DC=suse,DC=de
	 *
	 * ADS_EXTENDED_DN_STRING (only with w2k3):
	 * <GUID=63198e23-39cb-4b0f-b032-ba0105525a29>;<SID=S-1-5-21-4257769659-666132843-1169174103-1110>;CN=gd,OU=berlin,OU=suse,DC=ber,DC=suse,DC=de
	 *
	 * Object with no SID, such as an Exchange Public Folder
	 * <GUID=28907fb4bdf6854993e7f0a10b504e7c>;CN=public,CN=Microsoft Exchange System Objects,DC=sd2k3ms,DC=west,DC=isilon,DC=com
	 */

	p = strchr(dn, ';');
	if (!p) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}

	if (strncmp(p, ";<SID=", strlen(";<SID=")) != 0) {
		DEBUG(5,("No SID present in extended dn\n"));
		return ADS_ERROR_NT(NT_STATUS_NOT_FOUND);
	}

	p += strlen(";<SID=");

	q = strchr(p, '>');
	if (!q) {
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}

	*q = '\0';

	DEBUG(100,("ads_get_sid_from_extended_dn: sid string is %s\n", p));

	switch (flags) {

	case ADS_EXTENDED_DN_STRING:
		if (!string_to_sid(sid, p)) {
			return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
		}
		break;
	case ADS_EXTENDED_DN_HEX_STRING: {
		fstring buf;
		size_t buf_len;

		buf_len = strhex_to_str(buf, sizeof(buf), p, strlen(p));
		if (buf_len == 0) {
			return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
		}

		if (!sid_parse(buf, buf_len, sid)) {
			DEBUG(10,("failed to parse sid\n"));
			return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
		}
		break;
		}
	default:
		DEBUG(10,("unknown extended dn format\n"));
		return ADS_ERROR_NT(NT_STATUS_INVALID_PARAMETER);
	}

	return ADS_ERROR_NT(NT_STATUS_OK);
}

/**
 * pull an array of struct dom_sids from a ADS result
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX for allocating sid array
 * @param msg Results of search
 * @param field Attribute to retrieve
 * @param flags string type of extended_dn
 * @param sids pointer to sid array to allocate
 * @return the count of SIDs pulled
 **/
 int ads_pull_sids_from_extendeddn(ADS_STRUCT *ads,
				   TALLOC_CTX *mem_ctx,
				   LDAPMessage *msg,
				   const char *field,
				   enum ads_extended_dn_flags flags,
				   struct dom_sid **sids)
{
	int i;
	ADS_STATUS rc;
	size_t dn_count, ret_count = 0;
	char **dn_strings;

	if ((dn_strings = ads_pull_strings(ads, mem_ctx, msg, field,
					   &dn_count)) == NULL) {
		return 0;
	}

	(*sids) = TALLOC_ZERO_ARRAY(mem_ctx, struct dom_sid, dn_count + 1);
	if (!(*sids)) {
		TALLOC_FREE(dn_strings);
		return 0;
	}

	for (i=0; i<dn_count; i++) {
		rc = ads_get_sid_from_extended_dn(mem_ctx, dn_strings[i],
						  flags, &(*sids)[i]);
		if (!ADS_ERR_OK(rc)) {
			if (NT_STATUS_EQUAL(ads_ntstatus(rc),
			    NT_STATUS_NOT_FOUND)) {
				continue;
			}
			else {
				TALLOC_FREE(*sids);
				TALLOC_FREE(dn_strings);
				return 0;
			}
		}
		ret_count++;
	}

	TALLOC_FREE(dn_strings);

	return ret_count;
}

/********************************************************************
********************************************************************/

char* ads_get_dnshostname( ADS_STRUCT *ads, TALLOC_CTX *ctx, const char *machine_name )
{
	LDAPMessage *res = NULL;
	ADS_STATUS status;
	int count = 0;
	char *name = NULL;

	status = ads_find_machine_acct(ads, &res, global_myname());
	if (!ADS_ERR_OK(status)) {
		DEBUG(0,("ads_get_dnshostname: Failed to find account for %s\n",
			global_myname()));
		goto out;
	}

	if ( (count = ads_count_replies(ads, res)) != 1 ) {
		DEBUG(1,("ads_get_dnshostname: %d entries returned!\n", count));
		goto out;
	}

	if ( (name = ads_pull_string(ads, ctx, res, "dNSHostName")) == NULL ) {
		DEBUG(0,("ads_get_dnshostname: No dNSHostName attribute!\n"));
	}

out:
	ads_msgfree(ads, res);

	return name;
}

/********************************************************************
********************************************************************/

char* ads_get_upn( ADS_STRUCT *ads, TALLOC_CTX *ctx, const char *machine_name )
{
	LDAPMessage *res = NULL;
	ADS_STATUS status;
	int count = 0;
	char *name = NULL;

	status = ads_find_machine_acct(ads, &res, machine_name);
	if (!ADS_ERR_OK(status)) {
		DEBUG(0,("ads_get_upn: Failed to find account for %s\n",
			global_myname()));
		goto out;
	}

	if ( (count = ads_count_replies(ads, res)) != 1 ) {
		DEBUG(1,("ads_get_upn: %d entries returned!\n", count));
		goto out;
	}

	if ( (name = ads_pull_string(ads, ctx, res, "userPrincipalName")) == NULL ) {
		DEBUG(2,("ads_get_upn: No userPrincipalName attribute!\n"));
	}

out:
	ads_msgfree(ads, res);

	return name;
}

/********************************************************************
********************************************************************/

char* ads_get_samaccountname( ADS_STRUCT *ads, TALLOC_CTX *ctx, const char *machine_name )
{
	LDAPMessage *res = NULL;
	ADS_STATUS status;
	int count = 0;
	char *name = NULL;

	status = ads_find_machine_acct(ads, &res, global_myname());
	if (!ADS_ERR_OK(status)) {
		DEBUG(0,("ads_get_dnshostname: Failed to find account for %s\n",
			global_myname()));
		goto out;
	}

	if ( (count = ads_count_replies(ads, res)) != 1 ) {
		DEBUG(1,("ads_get_dnshostname: %d entries returned!\n", count));
		goto out;
	}

	if ( (name = ads_pull_string(ads, ctx, res, "sAMAccountName")) == NULL ) {
		DEBUG(0,("ads_get_dnshostname: No sAMAccountName attribute!\n"));
	}

out:
	ads_msgfree(ads, res);

	return name;
}

#if 0

   SAVED CODE - we used to join via ldap - remember how we did this. JRA.

/**
 * Join a machine to a realm
 *  Creates the machine account and sets the machine password
 * @param ads connection to ads server
 * @param machine name of host to add
 * @param org_unit Organizational unit to place machine in
 * @return status of join
 **/
ADS_STATUS ads_join_realm(ADS_STRUCT *ads, const char *machine_name,
			uint32 account_type, const char *org_unit)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	char *machine;

	/* machine name must be lowercase */
	machine = SMB_STRDUP(machine_name);
	strlower_m(machine);

	/*
	status = ads_find_machine_acct(ads, (void **)&res, machine);
	if (ADS_ERR_OK(status) && ads_count_replies(ads, res) == 1) {
		DEBUG(0, ("Host account for %s already exists - deleting old account\n", machine));
		status = ads_leave_realm(ads, machine);
		if (!ADS_ERR_OK(status)) {
			DEBUG(0, ("Failed to delete host '%s' from the '%s' realm.\n",
				machine, ads->config.realm));
			return status;
		}
	}
	*/
	status = ads_add_machine_acct(ads, machine, account_type, org_unit);
	if (!ADS_ERR_OK(status)) {
		DEBUG(0, ("ads_join_realm: ads_add_machine_acct failed (%s): %s\n", machine, ads_errstr(status)));
		SAFE_FREE(machine);
		return status;
	}

	status = ads_find_machine_acct(ads, (void **)(void *)&res, machine);
	if (!ADS_ERR_OK(status)) {
		DEBUG(0, ("ads_join_realm: Host account test failed for machine %s\n", machine));
		SAFE_FREE(machine);
		return status;
	}

	SAFE_FREE(machine);
	ads_msgfree(ads, res);

	return status;
}
#endif

/**
 * Delete a machine from the realm
 * @param ads connection to ads server
 * @param hostname Machine to remove
 * @return status of delete
 **/
ADS_STATUS ads_leave_realm(ADS_STRUCT *ads, const char *hostname)
{
	ADS_STATUS status;
	void *msg;
	LDAPMessage *res;
	char *hostnameDN, *host;
	int rc;
	LDAPControl ldap_control;
	LDAPControl  * pldap_control[2] = {NULL, NULL};

	pldap_control[0] = &ldap_control;
	memset(&ldap_control, 0, sizeof(LDAPControl));
	ldap_control.ldctl_oid = (char *)LDAP_SERVER_TREE_DELETE_OID;

	/* hostname must be lowercase */
	host = SMB_STRDUP(hostname);
	strlower_m(host);

	status = ads_find_machine_acct(ads, &res, host);
	if (!ADS_ERR_OK(status)) {
		DEBUG(0, ("Host account for %s does not exist.\n", host));
		SAFE_FREE(host);
		return status;
	}

	msg = ads_first_entry(ads, res);
	if (!msg) {
		SAFE_FREE(host);
		return ADS_ERROR_SYSTEM(ENOENT);
	}

	hostnameDN = ads_get_dn(ads, talloc_tos(), (LDAPMessage *)msg);
	if (hostnameDN == NULL) {
		SAFE_FREE(host);
		return ADS_ERROR_SYSTEM(ENOENT);
	}

	rc = ldap_delete_ext_s(ads->ldap.ld, hostnameDN, pldap_control, NULL);
	if (rc) {
		DEBUG(3,("ldap_delete_ext_s failed with error code %d\n", rc));
	}else {
		DEBUG(3,("ldap_delete_ext_s succeeded with error code %d\n", rc));
	}

	if (rc != LDAP_SUCCESS) {
		const char *attrs[] = { "cn", NULL };
		LDAPMessage *msg_sub;

		/* we only search with scope ONE, we do not expect any further
		 * objects to be created deeper */

		status = ads_do_search_retry(ads, hostnameDN,
					     LDAP_SCOPE_ONELEVEL,
					     "(objectclass=*)", attrs, &res);

		if (!ADS_ERR_OK(status)) {
			SAFE_FREE(host);
			TALLOC_FREE(hostnameDN);
			return status;
		}

		for (msg_sub = ads_first_entry(ads, res); msg_sub;
			msg_sub = ads_next_entry(ads, msg_sub)) {

			char *dn = NULL;

			if ((dn = ads_get_dn(ads, talloc_tos(), msg_sub)) == NULL) {
				SAFE_FREE(host);
				TALLOC_FREE(hostnameDN);
				return ADS_ERROR(LDAP_NO_MEMORY);
			}

			status = ads_del_dn(ads, dn);
			if (!ADS_ERR_OK(status)) {
				DEBUG(3,("failed to delete dn %s: %s\n", dn, ads_errstr(status)));
				SAFE_FREE(host);
				TALLOC_FREE(dn);
				TALLOC_FREE(hostnameDN);
				return status;
			}

			TALLOC_FREE(dn);
		}

		/* there should be no subordinate objects anymore */
		status = ads_do_search_retry(ads, hostnameDN,
					     LDAP_SCOPE_ONELEVEL,
					     "(objectclass=*)", attrs, &res);

		if (!ADS_ERR_OK(status) || ( (ads_count_replies(ads, res)) > 0 ) ) {
			SAFE_FREE(host);
			TALLOC_FREE(hostnameDN);
			return status;
		}

		/* delete hostnameDN now */
		status = ads_del_dn(ads, hostnameDN);
		if (!ADS_ERR_OK(status)) {
			SAFE_FREE(host);
			DEBUG(3,("failed to delete dn %s: %s\n", hostnameDN, ads_errstr(status)));
			TALLOC_FREE(hostnameDN);
			return status;
		}
	}

	TALLOC_FREE(hostnameDN);

	status = ads_find_machine_acct(ads, &res, host);
	if (ADS_ERR_OK(status) && ads_count_replies(ads, res) == 1) {
		DEBUG(3, ("Failed to remove host account.\n"));
		SAFE_FREE(host);
		return status;
	}

	SAFE_FREE(host);
	return status;
}

/**
 * pull all token-sids from an LDAP dn
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX for allocating sid array
 * @param dn of LDAP object
 * @param user_sid pointer to struct dom_sid (objectSid)
 * @param primary_group_sid pointer to struct dom_sid (self composed)
 * @param sids pointer to sid array to allocate
 * @param num_sids counter of SIDs pulled
 * @return status of token query
 **/
 ADS_STATUS ads_get_tokensids(ADS_STRUCT *ads,
			      TALLOC_CTX *mem_ctx,
			      const char *dn,
			      struct dom_sid *user_sid,
			      struct dom_sid *primary_group_sid,
			      struct dom_sid **sids,
			      size_t *num_sids)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	int count = 0;
	size_t tmp_num_sids;
	struct dom_sid *tmp_sids;
	struct dom_sid tmp_user_sid;
	struct dom_sid tmp_primary_group_sid;
	uint32 pgid;
	const char *attrs[] = {
		"objectSid",
		"tokenGroups",
		"primaryGroupID",
		NULL
	};

	status = ads_search_retry_dn(ads, &res, dn, attrs);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	count = ads_count_replies(ads, res);
	if (count != 1) {
		ads_msgfree(ads, res);
		return ADS_ERROR_LDAP(LDAP_NO_SUCH_OBJECT);
	}

	if (!ads_pull_sid(ads, res, "objectSid", &tmp_user_sid)) {
		ads_msgfree(ads, res);
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	if (!ads_pull_uint32(ads, res, "primaryGroupID", &pgid)) {
		ads_msgfree(ads, res);
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	{
		/* hack to compose the primary group sid without knowing the
		 * domsid */

		struct dom_sid domsid;

		sid_copy(&domsid, &tmp_user_sid);

		if (!sid_split_rid(&domsid, NULL)) {
			ads_msgfree(ads, res);
			return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
		}

		if (!sid_compose(&tmp_primary_group_sid, &domsid, pgid)) {
			ads_msgfree(ads, res);
			return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
		}
	}

	tmp_num_sids = ads_pull_sids(ads, mem_ctx, res, "tokenGroups", &tmp_sids);

	if (tmp_num_sids == 0 || !tmp_sids) {
		ads_msgfree(ads, res);
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	if (num_sids) {
		*num_sids = tmp_num_sids;
	}

	if (sids) {
		*sids = tmp_sids;
	}

	if (user_sid) {
		*user_sid = tmp_user_sid;
	}

	if (primary_group_sid) {
		*primary_group_sid = tmp_primary_group_sid;
	}

	DEBUG(10,("ads_get_tokensids: returned %d sids\n", (int)tmp_num_sids + 2));

	ads_msgfree(ads, res);
	return ADS_ERROR_LDAP(LDAP_SUCCESS);
}

/**
 * Find a sAMAccoutName in LDAP
 * @param ads connection to ads server
 * @param mem_ctx TALLOC_CTX for allocating sid array
 * @param samaccountname to search
 * @param uac_ret uint32 pointer userAccountControl attribute value
 * @param dn_ret pointer to dn
 * @return status of token query
 **/
ADS_STATUS ads_find_samaccount(ADS_STRUCT *ads,
			       TALLOC_CTX *mem_ctx,
			       const char *samaccountname,
			       uint32 *uac_ret,
			       const char **dn_ret)
{
	ADS_STATUS status;
	const char *attrs[] = { "userAccountControl", NULL };
	const char *filter;
	LDAPMessage *res = NULL;
	char *dn = NULL;
	uint32 uac = 0;

	filter = talloc_asprintf(mem_ctx, "(&(objectclass=user)(sAMAccountName=%s))",
		samaccountname);
	if (filter == NULL) {
		status = ADS_ERROR_NT(NT_STATUS_NO_MEMORY);
		goto out;
	}

	status = ads_do_search_all(ads, ads->config.bind_path,
				   LDAP_SCOPE_SUBTREE,
				   filter, attrs, &res);

	if (!ADS_ERR_OK(status)) {
		goto out;
	}

	if (ads_count_replies(ads, res) != 1) {
		status = ADS_ERROR(LDAP_NO_RESULTS_RETURNED);
		goto out;
	}

	dn = ads_get_dn(ads, talloc_tos(), res);
	if (dn == NULL) {
		status = ADS_ERROR(LDAP_NO_MEMORY);
		goto out;
	}

	if (!ads_pull_uint32(ads, res, "userAccountControl", &uac)) {
		status = ADS_ERROR(LDAP_NO_SUCH_ATTRIBUTE);
		goto out;
	}

	if (uac_ret) {
		*uac_ret = uac;
	}

	if (dn_ret) {
		*dn_ret = talloc_strdup(mem_ctx, dn);
		if (!*dn_ret) {
			status = ADS_ERROR(LDAP_NO_MEMORY);
			goto out;
		}
	}
 out:
	TALLOC_FREE(dn);
	ads_msgfree(ads, res);

	return status;
}

/**
 * find our configuration path 
 * @param ads connection to ads server
 * @param mem_ctx Pointer to talloc context
 * @param config_path Pointer to the config path
 * @return status of search
 **/
ADS_STATUS ads_config_path(ADS_STRUCT *ads, 
			   TALLOC_CTX *mem_ctx, 
			   char **config_path)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	const char *config_context = NULL;
	const char *attrs[] = { "configurationNamingContext", NULL };

	status = ads_do_search(ads, "", LDAP_SCOPE_BASE, 
			       "(objectclass=*)", attrs, &res);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	config_context = ads_pull_string(ads, mem_ctx, res, 
					 "configurationNamingContext");
	ads_msgfree(ads, res);
	if (!config_context) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	if (config_path) {
		*config_path = talloc_strdup(mem_ctx, config_context);
		if (!*config_path) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

	return ADS_ERROR(LDAP_SUCCESS);
}

/**
 * find the displayName of an extended right 
 * @param ads connection to ads server
 * @param config_path The config path
 * @param mem_ctx Pointer to talloc context
 * @param GUID struct of the rightsGUID
 * @return status of search
 **/
const char *ads_get_extended_right_name_by_guid(ADS_STRUCT *ads, 
						const char *config_path, 
						TALLOC_CTX *mem_ctx, 
						const struct GUID *rights_guid)
{
	ADS_STATUS rc;
	LDAPMessage *res = NULL;
	char *expr = NULL;
	const char *attrs[] = { "displayName", NULL };
	const char *result = NULL;
	const char *path;

	if (!ads || !mem_ctx || !rights_guid) {
		goto done;
	}

	expr = talloc_asprintf(mem_ctx, "(rightsGuid=%s)", 
			       GUID_string(mem_ctx, rights_guid));
	if (!expr) {
		goto done;
	}

	path = talloc_asprintf(mem_ctx, "cn=Extended-Rights,%s", config_path);
	if (!path) {
		goto done;
	}

	rc = ads_do_search_retry(ads, path, LDAP_SCOPE_SUBTREE, 
				 expr, attrs, &res);
	if (!ADS_ERR_OK(rc)) {
		goto done;
	}

	if (ads_count_replies(ads, res) != 1) {
		goto done;
	}

	result = ads_pull_string(ads, mem_ctx, res, "displayName");

 done:
	ads_msgfree(ads, res);
	return result;
}

/**
 * verify or build and verify an account ou
 * @param mem_ctx Pointer to talloc context
 * @param ads connection to ads server
 * @param account_ou
 * @return status of search
 **/

ADS_STATUS ads_check_ou_dn(TALLOC_CTX *mem_ctx,
			   ADS_STRUCT *ads,
			   const char **account_ou)
{
	char **exploded_dn;
	const char *name;
	char *ou_string;

	exploded_dn = ldap_explode_dn(*account_ou, 0);
	if (exploded_dn) {
		ldap_value_free(exploded_dn);
		return ADS_SUCCESS;
	}

	ou_string = ads_ou_string(ads, *account_ou);
	if (!ou_string) {
		return ADS_ERROR_LDAP(LDAP_INVALID_DN_SYNTAX);
	}

	name = talloc_asprintf(mem_ctx, "%s,%s", ou_string,
			       ads->config.bind_path);
	SAFE_FREE(ou_string);

	if (!name) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	exploded_dn = ldap_explode_dn(name, 0);
	if (!exploded_dn) {
		return ADS_ERROR_LDAP(LDAP_INVALID_DN_SYNTAX);
	}
	ldap_value_free(exploded_dn);

	*account_ou = name;
	return ADS_SUCCESS;
}

#endif
