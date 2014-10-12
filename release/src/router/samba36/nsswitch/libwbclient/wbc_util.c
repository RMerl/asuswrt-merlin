/*
   Unix SMB/CIFS implementation.

   Winbind client asynchronous API, utility functions

   Copyright (C) Gerald (Jerry) Carter 2007-2008


   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Required Headers */

#include "replace.h"
#include "libwbclient.h"
#include "../winbind_client.h"

/** @brief Ping winbindd to see if the daemon is running
 *
 * @return #wbcErr
 **/
wbcErr wbcPing(void)
{
	struct winbindd_request request;
	struct winbindd_response response;

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	return wbcRequestResponse(WINBINDD_PING, &request, &response);
}

static void wbcInterfaceDetailsDestructor(void *ptr)
{
	struct wbcInterfaceDetails *i = (struct wbcInterfaceDetails *)ptr;
	free(i->winbind_version);
	free(i->netbios_name);
	free(i->netbios_domain);
	free(i->dns_domain);
}

/**
 * @brief Query useful information about the winbind service
 *
 * @param *_details	pointer to hold the struct wbcInterfaceDetails
 *
 * @return #wbcErr
 */

wbcErr wbcInterfaceDetails(struct wbcInterfaceDetails **_details)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcInterfaceDetails *info;
	struct wbcDomainInfo *domain = NULL;
	struct winbindd_request request;
	struct winbindd_response response;

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	info = (struct wbcInterfaceDetails *)wbcAllocateMemory(
		1, sizeof(struct wbcInterfaceDetails),
		wbcInterfaceDetailsDestructor);
	BAIL_ON_PTR_ERROR(info, wbc_status);

	/* first the interface version */
	wbc_status = wbcRequestResponse(WINBINDD_INTERFACE_VERSION, NULL, &response);
	BAIL_ON_WBC_ERROR(wbc_status);
	info->interface_version = response.data.interface_version;

	/* then the samba version and the winbind separator */
	wbc_status = wbcRequestResponse(WINBINDD_INFO, NULL, &response);
	BAIL_ON_WBC_ERROR(wbc_status);

	info->winbind_version = strdup(response.data.info.samba_version);
	BAIL_ON_PTR_ERROR(info->winbind_version, wbc_status);
	info->winbind_separator = response.data.info.winbind_separator;

	/* then the local netbios name */
	wbc_status = wbcRequestResponse(WINBINDD_NETBIOS_NAME, NULL, &response);
	BAIL_ON_WBC_ERROR(wbc_status);

	info->netbios_name = strdup(response.data.netbios_name);
	BAIL_ON_PTR_ERROR(info->netbios_name, wbc_status);

	/* then the local workgroup name */
	wbc_status = wbcRequestResponse(WINBINDD_DOMAIN_NAME, NULL, &response);
	BAIL_ON_WBC_ERROR(wbc_status);

	info->netbios_domain = strdup(response.data.domain_name);
	BAIL_ON_PTR_ERROR(info->netbios_domain, wbc_status);

	wbc_status = wbcDomainInfo(info->netbios_domain, &domain);
	if (wbc_status == WBC_ERR_DOMAIN_NOT_FOUND) {
		/* maybe it's a standalone server */
		domain = NULL;
		wbc_status = WBC_ERR_SUCCESS;
	} else {
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	if (domain) {
		info->dns_domain = strdup(domain->dns_name);
		wbcFreeMemory(domain);
		BAIL_ON_PTR_ERROR(info->dns_domain, wbc_status);
	} else {
		info->dns_domain = NULL;
	}

	*_details = info;
	info = NULL;

	wbc_status = WBC_ERR_SUCCESS;

done:
	wbcFreeMemory(info);
	return wbc_status;
}

static void wbcDomainInfoDestructor(void *ptr)
{
	struct wbcDomainInfo *i = (struct wbcDomainInfo *)ptr;
	free(i->short_name);
	free(i->dns_name);
}

/** @brief Lookup the current status of a trusted domain, sync wrapper
 *
 * @param domain      Domain to query
 * @param *dinfo       Pointer to returned struct wbcDomainInfo
 *
 * @return #wbcErr
 */

wbcErr wbcDomainInfo(const char *domain, struct wbcDomainInfo **dinfo)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainInfo *info = NULL;

	if (!domain || !dinfo) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Initialize request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.domain_name, domain,
		sizeof(request.domain_name)-1);

	wbc_status = wbcRequestResponse(WINBINDD_DOMAIN_INFO,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	info = (struct wbcDomainInfo *)wbcAllocateMemory(
		1, sizeof(struct wbcDomainInfo), wbcDomainInfoDestructor);
	BAIL_ON_PTR_ERROR(info, wbc_status);

	info->short_name = strdup(response.data.domain_info.name);
	BAIL_ON_PTR_ERROR(info->short_name, wbc_status);

	info->dns_name = strdup(response.data.domain_info.alt_name);
	BAIL_ON_PTR_ERROR(info->dns_name, wbc_status);

	wbc_status = wbcStringToSid(response.data.domain_info.sid,
				    &info->sid);
	BAIL_ON_WBC_ERROR(wbc_status);

	if (response.data.domain_info.native_mode)
		info->domain_flags |= WBC_DOMINFO_DOMAIN_NATIVE;
	if (response.data.domain_info.active_directory)
		info->domain_flags |= WBC_DOMINFO_DOMAIN_AD;
	if (response.data.domain_info.primary)
		info->domain_flags |= WBC_DOMINFO_DOMAIN_PRIMARY;

	*dinfo = info;
	info = NULL;

	wbc_status = WBC_ERR_SUCCESS;

 done:
	wbcFreeMemory(info);
	return wbc_status;
}

/* Get the list of current DCs */
wbcErr wbcDcInfo(const char *domain, size_t *num_dcs,
		 const char ***dc_names, const char ***dc_ips)
{
	struct winbindd_request request;
	struct winbindd_response response;
	const char **names = NULL;
	const char **ips = NULL;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	size_t extra_len;
	int i;
	char *p;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (domain != NULL) {
		strncpy(request.domain_name, domain,
			sizeof(request.domain_name) - 1);
	}

	wbc_status = wbcRequestResponse(WINBINDD_DC_INFO,
					&request, &response);
	BAIL_ON_WBC_ERROR(wbc_status);

	names = wbcAllocateStringArray(response.data.num_entries);
	BAIL_ON_PTR_ERROR(names, wbc_status);

	ips = wbcAllocateStringArray(response.data.num_entries);
	BAIL_ON_PTR_ERROR(ips, wbc_status);

	wbc_status = WBC_ERR_INVALID_RESPONSE;

	p = (char *)response.extra_data.data;

	if (response.length < (sizeof(struct winbindd_response)+1)) {
		goto done;
	}

	extra_len = response.length - sizeof(struct winbindd_response);

	if (p[extra_len-1] != '\0') {
		goto done;
	}

	for (i=0; i<response.data.num_entries; i++) {
		char *q;

		q = strchr(p, '\n');
		if (q == NULL) {
			goto done;
		}
		names[i] = strndup(p, q-p);
		BAIL_ON_PTR_ERROR(names[i], wbc_status);
		p = q+1;

		q = strchr(p, '\n');
		if (q == NULL) {
			goto done;
		}
		ips[i] = strndup(p, q-p);
		BAIL_ON_PTR_ERROR(ips[i], wbc_status);
		p = q+1;
	}
	if (p[0] != '\0') {
		goto done;
	}

        wbc_status = WBC_ERR_SUCCESS;
done:
	if (response.extra_data.data)
		free(response.extra_data.data);

	if (WBC_ERROR_IS_OK(wbc_status)) {
		*num_dcs = response.data.num_entries;
		*dc_names = names;
		names = NULL;
		*dc_ips = ips;
		ips = NULL;
	}
	wbcFreeMemory(names);
	wbcFreeMemory(ips);
	return wbc_status;
}

/* Resolve a NetbiosName via WINS */
wbcErr wbcResolveWinsByName(const char *name, char **ip)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *ipaddr;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Send request */

	strncpy(request.data.winsreq, name,
		sizeof(request.data.winsreq)-1);

	wbc_status = wbcRequestResponse(WINBINDD_WINS_BYNAME,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Display response */

	ipaddr = wbcStrDup(response.data.winsresp);
	BAIL_ON_PTR_ERROR(ipaddr, wbc_status);

	*ip = ipaddr;
	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}

/* Resolve an IP address via WINS into a NetbiosName */
wbcErr wbcResolveWinsByIP(const char *ip, char **name)
{
	struct winbindd_request request;
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *name_str;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Send request */

	strncpy(request.data.winsreq, ip,
		sizeof(request.data.winsreq)-1);

	wbc_status = wbcRequestResponse(WINBINDD_WINS_BYIP,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Display response */

	name_str = wbcStrDup(response.data.winsresp);
	BAIL_ON_PTR_ERROR(name_str, wbc_status);

	*name = name_str;
	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}

/**
 */

static wbcErr process_domain_info_string(struct wbcDomainInfo *info,
					 char *info_string)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *r = NULL;
	char *s = NULL;

	r = info_string;

	/* Short Name */
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	info->short_name = strdup(r);
	BAIL_ON_PTR_ERROR(info->short_name, wbc_status);


	/* DNS Name */
	r = s;
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	info->dns_name = strdup(r);
	BAIL_ON_PTR_ERROR(info->dns_name, wbc_status);

	/* SID */
	r = s;
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	wbc_status = wbcStringToSid(r, &info->sid);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Trust type */
	r = s;
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	if (strcmp(r, "None") == 0) {
		info->trust_type = WBC_DOMINFO_TRUSTTYPE_NONE;
	} else if (strcmp(r, "External") == 0) {
		info->trust_type = WBC_DOMINFO_TRUSTTYPE_EXTERNAL;
	} else if (strcmp(r, "Forest") == 0) {
		info->trust_type = WBC_DOMINFO_TRUSTTYPE_FOREST;
	} else if (strcmp(r, "In Forest") == 0) {
		info->trust_type = WBC_DOMINFO_TRUSTTYPE_IN_FOREST;
	} else {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	/* Transitive */
	r = s;
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	if (strcmp(r, "Yes") == 0) {
		info->trust_flags |= WBC_DOMINFO_TRUST_TRANSITIVE;
	}

	/* Incoming */
	r = s;
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	if (strcmp(r, "Yes") == 0) {
		info->trust_flags |= WBC_DOMINFO_TRUST_INCOMING;
	}

	/* Outgoing */
	r = s;
	if ((s = strchr(r, '\\')) == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	*s = '\0';
	s++;

	if (strcmp(r, "Yes") == 0) {
		info->trust_flags |= WBC_DOMINFO_TRUST_OUTGOING;
	}

	/* Online/Offline status */

	r = s;
	if (r == NULL) {
		wbc_status = WBC_ERR_INVALID_RESPONSE;
		BAIL_ON_WBC_ERROR(wbc_status);
	}
	if ( strcmp(r, "Offline") == 0) {
		info->domain_flags |= WBC_DOMINFO_DOMAIN_OFFLINE;
	}

	wbc_status = WBC_ERR_SUCCESS;

 done:
	return wbc_status;
}

static void wbcDomainInfoListDestructor(void *ptr)
{
	struct wbcDomainInfo *i = (struct wbcDomainInfo *)ptr;

	while (i->short_name != NULL) {
		free(i->short_name);
		free(i->dns_name);
		i += 1;
	}
}

/* Enumerate the domain trusts known by Winbind */
wbcErr wbcListTrusts(struct wbcDomainInfo **domains, size_t *num_domains)
{
	struct winbindd_response response;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	char *p = NULL;
	char *extra_data = NULL;
	struct wbcDomainInfo *d_list = NULL;
	int i = 0;

	*domains = NULL;
	*num_domains = 0;

	ZERO_STRUCT(response);

	/* Send request */

	wbc_status = wbcRequestResponse(WINBINDD_LIST_TRUSTDOM,
					NULL,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	/* Decode the response */

	p = (char *)response.extra_data.data;

	if ((p == NULL) || (strlen(p) == 0)) {
		/* We should always at least get back our
		   own SAM domain */

		wbc_status = WBC_ERR_DOMAIN_NOT_FOUND;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	d_list = (struct wbcDomainInfo *)wbcAllocateMemory(
		response.data.num_entries + 1,sizeof(struct wbcDomainInfo),
		wbcDomainInfoListDestructor);
	BAIL_ON_PTR_ERROR(d_list, wbc_status);

	extra_data = strdup((char*)response.extra_data.data);
	BAIL_ON_PTR_ERROR(extra_data, wbc_status);

	p = extra_data;

	/* Outer loop processes the list of domain information */

	for (i=0; i<response.data.num_entries && p; i++) {
		char *next = strchr(p, '\n');

		if (next) {
			*next = '\0';
			next++;
		}

		wbc_status = process_domain_info_string(&d_list[i], p);
		BAIL_ON_WBC_ERROR(wbc_status);

		p = next;
	}

	*domains = d_list;
	d_list = NULL;
	*num_domains = i;

 done:
	winbindd_free_response(&response);
	wbcFreeMemory(d_list);
	free(extra_data);
	return wbc_status;
}

static void wbcDomainControllerInfoDestructor(void *ptr)
{
	struct wbcDomainControllerInfo *i =
		(struct wbcDomainControllerInfo *)ptr;
	free(i->dc_name);
}

/* Enumerate the domain trusts known by Winbind */
wbcErr wbcLookupDomainController(const char *domain,
				 uint32_t flags,
				struct wbcDomainControllerInfo **dc_info)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;
	struct wbcDomainControllerInfo *dc = NULL;

	/* validate input params */

	if (!domain || !dc_info) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.dsgetdcname.domain_name, domain,
		sizeof(request.data.dsgetdcname.domain_name)-1);

	request.flags = flags;

	dc = (struct wbcDomainControllerInfo *)wbcAllocateMemory(
		 1, sizeof(struct wbcDomainControllerInfo),
		wbcDomainControllerInfoDestructor);
	BAIL_ON_PTR_ERROR(dc, wbc_status);

	/* Send request */

	wbc_status = wbcRequestResponse(WINBINDD_DSGETDCNAME,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	dc->dc_name = strdup(response.data.dsgetdcname.dc_unc);
	BAIL_ON_PTR_ERROR(dc->dc_name, wbc_status);

	*dc_info = dc;
	dc = NULL;

done:
	wbcFreeMemory(dc);
	return wbc_status;
}

static void wbcDomainControllerInfoExDestructor(void *ptr)
{
	struct wbcDomainControllerInfoEx *i =
		(struct wbcDomainControllerInfoEx *)ptr;
	free((char *)(i->dc_unc));
	free((char *)(i->dc_address));
	free((char *)(i->domain_guid));
	free((char *)(i->domain_name));
	free((char *)(i->forest_name));
	free((char *)(i->dc_site_name));
	free((char *)(i->client_site_name));
}

static wbcErr wbc_create_domain_controller_info_ex(const struct winbindd_response *resp,
						   struct wbcDomainControllerInfoEx **_i)
{
	wbcErr wbc_status = WBC_ERR_SUCCESS;
	struct wbcDomainControllerInfoEx *i;
	struct wbcGuid guid;

	i = (struct wbcDomainControllerInfoEx *)wbcAllocateMemory(
		1, sizeof(struct wbcDomainControllerInfoEx),
		wbcDomainControllerInfoExDestructor);
	BAIL_ON_PTR_ERROR(i, wbc_status);

	i->dc_unc = strdup(resp->data.dsgetdcname.dc_unc);
	BAIL_ON_PTR_ERROR(i->dc_unc, wbc_status);

	i->dc_address = strdup(resp->data.dsgetdcname.dc_address);
	BAIL_ON_PTR_ERROR(i->dc_address, wbc_status);

	i->dc_address_type = resp->data.dsgetdcname.dc_address_type;

	wbc_status = wbcStringToGuid(resp->data.dsgetdcname.domain_guid, &guid);
	if (WBC_ERROR_IS_OK(wbc_status)) {
		i->domain_guid = (struct wbcGuid *)malloc(
			sizeof(struct wbcGuid));
		BAIL_ON_PTR_ERROR(i->domain_guid, wbc_status);

		*i->domain_guid = guid;
	}

	i->domain_name = strdup(resp->data.dsgetdcname.domain_name);
	BAIL_ON_PTR_ERROR(i->domain_name, wbc_status);

	if (resp->data.dsgetdcname.forest_name[0] != '\0') {
		i->forest_name = strdup(resp->data.dsgetdcname.forest_name);
		BAIL_ON_PTR_ERROR(i->forest_name, wbc_status);
	}

	i->dc_flags = resp->data.dsgetdcname.dc_flags;

	if (resp->data.dsgetdcname.dc_site_name[0] != '\0') {
		i->dc_site_name = strdup(resp->data.dsgetdcname.dc_site_name);
		BAIL_ON_PTR_ERROR(i->dc_site_name, wbc_status);
	}

	if (resp->data.dsgetdcname.client_site_name[0] != '\0') {
		i->client_site_name = strdup(
			resp->data.dsgetdcname.client_site_name);
		BAIL_ON_PTR_ERROR(i->client_site_name, wbc_status);
	}

	*_i = i;
	i = NULL;

done:
	if (i != NULL) {
		wbcFreeMemory(i);
	}
	return wbc_status;
}

/* Get extended domain controller information */
wbcErr wbcLookupDomainControllerEx(const char *domain,
				   struct wbcGuid *guid,
				   const char *site,
				   uint32_t flags,
				   struct wbcDomainControllerInfoEx **dc_info)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct winbindd_request request;
	struct winbindd_response response;

	/* validate input params */

	if (!domain || !dc_info) {
		wbc_status = WBC_ERR_INVALID_PARAM;
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.data.dsgetdcname.flags = flags;

	strncpy(request.data.dsgetdcname.domain_name, domain,
		sizeof(request.data.dsgetdcname.domain_name)-1);

	if (site) {
		strncpy(request.data.dsgetdcname.site_name, site,
			sizeof(request.data.dsgetdcname.site_name)-1);
	}

	if (guid) {
		char *str = NULL;

		wbc_status = wbcGuidToString(guid, &str);
		BAIL_ON_WBC_ERROR(wbc_status);

		strncpy(request.data.dsgetdcname.domain_guid, str,
			sizeof(request.data.dsgetdcname.domain_guid)-1);

		wbcFreeMemory(str);
	}

	/* Send request */

	wbc_status = wbcRequestResponse(WINBINDD_DSGETDCNAME,
					&request,
					&response);
	BAIL_ON_WBC_ERROR(wbc_status);

	if (dc_info) {
		wbc_status = wbc_create_domain_controller_info_ex(&response,
								  dc_info);
		BAIL_ON_WBC_ERROR(wbc_status);
	}

	wbc_status = WBC_ERR_SUCCESS;
done:
	return wbc_status;
}

static void wbcNamedBlobDestructor(void *ptr)
{
	struct wbcNamedBlob *b = (struct wbcNamedBlob *)ptr;

	while (b->name != NULL) {
		free((char *)(b->name));
		free(b->blob.data);
		b += 1;
	}
}

/* Initialize a named blob and add to list of blobs */
wbcErr wbcAddNamedBlob(size_t *num_blobs,
		       struct wbcNamedBlob **pblobs,
		       const char *name,
		       uint32_t flags,
		       uint8_t *data,
		       size_t length)
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcNamedBlob *blobs, *blob;

	if (name == NULL) {
		return WBC_ERR_INVALID_PARAM;
	}

	/*
	 * Overallocate the b->name==NULL terminator for
	 * wbcNamedBlobDestructor
	 */
	blobs = (struct wbcNamedBlob *)wbcAllocateMemory(
		*num_blobs + 2, sizeof(struct wbcNamedBlob),
		wbcNamedBlobDestructor);

	if (blobs == NULL) {
		return WBC_ERR_NO_MEMORY;
	}

	if (*pblobs != NULL) {
		struct wbcNamedBlob *old = *pblobs;
		memcpy(blobs, old, sizeof(struct wbcNamedBlob) * (*num_blobs));
		if (*num_blobs != 0) {
			/* end indicator for wbcNamedBlobDestructor */
			old[0].name = NULL;
		}
		wbcFreeMemory(old);
	}
	*pblobs = blobs;

	blob = &blobs[*num_blobs];

	blob->name = strdup(name);
	BAIL_ON_PTR_ERROR(blob->name, wbc_status);
	blob->flags = flags;

	blob->blob.length = length;
	blob->blob.data	= (uint8_t *)malloc(length);
	BAIL_ON_PTR_ERROR(blob->blob.data, wbc_status);
	memcpy(blob->blob.data, data, length);

	*num_blobs += 1;
	*pblobs = blobs;
	blobs = NULL;

	wbc_status = WBC_ERR_SUCCESS;
done:
	wbcFreeMemory(blobs);
	return wbc_status;
}
