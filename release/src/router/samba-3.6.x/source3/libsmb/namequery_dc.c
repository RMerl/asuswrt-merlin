/* 
   Unix SMB/CIFS implementation.

   Winbind daemon connection manager

   Copyright (C) Tim Potter 2001
   Copyright (C) Andrew Bartlett 2002
   Copyright (C) Gerald Carter 2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "includes.h"
#include "libads/sitename_cache.h"
#include "ads.h"
#include "../librpc/gen_ndr/nbt.h"

/**********************************************************************
 Is this our primary domain ?
**********************************************************************/

#ifdef HAVE_KRB5
static bool is_our_primary_domain(const char *domain)
{
	int role = lp_server_role();

	if ((role == ROLE_DOMAIN_MEMBER) && strequal(lp_workgroup(), domain)) {
		return True;
	} else if (strequal(get_global_sam_name(), domain)) {
		return True;
	}
	return False;
}
#endif

/**************************************************************************
 Find the name and IP address for a server in the realm/domain
 *************************************************************************/

static bool ads_dc_name(const char *domain,
			const char *realm,
			struct sockaddr_storage *dc_ss,
			fstring srv_name)
{
	ADS_STRUCT *ads;
	char *sitename;
	int i;
	char addr[INET6_ADDRSTRLEN];

	if (!realm && strequal(domain, lp_workgroup())) {
		realm = lp_realm();
	}

	sitename = sitename_fetch(realm);

	/* Try this 3 times then give up. */
	for( i =0 ; i < 3; i++) {
		ads = ads_init(realm, domain, NULL);
		if (!ads) {
			SAFE_FREE(sitename);
			return False;
		}

		DEBUG(4,("ads_dc_name: domain=%s\n", domain));

#ifdef HAVE_ADS
		/* we don't need to bind, just connect */
		ads->auth.flags |= ADS_AUTH_NO_BIND;
		ads_connect(ads);
#endif

		if (!ads->config.realm) {
			SAFE_FREE(sitename);
			ads_destroy(&ads);
			return False;
		}

		/* Now we've found a server, see if our sitename
		   has changed. If so, we need to re-do the DNS query
		   to ensure we only find servers in our site. */

		if (stored_sitename_changed(realm, sitename)) {
			SAFE_FREE(sitename);
			sitename = sitename_fetch(realm);
			ads_destroy(&ads);
			/* Ensure we don't cache the DC we just connected to. */
			namecache_delete(realm, 0x1C);
			namecache_delete(domain, 0x1C);
			continue;
		}

#ifdef HAVE_ADS
		if (is_our_primary_domain(domain) && (ads->config.flags & NBT_SERVER_KDC)) {
			if (ads_closest_dc(ads)) {
				/* We're going to use this KDC for this realm/domain.
				   If we are using sites, then force the krb5 libs
				   to use this KDC. */

				create_local_private_krb5_conf_for_domain(realm,
									domain,
									sitename,
									&ads->ldap.ss,
									ads->config.ldap_server_name);
			} else {
				create_local_private_krb5_conf_for_domain(realm,
									domain,
									NULL,
									&ads->ldap.ss,
									ads->config.ldap_server_name);
			}
		}
#endif
		break;
	}

	if (i == 3) {
		DEBUG(1,("ads_dc_name: sitename (now \"%s\") keeps changing ???\n",
			sitename ? sitename : ""));
		SAFE_FREE(sitename);
		return False;
	}

	SAFE_FREE(sitename);

	fstrcpy(srv_name, ads->config.ldap_server_name);
	strupper_m(srv_name);
#ifdef HAVE_ADS
	*dc_ss = ads->ldap.ss;
#else
	zero_sockaddr(dc_ss);
#endif
	ads_destroy(&ads);

	print_sockaddr(addr, sizeof(addr), dc_ss);
	DEBUG(4,("ads_dc_name: using server='%s' IP=%s\n",
		 srv_name, addr));

	return True;
}

/****************************************************************************
 Utility function to return the name of a DC. The name is guaranteed to be
 valid since we have already done a name_status_find on it
 ***************************************************************************/

static bool rpc_dc_name(const char *domain,
			fstring srv_name,
			struct sockaddr_storage *ss_out)
{
	struct ip_service *ip_list = NULL;
	struct sockaddr_storage dc_ss;
	int count, i;
	NTSTATUS result;
	char addr[INET6_ADDRSTRLEN];

	/* get a list of all domain controllers */

	if (!NT_STATUS_IS_OK(get_sorted_dc_list(domain, NULL, &ip_list, &count,
						False))) {
		DEBUG(3, ("Could not look up dc's for domain %s\n", domain));
		return False;
	}

	/* Remove the entry we've already failed with (should be the PDC). */

	for (i = 0; i < count; i++) {
		if (is_zero_addr(&ip_list[i].ss))
			continue;

		if (name_status_find(domain, 0x1c, 0x20, &ip_list[i].ss, srv_name)) {
			result = check_negative_conn_cache( domain, srv_name );
			if ( NT_STATUS_IS_OK(result) ) {
				dc_ss = ip_list[i].ss;
				goto done;
			}
		}
	}

	SAFE_FREE(ip_list);

	/* No-one to talk to )-: */
	return False;		/* Boo-hoo */

 done:
	/* We have the netbios name and IP address of a domain controller.
	   Ideally we should sent a SAMLOGON request to determine whether
	   the DC is alive and kicking.  If we can catch a dead DC before
	   performing a cli_connect() we can avoid a 30-second timeout. */

	print_sockaddr(addr, sizeof(addr), &dc_ss);
	DEBUG(3, ("rpc_dc_name: Returning DC %s (%s) for domain %s\n", srv_name,
		  addr, domain));

	*ss_out = dc_ss;
	SAFE_FREE(ip_list);

	return True;
}

/**********************************************************************
 wrapper around ads and rpc methods of finds DC's
**********************************************************************/

bool get_dc_name(const char *domain,
		const char *realm,
		fstring srv_name,
		struct sockaddr_storage *ss_out)
{
	struct sockaddr_storage dc_ss;
	bool ret;
	bool our_domain = False;

	zero_sockaddr(&dc_ss);

	ret = False;

	if ( strequal(lp_workgroup(), domain) || strequal(lp_realm(), realm) )
		our_domain = True;

	/* always try to obey what the admin specified in smb.conf
	   (for the local domain) */

	if ( (our_domain && lp_security()==SEC_ADS) || realm ) {
		ret = ads_dc_name(domain, realm, &dc_ss, srv_name);
	}

	if (!domain) {
		/* if we have only the realm we can't do anything else */
		return False;
	}

	if (!ret) {
		/* fall back on rpc methods if the ADS methods fail */
		ret = rpc_dc_name(domain, srv_name, &dc_ss);
	}

	*ss_out = dc_ss;

	return ret;
}
