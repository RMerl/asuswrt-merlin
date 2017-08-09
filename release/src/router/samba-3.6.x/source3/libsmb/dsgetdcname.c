/*
   Unix SMB/CIFS implementation.

   dsgetdcname

   Copyright (C) Gerald Carter 2006
   Copyright (C) Guenther Deschner 2007-2008

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
#include "libads/sitename_cache.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "libads/cldap.h"
#include "libads/dns.h"
#include "libsmb/clidgram.h"

/* 15 minutes */
#define DSGETDCNAME_CACHE_TTL	60*15

struct ip_service_name {
	struct sockaddr_storage ss;
	unsigned port;
	const char *hostname;
};

static NTSTATUS make_dc_info_from_cldap_reply(TALLOC_CTX *mem_ctx,
					      uint32_t flags,
					      struct sockaddr_storage *ss,
					      struct NETLOGON_SAM_LOGON_RESPONSE_EX *r,
					      struct netr_DsRGetDCNameInfo **info);

/****************************************************************
****************************************************************/

void debug_dsdcinfo_flags(int lvl, uint32_t flags)
{
	DEBUG(lvl,("debug_dsdcinfo_flags: 0x%08x\n\t", flags));

	if (flags & DS_FORCE_REDISCOVERY)
		DEBUGADD(lvl,("DS_FORCE_REDISCOVERY "));
	if (flags & 0x000000002)
		DEBUGADD(lvl,("0x00000002 "));
	if (flags & 0x000000004)
		DEBUGADD(lvl,("0x00000004 "));
	if (flags & 0x000000008)
		DEBUGADD(lvl,("0x00000008 "));
	if (flags & DS_DIRECTORY_SERVICE_REQUIRED)
		DEBUGADD(lvl,("DS_DIRECTORY_SERVICE_REQUIRED "));
	if (flags & DS_DIRECTORY_SERVICE_PREFERRED)
		DEBUGADD(lvl,("DS_DIRECTORY_SERVICE_PREFERRED "));
	if (flags & DS_GC_SERVER_REQUIRED)
		DEBUGADD(lvl,("DS_GC_SERVER_REQUIRED "));
	if (flags & DS_PDC_REQUIRED)
		DEBUGADD(lvl,("DS_PDC_REQUIRED "));
	if (flags & DS_BACKGROUND_ONLY)
		DEBUGADD(lvl,("DS_BACKGROUND_ONLY "));
	if (flags & DS_IP_REQUIRED)
		DEBUGADD(lvl,("DS_IP_REQUIRED "));
	if (flags & DS_KDC_REQUIRED)
		DEBUGADD(lvl,("DS_KDC_REQUIRED "));
	if (flags & DS_TIMESERV_REQUIRED)
		DEBUGADD(lvl,("DS_TIMESERV_REQUIRED "));
	if (flags & DS_WRITABLE_REQUIRED)
		DEBUGADD(lvl,("DS_WRITABLE_REQUIRED "));
	if (flags & DS_GOOD_TIMESERV_PREFERRED)
		DEBUGADD(lvl,("DS_GOOD_TIMESERV_PREFERRED "));
	if (flags & DS_AVOID_SELF)
		DEBUGADD(lvl,("DS_AVOID_SELF "));
	if (flags & DS_ONLY_LDAP_NEEDED)
		DEBUGADD(lvl,("DS_ONLY_LDAP_NEEDED "));
	if (flags & DS_IS_FLAT_NAME)
		DEBUGADD(lvl,("DS_IS_FLAT_NAME "));
	if (flags & DS_IS_DNS_NAME)
		DEBUGADD(lvl,("DS_IS_DNS_NAME "));
	if (flags & 0x00040000)
		DEBUGADD(lvl,("0x00040000 "));
	if (flags & 0x00080000)
		DEBUGADD(lvl,("0x00080000 "));
	if (flags & 0x00100000)
		DEBUGADD(lvl,("0x00100000 "));
	if (flags & 0x00200000)
		DEBUGADD(lvl,("0x00200000 "));
	if (flags & 0x00400000)
		DEBUGADD(lvl,("0x00400000 "));
	if (flags & 0x00800000)
		DEBUGADD(lvl,("0x00800000 "));
	if (flags & 0x01000000)
		DEBUGADD(lvl,("0x01000000 "));
	if (flags & 0x02000000)
		DEBUGADD(lvl,("0x02000000 "));
	if (flags & 0x04000000)
		DEBUGADD(lvl,("0x04000000 "));
	if (flags & 0x08000000)
		DEBUGADD(lvl,("0x08000000 "));
	if (flags & 0x10000000)
		DEBUGADD(lvl,("0x10000000 "));
	if (flags & 0x20000000)
		DEBUGADD(lvl,("0x20000000 "));
	if (flags & DS_RETURN_DNS_NAME)
		DEBUGADD(lvl,("DS_RETURN_DNS_NAME "));
	if (flags & DS_RETURN_FLAT_NAME)
		DEBUGADD(lvl,("DS_RETURN_FLAT_NAME "));
	if (flags)
		DEBUGADD(lvl,("\n"));
}

/****************************************************************
****************************************************************/

static char *dsgetdcname_cache_key(TALLOC_CTX *mem_ctx, const char *domain)
{
	if (!domain) {
		return NULL;
	}

	return talloc_asprintf_strupper_m(mem_ctx, "DSGETDCNAME/DOMAIN/%s",
					  domain);
}

/****************************************************************
****************************************************************/

static NTSTATUS dsgetdcname_cache_delete(TALLOC_CTX *mem_ctx,
					const char *domain_name)
{
	char *key;

	key = dsgetdcname_cache_key(mem_ctx, domain_name);
	if (!key) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!gencache_del(key)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS dsgetdcname_cache_store(TALLOC_CTX *mem_ctx,
					const char *domain_name,
					const DATA_BLOB *blob)
{
	time_t expire_time;
	char *key;
	bool ret = false;

	key = dsgetdcname_cache_key(mem_ctx, domain_name);
	if (!key) {
		return NT_STATUS_NO_MEMORY;
	}

	expire_time = time(NULL) + DSGETDCNAME_CACHE_TTL;

	ret = gencache_set_data_blob(key, blob, expire_time);

	return ret ? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

/****************************************************************
****************************************************************/

static NTSTATUS store_cldap_reply(TALLOC_CTX *mem_ctx,
				  uint32_t flags,
				  struct sockaddr_storage *ss,
				  uint32_t nt_version,
				  struct NETLOGON_SAM_LOGON_RESPONSE_EX *r)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	char addr[INET6_ADDRSTRLEN];

       print_sockaddr(addr, sizeof(addr), ss);

	/* FIXME */
	r->sockaddr_size = 0x10; /* the w32 winsock addr size */
	r->sockaddr.sockaddr_family = 2; /* AF_INET */
	r->sockaddr.pdc_ip = talloc_strdup(mem_ctx, addr);

	ndr_err = ndr_push_struct_blob(&blob, mem_ctx, r,
		       (ndr_push_flags_fn_t)ndr_push_NETLOGON_SAM_LOGON_RESPONSE_EX);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (r->domain_name) {
		status = dsgetdcname_cache_store(mem_ctx, r->domain_name, &blob);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (r->client_site) {
			sitename_store(r->domain_name, r->client_site);
		}
	}
	if (r->dns_domain) {
		status = dsgetdcname_cache_store(mem_ctx, r->dns_domain, &blob);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (r->client_site) {
			sitename_store(r->dns_domain, r->client_site);
		}
	}

	status = NT_STATUS_OK;

 done:
	data_blob_free(&blob);

	return status;
}

/****************************************************************
****************************************************************/

static uint32_t get_cldap_reply_server_flags(struct netlogon_samlogon_response *r,
					     uint32_t nt_version)
{
	switch (nt_version & 0x0000001f) {
		case 0:
		case 1:
		case 16:
		case 17:
			return 0;
		case 2:
		case 3:
		case 18:
		case 19:
			return r->data.nt5.server_type;
		case 4:
		case 5:
		case 6:
		case 7:
			return r->data.nt5_ex.server_type;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			return r->data.nt5_ex.server_type;
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
		case 28:
			return r->data.nt5_ex.server_type;
		case 29:
		case 30:
		case 31:
			return r->data.nt5_ex.server_type;
		default:
			return 0;
	}
}

/****************************************************************
****************************************************************/

#define RETURN_ON_FALSE(x) if (!(x)) return false;

static bool check_cldap_reply_required_flags(uint32_t ret_flags,
					     uint32_t req_flags)
{
	if (ret_flags == 0) {
		return true;
	}

	if (req_flags & DS_PDC_REQUIRED)
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_PDC);

	if (req_flags & DS_GC_SERVER_REQUIRED)
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_GC);

	if (req_flags & DS_ONLY_LDAP_NEEDED)
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_LDAP);

	if ((req_flags & DS_DIRECTORY_SERVICE_REQUIRED) ||
	    (req_flags & DS_DIRECTORY_SERVICE_PREFERRED))
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_DS);

	if (req_flags & DS_KDC_REQUIRED)
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_KDC);

	if (req_flags & DS_TIMESERV_REQUIRED)
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_TIMESERV);

	if (req_flags & DS_WRITABLE_REQUIRED)
		RETURN_ON_FALSE(ret_flags & NBT_SERVER_WRITABLE);

	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS dsgetdcname_cache_fetch(TALLOC_CTX *mem_ctx,
					const char *domain_name,
					const struct GUID *domain_guid,
					uint32_t flags,
					struct netr_DsRGetDCNameInfo **info_p)
{
	char *key;
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	struct netr_DsRGetDCNameInfo *info;
	struct NETLOGON_SAM_LOGON_RESPONSE_EX r;
	NTSTATUS status;

	key = dsgetdcname_cache_key(mem_ctx, domain_name);
	if (!key) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!gencache_get_data_blob(key, &blob, NULL, NULL)) {
		return NT_STATUS_NOT_FOUND;
	}

	info = TALLOC_ZERO_P(mem_ctx, struct netr_DsRGetDCNameInfo);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, &r,
		      (ndr_pull_flags_fn_t)ndr_pull_NETLOGON_SAM_LOGON_RESPONSE_EX);

	data_blob_free(&blob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		dsgetdcname_cache_delete(mem_ctx, domain_name);
		return ndr_map_error2ntstatus(ndr_err);
	}

	status = make_dc_info_from_cldap_reply(mem_ctx, flags, NULL,
					       &r, &info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(netr_DsRGetDCNameInfo, info);
	}

	/* check flags */
	if (!check_cldap_reply_required_flags(info->dc_flags, flags)) {
		DEBUG(10,("invalid flags\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if ((flags & DS_IP_REQUIRED) &&
	    (info->dc_address_type != DS_ADDRESS_TYPE_INET)) {
	    	return NT_STATUS_INVALID_PARAMETER_MIX;
	}

	*info_p = info;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS dsgetdcname_cached(TALLOC_CTX *mem_ctx,
				   struct messaging_context *msg_ctx,
				   const char *domain_name,
				   const struct GUID *domain_guid,
				   uint32_t flags,
				   const char *site_name,
				   struct netr_DsRGetDCNameInfo **info)
{
	NTSTATUS status;

	status = dsgetdcname_cache_fetch(mem_ctx, domain_name, domain_guid,
					 flags, info);
	if (!NT_STATUS_IS_OK(status)
	    && !NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		DEBUG(10,("dsgetdcname_cached: cache fetch failed with: %s\n",
			nt_errstr(status)));
		return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	}

	if (flags & DS_BACKGROUND_ONLY) {
		return status;
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		struct netr_DsRGetDCNameInfo *dc_info;

		status = dsgetdcname(mem_ctx, msg_ctx, domain_name,
				     domain_guid, site_name,
				     flags | DS_FORCE_REDISCOVERY,
				     &dc_info);

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		*info = dc_info;
	}

	return status;
}

/****************************************************************
****************************************************************/

static bool check_allowed_required_flags(uint32_t flags,
					 const char *site_name)
{
	uint32_t return_type = flags & (DS_RETURN_FLAT_NAME|DS_RETURN_DNS_NAME);
	uint32_t offered_type = flags & (DS_IS_FLAT_NAME|DS_IS_DNS_NAME);
	uint32_t query_type = flags & (DS_BACKGROUND_ONLY|DS_FORCE_REDISCOVERY);

	/* FIXME: check for DSGETDC_VALID_FLAGS and check for excluse bits
	 * (DS_PDC_REQUIRED, DS_KDC_REQUIRED, DS_GC_SERVER_REQUIRED) */

	debug_dsdcinfo_flags(10, flags);

	if ((flags & DS_TRY_NEXTCLOSEST_SITE) && site_name) {
		return false;
	}

	if (return_type == (DS_RETURN_FLAT_NAME|DS_RETURN_DNS_NAME)) {
		return false;
	}

	if (offered_type == (DS_IS_DNS_NAME|DS_IS_FLAT_NAME)) {
		return false;
	}

	if (query_type == (DS_BACKGROUND_ONLY|DS_FORCE_REDISCOVERY)) {
		return false;
	}

#if 0
	if ((flags & DS_RETURN_DNS_NAME) && (!(flags & DS_IP_REQUIRED))) {
		printf("gd: here5 \n");
		return false;
	}
#endif
	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS discover_dc_netbios(TALLOC_CTX *mem_ctx,
				    const char *domain_name,
				    uint32_t flags,
				    struct ip_service_name **returned_dclist,
				    int *returned_count)
{
	NTSTATUS status;
	enum nbt_name_type name_type = NBT_NAME_LOGON;
	struct ip_service *iplist;
	int i;
	struct ip_service_name *dclist = NULL;
	int count;

	*returned_dclist = NULL;
	*returned_count = 0;

	if (lp_disable_netbios()) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	if (flags & DS_PDC_REQUIRED) {
		name_type = NBT_NAME_PDC;
	}

	status = internal_resolve_name(domain_name, name_type, NULL,
				       &iplist, &count,
				       "lmhosts wins bcast");
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("discover_dc_netbios: failed to find DC\n"));
		return status;
	}

	dclist = TALLOC_ZERO_ARRAY(mem_ctx, struct ip_service_name, count);
	if (!dclist) {
		SAFE_FREE(iplist);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<count; i++) {

		char addr[INET6_ADDRSTRLEN];
		struct ip_service_name *r = &dclist[i];

		print_sockaddr(addr, sizeof(addr),
			       &iplist[i].ss);

		r->ss	= iplist[i].ss;
		r->port = iplist[i].port;
		r->hostname = talloc_strdup(mem_ctx, addr);
		if (!r->hostname) {
			SAFE_FREE(iplist);
			return NT_STATUS_NO_MEMORY;
		}

	}

	*returned_dclist = dclist;
	*returned_count = count;
	SAFE_FREE(iplist);

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS discover_dc_dns(TALLOC_CTX *mem_ctx,
				const char *domain_name,
				const struct GUID *domain_guid,
				uint32_t flags,
				const char *site_name,
				struct ip_service_name **returned_dclist,
				int *return_count)
{
	int i, j;
	NTSTATUS status;
	struct dns_rr_srv *dcs = NULL;
	int numdcs = 0;
	int numaddrs = 0;
	struct ip_service_name *dclist = NULL;
	int count = 0;

	if (flags & DS_PDC_REQUIRED) {
		status = ads_dns_query_pdc(mem_ctx, domain_name,
					   &dcs, &numdcs);
	} else if (flags & DS_GC_SERVER_REQUIRED) {
		status = ads_dns_query_gcs(mem_ctx, domain_name, site_name,
					   &dcs, &numdcs);
	} else if (flags & DS_KDC_REQUIRED) {
		status = ads_dns_query_kdcs(mem_ctx, domain_name, site_name,
					    &dcs, &numdcs);
	} else if (flags & DS_DIRECTORY_SERVICE_REQUIRED) {
		status = ads_dns_query_dcs(mem_ctx, domain_name, site_name,
					   &dcs, &numdcs);
	} else if (domain_guid) {
		status = ads_dns_query_dcs_guid(mem_ctx, domain_name,
						domain_guid, &dcs, &numdcs);
	} else {
		status = ads_dns_query_dcs(mem_ctx, domain_name, site_name,
					   &dcs, &numdcs);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (numdcs == 0) {
		return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	}

	for (i=0;i<numdcs;i++) {
		numaddrs += MAX(dcs[i].num_ips,1);
	}

	dclist = TALLOC_ZERO_ARRAY(mem_ctx,
				   struct ip_service_name,
				   numaddrs);
	if (!dclist) {
		return NT_STATUS_NO_MEMORY;
	}

	/* now unroll the list of IP addresses */

	*return_count = 0;
	i = 0;
	j = 0;

	while ((i < numdcs) && (count < numaddrs)) {

		struct ip_service_name *r = &dclist[count];

		r->port = dcs[i].port;
		r->hostname = dcs[i].hostname;

		/* If we don't have an IP list for a name, lookup it up */

		if (!dcs[i].ss_s) {
			interpret_string_addr_prefer_ipv4(&r->ss,
						dcs[i].hostname, 0);
			i++;
			j = 0;
		} else {
			/* use the IP addresses from the SRV sresponse */

			if (j >= dcs[i].num_ips) {
				i++;
				j = 0;
				continue;
			}

			r->ss = dcs[i].ss_s[j];
			j++;
		}

		/* make sure it is a valid IP.  I considered checking the
		 * negative connection cache, but this is the wrong place for
		 * it.  Maybe only as a hac.  After think about it, if all of
		 * the IP addresses retuend from DNS are dead, what hope does a
		 * netbios name lookup have?  The standard reason for falling
		 * back to netbios lookups is that our DNS server doesn't know
		 * anything about the DC's   -- jerry */

		if (!is_zero_addr(&r->ss)) {
			count++;
			continue;
		}
	}

	*returned_dclist = dclist;
	*return_count = count;

	if (count > 0) {
		return NT_STATUS_OK;
	}

	return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
}

/****************************************************************
****************************************************************/

static NTSTATUS make_domain_controller_info(TALLOC_CTX *mem_ctx,
					    const char *dc_unc,
					    const char *dc_address,
					    uint32_t dc_address_type,
					    const struct GUID *domain_guid,
					    const char *domain_name,
					    const char *forest_name,
					    uint32_t flags,
					    const char *dc_site_name,
					    const char *client_site_name,
					    struct netr_DsRGetDCNameInfo **info_out)
{
	struct netr_DsRGetDCNameInfo *info;

	info = TALLOC_ZERO_P(mem_ctx, struct netr_DsRGetDCNameInfo);
	NT_STATUS_HAVE_NO_MEMORY(info);

	if (dc_unc) {
		info->dc_unc = talloc_strdup(mem_ctx, dc_unc);
		NT_STATUS_HAVE_NO_MEMORY(info->dc_unc);
	}

	if (dc_address) {
		if (!(dc_address[0] == '\\' && dc_address[1] == '\\')) {
			info->dc_address = talloc_asprintf(mem_ctx, "\\\\%s",
							   dc_address);
		} else {
			info->dc_address = talloc_strdup(mem_ctx, dc_address);
		}
		NT_STATUS_HAVE_NO_MEMORY(info->dc_address);
	}

	info->dc_address_type = dc_address_type;

	if (domain_guid) {
		info->domain_guid = *domain_guid;
	}

	if (domain_name) {
		info->domain_name = talloc_strdup(mem_ctx, domain_name);
		NT_STATUS_HAVE_NO_MEMORY(info->domain_name);
	}

	if (forest_name && *forest_name) {
		info->forest_name = talloc_strdup(mem_ctx, forest_name);
		NT_STATUS_HAVE_NO_MEMORY(info->forest_name);
		flags |= DS_DNS_FOREST_ROOT;
	}

	info->dc_flags = flags;

	if (dc_site_name) {
		info->dc_site_name = talloc_strdup(mem_ctx, dc_site_name);
		NT_STATUS_HAVE_NO_MEMORY(info->dc_site_name);
	}

	if (client_site_name) {
		info->client_site_name = talloc_strdup(mem_ctx,
						       client_site_name);
		NT_STATUS_HAVE_NO_MEMORY(info->client_site_name);
	}

	*info_out = info;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static void map_dc_and_domain_names(uint32_t flags,
				    const char *dc_name,
				    const char *domain_name,
				    const char *dns_dc_name,
				    const char *dns_domain_name,
				    uint32_t *dc_flags,
				    const char **hostname_p,
				    const char **domain_p)
{
	switch (flags & 0xf0000000) {
		case DS_RETURN_FLAT_NAME:
			if (dc_name && domain_name &&
			    *dc_name && *domain_name) {
				*hostname_p = dc_name;
				*domain_p = domain_name;
				break;
			}
		case DS_RETURN_DNS_NAME:
		default:
			if (dns_dc_name && dns_domain_name &&
			    *dns_dc_name && *dns_domain_name) {
				*hostname_p = dns_dc_name;
				*domain_p = dns_domain_name;
				*dc_flags |= DS_DNS_DOMAIN | DS_DNS_CONTROLLER;
				break;
			}
			if (dc_name && domain_name &&
			    *dc_name && *domain_name) {
				*hostname_p = dc_name;
				*domain_p = domain_name;
				break;
			}
	}
}

/****************************************************************
****************************************************************/

static NTSTATUS make_dc_info_from_cldap_reply(TALLOC_CTX *mem_ctx,
					      uint32_t flags,
					      struct sockaddr_storage *ss,
					      struct NETLOGON_SAM_LOGON_RESPONSE_EX *r,
					      struct netr_DsRGetDCNameInfo **info)
{
	const char *dc_hostname = NULL;
	const char *dc_domain_name = NULL;
	const char *dc_address = NULL;
	const char *dc_forest = NULL;
	uint32_t dc_address_type = 0;
	uint32_t dc_flags = 0;
	struct GUID *dc_domain_guid = NULL;
	const char *dc_server_site = NULL;
	const char *dc_client_site = NULL;

	char addr[INET6_ADDRSTRLEN];

	if (ss) {
		print_sockaddr(addr, sizeof(addr), ss);
		dc_address = addr;
		dc_address_type = DS_ADDRESS_TYPE_INET;
	}

	if (!ss && r->sockaddr.pdc_ip) {
		dc_address	= r->sockaddr.pdc_ip;
		dc_address_type	= DS_ADDRESS_TYPE_INET;
	} else {
		dc_address      = r->pdc_name;
		dc_address_type = DS_ADDRESS_TYPE_NETBIOS;
	}

	map_dc_and_domain_names(flags,
				r->pdc_name,
				r->domain_name,
				r->pdc_dns_name,
				r->dns_domain,
				&dc_flags,
				&dc_hostname,
				&dc_domain_name);

	dc_flags	|= r->server_type;
	dc_forest	= r->forest;
	dc_domain_guid	= &r->domain_uuid;
	dc_server_site	= r->server_site;
	dc_client_site	= r->client_site;

	return make_domain_controller_info(mem_ctx,
					   dc_hostname,
					   dc_address,
					   dc_address_type,
					   dc_domain_guid,
					   dc_domain_name,
					   dc_forest,
					   dc_flags,
					   dc_server_site,
					   dc_client_site,
					   info);
}

/****************************************************************
****************************************************************/

static uint32_t map_ds_flags_to_nt_version(uint32_t flags)
{
	uint32_t nt_version = 0;

	if (flags & DS_PDC_REQUIRED) {
		nt_version |= NETLOGON_NT_VERSION_PDC;
	}

	if (flags & DS_GC_SERVER_REQUIRED) {
		nt_version |= NETLOGON_NT_VERSION_GC;
	}

	if (flags & DS_TRY_NEXTCLOSEST_SITE) {
		nt_version |= NETLOGON_NT_VERSION_WITH_CLOSEST_SITE;
	}

	if (flags & DS_IP_REQUIRED) {
		nt_version |= NETLOGON_NT_VERSION_IP;
	}

	return nt_version;
}

/****************************************************************
****************************************************************/

static NTSTATUS process_dc_dns(TALLOC_CTX *mem_ctx,
			       const char *domain_name,
			       uint32_t flags,
			       struct ip_service_name *dclist,
			       int num_dcs,
			       struct netr_DsRGetDCNameInfo **info)
{
	int i = 0;
	bool valid_dc = false;
	struct netlogon_samlogon_response *r = NULL;
	uint32_t nt_version = NETLOGON_NT_VERSION_5 |
			      NETLOGON_NT_VERSION_5EX;
	uint32_t ret_flags = 0;
	NTSTATUS status;

	nt_version |= map_ds_flags_to_nt_version(flags);

	for (i=0; i<num_dcs; i++) {

		DEBUG(10,("LDAP ping to %s\n", dclist[i].hostname));

		if (ads_cldap_netlogon(mem_ctx, dclist[i].hostname,
					domain_name,
					nt_version,
					&r))
		{
			nt_version = r->ntver;
			ret_flags = get_cldap_reply_server_flags(r, nt_version);

			if (check_cldap_reply_required_flags(ret_flags, flags)) {
				valid_dc = true;
				break;
			}
		}

		continue;
	}

	if (!valid_dc) {
		return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	}

	status = make_dc_info_from_cldap_reply(mem_ctx, flags, &dclist[i].ss,
					       &r->data.nt5_ex, info);
	if (NT_STATUS_IS_OK(status)) {
		return store_cldap_reply(mem_ctx, flags, &dclist[i].ss,
					 nt_version, &r->data.nt5_ex);
	}

	return status;
}

/****************************************************************
****************************************************************/

/****************************************************************
****************************************************************/

static NTSTATUS process_dc_netbios(TALLOC_CTX *mem_ctx,
				   struct messaging_context *msg_ctx,
				   const char *domain_name,
				   uint32_t flags,
				   struct ip_service_name *dclist,
				   int num_dcs,
				   struct netr_DsRGetDCNameInfo **info)
{
	struct sockaddr_storage ss;
	struct ip_service ip_list;
	enum nbt_name_type name_type = NBT_NAME_LOGON;
	NTSTATUS status;
	int i;
	const char *dc_name = NULL;
	fstring tmp_dc_name;
	struct netlogon_samlogon_response *r = NULL;
	bool store_cache = false;
	uint32_t nt_version = NETLOGON_NT_VERSION_1 |
			      NETLOGON_NT_VERSION_5 |
			      NETLOGON_NT_VERSION_5EX_WITH_IP;

	if (msg_ctx == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (flags & DS_PDC_REQUIRED) {
		name_type = NBT_NAME_PDC;
	}

	nt_version |= map_ds_flags_to_nt_version(flags);

	DEBUG(10,("process_dc_netbios\n"));

	for (i=0; i<num_dcs; i++) {
		uint16_t val;
		int dgm_id;

		generate_random_buffer((uint8_t *)&val, 2);
		dgm_id = val;

		ip_list.ss = dclist[i].ss;
		ip_list.port = 0;

		if (!interpret_string_addr_prefer_ipv4(&ss, dclist[i].hostname, AI_NUMERICHOST)) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		status = nbt_getdc(msg_ctx, 10, &dclist[i].ss, domain_name,
				   NULL, nt_version,
				   mem_ctx, &nt_version, &dc_name, &r);
		if (NT_STATUS_IS_OK(status)) {
			store_cache = true;
			namecache_store(dc_name, NBT_NAME_SERVER, 1, &ip_list);
			goto make_reply;
		}

		if (name_status_find(domain_name,
				     name_type,
				     NBT_NAME_SERVER,
				     &dclist[i].ss,
				     tmp_dc_name))
		{
			struct NETLOGON_SAM_LOGON_RESPONSE_NT40 logon1;

			r = TALLOC_ZERO_P(mem_ctx, struct netlogon_samlogon_response);
			NT_STATUS_HAVE_NO_MEMORY(r);

			ZERO_STRUCT(logon1);

			nt_version = NETLOGON_NT_VERSION_1;

			logon1.nt_version = nt_version;
			logon1.pdc_name = tmp_dc_name;
			logon1.domain_name = talloc_strdup_upper(mem_ctx, domain_name);
			NT_STATUS_HAVE_NO_MEMORY(logon1.domain_name);

			r->data.nt4 = logon1;
			r->ntver = nt_version;

			map_netlogon_samlogon_response(r);

			namecache_store(tmp_dc_name, NBT_NAME_SERVER, 1, &ip_list);

			goto make_reply;
		}
	}

	return NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;

 make_reply:

	status = make_dc_info_from_cldap_reply(mem_ctx, flags, &dclist[i].ss,
					       &r->data.nt5_ex, info);
	if (NT_STATUS_IS_OK(status) && store_cache) {
		return store_cldap_reply(mem_ctx, flags, &dclist[i].ss,
					 nt_version, &r->data.nt5_ex);
	}

	return status;
}

/****************************************************************
****************************************************************/

static NTSTATUS dsgetdcname_rediscover(TALLOC_CTX *mem_ctx,
				       struct messaging_context *msg_ctx,
				       const char *domain_name,
				       const struct GUID *domain_guid,
				       uint32_t flags,
				       const char *site_name,
				       struct netr_DsRGetDCNameInfo **info)
{
	NTSTATUS status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	struct ip_service_name *dclist = NULL;
	int num_dcs;

	DEBUG(10,("dsgetdcname_rediscover\n"));

	if (flags & DS_IS_FLAT_NAME) {

		status = discover_dc_netbios(mem_ctx, domain_name, flags,
					     &dclist, &num_dcs);
		NT_STATUS_NOT_OK_RETURN(status);

		return process_dc_netbios(mem_ctx, msg_ctx, domain_name, flags,
					  dclist, num_dcs, info);
	}

	if (flags & DS_IS_DNS_NAME) {

		status = discover_dc_dns(mem_ctx, domain_name, domain_guid,
					 flags, site_name, &dclist, &num_dcs);
		NT_STATUS_NOT_OK_RETURN(status);

		return process_dc_dns(mem_ctx, domain_name, flags,
				      dclist, num_dcs, info);
	}

	status = discover_dc_dns(mem_ctx, domain_name, domain_guid, flags,
				 site_name, &dclist, &num_dcs);

	if (NT_STATUS_IS_OK(status) && num_dcs != 0) {

		status = process_dc_dns(mem_ctx, domain_name, flags, dclist,
					num_dcs, info);
		if (NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	status = discover_dc_netbios(mem_ctx, domain_name, flags, &dclist,
				     &num_dcs);
	NT_STATUS_NOT_OK_RETURN(status);

	return process_dc_netbios(mem_ctx, msg_ctx, domain_name, flags, dclist,
				  num_dcs, info);
}

static bool is_closest_site(struct netr_DsRGetDCNameInfo *info)
{
	if (info->dc_flags & DS_SERVER_CLOSEST) {
		return true;
	}

	if (!info->client_site_name) {
		return true;
	}

	if (!info->dc_site_name) {
		return false;
	}

	if (strcmp(info->client_site_name, info->dc_site_name) == 0) {
		return true;
	}

	return false;
}

/********************************************************************
 Internal dsgetdcname.
********************************************************************/

static NTSTATUS dsgetdcname_internal(TALLOC_CTX *mem_ctx,
		     struct messaging_context *msg_ctx,
		     const char *domain_name,
		     const struct GUID *domain_guid,
		     const char *site_name,
		     uint32_t flags,
		     struct netr_DsRGetDCNameInfo **info)
{
	NTSTATUS status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	struct netr_DsRGetDCNameInfo *myinfo = NULL;
	bool first = true;
	struct netr_DsRGetDCNameInfo *first_info = NULL;

	DEBUG(10,("dsgetdcname_internal: domain_name: %s, "
		  "domain_guid: %s, site_name: %s, flags: 0x%08x\n",
		  domain_name,
		  domain_guid ? GUID_string(mem_ctx, domain_guid) : "(null)",
		  site_name ? site_name : "(null)", flags));

	*info = NULL;

	if (!check_allowed_required_flags(flags, site_name)) {
		DEBUG(0,("invalid flags specified\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (flags & DS_FORCE_REDISCOVERY) {
		goto rediscover;
	}

	status = dsgetdcname_cached(mem_ctx, msg_ctx, domain_name, domain_guid,
				    flags, site_name, &myinfo);
	if (NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (flags & DS_BACKGROUND_ONLY) {
		goto done;
	}

 rediscover:
	status = dsgetdcname_rediscover(mem_ctx, msg_ctx, domain_name,
					domain_guid, flags, site_name,
					&myinfo);

 done:
	if (!NT_STATUS_IS_OK(status)) {
		if (!first) {
			*info = first_info;
			return NT_STATUS_OK;
		}
		return status;
	}

	if (!first) {
		TALLOC_FREE(first_info);
	} else if (!is_closest_site(myinfo)) {
		first = false;
		first_info = myinfo;
		/* TODO: may use the next_closest_site here */
		site_name = myinfo->client_site_name;
		goto rediscover;
	}

	*info = myinfo;
	return NT_STATUS_OK;
}

/********************************************************************
 dsgetdcname.

 This will be the only public function here.
********************************************************************/

NTSTATUS dsgetdcname(TALLOC_CTX *mem_ctx,
		     struct messaging_context *msg_ctx,
		     const char *domain_name,
		     const struct GUID *domain_guid,
		     const char *site_name,
		     uint32_t flags,
		     struct netr_DsRGetDCNameInfo **info)
{
	NTSTATUS status;
	const char *query_site = NULL;
	char *ptr_to_free = NULL;
	bool retry_query_with_null = false;

	if ((site_name == NULL) || (site_name[0] == '\0')) {
		ptr_to_free = sitename_fetch(domain_name);
		if (ptr_to_free != NULL) {
			retry_query_with_null = true;
		}
		query_site = ptr_to_free;
	} else {
		query_site = site_name;
	}

	status = dsgetdcname_internal(mem_ctx,
				msg_ctx,
				domain_name,
				domain_guid,
				query_site,
				flags,
				info);

	SAFE_FREE(ptr_to_free);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		return status;
	}

	/* Should we try again with site_name == NULL ? */
	if (retry_query_with_null) {
		status = dsgetdcname_internal(mem_ctx,
					msg_ctx,
					domain_name,
					domain_guid,
					NULL,
					flags,
					info);
	}

	return status;
}
