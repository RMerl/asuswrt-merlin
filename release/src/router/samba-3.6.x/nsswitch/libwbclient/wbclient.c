/*
   Unix SMB/CIFS implementation.

   Winbind client API

   Copyright (C) Gerald (Jerry) Carter 2007


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

/* From wb_common.c */

NSS_STATUS winbindd_request_response(int req_type,
				     struct winbindd_request *request,
				     struct winbindd_response *response);
NSS_STATUS winbindd_priv_request_response(int req_type,
					  struct winbindd_request *request,
					  struct winbindd_response *response);

/*
 result == NSS_STATUS_UNAVAIL: winbind not around
 result == NSS_STATUS_NOTFOUND: winbind around, but domain missing

 Due to a bad API NSS_STATUS_NOTFOUND is returned both when winbind_off
 and when winbind return WINBINDD_ERROR. So the semantics of this
 routine depends on winbind_on. Grepping for winbind_off I just
 found 3 places where winbind is turned off, and this does not conflict
 (as far as I have seen) with the callers of is_trusted_domains.

 --Volker
*/

static wbcErr wbcRequestResponseInt(
	int cmd,
	struct winbindd_request *request,
	struct winbindd_response *response,
	NSS_STATUS (*fn)(int req_type,
			 struct winbindd_request *request,
			 struct winbindd_response *response))
{
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	NSS_STATUS nss_status;

	/* for some calls the request and/or response can be NULL */

	nss_status = fn(cmd, request, response);

	switch (nss_status) {
	case NSS_STATUS_SUCCESS:
		wbc_status = WBC_ERR_SUCCESS;
		break;
	case NSS_STATUS_UNAVAIL:
		wbc_status = WBC_ERR_WINBIND_NOT_AVAILABLE;
		break;
	case NSS_STATUS_NOTFOUND:
		wbc_status = WBC_ERR_DOMAIN_NOT_FOUND;
		break;
	default:
		wbc_status = WBC_ERR_NSS_ERROR;
		break;
	}

	return wbc_status;
}

/**
 * @brief Wrapper around Winbind's send/receive API call
 *
 * @param cmd       Winbind command operation to perform
 * @param request   Send structure
 * @param response  Receive structure
 *
 * @return #wbcErr
 */
wbcErr wbcRequestResponse(int cmd,
			  struct winbindd_request *request,
			  struct winbindd_response *response)
{
	return wbcRequestResponseInt(cmd, request, response,
				     winbindd_request_response);
}

wbcErr wbcRequestResponsePriv(int cmd,
			      struct winbindd_request *request,
			      struct winbindd_response *response)
{
	return wbcRequestResponseInt(cmd, request, response,
				     winbindd_priv_request_response);
}

/** @brief Translate an error value into a string
 *
 * @param error
 *
 * @return a pointer to a static string
 **/
const char *wbcErrorString(wbcErr error)
{
	switch (error) {
	case WBC_ERR_SUCCESS:
		return "WBC_ERR_SUCCESS";
	case WBC_ERR_NOT_IMPLEMENTED:
		return "WBC_ERR_NOT_IMPLEMENTED";
	case WBC_ERR_UNKNOWN_FAILURE:
		return "WBC_ERR_UNKNOWN_FAILURE";
	case WBC_ERR_NO_MEMORY:
		return "WBC_ERR_NO_MEMORY";
	case WBC_ERR_INVALID_SID:
		return "WBC_ERR_INVALID_SID";
	case WBC_ERR_INVALID_PARAM:
		return "WBC_ERR_INVALID_PARAM";
	case WBC_ERR_WINBIND_NOT_AVAILABLE:
		return "WBC_ERR_WINBIND_NOT_AVAILABLE";
	case WBC_ERR_DOMAIN_NOT_FOUND:
		return "WBC_ERR_DOMAIN_NOT_FOUND";
	case WBC_ERR_INVALID_RESPONSE:
		return "WBC_ERR_INVALID_RESPONSE";
	case WBC_ERR_NSS_ERROR:
		return "WBC_ERR_NSS_ERROR";
	case WBC_ERR_UNKNOWN_USER:
		return "WBC_ERR_UNKNOWN_USER";
	case WBC_ERR_UNKNOWN_GROUP:
		return "WBC_ERR_UNKNOWN_GROUP";
	case WBC_ERR_AUTH_ERROR:
		return "WBC_ERR_AUTH_ERROR";
	case WBC_ERR_PWD_CHANGE_FAILED:
		return "WBC_ERR_PWD_CHANGE_FAILED";
	}

	return "unknown wbcErr value";
}

#define WBC_MAGIC (0x7a2b0e1e)
#define WBC_MAGIC_FREE (0x875634fe)

struct wbcMemPrefix {
	uint32_t magic;
	void (*destructor)(void *ptr);
};

static size_t wbcPrefixLen(void)
{
	size_t result = sizeof(struct wbcMemPrefix);
	return (result + 15) & ~15;
}

static struct wbcMemPrefix *wbcMemToPrefix(void *ptr)
{
	return (struct wbcMemPrefix *)(((char *)ptr) - wbcPrefixLen());
}

void *wbcAllocateMemory(size_t nelem, size_t elsize,
			void (*destructor)(void *ptr))
{
	struct wbcMemPrefix *result;

	if (nelem >= (2<<24)/elsize) {
		/* basic protection against integer wrap */
		return NULL;
	}

	result = (struct wbcMemPrefix *)calloc(
		1, nelem*elsize + wbcPrefixLen());
	if (result == NULL) {
		return NULL;
	}
	result->magic = WBC_MAGIC;
	result->destructor = destructor;
	return ((char *)result) + wbcPrefixLen();
}

/* Free library allocated memory */
void wbcFreeMemory(void *p)
{
	struct wbcMemPrefix *wbcMem;

	if (p == NULL) {
		return;
	}
	wbcMem = wbcMemToPrefix(p);
	if (wbcMem->magic != WBC_MAGIC) {
		return;
	}

	/* paranoid check to ensure we don't double free */
	wbcMem->magic = WBC_MAGIC_FREE;

	if (wbcMem->destructor != NULL) {
		wbcMem->destructor(p);
	}
	free(wbcMem);
	return;
}

char *wbcStrDup(const char *str)
{
	char *result;
	size_t len;

	len = strlen(str);
	result = (char *)wbcAllocateMemory(len+1, sizeof(char), NULL);
	if (result == NULL) {
		return NULL;
	}
	memcpy(result, str, len+1);
	return result;
}

static void wbcStringArrayDestructor(void *ptr)
{
	char **p = (char **)ptr;
	while (*p != NULL) {
		free(*p);
		p += 1;
	}
}

const char **wbcAllocateStringArray(int num_strings)
{
	return (const char **)wbcAllocateMemory(
		num_strings + 1, sizeof(const char *),
		wbcStringArrayDestructor);
}

wbcErr wbcLibraryDetails(struct wbcLibraryDetails **_details)
{
	struct wbcLibraryDetails *info;

	info = (struct wbcLibraryDetails *)wbcAllocateMemory(
		1, sizeof(struct wbcLibraryDetails), NULL);

	if (info == NULL) {
		return WBC_ERR_NO_MEMORY;
	}

	info->major_version = WBCLIENT_MAJOR_VERSION;
	info->minor_version = WBCLIENT_MINOR_VERSION;
	info->vendor_version = WBCLIENT_VENDOR_VERSION;

	*_details = info;
	return WBC_ERR_SUCCESS;
}
