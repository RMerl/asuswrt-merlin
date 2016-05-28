/* 
   Unix SMB/CIFS implementation.
   kerberos locator plugin
   Copyright (C) Guenther Deschner 2007
   
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

#if defined(HAVE_KRB5) && defined(HAVE_KRB5_LOCATE_PLUGIN_H)

#include <krb5/locate_plugin.h>

static const char *get_service_from_locate_service_type(enum locate_service_type svc)
{
	switch (svc) {
		case locate_service_kdc:
		case locate_service_master_kdc:
			return "88";
		case locate_service_kadmin:
		case locate_service_krb524:
			/* not supported */
			return NULL;
		case locate_service_kpasswd:
			return "464";
		default:
			break;
	}
	return NULL;

}

static const char *locate_service_type_name(enum locate_service_type svc)
{
	switch (svc) {
		case locate_service_kdc:
			return "locate_service_kdc";
		case locate_service_master_kdc:
			return "locate_service_master_kdc";
		case locate_service_kadmin:
			return "locate_service_kadmin";
		case locate_service_krb524:
			return "locate_service_krb524";
		case locate_service_kpasswd:
			return "locate_service_kpasswd";
		default:
			break;
	}
	return NULL;
}

static const char *socktype_name(int socktype)
{
	switch (socktype) {
		case SOCK_STREAM:
			return "SOCK_STREAM";
		case SOCK_DGRAM:
			return "SOCK_DGRAM";
		default:
			break;
	}
	return "unknown";
}

static const char *family_name(int family)
{
	switch (family) {
		case AF_UNSPEC:
			return "AF_UNSPEC";
		case AF_INET:
			return "AF_INET";
		case AF_INET6:
			return "AF_INET6";
		default:
			break;
	}
	return "unknown";
}

/**
 * Check input parameters, return KRB5_PLUGIN_NO_HANDLE for unsupported ones
 *
 * @param svc 
 * @param realm string
 * @param socktype integer
 * @param family integer
 *
 * @return integer.
 */

static int smb_krb5_locator_lookup_sanity_check(enum locate_service_type svc,
						const char *realm,
						int socktype,
						int family)
{
	if (!realm || strlen(realm) == 0) {
		return EINVAL;
	}

	switch (svc) {
		case locate_service_kdc:
		case locate_service_master_kdc:
		case locate_service_kpasswd:
			break;
		case locate_service_kadmin:
		case locate_service_krb524:
#ifdef KRB5_PLUGIN_NO_HANDLE
			return KRB5_PLUGIN_NO_HANDLE;
#else
			return KRB5_KDC_UNREACH; /* Heimdal */
#endif
		default:
			return EINVAL;
	}

	switch (family) {
		case AF_UNSPEC:
		case AF_INET:
			break;
		case AF_INET6: /* not yet */
#ifdef KRB5_PLUGIN_NO_HANDLE
			return KRB5_PLUGIN_NO_HANDLE;
#else
			return KRB5_KDC_UNREACH; /* Heimdal */
#endif
		default:
			return EINVAL;
	}

	switch (socktype) {
		case SOCK_STREAM:
		case SOCK_DGRAM:
		case 0: /* Heimdal uses that */
			break;
		default:
			return EINVAL;
	}

	return 0;
}

/**
 * Try to get addrinfo for a given host and call the krb5 callback
 *
 * @param name string
 * @param service string
 * @param in struct addrinfo hint
 * @param cbfunc krb5 callback function
 * @param cbdata void pointer cbdata
 *
 * @return krb5_error_code.
 */

static krb5_error_code smb_krb5_locator_call_cbfunc(const char *name, 
						    const char *service,
						    struct addrinfo *in,
						    int (*cbfunc)(void *, int, struct sockaddr *),
						    void *cbdata)
{
	struct addrinfo *out;
	int ret;
	int count = 3;

	while (count) {

		ret = getaddrinfo(name, service, in, &out);
		if (ret == 0) {
			break;
		}

		if (ret == EAI_AGAIN) {
			count--;
			continue;
		}

		DEBUG(10,("smb_krb5_locator_lookup: got ret: %s (%d)\n", 
			gai_strerror(ret), ret));
#ifdef KRB5_PLUGIN_NO_HANDLE
		return KRB5_PLUGIN_NO_HANDLE;
#else
		return KRB5_KDC_UNREACH; /* Heimdal */
#endif
	}

	ret = cbfunc(cbdata, out->ai_socktype, out->ai_addr);
	if (ret) {
		DEBUG(10,("smb_krb5_locator_lookup: failed to call callback: %s (%d)\n", 
			error_message(ret), ret));
	}

	freeaddrinfo(out);

	return ret;
}

/**
 * PUBLIC INTERFACE: locate init
 *
 * @param context krb5_context
 * @param privata_data pointer to private data pointer
 *
 * @return krb5_error_code.
 */

krb5_error_code smb_krb5_locator_init(krb5_context context, 
				      void **private_data)
{
	setup_logging("smb_krb5_locator", True);
	load_case_tables();
	lp_load(dyn_CONFIGFILE,True,False,False,True);

	DEBUG(10,("smb_krb5_locator_init: called\n"));

	return 0;
}

/**
 * PUBLIC INTERFACE: close locate
 *
 * @param private_data pointer to private data
 *
 * @return void.
 */

void smb_krb5_locator_close(void *private_data)
{
	DEBUG(10,("smb_krb5_locator_close: called\n"));

	/* gfree_all(); */
}

/**
 * PUBLIC INTERFACE: locate lookup
 *
 * @param private_data pointer to private data
 * @param svc enum locate_service_type.
 * @param realm string
 * @param socktype integer
 * @param family integer
 * @param cbfunc callback function to send back entries
 * @param cbdata void pointer to cbdata
 *
 * @return krb5_error_code.
 */

krb5_error_code smb_krb5_locator_lookup(void *private_data,
					enum locate_service_type svc,
					const char *realm,
					int socktype,
					int family,
					int (*cbfunc)(void *, int, struct sockaddr *),
					void *cbdata)
{
	NTSTATUS status;
	krb5_error_code ret;
	char *sitename = NULL;
	struct ip_service *ip_list;
	int count = 0;
	struct addrinfo aihints;
	char *saf_name = NULL;
	int i;

	DEBUG(10,("smb_krb5_locator_lookup: called for\n"));
	DEBUGADD(10,("\tsvc: %s (%d), realm: %s\n", 
		locate_service_type_name(svc), svc, realm));
	DEBUGADD(10,("\tsocktype: %s (%d), family: %s (%d)\n", 
		socktype_name(socktype), socktype,
	        family_name(family), family));

	ret = smb_krb5_locator_lookup_sanity_check(svc, realm, socktype, family);
	if (ret) {
		DEBUG(10,("smb_krb5_locator_lookup: returning ret: %s (%d)\n", 
			error_message(ret), ret));
		return ret;
	}

	/* first try to fetch from SAF cache */

	saf_name = saf_fetch(realm);
	if (!saf_name || strlen(saf_name) == 0) {
		DEBUG(10,("smb_krb5_locator_lookup: no SAF name stored for %s\n", 
			realm));
		goto find_kdc;
	}

	DEBUG(10,("smb_krb5_locator_lookup: got %s for %s from SAF cache\n", 
		saf_name, realm));

	ZERO_STRUCT(aihints);
	
	aihints.ai_family = family;
	aihints.ai_socktype = socktype;

	ret = smb_krb5_locator_call_cbfunc(saf_name, 
					  get_service_from_locate_service_type(svc), 
					  &aihints, 
					  cbfunc, cbdata);
	if (ret) {
		return ret;
	}

	return 0;

 find_kdc:

	/* now try to find via site-aware DNS SRV query */

	sitename = sitename_fetch(realm);
	status = get_kdc_list(realm, sitename, &ip_list, &count);

	/* if we didn't found any KDCs on our site go to the main list */

	if (NT_STATUS_IS_OK(status) && sitename && (count == 0)) {
		SAFE_FREE(ip_list);
		SAFE_FREE(sitename);
		status = get_kdc_list(realm, NULL, &ip_list, &count);
	}

	SAFE_FREE(sitename);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("smb_krb5_locator_lookup: got %s (%s)\n",
			nt_errstr(status), 
			error_message(nt_status_to_krb5(status))));
#ifdef KRB5_PLUGIN_NO_HANDLE
		return KRB5_PLUGIN_NO_HANDLE;
#else
		return KRB5_KDC_UNREACH; /* Heimdal */
#endif
	}

	for (i=0; i<count; i++) {

		const char *host = NULL;
		const char *port = NULL;

		ZERO_STRUCT(aihints);

		aihints.ai_family = family;
		aihints.ai_socktype = socktype;

		host = inet_ntoa(ip_list[i].ip);
		port = get_service_from_locate_service_type(svc);

		ret = smb_krb5_locator_call_cbfunc(host,
						  port,
						  &aihints, 
						  cbfunc, cbdata);
		if (ret) {
			/* got error */
			break;
		}
	}

	SAFE_FREE(ip_list);

	return ret;
}

#ifdef HEIMDAL_KRB5_LOCATE_PLUGIN_H
#define SMB_KRB5_LOCATOR_SYMBOL_NAME resolve /* Heimdal */
#else
#define SMB_KRB5_LOCATOR_SYMBOL_NAME service_locator /* MIT */
#endif

const krb5plugin_service_locate_ftable SMB_KRB5_LOCATOR_SYMBOL_NAME = {
	0, /* version */
	smb_krb5_locator_init,
	smb_krb5_locator_close,
	smb_krb5_locator_lookup,
};

#endif
